#!/bin/bash
# Regression tests for sudosh2.
#
# Copies the working tree to a temporary directory, builds it with the CI
# warning flags, then runs each test against the result. The build uses a
# private --sysconfdir, so no real installation is touched.
#
# Exit status is the number of failed tests (0 when everything passes).
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
PTY="$ROOT/build-test/regress_pty.py"
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
FAILS=0
ME=$(id -un)
ID="alice-$ME-1700000000-ABCDEFGHIJKLMNOP"

pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; FAILS=$((FAILS + 1)); }

# build NAME CONFIGURE_ARGS...: copy the tree and build it under $T/NAME.
# Binaries end up in $T/NAME/src.
build() {
    local name=$1; shift
    local dir="$T/$name"
    mkdir -p "$dir"
    rsync -a --exclude .git "$ROOT/" "$dir/" || return 1
    (cd "$dir" && ./configure "$@" >"$T/$name.configure.log" 2>&1 &&
        make CFLAGS='-Wall -Werror -pedantic' >"$T/$name.make.log" 2>&1)
}

# write_conf LOGDIR [EXTRA_LINE...]: write the sudosh.conf the build will read.
write_conf() {
    local dir=$1; shift
    mkdir -p "$T/etc"
    {
        echo "logdir = $dir"
        echo "delimiter = -"
        echo "defaultshell = /bin/sh"
        printf '%s\n' "$@"
    } > "$T/etc/sudosh.conf"
}

reset_logs() {
    rm -rf "$T/logs"
    mkdir -p "$T/logs"
    write_conf "$T/logs"
}

# script_mode: permission bits of the first script file in the log directory.
script_mode() {
    find "$T/logs" -name '*-script-*' -exec stat -c %a {} + 2>/dev/null | head -1
}

echo "=== version agreement"
if "$ROOT/build-test/check-version.sh" >"$T/version.out" 2>&1; then
    pass "version declarations agree"
else
    fail "version declarations agree"
    cat "$T/version.out"
fi

echo "=== building"
mkdir -p "$T/etc"
if ! build main --sysconfdir="$T/etc"; then
    echo "FAIL: build failed; see $T/main.make.log"
    tail -20 "$T/main.make.log" 2>/dev/null
    exit 1
fi
SUDOSH="$T/main/src/sudosh"
REPLAY="$T/main/src/sudosh-replay"

echo "=== -c allow list"
reset_logs
rm -f "$T/pwn-1" "$T/pwn-2"
write_conf "$T/logs" "-cargallow = ls"
"$SUDOSH" -c "ls /nonexistent-dir \$(touch $T/pwn-1)" >/dev/null 2>&1
"$SUDOSH" -c "$(printf 'ls /nonexistent-dir\ntouch %s/pwn-2' "$T")" >/dev/null 2>&1
"$SUDOSH" -c 'ls /nonexistent-dir' >/dev/null 2>&1
rc=$?
if [ ! -e "$T/pwn-1" ] && [ ! -e "$T/pwn-2" ]; then
    pass "-c rejects command substitution and newlines"
else
    fail "-c rejects command substitution and newlines"
fi
if [ "$rc" -eq 2 ]; then
    pass "-c still runs an allowed command"
else
    fail "-c still runs an allowed command (exit $rc, expected 2)"
fi

echo "=== -c with an empty command"
reset_logs
write_conf "$T/logs" "-cargallow = ls"
out=$(python3 "$PTY" emptyc "$SUDOSH")
if [ "${out%% *}" = EXITED ]; then
    pass "-c with an empty command does not start a shell"
else
    fail "-c with an empty command does not start a shell (got $out)"
fi

echo "=== -cargallow length"
reset_logs
long=$(head -c 3000 /dev/zero | tr '\0' a)
write_conf "$T/logs" "-cargallow = $long" "-cargallow = $long" "-cargallow = $long"
"$SUDOSH" -c 'ls' >"$T/out" 2>"$T/err"
rc=$?
if [ "$rc" -ne 0 ] && grep -q 'too long' "$T/err"; then
    pass "an oversized -cargallow list is refused"
else
    fail "an oversized -cargallow list is refused (exit $rc)"
fi

