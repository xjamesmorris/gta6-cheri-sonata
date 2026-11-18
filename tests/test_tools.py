#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Unit and negative-policy tests for gta6-wallpaper build & safety tools."""

from __future__ import annotations

import concurrent.futures
import contextlib
import copy
import hashlib
import importlib.machinery
import importlib.util
import json
import os
import pathlib
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import types
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent


def load_tool(name: str, rel_path: str):
    path = str(ROOT / rel_path)
    loader = importlib.machinery.SourceFileLoader(name, path)
    spec = importlib.util.spec_from_loader(name, loader)
    assert spec is not None and spec.loader is not None
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


check_slot2 = load_tool("check_slot2_uf2", "tools/check-slot2-uf2")
protection_elf = load_tool("check_protection_elf", "tools/check-protection-elf")
protection_transcript = load_tool("check_protection_transcript", "tools/check-protection-transcript")
preview_address = load_tool("preview_address", "tools/preview-address")


class PreviewAddressTests(unittest.TestCase):
    @staticmethod
    def interface(name, address):
        return {"ifname": name, "link_type": "ether", "flags": ["UP"],
                "addr_info": [{"family": "inet", "scope": "global", "local": address}]}

    def test_prefers_wired_route_and_excludes_wireless_bridge(self):
        interfaces = [
            self.interface("wired", "192.0.2.10"),
            self.interface("wifi", "198.51.100.10"),
            self.interface("bridge", "203.0.113.10"),
            {},
        ]
        routes = [{"dev": "wifi", "metric": 10}, {"dev": "wired", "metric": 100}]
        self.assertEqual(preview_address.choose_address(interfaces, routes, {"wifi", "bridge"}),
                         "192.0.2.10")
        self.assertEqual(preview_address.choose_address(interfaces[:1], []), "192.0.2.10")

    def test_no_lan_or_ambiguous_lan_requires_explicit_host(self):
        for interfaces in ([], [self.interface("lo", "127.0.0.1")],
                           [self.interface("public", "8.8.8.8")],
                           [self.interface("one", "192.0.2.10"),
                            self.interface("two", "198.51.100.10")]):
            with self.subTest(interfaces=interfaces), self.assertRaises(ValueError):
                preview_address.choose_address(interfaces, [])


def make_fake_cheri_elf(extra: bytes = b"\x00" * 64) -> bytes:
    hdr = bytearray(52)
    hdr[0:4] = b"\x7fELF"
    hdr[4] = 1  # ELFCLASS32
    hdr[5] = 1  # little-endian
    struct.pack_into("<HH", hdr, 16, 2, 243)  # ET_EXEC, EM_RISCV
    struct.pack_into("<I", hdr, 36, 0x00030000)  # EF_RISCV_CHERI
    return bytes(hdr) + extra


def wrap_uf2(payload: bytes, base: int = 0x10000000, family: int = 0x6CE29E60, block_size: int = 256) -> bytes:
    total = (len(payload) + block_size - 1) // block_size
    out = bytearray()
    for idx in range(total):
        chunk = payload[idx * block_size : (idx + 1) * block_size]
        padded = chunk + b"\x00" * (block_size - len(chunk))
        blk = bytearray(512)
        struct.pack_into(
            "<8I",
            blk,
            0,
            0x0A324655,
            0x9E5D5157,
            0x00002000,
            base + idx * block_size,
            block_size,
            idx,
            total,
            family,
        )
        blk[32 : 32 + block_size] = padded
        struct.pack_into("<I", blk, 508, 0x0AB16F30)
        out += blk
    return bytes(out)


class Slot2Uf2ValidatorTests(unittest.TestCase):
    def test_accepts_valid_slot2_cheri_elf(self) -> None:
        elf = make_fake_cheri_elf(b"abcde")
        uf2 = wrap_uf2(elf)
        blocks, size = check_slot2.verify_slot2_uf2(uf2, elf)
        self.assertEqual(blocks, 1)
        self.assertEqual(size, 256)

    def test_rejects_slot1_and_slot3_and_bitstream(self) -> None:
        elf = make_fake_cheri_elf()
        for bad_base in (0x00000000, 0x20000000, 0x30000000):
            with self.assertRaisesRegex(ValueError, "not in slot 2"):
                check_slot2.verify_slot2_uf2(wrap_uf2(elf, base=bad_base), elf)
        with self.assertRaisesRegex(ValueError, "not a Sonata application block"):
            check_slot2.verify_slot2_uf2(wrap_uf2(elf, family=0x6CE29E6B), elf)

    def test_rejects_non_cheri_elf_and_mismatched_strip(self) -> None:
        bad_elf = bytearray(make_fake_cheri_elf())
        struct.pack_into("<I", bad_elf, 36, 0)
        with self.assertRaisesRegex(ValueError, "CHERI"):
            check_slot2.verify_slot2_uf2(wrap_uf2(bytes(bad_elf)))

        elf = make_fake_cheri_elf(b"1234")
        with self.assertRaisesRegex(ValueError, "does not match stripped ELF"):
            check_slot2.verify_slot2_uf2(wrap_uf2(elf), elf + b"\x01")


