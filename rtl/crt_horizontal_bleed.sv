module crt_horizontal_bleed #(
    parameter logic [7:0] SIDE_WEIGHT = 8'd8,
    parameter logic [7:0] CENTER_WEIGHT = 8'd240
) (
    input  logic [15:0] left_rgb565,
    input  logic [15:0] center_rgb565,
    input  logic [15:0] right_rgb565,

    output logic [15:0] out_rgb565
);

    /*
     * Subtle, energy-preserving horizontal beam spread:
     * left 3.125%, center 93.750%, right 3.125%.
     */
    logic [4:0] left_r;
    logic [5:0] left_g;
    logic [4:0] left_b;

    logic [4:0] center_r;
    logic [5:0] center_g;
    logic [4:0] center_b;

    logic [4:0] right_r;
    logic [5:0] right_g;
    logic [4:0] right_b;

    logic [12:0] red_left_product;
    logic [12:0] red_center_product;
    logic [12:0] red_right_product;

    logic [13:0] green_left_product;
    logic [13:0] green_center_product;
    logic [13:0] green_right_product;

    logic [12:0] blue_left_product;
    logic [12:0] blue_center_product;
    logic [12:0] blue_right_product;

    /* Low sum bits are fixed-point fractions discarded by design. */
    /* verilator lint_off UNUSEDSIGNAL */
    logic [12:0] red_sum;
    logic [13:0] green_sum;
    logic [12:0] blue_sum;
    /* verilator lint_on UNUSEDSIGNAL */

    always_comb begin
        left_r = left_rgb565[15:11];
        left_g = left_rgb565[10:5];
        left_b = left_rgb565[4:0];

        center_r = center_rgb565[15:11];
        center_g = center_rgb565[10:5];
        center_b = center_rgb565[4:0];

        right_r = right_rgb565[15:11];
        right_g = right_rgb565[10:5];
        right_b = right_rgb565[4:0];

        red_left_product   = left_r * SIDE_WEIGHT;
        red_center_product = center_r * CENTER_WEIGHT;
        red_right_product  = right_r * SIDE_WEIGHT;

        green_left_product   = left_g * SIDE_WEIGHT;
        green_center_product = center_g * CENTER_WEIGHT;
        green_right_product  = right_g * SIDE_WEIGHT;

        blue_left_product   = left_b * SIDE_WEIGHT;
        blue_center_product = center_b * CENTER_WEIGHT;
        blue_right_product  = right_b * SIDE_WEIGHT;

        red_sum =
            red_left_product +
            red_center_product +
            red_right_product;

        green_sum =
            green_left_product +
            green_center_product +
            green_right_product;

        blue_sum =
            blue_left_product +
            blue_center_product +
            blue_right_product;

        out_rgb565 = {
            red_sum[12:8],
            green_sum[13:8],
            blue_sum[12:8]
        };
    end

endmodule
