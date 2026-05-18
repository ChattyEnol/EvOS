if (-not 
    ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)) {
    $scriptPath = $MyInvocation.MyCommand.Definition
    Start-Process PowerShell.exe -Verb RunAs -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$scriptPath`""
    Exit
}

Push-Location $PSScriptRoot

# 1. 编译
if (Test-Path "./Bin") { Remove-Item -Path "./Bin" -Recurse -Force }
Push-Location "$PSScriptRoot/EFI"; . ./Loader.ps1;
if ($LASTEXITCODE -ne 0) {
    Write-Host "[!] Loader generation failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}
Pop-Location

Push-Location "$PSScriptRoot/Evoncil"; . ./Evoncil.ps1;
if ($LASTEXITCODE -ne 0) {
    Write-Host "[!] Evoncil generation failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}
Pop-Location

# 2. 构建 VHDX

$vhdPath = Join-Path $PSScriptRoot "EvOS.vhdx"
$vhdSize = 64MB

Write-Host "[*] Creating VHDX: $vhdPath" -ForegroundColor Cyan

if (Test-Path $vhdPath) { 
    Write-Host "[!] Removing old VHDX..." -ForegroundColor Gray
    Remove-Item $vhdPath -Force 
}

Write-Host "[*] Creating new VHDX..." -ForegroundColor Gray
$vhd = New-VHD -Path $vhdPath -SizeBytes $vhdSize -Dynamic -Confirm:$false

Write-Host "[*] Mounting and formatting disk..." -ForegroundColor Yellow
$disk = $vhd | Mount-VHD -Passthru
$disk | Initialize-Disk -PartitionStyle GPT -PassThru |
New-Partition -AssignDriveLetter -UseMaximumSize |
Format-Volume -FileSystem FAT32 -NewFileSystemLabel "EVOS" -Confirm:$false | Out-Null

Write-Host "[*] Copying files to VHDX..." -ForegroundColor White
$drive = (Get-Disk $disk.Number | Get-Partition | Where-Object DriveLetter).DriveLetter
Copy-Item -Path "./Bin/*" -Destination "$($drive):/" -Recurse -Force

Write-Host "[*] Dismounting VHDX..." -ForegroundColor Yellow
Dismount-VHD -Path $vhdPath

Write-Host "[+] Done!" -ForegroundColor Green
Pop-Location