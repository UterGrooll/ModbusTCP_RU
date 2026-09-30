param(
    [string]$Compiler = 'g++',
    [string]$BuildPath = (Join-Path ([System.IO.Path]::GetTempPath()) ('modbus-tests-' + [guid]::NewGuid()))
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$hostPath = Join-Path $PSScriptRoot 'host'
$src = Join-Path $root 'src'
New-Item -ItemType Directory -Path $BuildPath -Force | Out-Null
$common = @('-std=c++11', '-Wall', '-Wextra', '-Werror', '-I', $hostPath, '-I', $src)
foreach ($profile in @('small', 'full', 'small64')) {
    $flags = @($common)
    if ($profile -ne 'full') { $flags += '-DARDUINO_ARCH_AVR' }
    if ($profile -eq 'small64') { $flags += '-DMB_BUFFER_SIZE=64' }
    $exe = Join-Path $BuildPath ($profile + '.exe')
    & $Compiler @flags (Join-Path $hostPath 'test_modbus.cpp') (Join-Path $src 'ModbusTCP_RU.cpp') -o $exe
    if ($LASTEXITCODE -ne 0) { throw "Compile failed: $profile" }
    Write-Output "PROFILE: $profile"
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "Tests failed: $profile" }
}
$probe = Join-Path $hostPath 'compile_probe.cpp'
$output = (& $Compiler @common -DARDUINO_ARCH_AVR -DMB_MAX_HOLDING=64 -fsyntax-only $probe 2>&1 | Out-String)
if ($LASTEXITCODE -eq 0 -or $output -notmatch 'does not fit MB_BUFFER_SIZE') { throw 'Unsafe buffer configuration was not diagnosed' }
Write-Output 'PASS unsafe buffer configuration rejected'
$output = (& $Compiler @common -DARDUINO_ARCH_AVR -DTEST_ADDRESS_CALLBACK -fsyntax-only $probe 2>&1 | Out-String)
if ($LASTEXITCODE -eq 0 -or $output -notmatch 'deleted') { throw 'Address callback overload was not diagnosed' }
Write-Output 'PASS obsolete address callback rejected'
$output = (& $Compiler @common -DARDUINO_ARCH_AVR (Join-Path $hostPath 'config_mismatch.cpp') (Join-Path $src 'ModbusTCP_RU.cpp') -o (Join-Path $BuildPath 'mismatch.exe') 2>&1 | Out-String)
if ($LASTEXITCODE -eq 0 -or $output -notmatch 'undefined reference.*ModbusTCP_RU') { throw "Configuration mismatch not diagnosed: $output" }
Write-Output 'PASS per-sketch layout mismatch rejected at link'
Write-Output "Build outputs: $BuildPath"