@contextlib.contextmanager
def loader_fixture():
    readlink = shutil.which("readlink")
    assert readlink is not None
    (ROOT / "build").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(dir=ROOT / "build") as tmp:
        root = pathlib.Path(tmp).resolve()
        tools = root / "tools"
        tools.mkdir()
        for name in ("load-slot2", "check-slot2-uf2"):
            shutil.copy2(ROOT / "tools" / name, tools / name)

        def write_artifacts(directory, elf, base=0x10000000):
            directory.mkdir(parents=True, exist_ok=True)
            (directory / "gta6_wallpaper.strip").write_bytes(elf)
            image = directory / "gta6_wallpaper.slot2.uf2"
            image.write_bytes(wrap_uf2(elf, base=base))
            (directory / "gta6_wallpaper.json").write_text(
                json.dumps({"final_hash": "fixture"}), encoding="utf-8"
            )
            return image

        release = root / "build" / "cheriot" / "cheriot" / "release"
        build_uf2 = write_artifacts(release, make_fake_cheri_elf())
        ftdi, uf2 = root / "ftdi", root / "uf2"
        ftdi.touch()
        uf2.touch()
        state, scratch = root / "state", root / "build"
        state.mkdir()
        commands = {
            "udevadm": r"""
import os, sys
device = sys.argv[-1].removeprefix("--name=")
if device == os.environ["SONATA_FTDI_DEVICE"]:
    props = {"ID_VENDOR_ID": "0403", "ID_MODEL_ID": "6011",
             "ID_SERIAL_SHORT": os.environ.get("TEST_FTDI_SERIAL", "SYNTHETIC-FTDI-0001")}
    path = "/devices/1-2/1-2.1"
elif device == os.environ["SONATA_UF2_DEVICE"]:
    props = {"ID_VENDOR_ID": "2b3e", "ID_MODEL_ID": "4002",
             "ID_FS_LABEL": "SONATA",
             "ID_FS_TYPE": os.environ.get("TEST_UF2_FS_TYPE", "vfat"),
             "ID_SERIAL_SHORT": os.environ.get("TEST_UF2_SERIAL", "SYNTHETIC-UF2-0001"),
             "DEVTYPE": "disk"}
    hub = os.environ.get("TEST_UF2_HUB", "1-2")
    path = f"/devices/{hub}/{hub}.2/block/uf2"
else:
    sys.exit(f"Unexpected device: {device}")
if "--query=property" in sys.argv:
    print("\n".join(f"{key}={value}" for key, value in props.items()))
elif "--query=path" in sys.argv:
    print(path)
else:
    sys.exit("Unexpected udevadm query")
""",
            "readlink": f"""
import os, sys
if sys.argv[-1].startswith("/sys/devices/"):
    print(sys.argv[-1])
else:
    os.execv({readlink!r}, [{readlink!r}, *sys.argv[1:]])
""",
            "findmnt": r"""
import os, pathlib, sys
mounted = pathlib.Path(os.environ["TEST_STATE"], "mounted")
if not mounted.exists():
    raise SystemExit(1)
if "TARGET" in sys.argv:
    print(mounted.read_text())
elif "FSTYPE" in sys.argv:
    print(os.environ.get("TEST_MOUNT_FS_TYPE", "vfat"))
""",
            "udisksctl": r"""
import os, pathlib, shutil, sys
state = pathlib.Path(os.environ["TEST_STATE"])
with open(state / "udisksctl.log", "a") as log:
    log.write(" ".join(sys.argv[1:]) + "\n")
device = sys.argv[sys.argv.index("-b") + 1]
if device != os.environ["SONATA_UF2_DEVICE"]:
    sys.exit(f"Unexpected device: {device}")
if sys.argv[1] == "mount":
    if os.environ.get("TEST_REBUILD_AT_MOUNT"):
        shutil.copyfile(os.environ["TEST_REBUILD_AT_MOUNT"], os.environ["TEST_BUILD_UF2"])
    (state / "mount").mkdir(exist_ok=True)
    present = os.environ.get("TEST_VOLUME_ENTRY")
    entry = state / "mount" / pathlib.Path(os.environ["TEST_BUILD_UF2"]).name
    if present == "directory":
        entry.mkdir()
    elif present == "stale file":
        entry.write_bytes(b"left by an earlier load")
    elif present:
        entry.symlink_to(present)
    (state / "mounted").write_text(str(state / "mount"))
    print(f"Mounted {device} at {state / 'mount'}")
elif sys.argv[1] == "unmount":
    if os.environ.get("TEST_UNMOUNT_FAILS"):
        sys.exit("Error unmounting: target is busy")
    (state / "mounted").unlink()
else:
    sys.exit("Unexpected udisksctl verb")
""",
            "systemd-inhibit": r"""
import os, pathlib, shutil, stat, sys
if os.environ.get("TEST_INHIBITOR_FAILS"):
    sys.exit("Authorization refused")
command = sys.argv[1:]
while command[0].startswith("--"):
    del command[0]
if stat.S_IMODE(pathlib.Path(command[-2]).stat().st_mode) != 0o700:
    sys.exit("Approved snapshot directory is not private")
if os.environ.get("TEST_SWITCH_BOARD"):
    os.environ["TEST_FTDI_SERIAL"] = "SYNTHETIC-OTHER-BOARD"
if os.environ.get("TEST_REPLACE_SNAPSHOT"):
    for name in ("gta6_wallpaper.slot2.uf2", "gta6_wallpaper.strip", "gta6_wallpaper.json"):
        shutil.copyfile(pathlib.Path(os.environ["TEST_REPLACE_SNAPSHOT"], name),
                        pathlib.Path(command[-2], name))
os.closerange(3, 1024)
os.execv(command[0], command)
""",
            "sync": "",
            "sleep": r"""
import os
arriving = os.environ.get("TEST_CONSOLE_OUTPUT")
if arriving:
    with open(arriving, "rb") as source, open(os.environ["SONATA_CONSOLE_LOG"], "ab") as log:
        log.write(source.read())
""",
        }
        for name, code in commands.items():
            command = tools / name
            command.write_text(f"#!{sys.executable}\n{code}", encoding="utf-8")
            command.chmod(0o755)
        env = {
            **os.environ,
            "PATH": f"{tools}:{os.environ['PATH']}",
            "SONATA_FTDI_DEVICE": str(ftdi),
            "SONATA_UF2_DEVICE": str(uf2),
            "SONATA_BOARD_SERIAL": "SYNTHETIC-FTDI-0001",
            "SONATA_UF2_SERIAL": "SYNTHETIC-UF2-0001",
            "SONATA_CONSOLE_LOG": str(root / "console.log"),
            "SONATA_EXPECT_UF2_SHA256": "",
            "SONATA_LOAD_INNER": "",
            "TEST_FTDI_SERIAL": "SYNTHETIC-FTDI-0001",
            "TEST_UF2_SERIAL": "SYNTHETIC-UF2-0001",
            "TEST_UF2_HUB": "1-2",
            "TEST_STATE": str(state),
            "TEST_BUILD_UF2": str(build_uf2),
        }
        for name in commands:
            assert shutil.which(name, path=env["PATH"]) == str(tools / name)

        def run(*args, cwd=None, text=True, **overrides):
            return subprocess.run(
                [str(tools / "load-slot2"), *args],
                env={**env, **overrides},
                cwd=cwd,
                stdin=subprocess.DEVNULL,
                capture_output=True,
                text=text,
                timeout=30,
            )

        yield types.SimpleNamespace(
            root=root, release=release, run=run, write_artifacts=write_artifacts,
            build_uf2=build_uf2, scratch=scratch,
            digest=hashlib.sha256(build_uf2.read_bytes()).hexdigest(),
            console_log=root / "console.log",
            latch=root / "build" / "load-slot2-not-finished",
            mounted_copy=state / "mount" / build_uf2.name,
            udisks_log=state / "udisksctl.log",
        )


