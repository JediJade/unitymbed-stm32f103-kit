<#
.SYNOPSIS
  Start a new UnityMbed STM32F103 project from one of the kit's templates.
.DESCRIPTION
  Copies templates\<Template> to <Dest>\<Name>, leaves out build output and
  IDE session state, and sets "name" in unitymbed.json so UnityMbed builds
  build\<Name>.elf/.bin/.hex. Refuses to touch a folder that already exists.
.EXAMPLE
  .\tools\new-project.ps1 -List
  .\tools\new-project.ps1 -Template usb-cdc-bare -Name MySensor
  .\tools\new-project.ps1 -Template blink-bare -Name Test1 -Dest D:\work
#>
[CmdletBinding()]
param(
    [string]$Template,
    [string]$Name,
    [string]$Dest = (Join-Path $HOME 'UnityMbed'),
    [switch]$List
)

$ErrorActionPreference = 'Stop'

$kit       = Split-Path -Parent $PSScriptRoot
$templates = Join-Path $kit 'templates'
$available = Get-ChildItem $templates -Directory | Select-Object -ExpandProperty Name

if ($List -or -not $Template -or -not $Name) {
    Write-Host "Templates in $templates :" -ForegroundColor Cyan
    foreach ($t in $available) {
        $readme = Join-Path (Join-Path $templates $t) 'README.md'
        $title  = ''
        if (Test-Path $readme) {
            $title = (Get-Content $readme -TotalCount 1 -Encoding UTF8) -replace '^#\s*', ''
        }
        Write-Host ("  {0,-14} {1}" -f $t, $title)
    }
    if (-not $List) {
        Write-Host ''
        Write-Host 'Usage: .\tools\new-project.ps1 -Template <name> -Name <ProjectName> [-Dest <folder>]' -ForegroundColor DarkYellow
    }
    return
}

if ($available -notcontains $Template) {
    throw "No template '$Template'. Available: $($available -join ', ')"
}

# The project name becomes a file name and a make variable: keep it plain.
if ($Name -notmatch '^[A-Za-z][A-Za-z0-9_-]*$') {
    throw "Project name '$Name' must start with a letter and contain only letters, digits, '_' or '-'."
}

$src    = Join-Path $templates $Template
$target = Join-Path $Dest $Name

if (Test-Path $target) {
    throw "$target already exists - pick another name or folder. Nothing was changed."
}

New-Item -ItemType Directory -Path $target -Force | Out-Null

# Copy everything except build output and IDE state, keeping the tree.
$skip = @('build', '.unitymbed')
Get-ChildItem $src -Recurse -File | ForEach-Object {
    $rel   = $_.FullName.Substring($src.Length).TrimStart('\', '/')
    $first = ($rel -split '[\\/]')[0]
    if ($skip -contains $first) { return }

    $out = Join-Path $target $rel
    $dir = Split-Path -Parent $out
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    Copy-Item $_.FullName $out
}

# Rename the project in unitymbed.json without reformatting the file.
$json = Join-Path $target 'unitymbed.json'
if (Test-Path $json) {
    $text = [System.IO.File]::ReadAllText($json)
    $text = [regex]::Replace($text, '"name"\s*:\s*"[^"]*"', ('"name": "' + $Name + '"'), 1)
    [System.IO.File]::WriteAllText($json, $text, (New-Object System.Text.UTF8Encoding $false))
}

$count = (Get-ChildItem $target -Recurse -File | Measure-Object).Count
Write-Host "Created $target from '$Template' ($count files)." -ForegroundColor Green
Write-Host 'Next: open the folder in UnityMbed and build, or from Git Bash:' -ForegroundColor DarkGray
Write-Host "  cd `"$target`"; make PROJECT=$Name; make flash PROJECT=$Name" -ForegroundColor DarkGray
