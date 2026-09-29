module video_frame_address #(
    parameter int unsigned SOURCE_WIDTH = 320,
    parameter int unsigned SOURCE_HEIGHT = 240
) (
    input  logic [1:0]  slot,
    input  logic [8:0]  x,
    input  logic [8:0]  y,

    output logic [17:0] word_address,
    output logic        pixel_lane
);

    localparam int unsigned WORDS_PER_LINE = SOURCE_WIDTH / 2;
    localparam int unsigned FRAME_WORDS =
        WORDS_PER_LINE * SOURCE_HEIGHT;
    localparam int unsigned ADDRESS_WORDS = 1 << 18;

    logic [17:0] slot_base;
    logic [17:0] row_offset;
    logic [17:0] column_offset;

    initial begin
        if (SOURCE_WIDTH == 0)
            $error("SOURCE_WIDTH must be greater than zero");

        if (SOURCE_HEIGHT == 0)
            $error("SOURCE_HEIGHT must be greater than zero");

        if ((SOURCE_WIDTH % 2) != 0)
            $error("SOURCE_WIDTH must be even for RGB565 word packing");

        if ((4 * FRAME_WORDS) > ADDRESS_WORDS)
            $error("Four framebuffers do not fit in the 18-bit address space");
    end

    always_comb begin
        case (slot)
            2'd0: slot_base = 18'd0;
            2'd1: slot_base = 18'(FRAME_WORDS);
            2'd2: slot_base = 18'(2 * FRAME_WORDS);
            2'd3: slot_base = 18'(3 * FRAME_WORDS);
        endcase

        row_offset = 18'(y * WORDS_PER_LINE);
        column_offset = 18'(x >> 1);

        word_address =
            slot_base +
            row_offset +
            column_offset;

        pixel_lane = x[0];
    end

endmodule
