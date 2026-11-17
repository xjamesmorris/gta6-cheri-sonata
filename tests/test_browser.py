#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise the browser adapter and compare its Wasm core with native C++."""

import ctypes
import functools
import hashlib
import http.server
import os
import pathlib
import re
import shutil
import subprocess
import tempfile
import threading
import unittest

from playwright.sync_api import sync_playwright

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUTPUT = ROOT / "build" / "game-preview"


class QuietHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, message, *args):
        if args and len(args) > 1 and str(args[1]).startswith(("4", "5")):
            super().log_message(message, *args)


class BrowserTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.native = ctypes.CDLL(str(ROOT / "build" / "game-tests" / "native.so"))
        cls.native.preview_init.argtypes = [ctypes.c_uint32]
        cls.native.preview_advance.argtypes = [ctypes.c_uint32, ctypes.c_uint32]
        cls.native.preview_render.restype = ctypes.POINTER(ctypes.c_uint8)
        cls.native.preview_money.restype = ctypes.c_int64
        cls.native.preview_bill_total.restype = ctypes.c_int64
        cls.server = http.server.ThreadingHTTPServer(
            ("127.0.0.1", 0), functools.partial(QuietHandler, directory=str(OUTPUT))
        )
        cls.server_thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.server_thread.start()
        cls.addClassCleanup(cls.server.server_close)
        cls.addClassCleanup(cls.server_thread.join)
        cls.addClassCleanup(cls.server.shutdown)
        cls.url = f"http://127.0.0.1:{cls.server.server_port}"
        cls.playwright = sync_playwright().start()
        cls.addClassCleanup(cls.playwright.stop)
        cls.browser = cls.playwright.chromium.launch(
            executable_path=os.environ["CHROMIUM"], headless=True, chromium_sandbox=True
        )
        cls.addClassCleanup(cls.browser.close)
        (OUTPUT / "screenshots").mkdir(exist_ok=True)

    def setUp(self):
        self.page = self.browser.new_page()
        self.addCleanup(self.page.close)
        self.errors = []
        self.expected_errors = False
        self.page.on("pageerror", lambda error: self.errors.append(str(error)))
        self.page.on(
            "console",
            lambda message: self.errors.append(message.text) if message.type == "error" else None,
        )

    def tearDown(self):
        if not self.expected_errors:
            self.assertEqual(self.errors, [])

    def open_preview(self):
        self.page.goto(self.url + "/?manual")
        self.page.wait_for_function("Boolean(window.drivingPreview)")
        self.page.locator("#lcd").focus()
        self.assertEqual(self.page.title(), "Grand Time Auto")
        self.assertEqual(self.page.evaluate("drivingPreview.module._preview_protection_origin()"), 0)
        self.assertIn("emulated", self.page.locator("#protection-mode").inner_text().lower())
        self.assertIsNone(self.page.locator("#artwork-toggle").get_attribute("disabled"))

    def advance(self, milliseconds, count=1):
        self.page.evaluate(
            "([ms, count]) => { for (let i = 0; i < count; i++) drivingPreview.step(ms); }",
            [milliseconds, count],
        )

    def state(self):
        return self.page.evaluate("""() => {
            const m = drivingPreview.module;
            return [m._preview_phase(), m._preview_stars(), m._preview_x(),
                m._preview_distance(), Number(m._preview_money()),
                m._preview_y(), m._preview_pending_hits()];
        }""")

    def screenshot(self, name):
        self.page.locator("#lcd").screenshot(path=str(OUTPUT / "screenshots" / f"{name}.png"))

    def canvas_chroma(self):
        return self.page.evaluate("""() => {
            const data = document.getElementById("lcd").getContext("2d")
                .getImageData(0, 0, 128, 160).data;
            let sum = 0;
            for (let i = 0; i < data.length; i += 4) {
                const r = data[i] >> 3, g = data[i + 1] >> 3, b = data[i + 2] >> 3;
                sum += Math.abs(r - g) + Math.abs(g - b) + Math.abs(b - r);
            }
            return sum;
        }""")

    def test_wallpaper_matches_selected_artwork(self):
        headers = {
            "primary": "wallpaper_image.h",
            "alternate": "wallpaper_image_alternate.h",
        }
        selection = os.environ.get("GTA6_TEST_ARTWORK", "primary")
        self.assertIn(selection, headers)
        header = (ROOT / "firmware" / headers[selection]).read_text()
        expected = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{2})", header))
        self.open_preview()
        self.advance(200, 30)
        self.assertEqual(self.state()[0], 1)
        actual = self.page.evaluate("""() => {
            const m = drivingPreview.module;
            const pointer = m._preview_render();
            return Array.from(m.HEAPU8.subarray(pointer, pointer + 128 * 160 * 2));
        }""")
        self.assertEqual(bytes(actual), expected)

    def drive_to_wasted(self):
        self.page.keyboard.down("ArrowLeft")
        for _ in range(900):
            self.advance(80)
            if self.state()[0] == 4:
                break
        self.page.keyboard.up("ArrowLeft")
        self.assertEqual(self.state()[0], 4)

    def test_keyboard_complete_round_and_restart(self):
        self.open_preview()
        self.advance(200, 30)
        self.assertEqual(self.state()[0], 1)
        self.screenshot("wallpaper")
        self.advance(200, 15)
        self.assertEqual(self.state()[0], 2)
        self.screenshot("prompt")
        self.page.keyboard.down("ArrowRight")
        self.advance(0)
        self.assertEqual(self.state()[0], 3)
        initial_x = self.state()[2]
        self.advance(40)
        self.assertGreater(self.state()[2], initial_x)
        self.page.keyboard.up("ArrowRight")
        self.advance(200, 10)
        self.screenshot("driving")
        self.drive_to_wasted()
        self.assertGreater(self.page.evaluate("drivingPreview.module._preview_damage()"), 0)
        self.assertEqual(self.page.locator('[data-rgb].on').count(), 2)
        self.screenshot("wasted")
        frozen = self.state()
        self.advance(200, 15)
        self.assertEqual(self.page.evaluate("""() => [
            drivingPreview.module._preview_rgb1(), drivingPreview.module._preview_rgb2()
        ]"""), [0xff0000, 0xff0000])
        colored = self.canvas_chroma()
        self.assertGreater(colored, 0)
        self.advance(200, 15)
        fading = self.page.evaluate("""() => [
            drivingPreview.module._preview_rgb1(), drivingPreview.module._preview_rgb2()
        ]""")
        self.assertEqual(fading[0], fading[1])
        self.assertGreater(fading[0], 0)
        self.assertLess(fading[0], 0xff0000)
        self.assertGreater(fading[0] >> 16, 0)
        self.assertGreater(fading[0] & 0xff, 0)
        self.assertEqual(fading[0] & 0xff00, 0)
        self.assertEqual(self.page.locator('[data-rgb="2"]').get_attribute("aria-label"),
                         "RGB LED 2: purple")
        self.assertGreater(self.canvas_chroma(), 0)
        self.assertLess(self.canvas_chroma(), colored)
        self.screenshot("wasted-fade")
        backgrounds = self.page.locator("[data-rgb]").evaluate_all(
            "(leds) => leds.map(led => getComputedStyle(led).backgroundColor)"
        )
        self.assertEqual(backgrounds[0], backgrounds[1])
        self.assertNotEqual(backgrounds[0], "rgb(255, 0, 0)")
        self.advance(200, 5)
        self.assertEqual(self.page.locator("[data-rgb].on").count(), 2)
        self.advance(200, 10)
        self.assertEqual(self.page.locator("[data-rgb].on").count(), 0)
        self.assertEqual(self.canvas_chroma(), 0)
        self.screenshot("wasted-monochrome")
        self.advance(200, 25)
        self.assertEqual(self.state(), frozen)
        self.advance(125, 2)
        self.assertEqual(self.page.locator('[data-rgb="2"].on').count(), 0)
        self.assertEqual(self.page.locator('[data-rgb="2"]').get_attribute("aria-label"),
                         "RGB LED 2: off")
        self.page.keyboard.down("Space")
        self.advance(0)
        self.assertEqual(self.state()[0], 4)
        self.advance(150, 5)
        self.assertEqual(self.page.evaluate("drivingPreview.module._preview_scale()"), 64)
        self.advance(150, 5)
        self.assertEqual(self.state()[0], 5)
        self.assertEqual(self.page.locator("[data-led].on").count(), 0)
        self.assertEqual(self.state()[4], frozen[4])
        bill_total = self.page.evaluate("Number(drivingPreview.module._preview_bill_total())")
        self.assertGreater(bill_total, 25000)
        self.advance(150, 10)
        self.assertEqual(self.state()[4], frozen[4])
        self.advance(200, 3)
        self.assertEqual(self.state()[4], frozen[4] - 25000)
        self.screenshot("bill")
        self.advance(200, 30)
        self.assertEqual(self.state()[0], 5)
        self.assertEqual(self.state()[4], frozen[4] - bill_total)
        self.advance(200, 60)
        self.assertEqual(self.state()[0], 5)
        self.assertEqual(self.state()[4], frozen[4] - bill_total)
        self.page.keyboard.up("Space")
        self.page.keyboard.down("ArrowRight")
        self.advance(150, 10)
        self.assertEqual(self.state()[:2], [3, 0])
        self.assertTrue(self.page.evaluate("""() => {
            const data = document.getElementById("lcd").getContext("2d")
                .getImageData(0, 0, 128, 160).data;
            return data.every((value, i) => i % 4 === 3 ? value === 255 : value === 0);
        }"""))
        self.advance(150, 10)
        self.page.keyboard.up("ArrowRight")
        self.assertEqual(self.state()[4], frozen[4] - bill_total)
        self.assertEqual(self.page.evaluate("drivingPreview.module._preview_damage()"), 0)
        self.assertGreater(self.canvas_chroma(), 0)
        negative = self.state()[4] < 0
        self.assertEqual(self.page.evaluate("""([x, y]) => Array.from(
            document.getElementById("lcd").getContext("2d")
                .getImageData(x, y, 1, 1).data)""", [4, 7] if negative else [5, 5]),
                [255, 0, 0, 255] if negative else [0, 255, 0, 255])
        self.screenshot("hospital")
        self.page.keyboard.down("Enter")
        self.advance(0)
        self.assertEqual(self.state()[0], 3)
        self.page.keyboard.up("Enter")

    def test_billing_left_resets_money_and_splash(self):
        self.open_preview()
        self.advance(200, 45)
        self.page.keyboard.press("ArrowRight")
        self.drive_to_wasted()
        self.page.keyboard.press("Space")
        self.advance(150, 10)
        self.advance(150, 30)
        self.assertEqual(self.state()[0], 5)
        self.assertGreater(self.page.evaluate("Number(drivingPreview.module._preview_bill_total())"), 0)
        self.page.keyboard.down("ArrowLeft")
        self.advance(150, 10)
        self.assertEqual(self.state()[:2], [0, 0])
        self.assertEqual(self.state()[4], 0)
        self.advance(150, 30)
        self.assertEqual(self.state()[0], 2)
        self.page.keyboard.up("ArrowLeft")
        self.page.keyboard.press("ArrowRight")
        self.assertEqual(self.state()[0], 3)
        self.assertEqual(self.state()[4], 0)

    def test_focus_loss_pauses_and_clears_held_keys(self):
        self.open_preview()
        self.advance(200, 45)
        self.page.keyboard.down("ArrowRight")
        self.advance(0)
        self.advance(40)
        before = self.state()
        self.page.evaluate("() => { document.body.tabIndex = -1; document.body.focus(); }")
        self.advance(200, 60)
        self.assertEqual(self.state(), before)
        self.page.locator("#lcd").focus()
        self.advance(40)
        self.assertEqual(self.state()[2], before[2], "blur must release the steering key")
        self.page.keyboard.up("ArrowRight")

    def test_forward_back_movement_is_bounded(self):
        self.open_preview()
        self.advance(200, 45)
        self.page.keyboard.press("ArrowRight")
        start_y = self.state()[5]
        self.page.keyboard.down("ArrowUp")
        self.advance(40, 30)
        self.assertEqual(self.state()[5], start_y - 32)
        self.page.keyboard.up("ArrowUp")
        self.page.keyboard.down("ArrowDown")
        self.advance(40, 60)
        self.assertEqual(self.state()[5], start_y + 32)
        self.page.keyboard.down("ArrowUp")
        self.advance(40, 2)
        self.assertEqual(self.state()[5], start_y + 32)
        self.page.keyboard.up("ArrowUp")
        self.page.keyboard.up("ArrowDown")

    def test_low_gold_pickup_flashes_green(self):
        self.open_preview()
        self.page.evaluate("drivingPreview.module._preview_init(5)")
        self.advance(200, 45)
        self.page.keyboard.press("ArrowRight")
        self.advance(40, 83)
        self.assertEqual(self.state()[4], 250000)
        indicator = self.page.locator('[data-rgb="2"]')
        for ticks, green in ((0, True), (4, True), (1, False), (20, False)):
            self.advance(40, ticks)
            self.assertEqual(indicator.evaluate("(led) => led.classList.contains('on')"), green)
            self.assertEqual(indicator.get_attribute("aria-label"),
                             f"RGB LED 2: {'green' if green else 'off'}")
            if green:
                self.assertEqual(indicator.evaluate("(led) => getComputedStyle(led).backgroundColor"),
                                 "rgb(0, 255, 0)")

    def test_damage_flashes_orange(self):
        self.open_preview()
        self.advance(200, 45)
        self.page.keyboard.press("ArrowRight")
        self.page.keyboard.down("ArrowLeft")
        self.advance(40, 8)
        self.page.keyboard.up("ArrowLeft")
        self.assertEqual(self.page.evaluate("drivingPreview.module._preview_damage()"), 1)
        self.assertGreater(self.page.evaluate("drivingPreview.module._preview_road_denials()"), 0)
        self.assertEqual(self.page.evaluate("drivingPreview.module._preview_tag_denials()"), 1)
        self.assertEqual(self.page.evaluate("drivingPreview.module._preview_protection_failed()"), 0)
        indicator = self.page.locator('[data-rgb="2"]')
        self.assertEqual(indicator.get_attribute("aria-label"), "RGB LED 2: orange")
        self.assertEqual(indicator.evaluate("(led) => getComputedStyle(led).backgroundColor"),
                         "rgb(255, 128, 0)")
        self.advance(40, 5)
        self.assertEqual(indicator.get_attribute("aria-label"), "RGB LED 2: orange")
        self.advance(40)
        self.assertEqual(indicator.get_attribute("aria-label"), "RGB LED 2: off")

    def test_large_gold_pickup_flashes_gold_twice(self):
        self.open_preview()
        self.page.evaluate("drivingPreview.module._preview_init(4)")
        self.advance(200, 45)
        self.page.keyboard.press("ArrowRight")
        self.advance(40, 531)
        self.assertEqual(self.state()[4], 503500)
        indicator = self.page.locator('[data-rgb="2"]')
        for ticks, gold in ((0, True), (7, False), (6, True), (6, False), (6, False)):
            self.advance(40, ticks)
            self.assertEqual(indicator.evaluate("(led) => led.classList.contains('on')"), gold)
            self.assertEqual(indicator.get_attribute("aria-label"),
                             f"RGB LED 2: {'gold' if gold else 'off'}")
            if gold:
                self.assertEqual(indicator.evaluate("(led) => getComputedStyle(led).backgroundColor"),
                                 "rgb(255, 173, 0)")

    def test_realtime_animation_loop(self):
        self.page.goto(self.url)
        self.page.wait_for_function("Boolean(window.drivingPreview)")
        self.page.locator("#lcd").focus()
        self.page.wait_for_function(
            "drivingPreview.module._preview_phase() === 1", timeout=15000
        )
        self.page.wait_for_function(
            "drivingPreview.module._preview_phase() === 2", timeout=10000
        )
        self.assertEqual(
            self.page.evaluate("""() => {
                const canvas = document.getElementById("lcd");
                return [canvas.width, canvas.height];
            }"""),
            [128, 160],
        )
        self.page.keyboard.press("ArrowRight")
        self.page.wait_for_function(
            "drivingPreview.module._preview_distance() >= 4", timeout=5000
        )
        self.assertEqual(self.state()[0], 3)

    def test_native_wasm_trace_and_canvas_parity(self):
        self.open_preview()
        trace = [(3000, 0), (3000, 0), (3000, 0),
                 (0, 8), (100, 8), (0, 0), (0, 1), (200, 1),
                 (0, 0), (0, 16), (0, 0), (0, 2)]
        trace += [(40, 1)] * 40 + [(80, 2)] * 20 + [(200, 1)] * 40
        trace += [(80000, 1), (0, 0), (0, 16), (1500, 16), (4500, 0),
                  (0, 2), (1500, 2), (1500, 0), (40, 4), (40, 8)]
        for seed in (1, 42, 0xFFFFFFFF):
            self.native.preview_init(seed)
            self.page.evaluate("(seed) => drivingPreview.module._preview_init(seed)", seed)
            for milliseconds, input_mask in trace:
                self.native.preview_advance(milliseconds, input_mask)
                pointer = self.native.preview_render()
                raw = ctypes.string_at(pointer, 40960)
                expected = {
                    "phase": self.native.preview_phase(),
                    "artwork": self.native.preview_artwork(),
                    "stars": self.native.preview_stars(),
                    "x": self.native.preview_x(),
                    "distance": self.native.preview_distance(),
                    "money": str(self.native.preview_money()),
                    "bill": str(self.native.preview_bill_total()),
                    "leds": self.native.preview_led_mask(),
                    "rgb": [self.native.preview_rgb1(), self.native.preview_rgb2()],
                    "damage": self.native.preview_damage(),
                    "y": self.native.preview_y(),
                    "pending": self.native.preview_pending_hits(),
                    "scale": self.native.preview_scale(),
                    "origin": self.native.preview_protection_origin(),
                    "failed": self.native.preview_protection_failed(),
                    "denials": [self.native.preview_tag_denials(),
                               self.native.preview_wallet_denials(),
                               self.native.preview_road_denials()],
                    "hash": hashlib.sha256(raw).hexdigest(),
                }
                actual = self.page.evaluate("""async ([ms, input]) => {
                    const m = drivingPreview.module;
                    m._preview_advance(ms, input);
                    const p = m._preview_render();
                    const hash = new Uint8Array(await crypto.subtle.digest(
                        "SHA-256", m.HEAPU8.slice(p, p + 40960)));
                    return {
                        phase: m._preview_phase(), artwork: m._preview_artwork(), stars: m._preview_stars(),
                        x: m._preview_x(), distance: m._preview_distance(),
                        money: m._preview_money().toString(),
                        bill: m._preview_bill_total().toString(),
                        leds: m._preview_led_mask(),
                        rgb: [m._preview_rgb1(), m._preview_rgb2()],
                        damage: m._preview_damage(),
                        y: m._preview_y(), pending: m._preview_pending_hits(),
                        scale: m._preview_scale(),
                        origin: m._preview_protection_origin(),
                        failed: m._preview_protection_failed(),
                        denials: [m._preview_tag_denials(), m._preview_wallet_denials(),
                                  m._preview_road_denials()],
                        hash: Array.from(hash, b => b.toString(16).padStart(2, "0")).join("")
                    };
                }""", [milliseconds, input_mask])
                self.assertEqual(actual, expected, (seed, milliseconds, input_mask))

        self.native.preview_init(1)
        self.native.preview_advance(3000, 0)
        raw = ctypes.string_at(self.native.preview_render(), 40960)
        scale = self.native.preview_scale()
        expected_rgba = bytearray()
        for y in range(160):
            for x in range(128):
                i = (y * 128 + x) * 2
                color = raw[i] | (raw[i + 1] << 8)
                r = (((color >> 11) & 31) * scale) >> 8
                g = (((color >> 5) & 63) * scale) >> 8
                b = ((color & 31) * scale) >> 8
                expected_rgba.extend(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2), 255))
        actual_rgba = self.page.evaluate("""() => {
            const m = drivingPreview.module;
            m._preview_init(1); m._preview_advance(3000, 0); drivingPreview.render();
            return Array.from(document.getElementById("lcd").getContext("2d")
                .getImageData(0, 0, 128, 160).data);
        }""")
        self.assertEqual(bytes(actual_rgba), bytes(expected_rgba))

    def test_hidden_down_switches_during_splash(self):
        self.open_preview()
        self.advance(200, 5)
        original = self.page.evaluate("drivingPreview.module._preview_artwork()")
        labels = ("Original LCD artwork", "Derivative LCD artwork")
        self.assertEqual(self.page.locator("#artwork-name").inner_text(), labels[original])
        brightness = self.page.evaluate("drivingPreview.module._preview_scale()")
        self.page.keyboard.down("ArrowDown")
        self.assertEqual(self.state()[0], 0)
        self.assertEqual(self.page.evaluate("drivingPreview.module._preview_scale()"), brightness)
        self.assertEqual(self.page.evaluate("drivingPreview.module._preview_artwork()"), 1 - original)
        self.assertEqual(self.page.locator("#artwork-name").inner_text(), labels[1 - original])
        self.assertEqual(self.page.locator("#artwork-toggle").get_attribute("aria-pressed"),
                         str(not original).lower())
        self.advance(200, 25)
        self.assertEqual(self.state()[0], 1)
        self.assertEqual(self.page.evaluate("drivingPreview.module._preview_artwork()"), 1 - original)
        self.page.keyboard.up("ArrowDown")
        self.page.keyboard.down("ArrowDown")
        self.assertEqual(self.state()[0], 1)
        self.assertEqual(self.page.evaluate("drivingPreview.module._preview_artwork()"), original)
        self.advance(200, 15)
        self.assertEqual(self.state()[0], 2)
        self.assertEqual(self.page.evaluate("drivingPreview.module._preview_artwork()"), original)
        self.page.keyboard.up("ArrowDown")
        self.page.keyboard.press("ArrowRight")
        self.assertEqual(self.state()[0], 3)

    def test_title_artwork_button_and_hidden_source_qr(self):
        self.open_preview()
        source_link = self.page.get_by_role("link", name="Source code", exact=True)
        self.assertTrue(source_link.is_visible())
        self.assertEqual(source_link.get_attribute("href"),
                         "https://github.com/xjamesmorris/gta6-cheri-sonata")
        self.advance(200, 30)
        original = self.page.evaluate("drivingPreview.module._preview_artwork()")
        button = self.page.locator("#artwork-toggle")
        self.assertEqual(button.inner_text(), "switch artwork")
        size = button.bounding_box()
        self.assertLessEqual(size["width"], 140)
        self.assertLessEqual(size["height"], 32)
        button.click()
        self.assertEqual(button.inner_text(), "switch artwork")
        self.assertEqual(self.page.evaluate("drivingPreview.module._preview_artwork()"), 1 - original)
        self.assertEqual(self.state()[0], 1)
        header_name = "wallpaper_image_alternate.h" if not original else "wallpaper_image.h"
        header = (ROOT / "firmware" / header_name).read_text()
        expected = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{2})", header))
        actual = self.page.evaluate("""() => {
            const m = drivingPreview.module, pointer = m._preview_render();
            return Array.from(m.HEAPU8.subarray(pointer, pointer + 40960));
        }""")
        self.assertEqual(bytes(actual), expected)
        self.advance(200, 15)
        self.page.keyboard.press("ArrowDown")
        self.assertEqual(self.page.evaluate("drivingPreview.module._preview_artwork()"), original)
        self.assertEqual(self.state()[0], 2)
        self.page.keyboard.press("ArrowUp")
        self.assertEqual(self.state()[0], 2)
        self.page.keyboard.down("ArrowLeft")
        self.assertEqual(self.state()[0], 6)
        self.assertIsNotNone(button.get_attribute("disabled"))
        self.page.evaluate("""() => document.getElementById("artwork-toggle")
            .dispatchEvent(new MouseEvent("click", {bubbles: true}))""")
        self.assertEqual(self.page.evaluate("drivingPreview.module._preview_artwork()"), original)
        self.advance(200, 5)
        self.assertEqual(self.state()[0], 6)
        self.page.keyboard.up("ArrowLeft")
        self.screenshot("source-qr")
        subprocess.run(
            [str(ROOT / "tools/generate-source-qr"), "--check", "--verify-image",
             str(OUTPUT / "screenshots/source-qr.png")], check=True, capture_output=True, text=True,
        )
        self.page.locator("#lcd").click()
        self.assertEqual(self.state()[0], 2)
        self.assertIsNone(button.get_attribute("disabled"))
        self.page.keyboard.press("ArrowRight")
        self.assertEqual(self.state()[0], 3)
        self.advance(40, 4)
        before = self.state()
        self.assertIsNotNone(button.get_attribute("disabled"))
        self.page.evaluate("""() => document.getElementById("artwork-toggle")
            .dispatchEvent(new MouseEvent("click", {bubbles: true}))""")
        self.assertEqual(self.state(), before)
        self.assertEqual(self.page.evaluate("drivingPreview.module._preview_artwork()"), original)

    def test_artwork_button_reenables_after_reset(self):
        self.open_preview()
        self.advance(200, 45)
        self.page.keyboard.press("ArrowRight")
        button = self.page.locator("#artwork-toggle")
        self.assertIsNotNone(button.get_attribute("disabled"))
        self.drive_to_wasted()
        self.assertIsNotNone(button.get_attribute("disabled"))
        self.page.keyboard.press("Space")
        self.advance(200, 31)
        self.assertEqual(self.state()[0], 5)
        self.assertIsNotNone(button.get_attribute("disabled"))
        self.page.keyboard.press("ArrowLeft")
        self.advance(200, 8)
        self.assertEqual(self.state()[0], 0)
        self.assertIsNone(button.get_attribute("disabled"))

    def test_title_does_not_start_automatic_play(self):
        self.open_preview()
        self.advance(200, 45)
        self.page.locator("#lcd").click()
        self.page.keyboard.press("Space")
        self.page.keyboard.press("Enter")
        self.advance(200, 150)
        self.assertEqual(self.state()[:2], [2, 0])
        self.assertEqual(self.state()[3:5], [0, 0])
        self.page.keyboard.press("ArrowRight")
        self.assertEqual(self.state()[0], 3)

    def test_pages_project_subpath_and_notices(self):
        staging = tempfile.TemporaryDirectory(dir=ROOT / "build")
        self.addCleanup(staging.cleanup)
        site = pathlib.Path(staging.name) / "gta6-cheri-sonata"
        site.mkdir()
        for name in ("index.html", "preview.js", "game.js", "game.wasm",
                     "artwork-license.txt", "code-license.txt"):
            shutil.copyfile(OUTPUT / name, site / name)
        server = http.server.ThreadingHTTPServer(
            ("127.0.0.1", 0), functools.partial(QuietHandler, directory=staging.name)
        )
        thread = threading.Thread(target=server.serve_forever)
        thread.start()
        self.addCleanup(server.server_close)
        self.addCleanup(thread.join)
        self.addCleanup(server.shutdown)
        self.url = f"http://127.0.0.1:{server.server_port}/gta6-cheri-sonata"

        self.open_preview()
        for name, credit in (("code-license.txt", "MIT License"),
                             ("artwork-license.txt", "CC BY 4.0")):
            response = self.page.request.get(self.url + "/" + name)
            self.assertEqual(response.status, 200)
            self.assertIn(credit, response.text())
        self.advance(200, 45)
        original = self.page.evaluate("drivingPreview.module._preview_artwork()")
        self.page.keyboard.press("ArrowDown")
        self.assertEqual(self.page.evaluate("drivingPreview.module._preview_artwork()"), 1 - original)
        self.page.keyboard.press("ArrowLeft")
        self.assertEqual(self.state()[0], 6)
        self.screenshot("source-qr-subpath")
        subprocess.run(
            [str(ROOT / "tools/generate-source-qr"), "--check", "--verify-image",
             str(OUTPUT / "screenshots/source-qr-subpath.png")],
            check=True, capture_output=True, text=True,
        )
        self.page.locator("#lcd").click()
        self.assertEqual(self.state()[0], 2)
        self.page.keyboard.press("ArrowRight")
        self.assertEqual(self.state()[0], 3)
        before = self.state()
        self.assertIsNotNone(self.page.locator("#artwork-toggle").get_attribute("disabled"))
        self.page.evaluate("""() => document.getElementById("artwork-toggle")
            .dispatchEvent(new MouseEvent("click", {bubbles: true}))""")
        self.assertEqual(self.state(), before)
        self.assertEqual(self.page.evaluate("drivingPreview.module._preview_artwork()"), 1 - original)

    def test_small_viewport_keeps_leds_and_controls_visible(self):
        self.page.set_viewport_size({"width": 320, "height": 700})
        self.open_preview()
        self.assertLessEqual(
            self.page.evaluate("document.documentElement.scrollWidth"), 320
        )
        self.assertTrue(self.page.locator("#artwork-toggle").is_visible())
        self.assertTrue(self.page.locator("#leds span").evaluate_all(
            "(leds) => leds.length === 10 && leds.every(led => {"
            "const r = led.getBoundingClientRect(); return r.left >= 0 && r.right <= innerWidth; })"
        ))

    def test_cheri_indicator_mapping_and_fade(self):
        self.open_preview()
        self.assertEqual(self.page.locator("[data-cheri]").count(), 9)
        self.assertEqual(self.page.locator("[data-cheri].on").count(), 0)
        self.assertEqual(self.page.locator('[data-board-status="legacy"].on').count(), 0)
        self.assertEqual(self.page.locator('[data-board-status="halted"].on').count(), 0)
        self.assertEqual(self.page.locator('[data-board-status="cheri"].on').count(), 1)
        self.advance(200, 45)
        self.page.keyboard.press("ArrowRight")
        self.page.keyboard.down("ArrowLeft")
        self.advance(40, 8)
        self.page.keyboard.up("ArrowLeft")
        self.assertGreater(self.page.evaluate("drivingPreview.module._preview_road_denials()"), 0)
        self.assertGreater(self.page.evaluate("drivingPreview.module._preview_tag_denials()"), 0)
        self.assertEqual(self.page.locator('[data-cheri="0"].on').count(), 1)
        self.assertEqual(self.page.locator('[data-cheri="1"].on').count(), 1)
        self.assertEqual(self.page.locator('[data-cheri="5"].on').count(), 0)
        self.advance(200, 9)
        self.assertEqual(self.page.locator('[data-cheri="0"].on').count(), 0)
        self.assertEqual(self.page.locator('[data-cheri="1"]').get_attribute("data-level"), "127")
        # Isolate retained-cause wiring from later game activity.
        self.page.evaluate("""() => {
            const m = drivingPreview.module;
            const tag = m._preview_tag_denials(), bounds = m._preview_road_denials();
            m._preview_tag_denials = () => tag;
            m._preview_road_denials = () => bounds;
            m._preview_wallet_denials = () => 1;
            m._preview_last_protection_event = () => 2;
            drivingPreview.render();
        }""")
        self.assertEqual(self.page.locator('[data-cheri="5"].on').count(), 1)
        initial = int(self.page.locator('[data-cheri="1"]').get_attribute("data-level"))
        self.advance(200, 4)
        halfway = int(self.page.locator('[data-cheri="1"]').get_attribute("data-level"))
        self.assertGreater(halfway, 0)
        self.assertLess(halfway, initial)
        self.assertEqual(self.page.locator('[data-cheri="5"]').get_attribute("data-level"), "127")
        self.advance(200, 5)
        self.assertEqual(self.page.locator('[data-cheri="1"].on').count(), 0)
        self.assertEqual(self.page.locator('[data-cheri="5"].on').count(), 1)
        for bit in (2, 3, 4, 6, 7, 8):
            self.assertEqual(self.page.locator(f'[data-cheri="{bit}"].on').count(), 0)
        self.page.evaluate("""() => {
            const m = drivingPreview.module, tag = m._preview_tag_denials();
            m._preview_tag_denials = () => tag + 1;
            m._preview_last_protection_event = () => 1;
            drivingPreview.render();
        }""")
        self.advance(200, 9)
        self.assertEqual(self.page.locator('[data-cheri="5"].on').count(), 0)
        # Wasm i32 exports are signed even when the C++ counter is uint32_t.
        self.page.evaluate("""() => {
            drivingPreview.module._preview_wallet_denials = () => -2147483648;
            drivingPreview.module._preview_last_protection_event = () => 2;
            drivingPreview.render();
        }""")
        self.assertEqual(self.page.locator('[data-cheri="5"].on').count(), 1)
        self.advance(200, 9)
        self.assertEqual(self.page.locator('[data-cheri="5"]').get_attribute("data-level"), "127")

    def test_failed_wasm_load_is_visible(self):
        self.expected_errors = True
        self.page.route("**/game.wasm", lambda route: route.abort())
        self.page.goto(self.url + "/?manual")
        self.page.wait_for_selector("#status[data-error]")
        self.assertIn("Preview failed:", self.page.locator("#status").inner_text())


if __name__ == "__main__":
    unittest.main(verbosity=2)
