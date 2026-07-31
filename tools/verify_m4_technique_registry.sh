#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
registry="$root/docs/product/m4_advanced_technique_registry.md"

[ -f "$registry" ] ||
    {
        echo "M4 technique registry is missing" >&2
        exit 1
    }

grep -Fq '**Registry schema:** 1' "$registry"
grep -Fq 'revision 2048934' "$registry"
grep -Fq '**M4 acceptance:** Blocked' "$registry"

awk -F '|' '
function trim(value) {
    gsub(/^[[:space:]]+|[[:space:]]+$/, "", value)
    return value
}

BEGIN {
    expected_id = 1
    rows = 0
    planned = 0
    primitive_ready = 0
    playable = 0
    verified = 0
}

$2 ~ /^[[:space:]]*[0-9]+[[:space:]]*$/ {
    id = trim($2) + 0
    technique = trim($3)
    status = trim($4)
    target = trim($5)
    dependencies = trim($6)
    evidence = trim($7)
    recipe = trim($8)

    if (id != expected_id)
    {
        printf "M4 technique registry expected row %d, found %d\n",
            expected_id, id > "/dev/stderr"
        exit 1
    }
    if (technique == "" || dependencies == "" ||
        evidence == "" || recipe == "")
    {
        printf "M4 technique registry row %d has an empty field\n",
            id > "/dev/stderr"
        exit 1
    }
    if (target != "M4.4")
    {
        printf "M4 technique registry row %d has target %s\n",
            id, target > "/dev/stderr"
        exit 1
    }
    if (status == "planned")
    {
        ++planned
    }
    else if (status == "primitive-ready")
    {
        ++primitive_ready
    }
    else if (status == "playable")
    {
        ++playable
    }
    else if (status == "verified")
    {
        ++verified
    }
    else
    {
        printf "M4 technique registry row %d has invalid status %s\n",
            id, status > "/dev/stderr"
        exit 1
    }

    ++rows
    ++expected_id
}

END {
    if (rows != 61)
    {
        printf "M4 technique registry expected 61 rows, found %d\n",
            rows > "/dev/stderr"
        exit 1
    }
    if (planned != 23 || primitive_ready != 4 ||
        playable != 33 || verified != 1)
    {
        printf "M4 technique registry status counts changed: planned=%d primitive_ready=%d playable=%d verified=%d\n",
            planned, primitive_ready, playable, verified > "/dev/stderr"
        exit 1
    }
}
' "$registry"

echo "m4-technique-registry=pass rows=61 verified=1 playable=33 primitive_ready=4 planned=23 acceptance=blocked"
