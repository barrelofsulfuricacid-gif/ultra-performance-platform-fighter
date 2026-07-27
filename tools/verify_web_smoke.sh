#!/bin/sh
set -eu

repository_root=$(git rev-parse --show-toplevel)
web_root="$repository_root/build/web"
port=${PF_WEB_SMOKE_PORT:-8123}
url="http://127.0.0.1:$port/web_client.html"
server_log="$web_root/web_smoke_server.log"
dom_output="$web_root/web_smoke_dom.html"

[ -f "$web_root/web_client.html" ] ||
    {
        echo "web smoke is missing; run ./tools/workflow.sh web first" >&2
        exit 1
    }

browser=${PF_BROWSER:-}
if [ -z "$browser" ]; then
    for candidate in \
        google-chrome-stable \
        google-chrome \
        chromium \
        chromium-browser \
        "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
    do
        if command -v "$candidate" >/dev/null 2>&1; then
            browser=$(command -v "$candidate")
            break
        fi
        if [ -x "$candidate" ]; then
            browser=$candidate
            break
        fi
    done
fi

[ -n "$browser" ] && [ -x "$browser" ] ||
    {
        echo "Chrome/Chromium was not found; set PF_BROWSER to its executable" >&2
        exit 1
    }

python3 -m http.server \
    "$port" \
    --bind 127.0.0.1 \
    --directory "$web_root" \
    >"$server_log" 2>&1 &
server_pid=$!
trap 'kill "$server_pid" 2>/dev/null || true' EXIT HUP INT TERM

attempt=0
while ! curl -fsS "$url" >/dev/null 2>&1; do
    attempt=$((attempt + 1))
    if [ "$attempt" -ge 20 ]; then
        echo "browser smoke server did not become ready: $server_log" >&2
        exit 1
    fi
    sleep 1
done

"$browser" \
    --headless \
    --no-sandbox \
    --use-gl=angle \
    --use-angle=swiftshader \
    --enable-unsafe-swiftshader \
    --virtual-time-budget=10000 \
    --dump-dom \
    "$url" \
    >"$dom_output"

grep -Fq \
    'web-client-smoke=pass sim_abi=1 tick_hz=60' \
    "$dom_output"
grep -Fq \
    'webgl2=pass batch_draws=1' \
    "$dom_output"

echo "web-browser-smoke=pass browser=$browser url=$url"
