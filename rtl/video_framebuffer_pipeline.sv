module video_framebuffer_pipeline #(
    parameter int unsigned SOURCE_WIDTH = 320,
    parameter int unsigned SOURCE_HEIGHT = 240
) (
    /*
     * Arcade/source clock domain.
     */
    input  logic        source_clk,
    input  logic        source_reset,

    input  logic        source_frame_start,
    input  logic        source_frame_complete,

    input  logic        pixel_valid,
    input  logic [15:0] pixel_rgb565,
    input  logic [8:0]  pixel_x,
    input  logic [8:0]  pixel_y,

    /*
     * Shared video/memory clock domain.
     *
     * Prototype target: 74.25 MHz.
     */
    input  logic        video_clk,
    input  logic        video_reset,

    /*
     * HDMI frame boundary.
     */
    input  logic        hdmi_frame_start,

    output logic        hdmi_frame_valid,
    output logic [1:0]  hdmi_read_slot,
    output logic [31:0] hdmi_frame_id,

    /*
     * HDMI line-fetch interface.
     *
     * Request one complete source line from the selected framebuffer.
     */
    input  logic        line_fetch_start,
    input  logic [8:0]  line_fetch_y,

    output logic        line_busy,
    output logic        line_valid,

    input  logic        line_release,

    output logic [1:0]  line_slot,
    output logic [8:0]  line_y,

    input  logic [8:0]  line_read_x,
    output logic [15:0] line_rgb565,

    /*
     * Physical single-port memory-controller interface.
     */
    output logic        memory_command_valid,
    input  logic        memory_command_ready,

    output logic        memory_command_write,
    output logic [17:0] memory_command_address,
    output logic [31:0] memory_command_write_data,

    input  logic        memory_write_done,

    input  logic        memory_read_response_valid,
    input  logic [31:0] memory_read_response_data,

    /*
     * Diagnostics.
     */
    output logic        source_frame_dropped,
    output logic        capture_failed,

    output logic        manager_error,
    output logic        memory_queue_error,
    output logic        result_bridge_error
);

    /*
     * Capture pipeline write client.
     */
    logic        capture_write_valid;
    logic        capture_write_ready;
    logic [17:0] capture_write_address;
    logic [31:0] capture_write_data;
    logic        capture_write_done;

    /*
     * Framebuffer line-fetch read client.
     */
    logic        fetch_read_valid;
    logic        fetch_read_ready;
    logic [17:0] fetch_read_address;

    logic        fetch_response_valid;
    logic [31:0] fetch_response_data;

    logic        fetch_start;

    /*
     * A line can only be requested when HDMI has selected a committed
     * framebuffer.
     */
    always_comb begin
        fetch_start =
            line_fetch_start &&
            hdmi_frame_valid;
    end

    /*
     * Source raster -> framebuffer writes -> frame selection.
     *
     * memory_clk and hdmi_clk intentionally use the same video clock in
     * this prototype architecture.
     */
    video_capture_pipeline #(
        .SOURCE_WIDTH  (SOURCE_WIDTH),
        .SOURCE_HEIGHT (SOURCE_HEIGHT)
    ) capture_pipeline (
        .source_clk             (source_clk),
        .source_reset           (source_reset),

        .source_frame_start     (source_frame_start),
        .source_frame_complete  (source_frame_complete),

        .pixel_valid            (pixel_valid),
        .pixel_rgb565           (pixel_rgb565),
        .pixel_x                (pixel_x),
        .pixel_y                (pixel_y),

        .memory_clk             (video_clk),
        .memory_reset           (video_reset),

        .mem_write_valid        (capture_write_valid),
        .mem_write_ready        (capture_write_ready),

        .mem_write_address      (capture_write_address),
        .mem_write_data         (capture_write_data),

        .mem_write_done         (capture_write_done),

        .hdmi_clk               (video_clk),
        .hdmi_reset             (video_reset),

        .hdmi_frame_start       (hdmi_frame_start),

        .hdmi_frame_valid       (hdmi_frame_valid),
        .hdmi_read_slot         (hdmi_read_slot),
        .hdmi_frame_id          (hdmi_frame_id),

        .source_frame_dropped   (source_frame_dropped),
        .capture_failed         (capture_failed),

        .manager_error          (manager_error),
        .memory_queue_error     (memory_queue_error),
        .result_bridge_error    (result_bridge_error)
    );

    /*
     * Read one complete source line from the framebuffer.
     */
    video_frame_line_fetcher #(
        .SOURCE_WIDTH  (SOURCE_WIDTH),
        .SOURCE_HEIGHT (SOURCE_HEIGHT)
    ) line_fetcher (
        .clk                     (video_clk),
        .reset                   (video_reset),

        .start                   (fetch_start),
        .request_slot            (hdmi_read_slot),
        .request_y               (line_fetch_y),

        .busy                    (line_busy),
        .line_valid              (line_valid),
        .release_line            (line_release),

        .line_slot               (line_slot),
        .line_y                  (line_y),

        .read_x                  (line_read_x),
        .read_rgb565             (line_rgb565),

        .mem_read_valid          (fetch_read_valid),
        .mem_read_ready          (fetch_read_ready),
        .mem_read_address        (fetch_read_address),

        .mem_response_valid      (fetch_response_valid),
        .mem_response_data       (fetch_response_data)
    );

    /*
     * Single-port SDRAM arbitration.
     *
     * Reads have strict priority over framebuffer writes.
     */
    video_memory_arbiter memory_arbiter (
        .clk                     (video_clk),
        .reset                   (video_reset),

        .write_valid             (capture_write_valid),
        .write_ready             (capture_write_ready),

        .write_address           (capture_write_address),
        .write_data              (capture_write_data),

        .write_done              (capture_write_done),

        .read_valid              (fetch_read_valid),
        .read_ready              (fetch_read_ready),

        .read_address            (fetch_read_address),

        .read_response_valid     (fetch_response_valid),
        .read_response_data      (fetch_response_data),

        .memory_command_valid    (memory_command_valid),
        .memory_command_ready    (memory_command_ready),

        .memory_command_write    (memory_command_write),
        .memory_command_address  (memory_command_address),
        .memory_command_write_data(
            memory_command_write_data
        ),

        .memory_write_done       (memory_write_done),

        .memory_read_response_valid(
            memory_read_response_valid
        ),
        .memory_read_response_data(
            memory_read_response_data
        )
    );

endmodule
