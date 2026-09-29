module video_memory_arbiter (
    /* Interface clock retained for symmetry; arbitration is combinational. */
    /* verilator lint_off UNUSEDSIGNAL */
    input  logic        clk,
    /* verilator lint_on UNUSEDSIGNAL */
    input  logic        reset,

    /*
     * Write client.
     */
    input  logic        write_valid,
    output logic        write_ready,

    input  logic [17:0] write_address,
    input  logic [31:0] write_data,

    output logic        write_done,

    /*
     * Read client.
     */
    input  logic        read_valid,
    output logic        read_ready,

    input  logic [17:0] read_address,

    output logic        read_response_valid,
    output logic [31:0] read_response_data,

    /*
     * Abstract single-port memory controller.
     */
    output logic        memory_command_valid,
    input  logic        memory_command_ready,

    output logic        memory_command_write,
    output logic [17:0] memory_command_address,
    output logic [31:0] memory_command_write_data,

    input  logic        memory_write_done,

    input  logic        memory_read_response_valid,
    input  logic [31:0] memory_read_response_data
);

    always_comb begin
        memory_command_valid      = 1'b0;
        memory_command_write      = 1'b0;
        memory_command_address    = 18'd0;
        memory_command_write_data = 32'd0;

        /*
         * READY must not depend on the corresponding VALID signal.
         *
         * Reads have strict priority:
         *
         *   read_ready  = controller can accept a command
         *   write_ready = controller can accept a command AND no read waits
         *
         * This breaks the combinational loop:
         *
         *   write_valid -> arbiter -> write_ready -> queue -> write_valid
         */
        read_ready =
            !reset &&
            memory_command_ready;

        write_ready =
            !reset &&
            !read_valid &&
            memory_command_ready;

        if (!reset) begin
            if (read_valid) begin
                memory_command_valid   = 1'b1;
                memory_command_write   = 1'b0;
                memory_command_address = read_address;
            end else if (write_valid) begin
                memory_command_valid      = 1'b1;
                memory_command_write      = 1'b1;
                memory_command_address    = write_address;
                memory_command_write_data = write_data;
            end
        end

        /*
         * Completion channels are already typed by the physical memory
         * controller and can be forwarded directly.
         */
        write_done =
            memory_write_done;

        read_response_valid =
            memory_read_response_valid;

        read_response_data =
            memory_read_response_data;
    end

endmodule
