# Glass

Glass is a third-party native OBS Studio source that draws a freely placeable
LucidGlass-style overlay over any chosen background source. The shape and its
glass effect can be tuned in real time and stacked with other sources at any
position in the scene's z-order.

The plugin is the source-plugin port of the LucidGlass shader by Andi Stone
(Andilippi). It exposes the same look as a regular OBS source so it can be
moved, resized, and layered like any other scene item.

## Features

- Rounded rectangular glass shape with configurable width, height, corner
  radius, and edge feathering.
- Multi-level background blur with adjustable intensity.
- Frosted glass effect with adjustable strength.
- Color tint with adjustable strength and alpha.
- Lens distortion with edge thickness, falloff, and magnification controls.
- Chromatic aberration with strength, thickness, and falloff.
- Outer glow with optional bi-directional shaping (angle, spread, softness).
- Inner glow with optional bi-directional shaping (angle, spread, softness).
- Drop shadow with offset, blur, opacity, and color.
- Liquid Motion Pack with cinematic Liquid Drift presets and simple strength,
  speed, depth, and seed controls.
- Grouped source properties so background, position, shape, material, lens,
  color, glow, shadow, and motion controls are easier to scan.
- Transparent-background mode so only the shape contributes to the canvas.
- Searchable background-source picker with alphabetical scene and source list.
- German and English locales.

## Requirements

- OBS Studio 30.x, 31.x, or 32.x
- Windows x64 for the packaged release
- A scene-item background source for the glass to refract

## Installation

Recommended installer:

1. Download the `*-installer.exe` file from the latest GitHub release.
2. Close OBS Studio.
3. Run the installer. By default it installs into your OBS user plugin folder.
4. Start OBS Studio again.

Portable or manual installation:

1. Download the release ZIP instead of the installer EXE.
2. Extract the ZIP into your OBS Studio installation folder, or copy the
   included `obs-plugins` and `data` folders into it.
3. Start OBS Studio again.

### Windows

Download the release archive and extract or copy its contents into your OBS
Studio installation directory.

The final layout should include:

```text
obs-plugins/64bit/obs-glass.dll
data/obs-plugins/obs-glass/locale/en-US.ini
data/obs-plugins/obs-glass/locale/de-DE.ini
data/obs-plugins/obs-glass/effects/glass.effect
```

Restart OBS after installation. The source appears in the source list under:

```text
+ -> Glass
```

## Basic Usage

1. Add a new source via the `+` button and choose `Glass`.
2. In the properties dialog, pick a background source from the dropdown. This
   is the image the glass refracts, blurs, and tints. Any video source or
   scene can be used.
3. Adjust shape position, size, and corner radius to place the glass.
4. Tune blur, frost, distortion, tint, glow, and shadow sliders to taste.
5. Place the Glass source anywhere in the scene's source list. Its z-order
   determines what it sits in front of.

The background source picker contains a search field for filtering long lists
of sources and scenes.

## Building from Source

Requires CMake 3.28 or newer, OBS development headers, and a supported
compiler toolchain.

```bash
git clone https://github.com/KSTYER1/obs-glass.git
cd obs-glass
cmake --preset windows-x64
cmake --build --preset windows-x64 --config RelWithDebInfo
```

The project uses the
[obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate) build
structure.

Run the static review guard after source changes:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\obs-glass-static.ps1
```

## Local Dist Output

Completed Windows builds should also have a local `dist/` folder for packaging
review. The folder mirrors the OBS-ready release layout:

```text
dist/
  obs-glass.dll
  obs-glass.nsi
  obs-glass-1.1.0-installer.exe
  obs-glass-1.1.0-portable.zip
  effects/
    glass.effect
  locale/
    en-US.ini
    de-DE.ini
  obs-glass-1.1.0-portable/
    obs-plugins/64bit/obs-glass.dll
    data/obs-plugins/obs-glass/effects/glass.effect
    data/obs-plugins/obs-glass/locale/en-US.ini
    data/obs-plugins/obs-glass/locale/de-DE.ini
```

`dist/` is generated from a verified build and is ignored by Git. Publish the
installer EXE and portable ZIP as release assets.

`package.ps1` deploys to `OBS 32.1 BETA` on the current user's Desktop by default.
Use `-SkipDeploy` when OBS is running and only package artifacts are needed.
Use `-DeployUserPlugin` only when explicitly installing into the
`%APPDATA%\obs-studio\plugins\obs-glass` user-plugin path.

## Version History

### 1.1.0

- Added Liquid Motion Pack with cinematic Liquid Drift presets: Aurora Drift,
  Crystal Warp, Heat Haze, and Deep Ripple.
- Added simple Liquid Motion controls for strength, speed, depth, and seed.
- Grouped source properties into focused sections for a cleaner settings UI.
- Keeps Liquid Motion disabled by default so existing saved Glass sources load
  unchanged.

### 1.0.4

- Fixed reload restoration so saved Glass sources re-read their settings during
  source creation and resolve their selected background source again after OBS
  has created all sources in the scene collection.
- Allows recursive scene-backed glass setups to load by keeping OBS'
  active-source recursion guard from rejecting a Glass source whose selected
  scene target contains that same Glass source.
- Replaced a Windows-only source-search comparison with OBS' portable
  case-insensitive string helper.

### 1.0.3

- Added Center X, Center Y, and Center Both buttons for quickly centering the
  glass position against the selected background source.
- Falls back to the last known output size when no background source is
  available.

### 1.0.2

- Fixed OBS source registration failure caused by setting
  `OBS_SOURCE_COMPOSITE` without an `audio_render` callback.
- Restored Glass availability in the OBS source list and existing scene
  source control.
- Added a static guard to prevent this load-blocking flag combination from
  returning.

### 1.0.1

- Fixed thread-safe background-source updates while rendering.
- Enumerates the selected background source for OBS source tracking.
- Avoids drawing stale background textures when the intermediate render
  target cannot be started.
- Removed the unused obs-frontend-api link dependency.
- Hides the Glass source itself from the background-source picker.

### 1.0.0

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

## License

Glass is licensed under GPL-2.0-or-later.

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