class LoadSlot2Tests(unittest.TestCase):
    def test_explicit_identity_and_device_configuration_is_required(self) -> None:
        with loader_fixture() as fx:
            for name in ("SONATA_BOARD_SERIAL", "SONATA_UF2_SERIAL",
                         "SONATA_FTDI_DEVICE", "SONATA_UF2_DEVICE"):
                with self.subTest(name=name):
                    result = fx.run("load", "--yes-pmod0-disconnected", **{name: ""})
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("configuration is required", result.stderr)
                    self.assertFalse(fx.udisks_log.exists())
                    self.assertFalse(fx.latch.exists())
                    self.assertEqual(list(fx.scratch.glob(".slot2-approved-*")), [])
            result = fx.run("preflight", SONATA_FTDI_DEVICE="relative-device")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("paths must be absolute", result.stderr)
            self.assertFalse(fx.udisks_log.exists())

    def test_ftdi_override_preserves_board_and_hub_checks(self) -> None:
        with loader_fixture() as fx:
            for overrides, error in (
                ({}, None),
                ({"TEST_FTDI_SERIAL": "SYNTHETIC-OTHER-BOARD"}, "does not match Sonata FTDI serial"),
                ({"TEST_UF2_SERIAL": "SYNTHETIC-OTHER-UF2"}, "not a SONATA RP2040 programming drive"),
                ({"TEST_UF2_HUB": "1-3"}, "not on the same Sonata USB hub"),
            ):
                with self.subTest(overrides=overrides):
                    result = fx.run("preflight", **overrides)
                    if error is None:
                        self.assertEqual(result.returncode, 0, result.stderr)
                        self.assertIn("preflight passed for application slot 2", result.stdout)
                    else:
                        self.assertNotEqual(result.returncode, 0)
                        self.assertIn(error, result.stderr)

    def test_mounted_device_is_rejected_without_writing(self) -> None:
        with loader_fixture() as fx:
            (fx.root / "state" / "mounted").write_text(str(fx.root / "fixture-mount"))
            result = fx.run("preflight")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("already mounted", result.stderr)
            self.assertFalse(fx.udisks_log.exists())
            self.assertFalse(fx.latch.exists())

    def test_preflight_is_read_only_and_ignores_cwd_modules(self) -> None:
        with loader_fixture() as fx:
            cwd = fx.root / "cwd"
            cwd.mkdir()
            for module in ("json", "struct"):
                (cwd / f"{module}.py").write_text(
                    f"open({str(cwd / 'imported')!r}, 'w').close()\n"
                    f"raise SystemExit('{module} imported from caller directory')\n",
                    encoding="utf-8",
                )
            before = sorted(fx.root.rglob("*"))
            result = fx.run("preflight", cwd=cwd, PYTHONPATH=str(cwd))
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("report_final_hash=fixture", result.stdout)
            self.assertEqual(sorted(fx.root.rglob("*")), before)

    def test_load_writes_snapshot_despite_concurrent_rebuild(self) -> None:
        with loader_fixture() as fx:
            approved = fx.build_uf2.read_bytes()
            rebuilt = fx.root / "rebuilt.uf2"
            rebuilt.write_bytes(wrap_uf2(make_fake_cheri_elf(), base=0x20000000))
            digest = hashlib.sha256(approved).hexdigest()
            result = fx.run(
                "load", "--yes-pmod0-disconnected",
                SONATA_EXPECT_UF2_SHA256=digest, TEST_REBUILD_AT_MOUNT=str(rebuilt),
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(fx.build_uf2.read_bytes(), rebuilt.read_bytes())
            self.assertEqual(fx.mounted_copy.read_bytes(), approved)
            self.assertIn(f"slot2_uf2_sha256={digest}", result.stdout)
            self.assertFalse(fx.latch.exists())
            self.assertEqual(list(fx.scratch.glob(".slot2-approved-*")), [])

    def test_inner_load_rejects_bad_snapshot_before_mounting(self) -> None:
        with loader_fixture() as fx:
            slot3_dir = fx.root / "slot3"
            slot3 = fx.write_artifacts(slot3_dir, make_fake_cheri_elf(), base=0x20000000)
            for args, code in (
                ((), 2),
                ((str(fx.release), "0" * 64), 1),
                ((str(slot3_dir), hashlib.sha256(slot3.read_bytes()).hexdigest()), 1),
            ):
                with self.subTest(args=args):
                    result = fx.run("_inner_load", *args, SONATA_LOAD_INNER="1")
                    self.assertEqual(result.returncode, code, result.stderr)
                    self.assertFalse(fx.udisks_log.exists())
                    self.assertFalse(fx.latch.exists())

    def test_changed_approval_or_board_and_inhibitor_failure_do_not_mount(self) -> None:
        with loader_fixture() as fx:
            replacement = fx.root / "replacement"
            fx.write_artifacts(replacement, make_fake_cheri_elf(b"new build"))
            for overrides, error in (
                ({"SONATA_EXPECT_UF2_SHA256": "0" * 64}, "does not match"),
                ({"SONATA_EXPECT_UF2_SHA256": "invalid"}, "64 lowercase"),
                ({"SONATA_EXPECT_UF2_SHA256": ""}, "requires SONATA_EXPECT_UF2_SHA256"),
                ({"TEST_SWITCH_BOARD": "1"}, "does not match Sonata FTDI"),
                ({"TEST_REPLACE_SNAPSHOT": str(replacement)}, "does not match"),
                ({"TEST_INHIBITOR_FAILS": "1"}, "Authorization refused"),
                ({"TEST_UF2_FS_TYPE": "ext4"}, "not a SONATA RP2040 programming drive"),
            ):
                with self.subTest(overrides=overrides):
                    result = fx.run("load", "--yes-pmod0-disconnected",
                                    **{"SONATA_EXPECT_UF2_SHA256": fx.digest, **overrides})
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(error, result.stderr)
                    self.assertFalse(fx.udisks_log.exists())
                    self.assertFalse(fx.latch.exists())
                    self.assertEqual(list(fx.scratch.glob(".slot2-approved-*")), [])

    def test_failed_unmount_keeps_latch_and_cleans_snapshot(self) -> None:
        with loader_fixture() as fx:
            result = fx.run("load", "--yes-pmod0-disconnected",
                            SONATA_EXPECT_UF2_SHA256=fx.digest, TEST_UNMOUNT_FAILS="1")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(f"sha256={fx.digest}\n", fx.latch.read_text())
            self.assertEqual(list(fx.scratch.glob(".slot2-approved-*")), [])
            mounts = fx.udisks_log.read_text()
            result = fx.run("load", "--yes-pmod0-disconnected", SONATA_EXPECT_UF2_SHA256=fx.digest)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Unfinished load latch exists", result.stderr)
            self.assertEqual(fx.udisks_log.read_text(), mounts)
            self.assertEqual(list(fx.scratch.glob(".slot2-approved-*")), [])

    def test_only_fat_volumes_and_regular_destinations_are_written(self) -> None:
        with loader_fixture() as fx:
            approved = fx.build_uf2.read_bytes()
            victim = fx.root / "victim"
            victim.write_bytes(b"operator file")
            for overrides, error in (
                ({"TEST_MOUNT_FS_TYPE": "ext4"}, "is mounted as 'ext4', not vfat"),
                ({"TEST_VOLUME_ENTRY": str(victim)}, "exists and is not a regular file"),
                ({"TEST_VOLUME_ENTRY": str(fx.root / "dangling")}, "exists and is not a regular file"),
                ({"TEST_VOLUME_ENTRY": "directory"}, "exists and is not a regular file"),
            ):
                with self.subTest(overrides=overrides):
                    result = fx.run("load", "--yes-pmod0-disconnected",
                                    SONATA_EXPECT_UF2_SHA256=fx.digest, **overrides)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(error, result.stderr)
                    self.assertIn("nothing was written", result.stderr)
                    self.assertEqual(victim.read_bytes(), b"operator file")
                    self.assertFalse((fx.root / "dangling").exists())
                    self.assertFalse(fx.mounted_copy.is_file() and not fx.mounted_copy.is_symlink())
                    self.assertFalse(fx.latch.exists())
                    self.assertFalse((fx.root / "state" / "mounted").exists())
                    self.assertEqual(list(fx.scratch.glob(".slot2-approved-*")), [])
                    shutil.rmtree(fx.root / "state" / "mount")
            fx.udisks_log.unlink()
            result = fx.run("load", "--yes-pmod0-disconnected",
                            SONATA_EXPECT_UF2_SHA256=fx.digest, TEST_VOLUME_ENTRY="stale file")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(fx.mounted_copy.read_bytes(), approved)
            self.assertFalse(fx.latch.exists())
            self.assertIn("--options nosuid,nodev,noexec,nosymfollow", fx.udisks_log.read_text())

    def test_console_echo_is_bounded_and_cannot_drive_the_terminal(self) -> None:
        with loader_fixture() as fx:
            fx.console_log.write_bytes(b"before the load\n")
            arriving = fx.root / "arriving"
            arriving.write_bytes(
                b"\x1b[32mcaptest\x1b[0m: PASS hardware\r\n"
                b"\x1b]0;title\x07\x1b[2J\x1b[1A\x9b31m\tOK: forged \xc2\x9b0m\x7f\n"
                + b"x" * 70000 + b"\nafter the limit\n"
            )
            result = fx.run("load", "--yes-pmod0-disconnected", text=False,
                            SONATA_EXPECT_UF2_SHA256=fx.digest, TEST_CONSOLE_OUTPUT=str(arriving))
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue(all(byte == 10 or 32 <= byte < 127 for byte in result.stdout))
            self.assertIn(b"\ncaptest: PASS hardware\n", result.stdout)
            self.assertIn(b"\n^[]0;title^G^[[2J^[[1AM-^[31m^IOK: forged M-BM-^[0m^?\n", result.stdout)
            self.assertNotIn(b"before the load", result.stdout)
            self.assertNotIn(b"after the limit", result.stdout)
            self.assertLessEqual(result.stdout.count(b"x" * 1000), 65)
            self.assertIn(b"Truncated: showed the first 65536 of 70", result.stdout)


class ConvertWallpaperGeometryTests(unittest.TestCase):
    def test_rgb565_rotation_and_portrait_pixel_art(self) -> None:
        code = r"""
import importlib.machinery, importlib.util, pathlib, re, subprocess, sys, tempfile
from PIL import Image

loader = importlib.machinery.SourceFileLoader("conv", sys.argv[1])
spec = importlib.util.spec_from_loader("conv", loader)
conv = importlib.util.module_from_spec(spec)
spec.loader.exec_module(conv)

# Verify RGB565 little-endian encoding matches ST7735 LCD_rgb565_to_bgr565:
# Red (255,0,0) -> 0xF800 -> [0x00, 0xF8]
# Green (0,255,0) -> 0x07E0 -> [0xE0, 0x07]
# Blue (0,0,255) -> 0x001F -> [0x1F, 0x00]
img = Image.new("RGB", (3, 1))
img.putpixel((0, 0), (255, 0, 0))
img.putpixel((1, 0), (0, 255, 0))
img.putpixel((2, 0), (0, 0, 255))
raw, _ = conv.quantize_rgb565(img, dither=False)
assert list(raw) == [0x00, 0xF8, 0xE0, 0x07, 0x1F, 0x00], list(raw)

# Verify ccw90 puts landscape top edge (y=0) along panel x=0 (System USB edge),
# with landscape top-right (159,0) at panel (0,0) and top-left (0,0) at (0,159).
land = Image.new("RGB", (160, 128), (0, 0, 0))
land.putpixel((0, 0), (255, 0, 0))
land.putpixel((159, 0), (0, 255, 0))
land.putpixel((0, 127), (0, 0, 255))
panel = conv.rotate_for_panel(land, "ccw90")
assert panel.size == (128, 160), panel.size
assert panel.getpixel((0, 159)) == (255, 0, 0)
assert panel.getpixel((0, 0)) == (0, 255, 0)
assert panel.getpixel((127, 159)) == (0, 0, 255)

# Native portrait keeps the top-left at the top-left without a pre-rotation.
portrait = Image.new("RGB", (256, 320), (0, 0, 0))
for y in range(320):
    for x in range(256):
        portrait.putpixel((x, y), (x, y * 255 // 319, (x + y) % 256))
for style in ("pixel-art", "retro"):
    art = conv.make_wallpaper(portrait, "crop", "0", style)
    assert art.size == (128, 160)
    assert len(art.getcolors(256)) <= 32
    for y in range(0, 160, 2):
        for x in range(0, 128, 2):
            assert len(art.crop((x, y, x + 2, y + 2)).getcolors(4)) == 1
    raw, preview = conv.quantize_rgb565(art, dither=False)
    assert len(set(raw[i:i+2] for i in range(0, len(raw), 2))) <= 32
    assert preview.size == (128, 160)

native = portrait.resize((128, 160), Image.Resampling.NEAREST)
enlarged = native.resize((512, 640), Image.Resampling.NEAREST)
recovered = conv.make_wallpaper(enlarged, "crop", "0", "native")
assert recovered.tobytes() == native.tobytes()

corners = Image.new("RGB", (128, 160), (0, 0, 0))
corners.paste((255, 0, 0), (0, 0, 64, 80))
corners.paste((0, 255, 0), (64, 0, 128, 80))
corners.paste((0, 0, 255), (0, 80, 64, 160))
corners.paste((255, 255, 0), (64, 80, 128, 160))
art = conv.make_wallpaper(corners, "crop", "0", "pixel-art")
assert art.getpixel((0, 0)) == (255, 0, 0)
assert art.getpixel((127, 0)) == (0, 255, 0)
assert art.getpixel((0, 159)) == (0, 0, 255)
assert art.getpixel((127, 159)) == (255, 255, 0)

# Keep supplied composition/lettering intact instead of drawing a new title.
source = Image.new("RGB", (16, 32), (30, 70, 90))
art = conv.make_wallpaper(source, "letterbox", "0", "artwork")
assert art.size == (128, 160)
assert art.getpixel((0, 80)) == (0, 0, 0)
assert art.getpixel((24, 80)) == (30, 70, 90)
assert art.getpixel((64, 80)) == (30, 70, 90)
assert art.getpixel((103, 80)) == (30, 70, 90)
assert art.getpixel((104, 80)) == (0, 0, 0)

# Crop away the outer panels, with no letterbox bars or replacement logo.
crop_source = Image.new("RGB", (128, 320), (30, 70, 90))
crop_source.paste((255, 0, 0), (0, 0, 128, 40))
crop_source.paste((0, 0, 255), (0, 280, 128, 320))
art = conv.make_wallpaper(crop_source, "crop", "0", "retro")
assert art.size == (128, 160)
assert art.getcolors() == [(128 * 160, (30, 70, 90))]
expected, _ = conv.quantize_rgb565(art, dither=False)

project = pathlib.Path(sys.argv[1]).resolve().parent.parent
(project / "build").mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(dir=project / "build") as tmp:
    output = pathlib.Path(tmp) / "wallpaper.h"
    subprocess.run([sys.executable, sys.argv[1], "--artwork", "primary", "--output", str(output)],
                   check=True, capture_output=True, text=True)
    header = output.read_text()
    assert "SPDX-License-Identifier: CC-BY-4.0" in header
    assert "artwork/README.md" in header
    assert "SPDX-License-Identifier: MIT" not in header
    primary = project / "firmware" / "wallpaper_image.h"
    original_primary = primary.read_bytes()
    with Image.open(project / "artwork" / "gta6-splash-orig-lcd.png") as source:
        primary_pixels = source.convert("RGB").resize((128, 160), Image.Resampling.NEAREST)
        assert primary_pixels.resize(source.size, Image.Resampling.NEAREST).tobytes() == source.tobytes()
    primary_expected, _ = conv.quantize_rgb565(primary_pixels, dither=False)
    assert bytes(int(x, 16) for x in re.findall(r"0x([0-9a-f]{2})", header)) == primary_expected
    alternate = pathlib.Path(tmp) / "alternate.h"
    subprocess.run([sys.executable, sys.argv[1], "--artwork", "alternate",
                    "--output", str(alternate)], check=True, capture_output=True, text=True)
    alternate_header = alternate.read_text()
    assert "SPDX-License-Identifier: CC-BY-4.0" in alternate_header
    with Image.open(project / "artwork" / "gta6-splash-derivative-lcd.png") as source:
        alternate_pixels = source.convert("RGB").crop((0, 128, 1024, 1408)).resize(
            (128, 160), Image.Resampling.LANCZOS)
    alternate_expected, _ = conv.quantize_rgb565(alternate_pixels, dither=False)
    assert len(set(alternate_expected[i:i+2] for i in range(0, len(alternate_expected), 2))) > 32
    assert any(alternate_expected[i:i+2] != alternate_expected[i+2:i+4]
               for i in range(0, len(alternate_expected), 4))
    assert bytes(int(x, 16) for x in re.findall(r"0x([0-9a-f]{2})", alternate_header)) == alternate_expected
    assert alternate_header != header
    subprocess.run([sys.executable, sys.argv[1], "--output", str(output)],
                   check=True, capture_output=True, text=True)
    assert output.read_text() == header
    for selection in ("primary", "alternate"):
        subprocess.run([sys.executable, sys.argv[1], "--artwork", selection, "--check"],
                       check=True, capture_output=True, text=True)
    assert primary.read_bytes() == original_primary
    custom = pathlib.Path(tmp) / "custom.png"
    crop_source.save(custom)
    subprocess.run([sys.executable, sys.argv[1], "--input", str(custom),
                    "--output", str(output)], check=True, capture_output=True, text=True)
    custom_header = output.read_text()
    assert "SPDX-License-Identifier:" not in custom_header
    assert bytes(int(x, 16) for x in re.findall(r"0x([0-9a-f]{2})", custom_header)) == expected
    conflict = subprocess.run(
        [sys.executable, sys.argv[1], "--artwork", "alternate", "--input", str(custom),
         "--output", str(output)], capture_output=True, text=True)
    assert conflict.returncode == 2
    for filename in ("gta6-splash-orig-lcd.png", "gta6-splash-derivative-lcd.png"):
        same_name_custom = pathlib.Path(tmp) / filename
        crop_source.save(same_name_custom)
        subprocess.run([sys.executable, sys.argv[1], "--input", str(same_name_custom),
                        "--output", str(output)], check=True, capture_output=True, text=True)
        assert "SPDX-License-Identifier:" not in output.read_text()
"""
        subprocess.run([sys.executable, "-B", "-c", code, str(ROOT / "tools" / "convert-wallpaper")], check=True)

    def test_build_helpers_reject_invalid_artwork(self) -> None:
        env = {**os.environ, "GTA6_WALLPAPER_SHELL": "1", "GTA6_PREVIEW_SHELL": "1"}
        for tool in ("check", "check-browser", "preview", "sim-test"):
            for arguments in (("--artwork",), ("--artwork", "unknown")):
                with self.subTest(tool=tool, arguments=arguments):
                    result = subprocess.run(
                        [str(ROOT / "tools" / tool), *arguments], env=env,
                        capture_output=True, text=True, timeout=10,
                    )
                    self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                    self.assertIn("artwork", result.stderr)

    def test_custom_input_name_cannot_leave_comment_or_replace_bundled_header(self) -> None:
        tool = str(ROOT / "tools" / "convert-wallpaper")
        artwork = ROOT / "artwork" / "gta6-splash-orig-lcd.png"
        bundled = ROOT / "firmware" / "wallpaper_image.h"
        original = bundled.read_bytes()
        self.addCleanup(bundled.write_bytes, original)
        (ROOT / "build").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as tmp:
            headers = []
            for name in ("x\n#error injected\r\\\x1b[2J\u2028.png", "plain.png"):
                image = pathlib.Path(tmp) / name
                shutil.copyfile(artwork, image)
                output = pathlib.Path(tmp) / "custom.h"
                result = subprocess.run(
                    [sys.executable, "-B", tool, "--input", str(image), "--output", str(output)],
                    capture_output=True, text=True,
                )
                self.assertEqual(result.returncode, 0, result.stderr)
                headers.append(output.read_text(encoding="utf-8").split("\n"))
            hostile, plain = headers
            self.assertEqual(
                hostile[1],
                "// Source: x\\n#error injected\\r\\\\\\x1b[2J\\u2028.png "
                "(fit=crop, rotate=0, style=artwork, dither=False)",
            )
            self.assertEqual(hostile[:1] + hostile[2:], plain[:1] + plain[2:])
            for mode in ((), ("--check",)):
                result = subprocess.run(
                    [sys.executable, "-B", tool, "--input", str(image), *mode],
                    capture_output=True, text=True,
                )
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertIn("--input requires an explicit --output", result.stderr)
        self.assertEqual(bundled.read_bytes(), original)


class ArtworkBuildTests(unittest.TestCase):
    def test_both_runtime_artworks_are_linked_once(self) -> None:
        selection = os.environ.get("GTA6_TEST_ARTWORK")
        path = ROOT / "build/cheriot/cheriot/release/gta6_wallpaper.strip"
        if selection is None or not path.is_file():
            self.skipTest("requires the selected firmware build from tools/check")
        headers = {
            "primary": "wallpaper_image.h",
            "alternate": "wallpaper_image_alternate.h",
        }
        self.assertIn(selection, headers)
        firmware = path.read_bytes()
        for name, filename in headers.items():
            header = (ROOT / "firmware" / filename).read_text()
            pixels = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{2})", header))
            self.assertEqual(firmware.count(pixels), 1, name)

    def test_hardware_image_is_accompanied_by_license_notices(self) -> None:
        release = ROOT / "build/cheriot/cheriot/release"
        if os.environ.get("GTA6_TEST_ARTWORK") is None or not (release / "gta6_wallpaper.slot2.uf2").is_file():
            self.skipTest("requires the firmware build from tools/check")
        for suffix, source in (("code-license", "LICENSE"), ("artwork-license", "artwork/README.md")):
            self.assertEqual((release / f"gta6_wallpaper.{suffix}.txt").read_bytes(),
                             (ROOT / source).read_bytes())
        notices = (release / "gta6_wallpaper.third-party-notices.txt").read_text(encoding="utf-8")
        summary = (ROOT / "THIRD-PARTY-NOTICES.md").read_text(encoding="utf-8")
        self.assertTrue(notices.startswith("===== THIRD-PARTY-NOTICES.md =====\n\n" + summary))
        for marker in (
            "===== LICENSES/Apache-2.0.txt =====\n\n\n                                 Apache License",
            "===== cheriot-rtos/LICENSE =====",
            "Copyright (c) Microsoft Corporation.",
            "===== cheriot-rtos/THIRD_PARTY_NOTICES.txt =====",
            "Copyright (c) 2019 - 2022 Daniil Goncharov",
            "The Regents of the University of California",
            "===== third_party/display_drivers/LICENSE =====",
            "Copyright [2022] [Douglas Reis]",
        ):
            self.assertIn(marker, notices)


