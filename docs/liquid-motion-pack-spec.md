# Liquid Motion Pack Feature Spec

Status: implemented and release-packaged locally
Target plugin: obs-glass
Target release: 1.1.0
Date: 2026-05-28

## Goal

Add a cinematic but stream-safe motion layer to obs-glass. The first motion
feature should make the glass material feel alive without turning existing
sources into a distracting effect by default.

Chosen direction:

- Motion family: Ambient Motion
- Motion signature: Liquid Drift
- Intensity target: Visible Liquid Glass
- Control model: Simple Macro Controls
- Release slice: Creator Liquid Pack
- Preset philosophy: Cinematic Liquid Set

## Non-Goals

- No audio-reactive behavior in the first release.
- No general multi-mode motion platform in the first release.
- No scene-transition state machine in the first release.
- No behavior change for existing saved Glass sources unless the user enables
  motion or chooses a motion preset.

## User-Facing Feature

Add a `Liquid Motion` section to the Glass source properties.

Suggested visible controls:

- `Enable Liquid Motion` checkbox.
- `Liquid Preset` list:
  - `Custom`
  - `Aurora Drift`
  - `Crystal Warp`
  - `Heat Haze`
  - `Deep Ripple`
- `Strength` slider.
- `Speed` slider.
- `Depth` slider.
- `Seed` integer field or slider.
- `Reset Preset` button, visible when a named preset is selected.

Suggested control ranges:

- `Strength`: `0.0` to `1.0`, default `0.0`.
- `Speed`: `0.0` to `2.0`, default depends on preset.
- `Depth`: `0.0` to `1.0`, default depends on preset.
- `Seed`: `0` to `9999`, default generated per source or fixed at `0`.

The `Strength = 0.0` path must be treated as effectively disabled.

## Settings Keys

Proposed new settings:

- `enable_liquid_motion` bool, default `false`.
- `liquid_preset` string, default `custom`.
- `liquid_strength` double, default `0.0`.
- `liquid_speed` double, default `0.35`.
- `liquid_depth` double, default `0.45`.
- `liquid_seed` int, default `0`.

Preset string values:

- `custom`
- `aurora_drift`
- `crystal_warp`
- `heat_haze`
- `deep_ripple`

Existing saved sources should receive these defaults through
`glass_source_defaults`. Because `enable_liquid_motion` is false and
`liquid_strength` is `0.0`, reloading older scene collections should render the
same as before.

## Presets

Presets should write the macro controls, not hidden one-off state. This keeps
the feature understandable and makes `Custom` easy to reason about.

Initial preset targets:

| Preset | Intent | Strength | Speed | Depth |
| --- | --- | ---: | ---: | ---: |
| Aurora Drift | soft colored current, showy but smooth | 0.45 | 0.28 | 0.45 |
| Crystal Warp | clear lens-like material movement | 0.52 | 0.36 | 0.55 |
| Heat Haze | warmer, wavier cinematic motion | 0.58 | 0.48 | 0.62 |
| Deep Ripple | strongest visible liquid movement | 0.70 | 0.42 | 0.75 |

These values are starting points for visual tuning, not final constants.

## Rendering Design

Use the existing custom-draw source render path. The current shader already has
the rounded shape mask, blur, frost, tint, distortion, glow, inner glow, and
shadow. Liquid Motion should add time-varying distortion inside that same mask.

Recommended shader uniforms:

- `EnableLiquidMotion` bool.
- `LiquidTime` float.
- `LiquidStrength` float.
- `LiquidSpeed` float.
- `LiquidDepth` float.
- `LiquidSeed` float.

The implementation should add a `video_tick(void *data, float seconds)`
callback to accumulate a per-source motion time. The callback is present in the
local OBS headers under `.deps/include/obs-source.h`.

Shader behavior:

- Compute a low-frequency drifting field from UV, seed, and `LiquidTime`.
- Apply the field to the existing refraction/distortion sampling path.
- Modulate by the existing rounded-rect alpha or shape mask so the effect does
  not leak outside the glass.
- Keep all new math behind `EnableLiquidMotion && LiquidStrength > 0.0`.
- Avoid additional render targets for the first release.

## UI Behavior

- Changing a named preset updates the macro controls.
- Editing a macro control after choosing a named preset should switch the preset
  to `Custom`, unless OBS property callbacks make that too fragile. If fragile,
  keep the named preset selected but document that the sliders override it.
- `Reset Preset` reapplies the selected preset values.
- `Liquid Motion` should be collapsed or placed after the existing distortion
  controls so existing workflows remain familiar.

## Localization

Add locale keys in both `en-US.ini` and `de-DE.ini` for:

- Section title.
- Enable checkbox.
- Preset list and each preset.
- Strength, Speed, Depth, Seed.
- Reset Preset button.
- Optional help text if the existing UI pattern supports it.

Static locale checks should be updated so both locale files remain complete.

## Compatibility And Migration

- Existing scenes must load with motion disabled.
- Existing scene/source reload behavior must keep the 1.0.4 fixes intact:
  - `create` applies saved settings.
  - `load` can re-resolve the target source after OBS creates all collection
    sources.
  - recursive target protection remains active.
- The feature must not add `OBS_SOURCE_COMPOSITE` or other output flags unless
  the implementation also satisfies the OBS API requirements for those flags.

## Performance Guardrails

- `enable_liquid_motion == false` or `liquid_strength <= 0.0` should skip the
  new shader branch.
- No new texrender passes in the first release.
- Avoid large loops or expensive noise functions in the pixel shader.
- Test at 1080p and 1440p with large Glass sources because full-screen panels
  amplify shader cost.

## Test Plan

Static checks:

- New settings have defaults, update reads, property controls, shader params,
  locale keys, and static-test coverage.
- Existing reload/load target resolution guards remain present.
- `get_properties` still tolerates `data == NULL`.
- Package scripts still reconfigure before build so version metadata stays
  current.

Build checks:

- Configure with `cmake --preset windows-x64`.
- Build `RelWithDebInfo`.
- Build `Release`.
- Run `plugins/maintained/obs-glass/tests/obs-glass-static.ps1`.
- Run root `tests/obs-glass-static.ps1` if it remains the workspace guard.

Manual OBS checks:

- Install first to `OBS 32.1 BETA` on the current user's Desktop.
- Load an old scene collection with saved Glass sources; verify they look
  unchanged.
- Add a new Glass source and enable each preset.
- Save, close OBS, restart OBS, and confirm selected preset/controls persist.
- Verify the source is not blank after reload.
- Verify no recursion errors appear when a scene target contains the Glass
  source.
- Compare GPU/frame impact with motion off, Aurora Drift, and Deep Ripple.

## Open Implementation Questions

- Should `Seed` be visible by default or hidden behind an advanced group?
- Should preset names be translated literally or kept as English product names
  in the German locale?
- Should `Heat Haze` affect tint subtly, or should presets only touch motion
  controls in the first release?
- Should `Strength` also scale the current `MaxDistortionAmount`, or should
  Liquid Motion add only an independent offset layer?
