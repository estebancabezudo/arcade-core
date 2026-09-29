module tangnano20k_video_shell #(
    parameter int unsigned SOURCE_WIDTH = 320,
    parameter int unsigned SOURCE_HEIGHT = 240,
    parameter int unsigned SCALE = 3
) (
    /* RP2350-provided source domain. */
    input  logic        rp2350_source_clk,
    input  logic        rp2350_source_reset,
    input  logic        rp2350_pixel_valid,
    input  logic [15:0] rp2350_rgb565,
    input  logic [8:0]  rp2350_x,
    input  logic [8:0]  rp2350_y,
    input  logic        rp2350_frame_start,
    input  logic        rp2350_frame_complete,

    /* 74.25 MHz domain supplied by a board-specific Gowin PLL wrapper. */
    input  logic        video_clk,
    input  logic        video_reset,

    /* Abstract 32-bit memory controller boundary. */
    output logic        memory_command_valid,
    input  logic        memory_command_ready,
    output logic        memory_command_write,
    output logic [17:0] memory_command_address,
    output logic [31:0] memory_command_write_data,
    input  logic        memory_write_done,
    input  logic        memory_read_response_valid,
    input  logic [31:0] memory_read_response_data,

    /* Parallel raster boundary for the board-specific TMDS wrapper. */
    output logic        hdmi_active,
    output logic        hdmi_hsync,
    output logic        hdmi_vsync,
    output logic [15:0] hdmi_rgb565,

    output logic        line_underflow,
    output logic        source_frame_dropped,
    output logic        capture_failed,
    output logic        video_error
);

    logic        source_pixel_valid;
    logic [15:0] source_rgb565;
    logic [8:0]  source_x;
    logic [8:0]  source_y;
    logic        source_frame_start;
    logic        source_frame_complete;

    logic manager_error;
    logic memory_queue_error;
    logic result_bridge_error;

    /* Debug-only metadata retained at the hardware integration boundary. */
    /* verilator lint_off UNUSEDSIGNAL */
    logic [10:0] hdmi_x;
    logic [9:0]  hdmi_y;
    logic        framebuffer_valid;
    logic [1:0]  framebuffer_slot;
    logic [31:0] framebuffer_id;
    /* verilator lint_on UNUSEDSIGNAL */

    rp2350_video_ingress ingress (
        .rp2350_pixel_valid    (rp2350_pixel_valid),
        .rp2350_rgb565         (rp2350_rgb565),
        .rp2350_x              (rp2350_x),
        .rp2350_y              (rp2350_y),
        .rp2350_frame_start    (rp2350_frame_start),
        .rp2350_frame_complete (rp2350_frame_complete),

        .source_pixel_valid    (source_pixel_valid),
        .source_rgb565         (source_rgb565),
        .source_x              (source_x),
        .source_y              (source_y),
        .source_frame_start    (source_frame_start),
        .source_frame_complete (source_frame_complete)
    );

    video_system #(
        .SOURCE_WIDTH  (SOURCE_WIDTH),
        .SOURCE_HEIGHT (SOURCE_HEIGHT),
        .SCALE         (SCALE)
    ) video (
        .source_clk                (rp2350_source_clk),
        .source_reset              (rp2350_source_reset),
        .source_frame_start        (source_frame_start),
        .source_frame_complete     (source_frame_complete),
        .pixel_valid               (source_pixel_valid),
        .pixel_rgb565              (source_rgb565),
        .pixel_x                   (source_x),
        .pixel_y                   (source_y),

        .video_clk                 (video_clk),
        .video_reset               (video_reset),

        .memory_command_valid      (memory_command_valid),
        .memory_command_ready      (memory_command_ready),
        .memory_command_write      (memory_command_write),
        .memory_command_address    (memory_command_address),
        .memory_command_write_data (memory_command_write_data),
        .memory_write_done         (memory_write_done),
        .memory_read_response_valid(memory_read_response_valid),
        .memory_read_response_data (memory_read_response_data),

        .hdmi_active               (hdmi_active),
        .hdmi_hsync                (hdmi_hsync),
        .hdmi_vsync                (hdmi_vsync),
        .hdmi_rgb565               (hdmi_rgb565),
        .hdmi_x                    (hdmi_x),
        .hdmi_y                    (hdmi_y),

        .framebuffer_valid         (framebuffer_valid),
        .framebuffer_slot          (framebuffer_slot),
        .framebuffer_id            (framebuffer_id),

        .line_underflow            (line_underflow),
        .source_frame_dropped      (source_frame_dropped),
        .capture_failed            (capture_failed),
        .manager_error             (manager_error),
        .memory_queue_error        (memory_queue_error),
        .result_bridge_error       (result_bridge_error)
    );

    always_comb begin
        video_error =
            manager_error ||
            memory_queue_error ||
            result_bridge_error;
    end

endmodule
