module video_system #(
    parameter int unsigned SOURCE_WIDTH = 320,
    parameter int unsigned SOURCE_HEIGHT = 240,
    parameter int unsigned SCALE = 3,
    parameter logic [8:0] BEAM_TOP_DARK_INTENSITY = 9'd154,
    parameter logic [8:0] BEAM_BOTTOM_DARK_INTENSITY = 9'd179,
    parameter logic [7:0] BLEED_SIDE_WEIGHT = 8'd8,
    parameter logic [7:0] BLEED_CENTER_WEIGHT = 8'd240,
    parameter logic [8:0] MASK_PRIMARY_INTENSITY = 9'd256,
    parameter logic [8:0] MASK_SECONDARY_INTENSITY = 9'd241
) (
    input  logic        source_clk,
    input  logic        source_reset,

    input  logic        source_frame_start,
    input  logic        source_frame_complete,

    input  logic        pixel_valid,
    input  logic [15:0] pixel_rgb565,
    input  logic [8:0]  pixel_x,
    input  logic [8:0]  pixel_y,

    input  logic        video_clk,
    input  logic        video_reset,

    output logic        memory_command_valid,
    input  logic        memory_command_ready,

    output logic        memory_command_write,
    output logic [17:0] memory_command_address,
    output logic [31:0] memory_command_write_data,

    input  logic        memory_write_done,

    input  logic        memory_read_response_valid,
    input  logic [31:0] memory_read_response_data,

    /* 720p raster before TMDS encoding. */
    output logic        hdmi_active,
    output logic        hdmi_hsync,
    output logic        hdmi_vsync,
    output logic [15:0] hdmi_rgb565,

    output logic [10:0] hdmi_x,
    output logic [9:0]  hdmi_y,

    output logic        framebuffer_valid,
    output logic [1:0]  framebuffer_slot,
    output logic [31:0] framebuffer_id,

    output logic        line_underflow,
    output logic        source_frame_dropped,
    output logic        capture_failed,

    output logic        manager_error,
    output logic        memory_queue_error,
    output logic        result_bridge_error
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

    initial begin
        if (SOURCE_WIDTH == 0)
            $error("SOURCE_WIDTH must be greater than zero");

        if (SOURCE_HEIGHT == 0)
            $error("SOURCE_HEIGHT must be greater than zero");

        if (SCALE == 0)
            $error("SCALE must be greater than zero");

        if ((SOURCE_WIDTH % 2) != 0)
            $error("SOURCE_WIDTH must be even for RGB565 word packing");

        if (CONTENT_WIDTH > HDMI_WIDTH)
            $error("Scaled content exceeds 1280 active pixels");

        if (CONTENT_HEIGHT > HDMI_HEIGHT)
            $error("Scaled content exceeds 720 active lines");
    end

    logic        timing_active;
    logic        timing_hsync;
    logic        timing_vsync;
    logic [10:0] timing_x;
    logic [9:0]  timing_y;
    logic        timing_line_start;

    /* The raw frame_start output is useful for waveform/debug correlation. */
    /* verilator lint_off UNUSEDSIGNAL */
    logic timing_frame_start;
    /* verilator lint_on UNUSEDSIGNAL */

    logic frame_select_pulse;

    assign frame_select_pulse =
        timing_line_start &&
        (timing_y == 10'd720);

    video_720p_timing timing (
        .clk         (video_clk),
        .reset       (video_reset),

        .x           (timing_x),
        .y           (timing_y),

        .active      (timing_active),
        .hsync       (timing_hsync),
        .vsync       (timing_vsync),

        .line_start  (timing_line_start),
        .frame_start (timing_frame_start)
    );

    logic        line_fetch_start;
    logic [8:0]  line_fetch_y;

    logic        line_busy;
    logic        line_valid;
    logic        line_release;

    /* line_slot is retained for waveform inspection of fetch ownership. */
    /* verilator lint_off UNUSEDSIGNAL */
    logic [1:0] line_slot;
    /* verilator lint_on UNUSEDSIGNAL */
    logic [8:0] fetched_line_y;

    logic [8:0]  line_read_x;
    logic [15:0] line_rgb565;

    video_framebuffer_pipeline #(
        .SOURCE_WIDTH  (SOURCE_WIDTH),
        .SOURCE_HEIGHT (SOURCE_HEIGHT)
    ) framebuffer (
        .source_clk               (source_clk),
        .source_reset             (source_reset),

        .source_frame_start       (source_frame_start),
        .source_frame_complete    (source_frame_complete),

        .pixel_valid              (pixel_valid),
        .pixel_rgb565             (pixel_rgb565),
        .pixel_x                  (pixel_x),
        .pixel_y                  (pixel_y),

        .video_clk                (video_clk),
        .video_reset              (video_reset),

        .hdmi_frame_start         (frame_select_pulse),

        .hdmi_frame_valid         (framebuffer_valid),
        .hdmi_read_slot           (framebuffer_slot),
        .hdmi_frame_id            (framebuffer_id),

        .line_fetch_start         (line_fetch_start),
        .line_fetch_y             (line_fetch_y),

        .line_busy                (line_busy),
        .line_valid               (line_valid),
        .line_release             (line_release),

        .line_slot                (line_slot),
        .line_y                   (fetched_line_y),

        .line_read_x              (line_read_x),
        .line_rgb565              (line_rgb565),

        .memory_command_valid     (memory_command_valid),
        .memory_command_ready     (memory_command_ready),

        .memory_command_write     (memory_command_write),
        .memory_command_address   (memory_command_address),
        .memory_command_write_data(memory_command_write_data),

        .memory_write_done        (memory_write_done),

        .memory_read_response_valid(memory_read_response_valid),
        .memory_read_response_data(memory_read_response_data),

        .source_frame_dropped     (source_frame_dropped),
        .capture_failed           (capture_failed),

        .manager_error            (manager_error),
        .memory_queue_error       (memory_queue_error),
        .result_bridge_error      (result_bridge_error)
    );

    logic        content_active;
    logic [15:0] content_rgb565;
    logic [9:0]  content_x;
    logic [1:0]  horizontal_repeat;
    logic [1:0]  vertical_repeat;

    hdmi_scanout_controller #(
        .SOURCE_WIDTH  (SOURCE_WIDTH),
        .SOURCE_HEIGHT (SOURCE_HEIGHT),
        .SCALE         (SCALE)
    ) scanout (
        .clk                 (video_clk),
        .reset               (video_reset),

        .timing_active       (timing_active),
        .timing_x            (timing_x),
        .timing_y            (timing_y),

        .frame_select_pulse  (frame_select_pulse),
        .frame_valid         (framebuffer_valid),

        .line_fetch_start    (line_fetch_start),
        .line_fetch_y        (line_fetch_y),

        .line_busy           (line_busy),
        .line_valid          (line_valid),
        .line_y              (fetched_line_y),

        .line_release        (line_release),

        .line_read_x         (line_read_x),
        .line_rgb565         (line_rgb565),

        .content_active      (content_active),
        .content_rgb565      (content_rgb565),

        .content_x           (content_x),
        .horizontal_repeat   (horizontal_repeat),
        .vertical_repeat     (vertical_repeat),

        .line_underflow      (line_underflow)
    );

    logic [15:0] beam_rgb565;

    crt_scanline #(
        .SCALE                 (SCALE),
        .TOP_DARK_INTENSITY    (BEAM_TOP_DARK_INTENSITY),
        .BOTTOM_DARK_INTENSITY (BEAM_BOTTOM_DARK_INTENSITY)
    ) beam_filter (
        .in_rgb565   (content_rgb565),
        .line_repeat (vertical_repeat),
        .out_rgb565  (beam_rgb565)
    );

    logic        bleed_valid;
    logic [15:0] bleed_rgb565;
    logic [9:0]  bleed_x;

    /* Metadata retained to make line/frame isolation visible in waveforms. */
    /* verilator lint_off UNUSEDSIGNAL */
    logic       bleed_line_start;
    logic [1:0] bleed_line_repeat;
    /* verilator lint_on UNUSEDSIGNAL */

    crt_horizontal_bleed_stream #(
        .SIDE_WEIGHT   (BLEED_SIDE_WEIGHT),
        .CENTER_WEIGHT (BLEED_CENTER_WEIGHT)
    ) bleed_filter (
        .clk             (video_clk),
        .reset           (video_reset),

        .in_valid        (content_active),
        .in_rgb565       (beam_rgb565),
        .in_x            (content_x),
        .in_line_start   (
            content_active &&
            (content_x == 10'd0) &&
            (horizontal_repeat == 2'd0)
        ),
        .in_line_repeat  (vertical_repeat),

        .out_valid       (bleed_valid),
        .out_rgb565      (bleed_rgb565),
        .out_x           (bleed_x),
        .out_line_start  (bleed_line_start),
        .out_line_repeat (bleed_line_repeat)
    );

    logic [15:0] masked_rgb565;

    logic        raster_active_delay;
    logic        raster_hsync_delay;
    logic        raster_vsync_delay;
    logic [10:0] raster_x_delay;
    logic [9:0]  raster_y_delay;

    crt_phosphor_mask #(
        .PRIMARY_INTENSITY   (MASK_PRIMARY_INTENSITY),
        .SECONDARY_INTENSITY (MASK_SECONDARY_INTENSITY)
    ) phosphor_filter (
        .in_rgb565  (bleed_rgb565),
        .x          (bleed_x),
        .out_rgb565 (masked_rgb565)
    );

    /*
     * The symmetric bleed filter needs the right neighbour and therefore
     * contributes exactly one video-clock of latency. Delay the raster
     * metadata by the same amount so the first and last content pixels stay
     * at their centred coordinates. No extra framebuffer or scanline is
     * introduced.
     */
    always_ff @(posedge video_clk) begin
        if (video_reset) begin
            raster_active_delay <= 1'b0;
            raster_hsync_delay  <= 1'b0;
            raster_vsync_delay  <= 1'b0;
            raster_x_delay      <= 11'd0;
            raster_y_delay      <= 10'd0;

            hdmi_active <= 1'b0;
            hdmi_hsync  <= 1'b0;
            hdmi_vsync  <= 1'b0;
            hdmi_x      <= 11'd0;
            hdmi_y      <= 10'd0;
        end else begin
            raster_active_delay <= timing_active;
            raster_hsync_delay  <= timing_hsync;
            raster_vsync_delay  <= timing_vsync;
            raster_x_delay      <= timing_x;
            raster_y_delay      <= timing_y;

            hdmi_active <= raster_active_delay;
            hdmi_hsync  <= raster_hsync_delay;
            hdmi_vsync  <= raster_vsync_delay;
            hdmi_x      <= raster_x_delay;
            hdmi_y      <= raster_y_delay;
        end
    end

    always_comb begin
        hdmi_rgb565 = 16'h0000;

        if (
            hdmi_active &&
            bleed_valid &&
            (hdmi_x >= 11'(CONTENT_X_START)) &&
            (hdmi_x < 11'(CONTENT_X_END)) &&
            ((hdmi_y - 10'(CONTENT_Y_START)) < 10'(CONTENT_HEIGHT)) &&
            ({1'b0, bleed_x} ==
                (hdmi_x - 11'(CONTENT_X_START)))
        ) begin
            hdmi_rgb565 = masked_rgb565;
        end
    end

endmodule
