#!/usr/bin/env bash
# Run the sudosh2 regression suite on FreeBSD, in a throwaway virtual machine.
#
# It downloads an official pre-installed FreeBSD VM image (cached, and checked
# against FreeBSD's published SHA256), boots a copy of it with QEMU, copies the
# working tree in, installs the build tools with pkg, runs
# build-test/check-version.sh and build-test/regress.sh as an ordinary user, and
# deletes the VM again. The shared downloaded image is never modified: each run
# uses a copy-on-write overlay.
#
# Usage: build-test/bsd-test.sh [COMMAND] [OPTIONS]
#
# Commands:
#   test    (default) boot the VM, copy the source in, run the tests, shut down
#   start   boot the VM and leave it running
#   sync    copy the current source tree into the running VM
#   ssh     [CMD...] open a shell in the running VM, or run CMD there
#   stop    shut the VM down and delete its disk
#   clean   stop the VM and delete the cached FreeBSD image files as well
#
# Options (each can also be set in the environment variable shown):
#   --release R   FreeBSD release              (BSD_RELEASE, default 14.5-RELEASE)
#   --port N      host port for the VM's ssh   (BSD_PORT,    default 2222)
#   --memory M    VM memory                    (BSD_MEMORY,  default 2G)
#   --cpus N      VM processors                (BSD_CPUS,    default 2)
#   --cache DIR   where the image is kept      (BSD_CACHE,   default ~/.cache/sudosh2-bsd)
#   --keep        with "test", leave the VM running afterwards (a VM that was
#                 already running is always left running)
#
# Needs: qemu-system-x86_64, qemu-img, xz, curl, ssh, ssh-keygen, sha256sum,
# tar, and one of genisoimage, mkisofs or xorriso. /dev/kvm makes it much
# faster; without it QEMU falls back to slow software emulation.
#
# The VM listens only on 127.0.0.1, accepts only a key made for this run, and
# is destroyed afterwards. Exit status of "test" is that of the test run.
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
RELEASE=${BSD_RELEASE:-14.5-RELEASE}
CACHE=${BSD_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/sudosh2-bsd}
PORT=${BSD_PORT:-2222}
MEMORY=${BSD_MEMORY:-2G}
CPUS=${BSD_CPUS:-2}
KEEP=0
USER_NAME=tester

info() { echo "[bsd] $*" >&2; }
die() { echo "[bsd] error: $*" >&2; exit 2; }

usage() { sed -n '2,/^set -u$/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'; }

# --- arguments --------------------------------------------------------------
COMMAND=test
if [ $# -gt 0 ] && [[ $1 != -* ]]; then COMMAND=$1; shift; fi
SSH_ARGS=()
if [ "$COMMAND" = ssh ]; then SSH_ARGS=("$@"); set --; fi
while [ $# -gt 0 ]; do
    case $1 in
        --release) RELEASE=$2; shift 2 ;;
        --port) PORT=$2; shift 2 ;;
        --memory) MEMORY=$2; shift 2 ;;
        --cpus) CPUS=$2; shift 2 ;;
        --cache) CACHE=$2; shift 2 ;;
        --keep) KEEP=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) die "unknown option: $1 (try --help)" ;;
    esac
done

IMG="FreeBSD-$RELEASE-amd64-BASIC-CLOUDINIT-ufs.qcow2"
BASE_URL="https://download.freebsd.org/ftp/releases/VM-IMAGES/$RELEASE/amd64/Latest"
VM_DIR="$CACHE/vm-$PORT"
UA='Mozilla/5.0'   # download.freebsd.org refuses requests without a User-Agent

# --- helpers ----------------------------------------------------------------
need() {
    local t
    for t in qemu-system-x86_64 qemu-img xz curl ssh ssh-keygen sha256sum tar; do
        command -v "$t" >/dev/null 2>&1 || die "$t is required but not installed"
    done
    ISO_TOOL=
    for t in genisoimage mkisofs xorriso; do
        if command -v "$t" >/dev/null 2>&1; then ISO_TOOL=$t; break; fi
    done
    [ -n "$ISO_TOOL" ] || die "one of genisoimage, mkisofs or xorriso is required"
}

