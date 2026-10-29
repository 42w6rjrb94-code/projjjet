# register-startup.ps1 — une seule fois en admin

#Requires -RunAsAdministrator

$scriptPath = Join-Path $PSScriptRoot "spoof-win.ps1"

if (-not (Test-Path $scriptPath)) {
    Write-Error "spoof-win.ps1 introuvable a $scriptPath"
    exit 1
}

$action = New-ScheduledTaskAction `
    -Execute  "powershell.exe" `
    -Argument "-WindowStyle Hidden -ExecutionPolicy Bypass -File `"$scriptPath`""

$trigger = New-ScheduledTaskTrigger -AtLogOn

$settings = New-ScheduledTaskSettingsSet `
    -AllowStartIfOnBatteries `
    -DontStopIfGoingOnBatteries `
    -ExecutionTimeLimit (New-TimeSpan -Minutes 2) `
    -MultipleInstances IgnoreNew

$principal = New-ScheduledTaskPrincipal `
    -UserId    "SYSTEM" `
    -LogonType ServiceAccount `
    -RunLevel  Highest

Register-ScheduledTask `
    -TaskName  "VMSpoofOnLogon" `
    -Action    $action `
    -Trigger   $trigger `
    -Settings  $settings `
    -Principal $principal `
    -Force

Write-Host "[+] Task VMSpoofOnLogon enregistree — fire a chaque logon en SYSTEM."
