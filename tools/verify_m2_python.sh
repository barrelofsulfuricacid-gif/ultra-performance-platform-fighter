#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
python_command=${PYTHON_COMMAND:-python3}
gymnasium_source=${PF_GYMNASIUM_SOURCE:-}

case "$(uname -s)" in
    Darwin)
        library="$root/build/headless/libpf_sim_rl.dylib"
        ;;
    MINGW*|MSYS*|CYGWIN*)
        library="$root/build/headless/pf_sim_rl.dll"
        ;;
    *)
        library="$root/build/headless/libpf_sim_rl.so"
        ;;
esac

[ -f "$library" ] || {
    echo "M2 Python verification failed: build headless first" >&2
    exit 1
}

python_path="$root/bindings/python/src"
if [ -n "$gymnasium_source" ]; then
    python_path="$gymnasium_source:$python_path"
fi

PF_SIM_LIBRARY="$library" \
PYTHONPATH="$python_path${PYTHONPATH:+:$PYTHONPATH}" \
    "$python_command" -c \
        'import gymnasium; assert gymnasium.__version__ == "1.3.0"'

PF_SIM_LIBRARY="$library" \
PYTHONPATH="$python_path${PYTHONPATH:+:$PYTHONPATH}" \
    "$python_command" -m unittest discover \
        -s "$root/bindings/python/tests" -v

PYTHONPATH="$python_path${PYTHONPATH:+:$PYTHONPATH}" \
    "$python_command" "$root/tools/benchmark_m2_python.py" \
        --library "$library"

echo "m2-python-verification=pass gymnasium=1.3.0 environments=64"
