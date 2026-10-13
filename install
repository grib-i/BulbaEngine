#!/bin/bash

set -o pipefail
export LC_ALL=C.UTF-8

PROJECT="bulba"
BUILD_TYPE="Debug"
CLEAN=0
ASAN=0
GPROF=0
VALGRIND=0
MONITOR=0

LOG_DIR="logs/$(date '+%Y-%m-%d_%H-%M-%S')"
RUN_LOG="$LOG_DIR/run.log"
APP_LOG="$LOG_DIR/application.log"

RESET=$'\033[0m'
RED=$'\033[91m'
GREEN=$'\033[92m'
YELLOW=$'\033[93m'
CYAN=$'\033[96m'
BOLD=$'\033[1m'

ok()   { echo -e "${GREEN}${BOLD}✓${RESET} $1"; }
info() { echo -e "${CYAN}${BOLD}●${RESET} $1"; }
warn() { echo -e "${YELLOW}${BOLD}▲${RESET} $1"; }
fail() { echo -e "${RED}${BOLD}✗${RESET} $1"; exit 1; }

for arg in "$@"; do
    case "$arg" in
        debug)          BUILD_TYPE="Debug" ;;
        release)        BUILD_TYPE="Release" ;;
        clean)          CLEAN=1 ;;
        valgrind)       VALGRIND=1 ;;
        ltrack|ltraces) MONITOR=1 ;;
        asan)           ASAN=1 ;;
        gprof)          GPROF=1 ;;

        *)
            echo "Usage: ./install [debug|release] [clean] [valgrind] [ltrack|ltraces]"
            exit 1
            ;;
    esac
done

if [ "$VALGRIND" -eq 1 ] && [ "$BUILD_TYPE" != "Debug" ]; then
    fail "Valgrind requires Debug build"
fi

mkdir -p "$LOG_DIR" || fail "Cannot create log directory"
touch "$RUN_LOG" "$APP_LOG" || fail "Cannot create log files"

exec > >(tee -a "$RUN_LOG") 2>&1

echo
echo -e "${CYAN}${BOLD}=== BULBA ENGINE ===${RESET}"
echo "Build: $BUILD_TYPE"
echo "Logs:  $LOG_DIR"
echo

for cmd in cmake ninja awk ps; do
    command -v "$cmd" >/dev/null || fail "$cmd not found"
done

if [ "$VALGRIND" -eq 1 ]; then
    command -v valgrind >/dev/null || fail "valgrind not found"
else
    command -v stdbuf >/dev/null || fail "stdbuf not found"
fi

[ "$CLEAN" -eq 1 ] && rm -rf build

if [ "$BUILD_TYPE" = "Debug" ]; then
    BUILD_DIR="build/debug"

    if [ "$VALGRIND" -eq 1 ]; then
        ASAN="-DBULBA_ENABLE_ASAN=OFF"
    else
        ASAN="-DBULBA_ENABLE_ASAN=ON"
    fi

    info "Configuring Debug..."

    cmake -S . -B "$BUILD_DIR" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug \
        -DPROJECT_NAME="$PROJECT" \
        -DBLB_BUILD_APP=ON \
        -DBLB_BUILD_SHADERS=ON \
        "$ASAN" ||
        fail "CMake configuration failed"

    info "Building Debug..."

    cmake --build "$BUILD_DIR" -j"$(nproc)" ||
        fail "Build failed"

    APP="./$BUILD_DIR/$PROJECT"

    [ -f "$APP" ] || fail "Binary not found: $APP"

    ok "Debug build complete"

    echo

    if [ "$VALGRIND" -eq 1 ]; then
        info "Running with Valgrind..."

        valgrind \
            --tool=memcheck \
            --leak-check=full \
            --show-leak-kinds=all \
            --track-origins=yes \
            --num-callers=30 \
            --log-file="$LOG_DIR/valgrind.log" \
            "$APP" >> "$APP_LOG" 2>&1

        RESULT=$?

    elif [ "$MONITOR" -eq 1 ]; then
        info "Running with monitor..."

        (
            export ASAN_OPTIONS="${ASAN_OPTIONS:+$ASAN_OPTIONS:}verify_asan_link_order=0"
            exec stdbuf -o0 -e0 "$APP"
        ) >> "$APP_LOG" 2>&1 &

        PID=$!

        trap 'kill "$PID" 2>/dev/null; exit 130' INT TERM

        while kill -0 "$PID" 2>/dev/null; do
            clear

            echo -e "${CYAN}${BOLD}=== BULBA ENGINE MONITOR ===${RESET}"
            echo

            ps -p "$PID" \
                -o pid=,%cpu=,%mem=,rss=,vsz=,stat=,etime=,comm= |
                awk '{
                    printf "PID:   %s\n", $1
                    printf "CPU:   %s%%\n", $2
                    printf "MEM:   %s%%\n", $3
                    printf "RSS:   %.2f MB\n", $4 / 1024
                    printf "VSZ:   %.2f MB\n", $5 / 1024
                    printf "STATE: %s\n", $6
                    printf "TIME:  %s\n", $7
                    printf "NAME:  %s\n", $8
                }'

            echo
            echo "Application log: $APP_LOG"
            echo "Ctrl+C to stop"

            sleep 0.5
        done

        wait "$PID"
        RESULT=$?

        trap - INT TERM

    else
        info "Running Debug..."

        (
            export ASAN_OPTIONS="${ASAN_OPTIONS:+$ASAN_OPTIONS:}verify_asan_link_order=0"
            exec stdbuf -o0 -e0 "$APP"
        ) >> "$APP_LOG" 2>&1

        RESULT=$?
    fi

    echo

    if [ "$RESULT" -eq 0 ]; then
        ok "Debug finished successfully"
    else
        warn "Debug exited with code $RESULT"
    fi

    echo "Application log: $APP_LOG"
    echo "Run log:         $RUN_LOG"

    exit "$RESULT"
fi

# ==================== RELEASE ====================

BUILD_DIR="build/release"

info "Configuring Release..."

cmake -S . -B "$BUILD_DIR" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DPROJECT_NAME="$PROJECT" \
    -DBLB_BUILD_APP=OFF \
    -DBLB_BUILD_SHADERS=ON \
    -DCMAKE_INSTALL_PREFIX=/usr/local ||
    fail "CMake configuration failed"

info "Building Release..."

cmake --build "$BUILD_DIR" -j"$(nproc)" ||
    fail "Build failed"

LIBRARY="$BUILD_DIR/libbulba_engine.so"

[ -f "$LIBRARY" ] || fail "Library not found: $LIBRARY"

ok "Release build complete"

info "Installing..."

sudo cmake --install "$BUILD_DIR" ||
    fail "Installation failed"

sudo ldconfig ||
    fail "ldconfig failed"

[ -f "/usr/local/lib/libbulba_engine.so" ] ||
    fail "Installed library not found"

ok "Release installed successfully"

echo "Library: /usr/local/lib/libbulba_engine.so"
echo "Run log: $RUN_LOG"
