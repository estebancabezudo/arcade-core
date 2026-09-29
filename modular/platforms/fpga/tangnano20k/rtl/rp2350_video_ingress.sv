module rp2350_video_ingress (
    input  logic        rp2350_pixel_valid,
    input  logic [15:0] rp2350_rgb565,
    input  logic [8:0]  rp2350_x,
    input  logic [8:0]  rp2350_y,
    input  logic        rp2350_frame_start,
    input  logic        rp2350_frame_complete,

    output logic        source_pixel_valid,
    output logic [15:0] source_rgb565,
    output logic [8:0]  source_x,
    output logic [8:0]  source_y,
    output logic        source_frame_start,
    output logic        source_frame_complete
);

    /*
     * Deliberately no ready/backpressure signal is returned to the RP2350.
     * The FPGA capture path either accepts the raster or aborts only the
     * affected framebuffer while the arcade machine continues to run.
     * All inputs are synchronous to the source clock at the shell boundary.
     */
    always_comb begin
        source_pixel_valid   = rp2350_pixel_valid;
        source_rgb565        = rp2350_rgb565;
        source_x             = rp2350_x;
        source_y             = rp2350_y;
        source_frame_start   = rp2350_frame_start;
        source_frame_complete = rp2350_frame_complete;
    end

endmodule