echo "=== -c log"
reset_logs
write_conf "$T/logs" "-cargallow = echo"
"$SUDOSH" -c 'echo hello world' >/dev/null 2>&1
logged=$(head -1 "$T"/logs/*-interactive-* 2>/dev/null)
if [ "$logged" = "echo hello world" ]; then
    pass "the -c log records the whole command line"
else
    fail "the -c log records the whole command line (got '$logged')"
fi

echo "=== session environment (pty)"
reset_logs
write_conf "$T/logs" "clearenvironment = false"
foo=$(FOO=visible python3 "$PTY" envshow "$SUDOSH" | sed -n 's/^FOO=//p')
if [ "$foo" = visible ]; then
    pass "clearenvironment = false keeps the caller's environment"
else
    fail "clearenvironment = false keeps the caller's environment (FOO='$foo')"
fi
reset_logs
foo=$(FOO=visible python3 "$PTY" envshow "$SUDOSH" | sed -n 's/^FOO=//p')
if [ -z "$foo" ]; then
    pass "the environment is cleared by default"
else
    fail "the environment is cleared by default (FOO='$foo')"
fi
reset_logs
sess_user=$(USER=someone-else python3 "$PTY" envshow "$SUDOSH" | sed -n 's/^USER=//p')
if [ "$sess_user" = "$ME" ]; then
    pass "the session environment sets USER to the user being logged in as"
else
    fail "the session environment sets USER to the user being logged in as (USER='$sess_user', expected $ME)"
fi
reset_logs
fds=$( (exec 150>/dev/null; python3 "$PTY" fds "$SUDOSH") | sed -n 's/^FDS=//p')
if [ -n "$fds" ] && ! echo " $fds " | grep -q ' 150 '; then
    pass "a high-numbered inherited descriptor is not passed to the session shell"
else
    fail "a high-numbered inherited descriptor is not passed to the session shell (shell holds: $fds)"
fi

echo "=== session signal and exit handling (pty)"
reset_logs
out=$(python3 "$PTY" sigterm "$SUDOSH")
if [ "${out%% *}" = EXITED ] && [ "$(script_mode)" = 440 ]; then
    pass "SIGTERM ends the session and closes logs"
else
    fail "SIGTERM ends the session and closes logs (got $out, mode $(script_mode))"
fi

reset_logs
out=$(python3 "$PTY" sighup "$SUDOSH")
if [ "${out%% *}" = EXITED ] && [ "$(script_mode)" = 440 ]; then
    pass "a dropped connection (SIGHUP) ends the session and closes logs"
else
    fail "a dropped connection (SIGHUP) ends the session and closes logs (got $out, mode $(script_mode))"
fi

reset_logs
out=$(python3 "$PTY" exit "$SUDOSH")
if [ "${out%% *}" = EXITED ] && [ "$(script_mode)" = 440 ]; then
    pass "typing exit ends the session and closes logs"
else
    fail "typing exit ends the session and closes logs (got $out, mode $(script_mode))"
fi

echo "=== session timing (pty)"
reset_logs
python3 "$PTY" timing "$SUDOSH" >/dev/null
# Output that arrives two seconds after the previous output must carry that gap.
max_delta=$(find "$T/logs" -name '*-time-*' -exec cat {} + | awk 'BEGIN { m = 0 } $2 > 0 && $1 > m { m = $1 } END { print m }')
if awk -v t="$max_delta" 'BEGIN { exit !(t >= 1.5) }'; then
    pass "a pause before output is recorded as that pause (${max_delta}s)"
else
    fail "a pause before output is recorded as that pause (largest gap ${max_delta}s)"
fi

echo "=== hostile environment (LD_PRELOAD test doubles)"
PRELOAD="$T/regress_preload.so"
if ! gcc -shared -fPIC -Wall -o "$PRELOAD" "$ROOT/build-test/regress_preload.c" -ldl 2>"$T/preload.log"; then
    fail "build LD_PRELOAD test library (see $T/preload.log)"
else
    reset_logs
    out=$(REGRESS_PRELOAD="$PRELOAD" REGRESS_FAKE_UID=4242 python3 "$PTY" wait "$SUDOSH")
    if [ "$out" = "EXITED 1" ]; then
        pass "a terminal owner with no passwd entry is refused cleanly"
    else
        fail "a terminal owner with no passwd entry is refused cleanly (got $out)"
    fi

    reset_logs
    write_conf "$T/logs" "syslog.facility = LOG_DAEMON" "syslog.priority = LOG_INFO"
    rm -f "$T/syslog.log"
    out=$(REGRESS_SYSLOG_LOG="$T/syslog.log" REGRESS_PRELOAD="$PRELOAD" REGRESS_FAKE_UID=4242 \
        REGRESS_FAKE_NAME='x%n%n' python3 "$PTY" exit "$SUDOSH")
    if [ "$out" = "EXITED 0" ] && grep -q 'starting session for x%n%n' "$T/syslog.log" 2>/dev/null; then
        pass "a login name containing % is logged literally at session start"
    else
        fail "a login name containing % is logged literally at session start (got $out)"
    fi

    write_conf "$T/logs" "defaultshell = /nonexistent/shell" "delimiter = -"
    LD_PRELOAD="$PRELOAD" REGRESS_FAKE_SHELLS=1 "$REPLAY" >"$T/out" 2>"$T/err"
    if grep -q "default shell '/nonexistent/shell': " "$T/err"; then
        pass "the defaultshell error names the configured shell"
    else
        fail "the defaultshell error names the configured shell (got: $(cat "$T/err"))"
    fi

    reset_logs
    out=$(REGRESS_PRELOAD="$PRELOAD" REGRESS_SHORT_WRITE=1 python3 "$PTY" hello "$SUDOSH")
    count=$(cat "$T"/logs/*-script-* 2>/dev/null | grep -o hello | wc -l)
    if [ "$out" = "EXITED 0" ] && [ "$count" -ge 2 ]; then
        pass "writes that accept one byte at a time still reach the shell and the log"
    else
        fail "writes that accept one byte at a time still reach the shell and the log (got $out, $count)"
    fi

    reset_logs
    write_conf "$T/logs" "-cargallow = echo"
    LD_PRELOAD="$PRELOAD" REGRESS_EXECL_FAIL=1 "$SUDOSH" -c 'echo hi' >"$T/out" 2>"$T/err"
    rc=$?
    if [ "$rc" -ne 0 ] && grep -q 'cannot run' "$T/err"; then
        pass "-c reports a failure to run the command"
    else
        fail "-c reports a failure to run the command (exit $rc)"
    fi

    reset_logs
    write_conf "$T/logs" "-cargallow = echo"
    LD_PRELOAD="$PRELOAD" REGRESS_NOW=2147483748 "$SUDOSH" -c 'echo hi' >/dev/null 2>&1
    if ls "$T/logs" | grep -q -- '-interactive-2147483748-'; then
        pass "log file names keep a positive timestamp after 2038"
    else
        fail "log file names keep a positive timestamp after 2038 ($(ls "$T/logs" | tr '\n' ' '))"
    fi

    reset_logs
    rm -f "$T/syslog.log"
    out=$(REGRESS_SYSLOG_LOG="$T/syslog.log" REGRESS_PRELOAD="$PRELOAD" REGRESS_SHELL=/nonexistent/sh \
        python3 "$PTY" wait "$SUDOSH")
    if [ "$out" = "EXITED 1" ] && [ ! -s "$T/syslog.log" ]; then
        pass "an invalid shell is refused without logging when syslog is not configured"
    else
        fail "an invalid shell is refused without logging when syslog is not configured (got $out, log: $(cat "$T/syslog.log" 2>/dev/null))"
    fi

    reset_logs
    out=$(REGRESS_PRELOAD="$PRELOAD" REGRESS_PTSNAME_FAIL=1 python3 "$PTY" wait "$SUDOSH")
    if [ "$out" = "EXITED 1" ]; then
        pass "a failure to name the session terminal ends sudosh cleanly"
    else
        fail "a failure to name the session terminal ends sudosh cleanly (got $out)"
    fi
fi

echo "=== input recording (--enable-recordinput build)"
if build rec --sysconfdir="$T/etc" --enable-recordinput; then
    reset_logs
    python3 "$PTY" input "$T/rec/src/sudosh" >/dev/null
    input_file=$(find "$T/logs" -name '*-input-*' | head -1)
    got=$(cat "$input_file" 2>/dev/null)
    want=$(printf 'ab\nexit\n')
    if [ "$got" = "$want" ]; then
        pass "every keystroke after the first is recorded"
    else
        fail "every keystroke after the first is recorded (got '$got')"
    fi
else
    fail "--enable-recordinput build (see $T/rec.make.log)"
fi

echo "=== replay"
mk_time() { printf '%s' "$1" > "$T/logs/alice-$ME-time-1700000000-ABCDEFGHIJKLMNOP"; }
mk_script() { printf '%s' "$1" > "$T/logs/alice-$ME-script-1700000000-ABCDEFGHIJKLMNOP"; }

reset_logs
mk_script "$(head -c 100000 /dev/zero | tr '\0' A)"
mk_time "$(printf '0.1 100000\n')"
"$REPLAY" "$ID" 1 1 >"$T/out" 2>"$T/err"
rc=$?
if [ "$rc" -eq 1 ] && grep -q limit "$T/err"; then
    pass "replay rejects a chunk larger than its buffer"
else
    fail "replay rejects a chunk larger than its buffer (exit $rc)"
fi

reset_logs
mk_script hello
mk_time "$(printf '0.1 5\n0.2 -1\n')"
"$REPLAY" "$ID" 1 1 >"$T/out" 2>"$T/err"
rc=$?
if [ "$rc" -eq 0 ] && [ "$(cat "$T/out")" = hello ]; then
    pass "replay skips the -1 entries written by older sudosh"
else
    fail "replay skips the -1 entries written by older sudosh (exit $rc)"
fi

reset_logs
mk_script "$(head -c 8192 /dev/zero | tr '\0' A)"
mk_time "$(printf '0.1 8192\n')"
"$REPLAY" "$ID" 0 >"$T/out" 2>"$T/err"
rc=$?
size=$(wc -c < "$T/out")
if [ "$rc" -eq 0 ] && [ "$size" -eq 8192 ]; then
    pass "replay -d 0 prints exactly the logged bytes"
else
    fail "replay -d 0 prints exactly the logged bytes (got $size bytes)"
fi

reset_logs
mk_script x
mk_time "$(printf '10.0 1\n')"
start=$(date +%s%N)
"$REPLAY" "$ID" 2 2 >/dev/null 2>&1
rc=$?
end=$(date +%s%N)
ms=$(( (end - start) / 1000000 ))
if [ "$rc" -eq 0 ] && [ "$ms" -ge 1800 ] && [ "$ms" -lt 4000 ]; then
    pass "MAXWAIT caps a pause at MAXWAIT seconds of playback"
else
    fail "MAXWAIT caps a pause at MAXWAIT seconds of playback (took ${ms}ms)"
fi

reset_logs
printf 'ls -l\n' > "$T/logs/alice-$ME-interactive-1700000003-AAAAAAAAAAAAAAAA"
printf 'secret\n' > "$T/logs/alice-$ME-interactive-1700000004-BBBBBBBBBBBBBBBB"
chmod 000 "$T/logs/alice-$ME-interactive-1700000004-BBBBBBBBBBBBBBBB"
if [ "$(id -u)" -eq 0 ]; then
    echo "SKIP: running as root, unreadable-file test needs a normal user"
else
    "$REPLAY" >"$T/out" 2>"$T/err"
    rc=$?
    if [ "$rc" -eq 0 ] && grep -q 'ls -l' "$T/out"; then
        pass "an unreadable interactive log does not hide the others"
    else
        fail "an unreadable interactive log does not hide the others (exit $rc)"
    fi
fi

reset_logs
printf 'x' > "$T/logs/alice-$ME-input-1700000005-CCCCCCCCCCCCCCCC"
"$REPLAY" >"$T/out" 2>"$T/err"
rc=$?
if [ "$rc" -eq 0 ] && ! grep -q 'missing script' "$T/err"; then
    pass "a lone input file does not create a session"
else
    fail "a lone input file does not create a session (exit $rc)"
fi

echo "=== forged and malformed log files"
reset_logs
touch "$T/logs/forge-$ME-script-99999999999999999-AAAA"
timeout 10 "$REPLAY" >"$T/out" 2>"$T/err"
rc=$?
if [ "$rc" -eq 0 ] && grep -q '(bad date)' "$T/out"; then
    pass "a file name with an out-of-range timestamp does not crash the listing"
else
    fail "a file name with an out-of-range timestamp does not crash the listing (exit $rc)"
fi

reset_logs
printf 'real\n' > "$T/logs/good-$ME-script-1700000000-GGGG"
printf '0.1 4\n' > "$T/logs/good-$ME-time-1700000000-GGGG"
printf 'garbage\n' > "$T/logs/evil-$ME-time-1-AAAA"
"$REPLAY" >"$T/out" 2>"$T/err"
rc=$?
if [ "$rc" -eq 0 ] && grep -q "good-$ME-1700000000-GGGG" "$T/out"; then
    pass "one malformed time file does not hide the other sessions"
else
    fail "one malformed time file does not hide the other sessions (exit $rc)"
fi

reset_logs
printf 'real\n' > "$T/logs/good-$ME-script-1700000000-GGGG"
mkfifo "$T/logs/fifo-$ME-time-1-AAAA"
ln -s /etc/passwd "$T/logs/link-$ME-script-1-AAAA"
timeout 10 "$REPLAY" >"$T/out" 2>"$T/err"
rc=$?
if [ "$rc" -eq 0 ] && grep -q "good-$ME-1700000000-GGGG" "$T/out" && ! grep -q 'fifo-\|link-' "$T/out"; then
    pass "a planted FIFO or symlink is skipped instead of hanging or being followed"
else
    fail "a planted FIFO or symlink is skipped instead of hanging or being followed (exit $rc)"
fi

reset_logs
printf 'forged\n' > "$T/logs/forge-root-script-1700000000-FFFF"
printf 'real\n' > "$T/logs/good-$ME-script-1700000000-GGGG"
if [ "$(id -u)" -eq 0 ]; then
    echo "SKIP: running as root, the forged-owner test needs a normal user"
else
    "$REPLAY" >"$T/out" 2>"$T/err"
    rc=$?
    if [ "$rc" -eq 0 ] && grep -q "good-$ME-1700000000-GGGG" "$T/out" && ! grep -q 'forge-root' "$T/out"; then
        pass "a file not owned by the user in its name is ignored"
    else
        fail "a file not owned by the user in its name is ignored (exit $rc)"
    fi
fi

reset_logs
printf '\033]0;INJECTED\007\033[2Jboo\n' > "$T/logs/forged-$ME-interactive-1700000000-AAAA"
"$REPLAY" >"$T/out" 2>"$T/err"
if [ "$(tr -d -c '\033' < "$T/out" | wc -c)" -eq 0 ] && grep -q 'INJECTED' "$T/out"; then
    pass "control characters in a log header are not sent to the terminal"
else
    fail "control characters in a log header are not sent to the terminal"
fi

reset_logs
mk_script "$(head -c 20 /dev/zero | tr '\0' x)"
mk_time "$(for i in $(seq 20); do echo '0.090000 1'; done)"
start=$(date +%s%N)
"$REPLAY" "$ID" >/dev/null 2>&1
end=$(date +%s%N)
ms=$(( (end - start) / 1000000 ))
if [ "$ms" -ge 1500 ] && [ "$ms" -lt 4000 ]; then
    pass "short pauses are replayed at their recorded length (${ms}ms for 1.8s)"
else
    fail "short pauses are replayed at their recorded length (took ${ms}ms, expected about 1800)"
fi

reset_logs
printf 'ab\0cd' > "$T/logs/alice-$ME-script-1700000000-ABCDEFGHIJKLMNOP"
printf '0.01 5\n' > "$T/logs/alice-$ME-time-1700000000-ABCDEFGHIJKLMNOP"
size=$("$REPLAY" "$ID" 2>/dev/null | wc -c)
if [ "$size" -eq 5 ]; then
    pass "a recorded chunk containing a NUL byte is replayed in full"
else
    fail "a recorded chunk containing a NUL byte is replayed in full (got $size bytes of 5)"
fi

echo "=== random session suffix"
cat > "$T/randtest.c" <<'EOF'
#include <stdio.h>
#include <string.h>
char *rand2str(size_t len);
int main(void)
{
    int i, j, samples = 200, distinct_total = 0, bad = 0;
    for (i = 0; i < samples; i++) {
        const char *s = rand2str(16);
        int seen[256] = {0}, distinct = 0;
        if (strlen(s) != 16)
            bad++;
        for (j = 0; j < 16; j++) {
            unsigned char c = (unsigned char)s[j];
            if (!seen[c]) {
                seen[c] = 1;
                distinct++;
            }
        }
        distinct_total += distinct;
    }
    printf("%.2f %d %s\n", (double)distinct_total / samples, bad, rand2str(16));
    return 0;
}
EOF
if gcc -Wall -I"$T/main" -o "$T/randtest" "$T/randtest.c" "$T/main/src/rand.c" 2>"$T/rand.build.log"; then
    read -r avg bad _ < <("$T/randtest")
    if [ "$bad" -eq 0 ] && awk -v a="$avg" 'BEGIN { exit !(a >= 13) }'; then
        pass "random suffix uses distinct characters (avg $avg distinct of 16)"
    else
        fail "random suffix uses distinct characters (avg $avg distinct of 16)"
    fi
    a=$(LD_PRELOAD="$PRELOAD" REGRESS_FROZEN_CLOCK=1 "$T/randtest")
    b=$(LD_PRELOAD="$PRELOAD" REGRESS_FROZEN_CLOCK=1 "$T/randtest")
    if [ "$a" != "$b" ]; then
        pass "the session suffix does not depend on the clock and pid alone"
    else
        fail "the session suffix does not depend on the clock and pid alone (both runs gave: $a)"
    fi
else
    fail "random suffix test build (see $T/rand.build.log)"
fi

echo "=== logdir creation and permissions"
rm -rf "$T/mklogs"
write_conf "$T/mklogs"
(umask 022; "$REPLAY" >/dev/null 2>&1)
rc=$?
mode=$(stat -c %a "$T/mklogs" 2>/dev/null)
if [ "$rc" -eq 0 ] && [ "$mode" = 1733 ]; then
    pass "new logdir is created with mode 1733"
else
    fail "new logdir is created with mode 1733 (exit $rc, mode $mode)"
fi

mkdir -p "$T/mklogs2"
chmod 733 "$T/mklogs2"
write_conf "$T/mklogs2"
"$REPLAY" >/dev/null 2>&1
mode=$(stat -c %a "$T/mklogs2")
if [ "$mode" = 1733 ]; then
    pass "existing logdir is corrected to mode 1733"
else
    fail "existing logdir is corrected to mode 1733 (mode $mode)"
fi

printf 'not a directory\n' > "$T/afile"
chmod 644 "$T/afile"
write_conf "$T/afile"
"$REPLAY" >/dev/null 2>&1
rc=$?
mode=$(stat -c %a "$T/afile")
if [ "$rc" -ne 0 ] && [ "$mode" = 644 ]; then
    pass "a non-directory logdir is refused and left alone"
else
    fail "a non-directory logdir is refused and left alone (exit $rc, mode $mode)"
fi

echo "=== configuration comments"
rm -rf "$T/cmtlogs" "$T"/cmtlogs#*
write_conf "$T/cmtlogs # where recordings go"
"$REPLAY" >/dev/null 2>&1
if [ -d "$T/cmtlogs" ] && ! ls "$T" | grep -q 'cmtlogs#'; then
    pass "an inline # comment is not part of the value"
else
    fail "an inline # comment is not part of the value ($(ls "$T" | grep cmtlogs | tr '\n' ' '))"
fi

echo "=== configuration validation"
write_conf "$T/logs" "syslog.facility = LOG_DAEMON" "defaultshell = /bin/true"
"$REPLAY" >"$T/out" 2>"$T/err"
rc=$?
if [ "$rc" -ne 0 ] && grep -q 'not in /etc/shells' "$T/err"; then
    pass "defaultshell is checked against /etc/shells after syslog.facility"
else
    fail "defaultshell is checked against /etc/shells after syslog.facility (exit $rc)"
fi

echo
if [ "$FAILS" -eq 0 ]; then
    echo "All tests passed."
else
    echo "$FAILS test(s) failed."
fi
exit "$FAILS"
