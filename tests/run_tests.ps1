# Builds and runs all host (PC) unit tests (mesh core and radio driver).
# Usage, from the repository root:  .\tests\run_tests.ps1
# Requires gcc on PATH (for example MSYS2 UCRT64).

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$core  = @("core/crc16.c", "core/packet.c", "core/dedup.c", "core/mesh.c", "radio/sx1276.c")
$tests = @("test_packet", "test_crc16", "test_packet_edge", "test_mesh", "test_sx1276_regs", "test_sx1276")
$failed = 0

foreach ($t in $tests) {
    $exe = "tests/$t.exe"
    & gcc -std=c11 -Wall -Wextra -Werror -Icore -Iradio @core "tests/$t.c" -o $exe
    if ($LASTEXITCODE -ne 0) {
        Write-Host "BUILD FAILED: $t" -ForegroundColor Red
        $failed++
        continue
    }
    & ".\$exe"
    if ($LASTEXITCODE -ne 0) { $failed++ }
}

if ($failed -eq 0) {
    Write-Host "All test suites passed" -ForegroundColor Green
    exit 0
}
Write-Host "$failed test suite(s) failed" -ForegroundColor Red
exit 1
