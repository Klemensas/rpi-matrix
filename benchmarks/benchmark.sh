#!/bin/bash
# =============================================================================
# RPI-MATRIX BENCHMARK - Full Pipeline Performance Testing
# =============================================================================
#
# Tests the complete rendering pipeline: Camera → Effect Processing → LED Matrix
#
# Usage:
#   sudo ./benchmarks/benchmark.sh              Run full benchmark suite
#   sudo ./benchmarks/benchmark.sh --save       Save results as baseline
#   sudo ./benchmarks/benchmark.sh --check      Check for regressions (CI mode)
#   sudo ./benchmarks/benchmark.sh --quick      Quick test (fewer effects)
#
# What it tests:
#   - All 10 effects in EXTEND mode (default)
#   - All 10 effects in REPEAT mode (multi-view)
#   - Panel mode switching performance
#
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "${SCRIPT_DIR}")"
BUILD_DIR="${PROJECT_DIR}/build"
APP_BIN="${BUILD_DIR}/camera_to_matrix"
BASELINE_FILE="${SCRIPT_DIR}/baseline.json"
RESULTS_FILE="${SCRIPT_DIR}/results.json"

# Hardware configuration - adjust to match your setup
WIDTH=576
HEIGHT=192
SENSOR_WIDTH=4608
SENSOR_HEIGHT=2592
LED_CHAIN=3

# Benchmark configuration
SECONDS_PER_EFFECT=6      # How long to test each effect
WARMUP_SECONDS=2          # Initial warmup before measuring
REGRESSION_THRESHOLD=15   # Percentage drop that triggers failure

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# Effect definitions
declare -a EFFECTS=(
    "1:Debug (Pass-through)"
    "2:Filled Silhouette"
    "3:Outline Only"
    "4:Motion Trails"
    "5:Rainbow Motion Trails"
    "6:Double Exposure"
    "7:Procedural Shapes"
    "8:Wave Patterns"
    "9:Geometric Abstraction"
    "10:Mandelbrot Root Veins"
)

# Quick mode - test subset of effects
declare -a QUICK_EFFECTS=(
    "1:Debug (Pass-through)"
    "2:Filled Silhouette"
    "5:Rainbow Motion Trails"
    "7:Procedural Shapes"
    "8:Wave Patterns"
    "10:Mandelbrot Root Veins"
)

print_header() {
    echo -e "${CYAN}"
    echo "╔════════════════════════════════════════════════════════════════════╗"
    echo "║              RPI-MATRIX FULL PIPELINE BENCHMARK                    ║"
    echo "║      Camera Capture → Effect Processing → LED Matrix Output        ║"
    echo "╚════════════════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
}

check_root() {
    if [ "$EUID" -ne 0 ]; then
        echo -e "${RED}[ERROR]${NC} This benchmark requires root for LED matrix access."
        echo "        Run with: sudo $0 $*"
        exit 1
    fi
}

check_binary() {
    if [ ! -f "${APP_BIN}" ]; then
        echo -e "${YELLOW}[BUILD]${NC} Building camera_to_matrix..."
        mkdir -p "${BUILD_DIR}"
        cd "${BUILD_DIR}"
        cmake .. -DBUILD_RPI=ON
        make camera_to_matrix -j$(nproc)
        cd "${PROJECT_DIR}"
    fi
}

# Send a key to the running application
send_key() {
    local key=$1
    echo "${key}" >&3
    sleep 0.3
}

# Extract average FPS from output file since a given line
extract_fps() {
    local tmpfile=$1
    local start_line=$2
    
    # Match "FPS: X" format from application output
    local fps_values=$(tail -n +$((start_line + 1)) "${tmpfile}" | grep -oP 'FPS: \K[0-9]+' | tail -n +2)
    
    local sum=0
    local count=0
    for fps in $fps_values; do
        sum=$((sum + fps))
        count=$((count + 1))
    done
    
    if [ $count -gt 0 ]; then
        echo $((sum / count))
    else
        echo "0"
    fi
}

