#!/bin/sh
# Builds and runs all host (PC) unit tests. Usage: sh tests/run_tests.sh
# Used by GitHub Actions; on Windows use tests/run_tests.ps1.
set -u
cd "$(dirname "$0")/.." || exit 1

CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -Wextra -Werror -Icore -Iradio"
CORE="core/crc16.c core/packet.c core/dedup.c core/mesh.c"
TESTS="test_packet test_crc16 test_packet_edge test_mesh test_sx1276_regs"
OUT=${TMPDIR:-/tmp}/lora-mesh-tests
mkdir -p "$OUT"

failed=0
for t in $TESTS; do
    if ! $CC $CFLAGS $CORE "tests/$t.c" -o "$OUT/$t"; then
        echo "BUILD FAILED: $t"
        failed=$((failed + 1))
        continue
    fi
    "$OUT/$t" || failed=$((failed + 1))
done

if [ "$failed" -eq 0 ]; then
    echo "All test suites passed"
    exit 0
fi
echo "$failed test suite(s) failed"
exit 1
