module hdmi_scanout_controller #(
    parameter int unsigned SOURCE_WIDTH = 320,
    parameter int unsigned SOURCE_HEIGHT = 240,
    parameter int unsigned SCALE = 3
) (
    input  logic        clk,
    input  logic        reset,

    input  logic        timing_active,
    input  logic [10:0] timing_x,
    input  logic [9:0]  timing_y,

    input  logic        frame_select_pulse,
    input  logic        frame_valid,

    output logic        line_fetch_start,
    output logic [8:0]  line_fetch_y,

    input  logic        line_busy,
    input  logic        line_valid,
    input  logic [8:0]  line_y,

    output logic        line_release,

    output logic [8:0]  line_read_x,
    input  logic [15:0] line_rgb565,

    output logic        content_active,
    output logic [15:0] content_rgb565,

    output logic [9:0]  content_x,
    output logic [1:0]  horizontal_repeat,
    output logic [1:0]  vertical_repeat,

    output logic        line_underflow
);

    localparam int unsigned HDMI_WIDTH = 1280;
    localparam int unsigned HDMI_HEIGHT = 720;
    localparam int unsigned CONTENT_WIDTH = SOURCE_WIDTH * SCALE;
    localparam int unsigned CONTENT_HEIGHT = SOURCE_HEIGHT * SCALE;
    localparam int unsigned CONTENT_X_START =
        (HDMI_WIDTH - CONTENT_WIDTH) / 2;
    localparam int unsigned CONTENT_Y_START =
        (HDMI_HEIGHT - CONTENT_HEIGHT) / 2;
    logic        fetch_pending;
    logic [8:0]  pending_y;
    logic [8:0]  expected_y;
    logic        content_vertical_active;
    logic        renderer_cache_valid;
    logic        renderer_release;

    always_comb begin
        content_vertical_active =
            timing_active &&
            ((timing_y - 10'(CONTENT_Y_START)) < 10'(CONTENT_HEIGHT));

        expected_y =
            content_vertical_active
                ? 9'(
                    (timing_y - 10'(CONTENT_Y_START)) /
                    10'(SCALE)
                )
                : 9'd0;

        renderer_cache_valid =
            frame_valid &&
            line_valid &&
            content_vertical_active &&
            (line_y == expected_y);

        line_fetch_start =
            fetch_pending &&
            frame_valid &&
            !line_busy &&
            !line_valid;

        line_fetch_y = pending_y;
        line_release = renderer_release;
    end

    hdmi_line_renderer #(
        .SOURCE_WIDTH  (SOURCE_WIDTH),
        .SOURCE_HEIGHT (SOURCE_HEIGHT),
        .SCALE         (SCALE)
    ) renderer (
        .timing_active     (timing_active),
        .timing_x          (timing_x),
        .timing_y          (timing_y),

        .cache_valid       (renderer_cache_valid),
        .cache_rgb565      (line_rgb565),

        .cache_read_x      (line_read_x),
        .release_line      (renderer_release),

        .content_active    (content_active),
        .content_x         (content_x),
        .horizontal_repeat (horizontal_repeat),
        .vertical_repeat   (vertical_repeat),

        .out_rgb565        (content_rgb565)
    );

    always_ff @(posedge clk) begin
        if (reset) begin
            fetch_pending  <= 1'b0;
            pending_y      <= 9'd0;
            line_underflow <= 1'b0;
        end else begin
            /* Prefetch source line zero during vertical blanking. */
            if (frame_select_pulse) begin
                fetch_pending  <= 1'b1;
                pending_y      <= 9'd0;
                line_underflow <= 1'b0;
            end

            if (line_fetch_start)
                fetch_pending <= 1'b0;

            /*
             * Release only after the last horizontal pixel and last
             * vertical repeat. The centred borders plus horizontal blanking
             * provide the next line's prefetch window.
             */
            if (line_release) begin
                if (expected_y < 9'(SOURCE_HEIGHT - 1)) begin
                    fetch_pending <= 1'b1;
                    pending_y <= expected_y + 1'b1;
                end
            end

            /* Borders never require a cached line and cannot underflow. */
            if (
                frame_valid &&
                content_vertical_active &&
                (timing_x == 11'(CONTENT_X_START))
            ) begin
                if (
                    !line_valid ||
                    (line_y != expected_y)
                ) begin
                    line_underflow <= 1'b1;
                end
            end
        end
    end

endmodule
