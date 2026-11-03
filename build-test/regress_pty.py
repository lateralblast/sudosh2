#!/usr/bin/env python3
"""Drive sudosh under a pseudo-terminal for build-test/regress.sh.

Usage: regress_pty.py MODE SUDOSH_BINARY

MODE is one of:
  sigterm  send SIGTERM to sudosh
  exit     type "exit" into the session
  input    type "ab", Enter, "exit", Enter
  hello    type "echo hello", Enter, "exit", Enter
  timing   run "sleep 2; echo done; exit"
  emptyc   run "sudosh -c ''" with no input
  wait     run sudosh with no input

Prints one line: EXITED <status>, SIGNAL <number>, or ALIVE. sudosh is killed
afterwards if it is still running.

The child gets LD_PRELOAD from REGRESS_PRELOAD when that is set. It is set only
for the child, so this driver itself is never preloaded.
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


def run(argv, keys=(), sig=None, lead=0.5):
    env = dict(os.environ, SHELL="/bin/sh", TERM="dumb")
    if os.environ.get("REGRESS_PRELOAD"):
        env["LD_PRELOAD"] = os.environ["REGRESS_PRELOAD"]
    pid, fd = pty.fork()
    if pid == 0:
        os.execve(argv[0], argv, env)

    drain(fd, lead)
    if sig is not None:
        os.kill(pid, sig)
    for key in keys:
        os.write(fd, key)
        drain(fd, 0.5)

    deadline = time.time() + 3
    while time.time() < deadline:
        drain(fd, 0.1)
        done, status = os.waitpid(pid, os.WNOHANG)
        if done == pid:
            if os.WIFSIGNALED(status):
                print("SIGNAL", os.WTERMSIG(status))
            else:
                print("EXITED", os.WEXITSTATUS(status))
            return

    os.kill(pid, signal.SIGKILL)
    os.waitpid(pid, 0)
    print("ALIVE")


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
    elif mode == "hello":
        run(argv, keys=[b"echo hello\r", b"exit\r"])
    elif mode == "timing":
        run(argv, keys=[b"sleep 2; echo done; exit\r"])
    elif mode == "emptyc":
        run([binary, "-c", ""])
    elif mode == "wait":
        run(argv)
    else:
        print(__doc__, file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
