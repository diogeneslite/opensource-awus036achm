#
# ALFA AWUS036ACHM Windows 11 Driver Environment Validator
# Checks Secure Boot, Test Signing, and Driver Signing Store
#

Write-Host "=================================================================" -ForegroundColor Cyan
Write-Host "  ALFA AWUS036ACHM Open-Source Driver Environment Validator    " -ForegroundColor Cyan
Write-Host "=================================================================" -ForegroundColor Cyan
Write-Host ""

# 1. Check Secure Boot
$secureBootReg = Get-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\SecureBoot\State" -ErrorAction SilentlyContinue
$secureBootEnabled = if ($secureBootReg) { $secureBootReg.UEFISecureBootEnabled -eq 1 } else { $false }

if ($secureBootEnabled) {
    Write-Host "[!] UEFI Secure Boot: ENABLED" -ForegroundColor Red
    Write-Host "    -> Windows 11 Kernel-Mode Code Signing (KMCS) will block ANY" -ForegroundColor Yellow
    Write-Host "       test-signed driver (Code 52 / STATUS_INVALID_IMAGE_HASH)." -ForegroundColor Yellow
    Write-Host "    -> FIX: To test custom drivers, restart into UEFI BIOS and" -ForegroundColor Yellow
    Write-Host "       temporarily disable Secure Boot." -ForegroundColor Yellow
} else {
    Write-Host "[+] UEFI Secure Boot: DISABLED (Ready for Test Signing)" -ForegroundColor Green
}

# 2. Check Test Signing
$systemStartOptions = (Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Control" -ErrorAction SilentlyContinue).SystemStartOptions
$testSigningEnabled = $systemStartOptions -match "TESTSIGNING"

if ($testSigningEnabled) {
    Write-Host "[+] Windows Test Signing: ACTIVE" -ForegroundColor Green
    Write-Host "    -> Kernel will load verified test-signed .sys binaries." -ForegroundColor Green
} else {
    Write-Host "[-] Windows Test Signing: NOT ACTIVE" -ForegroundColor Red
    Write-Host "    -> FIX: Open PowerShell as Administrator and run:" -ForegroundColor Yellow
    Write-Host "       bcdedit /set testsigning on" -ForegroundColor White
    Write-Host "       Then reboot your computer." -ForegroundColor Yellow
}

# 3. Check Certificate in Local Machine Stores
$certThumbprint = "F41A2363730CE778BA094C04A0D9D1EFA0F6C6BE"
$rootCert = Get-ChildItem Cert:\LocalMachine\Root -ErrorAction SilentlyContinue | Where-Object { $_.Thumbprint -eq $certThumbprint }
$pubCert = Get-ChildItem Cert:\LocalMachine\TrustedPublisher -ErrorAction SilentlyContinue | Where-Object { $_.Thumbprint -eq $certThumbprint }

if ($rootCert -and $pubCert) {
    Write-Host "[+] WDK Test Certificate: INSTALLED in Root and TrustedPublisher" -ForegroundColor Green
} else {
    Write-Host "[-] WDK Test Certificate: MISSING from Machine Certificate Store" -ForegroundColor Red
    Write-Host "    -> FIX: Run with Administrator privileges:" -ForegroundColor Yellow
    Write-Host "       certutil -addstore `"Root`" `"$PSScriptRoot\driver\awus036achm.cer`"" -ForegroundColor White
    Write-Host "       certutil -addstore `"TrustedPublisher`" `"$PSScriptRoot\driver\awus036achm.cer`"" -ForegroundColor White
}

Write-Host ""
Write-Host "=================================================================" -ForegroundColor Cyan
if (!$secureBootEnabled -and $testSigningEnabled -and $rootCert -and $pubCert) {
    Write-Host " STATUS: System is FULLY READY to load awus036achm.sys!" -ForegroundColor Green
} else {
    Write-Host " STATUS: Follow the FIX steps above before installing awus036achm.inf" -ForegroundColor Yellow
}
Write-Host "=================================================================" -ForegroundColor Cyan
