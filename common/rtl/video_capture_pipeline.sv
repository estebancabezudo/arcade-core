module video_capture_pipeline #(
    parameter int unsigned SOURCE_WIDTH = 320,
    parameter int unsigned SOURCE_HEIGHT = 240
) (
    /*
     * Arcade/source clock domain.
     *
     * source_frame_start must occur during vertical blanking, before the
     * first active pixel of the new frame.
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
     * Memory clock domain.
     */
    input  logic        memory_clk,
    input  logic        memory_reset,

    output logic        mem_write_valid,
    input  logic        mem_write_ready,

    output logic [17:0] mem_write_address,
    output logic [31:0] mem_write_data,

    input  logic        mem_write_done,

    /*
     * HDMI clock domain.
     */
    input  logic        hdmi_clk,
    input  logic        hdmi_reset,

    input  logic        hdmi_frame_start,

    output logic        hdmi_frame_valid,
    output logic [1:0]  hdmi_read_slot,
    output logic [31:0] hdmi_frame_id,

    /*
     * Diagnostics.
     */
    output logic        source_frame_dropped,
    output logic        capture_failed,

    output logic        manager_error,
    output logic        memory_queue_error,
    output logic        result_bridge_error
);

    logic manager_capture_enable;
    logic [1:0] manager_write_slot;
    logic [31:0] manager_frame_id;

    logic manager_commit_error;
    logic manager_abort_error;

    /*
     * The manager allocates the framebuffer on source_frame_start.
     *
     * video_capture_writer receives frame_begin on the following source
     * clock, once the manager's allocation outputs are stable.
     *
     * In the final interface source_frame_start will occur in vertical
     * blanking, so there is ample time before the first active pixel.
     */
    logic writer_frame_begin;

    always_ff @(posedge source_clk) begin
        if (source_reset)
            writer_frame_begin <= 1'b0;
        else
            writer_frame_begin <= source_frame_start;
    end

    /*
     * Capture writer -> memory command queue.
     */
    logic        command_valid;
    logic        command_ready;
    logic [1:0]  command_type;

    logic [17:0] command_write_address;
    logic [31:0] command_write_data;

    logic [1:0]  command_marker_slot;
    logic [31:0] command_marker_frame_id;

    logic capture_protocol_error;

    /*
     * Memory command queue -> result bridge.
     */
    logic        memory_commit_valid;
    logic [1:0]  memory_commit_slot;
    logic [31:0] memory_commit_frame_id;

    logic        memory_abort_valid;
    logic [1:0]  memory_abort_slot;
    logic [31:0] memory_abort_frame_id;

    /*
     * Result bridge -> frame manager.
     */
    logic        source_commit_valid;
    logic [1:0]  source_commit_slot;
    logic [31:0] source_commit_frame_id;

    logic        source_abort_valid;
    logic [1:0]  source_abort_slot;
    logic [31:0] source_abort_frame_id;

    /* Exposed by the bridge for diagnostics; the queue emits one result at a time. */
    /* verilator lint_off UNUSEDSIGNAL */
    logic memory_result_ready;
    logic capture_active_unused;
    /* verilator lint_on UNUSEDSIGNAL */

    video_frame_manager frame_manager (
        .source_clk              (source_clk),
        .source_reset            (source_reset),

        .source_frame_start      (source_frame_start),
        .source_frame_complete   (source_frame_complete),

        .source_commit_valid     (source_commit_valid),
        .source_commit_slot      (source_commit_slot),
        .source_commit_frame_id  (source_commit_frame_id),

        .source_abort_valid      (source_abort_valid),
        .source_abort_slot       (source_abort_slot),
        .source_abort_frame_id   (source_abort_frame_id),

        .source_capture_enable   (manager_capture_enable),
        .source_write_slot       (manager_write_slot),
        .source_frame_id         (manager_frame_id),

        .source_frame_dropped    (source_frame_dropped),
        .source_commit_error     (manager_commit_error),
        .source_abort_error      (manager_abort_error),

        .hdmi_clk                (hdmi_clk),
        .hdmi_reset              (hdmi_reset),

        .hdmi_frame_start        (hdmi_frame_start),

        .hdmi_frame_valid        (hdmi_frame_valid),
        .hdmi_read_slot          (hdmi_read_slot),
        .hdmi_frame_id           (hdmi_frame_id)
    );

    video_capture_writer #(
        .SOURCE_WIDTH  (SOURCE_WIDTH),
        .SOURCE_HEIGHT (SOURCE_HEIGHT)
    ) capture_writer (
        .clk                     (source_clk),
        .reset                   (source_reset),

        .frame_begin             (writer_frame_begin),
        .frame_capture_enable    (manager_capture_enable),
        .frame_slot              (manager_write_slot),
        .frame_id                (manager_frame_id),

        .pixel_valid             (pixel_valid),
        .pixel_rgb565            (pixel_rgb565),
        .pixel_x                 (pixel_x),
        .pixel_y                 (pixel_y),

        .frame_end               (source_frame_complete),

        .command_valid           (command_valid),
        .command_ready           (command_ready),

        .command_type            (command_type),

        .command_write_address   (command_write_address),
        .command_write_data      (command_write_data),

        .command_marker_slot     (command_marker_slot),
        .command_marker_frame_id (command_marker_frame_id),

        .capture_active          (capture_active_unused),
        .capture_failed          (capture_failed),
        .protocol_error          (capture_protocol_error)
    );

    video_memory_command_queue memory_queue (
        .source_clk              (source_clk),
        .source_reset            (source_reset),

        .source_command_valid    (command_valid),
        .source_command_ready    (command_ready),

        .source_command_type     (command_type),

        .source_write_address    (command_write_address),
        .source_write_data       (command_write_data),

        .source_marker_slot      (command_marker_slot),
        .source_marker_frame_id  (command_marker_frame_id),

        .memory_clk              (memory_clk),
        .memory_reset            (memory_reset),

        .mem_write_valid         (mem_write_valid),
        .mem_write_ready         (mem_write_ready),

        .mem_write_address       (mem_write_address),
        .mem_write_data          (mem_write_data),

        .mem_write_done          (mem_write_done),

        .commit_valid            (memory_commit_valid),
        .commit_slot             (memory_commit_slot),
        .commit_frame_id         (memory_commit_frame_id),

        .abort_valid             (memory_abort_valid),
        .abort_slot              (memory_abort_slot),
        .abort_frame_id          (memory_abort_frame_id),

        .protocol_error          (memory_queue_error)
    );

    video_memory_result_bridge result_bridge (
        .memory_clk              (memory_clk),
        .memory_reset            (memory_reset),

        .memory_commit_valid     (memory_commit_valid),
        .memory_commit_slot      (memory_commit_slot),
        .memory_commit_frame_id  (memory_commit_frame_id),

        .memory_abort_valid      (memory_abort_valid),
        .memory_abort_slot       (memory_abort_slot),
        .memory_abort_frame_id   (memory_abort_frame_id),

        .memory_result_ready     (memory_result_ready),
        .memory_protocol_error   (result_bridge_error),

        .source_clk              (source_clk),
        .source_reset            (source_reset),

        .source_commit_valid     (source_commit_valid),
        .source_commit_slot      (source_commit_slot),
        .source_commit_frame_id  (source_commit_frame_id),

        .source_abort_valid      (source_abort_valid),
        .source_abort_slot       (source_abort_slot),
        .source_abort_frame_id   (source_abort_frame_id)
    );

    always_comb begin
        manager_error =
            manager_commit_error ||
            manager_abort_error ||
            capture_protocol_error;
    end

endmodule
