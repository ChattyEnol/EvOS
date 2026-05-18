# ./Evoncil/Evoncil.ps1

Push-Location $PSScriptRoot

$platform = "X64"
$target = "x86_64-unknown-windows"
$outDirectory = "../Bin/EFI/Evoncil"
$outFilename = "Evoncil.EFI"

$tempDirectory = "./.Temp"
$tempLibraryName = "EvNoyau.lib"

if (Test-Path $tempDirectory) { Remove-Item -Path $tempDirectory -Recurse -Force }
if (!(Test-Path $outDirectory)) { New-Item -ItemType Directory -Path $outDirectory -Force }
if (!(Test-Path $tempDirectory)) { New-Item -ItemType Directory -Path $tempDirectory -Force }

$stub_include = @(
    "-I", "../EFI/Include",
    "-I", "../EFI/Include/$platform",
    "-I", "../EFI/Evoncil/Include"
)

$cmpl_args = @(
    "-target", $target,
    "-ffreestanding",
    "-fno-stack-protector",
    "-mno-red-zone",
    "-O3",
    "-I", "..",
    "-I", "./Include",
    "-I", "./Library/Include"
)

$stub_source = @(
    "../EFI/Evoncil/*.c",
    "../EFI/Evoncil/Sources/*.c"
)

$sources = @(
    "./*.c",
    "./Sources/HAL/$platform/*.c",
    "./Sources/HAL/$platform/*.s",
    "./Sources/Noyau/*.c",
    "./Sources/User/*.c",
    "./Sources/Drivers/*.c",
    "./Sources/File/*.c",
    "./Sources/UI/*.c",
    "./Library/Sources/*.c"
)

$link_args = @(
    "/entry:EvStub",
    "/subsystem:efi_application",
    "/nodefaultlib",
    "/out:$outDirectory/$outFilename",
    "$tempDirectory/*.o",
    "$tempDirectory/$tempLibraryName"
)

Write-Host "[*] Compiling EvKernel..." -ForegroundColor Magenta
& clang $cmpl_args -c $sources
if ($LASTEXITCODE -ne 0) {
    Write-Host "[!] EvKernel compilation failed!" -ForegroundColor Red
    Remove-Item ./*.o -Recurse -Force
    exit $LASTEXITCODE
}
Move-Item ./*.o "$tempDirectory/" -Force

Write-Host "[*] Archiving EvKernel..." -ForegroundColor Magenta
& llvm-ar rc "$tempDirectory/$tempLibraryName" "$tempDirectory/*.o"
if ($LASTEXITCODE -ne 0) {
    Write-Host "[!] EvKernel library archiving failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "[*] Compiling Stub..." -ForegroundColor Magenta
& clang $cmpl_args $stub_include -c $stub_source
if ($LASTEXITCODE -ne 0) {
    Write-Host "[!] Stub compilation failed!" -ForegroundColor Red
    Remove-Item ./*.o -Recurse -Force
    exit $LASTEXITCODE
}
Move-Item ./*.o "$tempDirectory/" -Force

Write-Host "[*] Linking Evoncil..." -ForegroundColor Magenta
& lld-link $link_args
if ($LASTEXITCODE -ne 0) {
    Write-Host "[!] Evoncil linking failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}

Remove-Item -Path $tempDirectory -Recurse -Force

Write-Host "[*] Signing..." -ForegroundColor White
$cert = Get-ChildItem Cert:\LocalMachine\My | Where-Object { $_.FriendlyName -eq "EvOS Certificate" }
Set-AuthenticodeSignature -FilePath "$outDirectory/$outFilename" -Certificate $cert
if ($LASTEXITCODE -ne 0) {
    Write-Host "[!] Signing failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}

Pop-Location
