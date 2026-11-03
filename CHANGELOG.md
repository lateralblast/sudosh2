# Changelog

All notable changes to sudosh2 are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Versions are `MAJOR.MINOR.PATCH`, where no component goes above 9 (so `1.0.9` is followed by `1.1.0`).

## [Unreleased]

## [1.8.1] - 2026-11-03

### Added

- `build-test/bsd-test.sh` runs the regression suite on FreeBSD in a throwaway QEMU virtual machine, from an official FreeBSD VM image that is downloaded once and checked against FreeBSD's published SHA256. It boots a copy-on-write overlay with a key made for the run, copies the source in, runs `check-version.sh` and `regress.sh` as an ordinary user, and deletes the VM. Commands: `test`, `start`, `sync`, `ssh`, `stop`, `clean`. The README describes it.

## [1.8.0] - 2026-11-03

### Changed

- `build-test/regress.sh` runs on FreeBSD as well as Linux. It no longer assumes GNU `stat`, `date +%s%N`, `rsync`, `gcc`, `make` or `/proc`, or that `ls` exits with 2, and it skips the glibc-only LD_PRELOAD tests on other systems. It uses `#!/usr/bin/env bash`.

## [1.7.9] - 2026-11-03

### Fixed

- sudosh can open its pseudo-terminal on FreeBSD. It opened `/dev/ptmx` or `/dev/ptc`, which FreeBSD does not have, so it could not start a session there. It now uses `posix_openpt()` where `configure` finds it, and still falls back to the device paths. `configure.ac` checks for `posix_openpt`.

## [1.7.8] - 2026-11-03

### Fixed

- When a session fails to start (for example no pseudo-terminal can be opened), sudosh seals the recordings and reports the real error. It used to try to regain root with an effective user id it had not recorded yet, so for a non-root user the call failed, the recordings stayed at mode 600, and the message was "Reacquiring uid 0 failed". Found while testing on FreeBSD.

## [1.7.7] - 2026-11-03

### Changed

- Regression tests cover the raw-mode `VMIN` setting, the refusal when no shell can be found, and the unsigned-character cast before `isspace`. The `VMIN` test also showed that the 1.5.7 entry was wrong to call `VEOF` and `VMIN` the same setting on Linux; that entry is corrected.

## [1.7.6] - 2026-11-03

### Changed

- The man pages and README recommend setting up file transfer in `sshd` (`internal-sftp` with `ChrootDirectory`, `rrsync` or a fixed `command=` in `authorized_keys`) instead of allowing scp or rsync with `-c arg allow`. They also describe the `-c` syslog logging and its minimal environment.

## [1.7.5] - 2026-11-03

### Changed

- `sudosh -c` runs the command with a minimal environment (`HOME`, `LOGNAME`, `USER` and a fixed `PATH`), as interactive sessions do, unless `clearenvironment = false`. It used to pass on the caller's whole environment, including `PATH`, `LD_PRELOAD` and `BASH_ENV`. A command that needs a variable must now be allowed with `clearenvironment = false`, and a program outside the fixed `PATH` needs its full path in the `-c arg allow` entry.

## [1.7.4] - 2026-11-03

### Added

- With `syslog.priority` set, `sudosh -c` logs every command to syslog, as `running -c command …` or `refused -c command …`, with the user and the command. Refused commands were only printed to stderr before. Control characters in the command are replaced so it cannot forge a second log line.

## [1.7.3] - 2026-11-03

### Fixed

- The `-c arg allow` section of the `sudosh.conf` man page is in the right order again. Text about login shells and `*` had ended up after the warning, as if part of it.

## [1.7.2] - 2026-11-03

### Fixed

- A line in `sudosh.conf` that is longer than the read buffer is ignored whole, with a warning. It used to be split in two and the second half read as a setting of its own, so a very long comment line could hide a setting.

## [1.7.1] - 2026-11-03

### Fixed

- Durations in the `sudosh-replay` listing show every non-zero unit, such as `1h1m1s`. Minutes and seconds were dropped once the session was an hour long, so `1h1m1s` was shown as `1h`.

## [1.7.0] - 2026-11-03

### Added

- sudosh prints a warning at the start of a session, and for `-c`, when a user name contains the file name delimiter. The session is recorded, but `sudosh-replay` splits names at the delimiter and cannot list it. The warning says to choose another delimiter in `sudosh.conf`.

