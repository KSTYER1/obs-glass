param(
    [string]$PluginRoot = "."
)

$ErrorActionPreference = "Stop"

$sourcePath = Join-Path $PluginRoot "src/glass-source.c"
$cmakePath = Join-Path $PluginRoot "CMakeLists.txt"
$effectPath = Join-Path $PluginRoot "data/effects/glass.effect"
$enLocalePath = Join-Path $PluginRoot "data/locale/en-US.ini"
$deLocalePath = Join-Path $PluginRoot "data/locale/de-DE.ini"
if (-not (Test-Path -LiteralPath $sourcePath)) {
    throw "Source file not found: $sourcePath"
}
if (-not (Test-Path -LiteralPath $cmakePath)) {
    throw "CMake file not found: $cmakePath"
}
if (-not (Test-Path -LiteralPath $effectPath)) {
    throw "Effect file not found: $effectPath"
}
if (-not (Test-Path -LiteralPath $enLocalePath)) {
    throw "English locale file not found: $enLocalePath"
}
if (-not (Test-Path -LiteralPath $deLocalePath)) {
    throw "German locale file not found: $deLocalePath"
}

$source = Get-Content -Raw -LiteralPath $sourcePath
$cmake = Get-Content -Raw -LiteralPath $cmakePath
$effect = Get-Content -Raw -LiteralPath $effectPath
$enLocale = Get-Content -Raw -LiteralPath $enLocalePath
$deLocale = Get-Content -Raw -LiteralPath $deLocalePath
$failures = New-Object System.Collections.Generic.List[string]

function Get-FunctionBody {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Name
    )

    $match = [regex]::Match($Text, "(?s)static\s+[^{;]*\b$Name\s*\([^)]*\)\s*\{")
    if (-not $match.Success) {
        return $null
    }

    $start = $match.Index + $match.Length
    $depth = 1
    for ($i = $start; $i -lt $Text.Length; $i++) {
        if ($Text[$i] -eq '{') {
            $depth++
        } elseif ($Text[$i] -eq '}') {
            $depth--
            if ($depth -eq 0) {
                return $Text.Substring($start, $i - $start)
            }
        }
    }

    return $null
}

function Assert-Contains {
    param(
        [string]$Text,
        [string]$Pattern,
        [string]$Message
    )

    if ($Text -notmatch $Pattern) {
        $failures.Add($Message)
    }
}

function Assert-Missing {
    param(
        [string]$Text,
        [string]$Pattern,
        [string]$Message
    )

    if ($Text -match $Pattern) {
        $failures.Add($Message)
    }
}

function Assert-BodyContains {
    param(
        [string]$FunctionName,
        [string]$Pattern,
        [string]$Message
    )

    $body = Get-FunctionBody -Text $source -Name $FunctionName
    if ($null -eq $body) {
        $failures.Add("Missing function: $FunctionName")
        return
    }
    Assert-Contains $body $Pattern $Message
}

function Assert-BodyMissing {
    param(
        [string]$FunctionName,
        [string]$Pattern,
        [string]$Message
    )

    $body = Get-FunctionBody -Text $source -Name $FunctionName
    if ($null -eq $body) {
        $failures.Add("Missing function: $FunctionName")
        return
    }
    Assert-Missing $body $Pattern $Message
}

