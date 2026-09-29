module async_fifo #(
    parameter int unsigned DATA_WIDTH = 50,
    parameter int unsigned ADDR_WIDTH = 9
) (
    input  logic                  write_clk,
    input  logic                  write_reset,

    input  logic                  write_valid,
    input  logic [DATA_WIDTH-1:0] write_data,
    output logic                  write_ready,

    input  logic                  read_clk,
    input  logic                  read_reset,

    output logic                  read_valid,
    output logic [DATA_WIDTH-1:0] read_data,
    input  logic                  read_ready
);

    localparam int unsigned DEPTH = 1 << ADDR_WIDTH;
    localparam int unsigned PTR_WIDTH = ADDR_WIDTH + 1;

    localparam logic [PTR_WIDTH-1:0] PTR_ONE =
        {{(PTR_WIDTH - 1){1'b0}}, 1'b1};

    logic [DATA_WIDTH-1:0] memory [0:DEPTH-1];

    logic [PTR_WIDTH-1:0] write_binary;
    logic [PTR_WIDTH-1:0] write_gray;
    logic [PTR_WIDTH-1:0] write_binary_next;
    logic [PTR_WIDTH-1:0] write_gray_next;

    logic [PTR_WIDTH-1:0] read_binary;
    logic [PTR_WIDTH-1:0] read_gray;
    logic [PTR_WIDTH-1:0] read_binary_next;
    logic [PTR_WIDTH-1:0] read_gray_next;

    logic [PTR_WIDTH-1:0] read_gray_sync_1;
    logic [PTR_WIDTH-1:0] read_gray_sync_2;

    logic [PTR_WIDTH-1:0] write_gray_sync_1;
    logic [PTR_WIDTH-1:0] write_gray_sync_2;

    logic write_full;
    logic write_full_next;

    logic read_empty;
    logic read_empty_next;

    function automatic logic [PTR_WIDTH-1:0] binary_to_gray(
        input logic [PTR_WIDTH-1:0] value
    );
        return (value >> 1) ^ value;
    endfunction

    always_comb begin
        write_binary_next = write_binary;

        if (write_valid && !write_full)
            write_binary_next = write_binary + PTR_ONE;

        write_gray_next =
            binary_to_gray(write_binary_next);

        write_full_next =
            write_gray_next ==
            {
                ~read_gray_sync_2[PTR_WIDTH-1:PTR_WIDTH-2],
                read_gray_sync_2[PTR_WIDTH-3:0]
            };

        write_ready = !write_full;
    end

    always_comb begin
        read_binary_next = read_binary;

        if (read_ready && !read_empty)
            read_binary_next = read_binary + PTR_ONE;

        read_gray_next =
            binary_to_gray(read_binary_next);

        read_empty_next =
            read_gray_next == write_gray_sync_2;

        read_valid = !read_empty;

        read_data =
            memory[read_binary[ADDR_WIDTH-1:0]];
    end

    /*
     * Write domain.
     */
    always_ff @(posedge write_clk) begin
        if (write_reset) begin
            write_binary <= '0;
            write_gray   <= '0;
            write_full   <= 1'b0;
        end else begin
            if (write_valid && !write_full) begin
                memory[write_binary[ADDR_WIDTH-1:0]] <=
                    write_data;
            end

            write_binary <= write_binary_next;
            write_gray   <= write_gray_next;
            write_full   <= write_full_next;
        end
    end

    /*
     * Read pointer synchronization into write domain.
     */
    always_ff @(posedge write_clk) begin
        if (write_reset) begin
            read_gray_sync_1 <= '0;
            read_gray_sync_2 <= '0;
        end else begin
            read_gray_sync_1 <= read_gray;
            read_gray_sync_2 <= read_gray_sync_1;
        end
    end

    /*
     * Read domain.
     */
    always_ff @(posedge read_clk) begin
        if (read_reset) begin
            read_binary <= '0;
            read_gray   <= '0;
            read_empty  <= 1'b1;
        end else begin
            read_binary <= read_binary_next;
            read_gray   <= read_gray_next;
            read_empty  <= read_empty_next;
        end
    end

    /*
     * Write pointer synchronization into read domain.
     */
    always_ff @(posedge read_clk) begin
        if (read_reset) begin
            write_gray_sync_1 <= '0;
            write_gray_sync_2 <= '0;
        end else begin
            write_gray_sync_1 <= write_gray;
            write_gray_sync_2 <= write_gray_sync_1;
        end
    end

endmodule
