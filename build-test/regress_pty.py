#!/usr/bin/env python3
"""Drive sudosh under a pseudo-terminal for build-test/regress.sh.

Usage: regress_pty.py MODE SUDOSH_BINARY

MODE is one of:
  sigterm  send SIGTERM to sudosh and report whether it exits
  exit     type "exit" into the session and report whether sudosh exits
  input    type "ab", Enter, "exit", Enter and report whether sudosh exits
  emptyc   run "sudosh -c ''" with no input and report whether it exits

Prints EXITED or ALIVE. sudosh is killed afterwards if it is still running.
"""
import os
import pty
import select
import signal
import sys
import time


def drain(fd, seconds):
    end = time.time() + seconds
    while time.time() < end:
        ready, _, _ = select.select([fd], [], [], 0.05)
        if ready:
            try:
                if not os.read(fd, 4096):
                    return
            except OSError:
                return


def run(argv, keys=(), sig=None):
    env = dict(os.environ, SHELL="/bin/sh", TERM="dumb")
    pid, fd = pty.fork()
    if pid == 0:
        os.execve(argv[0], argv, env)

    drain(fd, 0.5)
    if sig is not None:
        os.kill(pid, sig)
    for key in keys:
        os.write(fd, key)
        drain(fd, 0.5)

    exited = False
    deadline = time.time() + 3
    while time.time() < deadline:
        drain(fd, 0.1)
        done, _ = os.waitpid(pid, os.WNOHANG)
        if done == pid:
            exited = True
            break

    if not exited:
        os.kill(pid, signal.SIGKILL)
        os.waitpid(pid, 0)
    print("EXITED" if exited else "ALIVE")


def main():
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    mode, binary = sys.argv[1], sys.argv[2]
    argv = [binary]
    if mode == "sigterm":
        run(argv, sig=signal.SIGTERM)
    elif mode == "exit":
        run(argv, keys=[b"exit\r"])
    elif mode == "input":
        run(argv, keys=[b"ab\r", b"exit\r"])
    elif mode == "emptyc":
        run([binary, "-c", ""])
    else:
        print(__doc__, file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
