#!/usr/bin/env python3
"""Optional Chromium smoke tests; --inline avoids URL loading in restricted runners.

Architecture and design
The HTTP fixture applies the production-equivalent script/style CSP so the real
standalone entry proves external Shadow DOM styling without suppressing console policy
violations. Inline mode remains an offline emulation fallback and is not the CSP gate.
"""

import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import re
import threading
import unittest

from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parents[3]
OPTIONS = None


class QuietHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Content-Security-Policy", "script-src 'self'; style-src 'self'")
        super().end_headers()

    def log_message(self, format, *args):
        pass


class BrowserSmoke(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.playwright = sync_playwright().start()
        arguments = {"headless": True, "args": ["--no-sandbox", "--disable-dev-shm-usage"]}
        if OPTIONS.executable:
            arguments["executable_path"] = OPTIONS.executable
        cls.browser = cls.playwright.chromium.launch(**arguments)
        cls.server = None
        if not OPTIONS.inline:
            cls.server = ThreadingHTTPServer(("127.0.0.1", 0), partial(QuietHandler, directory=str(ROOT)))
            threading.Thread(target=cls.server.serve_forever, daemon=True).start()

    @classmethod
    def tearDownClass(cls):
        cls.browser.close()
        cls.playwright.stop()
        if cls.server:
            cls.server.shutdown()
            cls.server.server_close()

    def setUp(self):
        self.context = self.browser.new_context(viewport={"width": 1000, "height": 800}, has_touch=True)
        self.page = self.context.new_page()
        self.errors = []
        self.csp_violations = []
        self.page.on("pageerror", lambda error: self.errors.append(str(error)))
        self.page.on("console", lambda message: self.csp_violations.append(message.text)
                     if "violates the following Content Security Policy" in message.text else None)
        self.load("standalone/platforms/browser/index.html")

    def tearDown(self):
        self.context.close()
        self.assertEqual(self.errors, [])
        if not OPTIONS.inline:
            self.assertEqual(self.csp_violations, [])

    def load(self, entry):
        if OPTIONS.inline:
            # The HTML/JS bytes are unchanged; only external script loading is replaced.
            html = (ROOT / entry).read_text(encoding="utf-8")
            html = re.sub(r'<script src="[^"]+"[\s\S]*?</script>', "", html)
            self.page.set_content(html)
            self.page.add_script_tag(content=(ROOT / "build/standalone/browser/pacman.js").read_text(encoding="utf-8"))
        else:
            self.page.goto(f"http://127.0.0.1:{self.server.server_port}/{entry}")
        self.page.wait_for_function("window.PacmanWeb && PacmanWeb.getStats()?.booted")

    def start(self):
        self.page.locator("canvas").click()
        self.page.wait_for_function("PacmanWeb.getStats().running")

    def test_screen_only_start_pause_resume(self):
        self.assertEqual(self.page.locator("canvas").count(), 1)
        self.assertEqual(self.page.locator("button,header,input,.overlay,.stats").count(), 0)
        self.assertFalse(self.page.evaluate("PacmanWeb.getStats().running"))
        self.start()
        self.page.keyboard.press("p")
        self.assertFalse(self.page.evaluate("PacmanWeb.getStats().running"))
        frame = self.page.evaluate("PacmanWeb.getStats().frame")
        self.page.wait_for_timeout(100)
        self.assertEqual(self.page.evaluate("PacmanWeb.getStats().frame"), frame)
        self.page.locator("canvas").click()
        self.page.wait_for_function("PacmanWeb.getStats().running")
        self.page.wait_for_function(f"PacmanWeb.getStats().frame > {frame}")

    def test_keyboard_and_mute(self):
        self.page.keyboard.press("Enter")
        self.page.wait_for_function("PacmanWeb.getStats().running")
        self.page.keyboard.down("ArrowLeft")
        self.assertEqual(self.page.evaluate("PacmanWeb.app.directionBit"), 2)
        self.page.keyboard.down("ArrowUp")
        self.assertEqual(self.page.evaluate("PacmanWeb.app.directionBit"), 1)
        self.page.keyboard.up("ArrowUp")
        self.assertEqual(self.page.evaluate("PacmanWeb.app.directionBit"), 2)
        self.page.keyboard.up("ArrowLeft")
        self.assertEqual(self.page.evaluate("PacmanWeb.app.directionBit"), 0)
        self.page.keyboard.press("m")
        self.assertTrue(self.page.evaluate("PacmanWeb.getStats().muted"))
        self.page.keyboard.press("m")
        self.assertFalse(self.page.evaluate("PacmanWeb.getStats().muted"))
        self.page.keyboard.press("5")
        self.page.wait_for_function("PacmanWeb.app.coinFrames === 0")

    def test_fullscreen_keyboard_control(self):
        self.page.keyboard.press("f")
        self.page.wait_for_function("document.fullscreenElement !== null")
        self.assertEqual(self.page.evaluate("document.fullscreenElement.tagName"), "PACMAN-ARCADE")
        self.page.keyboard.press("Escape")
        self.page.wait_for_function("document.fullscreenElement === null")

    def test_touch_and_small_viewports(self):
        self.page.set_viewport_size({"width": 390, "height": 844})
        self.page.locator("canvas").tap()
        self.page.wait_for_function("PacmanWeb.getStats().running")
        box = self.page.locator("canvas").bounding_box()
        self.assertLessEqual(box["width"], 390)
        self.assertLessEqual(box["height"], 844)
        self.assertAlmostEqual(box["width"] / box["height"], 224 / 288, places=3)
        session = self.context.new_cdp_session(self.page)
        x, y = box["x"] + 70, box["y"] + 70
        session.send("Input.dispatchTouchEvent", {"type": "touchStart", "touchPoints": [{"x": x, "y": y}]})
        session.send("Input.dispatchTouchEvent", {"type": "touchMove", "touchPoints": [{"x": x + 40, "y": y}]})
        self.assertEqual(self.page.evaluate("PacmanWeb.app.directionBit"), 4)
        session.send("Input.dispatchTouchEvent", {"type": "touchEnd", "touchPoints": []})
        self.assertEqual(self.page.evaluate("PacmanWeb.app.directionBit"), 0)
        self.page.set_viewport_size({"width": 844, "height": 390})
        self.page.wait_for_timeout(50)
        box = self.page.locator("canvas").bounding_box()
        self.assertLessEqual(box["height"], 390)

    def test_reset_destroy_and_original_full_interface(self):
        self.start()
        self.page.evaluate("PacmanWeb.reset()")
        self.page.wait_for_function("PacmanWeb.getStats().booted")
        self.assertEqual(self.page.evaluate("PacmanWeb.getStats().frame"), 360)
        self.assertFalse(self.page.evaluate("PacmanWeb.getStats().running"))
        self.page.evaluate("PacmanWeb.mount('#game', {screenOnly: false})")
        self.page.wait_for_function("PacmanWeb.getStats().booted")
        self.assertGreater(self.page.locator("button").count(), 0)
        self.page.locator("[data-action=play]").click()
        self.page.wait_for_function("PacmanWeb.getStats().running")
        self.page.evaluate("PacmanWeb.destroy()")
        self.assertEqual(self.page.locator("canvas").count(), 0)
        self.assertIsNone(self.page.evaluate("PacmanWeb.getStats()"))

    def test_audio_unavailable_can_play_silently(self):
        self.page.evaluate("globalThis.AudioContext = undefined; globalThis.webkitAudioContext = undefined")
        self.start()
        self.assertEqual(self.page.evaluate("PacmanWeb.getStats().audioState"), "unavailable")
        self.assertTrue(self.page.evaluate("PacmanWeb.getStats().running"))

    def test_web_audio_produces_nonzero_output(self):
        self.start()
        self.page.evaluate("""() => {
          const audio = PacmanWeb.app.audio;
          window.probe = audio.context.createAnalyser();
          audio.gain.connect(probe);
          window.samples = new Float32Array(probe.fftSize);
        }""")
        self.page.wait_for_function("""() => {
          probe.getFloatTimeDomainData(samples);
          return samples.some(value => Math.abs(value) > 0.0001);
        }""", timeout=10000)
        self.assertEqual(self.page.evaluate("PacmanWeb.getStats().audioState"), "running")
        if not OPTIONS.inline:
            self.assertEqual(self.page.evaluate("PacmanWeb.getStats().audioMode"), "AudioWorklet")

    def test_visibility_pause_and_portable_entry(self):
        self.start()
        self.page.evaluate("""() => {
          Object.defineProperty(document, 'hidden', {configurable:true, value:true});
          document.dispatchEvent(new Event('visibilitychange'));
        }""")
        self.assertFalse(self.page.evaluate("PacmanWeb.getStats().running"))
        self.page.evaluate("delete document.hidden; PacmanWeb.destroy()")
        if OPTIONS.inline:
            # Reset the inline fixture just as a navigation would reset globals.
            self.page.evaluate("delete globalThis.PacmanWeb")
        self.load("build/standalone/browser/index.html")
        self.assertEqual(self.page.locator("button").count(), 0)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inline", action="store_true", help="Inject fixture HTML/JS instead of testing URL loading")
    parser.add_argument("--executable", help="Optional Chromium executable path")
    OPTIONS, remaining = parser.parse_known_args()
    unittest.main(argv=[__file__, *remaining])