run_benchmark() {
    local mode=$1  # "full" or "quick"
    
    echo -e "${BLUE}[BENCH]${NC} Starting benchmark..."
    echo "        Resolution: ${WIDTH}x${HEIGHT}"
    echo "        LED Chain: ${LED_CHAIN} panels"
    echo "        Duration per test: ${SECONDS_PER_EFFECT}s"
    echo ""
    
    # Select effects based on mode
    local effects_to_test
    if [ "$mode" = "quick" ]; then
        effects_to_test=("${QUICK_EFFECTS[@]}")
        echo -e "${YELLOW}[QUICK]${NC} Running quick benchmark (subset of effects)"
    else
        effects_to_test=("${EFFECTS[@]}")
    fi
    
    # Initialize JSON output
    local results_json='{"benchmark_info":{"type":"realworld","width":'${WIDTH}',"height":'${HEIGHT}',"led_chain":'${LED_CHAIN}',"seconds_per_effect":'${SECONDS_PER_EFFECT}'},"results":{"extend":[],"repeat":[]}}'
    
    # Create named pipe for sending commands
    local fifo=$(mktemp -u)
    mkfifo "$fifo"
    
    # Start the application
    local tmpfile=$(mktemp)
    ${APP_BIN} \
        --width ${WIDTH} \
        --height ${HEIGHT} \
        --sensor-width ${SENSOR_WIDTH} \
        --sensor-height ${SENSOR_HEIGHT} \
        --led-chain ${LED_CHAIN} \
        < "$fifo" > "${tmpfile}" 2>&1 &
    local app_pid=$!
    
    # Open fifo for writing
    exec 3>"$fifo"
    
    # Wait for app to initialize
    sleep 2
    
    # Disable auto-cycling
    send_key 'a'
    
    # =========================================================================
    # TEST 1: EXTEND MODE (default - image spans across panels)
    # =========================================================================
    echo -e "\n${BOLD}━━━ EXTEND MODE (image spans across panels) ━━━${NC}"
    
    local extend_results='['
    local first=true
    
    for effect_entry in "${effects_to_test[@]}"; do
        local effect_key="${effect_entry%%:*}"
        local effect_name="${effect_entry#*:}"
        
        echo -ne "  [${effect_key}] ${effect_name}..."
        
        # Select effect
        send_key "${effect_key}"
        
        # Mark start position
        local start_lines=$(wc -l < "${tmpfile}")
        
        # Wait for measurement
        sleep ${SECONDS_PER_EFFECT}
        
        # Extract FPS
        local avg_fps=$(extract_fps "${tmpfile}" "${start_lines}")
        
        # Color output based on FPS
        if [ "$avg_fps" -ge 30 ]; then
            echo -e " ${GREEN}${avg_fps} FPS${NC}"
        elif [ "$avg_fps" -ge 25 ]; then
            echo -e " ${YELLOW}${avg_fps} FPS${NC}"
        else
            echo -e " ${RED}${avg_fps} FPS${NC}"
        fi
        
        # Add to JSON
        if [ "$first" = true ]; then
            first=false
        else
            extend_results+=','
        fi
        extend_results+='{"effect_id":'${effect_key}',"effect_name":"'${effect_name}'","avg_fps":'${avg_fps}'}'
    done
    extend_results+=']'
    
    # =========================================================================
    # TEST 2: REPEAT MODE (same image on each panel)
    # =========================================================================
    echo -e "\n${BOLD}━━━ REPEAT MODE (same image on each panel) ━━━${NC}"
    
    # Switch to REPEAT mode (key 'q')
    send_key 'q'
    sleep 0.5
    
    local repeat_results='['
    first=true
    
    for effect_entry in "${effects_to_test[@]}"; do
        local effect_key="${effect_entry%%:*}"
        local effect_name="${effect_entry#*:}"
        
        echo -ne "  [${effect_key}] ${effect_name}..."
        
        # Select effect
        send_key "${effect_key}"
        
        # Mark start position
        local start_lines=$(wc -l < "${tmpfile}")
        
        # Wait for measurement
        sleep ${SECONDS_PER_EFFECT}
        
        # Extract FPS
        local avg_fps=$(extract_fps "${tmpfile}" "${start_lines}")
        
        # Color output based on FPS
        if [ "$avg_fps" -ge 30 ]; then
            echo -e " ${GREEN}${avg_fps} FPS${NC}"
        elif [ "$avg_fps" -ge 25 ]; then
            echo -e " ${YELLOW}${avg_fps} FPS${NC}"
        else
            echo -e " ${RED}${avg_fps} FPS${NC}"
        fi
        
        # Add to JSON
        if [ "$first" = true ]; then
            first=false
        else
            repeat_results+=','
        fi
        repeat_results+='{"effect_id":'${effect_key}',"effect_name":"'${effect_name}'","avg_fps":'${avg_fps}'}'
    done
    repeat_results+=']'
    
    # =========================================================================
    # TEST 3: MULTI-PANEL MODE (different effects per panel)
    # =========================================================================
    echo -e "\n${BOLD}━━━ MULTI-PANEL MODE (different effects per panel) ━━━${NC}"
    
    # Switch back to EXTEND mode first
    send_key 'q'
    sleep 0.3
    
    # Enable multi-panel mode (key '§' or try with backtick/tilde as fallback)
    # The § key might not work via pipe, let's test the mode switching
    echo -ne "  Multi-panel toggle test..."
    
    # Mark start position
    local start_lines=$(wc -l < "${tmpfile}")
    
    # Try to enable multi-panel mode
    # Note: § key may need special handling
    printf '\xc2\xa7' >&3  # UTF-8 encoding of §
    sleep 0.5
    
    # Set different effects on panels (if multi-panel is active)
    send_key '2'  # Panel 1 or all
    sleep 0.3
    printf '\xc2\xa7' >&3  # Cycle to next panel
    sleep 0.3
    send_key '4'  # Panel 2
    sleep 0.3
    printf '\xc2\xa7' >&3  # Cycle to next panel
    sleep 0.3
    send_key '7'  # Panel 3
    sleep 0.3
    
    # Wait for measurement
    sleep ${SECONDS_PER_EFFECT}
    
    # Extract FPS
    local multi_fps=$(extract_fps "${tmpfile}" "${start_lines}")
    
    if [ "$multi_fps" -ge 30 ]; then
        echo -e " ${GREEN}${multi_fps} FPS${NC}"
    elif [ "$multi_fps" -ge 25 ]; then
        echo -e " ${YELLOW}${multi_fps} FPS${NC}"
    else
        echo -e " ${RED}${multi_fps} FPS${NC}"
    fi
    
    # =========================================================================
    # Cleanup and save results
    # =========================================================================
    
    # Close fifo and kill app
    exec 3>&-
    kill $app_pid 2>/dev/null || true
    wait $app_pid 2>/dev/null || true
    rm -f "$fifo" "${tmpfile}"
    
    # Construct final JSON
    cat > "${RESULTS_FILE}" << EOF
{
  "benchmark_info": {
    "type": "realworld",
    "width": ${WIDTH},
    "height": ${HEIGHT},
    "led_chain": ${LED_CHAIN},
    "seconds_per_effect": ${SECONDS_PER_EFFECT}
  },
  "results": {
    "extend": ${extend_results},
    "repeat": ${repeat_results},
    "multi_panel_fps": ${multi_fps}
  }
}
EOF
    
    echo ""
    echo -e "${GREEN}[DONE]${NC} Results saved to ${RESULTS_FILE}"
}