Assert-Contains $source "#include\s+<util/threading\.h>" "glass-source must include OBS threading helpers for mutex use."
Assert-Contains $source "obs_data_set_default_string\s*\(\s*settings,\s*`"source_search`"" "glass_source_defaults must define source_search because properties read it."
Assert-Contains $source "pthread_mutex_t\s+target_mutex" "glass_source must store a mutex for target weak-reference access."
Assert-Contains $source "char\s+\*target_name" "glass_source must retain the selected target name for reload-time resolution."
Assert-BodyContains "glass_source_create" "pthread_mutex_init\s*\(\s*&ctx->target_mutex" "glass_source_create must initialize target_mutex."
Assert-BodyContains "glass_source_create" "glass_source_update\s*\(\s*ctx\s*,\s*settings\s*\)" "glass_source_create must explicitly initialize settings because OBS does not call update automatically during creation."
Assert-BodyContains "glass_source_destroy" "pthread_mutex_destroy\s*\(\s*&ctx->target_mutex\s*\)" "glass_source_destroy must destroy target_mutex."
Assert-BodyContains "glass_source_destroy" "bfree\s*\(\s*ctx->target_name\s*\)" "glass_source_destroy must free the stored target name."
Assert-BodyContains "glass_source_swap_target" "pthread_mutex_lock\s*\(\s*&ctx->target_mutex\s*\)" "target weak-reference swaps must lock target_mutex."
Assert-BodyContains "glass_source_swap_target" "ctx->target_weak\s*=\s*new_target" "target weak-reference swaps must atomically assign target_weak."
Assert-BodyContains "glass_source_swap_target" "obs_weak_source_release\s*\(\s*old_target\s*\)" "target weak-reference swaps must release the old target weak ref after the swap."
Assert-BodyContains "glass_source_update_target_name" "ctx->target_name\s*=\s*bstrdup" "glass_source_update_target_name must retain the selected target name."
Assert-BodyContains "glass_source_load" "glass_source_update\s*\(\s*data\s*,\s*settings\s*\)" "glass_source_load must re-read saved settings after OBS has created all loading sources."
Assert-Contains $source "\.load\s*=\s*glass_source_load" "obs_source_info must register glass_source_load for reload-time target resolution."
Assert-BodyContains "glass_source_get_target" "obs_weak_source_get_source\s*\(\s*ctx->target_weak\s*\)" "glass_source_get_target must promote the weak target under lock."
Assert-BodyMissing "glass_source_render" "obs_weak_source_get_source\s*\(\s*ctx->target_weak\s*\)" "glass_source_render must not read target_weak directly."
Assert-BodyContains "glass_source_enum_active_sources" "enum_callback\s*\(\s*ctx->source\s*,\s*target\s*,\s*param\s*\)" "glass_source_enum_active_sources must enumerate the selected background source."
Assert-BodyContains "glass_source_enum_active_sources" "!\s*glass_source_target_contains_self\s*\(\s*ctx\s*,\s*target\s*\)" "glass_source_enum_active_sources must avoid reporting recursive scene targets that contain the Glass source itself."
Assert-BodyContains "glass_source_target_contains_self" "obs_group_or_scene_from_source\s*\(\s*target\s*\)" "recursive target detection must inspect scene/group targets."
Assert-BodyContains "glass_source_target_contains_self" "obs_scene_find_source_recursive\s*\(\s*scene\s*,\s*self_name\s*\)" "recursive target detection must find the Glass source inside selected scene/group targets."
Assert-Contains $source "OBS_SOURCE_VIDEO\s*\|\s*OBS_SOURCE_CUSTOM_DRAW" "glass source output_flags must include OBS_SOURCE_CUSTOM_DRAW."
Assert-Missing $source "OBS_SOURCE_COMPOSITE" "glass source must not set OBS_SOURCE_COMPOSITE without implementing audio_render; OBS rejects registration otherwise."
Assert-Contains $source "\.enum_active_sources\s*=\s*glass_source_enum_active_sources" "obs_source_info must register enum_active_sources."
Assert-BodyContains "glass_source_render" "if\s*\(\s*!gs_texrender_begin\s*\([^)]*\)\s*\)\s*\{(?s).*ctx->rendering\s*=\s*false;\s*return;" "glass_source_render must return immediately if gs_texrender_begin fails."
Assert-BodyContains "populate_sources_filtered" "strcmp\s*\(\s*names\.items\[i\]\s*,\s*skip_name\s*\)\s*!=\s*0" "populate_sources_filtered must skip the Glass source itself."
Assert-Contains $source "obs_properties_add_button2\s*\(\s*(props|current_props)\s*,\s*`"center_x`"" "glass source properties must expose a Center X button."
Assert-Contains $source "obs_properties_add_button2\s*\(\s*(props|current_props)\s*,\s*`"center_y`"" "glass source properties must expose a Center Y button."
Assert-Contains $source "obs_properties_add_button2\s*\(\s*(props|current_props)\s*,\s*`"center_both`"" "glass source properties must expose a Center Both button."
Assert-BodyContains "glass_source_set_center" "obs_data_set_double\s*\(\s*settings,\s*`"pos_x`"" "Center buttons must update pos_x through source settings."
Assert-BodyContains "glass_source_set_center" "obs_data_set_double\s*\(\s*settings,\s*`"pos_y`"" "Center buttons must update pos_y through source settings."
Assert-Missing $cmake "obs-frontend-api" "CMakeLists.txt must not require obs-frontend-api when it is unused."

