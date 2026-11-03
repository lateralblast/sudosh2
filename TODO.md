# TODO

Status: `[ ]` open, `[~]` in progress, `[x]` done.

## Open

- [ ] Add automated tests for fixes that the regression suite does not cover yet: timing order (1.1.5), `getpwuid` NULL checks (1.2.2), defaultshell error message (1.2.1), partial-write retry (1.2.7), syslog format argument (1.2.5).
- [ ] Regenerate `configure` and `Makefile.in` with `autoreconf -fi`. They were edited by hand for the version and for `ChangeLog` → `CHANGELOG.md`, and `configure.ac` now uses `AM_INIT_AUTOMAKE([foreign])`.
- [ ] Update `sudosh.1.in` and `sudosh.conf.5.in`: the log directory is now mode 1733, `-c` refuses shell metacharacters and an empty command, and `-cargallow` has a length limit.
- [ ] Update `debian/changelog`, which still says 2.0.1 and is unrelated to the project version.
- [ ] Confirm the release date of 1.0.7. The old ChangeLog says 2015-04-29, the `v1.0.7` tag says 2019-07-16. CHANGELOG.md uses the tag date.
- [ ] Low-priority review findings not yet fixed:
  - `parse.c` calls `ispunct()` on a `char` that may be negative (undefined behaviour).
  - `sudosh.c` ends the session when `read(0)` returns EINTR, instead of retrying.
  - `sudosh.c` opens the `-c` interactive log with `fopen("w")` rather than `O_EXCL`.
  - `sudosh.c` has a redundant `if (c_str[0] != 0)` after the new empty-command check.
  - `sudosh-replay` `invalid float` error lacks a trailing newline.
- [ ] Commit the work (the user makes commits). Make sure `build-test/*.sh` and `build-test/regress_pty.py` are committed with their executable bits.

## Done

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
