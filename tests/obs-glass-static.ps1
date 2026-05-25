param(
    [string]$PluginRoot = "."
)

$ErrorActionPreference = "Stop"

$sourcePath = Join-Path $PluginRoot "src/glass-source.c"
$cmakePath = Join-Path $PluginRoot "CMakeLists.txt"
if (-not (Test-Path -LiteralPath $sourcePath)) {
    throw "Source file not found: $sourcePath"
}
if (-not (Test-Path -LiteralPath $cmakePath)) {
    throw "CMake file not found: $cmakePath"
}

$source = Get-Content -Raw -LiteralPath $sourcePath
$cmake = Get-Content -Raw -LiteralPath $cmakePath
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
Assert-Contains $source "pthread_mutex_t\s+target_mutex" "glass_source must store a mutex for target weak-reference access."
Assert-BodyContains "glass_source_create" "pthread_mutex_init\s*\(\s*&ctx->target_mutex" "glass_source_create must initialize target_mutex."
Assert-BodyContains "glass_source_destroy" "pthread_mutex_destroy\s*\(\s*&ctx->target_mutex\s*\)" "glass_source_destroy must destroy target_mutex."
Assert-BodyContains "glass_source_update" "pthread_mutex_lock\s*\(\s*&ctx->target_mutex\s*\)" "glass_source_update must lock target_mutex before swapping target_weak."
Assert-BodyContains "glass_source_update" "ctx->target_weak\s*=\s*new_target" "glass_source_update must atomically swap in the new target weak ref."
Assert-BodyContains "glass_source_update" "obs_weak_source_release\s*\(\s*old_target\s*\)" "glass_source_update must release the old target weak ref after the swap."
Assert-BodyContains "glass_source_get_target" "obs_weak_source_get_source\s*\(\s*ctx->target_weak\s*\)" "glass_source_get_target must promote the weak target under lock."
Assert-BodyMissing "glass_source_render" "obs_weak_source_get_source\s*\(\s*ctx->target_weak\s*\)" "glass_source_render must not read target_weak directly."
Assert-BodyContains "glass_source_enum_active_sources" "enum_callback\s*\(\s*ctx->source\s*,\s*target\s*,\s*param\s*\)" "glass_source_enum_active_sources must enumerate the selected background source."
Assert-Contains $source "OBS_SOURCE_VIDEO\s*\|\s*OBS_SOURCE_CUSTOM_DRAW" "glass source output_flags must include OBS_SOURCE_CUSTOM_DRAW."
Assert-Missing $source "OBS_SOURCE_COMPOSITE" "glass source must not set OBS_SOURCE_COMPOSITE without implementing audio_render; OBS rejects registration otherwise."
Assert-Contains $source "\.enum_active_sources\s*=\s*glass_source_enum_active_sources" "obs_source_info must register enum_active_sources."
Assert-BodyContains "glass_source_render" "if\s*\(\s*!gs_texrender_begin\s*\([^)]*\)\s*\)\s*\{(?s).*ctx->rendering\s*=\s*false;\s*return;" "glass_source_render must return immediately if gs_texrender_begin fails."
Assert-BodyContains "populate_sources_filtered" "strcmp\s*\(\s*names\.items\[i\]\s*,\s*skip_name\s*\)\s*!=\s*0" "populate_sources_filtered must skip the Glass source itself."
Assert-Missing $cmake "obs-frontend-api" "CMakeLists.txt must not require obs-frontend-api when it is unused."

if ($failures.Count -gt 0) {
    foreach ($failure in $failures) {
        Write-Error $failure -ErrorAction Continue
    }
    exit 1
}

Write-Output "obs-glass static checks passed."