Assert-Contains $source "bool\s+enable_liquid_motion" "glass_source must store the Liquid Motion enable setting."
Assert-Contains $source "float\s+liquid_strength,\s*liquid_speed,\s*liquid_depth,\s*liquid_seed,\s*liquid_time" "glass_source must store Liquid Motion macro controls and accumulated time."
Assert-Contains $source "gs_eparam_t\s+\*p_enable_liquid_motion" "glass_source must cache the EnableLiquidMotion shader parameter."
Assert-Contains $source "gs_eparam_t\s+\*p_liquid_time" "glass_source must cache the LiquidTime shader parameter."
Assert-Contains $source "gs_eparam_t\s+\*p_liquid_strength" "glass_source must cache the LiquidStrength shader parameter."
Assert-Contains $source "gs_eparam_t\s+\*p_liquid_speed" "glass_source must cache the LiquidSpeed shader parameter."
Assert-Contains $source "gs_eparam_t\s+\*p_liquid_depth" "glass_source must cache the LiquidDepth shader parameter."
Assert-Contains $source "gs_eparam_t\s+\*p_liquid_seed" "glass_source must cache the LiquidSeed shader parameter."
Assert-BodyContains "cache_params" "GP\s*\(\s*`"EnableLiquidMotion`"\s*\)" "cache_params must resolve EnableLiquidMotion."
Assert-BodyContains "cache_params" "GP\s*\(\s*`"LiquidTime`"\s*\)" "cache_params must resolve LiquidTime."
Assert-BodyContains "glass_source_defaults" "obs_data_set_default_bool\s*\(\s*settings,\s*`"enable_liquid_motion`"\s*,\s*false\s*\)" "Liquid Motion must default off for compatibility with old scenes."
Assert-BodyContains "glass_source_defaults" "obs_data_set_default_string\s*\(\s*settings,\s*`"liquid_preset`"\s*,\s*`"custom`"\s*\)" "Liquid Motion preset must default to custom."
Assert-BodyContains "glass_source_defaults" "obs_data_set_default_double\s*\(\s*settings,\s*`"liquid_strength`"\s*,\s*0\.0\s*\)" "Liquid Motion strength must default to zero."
Assert-BodyContains "glass_source_defaults" "obs_data_set_default_double\s*\(\s*settings,\s*`"liquid_speed`"\s*,\s*0\.35\s*\)" "Liquid Motion speed must have the planned default."
Assert-BodyContains "glass_source_defaults" "obs_data_set_default_double\s*\(\s*settings,\s*`"liquid_depth`"\s*,\s*0\.45\s*\)" "Liquid Motion depth must have the planned default."
Assert-BodyContains "glass_source_defaults" "obs_data_set_default_int\s*\(\s*settings,\s*`"liquid_seed`"\s*,\s*0\s*\)" "Liquid Motion seed must default to zero."
Assert-BodyContains "glass_source_update" "obs_data_get_bool\s*\(\s*settings,\s*`"enable_liquid_motion`"\s*\)" "glass_source_update must read enable_liquid_motion."
Assert-BodyContains "glass_source_update" "obs_data_get_double\s*\(\s*settings,\s*`"liquid_strength`"\s*\)" "glass_source_update must read liquid_strength."
Assert-BodyContains "glass_source_update" "obs_data_get_double\s*\(\s*settings,\s*`"liquid_speed`"\s*\)" "glass_source_update must read liquid_speed."
Assert-BodyContains "glass_source_update" "obs_data_get_double\s*\(\s*settings,\s*`"liquid_depth`"\s*\)" "glass_source_update must read liquid_depth."
Assert-BodyContains "glass_source_update" "obs_data_get_int\s*\(\s*settings,\s*`"liquid_seed`"\s*\)" "glass_source_update must read liquid_seed."
Assert-Contains $source "static\s+void\s+glass_source_video_tick\s*\(\s*void\s+\*data\s*,\s*float\s+seconds\s*\)" "Liquid Motion must use OBS video_tick for a per-source time base."
Assert-BodyContains "glass_source_video_tick" "ctx->liquid_time\s*\+=" "video_tick must accumulate liquid_time."
Assert-Contains $source "\.video_tick\s*=\s*glass_source_video_tick" "obs_source_info must register the Liquid Motion video_tick callback."
Assert-Contains $source "static\s+bool\s+apply_liquid_preset_to_settings" "Liquid presets must be applied through a shared helper that reports whether a named preset was applied."
Assert-Contains $source "`"aurora_drift`"" "Liquid presets must include Aurora Drift."
Assert-Contains $source "`"crystal_warp`"" "Liquid presets must include Crystal Warp."
Assert-Contains $source "`"heat_haze`"" "Liquid presets must include Heat Haze."
Assert-Contains $source "`"deep_ripple`"" "Liquid presets must include Deep Ripple."
Assert-BodyContains "on_liquid_preset_changed" "return\s+apply_liquid_preset_to_settings\s*\(\s*settings\s*,\s*preset\s*\)" "Selecting Custom must not reset existing Liquid Motion macro controls."
Assert-Contains $source "obs_properties_add_group\s*\(\s*props,\s*`"liquid_motion_section`"\s*,\s*TEXT\s*\(\s*`"LiquidMotionSection`"\s*\)" "Liquid Motion controls must be grouped under the localized Liquid Motion section."
Assert-Contains $source "obs_properties_add_group\s*\(\s*props,\s*`"background_section`"\s*,\s*TEXT\s*\(\s*`"BackgroundSection`"\s*\)" "Background source controls must be grouped under a localized Background Source section."
Assert-Contains $source "obs_properties_add_group\s*\(\s*props,\s*`"position_section`"\s*,\s*TEXT\s*\(\s*`"PositionSection`"\s*\)" "Position controls must be grouped under a localized Position section."
Assert-Contains $source "obs_properties_add_group\s*\(\s*props,\s*`"shape_section`"\s*,\s*TEXT\s*\(\s*`"ShapeSection`"\s*\)" "Shape controls must be grouped under a localized Shape section."
Assert-Contains $source "obs_properties_add_group\s*\(\s*props,\s*`"glass_material_section`"\s*,\s*TEXT\s*\(\s*`"GlassMaterialSection`"\s*\)" "Glass material controls must be grouped under a localized Glass Material section."
Assert-Contains $source "obs_properties_add_group\s*\(\s*props,\s*`"lens_distortion_section`"\s*,\s*TEXT\s*\(\s*`"LensDistortionSection`"\s*\)" "Lens distortion controls must be grouped under a localized Lens Distortion section."
Assert-Contains $source "obs_properties_add_group\s*\(\s*props,\s*`"chromatic_aberration_section`"\s*,\s*TEXT\s*\(\s*`"ChromaticAberrationSection`"\s*\)" "Chromatic Aberration controls must be grouped under a localized section."
Assert-Contains $source "obs_properties_add_group\s*\(\s*props,\s*`"outer_glow_section`"\s*,\s*TEXT\s*\(\s*`"OuterGlowSection`"\s*\)" "Outer Glow controls must be grouped under a localized section."
Assert-Contains $source "obs_properties_add_group\s*\(\s*props,\s*`"inner_glow_section`"\s*,\s*TEXT\s*\(\s*`"InnerGlowSection`"\s*\)" "Inner Glow controls must be grouped under a localized section."
Assert-Contains $source "obs_properties_add_group\s*\(\s*props,\s*`"drop_shadow_section`"\s*,\s*TEXT\s*\(\s*`"DropShadowSection`"\s*\)" "Drop Shadow controls must be grouped under a localized section."
Assert-Contains $source "obs_properties_add_list\s*\(\s*(props|motion_props)\s*,\s*`"liquid_preset`"" "Properties must expose a Liquid Preset list."
Assert-Contains $source "obs_properties_add_button2\s*\(\s*(props|motion_props)\s*,\s*`"reset_liquid_preset`"" "Properties must expose a Reset Preset button."
Assert-Contains $source "(obs_properties_add_bool\s*\(\s*(props|motion_props)\s*,\s*`"enable_liquid_motion`"|CHECK\s*\(\s*`"enable_liquid_motion`")" "Properties must expose an Enable Liquid Motion checkbox."
Assert-Contains $source "(obs_properties_add_float_slider\s*\(\s*(props|motion_props)\s*,\s*`"liquid_strength`"|SLIDER_F\s*\(\s*`"liquid_strength`")" "Properties must expose a Liquid Strength slider."
Assert-Contains $source "(obs_properties_add_float_slider\s*\(\s*(props|motion_props)\s*,\s*`"liquid_speed`"|SLIDER_F\s*\(\s*`"liquid_speed`")" "Properties must expose a Liquid Speed slider."
Assert-Contains $source "(obs_properties_add_float_slider\s*\(\s*(props|motion_props)\s*,\s*`"liquid_depth`"|SLIDER_F\s*\(\s*`"liquid_depth`")" "Properties must expose a Liquid Depth slider."
Assert-Contains $source "(obs_properties_add_int_slider\s*\(\s*(props|motion_props)\s*,\s*`"liquid_seed`"|SLIDER_I\s*\(\s*`"liquid_seed`")" "Properties must expose a Liquid Seed slider."
Assert-BodyContains "glass_source_render" "gs_effect_set_bool\s*\(\s*ctx->p_enable_liquid_motion" "glass_source_render must set EnableLiquidMotion."
Assert-BodyContains "glass_source_render" "gs_effect_set_float\s*\(\s*ctx->p_liquid_time" "glass_source_render must set LiquidTime."
Assert-BodyContains "glass_source_render" "gs_effect_set_float\s*\(\s*ctx->p_liquid_strength" "glass_source_render must set LiquidStrength."
Assert-BodyContains "glass_source_render" "gs_effect_set_float\s*\(\s*ctx->p_liquid_speed" "glass_source_render must set LiquidSpeed."
Assert-BodyContains "glass_source_render" "gs_effect_set_float\s*\(\s*ctx->p_liquid_depth" "glass_source_render must set LiquidDepth."
Assert-BodyContains "glass_source_render" "gs_effect_set_float\s*\(\s*ctx->p_liquid_seed" "glass_source_render must set LiquidSeed."

