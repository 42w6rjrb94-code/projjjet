# spoof-win.ps1 — VM Windows, PowerShell 5.1+, run as SYSTEM via Task Scheduler
# Cible : Brawlhalla / Steam / AC client-side
# Couvre : MAC, MachineGuid, Hostname, ProductId, Timezone, DiagTrack GUIDs, TTL, Volume serial

#Requires -RunAsAdministrator

param(
    [string]$AdapterName = "Ethernet",         # FILL: nom exact depuis ipconfig /all dans la VM
    [string]$TimeZone    = "Romance Standard Time"  # aligné IP 5G française
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "SilentlyContinue"

# ── helpers ───────────────────────────────────────────────────────────────────

function New-RandomMac {
    $b    = [byte[]](1..6 | ForEach-Object { Get-Random -Min 0 -Max 256 })
    $b[0] = ($b[0] -band 0xFE) -bor 0x02   # locally administered, unicast
    return ($b | ForEach-Object { $_.ToString("X2") }) -join "-"
}

function New-RandomGuid   { return [guid]::NewGuid().ToString() }

function New-RandomHostname {
    $pfx = @("DESKTOP","LAPTOP","PC","WORKSTATION") | Get-Random
    $sfx = -join ((65..90) + (48..57) | Get-Random -Count 7 | ForEach-Object { [char]$_ })
    return "$pfx-$sfx"
}

function Write-Step ([string]$m) { Write-Host "[*] $m" -ForegroundColor Cyan  }
function Write-Done ([string]$m) { Write-Host "[+] $m" -ForegroundColor Green }
function Write-Warn ([string]$m) { Write-Host "[!] $m" -ForegroundColor Yellow }

# ── lock file — même identité si pas banni ───────────────────────────────────
# Supprime C:\spoof.lock pour forcer une nouvelle identité au prochain boot.

$lockFile = "C:\spoof.lock"

if (Test-Path $lockFile) {
    Write-Warn "Lock file trouvé — identité existante conservée."
    Write-Warn "Supprime C:\spoof.lock pour forcer une nouvelle identité."
    exit 0
}

Write-Step "Aucun lock — génération d'une nouvelle identité..."

# ── 1. MAC address ────────────────────────────────────────────────────────────

Write-Step "MAC spoof sur '$AdapterName'..."
$mac      = New-RandomMac
$macPlain = $mac -replace "-", ""

$nicClass = "HKLM:\SYSTEM\CurrentControlSet\Control\Class\{4d36e972-e325-11ce-bfc1-08002be10318}"
$nicKey   = Get-ChildItem $nicClass -ErrorAction SilentlyContinue | Where-Object {
    (Get-ItemProperty $_.PSPath -ErrorAction SilentlyContinue).DriverDesc -like "*Ethernet*"
} | Select-Object -First 1

if ($nicKey) {
    Set-ItemProperty $nicKey.PSPath -Name "NetworkAddress" -Value $macPlain
}

$adapter = Get-NetAdapter -Name $AdapterName -ErrorAction SilentlyContinue
if ($adapter) {
    Disable-NetAdapter -Name $AdapterName -Confirm:$false
    Set-NetAdapter     -Name $AdapterName -MacAddress $mac -ErrorAction SilentlyContinue
    Enable-NetAdapter  -Name $AdapterName -Confirm:$false
}
Write-Done "MAC → $mac"

# ── 2. MachineGuid ────────────────────────────────────────────────────────────

Write-Step "Regeneration MachineGuid..."
$guid = New-RandomGuid
Set-ItemProperty "HKLM:\SOFTWARE\Microsoft\Cryptography" -Name "MachineGuid" -Value $guid
Write-Done "MachineGuid → $guid"

# ── 3. Hostname ───────────────────────────────────────────────────────────────

Write-Step "Randomisation hostname..."
$newHostname = New-RandomHostname
Rename-Computer -NewName $newHostname -Force -ErrorAction SilentlyContinue
Write-Done "Hostname (après reboot) → $newHostname"

# ── 4. ProductId ──────────────────────────────────────────────────────────────

Write-Step "Randomisation ProductId..."
$newPid = "$(Get-Random -Min 10000 -Max 99999)-OEM-$(Get-Random -Min 1000000 -Max 9999999)-$(Get-Random -Min 10000 -Max 99999)"
Set-ItemProperty "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion" -Name "ProductId" -Value $newPid
Write-Done "ProductId → $newPid"

# ── 5. Timezone ───────────────────────────────────────────────────────────────

Write-Step "Timezone → $TimeZone..."
Set-TimeZone -Id $TimeZone
Write-Done "Timezone set."

# ── 6. DiagTrack / telemetry GUIDs ───────────────────────────────────────────

Write-Step "Nettoyage DiagTrack et telemetry GUIDs..."

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

Write-Step "TTL → 64 (signature Linux)..."
Set-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Services\Tcpip\Parameters" `
    -Name "DefaultTTL" -Value 64 -Type DWord
Write-Done "DefaultTTL = 64"

# ── 8. Steam HWID (hardware survey cache) ────────────────────────────────────
# Steam stocke un cache hardware survey dans le registry et AppData.
# Vider ça force Steam à recalculer l'identité hardware au prochain lancement.

Write-Step "Nettoyage cache hardware Steam..."

$steamHwSurvey = "$env:APPDATA\Steam\AppUpdateStats.db"
if (Test-Path $steamHwSurvey) { Remove-Item $steamHwSurvey -Force }

$steamReg = "HKCU:\Software\Valve\Steam"
if (Test-Path $steamReg) {
    @("SurveyScheduled","SurveyDate","LastInstallFailed") | ForEach-Object {
        Remove-ItemProperty $steamReg -Name $_ -Force -ErrorAction SilentlyContinue
    }
}

Write-Done "Cache Steam nettoyé."

# ── 9. VirtualBox guest detection masking ────────────────────────────────────
# Brawlhalla / Steam énumèrent les devices. VBoxGuest.sys et VBoxVideo.sys
# sont des tells immédiats. On renomme les valeurs de registre qui exposent
# le nom VirtualBox dans l'énumération PnP.

Write-Step "Masquage identifiants VirtualBox dans le registry..."

$vboxPaths = @(
    "HKLM:\SYSTEM\CurrentControlSet\Services\VBoxGuest",
    "HKLM:\SYSTEM\CurrentControlSet\Services\VBoxSF",
    "HKLM:\SYSTEM\CurrentControlSet\Services\VBoxVideo",
    "HKLM:\SYSTEM\CurrentControlSet\Services\VBoxMouse",
    "HKLM:\SYSTEM\CurrentControlSet\Services\VBoxService"
)

foreach ($path in $vboxPaths) {
    if (Test-Path $path) {
        # Renomme la valeur DisplayName pour ne plus exposer "VirtualBox"
        $current = (Get-ItemProperty $path -Name "DisplayName" -ErrorAction SilentlyContinue).DisplayName
        if ($current -and $current -like "*VirtualBox*") {
            Set-ItemProperty $path -Name "DisplayName" -Value "Generic System Service"
        }
        $currentDesc = (Get-ItemProperty $path -Name "Description" -ErrorAction SilentlyContinue).Description
        if ($currentDesc -and $currentDesc -like "*VirtualBox*") {
            Set-ItemProperty $path -Name "Description" -Value "System hardware driver"
        }
    }
}

# Registry enum PnP — cache des devices qui exposent VirtualBox
$pnpEnum = "HKLM:\SYSTEM\CurrentControlSet\Enum"
Get-ChildItem $pnpEnum -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
    $friendlyName = (Get-ItemProperty $_.PSPath -Name "FriendlyName" -ErrorAction SilentlyContinue).FriendlyName
    if ($friendlyName -like "*VirtualBox*") {
        Set-ItemProperty $_.PSPath -Name "FriendlyName" -Value "Generic Display Adapter" -ErrorAction SilentlyContinue
    }
    $desc = (Get-ItemProperty $_.PSPath -Name "DeviceDesc" -ErrorAction SilentlyContinue).DeviceDesc
    if ($desc -like "*VirtualBox*") {
        Set-ItemProperty $_.PSPath -Name "DeviceDesc" -Value "Standard VGA Graphics Adapter" -ErrorAction SilentlyContinue
    }
}

Write-Done "VirtualBox strings masquées dans le registry."

# ── 10. Lock file — écrit après succès complet ───────────────────────────────

$lockData = @{
    Date       = (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")
    MAC        = $mac
    MachineGuid= $guid
    Hostname   = $newHostname
    ProductId  = $newPid
} | ConvertTo-Json

$lockData | Out-File -FilePath $lockFile -Encoding UTF8 -Force
Write-Done "Lock file écrit → $lockFile"

# ── résumé ────────────────────────────────────────────────────────────────────

Write-Host ""
Write-Host "══════════════════════════════════════════════════" -ForegroundColor Yellow
Write-Host " SPOOF COMPLET" -ForegroundColor Yellow
Write-Host " Reboot requis pour : hostname" -ForegroundColor Yellow
Write-Host " Lock actif — même identité jusqu'à suppression de C:\spoof.lock" -ForegroundColor Yellow
Write-Host " Pour nouvelle identité : supprime C:\spoof.lock + reboot" -ForegroundColor Yellow
Write-Host "══════════════════════════════════════════════════" -ForegroundColor Yellow
