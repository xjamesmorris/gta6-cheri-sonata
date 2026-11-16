-- SPDX-License-Identifier: MIT
--
-- Sonata v1.3 wallpaper firmware targeting application slot 2 ONLY.
-- Other application slots and FPGA bitstreams are not modified.

set_project("gta6-cheri-sonata")

sonata_software = os.getenv("SONATA_SOFTWARE")
sdkdir = path.join(sonata_software, "cheriot-rtos/sdk")
includes(sdkdir)
set_toolchains("cheriot-clang")

includes(path.join(sdkdir, "lib"))

option("board")
    set_default("sonata-1.1")
option_end()

option("artwork")
    set_default("alternate")
    set_values("primary", "alternate")
option_end()

option("game-scene")
    set_default("none")
    set_values("none", "wallpaper", "prompt", "driving", "wanted", "escaping", "recovery", "wasted", "restart", "money-bag", "hospital", "billing", "billing-in", "billing-out", "police", "jackpot", "gold", "fleet", "traffic", "crossing", "billboard", "police-station", "gas-station", "police-crash", "cheri-selftest", "wasted-fade", "wasted-monochrome", "source-qr")
option_end()

-- Emit a UF2 image ONLY for application slot 2 (base 0x10000000, family
-- 0x6CE29E60).  Never emit slot 1 or slot 3 UF2 files.
local artwork_notice = path.join(os.scriptdir(), "artwork", "README.md")
function convert_to_slot2_uf2(target)
    import("core.project.config")
    if config.get("board") == "sonata-simulator" then
        return
    end
    local firmware = target:targetfile()
    os.execv("llvm-strip", {firmware, "-o", firmware .. ".strip"})
    os.execv("uf2conv", {firmware .. ".strip", "-b0x10000000", "-f0x6CE29E60", "-co", firmware .. ".slot2.uf2"})
    os.cp(artwork_notice, firmware .. ".artwork-license.txt")
end

local display_drivers = path.join(sonata_software, "third_party/display_drivers/src")

compartment("game_wallet")
    set_warnings("allextra")
    add_defines("CHERIOT_NO_AMBIENT_MALLOC")
    add_files("firmware/game_wallet.cc")

compartment("cap_probe")
    set_warnings("allextra")
    add_defines("CHERIOT_NO_AMBIENT_MALLOC")
    add_files("firmware/cap_probe.cc")

-- Custom minimal ST7735 + backlight library: holds spi_lcd and pwm_lcd, keeps
-- backlight off until GRAM is cleared to black, streams full-row 20 MHz SPI
-- bursts for smooth fade-in, and omits unused bitmap font tables.
library("lcd_panel")
    set_default(false)
    add_cxxflags("-Wall", "-Wextra")
    add_includedirs(display_drivers, {public = true})
    add_files(path.join(display_drivers, "core/lcd_base.c"))
    add_files(path.join(display_drivers, "st7735/lcd_st7735.c"))
    add_files("firmware/lcd_panel.cc")

compartment("wallpaper")
    set_warnings("allextra")
    add_defines("CHERIOT_NO_AMBIENT_MALLOC")
    add_deps("lcd_panel", "debug", "game_wallet", "cap_probe")
    add_files("firmware/wallpaper.cc")
    add_files("firmware/game.cc", "firmware/game_renderer.cc", "firmware/protection_cheriot.cc", "firmware/protection_selftest.cc")
    on_load(function(target)
        import("core.project.config")
        local artwork = config.get("artwork") or "alternate"
        assert(artwork == "primary" or artwork == "alternate",
               "artwork must be primary or alternate")
        target:add("defines", "DRIVING_ARTWORK_" .. artwork:upper())
        local scene = config.get("game-scene") or "none"
        if scene ~= "none" then
            assert(config.get("board") == "sonata-simulator",
                   "game-scene fixtures require --board=sonata-simulator")
            local scenes = {"wallpaper", "prompt", "driving", "wanted", "escaping", "recovery", "wasted", "restart", "money-bag", "hospital", "billing", "billing-in", "billing-out", "police", "jackpot", "gold", "fleet", "traffic", "crossing", "billboard", "police-station", "gas-station", "police-crash", "cheri-selftest", "wasted-fade", "wasted-monochrome", "source-qr"}
            for index, name in ipairs(scenes) do
                if name == scene then
                    target:add("defines", "DRIVING_SIM_SCENE=" .. index)
                end
            end
            target:add("includedirs", "firmware")
            target:add("files", "tests/sim_scenarios.cc")
        end
    end)

firmware("gta6_wallpaper")
    add_deps("freestanding", "debug", "wallpaper")
    on_load(function(target)
        import("core.project.config")
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "wallpaper",
                priority = 1,
                entry_point = "wallpaper_run",
                stack_size = 0x1000,
                -- wallpaper -> scheduler (for sleeps) is 2 frames; 1 spare = 3.
                trusted_stack_frames = 3
            }
        }, {expand = false})
        -- Copy read-only Nix-store SDK linker templates to writable files so
        -- repeated builds do not fail trying to overwrite 0444 files.
        local copies = {}
        for _, template in ipairs(table.wrap(target:get("configfiles"))) do
            local copy = path.join(config.buildir(), "sdk-templates", path.filename(template))
            local text = io.readfile(template)
            if not os.isfile(copy) or io.readfile(copy) ~= text then
                io.writefile(copy, text)
            end
            table.insert(copies, {copy, target:extraconf("configfiles", template)})
        end
        target:set("configfiles", nil)
        for _, copy in ipairs(copies) do
            target:add("configfiles", copy[1], copy[2])
        end
    end)
    after_link(convert_to_slot2_uf2)
