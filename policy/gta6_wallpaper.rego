# SPDX-License-Identifier: MIT
#
# Audit policy for gta6_wallpaper (application slot 2).
#
# The contract is deny-by-default. It extends the CHERIoT RTOS base policy
# (data.rtos.valid) and is evaluated by cheriot-audit. A valid image has
# exactly the components and board-device grants listed here, exactly the
# listed imports in application components, exactly the listed exports of and
# calls into the components this project owns, and one application thread.
# It can drive only the LCD (via lcd_panel), the uart0 console (via debug)
# and the onboard joystick, user LEDs and RGB status LEDs (via wallpaper).
# Another compartment or board-device grant anywhere in the image, or another
# import by an application component, fails the audit until this file changes.
#
# Not pinned here: the imports RTOS components make among themselves and the
# bounds of the allocator's heap. They come from the pinned SDK. The RTOS base
# policy checks only allocator quota objects and which components hold the
# revoker and interrupt controllers.
#
# Sets are combined with comprehensions and multi-body rules only: the pinned
# cheriot-audit rejects or mis-evaluates some uses of the |, & and - operators.

package gta6_wallpaper

import future.keywords

# Every compartment and library linked into the image.
image_components := {
	"allocator",
	"atomic1",
	"atomic4",
	"cap_probe",
	"compartment_helpers",
	"compartment_switcher",
	"crt",
	"debug",
	"freestanding",
	"game_wallet",
	"lcd_panel",
	"locks",
	"scheduler",
	"token_library",
	"wallpaper",
}

# Components chosen by this application that hold authority. Every import
# they make is listed below.
app_components := {"wallpaper", "lcd_panel", "debug"}

# Components that must hold no authority at all: no imports of any kind.
isolated_components := {"game_wallet", "cap_probe"}

# Every [component, board device] MMIO grant in the image.
image_mmio := {
	["allocator", "revoker"],
	["allocator", "shadow"],
	["scheduler", "clint"],
	["scheduler", "plic"],
	["debug", "uart"],
	["lcd_panel", "spi_lcd"],
	["lcd_panel", "pwm_lcd"],
	["wallpaper", "gpio_board"],
	["wallpaper", "rgbled"],
}

# Application grants. Each covers exactly its device with load/store only.
app_mmio := {
	["debug", "uart"],
	["lcd_panel", "spi_lcd"],
	["lcd_panel", "pwm_lcd"],
	["wallpaper", "gpio_board"],
	["wallpaper", "rgbled"],
}

# Named explicitly so that a renamed board device cannot make the denial
# vacuous. The image-wide grant list above already excludes them.
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

# Exact exports of the components this project owns.
app_exports := {
	"wallpaper": {"__export_wallpaper__Z13wallpaper_runv"},
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
	"lcd_panel": {
		"__library_export_libcalls__ZN13wallpaper_lcd8internal10panel_initEP16LCD_Interface_stP15stSt7735Context",
		"__library_export_libcalls__ZN13wallpaper_lcd5Panel18draw_rgb565_scaledEPKhjjj",
		"__library_export_libcalls__ZN13wallpaper_lcd5Panel18touch_pixel_rgb565EjjPKh",
	},
}

# Compartment entry points run with interrupts enabled; library functions
# inherit the caller's state.
app_export_interrupts := {
	"wallpaper": "enabled",
	"game_wallet": "enabled",
	"cap_probe": "enabled",
	"lcd_panel": "inherit",
}

# Every compartment call made by an application component and every call,
# from anywhere in the image, into a component this project owns:
# [caller, callee, export symbol].
app_calls_allowed := {
	["wallpaper", "scheduler", "__export_scheduler__Z12thread_sleepP7Timeoutj"],
	["lcd_panel", "scheduler", "__export_scheduler__Z12thread_sleepP7Timeoutj"],
	["wallpaper", "game_wallet", "__export_game_wallet__Z12wallet_resetv"],
	["wallpaper", "game_wallet", "__export_game_wallet__Z13wallet_creditx"],
	["wallpaper", "game_wallet", "__export_game_wallet__Z12wallet_debitx"],
	["wallpaper", "game_wallet", "__export_game_wallet__Z11wallet_viewv"],
	["wallpaper", "cap_probe", "__export_cap_probe__Z11probe_storePvjh"],
	["wallpaper", "cap_probe", "__export_cap_probe__Z16probe_fault_takev"],
	["wallpaper", "cap_probe", "__export_cap_probe__Z12probe_retainPv"],
	["wallpaper", "lcd_panel", "__library_export_libcalls__ZN13wallpaper_lcd8internal10panel_initEP16LCD_Interface_stP15stSt7735Context"],
	["wallpaper", "lcd_panel", "__library_export_libcalls__ZN13wallpaper_lcd5Panel18draw_rgb565_scaledEPKhjjj"],
}

# Every shared-library function an application component calls:
# [caller, export symbol]. A library runs with its own imports, so a new
# library call can widen what application code reaches.
app_library_calls_allowed := {
	["wallpaper", "__library_export_libcalls___divdi3"],
	["wallpaper", "__library_export_libcalls___udivdi3"],
	["wallpaper", "__library_export_libcalls__Z6memcmpPKvS0_j"],
	["wallpaper", "__library_export_libcalls__Z23debug_log_message_writePKcS0_P19DebugFormatArgumentj"],
	["wallpaper", "__library_export_libcalls__Z25stack_lowest_used_addressv"],
	["wallpaper", "__library_export_libcalls__ZN13wallpaper_lcd8internal10panel_initEP16LCD_Interface_stP15stSt7735Context"],
	["wallpaper", "__library_export_libcalls__ZN13wallpaper_lcd5Panel18draw_rgb565_scaledEPKhjjj"],
	["lcd_panel", "__library_export_libcalls__Z6memcmpPKvS0_j"],
	["lcd_panel", "__library_export_libcalls__Z23debug_log_message_writePKcS0_P19DebugFormatArgumentj"],
	["debug", "__library_export_libcalls___divdi3"],
}

