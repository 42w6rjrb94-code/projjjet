# new-identity.ps1 — supprime le lock pour forcer une nouvelle identite au prochain boot

#Requires -RunAsAdministrator

$lockFile = "C:\spoof.lock"

if (Test-Path $lockFile) {
    Remove-Item $lockFile -Force
    Write-Host "[+] Lock supprime — nouvelle identite au prochain boot."
} else {
    Write-Host "[!] Pas de lock — deja en mode nouvelle identite."
}

$choice = Read-Host "Reboot maintenant ? (o/n)"
if ($choice -eq "o") { Restart-Computer -Force }
