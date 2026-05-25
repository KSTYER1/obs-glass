<#
.SYNOPSIS
    Baut obs-glass und erzeugt Release-Pakete (Portable-ZIP + NSIS-Installer).

.USAGE
    .\package.ps1
    .\package.ps1 -Version "1.1.0"
    .\package.ps1 -Version "1.1.0" -DeployUserPlugin
#>
param(
    [string]$Version = "1.0.1",
    [switch]$DeployUserPlugin
)

$ErrorActionPreference = "Stop"
$ROOT    = $PSScriptRoot
$BUILD   = "$ROOT\build_x64"
$DLL     = "$BUILD\Release\obs-glass.dll"
$DATA    = "$ROOT\data"
$DIST    = "$ROOT\dist"
$NSIS    = "${env:ProgramFiles(x86)}\NSIS\makensis.exe"
if (-not (Test-Path $NSIS)) { $NSIS = "${env:ProgramFiles}\NSIS\makensis.exe" }

# ---------------------------------------------------------------------------
# 1. Release bauen
# ---------------------------------------------------------------------------
Write-Host "`n[1/4] Baue Release..." -ForegroundColor Cyan
cmake --build "$BUILD" --config Release | Where-Object { $_ -match "(error|warning|->)" }
if (-not (Test-Path $DLL)) { throw "Build fehlgeschlagen: $DLL nicht gefunden" }

# ---------------------------------------------------------------------------
# 2. Portable-Ordner aufbauen
# ---------------------------------------------------------------------------
Write-Host "`n[2/4] Erzeuge Portable-Ordner..." -ForegroundColor Cyan
New-Item -ItemType Directory -Force $DIST | Out-Null
Remove-Item "$DIST\obs-glass-*-portable" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item "$DIST\obs-glass-*-portable.zip" -Force -ErrorAction SilentlyContinue
Remove-Item "$DIST\obs-glass-*-installer.exe" -Force -ErrorAction SilentlyContinue
$portName = "obs-glass-$Version-portable"
$portDir  = "$DIST\$portName"

Remove-Item "$portDir" -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force "$portDir\obs-plugins\64bit"      | Out-Null
New-Item -ItemType Directory -Force "$portDir\data\obs-plugins\obs-glass\locale"  | Out-Null
New-Item -ItemType Directory -Force "$portDir\data\obs-plugins\obs-glass\effects" | Out-Null

Copy-Item $DLL "$portDir\obs-plugins\64bit\obs-glass.dll"
Copy-Item "$DATA\locale\en-US.ini"   "$portDir\data\obs-plugins\obs-glass\locale\"
Copy-Item "$DATA\locale\de-DE.ini"   "$portDir\data\obs-plugins\obs-glass\locale\"
Copy-Item "$DATA\effects\glass.effect" "$portDir\data\obs-plugins\obs-glass\effects\"

# Portable-ZIP
$zipPath = "$DIST\obs-glass-$Version-portable.zip"
Remove-Item $zipPath -Force -ErrorAction SilentlyContinue
Compress-Archive -Path "$portDir\*" -DestinationPath $zipPath
Write-Host "  -> $zipPath" -ForegroundColor Green

# ---------------------------------------------------------------------------
# 3. NSIS-Skript generieren
# ---------------------------------------------------------------------------
Write-Host "`n[3/4] Generiere NSIS-Skript..." -ForegroundColor Cyan
$nsiPath = "$DIST\obs-glass.nsi"

$nsiScript = @"
Unicode True
!define PLUGIN_NAME    "obs-glass"
!define PLUGIN_VERSION "$Version"
!define PUBLISHER      "obs-glass contributors"

Name            "`${PLUGIN_NAME} `${PLUGIN_VERSION}"
OutFile         "obs-glass-`${PLUGIN_VERSION}-installer.exe"
InstallDir      "`$APPDATA\obs-studio\plugins\`${PLUGIN_NAME}"
InstallDirRegKey HKCU "Software\`${PLUGIN_NAME}" "InstallDir"
RequestExecutionLevel user

SetCompressor    /SOLID lzma
SetCompressorDictSize 8

!include "MUI2.nsh"

!define MUI_ABORTWARNING
!define MUI_ICON "`${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "`${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "German"
!insertmacro MUI_LANGUAGE "English"

Section "Plugin installieren" SecMain
    SectionIn RO

    SetOutPath "`$INSTDIR\bin\64bit"
    File "obs-glass.dll"

    SetOutPath "`$INSTDIR\data\locale"
    File "locale\en-US.ini"
    File "locale\de-DE.ini"

    SetOutPath "`$INSTDIR\data\effects"
    File "effects\glass.effect"

    WriteRegStr HKCU "Software\`${PLUGIN_NAME}" "InstallDir" "`$INSTDIR"
    WriteUninstaller "`$INSTDIR\uninstall.exe"

    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\`${PLUGIN_NAME}" \
        "DisplayName" "`${PLUGIN_NAME} `${PLUGIN_VERSION}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\`${PLUGIN_NAME}" \
        "UninstallString" "`$INSTDIR\uninstall.exe"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\`${PLUGIN_NAME}" \
        "Publisher" "`${PUBLISHER}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\`${PLUGIN_NAME}" \
        "DisplayVersion" "`${PLUGIN_VERSION}"