## [1.6.9] - 2026-11-03

### Fixed

- `sudosh-replay` lists and replays sessions whose user names are up to 128 characters long, the length sudosh writes. It read only 32, so longer names were reported as invalid files.

## [1.6.8] - 2026-11-03

### Fixed

- The terminal is put in raw mode, and restored on exit, with `tcgetattr` and `tcsetattr`. The Linux-only `TCGETS` and `TCSETS` ioctls were used before, inside `#ifdef`s, so on other systems such as BSD the terminal was never put in raw mode. Not tested on BSD.

## [1.6.7] - 2026-11-03

### Fixed

- The regression suite no longer needs the automake version that made the committed `configure` and `Makefile.in`. On a machine with an older automake (such as Ubuntu 24.04) it failed to build. The CI job's steps now pass in an Ubuntu 24.04 container, as a non-root user, from a fresh checkout.

## [1.6.6] - 2026-11-03

### Changed

- The LD_PRELOAD test library also hooks `__syslog_chk`, which optimised builds call instead of `syslog`. The syslog test used to fail in a build with optimisation and sanitizers; the whole regression suite now passes under AddressSanitizer and UBSan.

## [1.6.5] - 2026-11-03

### Fixed

- The RPM spec creates `/var/log/sudosh` with mode 1733, matching the code and man page. It still said 0733, so a package install and `rpm -V` disagreed with what sudosh sets.

## [1.6.4] - 2026-11-03

### Changed

- sudosh checks the result of `ptsname()` and reports `ptsname` as the failure. A NULL result was passed on to `open()`, which failed harmlessly with a less clear message.

## [1.6.3] - 2026-11-03

### Fixed

- The random part of session file names is seeded from `/dev/urandom`. The clock and process id alone allowed only a few million different values, so a name could be guessed; they are used only if the system source cannot be read.

## [1.6.2] - 2026-11-03

### Fixed

- The session shell does not inherit file descriptors above 99. Only descriptors 3 to 99 were closed before, so a higher one open in the caller leaked into the shell.

## [1.6.1] - 2026-11-03

### Fixed

- The session environment sets `USER` as well as `LOGNAME`, `HOME`, `SHELL`, `TERM` and `PATH`. It was never set, though the 2005 changelog said it was.

## [1.6.0] - 2026-11-03

### Fixed

- The config parser passes `isspace` an unsigned character, so a byte above 127 in `sudosh.conf` no longer makes the call undefined.

## [1.5.9] - 2026-11-03

### Fixed

- sudosh does not call syslog for a shell that is not in `/etc/shells` when syslog is not configured. It passed an invalid priority of -1.

## [1.5.8] - 2026-11-03

### Fixed

- sudosh reports "Could not determine a valid shell" instead of crashing when `SHELL` is unset and the password entry has no shell. The check that handled this ran after the code that crashed.

## [1.5.7] - 2026-11-03

### Fixed

- The terminal is put in raw mode by setting `VMIN` and `VTIME`. The old code set `VEOF`, which is a different setting on Linux and BSD, so `VMIN` stayed 0 and a read could return nothing. (Corrected in 1.7.7: this entry first said the two were the same setting on Linux.)

## [1.5.6] - 2026-11-03

### Fixed

- Log file names use a `long` timestamp. After 2038-01-19 the `int` timestamp wrapped negative, which put the file name delimiter into the name so `sudosh-replay` could not list the session.

## [1.5.5] - 2026-11-03

### Fixed

- `sudosh -c` prints an error and exits with status 127 when it cannot run `/bin/sh`. It exited 0 without a message, so a command that never ran looked successful.

## [1.5.4] - 2026-11-03

### Fixed

- A `#` in `sudosh.conf` starts a comment that runs to the end of the line, including after a value. It used to become part of the value, so `logdir = /var/log/sudosh # recordings` created a directory with the comment in its name.

## [1.5.3] - 2026-11-03

### Fixed

- `clearenvironment = false` keeps the environment, as the man page says. `false`, `off`, `0` and `disabled` are accepted as well as values starting with `no`, in any case. Only values starting with `no` worked before.

## [1.5.2] - 2026-11-03

### Changed

