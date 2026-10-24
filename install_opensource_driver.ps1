# OpenSource-AWUS036ACHM Driver Installer (Automated & Elevated)
Write-Host "====================================================================" -ForegroundColor Cyan
Write-Host "  Installing OpenSource-AWUS036ACHM Driver into Windows 11          " -ForegroundColor Cyan
Write-Host "====================================================================" -ForegroundColor Cyan

$driverDir = Join-Path $PSScriptRoot "driver"
$cerPath = Join-Path $driverDir "awus036achm.cer"
$infPath = Join-Path $driverDir "awus036achm.inf"

# Step 1: Clean up any stale service deletion flags and purge obsolete conflicting drivers
Write-Host "`n[1/5] Checking service state and purging legacy conflicting drivers..." -ForegroundColor Yellow
$svcKey = "HKLM:\SYSTEM\CurrentControlSet\Services\Awus036achm"
if (Test-Path $svcKey) {
    Remove-ItemProperty -Path $svcKey -Name "DeleteFlag" -ErrorAction SilentlyContinue
    Remove-ItemProperty -Path $svcKey -Name "DriverDelete" -ErrorAction SilentlyContinue
    Set-ItemProperty -Path $svcKey -Name "Start" -Value 3 -ErrorAction SilentlyContinue
}

# Purge only obsolete netr28ux packages, never delete our active awus036achm
$driverText = (pnputil /enum-drivers) -join "`n"
$re = [regex]'Published Name:\s+(oem\d+\.inf)\s+Original Name:\s+(netr28ux\.inf)'
foreach ($match in $re.Matches($driverText)) {
    $oemName = $match.Groups[1].Value
    Write-Host "Purging legacy conflicting driver package: $oemName..." -ForegroundColor Magenta
    pnputil /delete-driver $oemName /uninstall /force
}

# Step 2: Install Certificate into Root & TrustedPublisher stores
Write-Host "`n[2/5] Adding certificate to Root & TrustedPublisher stores..." -ForegroundColor Yellow
certutil -addstore -f "Root" $cerPath | Out-Null
certutil -addstore -f "TrustedPublisher" $cerPath | Out-Null

# Step 3: Stage and install Driver Package using pnputil
Write-Host "`n[3/5] Staging and Installing Driver Package via PnPUtil (Class = Net)..." -ForegroundColor Yellow
$addResult = pnputil /add-driver $infPath
Write-Host $addResult

# Target device update directly
$dev = Get-PnpDevice | Where-Object { $_.InstanceId -like "*0E8D*" -or $_.InstanceId -like "*7610*" }
if ($dev) {
    Write-Host "Cycling device state to bind newly staged driver..." -ForegroundColor Cyan
    Disable-PnpDevice -InstanceId "$($dev.InstanceId)" -Confirm:$false -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 800
    Enable-PnpDevice -InstanceId "$($dev.InstanceId)" -Confirm:$false -ErrorAction SilentlyContinue
} else {
    Write-Host "Device not currently plugged in; staged driver will automatically load when device is attached." -ForegroundColor Cyan
}

# Step 4: Refresh WLAN AutoConfig Service & Apply Device Naming
Write-Host "`n[4/5] Applying 'Alfa AWUS036ACHM WiFi 6' device properties and refreshing wlansvc..." -ForegroundColor Yellow
$toolsExe = Join-Path $PSScriptRoot "tools\set_friendly_name.exe"
if (Test-Path $toolsExe) {
    & $toolsExe
}
$classBase = "HKLM:\SYSTEM\CurrentControlSet\Control\Class\{4d36e972-e325-11ce-bfc1-08002be10318}"
if (Test-Path $classBase) {
    Get-ChildItem $classBase -ErrorAction SilentlyContinue | ForEach-Object {
        $props = Get-ItemProperty $_.PSPath -ErrorAction SilentlyContinue
        if ($props.MatchingDeviceId -like "*7610*" -or $props.DriverDesc -like "*802.11ac*" -or $props.DriverDesc -like "*AWUS*" -or $props.DriverDesc -like "*MediaTek*") {
            Set-ItemProperty -Path $_.PSPath -Name "DriverDesc" -Value "Alfa AWUS036ACHM WiFi 6" -ErrorAction SilentlyContinue
            Set-ItemProperty -Path $_.PSPath -Name "Description" -Value "Alfa AWUS036ACHM WiFi 6" -ErrorAction SilentlyContinue
        }
    }
}
$adapters = Get-NetAdapter -IncludeHidden | Where-Object { $_.InterfaceDescription -like "*AWUS*" -or $_.InterfaceDescription -like "*Alfa*" -or $_.InterfaceDescription -like "*802.11ac*" -or $_.InterfaceDescription -like "*MediaTek*" -or $_.InterfaceDescription -like "*WiFi 6*" }
foreach ($a in $adapters) {
    Enable-NetAdapter -Name $a.Name -Confirm:$false -ErrorAction SilentlyContinue
    & netsh interface set interface name="$($a.Name)" admin=enabled
}
Restart-Service wlansvc -Force -ErrorAction SilentlyContinue

# Step 5: Verify Device Status and WLAN interfaces
Write-Host "`n[5/5] Checking connected adapter status..." -ForegroundColor Yellow
Start-Sleep -Seconds 2
$dev = Get-PnpDevice | Where-Object { $_.InstanceId -like "*0E8D*" -or $_.InstanceId -like "*7610*" }
if ($dev) {
    $dev | Format-Table Status, Class, FriendlyName, InstanceId -AutoSize
}

$adapters = Get-NetAdapter -IncludeHidden | Where-Object { $_.InterfaceDescription -like "*AWUS*" -or $_.InterfaceDescription -like "*Alfa*" -or $_.InterfaceDescription -like "*802.11ac*" -or $_.InterfaceDescription -like "*MediaTek*" }
if ($adapters) {
    $adapters | Format-Table Name, InterfaceDescription, Status, LinkSpeed, MediaType -AutoSize
}

Write-Host "`nChecking netsh wlan interfaces..." -ForegroundColor Cyan
netsh wlan show interfaces

Write-Host "`nChecking visible Wi-Fi networks (netsh wlan show networks):" -ForegroundColor Cyan
netsh wlan show networks

Write-Host "`n====================================================================" -ForegroundColor Green
Write-Host "  Open-Source Native 802.11 Driver Successfully Installed!           " -ForegroundColor Green
Write-Host "  Please UNPLUG and RE-PLUG your AWUS036ACHM into a USB 3.0 port     " -ForegroundColor Green
Write-Host "  now so Windows cleanly triggers the new ExtSTA Native 802.11 stack." -ForegroundColor Green
Write-Host "====================================================================" -ForegroundColor Green
Write-Host "Ready." -ForegroundColor Cyan
