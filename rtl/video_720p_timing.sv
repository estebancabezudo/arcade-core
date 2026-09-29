module video_720p_timing (
    input  logic        clk,
    input  logic        reset,

    output logic [10:0] x,
    output logic [9:0]  y,

    output logic        active,
    output logic        hsync,
    output logic        vsync,

    output logic        line_start,
    output logic        frame_start
);

    localparam logic [10:0] H_ACTIVE = 11'd1280;
    localparam logic [10:0] H_FRONT  = 11'd110;
    localparam logic [10:0] H_SYNC   = 11'd40;
    localparam logic [10:0] H_BACK   = 11'd220;

    localparam logic [9:0] V_ACTIVE = 10'd720;
    localparam logic [9:0] V_FRONT  = 10'd5;
    localparam logic [9:0] V_SYNC   = 10'd5;
    localparam logic [9:0] V_BACK   = 10'd20;

    localparam logic [10:0] H_SYNC_START =
        H_ACTIVE + H_FRONT;

    localparam logic [10:0] H_SYNC_END =
        H_ACTIVE + H_FRONT + H_SYNC;

    localparam logic [10:0] H_TOTAL =
        H_ACTIVE + H_FRONT + H_SYNC + H_BACK;

    localparam logic [9:0] V_SYNC_START =
        V_ACTIVE + V_FRONT;

    localparam logic [9:0] V_SYNC_END =
        V_ACTIVE + V_FRONT + V_SYNC;

    localparam logic [9:0] V_TOTAL =
        V_ACTIVE + V_FRONT + V_SYNC + V_BACK;

    localparam logic [10:0] H_LAST = H_TOTAL - 1'b1;
    localparam logic [9:0]  V_LAST = V_TOTAL - 1'b1;

    always_ff @(posedge clk) begin
        if (reset) begin
            x <= 11'd0;
            y <= 10'd0;
        end else begin
            if (x == H_LAST) begin
                x <= 11'd0;

                if (y == V_LAST)
                    y <= 10'd0;
                else
                    y <= y + 1'b1;
            end else begin
                x <= x + 1'b1;
            end
        end
    end

    always_comb begin
        active =
            (x < H_ACTIVE) &&
            (y < V_ACTIVE);

        hsync =
            (x >= H_SYNC_START) &&
            (x < H_SYNC_END);

        vsync =
            (y >= V_SYNC_START) &&
            (y < V_SYNC_END);

        line_start =
            (x == 11'd0);

        frame_start =
            (x == 11'd0) &&
            (y == 10'd0);
    end

endmodule