# Application components may import devices, shared code and compartment
# entry points, never shared or sealed objects.
app_import_kinds := {"MMIO", "LibraryFunction", "CompartmentExport"}

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

default image_components_exact := false

image_components_exact if {
	{name | input.compartments[name]} == image_components
}

default image_mmio_exact := false

image_mmio_exact if {
	grants := {[c, name] |
		imp := input.compartments[c].imports[_]
		mmio_overlaps(imp, data.board.devices[name])
	}
	grants == image_mmio

	# Memory outside every board device is the heap and its bookkeeping. Only
	# the allocator may hold it. Its bounds come from the pinned SDK's linker
	# script and are not pinned here.
	holders := {c |
		imp := input.compartments[c].imports[_]
		imp.kind == "MMIO"
		not mmio_overlaps_any_board_device(imp)
	}
	holders == {"allocator"}
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

default app_mmio_permissions_exact := false

app_mmio_permissions_exact if {
	every grant in app_mmio {
		exact_app_import(grant[0], grant[1])
	}
}

exact_app_import(owner, name) if {
	device := data.board.devices[name]
	imports := [[c, imp] |
		imp := input.compartments[c].imports[_]
		mmio_overlaps(imp, device)
	]
	count(imports) == 1
	every entry in imports {
		entry[0] == owner
		imp := entry[1]
		imp.start == device.start
		imp.length == device.length
		imp.permits_load == true
		imp.permits_store == true
		imp.permits_load_store_capabilities == false
		imp.permits_load_mutable == false
	}
}

default app_import_kinds_exact := false

app_import_kinds_exact if {
	others := {[c, imp.kind] |
		c := app_components[_]
		imp := input.compartments[c].imports[_]
		not app_import_kinds[imp.kind]
	}
	others == set()
}

default single_wallpaper_thread := false

single_wallpaper_thread if {
	count(input.threads) == 1
	input.threads[0].entry_point.compartment_name == "wallpaper"
	input.threads[0].stack.length == 4096
	input.threads[0].trusted_stack.length == 264
}

default app_exports_exact := false

app_exports_exact if {
	every name, symbols in app_exports {
		component := input.compartments[name]
		count(component.exports) == count(symbols)
		actual := {entry.export_symbol | entry := component.exports[_]}
		actual == symbols
		every entry in component.exports {
			entry.kind == "Function"
			entry.interrupt_status == app_export_interrupts[name]
		}
	}
}

no_component_imports(component) if {
	object.get(component, "imports", null) == null
}

no_component_imports(component) if {
	component.imports == []
}

default isolated_components_have_no_imports := false

isolated_components_have_no_imports if {
	every name in isolated_components {
		no_component_imports(input.compartments[name])
	}
}

default app_library_calls_exact := false

app_library_calls_exact if {
	calls := {[caller, imp.export_symbol] |
		caller := app_components[_]
		imp := input.compartments[caller].imports[_]
		imp.kind == "LibraryFunction"
	}
	calls == app_library_calls_allowed
}

app_symbol_owner := {symbol: owner |
	symbols := app_exports[owner]
	symbol := symbols[_]
}

# Compartment calls made by application components, including imports that
# name no compartment such as sealing types.
app_calls contains [caller, object.get(imp, "compartment_name", ""), imp.export_symbol] if {
	caller := app_components[_]
	imp := input.compartments[caller].imports[_]
	imp.kind == "CompartmentExport"
}

# Calls from anywhere that name a component this project owns.
app_calls contains [caller, imp.compartment_name, imp.export_symbol] if {
	imp := input.compartments[caller].imports[_]
	imp.kind == "CompartmentExport"
	app_exports[imp.compartment_name]
}

# Calls from anywhere to a symbol this project exports, which also covers
# library functions.
app_calls contains [caller, owner, imp.export_symbol] if {
	imp := input.compartments[caller].imports[_]
	owner := app_symbol_owner[imp.export_symbol]
}

default app_calls_exact := false

app_calls_exact if {
	app_calls == app_calls_allowed
}

default valid := false

valid if {
	data.rtos.valid
	image_components_exact
	image_mmio_exact
	forbidden_devices_unreachable
	app_mmio_permissions_exact
	app_import_kinds_exact
	isolated_components_have_no_imports
	app_library_calls_exact
	app_calls_exact
	app_exports_exact
	single_wallpaper_thread
}

# Diagnostics for tools/check: the rules that reject an image.
failed_rules contains "data.rtos.valid" if not data.rtos.valid

failed_rules contains "image_components_exact" if not image_components_exact

failed_rules contains "image_mmio_exact" if not image_mmio_exact

failed_rules contains "forbidden_devices_unreachable" if not forbidden_devices_unreachable

failed_rules contains "app_mmio_permissions_exact" if not app_mmio_permissions_exact

failed_rules contains "app_import_kinds_exact" if not app_import_kinds_exact

failed_rules contains "isolated_components_have_no_imports" if not isolated_components_have_no_imports

failed_rules contains "app_library_calls_exact" if not app_library_calls_exact

failed_rules contains "app_calls_exact" if not app_calls_exact

failed_rules contains "app_exports_exact" if not app_exports_exact

failed_rules contains "single_wallpaper_thread" if not single_wallpaper_thread
