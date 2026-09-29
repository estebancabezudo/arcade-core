module video_frame_manager (
    input  logic        source_clk,
    input  logic        source_reset,

    input  logic        source_frame_start,
    input  logic        source_frame_complete,

    /*
     * Successful SDRAM completion.
     */
    input  logic        source_commit_valid,
    input  logic [1:0]  source_commit_slot,
    input  logic [31:0] source_commit_frame_id,

    /*
     * Aborted SDRAM completion.
     *
     * When this arrives, all partial writes belonging to the frame
     * have already completed, so the slot can safely be reused.
     */
    input  logic        source_abort_valid,
    input  logic [1:0]  source_abort_slot,
    input  logic [31:0] source_abort_frame_id,

    output logic        source_capture_enable,
    output logic [1:0]  source_write_slot,
    output logic [31:0] source_frame_id,

    output logic        source_frame_dropped,
    output logic        source_commit_error,
    output logic        source_abort_error,

    input  logic        hdmi_clk,
    input  logic        hdmi_reset,

    input  logic        hdmi_frame_start,

    output logic        hdmi_frame_valid,
    output logic [1:0]  hdmi_read_slot,
    output logic [31:0] hdmi_frame_id
);

    /*
     * ready_toggle != ack_toggle means that the slot contains a fully
     * committed frame visible to HDMI.
     */
    logic [3:0] ready_toggle;
    logic [3:0] ack_toggle;
    logic [3:0] ack_toggle_next;

    logic [3:0] ready_sync_1;
    logic [3:0] ready_sync_2;

    logic [3:0] ack_sync_1;
    logic [3:0] ack_sync_2;

    /*
     * Reserved means that a source frame owns this slot.
     *
     * It remains reserved after raster completion until either:
     *
     *   COMMIT -> publish to HDMI
     *   ABORT  -> discard and reuse
     */
    logic [3:0] slot_reserved;

    logic [31:0] slot_frame_id_0;
    logic [31:0] slot_frame_id_1;
    logic [31:0] slot_frame_id_2;
    logic [31:0] slot_frame_id_3;

    logic [31:0] next_source_frame_id;

    logic source_frame_active;

    logic slot_0_abort_release;
    logic slot_1_abort_release;
    logic slot_2_abort_release;
    logic slot_3_abort_release;

    logic slot_0_free;
    logic slot_1_free;
    logic slot_2_free;
    logic slot_3_free;

    logic [3:0] hdmi_occupied;

    logic        newest_valid;
    logic [1:0]  newest_slot;
    logic [31:0] newest_id;

    logic        newest_pending_valid;
    logic [1:0]  newest_pending_slot;
    logic [31:0] newest_pending_id;

    /*
     * HDMI acknowledgements cross into the source domain.
     */
    always_ff @(posedge source_clk) begin
        if (source_reset) begin
            ack_sync_1 <= 4'b0000;
            ack_sync_2 <= 4'b0000;
        end else begin
            ack_sync_1 <= ack_toggle;
            ack_sync_2 <= ack_sync_1;
        end
    end

    /*
     * Source completion toggles cross into the HDMI domain.
     */
    always_ff @(posedge hdmi_clk) begin
        if (hdmi_reset) begin
            ready_sync_1 <= 4'b0000;
            ready_sync_2 <= 4'b0000;
        end else begin
            ready_sync_1 <= ready_toggle;
            ready_sync_2 <= ready_sync_1;
        end
    end

    /*
     * An ABORT may free a slot on the same source clock edge that a new
     * frame begins. This avoids unnecessarily dropping that new frame.
     */
    always_comb begin
        slot_0_abort_release =
            source_abort_valid &&
            !source_commit_valid &&
            (source_abort_slot == 2'd0) &&
            slot_reserved[0] &&
            (slot_frame_id_0 == source_abort_frame_id);

        slot_1_abort_release =
            source_abort_valid &&
            !source_commit_valid &&
            (source_abort_slot == 2'd1) &&
            slot_reserved[1] &&
            (slot_frame_id_1 == source_abort_frame_id);

        slot_2_abort_release =
            source_abort_valid &&
            !source_commit_valid &&
            (source_abort_slot == 2'd2) &&
            slot_reserved[2] &&
            (slot_frame_id_2 == source_abort_frame_id);

        slot_3_abort_release =
            source_abort_valid &&
            !source_commit_valid &&
            (source_abort_slot == 2'd3) &&
            slot_reserved[3] &&
            (slot_frame_id_3 == source_abort_frame_id);

        slot_0_free =
            (
                !slot_reserved[0] &&
                (ready_toggle[0] == ack_sync_2[0])
            ) ||
            slot_0_abort_release;

        slot_1_free =
            (
                !slot_reserved[1] &&
                (ready_toggle[1] == ack_sync_2[1])
            ) ||
            slot_1_abort_release;

        slot_2_free =
            (
                !slot_reserved[2] &&
                (ready_toggle[2] == ack_sync_2[2])
            ) ||
            slot_2_abort_release;

        slot_3_free =
            (
                !slot_reserved[3] &&
                (ready_toggle[3] == ack_sync_2[3])
            ) ||
            slot_3_abort_release;
    end

    /*
     * Source-side frame allocation, commit and abort handling.
     */
    always_ff @(posedge source_clk) begin
        if (source_reset) begin
            ready_toggle          <= 4'b0000;
            slot_reserved         <= 4'b0000;

            slot_frame_id_0       <= 32'd0;
            slot_frame_id_1       <= 32'd0;
            slot_frame_id_2       <= 32'd0;
            slot_frame_id_3       <= 32'd0;

            next_source_frame_id  <= 32'd0;

            source_frame_active   <= 1'b0;
            source_capture_enable <= 1'b0;
            source_write_slot     <= 2'd0;
            source_frame_id       <= 32'd0;

            source_frame_dropped  <= 1'b0;
            source_commit_error   <= 1'b0;
            source_abort_error    <= 1'b0;
        end else begin
            source_frame_dropped <= 1'b0;
            source_commit_error  <= 1'b0;
            source_abort_error   <= 1'b0;

            /*
             * COMMIT and ABORT for the same cycle are a protocol error.
             */
            if (source_commit_valid && source_abort_valid) begin
                source_commit_error <= 1'b1;
                source_abort_error  <= 1'b1;
            end else if (source_commit_valid) begin
                case (source_commit_slot)
                    2'd0: begin
                        if (
                            slot_reserved[0] &&
                            (slot_frame_id_0 == source_commit_frame_id)
                        ) begin
                            ready_toggle[0] <= ~ready_toggle[0];
                            slot_reserved[0] <= 1'b0;
                        end else begin
                            source_commit_error <= 1'b1;
                        end
                    end

                    2'd1: begin
                        if (
                            slot_reserved[1] &&
                            (slot_frame_id_1 == source_commit_frame_id)
                        ) begin
                            ready_toggle[1] <= ~ready_toggle[1];
                            slot_reserved[1] <= 1'b0;
                        end else begin
                            source_commit_error <= 1'b1;
                        end
                    end

                    2'd2: begin
                        if (
                            slot_reserved[2] &&
                            (slot_frame_id_2 == source_commit_frame_id)
                        ) begin
                            ready_toggle[2] <= ~ready_toggle[2];
                            slot_reserved[2] <= 1'b0;
                        end else begin
                            source_commit_error <= 1'b1;
                        end
                    end

                    2'd3: begin
                        if (
                            slot_reserved[3] &&
                            (slot_frame_id_3 == source_commit_frame_id)
                        ) begin
                            ready_toggle[3] <= ~ready_toggle[3];
                            slot_reserved[3] <= 1'b0;
                        end else begin
                            source_commit_error <= 1'b1;
                        end
                    end
                endcase
            end else if (source_abort_valid) begin
                case (source_abort_slot)
                    2'd0: begin
                        if (
                            slot_reserved[0] &&
                            (slot_frame_id_0 == source_abort_frame_id)
                        ) begin
                            /*
                             * No ready_toggle change:
                             * HDMI must never see this frame.
                             */
                            slot_reserved[0] <= 1'b0;
                        end else begin
                            source_abort_error <= 1'b1;
                        end
                    end

                    2'd1: begin
                        if (
                            slot_reserved[1] &&
                            (slot_frame_id_1 == source_abort_frame_id)
                        ) begin
                            slot_reserved[1] <= 1'b0;
                        end else begin
                            source_abort_error <= 1'b1;
                        end
                    end

                    2'd2: begin
                        if (
                            slot_reserved[2] &&
                            (slot_frame_id_2 == source_abort_frame_id)
                        ) begin
                            slot_reserved[2] <= 1'b0;
                        end else begin
                            source_abort_error <= 1'b1;
                        end
                    end

                    2'd3: begin
                        if (
                            slot_reserved[3] &&
                            (slot_frame_id_3 == source_abort_frame_id)
                        ) begin
                            slot_reserved[3] <= 1'b0;
                        end else begin
                            source_abort_error <= 1'b1;
                        end
                    end
                endcase
            end

            /*
             * Allocate a slot at the start of every arcade frame.
             *
             * The emulator itself is never stalled.
             */
            if (source_frame_start) begin
                source_frame_active <= 1'b1;
                source_frame_id     <= next_source_frame_id;

                if (slot_0_free) begin
                    source_capture_enable <= 1'b1;
                    source_write_slot     <= 2'd0;

                    slot_reserved[0] <= 1'b1;
                    slot_frame_id_0  <= next_source_frame_id;
                end else if (slot_1_free) begin
                    source_capture_enable <= 1'b1;
                    source_write_slot     <= 2'd1;

                    slot_reserved[1] <= 1'b1;
                    slot_frame_id_1  <= next_source_frame_id;
                end else if (slot_2_free) begin
                    source_capture_enable <= 1'b1;
                    source_write_slot     <= 2'd2;

                    slot_reserved[2] <= 1'b1;
                    slot_frame_id_2  <= next_source_frame_id;
                end else if (slot_3_free) begin
                    source_capture_enable <= 1'b1;
                    source_write_slot     <= 2'd3;

                    slot_reserved[3] <= 1'b1;
                    slot_frame_id_3  <= next_source_frame_id;
                end else begin
                    source_capture_enable <= 1'b0;
                    source_frame_dropped  <= 1'b1;
                end

                next_source_frame_id <=
                    next_source_frame_id + 1'b1;
            end

            /*
             * Raster completion only ends capture.
             *
             * The slot remains reserved until COMMIT or ABORT returns from
             * the memory path.
             */
            if (
                source_frame_complete &&
                source_frame_active
            ) begin
                source_frame_active   <= 1'b0;
                source_capture_enable <= 1'b0;
            end
        end
    end

    /*
     * Committed frames visible in the HDMI domain.
     */
    always_comb begin
        hdmi_occupied =
            ready_sync_2 ^ ack_toggle;
    end

    /*
     * Newest committed frame.
     */
    always_comb begin
        newest_valid = 1'b0;
        newest_slot  = 2'd0;
        newest_id    = 32'd0;

        if (hdmi_occupied[0]) begin
            newest_valid = 1'b1;
            newest_slot  = 2'd0;
            newest_id    = slot_frame_id_0;
        end

        if (
            hdmi_occupied[1] &&
            (!newest_valid || slot_frame_id_1 > newest_id)
        ) begin
            newest_valid = 1'b1;
            newest_slot  = 2'd1;
            newest_id    = slot_frame_id_1;
        end

        if (
            hdmi_occupied[2] &&
            (!newest_valid || slot_frame_id_2 > newest_id)
        ) begin
            newest_valid = 1'b1;
            newest_slot  = 2'd2;
            newest_id    = slot_frame_id_2;
        end

        if (
            hdmi_occupied[3] &&
            (!newest_valid || slot_frame_id_3 > newest_id)
        ) begin
            newest_valid = 1'b1;
            newest_slot  = 2'd3;
            newest_id    = slot_frame_id_3;
        end
    end

    /*
     * Newest committed frame that is not currently being displayed.
     */
    always_comb begin
        newest_pending_valid = 1'b0;
        newest_pending_slot  = 2'd0;
        newest_pending_id    = 32'd0;

        if (
            hdmi_occupied[0] &&
            !(hdmi_frame_valid && hdmi_read_slot == 2'd0)
        ) begin
            newest_pending_valid = 1'b1;
            newest_pending_slot  = 2'd0;
            newest_pending_id    = slot_frame_id_0;
        end

        if (
            hdmi_occupied[1] &&
            !(hdmi_frame_valid && hdmi_read_slot == 2'd1) &&
            (!newest_pending_valid ||
             slot_frame_id_1 > newest_pending_id)
        ) begin
            newest_pending_valid = 1'b1;
            newest_pending_slot  = 2'd1;
            newest_pending_id    = slot_frame_id_1;
        end

        if (
            hdmi_occupied[2] &&
            !(hdmi_frame_valid && hdmi_read_slot == 2'd2) &&
            (!newest_pending_valid ||
             slot_frame_id_2 > newest_pending_id)
        ) begin
            newest_pending_valid = 1'b1;
            newest_pending_slot  = 2'd2;
            newest_pending_id    = slot_frame_id_2;
        end

        if (
            hdmi_occupied[3] &&
            !(hdmi_frame_valid && hdmi_read_slot == 2'd3) &&
            (!newest_pending_valid ||
             slot_frame_id_3 > newest_pending_id)
        ) begin
            newest_pending_valid = 1'b1;
            newest_pending_slot  = 2'd3;
            newest_pending_id    = slot_frame_id_3;
        end
    end

    /*
     * Release obsolete committed frames as early as possible.
     */
    always_comb begin
        ack_toggle_next = ack_toggle;

        if (hdmi_frame_start) begin
            if (newest_valid) begin
                if (hdmi_occupied[0] && newest_slot != 2'd0)
                    ack_toggle_next[0] = ready_sync_2[0];

                if (hdmi_occupied[1] && newest_slot != 2'd1)
                    ack_toggle_next[1] = ready_sync_2[1];

                if (hdmi_occupied[2] && newest_slot != 2'd2)
                    ack_toggle_next[2] = ready_sync_2[2];

                if (hdmi_occupied[3] && newest_slot != 2'd3)
                    ack_toggle_next[3] = ready_sync_2[3];
            end
        end else if (hdmi_frame_valid) begin
            if (
                hdmi_occupied[0] &&
                hdmi_read_slot != 2'd0 &&
                (!newest_pending_valid ||
                 newest_pending_slot != 2'd0)
            )
                ack_toggle_next[0] = ready_sync_2[0];

            if (
                hdmi_occupied[1] &&
                hdmi_read_slot != 2'd1 &&
                (!newest_pending_valid ||
                 newest_pending_slot != 2'd1)
            )
                ack_toggle_next[1] = ready_sync_2[1];

            if (
                hdmi_occupied[2] &&
                hdmi_read_slot != 2'd2 &&
                (!newest_pending_valid ||
                 newest_pending_slot != 2'd2)
            )
                ack_toggle_next[2] = ready_sync_2[2];

            if (
                hdmi_occupied[3] &&
                hdmi_read_slot != 2'd3 &&
                (!newest_pending_valid ||
                 newest_pending_slot != 2'd3)
            )
                ack_toggle_next[3] = ready_sync_2[3];
        end else if (newest_valid) begin
            /*
             * Startup: retain only the newest committed candidate.
             */
            if (hdmi_occupied[0] && newest_slot != 2'd0)
                ack_toggle_next[0] = ready_sync_2[0];

            if (hdmi_occupied[1] && newest_slot != 2'd1)
                ack_toggle_next[1] = ready_sync_2[1];

            if (hdmi_occupied[2] && newest_slot != 2'd2)
                ack_toggle_next[2] = ready_sync_2[2];

            if (hdmi_occupied[3] && newest_slot != 2'd3)
                ack_toggle_next[3] = ready_sync_2[3];
        end
    end

    /*
     * HDMI changes frame only at an HDMI frame boundary.
     */
    always_ff @(posedge hdmi_clk) begin
        if (hdmi_reset) begin
            ack_toggle       <= 4'b0000;

            hdmi_frame_valid <= 1'b0;
            hdmi_read_slot   <= 2'd0;
            hdmi_frame_id    <= 32'd0;
        end else begin
            ack_toggle <= ack_toggle_next;

            if (hdmi_frame_start) begin
                if (newest_valid) begin
                    hdmi_frame_valid <= 1'b1;
                    hdmi_read_slot   <= newest_slot;
                    hdmi_frame_id    <= newest_id;
                end else begin
                    hdmi_frame_valid <= 1'b0;
                end
            end
        end
    end

endmodule