- The man pages warn that `-c arg allow` checks only the command name, not its arguments, so an allowed program with options that run programs or write files (find -exec, find -fprint, rsync -e, scp -S) can be used to get around it.

## [1.5.1] - 2026-11-03

### Fixed

- The `-c` log records the whole command line, with its arguments, not just the first word. `sudosh-replay` shows it in the ID column.

## [1.5.0] - 2026-11-03

### Fixed

- A dropped connection (SIGHUP), SIGINT or SIGQUIT now ends the session through the normal shutdown, so the recordings are closed, set read-only and the stop message is logged. A write error on the terminal does the same instead of exiting at once. Before, the recordings were left writable (mode 600) with no end of session entry.

## [1.4.9] - 2026-11-03

### Fixed

- `sudosh-replay` writes each recorded chunk at its full length. A NUL byte in the output cut the chunk short, so sessions that printed binary data replayed incomplete.

## [1.4.8] - 2026-11-03

### Fixed

- `sudosh-replay` waits for the recorded length of short pauses. Fractions with a leading zero (such as 0.09 s) were read as octal, so most pauses under 0.1 s were skipped.

## [1.4.7] - 2026-11-03

### Fixed

- Security: `sudosh-replay` shows control characters in log file names and headers as `?`, so a forged log file can no longer send escape sequences to the terminal that lists or replays it.

## [1.4.6] - 2026-11-03

### Fixed

- Security: `sudosh-replay` ignores a log file that is not owned by the user named in its file name (or by root). The log directory is writable by everyone, so anyone could plant a recording that looked genuine.

## [1.4.5] - 2026-11-03

### Fixed

- `sudosh-replay` skips anything in the log directory that is not a regular file. A planted FIFO used to make it hang, and symlinks were followed. The `linux` macro check for directories, which is undefined in strict ISO mode, is replaced.

## [1.4.4] - 2026-11-03

### Fixed

- `sudosh-replay` lists the other sessions when one time file is malformed or unreadable. That session shows a duration of 0 and a warning, instead of the whole listing stopping.

## [1.4.3] - 2026-11-03

### Fixed

- `sudosh-replay` no longer crashes on a log file whose name has an out-of-range timestamp. The date shows as `(bad date)`.

## [1.4.2] - 2026-11-03

### Changed

- The 1.0.7 release date is the `v1.0.7` tag date, 2019-07-16. The 2015 date in the old ChangeLog is not used.

## [1.4.1] - 2026-11-03

### Changed

- `debian/changelog` is archived and no longer maintained. It still describes an old 2.0.1 package and is not updated for this release.

## [1.4.0] - 2026-11-03

### Changed

- `sudosh -c` no longer repeats a check that the command is non-empty; the empty-command refusal in 1.3.1 is the only check now.

## [1.3.9] - 2026-11-03

### Fixed

- `sudosh -c` creates its interactive log with `O_EXCL`, so it refuses to write through an existing file or symlink.

## [1.3.8] - 2026-11-03

### Fixed

- A signal that interrupts the read of terminal input no longer ends the session. The read is retried on EINTR.

## [1.3.7] - 2026-11-03

### Fixed

- `sudosh-replay` invalid timing messages end with a newline, so the shell prompt no longer runs onto the error.

## [1.3.6] - 2026-11-03

### Fixed

- The delimiter check passes `ispunct` an unsigned character, so a delimiter byte above 127 no longer makes the call undefined.

## [1.3.5] - 2026-11-03

### Changed

- The man pages describe the `-c` option and its restrictions, the 1733 log directory mode, and the size limit on `-c arg allow` entries. A malformed `default shell` heading in `sudosh.conf.5` is fixed.

## [1.3.4] - 2026-11-03

### Changed

- `configure` and `Makefile.in` regenerated with `autoreconf -fi`, so the `src/Makefile.am` strict-mode define and the `CHANGELOG.md` distribution entry take effect in the generated build files.

## [1.3.3] - 2026-11-03

### Changed

- Regression tests cover the session timing, terminal-owner, login-name, defaultshell and one-byte-write fixes. They use an LD_PRELOAD test library in `build-test/regress_preload.c` and a pty driver in `build-test/regress_pty.py`. Each test was checked against the bug it guards.

## [1.3.2] - 2026-11-03

### Fixed

