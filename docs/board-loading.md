# Explicit, attended board loading

`tools/load-slot2` is a Linux operator tool for Sonata application **slot 2**
only. It never writes application slots 1 or 3 or FPGA bitstreams.

**Every UF2 write stops and reloads the FPGA. Disconnect Pmod0 from external
hardware before loading. Do not interrupt a copy.** Keep an existing serial
console owner in control; the loader never opens a serial port.

Build and validate first with `./tools/check`. For each physical board,
independently identify its FTDI USB device and RP2040 programming disk using
the operator's device inventory and read-only `udevadm` inspection. Configure:

| Variable | Required value |
| --- | --- |
| `SONATA_BOARD_SERIAL` | The approved FTDI `ID_SERIAL_SHORT` |
| `SONATA_UF2_SERIAL` | The approved programmer `ID_SERIAL_SHORT` |
| `SONATA_FTDI_DEVICE` | Absolute FTDI USB device path or verified alias, not a serial TTY |
| `SONATA_UF2_DEVICE` | Absolute programming disk path or verified alias |
| `SONATA_CONSOLE_LOG` | Optional absolute path to the existing operator-owned console log |
| `SONATA_EXPECT_UF2_SHA256` | Approved UF2 digest: 64 lowercase hexadecimal digits. Required with `--yes-pmod0-disconnected`, optional for an attended load |

No serial numbers, port-specific aliases or operator home paths are bundled.
Do not blindly trust the first disk with a matching filesystem label. Changing
device paths does not waive identity checks. Recheck ephemeral device paths
after reconnecting.

```sh
./tools/load-slot2 preflight
```

`preflight` is read-only. Missing configuration fails closed. It verifies
FTDI vendor/product/serial, programmer vendor/product/serial, the SONATA label,
FAT filesystem type, disk type, common USB hub topology, unmounted status,
unfinished-load latch, slot-2 UF2 format, stripped ELF payload and any supplied
approval hash.

With the required values exported, Pmod0 disconnected and an attended
terminal:

```sh
./tools/load-slot2 load
```

The operator confirms the displayed digest. For an explicitly preapproved
automated load, supply `SONATA_EXPECT_UF2_SHA256` and
`--yes-pmod0-disconnected`; the flag only acknowledges disconnection and
skips the terminal question. It does not bypass validation, and it is refused
without a well-formed digest, so an unattended load always names the exact
approved image.

Loading creates an owner-only, randomly named snapshot under the repository's
`build` directory. Only the validated, approved snapshot bytes are copied,
even if a concurrent build replaces its output. The inner load repeats board,
mount, slot and hash checks after the confirmation/inhibitor boundary.
Python handling of reports, snapshot creation and UF2 validation uses isolated
mode.

Mounts use `nosuid,nodev,noexec,nosymfollow`. Once mounted, the volume must be
FAT and the destination name must be absent or a regular file. A link,
directory or any other entry is refused and the drive is unmounted unwritten,
so a substituted volume cannot redirect the copy to another file. The genuine
drive is a RAM disk that still lists the file from an earlier load until the
board is power-cycled. That stale entry is removed and the new file is created
exclusively, without following links. `nosymfollow` needs Linux 5.10 or later
and a udisks2 configuration that permits it; otherwise the mount fails and
nothing is written.

The unfinished-write latch remains if copying or unmounting fails. A
subsequent load refuses to proceed: power-cycle and inspect the board and
console before the operator removes the latch. Snapshots are removed on exit;
a failed write is never silently treated as complete.

With `SONATA_CONSOLE_LOG` set, the loader shows the console output that
arrived after the write. The firmware in the selected slot chooses those
bytes, so they are treated as untrusted: colour codes and carriage returns are
removed, every other control or non-ASCII byte is shown in visible form such
as `^[`, and at most 64 KiB is displayed. Read the log file for the rest.

Select application slot 2 with SW7 and use SW5 for processor reset as needed.
Selecting another slot returns to its existing firmware.

`tests/test_tools.py` tests the loader entirely with synthetic identities and
mock device tools. Those tests do not operate physical hardware.
