# Research

Ideas considered but not built, so they are not lost and not re-litigated. Open work is in [TODO.md](TODO.md).

## Restricting arguments of `-c arg allow` commands

**Idea.** `-cargallow` currently matches only the command name, so an allowed program can be given any arguments except shell metacharacters. Options considered:

- (a) Keep the behaviour and the documented warning.
- (b) Allow whole command lines, so `scp -t` matches only that exact form.
- (c) Allow a command plus a list of permitted options per entry (for example `scp:-t,-f,...`). Existing config lines keep working.
- (d) Tokenise the command and `execve` it without `/bin/sh -c`, so quoting cannot hide anything.

**Why it might help.** `find -fprint`, `rsync -e` and `scp -S` run programs or write files, so an allowed command can be an escape. (d) is safer than (b): (b) still goes through the shell, (d) does not.

**Open questions.**

- Direction recorded: (c) plus (d) for restricted entries only; entries without an option list stay on the `/bin/sh -c` path.
- Which option lists are right for the scp, sftp-server and rsync server commands that clients send? They need tests against real clients.
- Per-user allow-lists, a built-in blocklist of dangerous options (brittle) and a service account for `-c` commands were also considered and not preferred.
- Already done instead: the limit is documented, `-c` commands are logged to syslog, the `-c` environment is cleaned, and the recommended file-transfer set-ups (`internal-sftp` with `ChrootDirectory`, `rrsync`, `command=`) are documented.

## BSD tests in CI

**Idea.** Run `build-test/bsd-test.sh` from GitHub Actions.

**Why it might help.** FreeBSD-only breakage (pty opening, terminal modes, `stat` and `ls` differences) was found only by hand-running the VM.

**Open questions.**

- GitHub Ubuntu runners have `/dev/kvm`, but the image is about 650 MB (1.9 GB unpacked); it would need caching between runs.
- Do the extra minutes per run justify it, or should it be a scheduled or manual job?