- sudosh builds in strict ISO mode (`-std=c17 -O2` with `-Werror`). The POSIX/BSD interfaces are requested with `_DEFAULT_SOURCE` in `src/Makefile.am`, the old `__USE_*` workaround in `super.h` is removed, and `parse.c` no longer triggers a truncation warning.

## [1.3.1] - 2026-11-03

### Fixed

- `sudosh -c` with an empty command is refused. It used to skip the allow list and start an interactive shell.

## [1.3.0] - 2026-11-03

### Fixed

- `sudosh.conf` is refused when its `-cargallow` entries do not fit the allow list, instead of writing past the end of the buffer.

## [1.2.9] - 2026-11-03

### Fixed

- `sudosh-replay` no longer lists an input log as a session when it has no matching script log. The listing reported it as missing script information.

## [1.2.8] - 2026-11-03

### Fixed

- With `--enable-recordinput`, every keystroke in a read is written to the input log. Before, only the first byte of a read was logged, and after the first ordinary key the special keys (Enter, Ctrl-C, Ctrl-D, Ctrl-Z, Esc) were no longer logged.

## [1.2.7] - 2026-11-03

### Fixed

- Writes to the terminal, session logs and timing log retry after a short write and after EINTR. Before, a signal during a write, or a partial write, ended the session or silently dropped data.

## [1.2.6] - 2026-11-03

### Fixed

- SIGTERM and SIGCHLD only record the request in their handlers, and the session shuts down from the main loop. The shutdown used to run stdio, syslog and seteuid calls inside the signal handler, which is not safe.

## [1.2.5] - 2026-11-03

### Fixed

- The session start message is passed to syslog as an argument instead of as a format string, so a `%` in a user or shell name is logged as written.

## [1.2.4] - 2026-11-03

### Fixed

- An unreadable interactive session log is skipped with a warning. Before, it stopped `sudosh-replay` from listing any sessions.

## [1.2.3] - 2026-11-03

### Fixed

- `sudosh-replay` checks log file name suffixes without reading before the start of short names. The compressed-file check also matches `.bz2` correctly.

## [1.2.2] - 2026-11-03

### Fixed

- sudosh no longer crashes when the owner of the terminal has no password entry. The lookup result was dereferenced without a check.

## [1.2.1] - 2026-11-03

### Fixed

- The error for a defaultshell that cannot be stat'ed names the configured shell. It printed the last value read from the config file instead.

## [1.2.0] - 2026-11-03

### Fixed

- The defaultshell check against /etc/shells always ran, even when syslog.facility was set. The shell lookup shared a flag with the facility check, so a valid facility let any defaultshell through.

## [1.1.9] - 2026-11-03

### Fixed

- The log directory permissions are checked on every run, including when the directory already exists. Before, only a missing directory was created and corrected, so the 1733 mode from 1.1.8 did not reach existing installations.

## [1.1.8] - 2026-11-03

### Changed

- The log directory is created and kept with mode 1733 (sticky bit set) instead of 0733. Users can still write new logs but cannot delete or rename other users' logs. An existing log directory is corrected the next time sudosh or sudosh-replay runs.

## [1.1.7] - 2026-11-03

### Fixed

- A log directory path that already exists as a regular file is refused without changing that file's permissions. It used to be chmod'ed to 0733 before the error was reported.

## [1.1.6] - 2026-11-03

### Fixed

- Log directory creation compares the permissions of the directory it just made. It used to compare an uninitialised stat buffer, so the permission fix-up after mkdir ran by chance.

## [1.1.5] - 2026-11-03

### Fixed

- Session timing is stamped when output arrives, not when sudosh starts waiting for it. The old stamp shifted every recorded pause by one chunk, so replay paused at the wrong moments. Timing entries are no longer written with a byte count of -1 at session end.

## [1.1.4] - 2026-11-03

### Fixed

- `sudosh-replay` MAXWAIT caps a long pause at MAXWAIT seconds of playback. The cap was divided by the speed multiplier twice, so capped pauses were too short.

## [1.1.3] - 2026-11-03

### Fixed

- The random suffix of session file names is seeded once per process, so characters are no longer repeated in runs. The string is also NUL-terminated explicitly.

## [1.1.2] - 2026-11-03

### Fixed

