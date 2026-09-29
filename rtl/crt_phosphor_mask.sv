module crt_phosphor_mask #(
    parameter logic [8:0] PRIMARY_INTENSITY = 9'd256,
    parameter logic [8:0] SECONDARY_INTENSITY = 9'd241
) (
    input  logic [15:0] in_rgb565,
    input  logic [9:0]  x,

    output logic [15:0] out_rgb565
);

    logic [4:0] red_in;
    logic [5:0] green_in;
    logic [4:0] blue_in;

    /* Low product bits are fixed-point fractions discarded by design. */
    /* verilator lint_off UNUSEDSIGNAL */
    logic [13:0] red_product;
    logic [14:0] green_product;
    logic [13:0] blue_product;
    /* verilator lint_on UNUSEDSIGNAL */

    logic [8:0] red_intensity;
    logic [8:0] green_intensity;
    logic [8:0] blue_intensity;

    always_comb begin
        red_in   = in_rgb565[15:11];
        green_in = in_rgb565[10:5];
        blue_in  = in_rgb565[4:0];

        /*
         * Subtle aperture-grille RGB pattern. The dominant phosphor is
         * exactly 100%; the other two are 241/256, approximately 94%.
         */
        case (x % 3)
            0: begin
                red_intensity   = PRIMARY_INTENSITY;
                green_intensity = SECONDARY_INTENSITY;
                blue_intensity  = SECONDARY_INTENSITY;
            end

            1: begin
                red_intensity   = SECONDARY_INTENSITY;
                green_intensity = PRIMARY_INTENSITY;
                blue_intensity  = SECONDARY_INTENSITY;
            end

            default: begin
                red_intensity   = SECONDARY_INTENSITY;
                green_intensity = SECONDARY_INTENSITY;
                blue_intensity  = PRIMARY_INTENSITY;
            end
        endcase

        red_product   = red_in * red_intensity;
        green_product = green_in * green_intensity;
        blue_product  = blue_in * blue_intensity;

        out_rgb565 = {
            red_product[12:8],
            green_product[13:8],
            blue_product[12:8]
        };
    end

endmodule
