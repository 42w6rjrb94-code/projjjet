# spoof-win.ps1 — v2 — VirtualBox safe
# MAC via registre uniquement (pas de Set-NetAdapter qui tue le driver VBox)
# Lancer en PowerShell admin, ou automatique via Task Scheduler au logon

#Requires -RunAsAdministrator

param(
    [string]$TimeZone = "Romance Standard Time"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "SilentlyContinue"

function New-RandomMac {
    $b    = [byte[]](1..6 | ForEach-Object { Get-Random -Min 0 -Max 256 })
    $b[0] = ($b[0] -band 0xFE) -bor 0x02
    return ($b | ForEach-Object { $_.ToString("X2") }) -join ""
}

function New-RandomGuid    { return [guid]::NewGuid().ToString() }

function New-RandomHostname {
    $pfx = @("DESKTOP","LAPTOP","PC","WORKSTATION") | Get-Random
    $sfx = -join ((65..90) + (48..57) | Get-Random -Count 7 | ForEach-Object { [char]$_ })
    return "$pfx-$sfx"
}

function Write-Step ([string]$m) { Write-Host "[*] $m" -ForegroundColor Cyan  }
function Write-Done ([string]$m) { Write-Host "[+] $m" -ForegroundColor Green }
function Write-Warn ([string]$m) { Write-Host "[!] $m" -ForegroundColor Yellow }

# ── lock file ─────────────────────────────────────────────────────────────────

$lockFile = "C:\spoof.lock"

if (Test-Path $lockFile) {
    Write-Warn "Lock present — identite existante conservee."
    Write-Warn "Supprime C:\spoof.lock pour forcer une nouvelle identite."
    exit 0
}

Write-Step "Nouvelle identite en cours..."

# ── 1. MAC — registre uniquement (s'applique au prochain reboot) ──────────────

Write-Step "MAC spoof via registre..."
$mac = New-RandomMac

$nicClass = "HKLM:\SYSTEM\CurrentControlSet\Control\Class\{4d36e972-e325-11ce-bfc1-08002be10318}"
$nicKeys  = Get-ChildItem $nicClass -ErrorAction SilentlyContinue

foreach ($key in $nicKeys) {
    $props = Get-ItemProperty $key.PSPath -ErrorAction SilentlyContinue
    if ($props.DriverDesc -like "*PRO/1000*" -or $props.DriverDesc -like "*Ethernet*") {
        Set-ItemProperty $key.PSPath -Name "NetworkAddress" -Value $mac -ErrorAction SilentlyContinue
        Write-Done "MAC registre -> $($mac -replace '(..)(?!$)','$1-') sur $($props.DriverDesc)"
        break
    }
}

# ── 2. MachineGuid ────────────────────────────────────────────────────────────

Write-Step "MachineGuid..."
$guid = New-RandomGuid
Set-ItemProperty "HKLM:\SOFTWARE\Microsoft\Cryptography" -Name "MachineGuid" -Value $guid
Write-Done "MachineGuid -> $guid"

# ── 3. Hostname ───────────────────────────────────────────────────────────────

Write-Step "Hostname..."
$newHostname = New-RandomHostname
Rename-Computer -NewName $newHostname -Force -ErrorAction SilentlyContinue
Write-Done "Hostname (apres reboot) -> $newHostname"

# ── 4. ProductId ──────────────────────────────────────────────────────────────

Write-Step "ProductId..."
$newPid = "$(Get-Random -Min 10000 -Max 99999)-OEM-$(Get-Random -Min 1000000 -Max 9999999)-$(Get-Random -Min 10000 -Max 99999)"
Set-ItemProperty "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion" -Name "ProductId" -Value $newPid
Write-Done "ProductId -> $newPid"

# ── 5. Timezone ───────────────────────────────────────────────────────────────

Write-Step "Timezone -> $TimeZone..."
Set-TimeZone -Id $TimeZone
Write-Done "Timezone set."

# ── 6. DiagTrack / telemetry ──────────────────────────────────────────────────

Write-Step "DiagTrack + telemetry GUIDs..."

Stop-Service "DiagTrack" -Force -ErrorAction SilentlyContinue
Set-Service  "DiagTrack" -StartupType Disabled -ErrorAction SilentlyContinue

$sqm = "HKLM:\SOFTWARE\Microsoft\SQMClient"
if (Test-Path $sqm) {
    Set-ItemProperty $sqm -Name "MachineId" -Value "{$(New-RandomGuid)}"
}

$diag = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Diagnostics\DiagTrack"
if (Test-Path $diag) {
    (Get-Item $diag).Property | Where-Object { $_ -match "GUID|Id" } | ForEach-Object {
        Set-ItemProperty $diag -Name $_ -Value "{$(New-RandomGuid)}"
    }
}

$census = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Census"
if (Test-Path $census) {
    (Get-Item $census).Property | ForEach-Object {
        Remove-ItemProperty $census -Name $_ -Force -ErrorAction SilentlyContinue
    }
}

Write-Done "Telemetry clean."

# ── 7. TTL ────────────────────────────────────────────────────────────────────

Write-Step "TTL -> 64..."
Set-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Services\Tcpip\Parameters" `
    -Name "DefaultTTL" -Value 64 -Type DWord
Write-Done "DefaultTTL = 64"

# ── 8. Steam hardware survey cache ───────────────────────────────────────────

Write-Step "Cache Steam..."
$steamHw = "$env:APPDATA\Steam\AppUpdateStats.db"
if (Test-Path $steamHw) { Remove-Item $steamHw -Force }

$steamReg = "HKCU:\Software\Valve\Steam"
if (Test-Path $steamReg) {
    @("SurveyScheduled","SurveyDate","LastInstallFailed") | ForEach-Object {
        Remove-ItemProperty $steamReg -Name $_ -Force -ErrorAction SilentlyContinue
    }
}
Write-Done "Cache Steam clean."

# ── 9. VirtualBox string masking ─────────────────────────────────────────────

Write-Step "Masquage strings VirtualBox..."

$vboxServices = @(
    "VBoxGuest","VBoxSF","VBoxVideo","VBoxMouse","VBoxService"
)

foreach ($svc in $vboxServices) {
    $path = "HKLM:\SYSTEM\CurrentControlSet\Services\$svc"
    if (Test-Path $path) {
        $dn = (Get-ItemProperty $path -Name "DisplayName" -ErrorAction SilentlyContinue).DisplayName
        if ($dn -like "*VirtualBox*" -or $dn -like "*VBox*") {
            Set-ItemProperty $path -Name "DisplayName" -Value "Generic System Service" -ErrorAction SilentlyContinue
        }
        $dc = (Get-ItemProperty $path -Name "Description" -ErrorAction SilentlyContinue).Description
        if ($dc -like "*VirtualBox*" -or $dc -like "*VBox*") {
            Set-ItemProperty $path -Name "Description" -Value "System hardware driver" -ErrorAction SilentlyContinue
        }
    }
}

$pnpEnum = "HKLM:\SYSTEM\CurrentControlSet\Enum"
Get-ChildItem $pnpEnum -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
    $fn = (Get-ItemProperty $_.PSPath -Name "FriendlyName" -ErrorAction SilentlyContinue).FriendlyName
    if ($fn -like "*VirtualBox*" -or $fn -like "*VBox*") {
        Set-ItemProperty $_.PSPath -Name "FriendlyName" -Value "Generic Display Adapter" -ErrorAction SilentlyContinue
    }
    $dd = (Get-ItemProperty $_.PSPath -Name "DeviceDesc" -ErrorAction SilentlyContinue).DeviceDesc
    if ($dd -like "*VirtualBox*" -or $dd -like "*VBox*") {
        Set-ItemProperty $_.PSPath -Name "DeviceDesc" -Value "Standard VGA Graphics Adapter" -ErrorAction SilentlyContinue
    }
}

Write-Done "VirtualBox strings masquees."

# ── 10. Lock file ─────────────────────────────────────────────────────────────

$lockData = @{
    Date        = (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")
    MAC         = $mac -replace '(..)(?!$)','$1-'
    MachineGuid = $guid
    Hostname    = $newHostname
    ProductId   = $newPid
} | ConvertTo-Json

$lockData | Out-File -FilePath $lockFile -Encoding UTF8 -Force
Write-Done "Lock -> $lockFile"

# ── resume ────────────────────────────────────────────────────────────────────

Write-Host ""
Write-Host "=================================================" -ForegroundColor Yellow
Write-Host " SPOOF COMPLET" -ForegroundColor Yellow
Write-Host " Reboot requis pour : hostname + MAC" -ForegroundColor Yellow
Write-Host " Lock actif — meme identite jusqu'a suppression de C:\spoof.lock" -ForegroundColor Yellow
Write-Host " Pour nouvelle identite : supprime C:\spoof.lock puis reboot" -ForegroundColor Yellow
Write-Host "=================================================" -ForegroundColor Yellow