vm_running() {
    [ -f "$VM_DIR/qemu.pid" ] && kill -0 "$(cat "$VM_DIR/qemu.pid")" 2>/dev/null
}

vm_ssh() {
    ssh -i "$VM_DIR/key" -p "$PORT" -o BatchMode=yes -o IdentitiesOnly=yes \
        -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
        -o LogLevel=ERROR -o ConnectTimeout=5 "$USER_NAME@127.0.0.1" "$@"
}

# Download the image if it is not cached, and check it against FreeBSD's
# published SHA256 before using it. (The checksum comes from the same server,
# so this guards against a damaged download, not against a compromised server.)
fetch_image() {
    [ -f "$CACHE/$IMG" ] && return 0
    mkdir -p "$CACHE"
    local xz="$CACHE/$IMG.xz" sums="$CACHE/CHECKSUM.SHA256.$RELEASE" want got
    info "downloading $IMG.xz (resumes if interrupted) ..."
    curl -fL -C - -A "$UA" -o "$xz" "$BASE_URL/$IMG.xz" || die "download failed"
    curl -fsL -A "$UA" -o "$sums" "$BASE_URL/CHECKSUM.SHA256" || die "could not fetch the checksum file"
    want=$(sed -n "s/^SHA256 ($IMG.xz) = //p" "$sums")
    [ -n "$want" ] || die "no checksum for $IMG.xz in $sums"
    got=$(sha256sum "$xz" | cut -d' ' -f1)
    if [ "$want" != "$got" ]; then
        rm -f "$xz"
        die "checksum mismatch for $IMG.xz (wanted $want, got $got); deleted it, run again"
    fi
    info "checksum ok; unpacking ..."
    xz -dc "$xz" > "$CACHE/$IMG.part" && mv "$CACHE/$IMG.part" "$CACHE/$IMG" || die "could not unpack the image"
}

make_seed() {
    local pub d="$VM_DIR/seed"
    pub=$(cat "$VM_DIR/key.pub")
    mkdir -p "$d"
    cat > "$d/meta-data" <<EOF
instance-id: sudosh2-bsd-$PORT
local-hostname: sudosh2-bsd
EOF
    cat > "$d/user-data" <<EOF
#cloud-config
users:
  - name: $USER_NAME
    gecos: sudosh2 test user
    shell: /bin/sh
    ssh_authorized_keys:
      - $pub
packages:
  - gmake
  - python3
  - bash
EOF
    case $ISO_TOOL in
        xorriso) xorriso -as mkisofs -quiet -o "$VM_DIR/seed.iso" -V cidata -J -R "$d" >/dev/null 2>&1 ;;
        *) "$ISO_TOOL" -quiet -output "$VM_DIR/seed.iso" -volid cidata -joliet -rock "$d" >/dev/null 2>&1 ;;
    esac || die "could not build the cloud-init disk"
}