Assert-Contains $effect "uniform\s+bool\s+EnableLiquidMotion" "glass.effect must define EnableLiquidMotion."
Assert-Contains $effect "uniform\s+float\s+LiquidTime" "glass.effect must define LiquidTime."
Assert-Contains $effect "uniform\s+float\s+LiquidStrength" "glass.effect must define LiquidStrength."
Assert-Contains $effect "uniform\s+float\s+LiquidSpeed" "glass.effect must define LiquidSpeed."
Assert-Contains $effect "uniform\s+float\s+LiquidDepth" "glass.effect must define LiquidDepth."
Assert-Contains $effect "uniform\s+float\s+LiquidSeed" "glass.effect must define LiquidSeed."
Assert-Contains $effect "float2\s+apply_liquid_motion\s*\(" "glass.effect must implement apply_liquid_motion."
Assert-Contains $effect "EnableLiquidMotion\s*&&\s*LiquidStrength\s*>\s*EPSILON" "Liquid shader branch must stay behind enable and strength guards."
Assert-Contains $effect "apply_liquid_motion\s*\(\s*dist_uv" "Liquid Motion must affect the glass refraction path."

$liquidLocaleKeys = @(
    "BackgroundSection",
    "PositionSection",
    "ShapeSection",
    "GlassMaterialSection",
    "LensDistortionSection",
    "ChromaticAberrationSection",
    "OuterGlowSection",
    "InnerGlowSection",
    "DropShadowSection",
    "LiquidMotionSection",
    "EnableLiquidMotion",
    "LiquidPreset",
    "LiquidPresetCustom",
    "LiquidPresetAuroraDrift",
    "LiquidPresetCrystalWarp",
    "LiquidPresetHeatHaze",
    "LiquidPresetDeepRipple",
    "LiquidStrength",
    "LiquidSpeed",
    "LiquidDepth",
    "LiquidSeed",
    "ResetLiquidPreset"
)
foreach ($key in $liquidLocaleKeys) {
    Assert-Contains $enLocale "(?m)^$key=" "English locale must define $key."
    Assert-Contains $deLocale "(?m)^$key=" "German locale must define $key."
}

if ($failures.Count -gt 0) {
    foreach ($failure in $failures) {
        Write-Error $failure -ErrorAction Continue
    }
    exit 1
}

Write-Output "obs-glass static checks passed."
