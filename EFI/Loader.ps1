# EFI/Build.ps1

Push-Location $PSScriptRoot

$platform = "X64"
$target = "x86_64-unknown-windows"
$outDirectory = "../Bin/EFI/BOOT"
$outFilename = "BOOTX64.EFI"
if (!(Test-Path $outDirectory)) { New-Item -ItemType Directory -Path $outDirectory -Force }

# 一个测试，我们减少一个参数看看到底有没有问题。
# "-fshort-wchar",
$cmpl_args = @(
    "-target", $target,
    "-ffreestanding",
    "-fno-stack-protector",
    "-mno-red-zone",
    "-O3",
    "-I", "./Include",
    "-I", "./Include/$platform",
    "-I", "./BOOT/Library/Include",
    "-I", "./BOOT/Library/Include/$platform",
    "-I", "./BOOT/Include",
    "-I", ".."
)

$link_args = @(
    "-Wl,-entry:EvLoader",
    "-Wl,-subsystem:efi_application",
    "-Wl,-nodefaultlib",
    "-fuse-ld=lld"
)

$source = @(
    "./BOOT/*.c",
    "./BOOT/Sources/*.c",
    "./BOOT/Library/Sources/*.c"
)

Write-Host "[*] Compiling Loader..." -ForegroundColor Cyan
clang $cmpl_args $link_args $source -o "$outDirectory/$outFilename"
if ($LASTEXITCODE -ne 0) {
    Write-Host "[!] Loader compilation failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "[*] Signing..." -ForegroundColor White
$cert = Get-ChildItem Cert:\LocalMachine\My | Where-Object { $_.FriendlyName -eq "EvOS Certificate" }
Set-AuthenticodeSignature -FilePath "$outDirectory/$outFilename" -Certificate $cert
if ($LASTEXITCODE -ne 0) {
    Write-Host "[!] Signing failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}

Pop-Location