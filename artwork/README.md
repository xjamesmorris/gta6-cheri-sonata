# Artwork attribution and license

The **Grand Time Auto project** supplies and licenses both images below under
**Creative Commons Attribution 4.0 International (CC BY 4.0)**.

| Selection | Source image | Dimensions | Owner-supplied provenance |
| --- | --- | --- | --- |
| `primary` | `artwork/gta6-8bit-01.png` | 941×1672 | Supplied and licensed by the project owner |
| `alternate` (newer, default) | `artwork/gta6-alternate-01.png` | 750×956 | Identified by the owner as independent and **generated using Claude** |

Only the alternate has the Claude-generation credit. These statements record
owner-supplied provenance and licensing, **not an independent rights audit**.

License: https://creativecommons.org/licenses/by/4.0/

Full license: https://creativecommons.org/licenses/by/4.0/legalcode.en

This notice covers both PNGs, their generated
`firmware/wallpaper_image.h` / `firmware/wallpaper_image_alternate.h` splash
data, and that data embedded in firmware, browser builds and rendered splash
previews. It does not relicense program code, third-party dependencies or
custom images supplied to the converter.

Suggested credit:

> Grand Time Auto artwork, supplied by the Grand Time Auto project. CC BY 4.0.
> Alternate image generated using Claude (owner-supplied provenance).
> Metadata removed; display derivatives cropped and smoothly resized for RGB565.

Retain appropriate credit, the license link and an indication of modifications
when sharing the artwork or adaptations. No endorsement is implied.

## Metadata-free public PNGs

Both public files retain their supplied dimensions and decoded RGB pixels.
Ancillary PNG metadata, including provenance metadata and opaque identifiers,
was removed for privacy; only image-bearing PNG chunks remain. Neither private
original was modified.

Public PNG SHA-256 values:

- Primary:
  `b9be1fa357f6943a6e5cfceb01d9ae3848ee09c5fc68252d8e7a0644f40a12a4`
- Alternate:
  `1d4569e1f2f7a0aa7547a19f91ded79bb592bce319e2eafb403809801fe51600`

Metadata removal does not replace the provenance disclosure above and does
not constitute provenance authentication or a rights audit.

## Display derivatives

`tools/convert-wallpaper` center-crops the full-resolution source to the
panel's 4:5 portrait ratio, smoothly resizes directly to 128×160 using Lanczos,
and emits the LCD's required little-endian RGB565 format. It retains the
supplied lettering and colors without an intermediate coarse grid, restricted
palette, replacement lettering or dithering.
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
