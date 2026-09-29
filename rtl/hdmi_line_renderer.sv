module hdmi_line_renderer #(
    parameter int unsigned SOURCE_WIDTH = 320,
    parameter int unsigned SOURCE_HEIGHT = 240,
    parameter int unsigned SCALE = 3
) (
    input  logic        timing_active,
    input  logic [10:0] timing_x,
    input  logic [9:0]  timing_y,

    input  logic        cache_valid,
    input  logic [15:0] cache_rgb565,

    output logic [8:0]  cache_read_x,
    output logic        release_line,

    output logic        content_active,
    output logic [9:0]  content_x,
    output logic [1:0]  horizontal_repeat,
    output logic [1:0]  vertical_repeat,

    output logic [15:0] out_rgb565
);

    localparam int unsigned HDMI_WIDTH = 1280;
    localparam int unsigned HDMI_HEIGHT = 720;
    localparam int unsigned CONTENT_WIDTH = SOURCE_WIDTH * SCALE;
    localparam int unsigned CONTENT_HEIGHT = SOURCE_HEIGHT * SCALE;
    localparam int unsigned CONTENT_X_START =
        (HDMI_WIDTH - CONTENT_WIDTH) / 2;
    localparam int unsigned CONTENT_Y_START =
        (HDMI_HEIGHT - CONTENT_HEIGHT) / 2;
    localparam int unsigned CONTENT_X_END =
        CONTENT_X_START + CONTENT_WIDTH;
    logic [10:0] content_x_wide;
    logic [9:0]  content_y_wide;

    initial begin
        if (SCALE == 0)
            $error("SCALE must be greater than zero");

        if (SCALE > 4)
            $error("SCALE exceeds the two-bit repeat metadata");

        if (CONTENT_WIDTH > HDMI_WIDTH)
            $error("Scaled content exceeds the HDMI width");

        if (CONTENT_HEIGHT > HDMI_HEIGHT)
            $error("Scaled content exceeds the HDMI height");
    end

    assign content_active =
        timing_active &&
        cache_valid &&
        (timing_x >= 11'(CONTENT_X_START)) &&
        (timing_x < 11'(CONTENT_X_END)) &&
        ((timing_y - 10'(CONTENT_Y_START)) < 10'(CONTENT_HEIGHT));

    assign content_x_wide =
        content_active
            ? timing_x - 11'(CONTENT_X_START)
            : 11'd0;

    assign content_y_wide =
        content_active
            ? timing_y - 10'(CONTENT_Y_START)
            : 10'd0;

    assign content_x = content_x_wide[9:0];

    assign cache_read_x =
        content_active
            ? 9'(content_x_wide / 11'(SCALE))
            : 9'd0;

    assign horizontal_repeat =
        content_active
            ? 2'(content_x_wide % 11'(SCALE))
            : 2'd0;

    assign vertical_repeat =
        content_active
            ? 2'(content_y_wide % 10'(SCALE))
            : 2'd0;

    assign out_rgb565 =
        content_active
            ? cache_rgb565
            : 16'h0000;

    assign release_line =
        content_active &&
        (timing_x == 11'(CONTENT_X_END - 1)) &&
        (vertical_repeat == 2'(SCALE - 1));

endmodule
