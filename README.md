# sudosh2
Version: 1.8.1

sudosh2 is an auditing layer between sudo and a user's shell, that can also be used as a login shell.

User sessions are recorded and are available for later playback by the system administrator.

sudosh2 does not record command history except when executed remotely via ssh's command option. If you're looking for something to just log commands, there are other tools for that.

sudosh2 does not log sessions to syslog. If you need to consolidate session logs to a single location, logging them to a network filesystem is currently the recommended option.

sudosh2 was forked from sudosh, which was pulled by its original developer when a commercial company bought it from him. The initial project goals were to fix several major bugs, 
make it 64 bit safe, and port to newer versions of Unix operating systems. The initial targets were RHEL5 and Solaris 10, but most recent free Unix systems should work perfectly.
There are known issues with AIX and HP/UX that I can't currently fix as I don't have access to either platform. Patches for these platforms would be accepted.

sudosh2 is generally in maintenance mode. I'll accept portability issues and simple feature requests, but probably not major feature requests at this time. 

sudosh2 is under the Open Source License 2.0 (which is very similar to BSD with Attribution), a copy of wihch is in the COPYING file. This is the license chosen by the original author.

Creating an Ubuntu/Debian Package:

```
cd sudosh2
sudo apt install devscripts debhelper
sudo debuild -b -uc -us
ls -l ../*.deb
ls -l ../*.deb
-rw-r--r-- 1 root root 30628 Aug 13 15:24 ../sudosh2_2.0.1-1_amd64.deb
```

Fix for autoconf errors:

```
autoreconf -fi && ./configure && make

```

## File transfer

Letting users run scp, sftp or rsync through sudosh with `-c arg allow` only checks the command name, not its arguments. For users who just need to move files, configure `sshd` instead: `internal-sftp` with `ChrootDirectory`, `rrsync` in `authorized_keys`, or a fixed `command=`. See the FILE TRANSFER section of `sudosh.conf(5)`.

## Testing

- `build-test/check-version.sh` checks that the version is the same everywhere it is declared.
- `build-test/regress.sh` builds a copy of the tree and runs the regression suite. It runs on Linux and FreeBSD (the `LD_PRELOAD` tests are Linux only).
- `build-test/bsd-test.sh` runs that suite on FreeBSD in a throwaway QEMU virtual machine, so you can test BSD from a Linux machine. It downloads an official FreeBSD VM image once (about 650 MB, checked against FreeBSD's published SHA256), boots a copy of it, copies the source in, runs the tests as an ordinary user, and deletes the copy. It needs `qemu-system-x86_64`, `qemu-img`, `xz`, `curl`, `ssh` and `genisoimage` (or `mkisofs` or `xorriso`), and `/dev/kvm` to be fast. Run `build-test/bsd-test.sh --help` for the commands (`test`, `start`, `sync`, `ssh`, `stop`, `clean`) and options.

