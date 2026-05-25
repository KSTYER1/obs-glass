# OBS Forum Submission Draft: Glass

## Resource Title

Glass

## Version

1.0.1

## Category

OBS Studio Plugins

## Tags

glass, glassmorphism, liquid glass, blur, frost, lens, distortion, glow,
shadow, overlay, source, lucidglass

## Short Tagline

Placeable LucidGlass overlay source for OBS with blur, frost, distortion,
glow, and shadow.

## Supported Bit Versions

64-bit

## Supported Platforms

Windows

## Minimum OBS Studio Version

30.0.0

## Source Code URL

https://github.com/KSTYER1/obs-glass

## Download URL

https://github.com/KSTYER1/obs-glass/releases/tag/v1.0.1

## Overview

Glass is an unofficial third-party native OBS Studio source. It draws a freely
placeable LucidGlass-style overlay over any chosen background source: a
rounded rectangular shape that blurs, frosts, tints, distorts, and optionally
glows or casts a shadow over what sits behind it on the canvas.

The plugin is the source-plugin port of the LucidGlass shader by Andi Stone
(Andilippi). Unlike a filter, it lives as its own scene item so it can be
moved, resized, and layered at any position in the scene's z-order.

The background source is picked explicitly in the source properties, with a
searchable dropdown of every scene and source in the project.

## Features

- Rounded rectangular glass shape with configurable width, height, corner
  radius, and edge feathering.
- Multi-level background blur with adjustable intensity.
- Frosted glass effect with adjustable strength.
- Color tint with adjustable strength and alpha.
- Lens distortion with edge thickness, falloff, and magnification.
- Chromatic aberration with strength, thickness, and falloff.
- Outer glow with optional bi-directional shaping (angle, spread, softness).
- Inner glow with optional bi-directional shaping (angle, spread, softness).
- Drop shadow with offset, blur, opacity, and color.
- Transparent-background mode so only the glass shape contributes to the
  canvas.
- Searchable background-source picker with alphabetical scene and source list.
- German and English locales.

## Installation

Download the Windows x64 release. Two options:

1. Run `obs-glass-1.0.1-installer.exe`. By default it installs into your OBS
   user plugin folder.
2. Or extract `obs-glass-1.0.1-portable.zip` into your OBS Studio installation
   folder.

The final layout should include:

```text
obs-plugins/64bit/obs-glass.dll
data/obs-plugins/obs-glass/locale/en-US.ini
data/obs-plugins/obs-glass/locale/de-DE.ini
data/obs-plugins/obs-glass/effects/glass.effect
```

Restart OBS after installation. The source appears under `+ -> Glass`.

## Basic Usage

1. Add a new source via the `+` button and choose `Glass`.
2. In the properties dialog, pick a background source from the dropdown.
3. Adjust shape position, size, and corner radius to place the glass.
4. Tune blur, frost, distortion, tint, glow, and shadow sliders to taste.
5. Place the Glass source anywhere in the scene's source list to control its
   z-order on the canvas.

## Version 1.0.1

- Fixed thread-safe background-source updates while rendering.
- Declared Glass as a composite source and enumerates its selected
  background source for OBS source tracking.
- Avoids drawing stale background textures when the intermediate render
  target cannot be started.
- Removed the unused obs-frontend-api link dependency.
- Hides the Glass source itself from the background-source picker.

## Version 1.0.0

- Initial release.
- Added freely placeable Glass source.
- Added rounded rectangular shape with corner radius and edge feathering.
- Added multi-level background blur with adjustable intensity.
- Added frosted glass effect.
- Added color tint with strength control.
- Added lens distortion with magnification.
- Added chromatic aberration.
- Added outer glow with optional directional shaping.
- Added inner glow with optional directional shaping.
- Added drop shadow with full configuration.
- Added transparent-background mode.
- Added searchable background-source picker.
- Added German and English locales.

## Support / Bugs

Please report issues in the resource discussion thread or in the GitHub issue
tracker.

## License

GPL-2.0-or-later.

## Third-Party Attribution

LucidGlass shader by Andi Stone (Andilippi), based on prior work by 4eckme and
references on Shadertoy. This plugin ports that shader into a standalone OBS
source with full property exposure.

## Disclaimer

Glass is an unofficial third-party plugin and is not affiliated with or
endorsed by the OBS Project.

AI-assisted tools were used during development and release preparation. The
maintainer is responsible for reviewing, testing, and publishing the released
plugin.

## Pre-Submit Checklist

- [x] Public GitHub repository exists.
- [x] README is visible on GitHub.
- [x] GPL license is visible on GitHub.
- [x] Source Code URL field points to the repository.
- [x] Release ZIP is attached to GitHub Releases or uploaded to the forum.
- [ ] At least one screenshot/GIF is added to the resource description.
- [ ] Description is in English.
- [ ] No OBS logo is used as resource icon or marketing artwork.
