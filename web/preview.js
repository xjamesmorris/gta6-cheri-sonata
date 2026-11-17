// SPDX-License-Identifier: MIT
"use strict";

(async () => {
  const canvas = document.getElementById("lcd");
  const status = document.getElementById("status");
  const leds = document.querySelectorAll("[data-led]");
  const rgbLeds = document.querySelectorAll("[data-rgb]");
  const cheriLeds = document.querySelectorAll("[data-cheri]");
  const boardLeds = document.querySelectorAll("[data-board-status]");
  const artworkButton = document.getElementById("artwork-toggle");
  const artworkName = document.getElementById("artwork-name");
  const keys = new Set();
  const inputs = new Map([
    ["ArrowLeft", 1], ["ArrowRight", 2], ["ArrowUp", 4],
    ["ArrowDown", 8], ["Space", 16], ["Enter", 16],
  ]);
  const manual = new URLSearchParams(location.search).has("manual");
  let stopped = false;
  let previous = null;

  function fail(error) {
    stopped = true;
    status.dataset.error = "true";
    status.textContent = `Preview failed: ${error.message || error}`;
    if (artworkButton) artworkButton.disabled = true;
    console.error(error);
  }

  function clearInput() {
    keys.clear();
    previous = null;
  }

  function paused() {
    return document.hidden || !document.hasFocus() || document.activeElement !== canvas;
  }

  try {
    const context = canvas.getContext("2d");
    if (!context) throw new Error("A 2D canvas is required.");
    if (typeof createDrivingGame !== "function") {
      throw new Error("game.js could not load. Rebuild with tools/preview.");
    }
    const module = await createDrivingGame({ onAbort: fail });
    if (module._preview_protection_origin() !== 0) {
      throw new Error("The browser must use the explicitly emulated protection backend.");
    }
    const width = module._preview_width();
    const height = module._preview_height();
    canvas.width = width;
    canvas.height = height;
    const image = context.createImageData(width, height);
    module._preview_init(1);
    artworkButton.disabled = false;
    let indicatorMs = 0;
    const faultCounts = new Uint32Array(9);
    const lastFaultMs = new Array(9).fill(-Infinity);
    let retainedCause = -1;
    const eventBits = new Map([[1, 1], [2, 5], [3, 0]]);
    const faultSources = new Map([
      [0, () => module._preview_road_denials()],
      [1, () => module._preview_tag_denials()],
      [5, () => module._preview_wallet_denials()],
    ]);

    function render() {
      const pointer = module._preview_render();
      const scale = module._preview_scale();
      const heap = module.HEAPU8;
      for (let y = 0; y < height; y++) {
        for (let x = 0; x < width; x++) {
          const source = pointer + (y * width + x) * 2;
          const color = heap[source] | (heap[source + 1] << 8);
          const r = (((color >> 11) & 31) * scale) >> 8;
          const g = (((color >> 5) & 63) * scale) >> 8;
          const b = ((color & 31) * scale) >> 8;
          const target = (y * width + x) * 4;
          image.data[target] = (r << 3) | (r >> 2);
          image.data[target + 1] = (g << 2) | (g >> 4);
          image.data[target + 2] = (b << 3) | (b >> 2);
          image.data[target + 3] = 255;
        }
      }
      context.putImageData(image, 0, 0);
      const alternate = module._preview_artwork() !== 0;
      const artworkLabel = alternate ? "Derivative LCD artwork" : "Original LCD artwork";
      if (artworkName.textContent !== artworkLabel) artworkName.textContent = artworkLabel;
      artworkButton.setAttribute("aria-pressed", String(alternate));
      const mask = module._preview_led_mask();
      for (const led of leds) {
        const on = (mask & (1 << Number(led.dataset.led))) !== 0;
        led.classList.toggle("on", on);
        led.setAttribute("aria-label", `ULED ${led.dataset.led}: ${on ? "on" : "off"}`);
      }
      const rgbColors = [module._preview_rgb1(), module._preview_rgb2()];
      for (const led of rgbLeds) {
        const color = rgbColors[Number(led.dataset.rgb) - 1];
        const red = (color >> 16) & 255;
        const green = (color >> 8) & 255;
        const blue = color & 255;
        const strength = Math.max(red, green, blue) / 255;
        const on = color !== 0;
        led.classList.toggle("on", on);
        led.style.backgroundColor = on
          ? `rgb(${Math.round(red + 48 * (1 - strength))}, ${Math.round(green + 48 * (1 - strength))}, ${Math.round(blue + 63 * (1 - strength))})`
          : "";
        led.style.boxShadow = on
          ? `0 0 6px rgba(${red}, ${green}, ${blue}, ${strength})`
          : "";
        const name = !on ? "off" : color === 0xffad00 ? "gold"
          : color === 0xff8000 ? "orange" : red && blue ? "purple"
          : red ? "red" : green ? "green" : "blue";
        led.setAttribute("aria-label", `RGB LED ${led.dataset.rgb}: ${name}`);
      }
      for (const led of boardLeds) {
        const on = ["fpga", "boot", "cheri"].includes(led.dataset.boardStatus);
        led.classList.toggle("on", on);
        led.setAttribute("aria-label", `${led.dataset.name}: ${on ? "on" : "off"} (emulated)`);
      }
      let faultChanged = false;
      for (const [bit, readCount] of faultSources) {
        const count = readCount() >>> 0;
        if (count > faultCounts[bit]) {
          lastFaultMs[bit] = indicatorMs;
          faultChanged = true;
        }
        faultCounts[bit] = count;
      }
      if (faultChanged) {
        const cause = eventBits.get(module._preview_last_protection_event());
        if (cause === undefined) {
          fail(new Error("Unknown emulated protection event."));
          return;
        }
        if (retainedCause >= 0 && retainedCause !== cause) lastFaultMs[retainedCause] = indicatorMs;
        retainedCause = cause;
      }
      for (const led of cheriLeds) {
        const bit = Number(led.dataset.cheri);
        // The board holds the current CSR cause high; older causes then PWM-fade.
        const elapsed = indicatorMs - lastFaultMs[bit];
        const level = bit === retainedCause ? 127
          : Math.max(0, 127 - Math.floor(elapsed * 40000 / 524161));
        const strength = level / 128;
        const on = level > 0;
        led.classList.toggle("on", on);
        led.dataset.level = String(level);
        led.style.backgroundColor = on
          ? `rgb(${Math.round(48 + 207 * strength)}, ${Math.round(48 * (1 - strength))}, ${Math.round(63 * (1 - strength))})`
          : "";
        led.style.boxShadow = on ? `0 0 6px rgba(255, 0, 0, ${strength})` : "";
        led.setAttribute("aria-label", `CHERI ${led.dataset.name}: ${on ? "on" : "off"} (emulated)`);
      }
    }

    function step(milliseconds) {
      if (!paused() && !stopped) {
        let input = 0;
        for (const key of keys) input |= inputs.get(key);
        const elapsed = Math.max(0, Math.min(milliseconds, 200));
        indicatorMs += elapsed;
        module._preview_advance(elapsed, input);
        if (module._preview_protection_failed()) {
          fail(new Error("Capability protection invariant failed; game stopped."));
        }
      }
      render();
    }

    window.addEventListener("keydown", (event) => {
      if (document.activeElement === canvas && inputs.has(event.code) &&
          !event.ctrlKey && !event.altKey && !event.metaKey) {
        event.preventDefault();
        keys.add(event.code);
        step(0);
      }
    });
    window.addEventListener("keyup", (event) => {
      if (keys.delete(event.code)) {
        event.preventDefault();
        step(0);
      }
    });
    canvas.addEventListener("blur", clearInput);
    canvas.addEventListener("focus", clearInput);
    canvas.addEventListener("click", () => {
      if (!stopped && module._preview_phase() === 6) {
        clearInput();
        module._preview_advance(0, 0);
        module._preview_advance(0, 16);
        module._preview_advance(0, 0);
        canvas.focus();
        render();
      }
    });
    artworkButton.addEventListener("click", () => {
      if (stopped) return;
      clearInput();
      module._preview_advance(0, 0);
      module._preview_toggle_artwork();
      canvas.focus();
      render();
    });
    window.addEventListener("blur", clearInput);
    window.addEventListener("focus", clearInput);
    document.addEventListener("visibilitychange", clearInput);
    canvas.focus();
    render();
    window.drivingPreview = { module, render, step };

    function animate(now) {
      if (stopped) return;
      try {
        const elapsed = previous === null ? 0 : Math.floor(now - previous);
        if (!manual) step(elapsed);
        if (stopped) return;
        previous = now - (previous === null ? 0 : (now - previous - elapsed));
        status.textContent = paused()
          ? "Paused. Focus the game to continue."
          : "Shared C++ firmware gameplay - browser display and keyboard adapter.";
        requestAnimationFrame(animate);
      } catch (error) {
        fail(error);
      }
    }
    requestAnimationFrame(animate);
  } catch (error) {
    fail(error);
  }
})();