display_results() {
    if [ ! -f "${RESULTS_FILE}" ]; then
        echo -e "${RED}[ERROR]${NC} No results file found"
        return 1
    fi
    
    echo ""
    python3 - "${RESULTS_FILE}" << 'PYTHON_SCRIPT'
import json
import sys

with open(sys.argv[1]) as f:
    data = json.load(f)

info = data['benchmark_info']
results = data['results']

def status_str(fps):
    if fps >= 30:
        return '\033[0;32m  OK  \033[0m'
    elif fps >= 25:
        return '\033[1;33mMARGIN\033[0m'
    else:
        return '\033[0;31m SLOW \033[0m'

print("\033[0;36m" + "═" * 70 + "\033[0m")
print("\033[0;36m                      BENCHMARK RESULTS SUMMARY                      \033[0m")
print("\033[0;36m" + "═" * 70 + "\033[0m")
print(f"\nResolution: {info['width']}x{info['height']}  |  Panels: {info['led_chain']}  |  Test: {info['seconds_per_effect']}s/effect\n")

# EXTEND mode results
print("\033[1m┌─ EXTEND MODE (image spans panels) ─────────────────────────────────┐\033[0m")
print(f"│ {'Effect':<35} │ {'FPS':>6} │ {'Status':>8} │")
print("├" + "─" * 37 + "┼" + "─" * 8 + "┼" + "─" * 10 + "┤")
for r in results['extend']:
    print(f"│ {r['effect_name']:<35} │ {r['avg_fps']:>6} │ {status_str(r['avg_fps'])} │")
print("└" + "─" * 37 + "┴" + "─" * 8 + "┴" + "─" * 10 + "┘")

# REPEAT mode results
print("\n\033[1m┌─ REPEAT MODE (same image per panel) ───────────────────────────────┐\033[0m")
print(f"│ {'Effect':<35} │ {'FPS':>6} │ {'Status':>8} │")
print("├" + "─" * 37 + "┼" + "─" * 8 + "┼" + "─" * 10 + "┤")
for r in results['repeat']:
    print(f"│ {r['effect_name']:<35} │ {r['avg_fps']:>6} │ {status_str(r['avg_fps'])} │")
print("└" + "─" * 37 + "┴" + "─" * 8 + "┴" + "─" * 10 + "┘")

# Multi-panel result
multi_fps = results.get('multi_panel_fps', 0)
print(f"\n\033[1mMulti-Panel Mode (mixed effects):\033[0m {multi_fps} FPS {status_str(multi_fps)}")
print()
PYTHON_SCRIPT
}

save_baseline() {
    if [ -f "${RESULTS_FILE}" ]; then
        cp "${RESULTS_FILE}" "${BASELINE_FILE}"
        echo -e "${GREEN}[SAVE]${NC} Baseline saved to ${BASELINE_FILE}"
    else
        echo -e "${RED}[ERROR]${NC} No results to save"
        exit 1
    fi
}

