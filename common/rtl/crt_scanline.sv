module crt_scanline #(
    parameter int unsigned SCALE = 3,
    parameter logic [8:0] TOP_DARK_INTENSITY = 9'd154,
    parameter logic [8:0] BOTTOM_DARK_INTENSITY = 9'd179
) (
    input  logic [15:0] in_rgb565,
    input  logic [1:0]  line_repeat,

    output logic [15:0] out_rgb565
);

    logic [4:0] red_in;
    logic [5:0] green_in;
    logic [4:0] blue_in;

    logic [5:0] red_6;
    logic [5:0] blue_6;
    /* Low product bits are fixed-point fractions discarded by design. */
    /* verilator lint_off UNUSEDSIGNAL */
    logic [7:0] luminance_sum;
    logic [5:0] luminance;

    logic [8:0] intensity;
    logic [9:0] bottom_luminance_product;

    /*
     * Products use Q8 scaling. An intensity of 256 is exactly 100%, so the
     * centre line does not lose the least-significant channel value.
     */
    logic [13:0] red_product;
    logic [14:0] green_product;
    logic [13:0] blue_product;
    /* verilator lint_on UNUSEDSIGNAL */

    always_comb begin
        red_in   = in_rgb565[15:11];
        green_in = in_rgb565[10:5];
        blue_in  = in_rgb565[4:0];

        /* Expand R/B to six bits, then approximate (R + 2G + B) / 4. */
        red_6  = {red_in, red_in[4]};
        blue_6 = {blue_in, blue_in[4]};

        luminance_sum =
            {2'b00, red_6} +
            {1'b0, green_in, 1'b0} +
            {2'b00, blue_6};

        luminance = luminance_sum[7:2];

        /*
         * Luminance-dependent vertical beam profile:
         *
         *   dark:   approximately 60% / 100% / 70%
         *   middle: approximately 72% / 100% / 80%
         *   bright: approximately 85% / 100% / 90%
         */
        bottom_luminance_product =
            (luminance * 6'd13) + 10'd8;

        if (SCALE == 3) begin
            case (line_repeat)
                2'd0: begin
                    intensity =
                        TOP_DARK_INTENSITY +
                        {3'b000, luminance};
                end

                2'd1: intensity = 9'd256;

                2'd2: begin
                    intensity =
                        BOTTOM_DARK_INTENSITY +
                        {3'b000, bottom_luminance_product[9:4]};
                end

                default: intensity = 9'd0;
            endcase
        end else begin
            /*
             * Two-row scaling must not reuse the asymmetric first two rows
             * of the 3x profile. SCALE=2 is intentionally neutral and
             * symmetric; other supported integer scales use the same safe
             * fallback.
             */
            intensity = 9'd256;
        end

        red_product   = red_in * intensity;
        green_product = green_in * intensity;
        blue_product  = blue_in * intensity;

        out_rgb565 = {
            red_product[12:8],
            green_product[13:8],
            blue_product[12:8]
        };
    end

endmodule
