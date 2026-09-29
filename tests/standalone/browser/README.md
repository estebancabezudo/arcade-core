# Browser verification

From the repository root, `make test-web` rebuilds the private bundle, executes seven Node.js regressions, and runs five Python packaging tests. Python 3 and Node.js 18+ are required; these tests do not launch a browser. No npm dependencies are needed for the Node.js tests.

`original-baseline.json` records a deterministic 720-frame trace of the original JavaScript from the supplied project before the reorganization. The test drives coin at frames 360–361, start at 378–381 and left at 450–649. It checks CPU PC/T-states, RAM, final framebuffer and all 570,240 PCM samples (SHA-256). It is a regression reference for this change, not a claim that every Z80 instruction was newly validated.

For an optional real Chromium smoke test, use an environment with the Python `playwright` package and a Chromium executable:

```bash
python3 tests/standalone/browser/browser_smoke.py --executable /usr/bin/chromium
```

The default mode starts a temporary loopback HTTP server rooted at the checkout. It tests the actual entry-page URLs and JavaScript loading, screen-only DOM, start/pause/resume, keyboard directions, mute, touch dragging, small viewports, reset, destroy/remount, the original full interface, unavailable audio and nonzero Web Audio output. The visibility-change case uses a simulated event. It does not validate every browser or physical sound hardware.

Restricted runners may block both `file://` and loopback HTTP navigation. For those environments:

```bash
python3 tests/standalone/browser/browser_smoke.py --inline --executable /usr/bin/chromium
```

Inline mode reads the same HTML and JavaScript locally and injects them into a blank browser page. It exercises execution, DOM, input and Web Audio, **not URL loading, server behavior or `file://` compatibility**. The reorganization was tested in this mode because this runner blocks local navigation. Its audio path was the scheduled Web Audio fallback; AudioWorklet module loading was not retested here.

For changes to the core, additionally check the full C/RTL suite and the browser's AudioWorklet path on the development machine. Never infer hardware validation from these software checks.
