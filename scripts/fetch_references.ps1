# Clones reference repositories into third_party/refs/ (gitignored) for study.
#
# Nothing here is built or copied into the project; see AGENTS.md (license
# rules) and docs/kb/external-references.md for what each may be used for.
$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$RefsDir = Join-Path $Root "third_party\refs"
New-Item -ItemType Directory -Force $RefsDir | Out-Null

$References = @(
    @{ Name = "jsrf-recomp"; Url = "https://github.com/andeecollard/jsrf-recomp.git" },
    @{ Name = "dplewis-jsrf"; Url = "https://github.com/dplewis/jsrf.git" },
    @{ Name = "nxdk"; Url = "https://github.com/XboxDev/nxdk.git" }
)

foreach ($reference in $References)
{
    $target = Join-Path $RefsDir $reference.Name
    if (Test-Path (Join-Path $target ".git"))
    {
        Write-Host "update $($reference.Name)"
        git -C $target pull --ff-only --quiet
    }
    else
    {
        Write-Host "clone  $($reference.Name)"
        git clone --depth 1 --quiet $reference.Url $target
    }
    if ($LASTEXITCODE -ne 0)
    {
        throw "git failed for $($reference.Name)"
    }
}
