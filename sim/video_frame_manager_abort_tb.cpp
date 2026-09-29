#include <cstdint>
#include <iostream>

#include "Vvideo_frame_manager.h"
#include "verilated.h"

static void source_tick(Vvideo_frame_manager& dut) {
    dut.source_clk = 1;
    dut.eval();

    dut.source_clk = 0;
    dut.eval();
}

static void hdmi_tick(Vvideo_frame_manager& dut) {
    dut.hdmi_clk = 1;
    dut.eval();

    dut.hdmi_clk = 0;
    dut.eval();
}

static void clear_source_events(Vvideo_frame_manager& dut) {
    dut.source_frame_start = 0;
    dut.source_frame_complete = 0;

    dut.source_commit_valid = 0;
    dut.source_commit_slot = 0;
    dut.source_commit_frame_id = 0;

    dut.source_abort_valid = 0;
    dut.source_abort_slot = 0;
    dut.source_abort_frame_id = 0;
}

static bool check_errors(Vvideo_frame_manager& dut) {
    if (dut.source_commit_error) {
        std::cerr << "Unexpected source_commit_error\n";
        return false;
    }

    if (dut.source_abort_error) {
        std::cerr << "Unexpected source_abort_error\n";
        return false;
    }

    return true;
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    Vvideo_frame_manager dut;

    dut.source_clk = 0;
    dut.hdmi_clk = 0;

    dut.source_reset = 1;
    dut.hdmi_reset = 1;

    clear_source_events(dut);

    dut.hdmi_frame_start = 0;

    dut.eval();

    source_tick(dut);
    hdmi_tick(dut);

    dut.source_reset = 0;
    dut.hdmi_reset = 0;
    dut.eval();

    /*
     * Frame 0:
     * capture normally into slot 0 and commit it.
     */
    dut.source_frame_start = 1;
    source_tick(dut);
    dut.source_frame_start = 0;
    dut.eval();

    if (!check_errors(dut))
        return 1;

    if (!dut.source_capture_enable) {
        std::cerr << "Frame 0 capture was not enabled\n";
        return 1;
    }

    if (dut.source_write_slot != 0) {
        std::cerr
            << "Frame 0 expected slot 0, got "
            << static_cast<int>(dut.source_write_slot)
            << "\n";
        return 1;
    }

    if (dut.source_frame_id != 0) {
        std::cerr
            << "Frame 0 expected ID 0, got "
            << dut.source_frame_id
            << "\n";
        return 1;
    }

    dut.source_frame_complete = 1;
    source_tick(dut);
    dut.source_frame_complete = 0;
    dut.eval();

    dut.source_commit_valid = 1;
    dut.source_commit_slot = 0;
    dut.source_commit_frame_id = 0;
    source_tick(dut);
    clear_source_events(dut);
    dut.eval();

    if (!check_errors(dut))
        return 1;

    /*
     * Allow committed frame 0 to cross into the HDMI clock domain.
     */
    hdmi_tick(dut);
    hdmi_tick(dut);

    dut.hdmi_frame_start = 1;
    hdmi_tick(dut);
    dut.hdmi_frame_start = 0;
    dut.eval();

    if (!dut.hdmi_frame_valid || dut.hdmi_frame_id != 0) {
        std::cerr
            << "HDMI failed to select committed frame 0\n";
        return 1;
    }

    /*
     * Frame 1:
     * slot 0 is currently being displayed, so this frame should use slot 1.
     */
    dut.source_frame_start = 1;
    source_tick(dut);
    dut.source_frame_start = 0;
    dut.eval();

    if (!dut.source_capture_enable) {
        std::cerr << "Frame 1 capture was not enabled\n";
        return 1;
    }

    if (dut.source_write_slot != 1) {
        std::cerr
            << "Frame 1 expected slot 1, got "
            << static_cast<int>(dut.source_write_slot)
            << "\n";
        return 1;
    }

    if (dut.source_frame_id != 1) {
        std::cerr
            << "Frame 1 expected ID 1, got "
            << dut.source_frame_id
            << "\n";
        return 1;
    }

    dut.source_frame_complete = 1;
    source_tick(dut);
    dut.source_frame_complete = 0;
    dut.eval();

    /*
     * Abort frame 1 and start frame 2 on the SAME source clock edge.
     *
     * The manager should safely reuse slot 1 immediately because ABORT means
     * all partial SDRAM writes for frame 1 have already completed.
     */
    dut.source_abort_valid = 1;
    dut.source_abort_slot = 1;
    dut.source_abort_frame_id = 1;

    dut.source_frame_start = 1;

    source_tick(dut);

    clear_source_events(dut);
    dut.eval();

    if (!check_errors(dut))
        return 1;

    if (dut.source_frame_dropped) {
        std::cerr
            << "Frame 2 was dropped instead of reusing aborted slot 1\n";
        return 1;
    }

    if (!dut.source_capture_enable) {
        std::cerr << "Frame 2 capture was not enabled\n";
        return 1;
    }

    if (dut.source_write_slot != 1) {
        std::cerr
            << "Frame 2 did not immediately reuse slot 1; got slot "
            << static_cast<int>(dut.source_write_slot)
            << "\n";
        return 1;
    }

    if (dut.source_frame_id != 2) {
        std::cerr
            << "Frame 2 expected ID 2, got "
            << dut.source_frame_id
            << "\n";
        return 1;
    }

    /*
     * Before frame 2 is committed, HDMI reaches another frame boundary.
     * It must repeat frame 0. Aborted frame 1 must never appear.
     */
    hdmi_tick(dut);
    hdmi_tick(dut);

    dut.hdmi_frame_start = 1;
    hdmi_tick(dut);
    dut.hdmi_frame_start = 0;
    dut.eval();

    if (!dut.hdmi_frame_valid) {
        std::cerr << "HDMI lost its current frame after abort\n";
        return 1;
    }

    if (dut.hdmi_frame_id == 1) {
        std::cerr << "Aborted frame 1 became visible to HDMI\n";
        return 1;
    }

    if (dut.hdmi_frame_id != 0) {
        std::cerr
            << "HDMI should repeat frame 0 before frame 2 commit; got "
            << dut.hdmi_frame_id
            << "\n";
        return 1;
    }

    /*
     * Complete and commit frame 2.
     */
    dut.source_frame_complete = 1;
    source_tick(dut);
    dut.source_frame_complete = 0;
    dut.eval();

    dut.source_commit_valid = 1;
    dut.source_commit_slot = 1;
    dut.source_commit_frame_id = 2;
    source_tick(dut);
    clear_source_events(dut);
    dut.eval();

    if (!check_errors(dut))
        return 1;

    /*
     * Synchronize frame 2 into HDMI domain and swap on next HDMI frame.
     */
    hdmi_tick(dut);
    hdmi_tick(dut);

    dut.hdmi_frame_start = 1;
    hdmi_tick(dut);
    dut.hdmi_frame_start = 0;
    dut.eval();

    if (!dut.hdmi_frame_valid) {
        std::cerr << "HDMI did not receive frame 2\n";
        return 1;
    }

    if (dut.hdmi_frame_id != 2) {
        std::cerr
            << "Expected HDMI frame 2 after commit, got "
            << dut.hdmi_frame_id
            << "\n";
        return 1;
    }

    if (dut.hdmi_frame_id == 1) {
        std::cerr << "Aborted frame 1 was exposed\n";
        return 1;
    }

    std::cout << "PASS: framebuffer abort handling verified\n";
    std::cout << "PASS: aborted frame was never visible to HDMI\n";
    std::cout << "PASS: aborted slot reused on the same source clock\n";
    std::cout << "PASS: HDMI repeated previous committed frame during abort\n";
    std::cout << "PASS: subsequent committed frame became visible normally\n";

    return 0;
}
