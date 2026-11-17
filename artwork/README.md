# Artwork attribution and license

The **Grand Time Auto project owner** supplies and licenses both splash images
below under
**Creative Commons Attribution 4.0 International (CC BY 4.0)**.

| Selection | Source image | Dimensions | Attribution |
| --- | --- | --- | --- |
| `primary` / original (default) | `artwork/gta6-splash-orig-lcd.png` | 512×640 RGB | Supplied and licensed by the project owner |
| `alternate` / derivative | `artwork/gta6-splash-derivative-lcd.png` | 1024×1536 opaque RGBA | Supplied and licensed by the project owner |

The owner confirmed this licensing for v1.0.0 on November 17, 2026. No
additional creator or model provenance is asserted. This records the owner's
licensing statement, **not an independent rights audit**.

License: https://creativecommons.org/licenses/by/4.0/

Full license: https://creativecommons.org/licenses/by/4.0/legalcode.en

This notice covers the two named splash PNGs, their generated
`firmware/wallpaper_image.h` / `firmware/wallpaper_image_alternate.h` splash
data, and that data embedded in firmware, browser builds and rendered splash
previews. It does not relicense program code, third-party dependencies or
custom images supplied to the converter.

Suggested credit:

> Grand Time Auto splash artwork, supplied by the Grand Time Auto project owner.
> CC BY 4.0. Original grid recovered with nearest-neighbor sampling; derivative
> provenance metadata removed, center-cropped and Lanczos-resized; both encoded
> for an RGB565 LCD.

Retain appropriate credit, the license link and an indication of modifications
when sharing the artwork or adaptations. No endorsement is implied.

## Public PNG preservation

Both public splash files retain their supplied dimensions and decoded RGB/RGBA
pixels. The original PNG is byte-identical, including its sRGB color-space
information. Only the derivative's nonvisual `caBX` provenance container was
removed for privacy; its image-bearing chunks are unchanged. Neither supplied
original was modified.

Public PNG SHA-256 values:

- Primary:
  `59ea4157ce494572e210a20e8a82f4e9baf8fd7ae72ec820b0f4c69d57c24017`
- Alternate:
  `ec889f4d33951b156c3f61f931663553b574b2e55b3f02df4976c811add95f8a`

The supplied derivative's SHA-256 before metadata removal was
`c0598756b5d336994e45806819074bea43bf3529cf34c15f5523266ac6d2b67f`.
Metadata removal does not authenticate provenance or constitute a rights audit.

## Display derivatives

`tools/convert-wallpaper` defaults to `primary` with the `native` style. The
512×640 original is an exact 4× nearest-neighbor enlargement of a 128×160 image.
Nearest-neighbor sampling recovers that grid exactly, with no crop or
pre-rotation, before RGB565 quantization.

The 1024×1536 derivative is not native 128×160 artwork. Its default `artwork`
style center-crops to the panel's 4:5 ratio: 128 source rows are removed from
both top and bottom, leaving 1024×1280. Lanczos then resizes it to 128×160.
Custom inputs also default to the smooth `artwork` style.

Both paths emit the LCD's required little-endian RGB565 format. They retain
the supplied lettering without an extra coarse grid, restricted palette,
replacement lettering or dithering. `native`, `artwork`, `retro`, `pixel-art`
and `photo` can be selected explicitly; the latter modes are opt-in.
These transformations apply to generated display data, not the public source
PNG pixels.

Reproduce and verify both headers:

```sh
./tools/convert-wallpaper --artwork primary
./tools/convert-wallpaper --artwork alternate
./tools/convert-wallpaper --artwork primary --check
./tools/convert-wallpaper --artwork alternate --check
```

The browser build copies this notice to `artwork-license.txt`; hardware
firmware builds place it beside the image as `gta6_wallpaper.artwork-license.txt`.
Only the designated project input paths receive the generated CC-BY-4.0
header notice. A custom file, even one with the same basename elsewhere, is
not automatically relicensed.