APACHE_2_0_SHA256 = "cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30"
LOWRISC_DERIVED = ("firmware/lcd_panel.cc", "firmware/lcd_panel.hh", "xmake.lua")


class LicenseNoticeTests(unittest.TestCase):
    def test_upstream_derived_files_keep_their_license_and_notice(self) -> None:
        lock = json.loads((ROOT / "flake.lock").read_text(encoding="utf-8"))
        revision = lock["nodes"]["sonata-software"]["locked"]["rev"]
        notices = (ROOT / "THIRD-PARTY-NOTICES.md").read_text(encoding="utf-8")
        project_license = (ROOT / "LICENSE").read_text(encoding="utf-8")
        self.assertIn(revision, notices)
        for name in LOWRISC_DERIVED:
            with self.subTest(name=name):
                header = (ROOT / name).read_text(encoding="utf-8")[:1600]
                self.assertIn("Copyright lowRISC Contributors.", header)
                self.assertIn("SPDX-License-Identifier: Apache-2.0", header)
                self.assertIn("Changes made by the Grand Time Auto project", header)
                self.assertIn(revision, header)
                self.assertIn(f"`{name}`", notices)
                self.assertIn(name, project_license)
        apache = (ROOT / "LICENSES" / "Apache-2.0.txt").read_bytes()
        self.assertEqual(hashlib.sha256(apache).hexdigest(), APACHE_2_0_SHA256)

    def test_every_source_file_declares_its_expected_license(self) -> None:
        expected = {name: "Apache-2.0" for name in LOWRISC_DERIVED}
        expected.update({"firmware/wallpaper_image.h": "CC-BY-4.0",
                         "firmware/wallpaper_image_alternate.h": "CC-BY-4.0"})
        sources = [ROOT / name for name in (
            "xmake.lua", "flake.nix", "policy/gta6_wallpaper.rego", "web/preview.cc", "web/preview.js")]
        for directory in ("firmware", "tools", "tests"):
            sources += sorted(path for path in (ROOT / directory).iterdir() if path.is_file())
        for path in sources:
            name = path.relative_to(ROOT).as_posix()
            with self.subTest(name=name), path.open(encoding="utf-8") as source:
                head = "".join(line for _, line in zip(range(4), source))
                self.assertEqual(re.findall(r"SPDX-License-Identifier: (\S+)", head),
                                 [expected.get(name, "MIT")])


