# new-identity.ps1 — raccourci : supprime le lock et reboot
# Lance ça quand tu veux une nouvelle identité (après un ban, ou par précaution)

#Requires -RunAsAdministrator

$lockFile = "C:\spoof.lock"

if (Test-Path $lockFile) {
    Remove-Item $lockFile -Force
    Write-Host "[+] Lock supprimé — nouvelle identité au prochain boot."
} else {
    Write-Host "[!] Pas de lock trouvé — déjà en mode nouvelle identité."
}

$choice = Read-Host "Reboot maintenant ? (o/n)"
if ($choice -eq "o") {
    Restart-Computer -Force
}
