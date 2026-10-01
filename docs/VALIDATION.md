# Reorganization validation (previous delivery)

For the current delivery, see [Raspberry Pi integration validation](validation/raspberry-pi-integration.md). The results below are retained as the historical reorganization record.

Scope: the supplied `arcade-core(2).zip`, directory migration, ROM-free browser template/packaging, screen-only UI, and updated documentation. These checks do not replace the project's historical validation or claim new physical FPGA results.

## Executed

- All 36 existing C test executables compile with C17, `-O2`, `-Wall`, `-Wextra`, `-Wpedantic`, `-Werror` and pass through the new `make test-c` target.
- Seven Node.js tests pass, including a 720-frame deterministic comparison against the original supplied JavaScript: CPU PC/T-states, RAM, final video and 570,240 PCM samples match. Deferred WSG writes, ROM corruption rejection, reset and framebuffer reuse are covered.
- Five Python packager tests pass: reproducible output, exact portable paths, wrong size, missing ROM, invalid CRC without overwriting the previous output, and invocation outside the project directory.
- Seven Chromium smoke tests pass in explicit inline-fixture mode: canvas-only DOM, play/pause/resume, keyboard, mute, touch, portrait/landscape sizing, reset, destroy/remount, original full interface, no-audio operation, nonzero Web Audio output and simulated visibility pause.
- Both rendered HTML READMEs are checked with an injected iframe fixture: one canvas, no external controls, successful start and pause. This does not test file/HTTP navigation.
- JavaScript and Python syntax, Bash syntax, Make target expansion, moved source paths, UTF-8 and local documentation links are checked.
- The common C sources, generic RTL, FPGA logic, native tests and raw ROMs are byte-identical to the supplied versions. The three host executable sources only change the default ROM path. Native build-script changes concern paths and shared manifests.

## Environment limitations

Chromium in this environment refuses `file://` and loopback HTTP navigation with `ERR_BLOCKED_BY_ADMINISTRATOR`. The browser tests therefore inject the actual HTML/JS into a blank page. This validates the running UI/engine but not direct file opening, real server headers or URL loading. The exercised audio path is scheduled Web Audio; the unchanged AudioWorklet path is not claimed as newly verified.

SDL2 development headers and Verilator are unavailable here. The complete `make pacman-host`, RTL lint/test suite and physical targets were not executed. Their source paths were updated and statically checked, while the C-only suite was executed. Full SDL/RTL integration validation remains pending on the development machine.

The browser regressions compare the changed bundle with the original input, not a newly generated C audio/video trace. No universal performance or cross-browser guarantee is made.
