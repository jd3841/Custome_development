<#
Usage: Run from project root in PowerShell:
  ./build_and_run.ps1

This script detects `gcc`, `clang`, or MSVC `cl`. If found it will attempt
to compile `main.c` and `sip_communication.c` and run the resulting binary.
If no compiler is present it prints concise install instructions for Windows.
#>

Write-Host "Detecting C compilers..."

$gcc = Get-Command gcc -ErrorAction SilentlyContinue
$clang = Get-Command clang -ErrorAction SilentlyContinue
$cl = Get-Command cl -ErrorAction SilentlyContinue

if ($gcc) {
    Write-Host "Found gcc at:" $gcc.Path
    gcc main.c sip_communication.c -o main.exe
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Build succeeded. Running main.exe (press q to quit)"
        echo q | .\main.exe
        exit 0
    }
    Write-Host "gcc failed with exit code" $LASTEXITCODE
    exit $LASTEXITCODE
} elseif ($clang) {
    Write-Host "Found clang at:" $clang.Path
    clang main.c sip_communication.c -o main.exe
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Build succeeded. Running main.exe (press q to quit)"
        echo q | .\main.exe
        exit 0
    }
    Write-Host "clang failed with exit code" $LASTEXITCODE
    exit $LASTEXITCODE
} elseif ($cl) {
    Write-Host "Found MSVC cl at:" $cl.Path
    Write-Host "Note: run this script from a Developer Command Prompt for full environment."
    cl /nologo /Fe:main.exe main.c sip_communication.c
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Build succeeded. Running main.exe (press q to quit)"
        .\main.exe
        exit 0
    }
    Write-Host "cl failed with exit code" $LASTEXITCODE
    exit $LASTEXITCODE
} else {
    Write-Host "No C compiler found on PATH. Quick install options:"
    Write-Host "- Install MSYS2 + MinGW-w64 (recommended for GCC):"
    Write-Host "  1) Visit https://www.msys2.org/ and run the installer."
    Write-Host "  2) Open 'MSYS2 MinGW 64-bit' shell and run:"
    Write-Host "     pacman -Syu"
    Write-Host "     pacman -S --needed base-devel mingw-w64-x86_64-toolchain"
    Write-Host "  3) Add the MinGW bin directory to PATH or use the MSYS2 shell to build."
    Write-Host "- Or enable WSL (Ubuntu) and install build-essential:"
    Write-Host "  sudo apt update && sudo apt install build-essential"
    Write-Host "- Or install Visual Studio with 'Desktop development with C++' workload"
    Write-Host "After installing, re-run this script or open a new shell where the compiler is available."
    exit 2
}