SectionEnd

Section "Uninstall"
    Delete "`$INSTDIR\bin\64bit\obs-glass.dll"
    Delete "`$INSTDIR\data\locale\en-US.ini"
    Delete "`$INSTDIR\data\locale\de-DE.ini"
    Delete "`$INSTDIR\data\effects\glass.effect"
    Delete "`$INSTDIR\uninstall.exe"
    RMDir  "`$INSTDIR\bin\64bit"
    RMDir  "`$INSTDIR\bin"
    RMDir  "`$INSTDIR\data\locale"
    RMDir  "`$INSTDIR\data\effects"
    RMDir  "`$INSTDIR\data"
    RMDir  "`$INSTDIR"
    DeleteRegKey HKCU "Software\`${PLUGIN_NAME}"
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\`${PLUGIN_NAME}"
SectionEnd
"@

New-Item -ItemType Directory -Force $DIST | Out-Null
Set-Content -Path $nsiPath -Value $nsiScript -Encoding UTF8

# Dateien fuer NSIS in $DIST bereitstellen
Copy-Item $DLL "$DIST\obs-glass.dll" -Force
New-Item -ItemType Directory -Force "$DIST\locale"  | Out-Null
New-Item -ItemType Directory -Force "$DIST\effects" | Out-Null
Copy-Item "$DATA\locale\en-US.ini"     "$DIST\locale\"     -Force
Copy-Item "$DATA\locale\de-DE.ini"     "$DIST\locale\"     -Force
Copy-Item "$DATA\effects\glass.effect" "$DIST\effects\"    -Force

# ---------------------------------------------------------------------------
# 4. NSIS-Installer bauen
# ---------------------------------------------------------------------------
Write-Host "`n[4/4] Baue NSIS-Installer..." -ForegroundColor Cyan
if (-not (Test-Path $NSIS)) {
    Write-Warning "makensis.exe nicht gefunden. Installer wird uebersprungen."
} else {
    Push-Location $DIST
    & $NSIS "obs-glass.nsi" 2>&1 | Where-Object { $_ -match "(Error|Output|Warning)" }
    Pop-Location
    $exePath = "$DIST\obs-glass-$Version-installer.exe"
    if (Test-Path $exePath) {
        Write-Host "  -> $exePath" -ForegroundColor Green
    } else {
        Write-Warning "NSIS Ausgabedatei nicht gefunden"
    }
}

# ---------------------------------------------------------------------------
# 5. In OBS-Beta deployen (OBS muss geschlossen sein)
# ---------------------------------------------------------------------------
Write-Host "`n[5/5] Deploye zu OBS..." -ForegroundColor Cyan

# Portable OBS
$obsBeta = "C:\Users\Awet\Desktop\OBS 32.1 BETA"
if (Test-Path $obsBeta) {
    $dst64    = "$obsBeta\obs-plugins\64bit"
    $dstData  = "$obsBeta\data\obs-plugins\obs-glass"
    New-Item -ItemType Directory -Force "$dstData\locale"  | Out-Null
    New-Item -ItemType Directory -Force "$dstData\effects" | Out-Null
    Copy-Item $DLL                          "$dst64\obs-glass.dll"      -Force
    Copy-Item "$DATA\locale\en-US.ini"      "$dstData\locale\"          -Force
    Copy-Item "$DATA\locale\de-DE.ini"      "$dstData\locale\"          -Force
    Copy-Item "$DATA\effects\glass.effect"  "$dstData\effects\"         -Force
    Write-Host "  -> Portable OBS ($obsBeta)" -ForegroundColor Green
} else {
    Write-Warning "Portable OBS nicht gefunden: $obsBeta"
}

if ($DeployUserPlugin) {
    # User-Plugin-Pfad (%APPDATA%\obs-studio\plugins\obs-glass\)
    $obsUser  = "$env:APPDATA\obs-studio\plugins\obs-glass"
    $usr64    = "$obsUser\bin\64bit"
    $usrData  = "$obsUser\data"
    New-Item -ItemType Directory -Force "$usr64"            | Out-Null
    New-Item -ItemType Directory -Force "$usrData\locale"   | Out-Null
    New-Item -ItemType Directory -Force "$usrData\effects"  | Out-Null
    Copy-Item $DLL                         "$usr64\obs-glass.dll"    -Force
    Copy-Item "$DATA\locale\en-US.ini"     "$usrData\locale\"        -Force
    Copy-Item "$DATA\locale\de-DE.ini"     "$usrData\locale\"        -Force
    Copy-Item "$DATA\effects\glass.effect" "$usrData\effects\"       -Force
    Write-Host "  -> User-Plugin ($obsUser)" -ForegroundColor Green
} else {
    Write-Host "  -> User-Plugin uebersprungen (mit -DeployUserPlugin aktivieren)" -ForegroundColor DarkGray
}

# ---------------------------------------------------------------------------
# Zusammenfassung
# ---------------------------------------------------------------------------
Write-Host "`n=== Fertig ===" -ForegroundColor Yellow
Write-Host "Ausgabe-Verzeichnis: $DIST"
Get-ChildItem $DIST -File | Select-Object Name, @{N="KB";E={[math]::Round($_.Length/1KB,1)}}
