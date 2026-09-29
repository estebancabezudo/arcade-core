module video_memory_command_queue (
    /*
     * Source clock domain.
     */
    input  logic        source_clk,
    input  logic        source_reset,

    input  logic        source_command_valid,
    output logic        source_command_ready,

    /*
     * Command types:
     *
     *   00 = WRITE
     *   01 = COMMIT
     *   10 = ABORT
     */
    input  logic [1:0]  source_command_type,

    input  logic [17:0] source_write_address,
    input  logic [31:0] source_write_data,

    input  logic [1:0]  source_marker_slot,
    input  logic [31:0] source_marker_frame_id,

    /*
     * Memory clock domain.
     */
    input  logic        memory_clk,
    input  logic        memory_reset,

    output logic        mem_write_valid,
    input  logic        mem_write_ready,

    output logic [17:0] mem_write_address,
    output logic [31:0] mem_write_data,

    /*
     * One pulse for every write that physically completes.
     */
    input  logic        mem_write_done,

    /*
     * Successful frame publication barrier.
     */
    output logic        commit_valid,
    output logic [1:0]  commit_slot,
    output logic [31:0] commit_frame_id,

    /*
     * Aborted frame barrier.
     *
     * Once emitted, all partial writes belonging to the aborted frame
     * have physically completed, so its slot can safely be reused.
     */
    output logic        abort_valid,
    output logic [1:0]  abort_slot,
    output logic [31:0] abort_frame_id,

    output logic        protocol_error
);

    localparam logic [1:0] COMMAND_WRITE  = 2'b00;
    localparam logic [1:0] COMMAND_COMMIT = 2'b01;
    localparam logic [1:0] COMMAND_ABORT  = 2'b10;

    /*
     * FIFO command format: 52 bits.
     *
     * WRITE:
     *
     *   [51:50] = 00
     *   [49:32] = 18-bit word address
     *   [31:0]  = 32-bit data
     *
     * COMMIT / ABORT:
     *
     *   [51:50] = command type
     *   [49:48] = slot
     *   [47:16] = frame ID
     *   [15:0]  = zero
     */
    logic [51:0] fifo_write_data;
    logic [51:0] fifo_read_data;

    logic fifo_read_valid;
    logic fifo_read_ready;

    logic [1:0] command_type;

    logic command_is_write;
    logic command_is_commit;
    logic command_is_abort;
    logic command_is_invalid;

    logic [15:0] outstanding_writes;

    logic write_accepted;
    logic write_completed;

    always_comb begin
        case (source_command_type)
            COMMAND_WRITE: begin
                fifo_write_data = {
                    COMMAND_WRITE,
                    source_write_address,
                    source_write_data
                };
            end

            COMMAND_COMMIT,
            COMMAND_ABORT: begin
                fifo_write_data = {
                    source_command_type,
                    source_marker_slot,
                    source_marker_frame_id,
                    16'd0
                };
            end

            default: begin
                fifo_write_data = {
                    2'b11,
                    50'd0
                };
            end
        endcase
    end

    async_fifo #(
        .DATA_WIDTH (52),
        .ADDR_WIDTH (9)
    ) command_fifo (
        .write_clk   (source_clk),
        .write_reset (source_reset),

        .write_valid (source_command_valid),
        .write_data  (fifo_write_data),
        .write_ready (source_command_ready),

        .read_clk    (memory_clk),
        .read_reset  (memory_reset),

        .read_valid  (fifo_read_valid),
        .read_data   (fifo_read_data),
        .read_ready  (fifo_read_ready)
    );

    always_comb begin
        command_type =
            fifo_read_data[51:50];

        command_is_write =
            command_type == COMMAND_WRITE;

        command_is_commit =
            command_type == COMMAND_COMMIT;

        command_is_abort =
            command_type == COMMAND_ABORT;

        command_is_invalid =
            command_type == 2'b11;

        mem_write_valid =
            fifo_read_valid &&
            command_is_write;

        mem_write_address =
            fifo_read_data[49:32];

        mem_write_data =
            fifo_read_data[31:0];

        /*
         * WRITE leaves the FIFO when memory accepts it.
         *
         * COMMIT and ABORT are barriers. They remain at the head of the
         * FIFO until every preceding accepted write has completed.
         *
         * Invalid commands are consumed to avoid deadlocking the queue and
         * raise protocol_error.
         */
        if (!fifo_read_valid) begin
            fifo_read_ready = 1'b0;
        end else if (command_is_write) begin
            fifo_read_ready = mem_write_ready;
        end else if (command_is_commit || command_is_abort) begin
            fifo_read_ready =
                outstanding_writes == 16'd0;
        end else begin
            fifo_read_ready = 1'b1;
        end

        write_accepted =
            mem_write_valid &&
            mem_write_ready;

        write_completed =
            mem_write_done;
    end

    always_ff @(posedge memory_clk) begin
        if (memory_reset) begin
            outstanding_writes <= 16'd0;

            commit_valid        <= 1'b0;
            commit_slot         <= 2'd0;
            commit_frame_id     <= 32'd0;

            abort_valid         <= 1'b0;
            abort_slot          <= 2'd0;
            abort_frame_id      <= 32'd0;

            protocol_error      <= 1'b0;
        end else begin
            commit_valid   <= 1'b0;
            abort_valid    <= 1'b0;
            protocol_error <= 1'b0;

            /*
             * Track memory operations that have been accepted but have not
             * physically completed yet.
             *
             * 10 = one new outstanding write
             * 01 = one write completed
             * 11 = one accepted and one completed: count unchanged
             */
            case ({
                write_accepted,
                write_completed
            })
                2'b10: begin
                    outstanding_writes <=
                        outstanding_writes + 1'b1;
                end

                2'b01: begin
                    if (outstanding_writes == 16'd0) begin
                        protocol_error <= 1'b1;
                    end else begin
                        outstanding_writes <=
                            outstanding_writes - 1'b1;
                    end
                end

                default: begin
                end
            endcase

            /*
             * Process barrier markers only after all preceding writes have
             * completed.
             */
            if (
                fifo_read_valid &&
                fifo_read_ready
            ) begin
                if (command_is_commit) begin
                    commit_valid <= 1'b1;

                    commit_slot <=
                        fifo_read_data[49:48];

                    commit_frame_id <=
                        fifo_read_data[47:16];
                end else if (command_is_abort) begin
                    abort_valid <= 1'b1;

                    abort_slot <=
                        fifo_read_data[49:48];

                    abort_frame_id <=
                        fifo_read_data[47:16];
                end else if (command_is_invalid) begin
                    protocol_error <= 1'b1;
                end
            end
        end
    end

endmodule
