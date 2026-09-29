module video_capture_writer #(
    parameter int unsigned SOURCE_WIDTH = 320,
    parameter int unsigned SOURCE_HEIGHT = 240
) (
    input  logic        clk,
    input  logic        reset,

    /*
     * frame_begin must occur before the first pixel of the frame.
     */
    input  logic        frame_begin,
    input  logic        frame_capture_enable,
    input  logic [1:0]  frame_slot,
    input  logic [31:0] frame_id,

    /*
     * Source raster.
     */
    input  logic        pixel_valid,
    input  logic [15:0] pixel_rgb565,
    input  logic [8:0]  pixel_x,
    input  logic [8:0]  pixel_y,

    /*
     * Assert after the final source pixel has been presented.
     */
    input  logic        frame_end,

    /*
     * Source side of video_memory_command_queue.
     */
    output logic        command_valid,
    input  logic        command_ready,

    output logic [1:0]  command_type,

    output logic [17:0] command_write_address,
    output logic [31:0] command_write_data,

    output logic [1:0]  command_marker_slot,
    output logic [31:0] command_marker_frame_id,

    /*
     * Status.
     */
    output logic        capture_active,
    output logic        capture_failed,
    output logic        protocol_error
);

    localparam logic [1:0] COMMAND_WRITE  = 2'b00;
    localparam logic [1:0] COMMAND_COMMIT = 2'b01;
    localparam logic [1:0] COMMAND_ABORT  = 2'b10;

    localparam int unsigned FRAME_WORDS =
        (SOURCE_WIDTH * SOURCE_HEIGHT) / 2;
    localparam int unsigned FRAME_WORD_COUNT_WIDTH =
        $clog2(FRAME_WORDS + 1);

    logic        capture_enabled;

    logic [1:0]  active_slot;
    logic [31:0] active_frame_id;

    logic        frame_end_seen;

    logic [FRAME_WORD_COUNT_WIDTH-1:0] generated_words;

    /*
     * Packer output.
     */
    logic        packer_valid;
    logic [17:0] packer_address;
    logic [31:0] packer_data;

    /*
     * One-entry skid buffer.
     *
     * This allows one generated SDRAM word to wait while the command FIFO
     * is temporarily unable to accept it.
     */
    logic        write_pending;
    logic [17:0] pending_address;
    logic [31:0] pending_data;

    logic        write_accepted;

    /*
     * Marker waiting to enter the command queue.
     */
    logic        marker_pending;
    logic [1:0]  marker_type;

    logic        marker_accepted;

    logic        packer_input_valid;

    /*
     * Once capture_failed is asserted, the raster keeps running but no
     * additional pixels are sent to the framebuffer packer.
     */
    always_comb begin
        packer_input_valid =
            capture_active &&
            capture_enabled &&
            !capture_failed &&
            pixel_valid;
    end

    video_frame_packer #(
        .SOURCE_WIDTH  (SOURCE_WIDTH),
        .SOURCE_HEIGHT (SOURCE_HEIGHT)
    ) packer (
        .clk              (clk),
        .reset            (reset),

        .in_valid         (packer_input_valid),
        .in_rgb565        (pixel_rgb565),
        .in_x             (pixel_x),
        .in_y             (pixel_y),
        .in_slot          (active_slot),

        .out_valid        (packer_valid),
        .out_word_address (packer_address),
        .out_word_data    (packer_data)
    );

    /*
     * Commands are emitted in strict order:
     *
     * pending WRITE first,
     * then COMMIT or ABORT marker.
     */
    always_comb begin
        command_valid = 1'b0;
        command_type  = COMMAND_WRITE;

        command_write_address = pending_address;
        command_write_data    = pending_data;

        command_marker_slot     = active_slot;
        command_marker_frame_id = active_frame_id;

        if (write_pending) begin
            command_valid = 1'b1;
            command_type  = COMMAND_WRITE;
        end else if (marker_pending) begin
            command_valid = 1'b1;
            command_type  = marker_type;
        end

        write_accepted =
            write_pending &&
            command_valid &&
            command_ready;

        marker_accepted =
            marker_pending &&
            !write_pending &&
            command_valid &&
            command_ready;
    end

    always_ff @(posedge clk) begin
        if (reset) begin
            capture_active   <= 1'b0;
            capture_enabled  <= 1'b0;
            capture_failed   <= 1'b0;

            active_slot      <= 2'd0;
            active_frame_id  <= 32'd0;

            frame_end_seen   <= 1'b0;
            generated_words  <= '0;

            write_pending    <= 1'b0;
            pending_address  <= 18'd0;
            pending_data     <= 32'd0;

            marker_pending   <= 1'b0;
            marker_type      <= COMMAND_WRITE;

            protocol_error   <= 1'b0;
        end else begin
            protocol_error <= 1'b0;

            /*
             * Begin a new source frame.
             *
             * The controller expects the previous frame's marker to have
             * entered the command queue before a new captured frame begins.
             */
            if (frame_begin) begin
                if (
                    capture_active ||
                    write_pending ||
                    marker_pending
                ) begin
                    protocol_error <= 1'b1;
                end else begin
                    capture_active  <= frame_capture_enable;
                    capture_enabled <= frame_capture_enable;
                    capture_failed  <= 1'b0;

                    active_slot     <= frame_slot;
                    active_frame_id <= frame_id;

                    frame_end_seen  <= 1'b0;
                    generated_words <= '0;
                end
            end

            /*
             * Consume an already-buffered WRITE when the command queue
             * becomes available.
             */
            if (write_accepted) begin
                write_pending <= 1'b0;
            end

            /*
             * Capture every 32-bit word produced by the RGB565 packer.
             *
             * If the one-entry skid buffer is still occupied, we cannot
             * retain this new word without stopping the source raster.
             * Therefore the current framebuffer is abandoned instead.
             */
            if (
                packer_valid &&
                capture_active &&
                capture_enabled &&
                !capture_failed
            ) begin
                if (
                    !write_pending ||
                    write_accepted
                ) begin
                    write_pending   <= 1'b1;
                    pending_address <= packer_address;
                    pending_data    <= packer_data;

                    generated_words <=
                        generated_words + 1'b1;
                end else begin
                    /*
                     * Do not stall the emulator.
                     *
                     * This word and all subsequent pixels of this frame are
                     * intentionally discarded. Earlier queued writes will
                     * drain before the ABORT barrier is emitted.
                     */
                    capture_failed <= 1'b1;
                end
            end

            /*
             * The raster itself continues independently of framebuffer
             * success.
             */
            if (frame_end && capture_active) begin
                frame_end_seen <= 1'b1;
            end

            /*
             * Failed capture:
             *
             * wait until the final retained WRITE has entered the queue,
             * then enqueue an ABORT barrier.
             */
            if (
                capture_active &&
                capture_failed &&
                !marker_pending &&
                !write_pending &&
                !packer_valid
            ) begin
                marker_pending <= 1'b1;
                marker_type    <= COMMAND_ABORT;
            end

            /*
             * Successful capture:
             *
             * exactly 38,400 packed words must have been generated.
             * The COMMIT marker is queued only after the final retained
             * WRITE has entered the command FIFO.
             */
            if (
                capture_active &&
                !capture_failed &&
                frame_end_seen &&
                (generated_words == FRAME_WORD_COUNT_WIDTH'(FRAME_WORDS)) &&
                !marker_pending &&
                !write_pending &&
                !packer_valid
            ) begin
                marker_pending <= 1'b1;
                marker_type    <= COMMAND_COMMIT;
            end

            /*
             * Once the marker enters video_memory_command_queue, this source
             * frame is finished from the capture writer's point of view.
             *
             * COMMIT/ABORT is still not visible to the frame manager until
             * all preceding SDRAM writes physically complete.
             */
            if (marker_accepted) begin
                marker_pending  <= 1'b0;

                capture_active  <= 1'b0;
                capture_enabled <= 1'b0;

                frame_end_seen  <= 1'b0;
            end
        end
    end

endmodule