- SIGTERM runs the session shutdown (terminal restore, log close, syslog stop message). It was registered with the SIGWINCH handler, so SIGTERM only resized the pty and the session kept running.

## [1.1.1] - 2026-11-03

### Fixed

- `sudosh-replay -d 0` prints the logged bytes with `fwrite` using the count `fread` returned. The old `fputs` read past the end of the buffer and printed stack contents.

## [1.1.0] - 2026-11-03

### Fixed

- `sudosh-replay` no longer copies a timing entry larger than its read buffer, which overflowed the stack. Timing entries with a negative byte count, written by older sudosh at the end of a session, are skipped instead of being replayed as errors.

## [1.0.9] - 2026-11-03

### Fixed

- Security: `-c` allow list no longer lets an allowed first word run commands through command substitution (`$(...)`) or newlines, which were passed to `/bin/sh -c` unchecked. The allow-list match also uses the whole word instead of its first 100 characters.

## [1.0.8] - 2026-04-11

### Added

- `SFTP` usage documented in the man page (`sudosh.1.in`).

### Changed

- `configure.ac` updated for modern autoconf and automake (#52).
- Removed the forced `-Werror` from the configure script.
- Reverted the autoconf requirement to 2.5.
- Config file location fixed, and `sysconfdir` handling fixed for the config file.
- `sudosh.conf` install rule fixed for non-root builds.
- README updated.

### Fixed

- Dead code in `rawmode()` removed.
- `strncpy` warnings with modern GCC (#42).
- Build errors on newer RHEL.
- `SIGCHLD` is no longer treated as an error.
- Dotfiles in the log directory are ignored by replay.
- Alpine musl build: fallback definitions for `FS_IMMUTABLE_FL` and related flags.
- Gentoo / gcc-15 build: `getopt` type conflict resolved. Alpine 3.21 and Gentoo CI builds were disabled.

Note: the Alpine and Gentoo fixes were committed after the 1.0.8 version bump and were never given a version of their own, so they are listed here.

## [1.0.7] - 2019-07-16

### Added

- `immutable_recordings` option in `sudosh.conf`: sets the append-only flag on timing, script and input files on Linux.
- Wildcard support for the `-cargallow` entries (for ssh `-c` argument allowing).
- `.gitignore` for build artifacts.
- Packaging files for `.deb` and `.spec`.

### Changed

- `WRITE` renamed to `DO_WRITE`.
- File opening for script, timing and input moved into a function.
- Man page for `sudosh.conf` describes the `clearenvironment` option.
- Documentation updated.
- `sudosh-replay` skips directories and archives.
- Variable declarations moved so the code builds with older compilers.
- Terminal flags no longer strip the 8th bit, so multibyte characters work.

### Fixed

- Invalid timing files no longer silently break all replay.
- Replay of a single file no longer behaves as O(N).
- Missing variable initialization in `sudosh-replay`.
- `fprintf` format errors.
- `getopt.c` includes `string.h`.
- `-Wformat-truncation` compile errors with newer GCC.
- Manpage comment in `sudosh.conf`.
- Directory checks in `replay.c` apply only on Linux.
- Changelog formatting fixed for rpmbuild.
- The `-cargallow` wildcard no longer disallows ordinary characters that the wildcard already permits.

## [1.0.6] - 2015-04-29

### Changed

- Version numbers corrected from an erroneous `1.0.4`.

### Fixed

- A stray `:` at the end of `PATH` put `::` into root's `PATH`, which violated the CIS standard.

## [1.0.4] - 2010-07-09

### Added

- Input logging is optional and disabled by default; enable it with the `--enable-recordinput` configure option.
- Updated `.spec` file.

### Fixed

- Compile issues on non-Linux systems (mostly HP-UX).
- File descriptor issues on non-Linux systems (Solaris).

## [1.0.1] - 2008-01-31

### Changed

- Config file parsing rewritten to be simpler and safer. Whitespace in config files is now ignored.
- The `-c` argument allow list always works when sudosh is used as a login shell, so tools such as scp, sftp, rsh and cvs can be permitted.

### Fixed

- Potential crash in `sudosh.c`.

## [1.0.0] - 2007-12-20

### Added

- New SourceForge project `sudosh2`, released as v1.0.

### Changed

- The log directory no longer has to be created by hand. It is checked and fixed each time sudosh runs; the `-i` option is accepted but ignored.

### Fixed

- Crash in config file parsing.
- Non-reentrant password entry handling.

## [pre-1.0] - 2004-02-12 to 2007-12-19

Development before the first sudosh2 release. The original ChangeLog did not record version numbers for these entries, so they are grouped by date here.

### Added

- 2004-02-12: initial release 0.1.0.
- 2004-03-16: AIX (greater than 4.3.3) support.
- 2004-09-24: first stable release, with Solaris, Linux, HP-UX and FreeBSD support.
- 2004-10-06: logging of keystrokes, and a `script(1)`-style log of input and output in `~/.sudosh_history`.
- 2004-10-09: GNU Autoconf support for cross-platform builds.
- 2004-10-12: AIX support, and `~/.sudosh_input` for keystrokes.
- 2004-10-18: `sudosh-replay` utility to play back recorded sessions; logdir automation in configure.
- 2004-10-25: `sudosh-replay` reports its version; configuration summary in configure.
- 2004-10-25: platform support for alpha-dec-osf, HP-UX 11, AIX 4.3.3 to 5.2, Solaris 2.8 and 2.9, SuSE and RHEL 3.
- 2005-02-14: `-i` / `--init` option.
- 2005-02-19: "to user" field in logs, showing which user sudosh switched to.
- 2005-03-05: `bash_profile` and `sudosh.spec` (thanks to Chris MacLeod).
- 2005-03-08: man pages `sudosh.1` and `sudosh-replay.8`.
- 2005-04-12: monitoring of log files, with syslog alerts when they are deleted or modified.
- 2005-05-07: random string in log file names, so unprivileged users cannot delete their own logs.
- 2005-05-09: `MAXWAIT` option for `sudosh-replay` (by Luc Charland).
- 2005-05-09: `setpenv()` support for AIX.
- 2005-05-11: sudosh can be used directly as a login shell in `/etc/passwd`; `--with-defshell` and `--with-filedelimit` configure options.
- 2005-05-30: `parse.c` reads a configuration file of `key = value` pairs.
- 2005-06-12: `-c` option so that sudosh can run remote commands such as rsync and scp.
- 2005-06-12: configuration file support.
- 2005-06-12: `sudosh-replay` logs non-interactive commands as well.

### Changed

- 2004-10-01: sudosh ported from Perl to C.
- 2004-10-18: syslog removed; all logs are kept locally.
- 2004-10-20: `syslog.c` removed.
- 2005-02-19: `sudosh-replay` rewritten to be more user-friendly.
- 2005-03-05: LOGDIR permissions handling changed.
- 2005-03-22: environment cleaned up; `TERM` is passed through.
- 2005-05-07: shell runs as a login shell (argv[0] prefixed with `-`); `LOGNAME`, `USER`, `TERM`, `SHELL` and `PATH` set; working directory set to `HOME`.
- 2005-05-07: tampering code removed because it was not portable.
- 2005-05-12: `TERM` no longer required; defaults to `dumb`.
- 2005-05-19: `FILEDELIMIT` renamed to `FDL`.
- 2005-05-20: about 90% of `sudosh-replay` ported to C; completed on 2005-05-30.
- 2005-05-21: NULL uses cast to `(char *) 0`.
- 2005-05-30: `-ansi -pedantic -Wall` added to CFLAGS; code compiles cleanly.

### Removed

- 2004-10-18: keystroke logging to syslog, because passwords could be written to syslog.
- 2005-05-07: tampering code.

### Fixed

- 2004-10-12: keystroke bug in `sudosh.c`; backspace and delete now logged readably.
- 2004-10-21: AIX and Solaris compile errors.
- 2004-10-24: major security hole in handling of the `SHELL` environment variable.
- 2004-10-25: environment was blanked out during exec, which broke the new shell.
- 2004-10-27: configure fixed for AIX.
- 2005-02-19: critical bug where sudosh reported the wrong user when run through sudo.
- 2005-03-11: `HOME` environment variable corrected.
- 2005-04-13: compile error on Solaris.
- 2005-05-09: version typo corrected.
- 2005-05-12: delimiter bug in `sudosh-replay`.
- 2005-06-12: `TIOCSCTTY` ioctl added (by Michael Grubb).
