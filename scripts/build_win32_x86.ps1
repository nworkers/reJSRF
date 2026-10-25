# Builds the Win32 x86 tree with the newest installed Visual Studio.
#
# The configuration is a parameter so Debug and Release come from the same
# procedure. One multi-config Visual Studio tree holds every configuration.
# -Variant picks the disc the build runs: us (default) in build\win32_x86, jp
# in build\win32_x86_jp, each with its own generated code
# (docs/design/20261020-i108-japanese-variant.md).
param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Debug",
    [ValidateSet("us", "jp", "bundle_kor", "kor")]
    [string]$Variant = "us",
    [string[]]$Target = @()
)

$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$variantName = if ($Variant -in @("bundle_kor", "kor")) { "jsrf_bundle_kor" } elseif ($Variant -eq "jp") { "jsrf_jp" } else { "jsrf_us" }
$buildFolder = if ($Variant -in @("bundle_kor", "kor")) { "build\win32_x86_bundle_kor" } elseif ($Variant -eq "jp") { "build\win32_x86_jp" } else { "build\win32_x86" }
$BuildDir = Join-Path $Root $buildFolder
$VsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"

function Resolve-CMake
{
    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmake -ne $null)
    {
        return $cmake.Source
    }
    if (Test-Path $VsWhere)
    {
        $vsRoot = & $VsWhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
        $vsCmake = Join-Path $vsRoot "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        if (Test-Path $vsCmake)
        {
            return $vsCmake
        }
    }
    throw "CMake was not found. Install CMake or Visual Studio with the C++ desktop workload."
}

function Resolve-VisualStudioGenerator
{
    $generatorMap = @{
        "18" = "Visual Studio 18 2026"
        "17" = "Visual Studio 17 2022"
    }
    if (Test-Path $VsWhere)
    {
        $installationsJson = & $VsWhere -all -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json
        # Windows PowerShell 5.1 emits a parsed JSON array as one object, so
        # it is collected first and enumerated by the second pipeline.
        $parsed = ($installationsJson -join "") | ConvertFrom-Json
        $installations = @($parsed) | Sort-Object { [version]$_.installationVersion } -Descending
        foreach ($installation in $installations)
        {
            $major = ([version]$installation.installationVersion).Major.ToString()
            if ($generatorMap.ContainsKey($major))
            {
                return $generatorMap[$major]
            }
        }
    }
    throw "No supported Visual Studio with the x86/x64 C++ tools was found (2022 or 2026)."
}

# rexbox carries its own submodules (SDL, imgui, smaa, XbSymbolDatabase), so a
# plain `git submodule update --init` leaves them empty and the configure stops
# inside rexbox. Say the recursive command before CMake runs. A tree without
# git metadata (an exported source) skips the check.
function Test-Submodules
{
    if (-not (Test-Path (Join-Path $Root ".git")))
    {
        return
    }
    $git = Get-Command git -ErrorAction SilentlyContinue
    if ($git -eq $null)
    {
        return
    }
    $status = & $git.Source -C $Root submodule status --recursive
    if ($LASTEXITCODE -ne 0)
    {
        return
    }
    # A line starting with "-" is a registered submodule that is not checked out.
    $missing = @($status | Where-Object { $_ -match "^-" } | ForEach-Object { ($_ -split "\s+")[1] })
    if ($missing.Count -gt 0)
    {
        throw ("Submodule(s) not checked out: " + ($missing -join ", ") + "; run: git submodule update --init --recursive")
    }
}

Test-Submodules
$cmake = Resolve-CMake
$generator = Resolve-VisualStudioGenerator

# A tree configured by a different generator cannot be reused.
$cache = Join-Path $BuildDir "CMakeCache.txt"
if (Test-Path $cache)
{
    $cachedGenerator = (Select-String -Path $cache -Pattern "^CMAKE_GENERATOR:INTERNAL=(.*)$").Matches.Groups[1].Value
    if ($cachedGenerator -ne $generator)
    {
        Remove-Item -Recurse -Force $BuildDir
    }
}

& $cmake -S $Root -B $BuildDir -G $generator -A Win32 "-DREJSRF_VARIANT=$variantName"
if ($LASTEXITCODE -ne 0)
{
    throw "CMake configure failed."
}

$buildArgs = @("--build", $BuildDir, "--config", $Configuration, "--parallel")
if ($Target.Count -gt 0)
{
    $buildArgs += "--target"
    $buildArgs += $Target
}
& $cmake @buildArgs
if ($LASTEXITCODE -ne 0)
{
    throw "Build failed."
}
