module video_frame_line_fetcher #(
    parameter int unsigned SOURCE_WIDTH = 320,
    parameter int unsigned SOURCE_HEIGHT = 240
) (
    input  logic        clk,
    input  logic        reset,

    input  logic        start,
    input  logic [1:0]  request_slot,
    input  logic [8:0]  request_y,

    output logic        busy,
    output logic        line_valid,
    input  logic        release_line,

    output logic [1:0]  line_slot,
    output logic [8:0]  line_y,

    /* Upper coordinate bits are intentionally unused for narrower sources. */
    /* verilator lint_off UNUSEDSIGNAL */
    input  logic [8:0]  read_x,
    /* verilator lint_on UNUSEDSIGNAL */
    output logic [15:0] read_rgb565,

    output logic        mem_read_valid,
    input  logic        mem_read_ready,
    output logic [17:0] mem_read_address,

    input  logic        mem_response_valid,
    input  logic [31:0] mem_response_data
);

    localparam int unsigned WORDS_PER_LINE = SOURCE_WIDTH / 2;
    localparam int unsigned LINE_INDEX_WIDTH = $clog2(SOURCE_WIDTH);

    logic [15:0] line_buffer [0:SOURCE_WIDTH-1];

    logic [8:0] issued_words;
    logic [8:0] received_words;

    logic [17:0] mapped_base_address;
    /* Pixel zero always selects lane zero; retained for mapper completeness. */
    /* verilator lint_off UNUSEDSIGNAL */
    logic unused_pixel_lane;
    /* verilator lint_on UNUSEDSIGNAL */

    video_frame_address #(
        .SOURCE_WIDTH  (SOURCE_WIDTH),
        .SOURCE_HEIGHT (SOURCE_HEIGHT)
    ) address_mapper (
        .slot         (line_slot),
        .x            (9'd0),
        .y            (line_y),
        .word_address (mapped_base_address),
        .pixel_lane   (unused_pixel_lane)
    );

    always_comb begin
        mem_read_valid =
            busy &&
            (issued_words < 9'(WORDS_PER_LINE));

        mem_read_address =
            mapped_base_address +
            18'(issued_words);

        read_rgb565 = line_buffer[LINE_INDEX_WIDTH'(read_x)];
    end

    always_ff @(posedge clk) begin
        if (reset) begin
            busy           <= 1'b0;
            line_valid     <= 1'b0;

            line_slot      <= 2'd0;
            line_y         <= 9'd0;

            issued_words   <= 9'd0;
            received_words <= 9'd0;
        end else begin
            if (start && !busy && !line_valid) begin
                busy           <= 1'b1;
                line_valid     <= 1'b0;

                line_slot      <= request_slot;
                line_y         <= request_y;

                issued_words   <= 9'd0;
                received_words <= 9'd0;
            end

            if (mem_read_valid && mem_read_ready)
                issued_words <= issued_words + 1'b1;

            /* Responses arrive in request order: even pixel, then odd. */
            if (mem_response_valid && busy) begin
                line_buffer[
                    LINE_INDEX_WIDTH'({received_words[7:0], 1'b0})
                ] <=
                    mem_response_data[15:0];

                line_buffer[
                    LINE_INDEX_WIDTH'({received_words[7:0], 1'b1})
                ] <=
                    mem_response_data[31:16];

                if (received_words == 9'(WORDS_PER_LINE - 1)) begin
                    busy       <= 1'b0;
                    line_valid <= 1'b1;
                end else begin
                    received_words <= received_words + 1'b1;
                end
            end

            if (line_valid && release_line)
                line_valid <= 1'b0;
        end
    end

endmodule
