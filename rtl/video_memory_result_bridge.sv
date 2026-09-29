module video_memory_result_bridge (
    /*
     * Memory clock domain.
     */
    input  logic        memory_clk,
    input  logic        memory_reset,

    input  logic        memory_commit_valid,
    input  logic [1:0]  memory_commit_slot,
    input  logic [31:0] memory_commit_frame_id,

    input  logic        memory_abort_valid,
    input  logic [1:0]  memory_abort_slot,
    input  logic [31:0] memory_abort_frame_id,

    output logic        memory_result_ready,
    output logic        memory_protocol_error,

    /*
     * Source clock domain.
     */
    input  logic        source_clk,
    input  logic        source_reset,

    output logic        source_commit_valid,
    output logic [1:0]  source_commit_slot,
    output logic [31:0] source_commit_frame_id,

    output logic        source_abort_valid,
    output logic [1:0]  source_abort_slot,
    output logic [31:0] source_abort_frame_id
);

    /*
     * Result format:
     *
     *   [34]    = 0 COMMIT / 1 ABORT
     *   [33:32] = slot
     *   [31:0]  = frame ID
     */
    logic [34:0] fifo_write_data;
    logic        fifo_write_valid;
    logic        fifo_write_ready;

    logic [34:0] fifo_read_data;
    logic        fifo_read_valid;

    always_comb begin
        /*
         * A producer must never assert COMMIT and ABORT together.
         */
        fifo_write_valid =
            memory_commit_valid ^
            memory_abort_valid;

        if (memory_abort_valid) begin
            fifo_write_data = {
                1'b1,
                memory_abort_slot,
                memory_abort_frame_id
            };
        end else begin
            fifo_write_data = {
                1'b0,
                memory_commit_slot,
                memory_commit_frame_id
            };
        end

        memory_result_ready =
            fifo_write_ready;
    end

    /*
     * Validate the memory-side producer.
     */
    always_ff @(posedge memory_clk) begin
        if (memory_reset) begin
            memory_protocol_error <= 1'b0;
        end else begin
            memory_protocol_error <= 1'b0;

            if (
                memory_commit_valid &&
                memory_abort_valid
            ) begin
                memory_protocol_error <= 1'b1;
            end else if (
                (memory_commit_valid ||
                 memory_abort_valid) &&
                !fifo_write_ready
            ) begin
                memory_protocol_error <= 1'b1;
            end
        end
    end

    async_fifo #(
        .DATA_WIDTH (35),
        .ADDR_WIDTH (3)
    ) result_fifo (
        .write_clk   (memory_clk),
        .write_reset (memory_reset),

        .write_valid (fifo_write_valid),
        .write_data  (fifo_write_data),
        .write_ready (fifo_write_ready),

        .read_clk    (source_clk),
        .read_reset  (source_reset),

        .read_valid  (fifo_read_valid),
        .read_data   (fifo_read_data),
        .read_ready  (1'b1)
    );

    /*
     * Register every FIFO result in the source clock domain.
     *
     * source_commit_valid and source_abort_valid are clean one-clock pulses.
     */
    always_ff @(posedge source_clk) begin
        if (source_reset) begin
            source_commit_valid    <= 1'b0;
            source_commit_slot     <= 2'd0;
            source_commit_frame_id <= 32'd0;

            source_abort_valid     <= 1'b0;
            source_abort_slot      <= 2'd0;
            source_abort_frame_id  <= 32'd0;
        end else begin
            source_commit_valid <= 1'b0;
            source_abort_valid  <= 1'b0;

            if (fifo_read_valid) begin
                if (fifo_read_data[34]) begin
                    source_abort_valid <= 1'b1;

                    source_abort_slot <=
                        fifo_read_data[33:32];

                    source_abort_frame_id <=
                        fifo_read_data[31:0];
                end else begin
                    source_commit_valid <= 1'b1;

                    source_commit_slot <=
                        fifo_read_data[33:32];

                    source_commit_frame_id <=
                        fifo_read_data[31:0];
                end
            end
        end
    end

endmodule
