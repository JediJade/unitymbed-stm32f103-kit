<#
.SYNOPSIS
  USB device sniffer - watches devices arriving/leaving in real time.
.EXAMPLE
  .\usb-sniffer.ps1                 # ST / STM32 / DFU devices only
  .\usb-sniffer.ps1 -All            # every USB device
  .\usb-sniffer.ps1 -Log usb.log    # also append to a file
#>
[CmdletBinding()]
param(
    [string]$Match = 'VID_0483|VID_0484|STM|ST-?Link|DFU',
    [switch]$All,
    [int]$IntervalMs = 400,
    [string]$Log
)

$ErrorActionPreference = 'Stop'

# Known USB IDs worth naming on sight.
$KnownPid = @{
    '0483:3744' = 'ST-Link V1'
    '0483:3748' = 'ST-Link V2'
    '0483:374A' = 'ST-Link V2-1 (bootloader)'
    '0483:374B' = 'ST-Link V2-1 (MSD+VCP+debug)'
    '0483:374D' = 'ST-Link V3 (bootloader)'
    '0483:374E' = 'ST-Link V3 (no MSD)'
    '0483:374F' = 'ST-Link V3 (MSD+VCP+debug)'
    '0483:3752' = 'ST-Link V3 (no MSD, dual VCP)'
    '0483:3753' = 'ST-Link V3 (debug only)'
    '0483:3754' = 'ST-Link V3 (MSD+dual VCP)'
    '0483:3757' = 'ST-Link V3 (power bridge)'
    '0483:DF11' = 'STM32 DFU bootloader (system memory)'
    '0483:5740' = 'STM32 USB CDC (Virtual COM, ST firmware)'
    '0483:5741' = 'STM32 USB device (ST VID)'
    '0483:572B' = 'STM32 USB audio'
    '0483:A2CA' = 'STM32 composite (ST sample fw)'
}

function Get-UsbSnapshot {
    $filter = if ($All) { '.' } else { $Match }
    $devs = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
        Where-Object { $_.InstanceId -match '^USB' -and ($_.InstanceId -match $filter -or $_.FriendlyName -match $filter) }

    $map = @{}
    foreach ($d in $devs) {
        $vid = ''; $pidv = ''; $serial = ''
        if ($d.InstanceId -match 'VID_([0-9A-Fa-f]{4})') { $vid = $Matches[1].ToUpper() }
        if ($d.InstanceId -match 'PID_([0-9A-Fa-f]{4})') { $pidv = $Matches[1].ToUpper() }
        # last path segment is the serial / instance id
        $serial = $d.InstanceId.Split([char]0x5C)[-1]

        $key = "$vid`:$pidv"
        $map[$d.InstanceId] = [pscustomobject]@{
            InstanceId = $d.InstanceId
            Name       = $d.FriendlyName
            Class      = $d.Class
            Status     = $d.Status
            Vid        = $vid
            Pid        = $pidv
            Ids        = $key
            Serial     = $serial
            Known      = $KnownPid[$key]
        }
    }
    return $map
}

function Write-Line {
    param([string]$Text, [ConsoleColor]$Color = 'Gray')
    Write-Host $Text -ForegroundColor $Color
    if ($Log) { Add-Content -Path $Log -Value $Text -Encoding utf8 }
}

function Format-Dev {
    param($d, [string]$Sigil)
    $ts   = (Get-Date).ToString('HH:mm:ss.fff')
    $name = if ($d.Known) { "$($d.Name)  [$($d.Known)]" } else { $d.Name }
    $line = "{0} {1} {2,-9} {3,-11} {4}" -f $ts, $Sigil, $d.Ids, $d.Class, $name
    if ($d.Serial -and $d.Serial -notmatch '^\d+&') { $line += "  sn=$($d.Serial)" }
    if ($d.Status -ne 'OK') { $line += "  <status: $($d.Status)>" }
    return $line
}

$scope = if ($All) { 'ALL USB devices' } else { "filter /$Match/" }
Write-Line "=== USB sniffer | $scope | poll ${IntervalMs}ms | Ctrl+C to stop ===" 'Cyan'
if ($Log) { Write-Host "logging to $Log" -ForegroundColor DarkGray }

$prev = Get-UsbSnapshot
if ($prev.Count -eq 0) {
    Write-Line "(nothing matching is plugged in right now)" 'DarkYellow'
} else {
    Write-Line "--- already connected ---" 'DarkGray'
    foreach ($d in $prev.Values | Sort-Object Ids, Name) { Write-Line (Format-Dev $d '=') 'White' }
}
Write-Line "--- watching ---" 'DarkGray'

while ($true) {
    Start-Sleep -Milliseconds $IntervalMs
    try { $cur = Get-UsbSnapshot } catch { continue }

    foreach ($k in $cur.Keys)  { if (-not $prev.ContainsKey($k)) { Write-Line (Format-Dev $cur[$k] '[+]') 'Green' } }
    foreach ($k in $prev.Keys) { if (-not $cur.ContainsKey($k))  { Write-Line (Format-Dev $prev[$k] '[-]') 'Red'   } }
    # status transitions on devices that stayed put (e.g. driver problem / reset)
    foreach ($k in $cur.Keys) {
        if ($prev.ContainsKey($k) -and $prev[$k].Status -ne $cur[$k].Status) {
            Write-Line (Format-Dev $cur[$k] '[~]') 'Yellow'
        }
    }
    $prev = $cur
}
