#!/bin/bash

# Docker Health Check Script for Timezone Server
# Tests if the UDP server is responding correctly

# Configuration
HOST="127.0.0.1"
PORT="${UDP_PORT:-2342}"
TIMEOUT=5

# Check if nc is available
if ! command -v nc >/dev/null 2>&1; then
    echo "Error: netcat (nc) not available in container" >&2
    exit 1
fi

# Function to test UDP server
test_healthcheck() {
    local test_type="${1:-basic}"
    
    if [ "$test_type" = "full" ]; then
        # Full health check - tests all major functions
        response=$(echo "HEALTHCHECK FULL" | timeout $TIMEOUT nc -u -w1 $HOST $PORT 2>/dev/null)
        expected_pattern="^OK FULL ALL_TESTS_PASSED$"
    else
        # Basic health check - tests core functions (Europe/Berlin + DE)
        response=$(echo "HEALTHCHECK" | timeout $TIMEOUT nc -u -w1 $HOST $PORT 2>/dev/null)
        expected_pattern="^OK CORE_FUNCTIONS_WORKING$"
    fi
    
    local nc_exit_code=$?
    
    if [ $nc_exit_code -eq 0 ] && echo "$response" | grep -q "$expected_pattern"; then
        return 0
    else
        # Log failure for debugging (only in case of failure)
        echo "Health check failed:" >&2
        echo "  Command exit code: $nc_exit_code" >&2
        echo "  Response: '$response'" >&2
        echo "  Expected pattern: '$expected_pattern'" >&2
        echo "  Test type: $test_type" >&2
        echo "  Target: $HOST:$PORT" >&2
        
        # Additional debugging
        if command -v ps >/dev/null 2>&1; then
            echo "  PHP processes:" >&2
            ps aux | grep php | grep -v grep >&2 || echo "    No PHP processes found" >&2
        fi
        
        return 1
    fi
}

# Run the health check
# Use "full" as argument for comprehensive testing, or leave empty for basic testing
TEST_TYPE="${1:-basic}"

if test_healthcheck "$TEST_TYPE"; then
    # Server is healthy
    exit 0
else
    # Server is unhealthy
    exit 1
fi