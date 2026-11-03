# TODO

Status: `[ ]` open, `[~]` in progress, `[x]` done.

## Open

- [ ] Consider running `build-test/bsd-test.sh` in CI. (See `RESEARCH.md`.) GitHub-hosted Ubuntu runners provide `/dev/kvm`, but the 650 MB image would need caching between runs.
- [ ] Only FreeBSD 14.5 has been tested. Other BSDs (OpenBSD, NetBSD, DragonFly) and other FreeBSD releases are untested, and the `LD_PRELOAD` tests are skipped on every non-Linux system.
- [ ] Decide whether `-c arg allow` should restrict arguments and not just the command name. Today a user can pass any arguments to an allowed program (apart from shell metacharacters), so `find -fprint`, `rsync -e` or `scp -S` can run programs or write files. The limit is documented in the man pages; behaviour is unchanged. Options to choose from: (a) keep the current behaviour and the documented warning; (b) allow whole command lines, so `-c arg allow = scp -t` matches only that exact form; (c) allow a command plus a list of permitted options; (d) run the command without `/bin/sh -c`, splitting it into arguments ourselves, so quoting cannot hide anything. Any of (b) to (d) changes the `sudosh.conf` format and needs a decision on compatibility with existing configs and with the scp/sftp/rsync server commands that clients send. Chosen direction if restricted entries are wanted later: (c) plus (d) for restricted entries only, leaving entries without an option list on the current `/bin/sh -c` path, with tested option lists for scp, sftp-server and rsync. See `RESEARCH.md` for the options weighed.

## Done

