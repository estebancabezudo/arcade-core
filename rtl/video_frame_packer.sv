module video_frame_packer #(
    parameter int unsigned SOURCE_WIDTH = 320,
    parameter int unsigned SOURCE_HEIGHT = 240
) (
    input  logic        clk,
    input  logic        reset,

    input  logic        in_valid,
    input  logic [15:0] in_rgb565,
    input  logic [8:0]  in_x,
    input  logic [8:0]  in_y,
    input  logic [1:0]  in_slot,

    output logic        out_valid,
    output logic [17:0] out_word_address,
    output logic [31:0] out_word_data
);

    logic [15:0] even_pixel;

    logic [17:0] address;
    logic        pixel_lane;

    video_frame_address #(
        .SOURCE_WIDTH  (SOURCE_WIDTH),
        .SOURCE_HEIGHT (SOURCE_HEIGHT)
    ) address_mapper (
        .slot         (in_slot),
        .x            (in_x),
        .y            (in_y),
        .word_address (address),
        .pixel_lane   (pixel_lane)
    );

    always_ff @(posedge clk) begin
        if (reset) begin
            even_pixel       <= 16'h0000;
            out_valid        <= 1'b0;
            out_word_address <= 18'd0;
            out_word_data    <= 32'd0;
        end else begin
            out_valid <= 1'b0;

            if (in_valid) begin
                if (!pixel_lane) begin
                    even_pixel <= in_rgb565;
                end else begin
                    /*
                     * bits 15:0  = even X
                     * bits 31:16 = odd X
                     */
                    out_word_address <= address;
                    out_word_data <= {
                        in_rgb565,
                        even_pixel
                    };
                    out_valid <= 1'b1;
                end
            end
        end
    end

endmodule
