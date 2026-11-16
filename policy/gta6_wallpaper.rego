# SPDX-License-Identifier: MIT
#
# Audit policy for gta6_wallpaper (application slot 2).
# Ensures the wallpaper image can drive only the LCD (via lcd) and uart0
# console (via debug), plus onboard joystick, user LEDs and RGB status LEDs.
# External GPIO, pinmux and secondary UARTs remain inaccessible.

package gta6_wallpaper

import future.keywords

app_components := {"wallpaper", "lcd_panel", "debug", "game_wallet", "cap_probe"}
owned_devices := {"spi_lcd", "pwm_lcd", "uart", "gpio_board", "rgbled"}
wallpaper_caller := {"wallpaper"}

forbidden_devices := {
	"gpio_pmod0",
	"gpio_pmod1",
	"gpio_pmodc",
	"gpio_rpi",
	"gpio_arduino",
	"pinmux_pins_sinks",
	"pinmux_block_sinks",
	"uart1",
	"uart2",
}

mmio_overlaps(imp, device) if {
	imp.kind == "MMIO"
	imp.start < device.start + device.length
	device.start < imp.start + imp.length
}

mmio_overlaps_any_board_device(imp) if {
	device := data.board.devices[_]
	mmio_overlaps(imp, device)
}

device_importers(name) := {c |
	imp := input.compartments[c].imports[_]
	mmio_overlaps(imp, data.board.devices[name])
}

default device_owners_exact := false

device_owners_exact if {
	owners := {[c, name] |
		name := owned_devices[_]
		imp := input.compartments[c].imports[_]
		mmio_overlaps(imp, data.board.devices[name])
	}
	owners == {
		["lcd_panel", "spi_lcd"],
		["lcd_panel", "pwm_lcd"],
		["debug", "uart"],
		["wallpaper", "gpio_board"],
		["wallpaper", "rgbled"],
	}
}

default only_wallpaper_uses_lcd := false

only_wallpaper_uses_lcd if {
	callers := {c | c := data.compartment.compartments_calling("lcd_panel")[_]}
	callers == wallpaper_caller
}

default forbidden_devices_unreachable := false

forbidden_devices_unreachable if {
	every name in forbidden_devices {
		data.board.devices[name]
	}
	importers := {c |
		name := forbidden_devices[_]
		c := device_importers(name)[_]
	}
	importers == set()
}

default app_mmio_exact := false

app_mmio_exact if {
	pairs := {[c, name] |
		c := app_components[_]
		imp := input.compartments[c].imports[_]
		mmio_overlaps(imp, data.board.devices[name])
	}
	pairs == {
		["lcd_panel", "spi_lcd"],
		["lcd_panel", "pwm_lcd"],
		["debug", "uart"],
		["wallpaper", "gpio_board"],
		["wallpaper", "rgbled"],
	}
	unknown := {[c, i] |
		c := app_components[_]
		imp := input.compartments[c].imports[i]
		imp.kind == "MMIO"
		not mmio_overlaps_any_board_device(imp)
	}
	unknown == set()
}

default board_io_permissions_exact := false

board_io_permissions_exact if {
	exact_board_io_import("gpio_board")
	exact_board_io_import("rgbled")
}

exact_board_io_import(name) if {
	device := data.board.devices[name]
	imports := [[c, imp] |
		imp := input.compartments[c].imports[_]
		mmio_overlaps(imp, device)
	]
	count(imports) == 1
	every entry in imports {
		entry[0] == "wallpaper"
		imp := entry[1]
		imp.start == device.start
		imp.length == device.length
		imp.permits_load == true
		imp.permits_store == true
		imp.permits_load_store_capabilities == false
		imp.permits_load_mutable == false
	}
}

default single_wallpaper_thread := false

single_wallpaper_thread if {
	count(input.threads) == 1
	input.threads[0].entry_point.compartment_name == "wallpaper"
	input.threads[0].stack.length == 4096
	input.threads[0].trusted_stack.length == 264
}

protected_exports := {
	"game_wallet": {
		"__export_game_wallet__Z12wallet_resetv",
		"__export_game_wallet__Z13wallet_creditx",
		"__export_game_wallet__Z12wallet_debitx",
		"__export_game_wallet__Z11wallet_viewv",
	},
	"cap_probe": {
		"__export_cap_probe__Z11probe_storePvjh",
		"__export_cap_probe__Z16probe_fault_takev",
		"__export_cap_probe__Z12probe_retainPv",
	},
}

default protected_components_exact := false

no_component_imports(component) if {
	object.get(component, "imports", null) == null
}

no_component_imports(component) if {
	component.imports == []
}

protected_components_exact if {
	every name, symbols in protected_exports {
		component := input.compartments[name]
		no_component_imports(component)
		count(component.exports) == count(symbols)
		actual := {entry.export_symbol | entry := component.exports[_]}
		actual == symbols
		every entry in component.exports {
			entry.kind == "Function"
			entry.interrupt_status == "enabled"
		}
	}
	expected := {["wallpaper", target, symbol] |
		symbol := protected_exports[target][_]
	}
	actual := {[name, imp.compartment_name, imp.export_symbol] |
		component := input.compartments[name]
		imp := component.imports[_]
		imp.kind == "CompartmentExport"
		protected_exports[imp.compartment_name]
	}
	actual == expected
}

default valid := false

valid if {
	data.rtos.valid
	device_owners_exact
	only_wallpaper_uses_lcd
	forbidden_devices_unreachable
	app_mmio_exact
	board_io_permissions_exact
	single_wallpaper_thread
	protected_components_exact
}
