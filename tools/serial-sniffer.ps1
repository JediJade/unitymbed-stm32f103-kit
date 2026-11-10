<#
.SYNOPSIS
  Serial (UART / ST-Link VCP) sniffer - dumps what the STM32 is sending.
.EXAMPLE
  .\serial-sniffer.ps1                              # auto-pick the ST VCP, 115200
  .\serial-sniffer.ps1 -Port COM5 -Baud 921600
  .\serial-sniffer.ps1 -Hex                         # hex+ascii dump instead of text
  .\serial-sniffer.ps1 -Log uart.log -Timestamp
  .\serial-sniffer.ps1 -List                        # just list available ports
#>
[CmdletBinding()]
param(
    [string]$Port,
    [int]$Baud = 115200,
    [ValidateSet('None','Odd','Even','Mark','Space')][string]$Parity = 'None',
    [int]$DataBits = 8,
    [ValidateSet('One','Two','OnePointFive')][string]$StopBits = 'One',
    [ValidateSet('None','XOnXOff','RequestToSend','RequestToSendXOnXOff')][string]$Handshake = 'None',
    [switch]$Hex,
    [switch]$Timestamp,
    [string]$Log,
    [switch]$List
)

$ErrorActionPreference = 'Stop'

function Get-Ports {
    $wmi = @{}
    Get-CimInstance Win32_PnPEntity -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '\((COM\d+)\)' } |
        ForEach-Object { if ($_.Name -match '\((COM\d+)\)') { $wmi[$Matches[1]] = $_.Name } }

    [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object | ForEach-Object {
        [pscustomobject]@{ Port = $_; Description = $wmi[$_] }
    }
}

$ports = Get-Ports
if ($List) {
    if (-not $ports) { Write-Host "no serial ports found" -ForegroundColor DarkYellow } else { $ports | Format-Table -AutoSize }
    return
}

if (-not $Port) {
    $st = $ports | Where-Object { $_.Description -match 'STLink|ST-Link|STMicro|USB Serial|CDC' } | Select-Object -First 1
    if ($st)        { $Port = $st.Port }
    elseif ($ports) { $Port = ($ports | Select-Object -First 1).Port }
    else {
        Write-Host "No serial port found. Plug in a board with a VCP (ST-Link V2-1/V3, or STM32 USB-CDC)." -ForegroundColor Red
        Write-Host "Note: a plain ST-Link V2 dongle has NO virtual COM port - wire the UART to a USB-TTL adapter instead." -ForegroundColor DarkYellow
        return
    }
    Write-Host "auto-selected $Port" -ForegroundColor DarkGray
}

$sp = New-Object System.IO.Ports.SerialPort $Port, $Baud, $Parity, $DataBits, $StopBits
$sp.Handshake    = [System.IO.Ports.Handshake]::$Handshake
$sp.ReadTimeout  = 200
$sp.DtrEnable    = $true
$sp.RtsEnable    = $true

try { $sp.Open() }
catch {
    Write-Host "Cannot open $Port : $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "(another program - CubeIDE, PuTTY, a terminal - may already hold the port)" -ForegroundColor DarkYellow
    return
}

$cfg = "$Port @ $Baud $DataBits$($Parity.Substring(0,1))$(if($StopBits -eq 'One'){'1'}elseif($StopBits -eq 'Two'){'2'}else{'1.5'}) hs=$Handshake"
Write-Host "=== serial sniffer | $cfg | $(if($Hex){'hex'}else{'text'}) | Ctrl+C to stop ===" -ForegroundColor Cyan
if ($Log) { Write-Host "logging to $Log" -ForegroundColor DarkGray }

function Emit { param([string]$Text)
    Write-Host $Text
    if ($Log) { Add-Content -Path $Log -Value $Text -Encoding utf8 }
}

$offset   = 0
$hexBuf   = New-Object System.Collections.Generic.List[byte]
$lineBuf  = New-Object System.Text.StringBuilder
$total    = 0

try {
    while ($true) {
        $n = 0
        try { $n = $sp.BytesToRead } catch { break }
        if ($n -le 0) { Start-Sleep -Milliseconds 20; continue }

        $buf = New-Object byte[] $n
        $read = $sp.Read($buf, 0, $n)
        $total += $read

        if ($Hex) {
            for ($i = 0; $i -lt $read; $i++) {
                $hexBuf.Add($buf[$i])
                if ($hexBuf.Count -eq 16) {
                    $bytes = $hexBuf.ToArray()
                    $h = ($bytes | ForEach-Object { $_.ToString('X2') }) -join ' '
                    $a = -join ($bytes | ForEach-Object { if ($_ -ge 32 -and $_ -le 126) { [char]$_ } else { '.' } })
                    Emit ("{0:X8}  {1,-47}  |{2}|" -f $offset, $h, $a)
                    $offset += 16
                    $hexBuf.Clear()
                }
            }
        }
        else {
            for ($i = 0; $i -lt $read; $i++) {
                $b = $buf[$i]
                if ($b -eq 10) {                      # LF -> flush line
                    $text = $lineBuf.ToString().TrimEnd([char]13)
                    $pfx  = if ($Timestamp) { (Get-Date).ToString('HH:mm:ss.fff') + '  ' } else { '' }
                    Emit ($pfx + $text)
                    $lineBuf.Clear() | Out-Null
                }
                elseif ($b -eq 13) { }                # swallow CR
                elseif ($b -ge 32 -and $b -le 126) { $lineBuf.Append([char]$b) | Out-Null }
                else { $lineBuf.Append(('\x{0:X2}' -f $b)) | Out-Null }   # show control/garbage bytes
            }
        }
    }
}
finally {
    # flush whatever is left over
    if ($Hex -and $hexBuf.Count -gt 0) {
        $bytes = $hexBuf.ToArray()
        $h = ($bytes | ForEach-Object { $_.ToString('X2') }) -join ' '
        $a = -join ($bytes | ForEach-Object { if ($_ -ge 32 -and $_ -le 126) { [char]$_ } else { '.' } })
        Emit ("{0:X8}  {1,-47}  |{2}|" -f $offset, $h, $a)
    }
    elseif ($lineBuf.Length -gt 0) { Emit $lineBuf.ToString() }

    if ($sp.IsOpen) { $sp.Close() }
    $sp.Dispose()
    Write-Host "`n=== closed $Port | $total bytes ===" -ForegroundColor Cyan
}
