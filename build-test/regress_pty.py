#!/usr/bin/env python3
"""Drive sudosh under a pseudo-terminal for build-test/regress.sh.

Usage: regress_pty.py MODE SUDOSH_BINARY

MODE is one of:
  sigterm  send SIGTERM to sudosh
  sighup   send SIGHUP to sudosh, as a dropped connection would
  exit     type "exit" into the session
  input    type "ab", Enter, "exit", Enter
  hello    type "echo hello", Enter, "exit", Enter
  timing   run "sleep 2; echo done; exit"
  envshow  print the FOO and USER variables the session shell sees
  fds      print the file descriptors the session shell holds open
  emptyc   run "sudosh -c ''" with no input
  wait     run sudosh with no input

Prints one line: EXITED <status>, SIGNAL <number>, or ALIVE. sudosh is killed
afterwards if it is still running. The envshow and fds modes then print one
more line of the form NAME=value.

The child gets LD_PRELOAD from REGRESS_PRELOAD when that is set. It is set only
for the child, so this driver itself is never preloaded.
"""
import os
import pty
import re
import select
import signal
import sys
import time


def drain(fd, seconds, sink):
    end = time.time() + seconds
    while time.time() < end:
        ready, _, _ = select.select([fd], [], [], 0.05)
        if ready:
            try:
                data = os.read(fd, 4096)
                if not data:
                    return
                sink.append(data)
            except OSError:
                return


def run(argv, keys=(), sig=None, lead=0.5):
    """Run argv in a pty. Returns (status line, captured output text)."""
    env = dict(os.environ, SHELL=os.environ.get("REGRESS_SHELL", "/bin/sh"), TERM="dumb")
    if os.environ.get("REGRESS_PRELOAD"):
        env["LD_PRELOAD"] = os.environ["REGRESS_PRELOAD"]
    pid, fd = pty.fork()
    if pid == 0:
        os.execve(argv[0], argv, env)

    out = []
    drain(fd, lead, out)
    if sig is not None:
        os.kill(pid, sig)
    for key in keys:
        os.write(fd, key)
        drain(fd, 0.5, out)

    status_line = "ALIVE"
    deadline = time.time() + 3
    while time.time() < deadline:
        drain(fd, 0.1, out)
        done, status = os.waitpid(pid, os.WNOHANG)
        if done == pid:
            if os.WIFSIGNALED(status):
                status_line = "SIGNAL %d" % os.WTERMSIG(status)
            else:
                status_line = "EXITED %d" % os.WEXITSTATUS(status)
            break

    if status_line == "ALIVE":
        os.kill(pid, signal.SIGKILL)
        os.waitpid(pid, 0)
    return status_line, b"".join(out).decode("latin1").replace("\r", "")


def marker(text, name):
    """Return the value printed as name=<value>= on its own line, or ''."""
    found = re.findall(r"^%s=(.*)=$" % re.escape(name), text, re.M)
    return found[-1] if found else ""


def main():
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    mode, binary = sys.argv[1], sys.argv[2]
    argv = [binary]
    if mode == "sigterm":
        print(run(argv, sig=signal.SIGTERM)[0])
    elif mode == "sighup":
        print(run(argv, sig=signal.SIGHUP)[0])
    elif mode == "exit":
        print(run(argv, keys=[b"exit\r"])[0])
    elif mode == "input":
        print(run(argv, keys=[b"ab\r", b"exit\r"])[0])
    elif mode == "hello":
        print(run(argv, keys=[b"echo hello\r", b"exit\r"])[0])
    elif mode == "timing":
        print(run(argv, keys=[b"sleep 2; echo done; exit\r"])[0])
    elif mode == "envshow":
        status, text = run(argv, keys=[b"echo FOOVAL=$FOO=\r", b"echo USERVAL=$USER=\r", b"exit\r"])
        print(status)
        print("FOO=" + marker(text, "FOOVAL"))
        print("USER=" + marker(text, "USERVAL"))
    elif mode == "fds":
        status, text = run(argv, keys=[b"echo FDLIST=$(ls /proc/self/fd | tr '\\n' ' ')=\r", b"exit\r"])
        print(status)
        print("FDS=" + marker(text, "FDLIST"))
    elif mode == "emptyc":
        print(run([binary, "-c", ""])[0])
    elif mode == "wait":
        print(run(argv)[0])
    else:
        print(__doc__, file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
