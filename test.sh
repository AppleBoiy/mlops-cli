#!/bin/bash

# test.sh - Automated test suite for the mops CLI
# Designed for Ubuntu Server

set -euo pipefail

# --- Color Constants ---
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# --- OS Check ---
if [ "$(uname -s)" != "Linux" ]; then
    echo -e "${RED}Error: This test script must be run on Linux (Ubuntu). macOS is not supported.${NC}"
    exit 1
fi

# --- Helper Functions ---
run_test() {
    local cmd="$1"
    local expected_code="${2:-0}"

    echo -n -e "Testing: ${YELLOW}${cmd}${NC} ... "

    set +e
    # Run the command, capturing stdout and stderr
    local output
    output=$(eval "$cmd" 2>&1)
    local exit_code=$?
    set -e

    if [ "$exit_code" -eq "$expected_code" ]; then
        echo -e "${GREEN}PASS${NC}"
    else
        echo -e "${RED}FAIL (Expected exit code $expected_code, got $exit_code)${NC}"
        echo -e "Output:\n$output"
        exit 1
    fi
}

run_test_allow_fail() {
    local cmd="$1"

    echo -n -e "Testing (Permitted to fail due to env/root reqs): ${YELLOW}${cmd}${NC} ... "

    set +e
    local output
    output=$(eval "$cmd" 2>&1)
    local exit_code=$?
    set -e

    if [[ "$exit_code" -eq 0 || "$exit_code" -eq 1 ]]; then
        echo -e "${GREEN}PASS${NC} (Exit Code: $exit_code)"
    else
        echo -e "${RED}FAIL (Got unexpected exit code $exit_code)${NC}"
        echo -e "Output:\n$output"
        exit 1
    fi
}

echo "========================================"
echo " Starting mops CLI Test Suite"
echo "========================================"

# 1. Build the binary in DEV_MODE to ensure the task command is enabled
echo "=> Building mops..."
make clean > /dev/null
make dev > /dev/null
echo -e "${GREEN}Build successful.${NC}\n"

# Clean up previous databases for a fresh state
rm -f mops.db /tmp/mops_bg.log /tmp/mops_task_*.log

# ==========================================
# 2. Test Base Command & Help Flags
# ==========================================
echo "=> Testing Base & Help Commands"
run_test "./mops" 1
run_test "./mops --help" 0
run_test "./mops -h" 0
run_test "./mops invalid_command" 1
echo ""

# ==========================================
# 3. Test Disk Operations
# ==========================================
echo "=> Testing Disk Operations (mops disk)"
run_test "./mops disk" 1
run_test "./mops disk --help" 0

# Status Subcommand
run_test "./mops disk status" 0
run_test "./mops disk status -h" 0
run_test "./mops disk status -l" 0
run_test "./mops disk status -h -l" 0

# Usage Subcommand
run_test "./mops disk usage" 0
run_test "./mops disk usage -h" 0
run_test "./mops disk usage -l" 0
run_test "./mops disk usage -h -l" 0

# Mounts Subcommand
run_test "./mops disk mounts" 0
run_test "./mops disk mounts -l" 0

# Invalid Subcommand
run_test "./mops disk invalid" 1
echo ""

# ==========================================
# 4. Test System & Hardware Metrics
# ==========================================
echo "=> Testing System Operations (mops sys)"
run_test "./mops sys" 1
run_test "./mops sys --help" 0

# CPU Subcommand
run_test "./mops sys cpu" 0
run_test "./mops sys cpu -h" 0
run_test "./mops sys cpu -l" 0
run_test "./mops sys cpu -h -l" 0

# GPU Subcommand (Will return 0 even if no GPU, gracefully stating so)
run_test "./mops sys gpu" 0
run_test "./mops sys gpu -h" 0
run_test "./mops sys gpu -l" 0
run_test "./mops sys gpu -h -l" 0
run_test "./mops sys gpu --pids" 0

# TPU Subcommand (Will return 0 gracefully if no TPU)
run_test "./mops sys tpu" 0
run_test "./mops sys tpu -h" 0
run_test "./mops sys tpu -l" 0

# OOM Subcommand (dmesg requires root sometimes)
run_test_allow_fail "./mops sys oom"

# Cgroup Subcommand (cgroups missing or non-root context)
run_test_allow_fail "./mops sys cgroup"
echo ""

# ==========================================
# 5. Test Network Operations
# ==========================================
echo "=> Testing Network Operations (mops net)"
run_test "./mops net" 1
run_test "./mops net --help" 0

# Port Subcommand
run_test "./mops net port" 1
run_test "./mops net port --help" 0
run_test "./mops net port 22" 0
run_test "./mops net port 80" 0
run_test "./mops net port 999999" 1 # Invalid port range
echo ""

# ==========================================
# 6. Test Task Management
# ==========================================
echo "=> Testing Task Operations (mops task)"
run_test "./mops task" 1
run_test "./mops task --help" 0

# Exec Subcommand
run_test "./mops task exec 'echo \"Sync test\"'" 0
run_test "./mops task exec 'false'" 1 # Command fails

# Background Subcommand
run_test "./mops task bg 'echo \"Async test\" && sleep 1'" 0

# Queue Subcommand
run_test "./mops task queue 'echo \"Queue test 1\"'" 0
run_test "./mops task queue 'echo \"Queue test 2\"'" 0
run_test "./mops task queue --exec" 0

# List Subcommand
run_test "./mops task list" 0

# Logs Subcommand (Task 3 is our bg test, which creates a log file)
sleep 0.5 # Give the bg task a moment to initialize the log file
run_test "./mops task logs 3" 0
run_test "./mops task logs 9999" 1 # Log file won't exist

# Clean Subcommand
run_test "./mops task clean" 0
run_test "./mops task clean --force" 0

# Kill Subcommand
# Spawn a long-running task to guarantee it's alive to kill
run_test "./mops task bg 'sleep 100'" 0
run_test "./mops task kill 6" 0

# Invalid task operations
run_test "./mops task kill invalid" 1
echo ""

echo "========================================"
echo -e "${GREEN}ALL TESTS PASSED SUCCESSFULLY!${NC}"
echo "========================================"

# Cleanup
rm -f mops.db /tmp/mops_bg.log /tmp/mops_task_*.log
