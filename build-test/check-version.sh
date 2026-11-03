#!/usr/bin/env bash
# Checks that the project version is identical everywhere it is declared:
#   configure.ac (AC_INIT), configure (generated), README.md (Version:),
#   sudosh.spec (%define version), and the newest CHANGELOG.md heading.
# Also checks that no version component exceeds 9.
# Exit status: 0 if all agree, 1 otherwise.
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
fails=0

v_ac=$(sed -n 's/^AC_INIT(\[sudosh2\],\[\([^]]*\)\].*/\1/p' "$ROOT/configure.ac")
v_cf=$(sed -n "s/^PACKAGE_VERSION='\(.*\)'/\1/p" "$ROOT/configure" | head -1)
v_readme=$(sed -n 's/^Version: *\([0-9][0-9.]*\)[[:space:]]*$/\1/p' "$ROOT/README.md" | head -1)
v_spec=$(sed -n 's/^%define version *\([0-9][0-9.]*\)[[:space:]]*$/\1/p' "$ROOT/sudosh.spec" | head -1)
v_log=$(sed -n 's/^## \[\([0-9][0-9.]*\)\].*/\1/p' "$ROOT/CHANGELOG.md" | head -1)

echo "configure.ac: $v_ac"
echo "configure:    $v_cf"
echo "README.md:    $v_readme"
echo "sudosh.spec:  $v_spec"
echo "CHANGELOG.md: $v_log"

for v in "$v_ac" "$v_cf" "$v_readme" "$v_spec" "$v_log"; do
    if [ -z "$v" ]; then
        echo "FAIL: a version declaration could not be read"
        fails=$((fails + 1))
        break
    fi
done

for v in "$v_ac" "$v_cf" "$v_readme" "$v_spec" "$v_log"; do
    if [ "$v" != "$v_ac" ]; then
        echo "FAIL: version $v does not match configure.ac ($v_ac)"
        fails=$((fails + 1))
    fi
done

if ! echo "$v_ac" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$'; then
    echo "FAIL: $v_ac is not MAJOR.MINOR.PATCH"
    fails=$((fails + 1))
elif ! echo "$v_ac" | awk -F. '{exit !($1 <= 9 && $2 <= 9 && $3 <= 9)}'; then
    echo "FAIL: $v_ac has a component above 9"
    fails=$((fails + 1))
fi

if [ "$fails" -eq 0 ]; then
    echo "PASS: all version declarations agree on $v_ac"
    exit 0
fi
exit 1