- [x] Commit the work (the user makes commits). Make sure `build-test/*.sh` and `build-test/regress_pty.py` are committed with their executable bits. (1.8.2)
- [x] Add `build-test/bsd-test.sh`, which runs the regression suite on FreeBSD in a throwaway QEMU VM from an official, checksum-verified VM image, and document it. (1.8.1)
- [x] Make `build-test/regress.sh` portable to FreeBSD: it assumed GNU `stat`, `date +%s%N`, `rsync`, `gcc`, `make`, `/proc` and an `ls` exit status of 2, and the glibc-only `LD_PRELOAD` tests must be skipped there. (1.8.0)
- [x] sudosh cannot open a pseudo-terminal on FreeBSD: it opens `/dev/ptmx` or `/dev/ptc`, and FreeBSD has neither. Use `posix_openpt()` (checked in `configure.ac`) and keep the device paths as a fallback. (1.7.9)
- [x] `bye()` runs `seteuid(orig_euid)` before `orig_euid` is set when the session fails early, so for a non-root user the recordings are left unsealed (mode 600) and the error is misleading ("Reacquiring uid 0 failed"). Found on FreeBSD; it also happens on Linux. (1.7.8)
- [x] Add automated tests for the small fixes that had none: `VMIN` in raw mode, the NULL-shell guard, and the `isspace` unsigned-char cast. (1.7.7)
- [x] Document the recommended way to allow file transfer: `internal-sftp` with `ChrootDirectory` and `ForceCommand`, `rrsync` for rsync, or `command=` in `authorized_keys`, instead of allowing scp or rsync with `-c arg allow`. (1.7.6)
- [x] `sudosh -c` runs `/bin/sh -c` with the caller's whole environment (`PATH`, `LD_PRELOAD`, `BASH_ENV` and so on); honour `clearenvironment` and use a fixed `PATH`, as interactive sessions do. (1.7.5)
- [x] `sudosh -c` does not log to syslog: record every `-c` command, allowed or refused, when `syslog.priority` is set. Refusals only print to stderr today. (1.7.4)
- [x] In `sudosh.conf.5.in` the `-c arg allow` paragraph is out of order: the original login-shell and `*` sentences ended up after the warning, as if part of it. (1.7.3)
- [x] `parse()` reads `sudosh.conf` with `fgets`, which splits a line longer than the buffer into two lines; the second part is then read as a setting of its own. (1.7.2)
- [x] `int2smalltime` drops minutes and seconds once hours are shown, so a 1 hour 1 minute session is listed as `1h`. (1.7.1)
- [x] sudosh gives no warning when a user name contains the file name delimiter, so the session is recorded but `sudosh-replay` cannot list it. (1.7.0)
- [x] `sudosh-replay` cannot parse user names longer than 32 characters: it reads at most 32 although sudosh writes up to 128. (1.6.9)
- [x] `rawmode()` and the terminal restore sit inside `#ifdef TCGETS`, so on BSD the terminal may never be put in raw mode. Use the portable `tcgetattr`/`tcsetattr` calls, and test on a BSD system. (1.6.8)
- [x] Check the new `regression` job and the strict-build step in the GitHub Actions workflow. They have only been run locally, and the tests rely on the runner's username and tools (`rsync`, `python3`, `gcc`). (1.6.7)
- [x] Investigate why the syslog-capture regression test fails under AddressSanitizer (suspected conflict with the preload test library; passes in the normal build). (1.6.6)
- [x] `sudosh.spec` still creates `/var/log/sudosh` with mode 0733; it should be 1733 like the code and man page. (1.6.5)
- [x] `findms()` passes a NULL `ptsname()` result to `open()`. It was not a crash (the call fails with EFAULT and sudosh exits with status 1), but the error was unclear. (1.6.4)
- [x] The session suffix generator is seeded from the clock and pid only; seed it from `/dev/urandom`. (1.6.3)
- [x] The session child closes only file descriptors 3 to 99, so higher inherited descriptors leak into the shell. (1.6.2)
- [x] `USER` is not set in the session environment, though the 2005 changelog says it is. (1.6.1)
- [x] `parse.c` passes a signed `char` to `isspace()`. (1.6.0)
- [x] The "not in /etc/shells" `mysyslog` call is missing the `priority != -1` guard. (1.5.9)
- [x] A NULL `pw_shell` would crash `strcmp` in the shell check. (1.5.8)
- [x] `rawmode()` sets `c_cc[VEOF]` instead of `c_cc[VMIN]`, so `VMIN` stays 0 (a read can return nothing). (1.5.7)
- [x] Log file names use `(int)` time, so timestamps wrap negative after 2038-01-19 and replay cannot parse the names. (1.5.6)
- [x] A failed `exec` in the `-c` path exits 0 with no message. (1.5.5)
- [x] Inline `#` comments on a `sudosh.conf` line become part of the value. (1.5.4)
- [x] `clearenvironment = false` still clears the environment; accept false, off, 0 and disabled as well as values starting with no. (1.5.3)
- [x] Document that `-c arg allow` checks only the command name, so allowed programs with command-running options (`find -fprint`, `rsync -e`, `scp -S`) are escapes. (1.5.2)
- [x] The `-c` interactive log records only the first word of the command, not its arguments. (1.5.1)
- [x] SIGHUP (a dropped connection) kills sudosh without finalising the logs (mode 600, no stop message); handle it, and finish the shutdown on write errors. (1.5.0)
- [x] Replay stops a recorded chunk at the first NUL byte (`fputs`); write the exact length. (1.4.9)
- [x] Replay pauses under 0.1 s vanish: the fractional part is parsed by `%i`, which reads a leading zero as octal. (1.4.8)
- [x] `sudosh-replay` prints names and headers from log files raw, so forged files can inject terminal escape codes; replace control characters. (1.4.7)
- [x] `sudosh-replay` trusts any file in the log directory; accept a file only if its owner matches the `to` user in its name. (1.4.6)
- [x] `sudosh-replay` hangs on a FIFO planted in the log directory; skip anything that is not a regular file. This also replaces the `#ifdef linux` directory check, which is undefined in strict ISO mode. (1.4.5)
- [x] `sudosh-replay` exits instead of skipping the session when one time file is malformed (`count_dur`), hiding every other session. (1.4.4)
- [x] `sudosh-replay` segfaults when a log file name has an out-of-range timestamp (`localtime` returns NULL). (1.4.3)
- [x] 1.0.7 release date dealt with: the `v1.0.7` tag date (2019-07-16) is used; the 2015 date in the old ChangeLog is not (1.4.2).
- [x] `debian/changelog` archived; not maintained for this release (1.4.1).
- [x] Low-priority review findings fixed (1.3.6–1.4.0): `ispunct` on an unsigned char, newline on the replay `invalid float` messages (1.3.7), `read(0)` retried on EINTR (1.3.8), `-c` interactive log created with `O_EXCL` (1.3.9), redundant `c_str` check removed (1.4.0). No automated tests yet for these five.
- [x] Man pages updated: `sudosh.1.in` documents `-c` and its restrictions, `sudosh.conf.5.in` documents the 1733 log directory and the `-cargallow` size limit (1.3.5).
- [x] Regenerated `configure` and `Makefile.in` with `autoreconf -fi` (1.3.4).
- [x] Automated tests for the timing, terminal-owner, login-name, defaultshell and one-byte-write fixes (1.3.3).
- [x] Strict ISO build (`-std=c17 -O2`) works: `_DEFAULT_SOURCE` set in `src/Makefile.am`, `__USE_*` workaround removed from `super.h`, `parse.c` truncation warning fixed. Strict build also added to CI (1.3.2).
- [x] Regression suite in CI: new `regression` job in `.github/workflows/build-test.yml` runs `build-test/check-version.sh` and `build-test/regress.sh`. Not yet committed.
- [x] Regression suite `build-test/regress.sh` and pty driver `build-test/regress_pty.py`. Not yet committed.
- [x] Version check `build-test/check-version.sh`, which checks configure.ac, configure, README, sudosh.spec and CHANGELOG.md agree.
- [x] Converted `ChangeLog` to `CHANGELOG.md` (Keep a Changelog format). The old file is removed and `Makefile.am`, `Makefile.in` and `sudosh.spec` now refer to `CHANGELOG.md`. Conversion did not change the version.
- [x] Added `Version:` line to README and synced `sudosh.spec` to the project version (1.0.8 at the time of conversion).
- [x] Fix `-c` shell injection via `$(...)` and newlines (1.0.9).
- [x] Fix `sudosh-replay` stack overflow on oversized chunks, and skip old `-1` timing entries (1.1.0).
- [x] Fix `sudosh-replay -d 0` reading past its buffer (1.1.1).
- [x] Fix SIGTERM handler being registered on the SIGWINCH handler (1.1.2).
- [x] Fix session suffix repeating characters (1.1.3).
- [x] Fix MAXWAIT dividing the cap by the speed multiplier twice (1.1.4).
- [x] Fix timing stamp taken before `select()`, shifting every pause by one chunk (1.1.5).
- [x] Fix `mklogdir()` comparing an uninitialised `stat` after `mkdir` (1.1.6).
- [x] Fix `mklogdir()` chmodding an existing regular file (1.1.7).
- [x] Fix log directory missing the sticky bit; now mode 1733 (1.1.8).
- [x] Fix existing log directories never having their permissions corrected (1.1.9).
- [x] Fix `defaultshell` `/etc/shells` check being skipped when `syslog.facility` is set (1.2.0).
- [x] Fix `defaultshell` error message printing the wrong value (1.2.1).
- [x] Fix NULL `getpwuid` dereference in the tty-owner lookup (1.2.2).
- [x] Fix `sudosh-replay` reading before the start of short filenames (1.2.3).
- [x] Fix one unreadable interactive log hiding the whole replay listing (1.2.4).
- [x] Fix syslog start message used as a printf format (1.2.5).
- [x] Fix signal handlers calling non-async-safe functions; shutdown now runs from the main loop (1.2.6).
- [x] Fix short and interrupted writes ending or dropping data (1.2.7).
- [x] Fix `--enable-recordinput` logging only the first byte of each read (1.2.8).
- [x] Fix lone input logs listed as broken sessions in replay (1.2.9).
- [x] Fix `-cargallow` overflowing its buffer in `sudosh.conf` (1.3.0).
- [x] Fix `sudosh -c ""` starting an interactive shell (1.3.1).
