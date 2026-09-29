module crt_horizontal_bleed_stream #(
    parameter logic [7:0] SIDE_WEIGHT = 8'd8,
    parameter logic [7:0] CENTER_WEIGHT = 8'd240
) (
    input  logic        clk,
    input  logic        reset,

    input  logic        in_valid,
    input  logic [15:0] in_rgb565,
    input  logic [9:0]  in_x,
    input  logic        in_line_start,
    input  logic [1:0]  in_line_repeat,

    output logic        out_valid,
    output logic [15:0] out_rgb565,
    output logic [9:0]  out_x,
    output logic        out_line_start,
    output logic [1:0]  out_line_repeat
);

    logic        have_center;

    logic [15:0] left_pixel;
    logic [15:0] center_pixel;

    logic [9:0] center_x;
    logic       center_line_start;
    logic [1:0] center_line_repeat;

    logic [15:0] filter_right;
    logic [15:0] filtered_rgb565;

    /*
     * When the next input belongs to a new line, the current center pixel is
     * the final pixel of the previous line. Its right neighbour must therefore
     * be black rather than the first pixel of the new line.
     *
     * An in_valid gap also terminates the current line and flushes its final
     * pixel against black.
     */
    always_comb begin
        if (in_valid && !in_line_start)
            filter_right = in_rgb565;
        else
            filter_right = 16'h0000;
    end

    crt_horizontal_bleed #(
        .SIDE_WEIGHT   (SIDE_WEIGHT),
        .CENTER_WEIGHT (CENTER_WEIGHT)
    ) filter (
        .left_rgb565   (left_pixel),
        .center_rgb565 (center_pixel),
        .right_rgb565  (filter_right),
        .out_rgb565    (filtered_rgb565)
    );

    always_ff @(posedge clk) begin
        if (reset) begin
            have_center      <= 1'b0;

            left_pixel       <= 16'h0000;
            center_pixel     <= 16'h0000;

            center_x         <= 10'd0;
            center_line_start <= 1'b0;
            center_line_repeat <= 2'd0;

            out_valid        <= 1'b0;
            out_rgb565       <= 16'h0000;
            out_x            <= 10'd0;
            out_line_start   <= 1'b0;
            out_line_repeat  <= 2'd0;
        end else begin
            out_valid      <= 1'b0;
            out_line_start <= 1'b0;

            if (in_valid) begin
                if (!have_center) begin
                    /*
                     * First pixel of a stream or line after a gap.
                     * There is no complete 3-pixel window yet.
                     */
                    have_center       <= 1'b1;
                    left_pixel        <= 16'h0000;
                    center_pixel      <= in_rgb565;

                    center_x          <= in_x;
                    center_line_start <= in_line_start;
                    center_line_repeat <= in_line_repeat;
                end else begin
                    /*
                     * A new line starts: flush the final pixel of the previous
                     * line with a black right neighbour while simultaneously
                     * capturing the first pixel of the new line.
                     */
                    out_valid       <= 1'b1;
                    out_rgb565      <= filtered_rgb565;
                    out_x           <= center_x;
                    out_line_start  <= center_line_start;
                    out_line_repeat <= center_line_repeat;

                    if (in_line_start) begin
                        left_pixel   <= 16'h0000;
                        center_pixel <= in_rgb565;
                    end else begin
                        left_pixel   <= center_pixel;
                        center_pixel <= in_rgb565;
                    end

                    center_x           <= in_x;
                    center_line_start  <= in_line_start;
                    center_line_repeat <= in_line_repeat;
                end
            end else if (have_center) begin
                /*
                 * Flush the final buffered pixel when the input stream stops.
                 */
                out_valid       <= 1'b1;
                out_rgb565      <= filtered_rgb565;
                out_x           <= center_x;
                out_line_start  <= center_line_start;
                out_line_repeat <= center_line_repeat;

                have_center <= 1'b0;
                left_pixel  <= 16'h0000;
            end
        end
    end

endmodule