compare_results() {
    if [ ! -f "${BASELINE_FILE}" ]; then
        echo -e "${YELLOW}[WARN]${NC} No baseline found. Run with --save first."
        return 0
    fi
    
    echo ""
    python3 - "${BASELINE_FILE}" "${RESULTS_FILE}" "${REGRESSION_THRESHOLD}" << 'PYTHON_SCRIPT'
import json
import sys

with open(sys.argv[1]) as f:
    baseline = json.load(f)
with open(sys.argv[2]) as f:
    current = json.load(f)
threshold = float(sys.argv[3])

has_regression = False

def compare_mode(mode_name, base_results, curr_results):
    global has_regression
    
    base_fps = {r['effect_name']: r['avg_fps'] for r in base_results}
    curr_fps = {r['effect_name']: r['avg_fps'] for r in curr_results}
    
    print(f"\n\033[1m{mode_name} Mode Comparison:\033[0m")
    print(f"{'Effect':<35} │ {'Base':>6} │ {'Curr':>6} │ {'Change':>10}")
    print("─" * 35 + "─┼" + "─" * 8 + "┼" + "─" * 8 + "┼" + "─" * 11)
    
    for name, curr in curr_fps.items():
        base = base_fps.get(name, 0)
        if base == 0:
            change_str = "NEW"
            color = '\033[0m'
        else:
            pct = ((curr - base) / base) * 100
            if pct < -threshold:
                color = '\033[0;31m'
                change_str = f"{pct:+.1f}% ▼▼"
                has_regression = True
            elif pct < 0:
                color = '\033[1;33m'
                change_str = f"{pct:+.1f}% ▼"
            elif pct > threshold:
                color = '\033[0;32m'
                change_str = f"{pct:+.1f}% ▲▲"
            else:
                color = '\033[0m'
                change_str = f"{pct:+.1f}%"
        
        reset = '\033[0m'
        print(f"{name:<35} │ {base:>6} │ {curr:>6} │ {color}{change_str:>10}{reset}")

compare_mode("EXTEND", baseline['results']['extend'], current['results']['extend'])
compare_mode("REPEAT", baseline['results']['repeat'], current['results']['repeat'])

# Compare multi-panel
base_multi = baseline['results'].get('multi_panel_fps', 0)
curr_multi = current['results'].get('multi_panel_fps', 0)
if base_multi > 0:
    pct = ((curr_multi - base_multi) / base_multi) * 100
    if pct < -threshold:
        has_regression = True
        print(f"\n\033[0;31mMulti-Panel: {base_multi} → {curr_multi} ({pct:+.1f}%) REGRESSION\033[0m")
    else:
        print(f"\nMulti-Panel: {base_multi} → {curr_multi} ({pct:+.1f}%)")

print()
sys.exit(1 if has_regression else 0)
PYTHON_SCRIPT
    return $?
}

usage() {
    echo "Usage: sudo $0 [OPTIONS]"
    echo ""
    echo "Full pipeline benchmark testing Camera → Processing → LED Matrix"
    echo ""
    echo "Options:"
    echo "  (no args)   Run full benchmark and display results"
    echo "  --save      Save results as new baseline"
    echo "  --check     Check for regressions (CI mode, exits 1 if found)"
    echo "  --quick     Quick benchmark (subset of effects)"
    echo "  --help      Show this help"
    echo ""
    echo "Tests performed:"
    echo "  • EXTEND mode  - Image spans across all panels"
    echo "  • REPEAT mode  - Same image repeated on each panel"
    echo "  • Multi-panel  - Different effects per panel"
    echo ""
    echo "Hardware config (edit script to change):"
    echo "  Resolution: ${WIDTH}x${HEIGHT}"
    echo "  LED chain: ${LED_CHAIN} panels"
}

# Main
print_header

case "${1:-}" in
    --help|-h)
        usage
        exit 0
        ;;
    --save)
        check_root
        check_binary
        run_benchmark "full"
        display_results
        save_baseline
        ;;
    --check)
        check_root
        check_binary
        run_benchmark "full"
        display_results
        if compare_results; then
            echo -e "${GREEN}[PASS]${NC} No significant regressions detected"
            exit 0
        else
            echo -e "${RED}[FAIL]${NC} Performance regression detected!"
            exit 1
        fi
        ;;
    --quick)
        check_root
        check_binary
        run_benchmark "quick"
        display_results
        ;;
    "")
        check_root
        check_binary
        run_benchmark "full"
        display_results
        ;;
    *)
        echo -e "${RED}[ERROR]${NC} Unknown option: $1"
        usage
        exit 1
        ;;
esac