start_vm() {
    need
    if vm_running; then info "VM already running on port $PORT"; return 0; fi
    fetch_image
    rm -rf "$VM_DIR"; mkdir -p "$VM_DIR"
    qemu-img create -q -f qcow2 -F qcow2 -b "$CACHE/$IMG" "$VM_DIR/disk.qcow2" 12G || die "could not create the VM disk"
    ssh-keygen -q -t ed25519 -N '' -C sudosh2-bsd-test -f "$VM_DIR/key" || die "ssh-keygen failed"
    make_seed

    local accel=(-machine q35 -cpu max)
    if [ -w /dev/kvm ]; then accel=(-machine q35,accel=kvm -cpu host); else info "no /dev/kvm: using slow software emulation"; fi
    info "booting FreeBSD $RELEASE (ssh on 127.0.0.1:$PORT) ..."
    qemu-system-x86_64 "${accel[@]}" -m "$MEMORY" -smp "$CPUS" -display none \
        -serial "file:$VM_DIR/console.log" -daemonize -pidfile "$VM_DIR/qemu.pid" \
        -drive "file=$VM_DIR/disk.qcow2,if=virtio,format=qcow2" \
        -drive "file=$VM_DIR/seed.iso,media=cdrom,readonly=on" \
        -netdev "user,id=n0,hostfwd=tcp:127.0.0.1:$PORT-:22" -device virtio-net-pci,netdev=n0 \
        || die "qemu failed to start (is port $PORT free?)"

    local i
    for i in $(seq 1 120); do
        vm_ssh true 2>/dev/null && break
        vm_running || die "the VM stopped unexpectedly; see $VM_DIR/console.log"
        sleep 5
    done
    vm_ssh true 2>/dev/null || { tail -20 "$VM_DIR/console.log" >&2; die "ssh did not come up in 10 minutes"; }
    info "ssh is up; waiting for the build tools to be installed ..."
    for i in $(seq 1 120); do
        if vm_ssh 'command -v gmake && command -v python3 && command -v bash' >/dev/null 2>&1; then
            # FreeBSD's default ~/.profile runs resizewin, which asks the terminal
            # for its size and waits. The tests run sessions on a pty that never
            # answers, so the keystrokes meant for the shell were swallowed.
            vm_ssh ': > .profile' || die "could not reset the test user's .profile"
            return 0
        fi
        sleep 5
    done
    tail -20 "$VM_DIR/console.log" >&2
    die "the build tools were not installed in 10 minutes"
}

sync_src() {
    vm_running || die "no VM is running on port $PORT (use: $0 start)"
    info "copying the source tree in ..."
    vm_ssh 'rm -rf sudosh2 && mkdir sudosh2' || die "ssh failed"
    (cd "$ROOT" && tar --exclude .git -cf - .) | vm_ssh 'tar -xf - -C sudosh2' || die "copy failed"
}

run_tests() {
    info "running the tests on $(vm_ssh uname -sr) ..."
    vm_ssh 'cd sudosh2 && build-test/check-version.sh && build-test/regress.sh'
}

stop_vm() {
    if vm_running; then
        kill "$(cat "$VM_DIR/qemu.pid")" 2>/dev/null
        local i
        for i in 1 2 3 4 5 6 7 8 9 10; do vm_running || break; sleep 1; done
        info "VM stopped"
    fi
    rm -rf "$VM_DIR"
}

# --- commands ---------------------------------------------------------------
case $COMMAND in
    test)
        # A VM that was already running (from "start") is left running.
        if vm_running; then KEEP=1; fi
        trap '[ "$KEEP" = 1 ] || stop_vm' EXIT
        start_vm
        sync_src
        run_tests
        rc=$?
        if [ "$KEEP" = 1 ]; then info "left running: $0 ssh   (stop with: $0 stop)"; fi
        exit $rc
        ;;
    start) start_vm; info "running; use: $0 ssh | sync | stop" ;;
    sync) sync_src ;;
    ssh)
        vm_running || die "no VM is running on port $PORT (use: $0 start)"
        if [ ${#SSH_ARGS[@]} -gt 0 ]; then vm_ssh "${SSH_ARGS[@]}"; else vm_ssh; fi
        ;;
    stop) stop_vm ;;
    clean)
        # Delete only what this script creates, not the whole directory: --cache
        # could point at a directory that holds other files.
        stop_vm
        rm -f "$CACHE/$IMG" "$CACHE/$IMG.xz" "$CACHE/$IMG.part" "$CACHE/CHECKSUM.SHA256.$RELEASE"
        rmdir "$CACHE" 2>/dev/null
        info "removed the cached $RELEASE image"
        ;;
    help) usage ;;
    *) die "unknown command: $COMMAND (try --help)" ;;
esac
