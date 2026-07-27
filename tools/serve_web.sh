#!/bin/sh
set -eu

repository_root=$(git rev-parse --show-toplevel)
port=${1:-8000}
web_root="$repository_root/build/web"

[ -f "$web_root/web_client.html" ] ||
    {
        echo "web smoke is missing; run ./tools/workflow.sh web first" >&2
        exit 1
    }

echo "serving=http://127.0.0.1:$port/web_client.html"
exec python3 -m http.server \
    "$port" \
    --bind 127.0.0.1 \
    --directory "$web_root"
