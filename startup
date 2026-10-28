# register-startup.ps1 — run ONCE en admin dans la VM
# Enregistre spoof-win.ps1 dans Task Scheduler au logon SYSTEM

#Requires -RunAsAdministrator

$scriptPath = Join-Path $PSScriptRoot "spoof-win.ps1"

if (-not (Test-Path $scriptPath)) {
    Write-Error "spoof-win.ps1 introuvable à $scriptPath"
    exit 1
}

$action = New-ScheduledTaskAction `
    -Execute  "powershell.exe" `
    -Argument "-WindowStyle Hidden -ExecutionPolicy Bypass -File `"$scriptPath`""

$trigger  = New-ScheduledTaskTrigger -AtLogOn

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

Write-Host "[+] Task 'VMSpoofOnLogon' enregistrée — fire à chaque logon en SYSTEM."
