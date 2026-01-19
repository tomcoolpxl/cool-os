#!/bin/bash
#
# Regression Test Runner for cool-os
#
# Runs the kernel in QEMU with isa-debug-exit device and parses results.
# Exit codes:
#   0 = All tests passed
#   1 = Tests failed or timeout
#
# Usage: ./scripts/run_regtest.sh [options]
# Options are passed through to QEMU (e.g., -d int for debug)

set -e

# Configuration
TIMEOUT=${REGTEST_TIMEOUT:-60}
BUILD_DIR="build"
DIST_DIR="${BUILD_DIR}/dist"
IMG="${DIST_DIR}/cool-os-regtest.img"
OVMF_CODE="${OVMF_CODE:-/usr/share/edk2/x64/OVMF_CODE.4m.fd}"
OVMF_VARS="${BUILD_DIR}/OVMF_VARS.4m.fd"
LOG_FILE="${BUILD_DIR}/regtest.log"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if image exists
if [ ! -f "${IMG}" ]; then
    echo -e "${RED}Error: Kernel image not found: ${IMG}${NC}"
    echo "Run 'make regtest-build' first."
    exit 1
fi

# Copy OVMF_VARS if needed
if [ ! -f "${OVMF_VARS}" ]; then
    if [ -f "/usr/share/edk2/x64/OVMF_VARS.4m.fd" ]; then
        cp /usr/share/edk2/x64/OVMF_VARS.4m.fd "${OVMF_VARS}"
    else
        echo -e "${RED}Error: OVMF_VARS not found${NC}"
        exit 1
    fi
fi

echo "Running cool-os regression tests..."
echo "Image: ${IMG}"
echo "Timeout: ${TIMEOUT}s"
echo "Log: ${LOG_FILE}"
echo ""

# Run QEMU with debug exit device, capture output
set +e  # Don't exit on non-zero

timeout ${TIMEOUT} qemu-system-x86_64 \
    -enable-kvm \
    -cpu host \
    -m 256M \
    -no-reboot \
    -device isa-debug-exit,iobase=0x501,iosize=1 \
    -drive if=pflash,format=raw,readonly=on,file="${OVMF_CODE}" \
    -drive if=pflash,format=raw,file="${OVMF_VARS}" \
    -drive format=raw,file="${IMG}" \
    -display none \
    -serial stdio \
    "$@" 2>&1 | tee "${LOG_FILE}"

EXIT_CODE=${PIPESTATUS[0]}

echo ""
echo "========================================"

# Parse results from log
PASSED=$(grep -c "\[REGTEST\] PASS" "${LOG_FILE}" 2>/dev/null || echo "0")
FAILED=$(grep -c "\[REGTEST\] FAIL" "${LOG_FILE}" 2>/dev/null || echo "0")

# Check for FAIL messages
if grep -q "\[REGTEST\] FAIL" "${LOG_FILE}"; then
    echo -e "${RED}REGRESSION TESTS FAILED${NC}"
    echo ""
    echo "Failed tests:"
    grep "\[REGTEST\] FAIL" "${LOG_FILE}" | sed 's/\[REGTEST\] /  /'
    echo ""
    echo "Summary: ${PASSED} passed, ${FAILED} failed"
    exit 1
fi

# Check QEMU exit code
# isa-debug-exit: exit code = (value << 1) | 1
# value 0x00 -> exit 1 (success)
# value 0x01 -> exit 3 (failure)
if [ ${EXIT_CODE} -eq 1 ]; then
    echo -e "${GREEN}REGRESSION TESTS PASSED${NC}"
    echo "Summary: ${PASSED} passed, ${FAILED} failed"
    exit 0
elif [ ${EXIT_CODE} -eq 3 ]; then
    echo -e "${RED}REGRESSION TESTS FAILED${NC}"
    echo "Summary: ${PASSED} passed, ${FAILED} failed"
    exit 1
elif [ ${EXIT_CODE} -eq 124 ]; then
    echo -e "${YELLOW}REGRESSION TESTS TIMED OUT${NC}"
    echo "The kernel did not exit within ${TIMEOUT} seconds."
    echo "This may indicate an infinite loop or hang."
    exit 1
else
    echo -e "${YELLOW}UNEXPECTED EXIT CODE: ${EXIT_CODE}${NC}"
    echo "Summary: ${PASSED} passed, ${FAILED} failed"
    # Check if we got a summary line
    if grep -q "\[REGTEST\] SUMMARY" "${LOG_FILE}"; then
        # Tests ran but QEMU exited unexpectedly
        if [ "${FAILED}" -eq 0 ] && [ "${PASSED}" -gt 0 ]; then
            echo -e "${GREEN}Tests appear to have passed${NC}"
            exit 0
        fi
    fi
    exit 1
fi