class PrefetchMirroredInputsTests(unittest.TestCase):
    def run_tool(self, reported_hash):
        with tempfile.TemporaryDirectory() as tmp:
            stub = pathlib.Path(tmp, "nix")
            stub.write_text(f"#!{sys.executable}\n" + r"""
import json, os, sys
with open(os.environ["TEST_NIX_CALLS"], "a") as log:
    log.write(json.dumps(sys.argv[1:]) + "\n")
print(json.dumps({"hash": os.environ["TEST_NIX_HASH"]}))
""", encoding="utf-8")
            stub.chmod(0o755)
            calls = pathlib.Path(tmp, "calls")
            calls.touch()
            result = subprocess.run(
                [sys.executable, "-B", str(ROOT / "tools/prefetch-mirrored-inputs")],
                env={**os.environ, "PATH": f"{tmp}:{os.environ['PATH']}",
                     "TEST_NIX_CALLS": str(calls), "TEST_NIX_HASH": reported_hash},
                stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=30,
            )
            return result, [json.loads(line)[-1] for line in calls.read_text().splitlines()]

    def test_every_codeberg_input_is_fetched_from_its_mirror_and_hash_checked(self) -> None:
        nodes = json.loads((ROOT / "flake.lock").read_text(encoding="utf-8"))["nodes"]
        codeberg = {(node["locked"]["url"], node["locked"]["ref"], node["locked"]["rev"],
                     node["locked"]["narHash"])
                    for node in nodes.values() if "codeberg.org" in node.get("locked", {}).get("url", "")}
        self.assertEqual({url for url, *_ in codeberg}, {"https://codeberg.org/HU90m/mdutils.git"})
        (_, ref, revision, nar_hash), = codeberg

        result, fetched = self.run_tool(nar_hash)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(fetched, [f"git+https://github.com/HU90m/mdutils.git?ref={ref}&rev={revision}"])

        result, _ = self.run_tool("sha256-" + "A" * 43 + "=")
        self.assertEqual(result.returncode, 1)
        self.assertIn(f"flake.lock records {nar_hash}", result.stderr)


class ProtectionTranscriptTests(unittest.TestCase):
    def fixture(self):
        classes = ("Tag", "Bounds", "Permit Store", "Permit Store Local Cap")
        simulator = "\n".join(
            f"*** CHERI '{name}' violation occurred *** at time {i}"
            for i, name in enumerate(classes)
        )
        records = ["captest: BEGIN hardware exhaustive=0x0"]
        for balance in (0, 1000, -1000):
            records.append(
                f"captest: wallet ret=-1 valid=1 count=1 mcause=28 mtval=339 before={balance} after={balance}"
            )
        for name, cause in (("tag", 322), ("road-left", 321), ("road-right", 321)):
            records.append(
                f"captest: {name} ret=-1 valid=1 count=1 mcause=28 mtval={cause} before=90 after=90"
            )
        records += [
            "captest: control success-rejected ret=0 record=0 before=90 after=165",
            "captest: control wrong-cause-rejected ret=-1 valid=1 count=1 mcause=28 mtval=322",
            "captest: retention ret=-1 valid=1 count=1 mcause=28 mtval=118 trapped=1",
            "captest: budget road_cycles=1000 stack_bytes=1500",
            "captest: PASS hardware",
            "captest: TOTAL tag=2 bounds=2 store=3 local=1",
            "captest: TOTAL tag=2 bounds=2 store=3 local=1",
        ]
        expected = {"origin": 0, "tag": 0, "bounds": 0, "store": 0, "local": 0}
        return simulator, "\n".join(records), expected

    def test_exact_hardware_fault_reconciliation(self):
        simulator, uart, expected = self.fixture()
        self.assertEqual(protection_transcript.verify(simulator, uart, expected),
                         {"tag": 2, "bounds": 2, "store": 3, "local": 1})
        with self.assertRaises(ValueError):
            protection_transcript.verify(simulator, uart, expected, exhaustive=True)

    def test_unaccounted_or_fabricated_faults_are_rejected(self):
        simulator, uart, expected = self.fixture()
        for output, log, reference in (
            (simulator + "\n*** CHERI 'Tag' violation occurred *** at time 99", uart, expected),
            (simulator.replace("'Tag'", "'Seal'", 1), uart, expected),
            ("\n".join(simulator.splitlines()[1:]), uart, expected),
            ("", uart.replace("tag=2 bounds=2 store=3 local=1",
                              "tag=0 bounds=0 store=0 local=0"), expected),
            (simulator, uart.replace("TOTAL tag=2", "TOTAL tag=1", 1), expected),
            (simulator, uart.replace("mcause=28", "mcause=2", 1), expected),
            (simulator, uart.replace("before=90 after=90", "before=90 after=91", 1), expected),
            (simulator, uart.replace(" before=90 after=90", "", 1), expected),
            (simulator, uart.replace("valid=1", "valid=0", 1), expected),
            (simulator, uart + "\nFAIL: unexpected failure", expected),
            (simulator, uart, {**expected, "origin": 1}),
        ):
            with self.subTest(output=output[-70:], log=log[-70:]):
                with self.assertRaises(ValueError):
                    protection_transcript.verify(output, log, reference)

    def test_tag_stripped_retention_and_gameplay_totals(self):
        simulator, uart, expected = self.fixture()
        simulator = "\n".join(simulator.splitlines()[:-1])
        uart = uart.replace(
            "retention ret=-1 valid=1 count=1 mcause=28 mtval=118 trapped=1",
            "retention ret=0 valid=0 count=0 mcause=0 mtval=0 trapped=0",
        ).replace("local=1", "local=0")
        expected.update(tag=1, bounds=1, store=1)
        uart = uart.rsplit("\n", 1)[0] + "\ncaptest: TOTAL tag=3 bounds=3 store=4 local=0"
        self.assertEqual(protection_transcript.verify(simulator, uart, expected),
                         {"tag": 3, "bounds": 3, "store": 4, "local": 0})


class ProtectionElfTests(unittest.TestCase):
    def test_handler_placement_and_minimum_stack_mutations(self):
        path = ROOT / "build/cheriot/cheriot/release/gta6_wallpaper.json"
        if not path.exists():
            self.skipTest("requires built hardware component ELFs")
        report = json.loads(path.read_text())
        if "cap_probe" not in report["compartments"]:
            self.skipTest("requires protection feature firmware")
        protection_elf.verify(report)
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as tmp:
            copied = copy.deepcopy(report)
            paths = {}
            for name in ("wallpaper", "game_wallet", "cap_probe"):
                source = protection_elf.component_path(report, name)
                target = pathlib.Path(tmp) / f"{name}.compartment"
                shutil.copy2(source, target)
                paths[name] = target
                for entry in copied["compartments"][name]["code"]["inputs"]:
                    entry["file"] = str(target)
            protection_elf.verify(copied)
            data, entries = protection_elf.symbols(paths["cap_probe"])
            missing = bytearray(data)
            missing[entries["compartment_error_handler"]["info_offset"]] &= 0xf0
            paths["cap_probe"].write_bytes(missing)
            with self.assertRaisesRegex(ValueError, "handler missing"):
                protection_elf.verify(copied)
            wrong_stack = bytearray(data)
            entry = entries["__export_cap_probe__Z11probe_storePvjh"]
            wrong_stack[entry["offset"] + 2] = 0
            paths["cap_probe"].write_bytes(wrong_stack)
            with self.assertRaisesRegex(ValueError, "minimum stack"):
                protection_elf.verify(copied)
            paths["cap_probe"].write_bytes(data)
            for entry in copied["compartments"]["game_wallet"]["code"]["inputs"]:
                entry["file"] = str(paths["cap_probe"])
            with self.assertRaisesRegex(ValueError, "must not be installed"):
                protection_elf.verify(copied)


class AuditNegativeMutationTests(unittest.TestCase):
    def test_audit_rejects_pmod0_pinmux_and_extra_thread(self) -> None:
        report_path = ROOT / "build" / "cheriot" / "cheriot" / "release" / "gta6_wallpaper.json"
        sonata_sw = os.environ.get("SONATA_SOFTWARE")
        if not report_path.is_file() or not sonata_sw:
            self.skipTest("requires built firmware report inside Nix shell")

        board_path = pathlib.Path(sonata_sw) / "cheriot-rtos" / "sdk" / "boards" / "sonata-1.1.json"
        policy_path = ROOT / "policy" / "gta6_wallpaper.rego"
        base_report = json.loads(report_path.read_text(encoding="utf-8"))

        mutations = []
        # 1. Inject gpio_pmod0 MMIO import into wallpaper compartment
        m_pmod0 = copy.deepcopy(base_report)
        m_pmod0["compartments"]["wallpaper"]["imports"].append(
            {"kind": "MMIO", "start": 0x800000C0, "length": 0x10, "permissions": "RW"}
        )
        mutations.append(("gpio_pmod0 import", m_pmod0))

        # 2. Inject pinmux_pins_sinks MMIO import into lcd_panel library
        m_pinmux = copy.deepcopy(base_report)
        m_pinmux["compartments"]["lcd_panel"]["imports"].append(
            {"kind": "MMIO", "start": 0x80005000, "length": 0x55, "permissions": "RW"}
        )
        mutations.append(("pinmux import", m_pinmux))

        # 3. Add an unexpected second thread
        m_thread = copy.deepcopy(base_report)
        m_thread["threads"].append(copy.deepcopy(m_thread["threads"][0]))
        mutations.append(("extra thread", m_thread))

        gpio = next(
            imp for imp in base_report["compartments"]["wallpaper"]["imports"]
            if imp.get("kind") == "MMIO" and imp.get("start") == 0x80000000
        )
        self.assertTrue(gpio["permits_store"])
        for flag in ("permits_load", "permits_store", "permits_load_store_capabilities", "permits_load_mutable"):
            mutated = copy.deepcopy(base_report)
            imp = next(
                item for item in mutated["compartments"]["wallpaper"]["imports"]
                if item.get("kind") == "MMIO" and item.get("start") == 0x80000000
            )
            imp[flag] = not gpio[flag]
            mutations.append((f"GPIO {flag}", mutated))

        for label, start, length in (
            ("widened GPIO", 0x80000000, 0x50),
            ("overlapping GPIO", 0x7FFFFFFC, 0x14),
        ):
            mutated = copy.deepcopy(base_report)
            imp = next(
                item for item in mutated["compartments"]["wallpaper"]["imports"]
                if item.get("kind") == "MMIO" and item.get("start") == 0x80000000
            )
            imp.update(start=start, length=length)
            mutations.append((label, mutated))

        mutated = copy.deepcopy(base_report)
        mutated["compartments"]["debug"]["imports"].append(copy.deepcopy(gpio))
        mutations.append(("extra GPIO owner", mutated))

        mutated = copy.deepcopy(base_report)
        writable = copy.deepcopy(gpio)
        writable["permits_store"] = True
        mutated["compartments"]["wallpaper"]["imports"].append(writable)
        mutations.append(("additional writable GPIO alias", mutated))

        rgb = next(
            imp for imp in base_report["compartments"]["wallpaper"]["imports"]
            if imp.get("kind") == "MMIO" and imp.get("start") == 0x80009000
        )
        for flag in ("permits_load", "permits_store", "permits_load_store_capabilities", "permits_load_mutable"):
            mutated = copy.deepcopy(base_report)
            imp = next(
                item for item in mutated["compartments"]["wallpaper"]["imports"]
                if item.get("kind") == "MMIO" and item.get("start") == 0x80009000
            )
            imp[flag] = not rgb[flag]
            mutations.append((f"RGB {flag}", mutated))
        mutated = copy.deepcopy(base_report)
        mutated["compartments"]["debug"]["imports"].append(copy.deepcopy(rgb))
        mutations.append(("extra RGB owner", mutated))
        mutated = copy.deepcopy(base_report)
        imp = next(
            item for item in mutated["compartments"]["wallpaper"]["imports"]
            if item.get("kind") == "MMIO" and item.get("start") == 0x80009000
        )
        imp["length"] += 4
        mutations.append(("widened RGB import", mutated))

        protected_imports = [
            imp for imp in base_report["compartments"]["wallpaper"]["imports"]
            if imp.get("kind") == "CompartmentExport" and
            imp.get("compartment_name") in ("game_wallet", "cap_probe")
        ]
        self.assertEqual(len(protected_imports), 7)
        wallet_call = next(
            imp for imp in protected_imports if imp["function"] == "wallet_credit(long long)"
        )
        mutated = copy.deepcopy(base_report)
        mutated["compartments"]["cap_probe"].setdefault("imports", []).append(
            copy.deepcopy(wallet_call)
        )
        mutations.append(("probe obtains wallet mutation authority", mutated))
        for caller in ("debug", "lcd_panel"):
            for target in ("game_wallet", "cap_probe"):
                mutated = copy.deepcopy(base_report)
                imp = next(item for item in protected_imports if item["compartment_name"] == target)
                mutated["compartments"][caller].setdefault("imports", []).append(copy.deepcopy(imp))
                mutations.append((f"{caller} calls {target}", mutated))
        for target in ("game_wallet", "cap_probe"):
            mutated = copy.deepcopy(base_report)
            mutated["compartments"][target].setdefault("imports", []).append(copy.deepcopy(gpio))
            mutations.append((f"{target} obtains MMIO", mutated))
            mutated = copy.deepcopy(base_report)
            extra = copy.deepcopy(mutated["compartments"][target]["exports"][0])
            extra["export_symbol"] += "_extra"
            mutated["compartments"][target]["exports"].append(extra)
            mutations.append((f"{target} exports extra entry", mutated))
            mutated = copy.deepcopy(base_report)
            mutated["compartments"][target]["exports"][0]["interrupt_status"] = "disabled"
            mutations.append((f"{target} disables entry interrupts", mutated))
            mutated = copy.deepcopy(base_report)
            del mutated["compartments"][target]
            mutations.append((f"missing {target}", mutated))
        for key in ("stack", "trusted_stack"):
            mutated = copy.deepcopy(base_report)
            mutated["threads"][0][key]["length"] += 8
            mutations.append((f"unexpected {key} size", mutated))

        def audit(report, query="data.gta6_wallpaper.valid", board=board_path):
            with tempfile.TemporaryDirectory(dir=ROOT / "build") as tmp:
                report_file = pathlib.Path(tmp) / "report.json"
                report_file.write_text(json.dumps(report), encoding="utf-8")
                return subprocess.run(
                    ["cheriot-audit", "--board", str(board), "--module", str(policy_path),
                     "--query", query, "--firmware-report", str(report_file)],
                    capture_output=True, text=True, check=True,
                ).stdout.strip()

        def rejecting_rules(report, board=board_path):
            return json.loads(audit(report, "data.gta6_wallpaper.failed_rules", board))

        def device_import(device):
            return {"kind": "MMIO", "start": device["start"], "length": device["length"],
                    "permits_load": True, "permits_store": True,
                    "permits_load_store_capabilities": False, "permits_load_mutable": False}

        def imports_of(report, name):
            return report["compartments"][name].setdefault("imports", [])

        # The contract is deny-by-default for the whole image, not only for
        # the components and devices it names.
        devices = json.loads(audit(base_report, "data.board.devices"))
        self.assertLessEqual({"adc", "i2c0", "i2c1", "spi0", "usbdev"}, set(devices))
        for name, device in sorted(devices.items()):
            mutated = copy.deepcopy(base_report)
            imports_of(mutated, "freestanding").append(device_import(device))
            mutations.append((f"freestanding obtains {name}", mutated))
        mutated = copy.deepcopy(base_report)
        imports_of(mutated, "scheduler").append(
            {**device_import(devices["i2c0"]), "start": 0x00110000, "length": 16})
        mutations.append(("scheduler obtains memory outside every device", mutated))

        mutated = copy.deepcopy(base_report)
        spare = copy.deepcopy(base_report["compartments"]["cap_probe"])
        for entry in spare["exports"]:
            entry["export_symbol"] = entry["export_symbol"].replace("cap_probe", "spare_probe")
        mutated["compartments"]["spare_probe"] = spare
        mutations.append(("extra compartment without authority", mutated))
        mutated = copy.deepcopy(base_report)
        audio = copy.deepcopy(base_report["compartments"]["cap_probe"])
        audio["exports"] = audio["exports"][:1]
        audio["exports"][0]["export_symbol"] = "__export_audio__Z10audio_playv"
        audio["imports"] = [device_import(devices["i2c0"])]
        mutated["compartments"]["audio"] = audio
        imports_of(mutated, "wallpaper").append({
            "kind": "CompartmentExport", "compartment_name": "audio",
            "export_symbol": "__export_audio__Z10audio_playv", "function": "audio_play()",
        })
        mutations.append(("extra compartment with a device, called by wallpaper", mutated))

        def find_import(component, **match):
            return next(
                imp for imp in base_report["compartments"][component]["imports"]
                if all(imp.get(key) == value for key, value in match.items())
            )

        lcd_call = find_import(
            "wallpaper", kind="LibraryFunction",
            export_symbol="__library_export_libcalls__ZN13wallpaper_lcd5Panel18draw_rgb565_scaledEPKhjjj")
        for caller, component, match in (
            ("scheduler", "wallpaper", {"export_symbol": lcd_call["export_symbol"]}),
            ("wallpaper", "scheduler", {"kind": "CompartmentExport", "compartment_name": "allocator"}),
            ("wallpaper", "allocator",
             {"export_symbol": "__export.sealing_type.allocator.MallocKey"}),
            ("wallpaper", "compartment_helpers", {"kind": "SharedObject"}),
        ):
            borrowed = find_import(component, **match)
            mutated = copy.deepcopy(base_report)
            imports_of(mutated, caller).append(copy.deepcopy(borrowed))
            label = borrowed.get("export_symbol", borrowed.get("shared_object"))
            mutations.append((f"{caller} imports {label}", mutated))
        mutated = copy.deepcopy(base_report)
        imports_of(mutated, "lcd_panel").append({
            "kind": "CompartmentExport", "compartment_name": "wallpaper",
            "export_symbol": "__export_wallpaper__Z13wallpaper_runv", "function": "wallpaper_run()",
        })
        mutations.append(("lcd_panel calls the wallpaper entry point", mutated))

        # A library runs with its own imports, so library calls are pinned too.
        memcmp = find_import("wallpaper", export_symbol="__library_export_libcalls__Z6memcmpPKvS0_j")
        for target in ("game_wallet", "cap_probe"):
            mutated = copy.deepcopy(base_report)
            imports_of(mutated, target).append(copy.deepcopy(memcmp))
            mutations.append((f"{target} imports a library function", mutated))
        for caller, library in (("wallpaper", "token_library"), ("lcd_panel", "locks"),
                                ("debug", "compartment_helpers")):
            provider = base_report["compartments"][library]
            mutated = copy.deepcopy(base_report)
            imports_of(mutated, caller).append({
                "kind": "LibraryFunction",
                "export_symbol": provider["exports"][0]["export_symbol"],
                "provided_by": provider["code"]["inputs"][0]["file"],
            })
            mutations.append((f"{caller} imports a {library} function", mutated))

        for target in ("wallpaper", "lcd_panel"):
            mutated = copy.deepcopy(base_report)
            extra = copy.deepcopy(mutated["compartments"][target]["exports"][0])
            extra["export_symbol"] += "_extra"
            mutated["compartments"][target]["exports"].append(extra)
            mutations.append((f"{target} exports extra entry", mutated))
            mutated = copy.deepcopy(base_report)
            mutated["compartments"][target]["exports"][0]["interrupt_status"] = "disabled"
            mutations.append((f"{target} disables entry interrupts", mutated))

        for owner, name in (("debug", "uart"), ("lcd_panel", "spi_lcd"), ("lcd_panel", "pwm_lcd")):
            for label, change in (
                ("narrowed", {"length": devices[name]["length"] - 4}),
                ("capability-bearing", {"permits_load_store_capabilities": True}),
            ):
                mutated = copy.deepcopy(base_report)
                imp = next(
                    item for item in mutated["compartments"][owner]["imports"]
                    if item.get("kind") == "MMIO" and item.get("start") == devices[name]["start"]
                )
                imp.update(change)
                mutations.append((f"{label} {name} grant", mutated))

        # Each rule must also reject on its own, so that none is dead weight.
        sole_rule = {
            "extra compartment without authority": "image_components_exact",
            "freestanding obtains i2c0": "image_mmio_exact",
            "narrowed uart grant": "app_mmio_permissions_exact",
            "wallpaper imports allocator_epoch": "app_import_kinds_exact",
            "game_wallet imports a library function": "isolated_components_have_no_imports",
            "cap_probe imports a library function": "isolated_components_have_no_imports",
            "wallpaper imports a token_library function": "app_library_calls_exact",
            "lcd_panel imports a locks function": "app_library_calls_exact",
            "debug imports a compartment_helpers function": "app_library_calls_exact",
            "lcd_panel calls the wallpaper entry point": "app_calls_exact",
            "wallpaper exports extra entry": "app_exports_exact",
            "extra thread": "single_wallpaper_thread",
        }
        singled_out = [(label, mutated) for label, mutated in mutations if label in sole_rule]
        self.assertEqual(len(singled_out), len(sole_rule))

        self.assertEqual(audit(base_report), "true")
        self.assertEqual(rejecting_rules(base_report), [])
        with concurrent.futures.ThreadPoolExecutor() as pool:
            verdicts = pool.map(audit, (mutated for _, mutated in mutations))
            for (label, _), verdict in zip(mutations, verdicts):
                self.assertEqual(verdict, "false", f"mutation '{label}' was not rejected")
            reasons = pool.map(rejecting_rules, (mutated for _, mutated in singled_out))
            for (label, _), reason in zip(singled_out, reasons):
                self.assertEqual(reason, [sole_rule[label]], label)

        # The named denial must not become vacuous if a board device is renamed.
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as tmp:
            renamed = pathlib.Path(tmp) / "board.json"
            renamed.write_text(
                board_path.read_text(encoding="utf-8").replace('"gpio_pmod0"', '"gpio_renamed"'),
                encoding="utf-8",
            )
            self.assertEqual(rejecting_rules(base_report, renamed), ["forbidden_devices_unreachable"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
