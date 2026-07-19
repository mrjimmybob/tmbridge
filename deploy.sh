#!/usr/bin/env bash
###############################################################################
# tmbridge — one-shot deploy for Debian/Ubuntu/Linux Mint
#
# Installs build dependencies, compiles the release binary, applies any config
# overrides, installs the systemd service (via install.sh), optionally creates
# a raw CUPS queue, and runs a live print test.
#
# Usage:
#   ./deploy.sh [options]
#
# Options:
#   --printer-host IP    Printer address        (edits tmbridge.conf)
#   --printer-port N     Printer port           (edits tmbridge.conf)
#   --listen-host IP     Bridge listen address  (edits tmbridge.conf)
#   --listen-port N      Bridge listen port     (edits tmbridge.conf)
#   --force-config       Overwrite /etc/tmbridge/tmbridge.conf if it exists
#   --cups[=NAME]        Create a raw CUPS queue (default name: Epson_TM_T20IV)
#   --no-test            Skip the post-install print test
#   -h, --help           Show this help and exit
#
# Run as a normal user with sudo available; the privileged steps are sudo'd.
# Re-runnable: on an existing install, pass --force-config to push config
# changes, otherwise the on-disk /etc/tmbridge/tmbridge.conf is left as-is.
###############################################################################
set -euo pipefail

cd "$(dirname "$0")"

CONF="tmbridge.conf"
CUPS_NAME=""
DO_CUPS=0
DO_TEST=1
FORCE_CONFIG=0

if [ "$(id -u)" -eq 0 ]; then
    SUDO=""
else
    SUDO="sudo"
fi

log()  { printf '\033[1;32m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m warn:\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[1;31merror:\033[0m %s\n' "$*" >&2; exit 1; }

usage() { sed -n '2,/^####/p' "$0" | sed 's/^# \{0,1\}//; s/^#//'; exit 0; }

# --- set a key=value in tmbridge.conf (replace existing line or append) -------
set_conf() {
    local key="$1" val="$2"
    if grep -qE "^${key}=" "$CONF"; then
        sed -i "s|^${key}=.*|${key}=${val}|" "$CONF"
    else
        printf '%s=%s\n' "$key" "$val" >> "$CONF"
    fi
    log "config: ${key}=${val}"
}

conf_value() { sed -n "s/^$1=//p" "$CONF" | head -1; }

# --- parse arguments ----------------------------------------------------------
while [ $# -gt 0 ]; do
    case "$1" in
        --printer-host) set_conf printer_host "$2"; shift 2 ;;
        --printer-port) set_conf printer_port "$2"; shift 2 ;;
        --listen-host)  set_conf listen_host  "$2"; shift 2 ;;
        --listen-port)  set_conf listen_port  "$2"; shift 2 ;;
        --force-config) FORCE_CONFIG=1; shift ;;
        --cups)         DO_CUPS=1; CUPS_NAME="Epson_TM_T20IV"; shift ;;
        --cups=*)       DO_CUPS=1; CUPS_NAME="${1#--cups=}"; shift ;;
        --no-test)      DO_TEST=0; shift ;;
        -h|--help)      usage ;;
        *)              die "unknown option: $1 (try --help)" ;;
    esac
done

[ -f Makefile ] && [ -d src ] || die "run this from the tmbridge repo root"

# --- 1. build dependencies ----------------------------------------------------
if command -v apt-get >/dev/null 2>&1; then
    log "installing build dependencies (apt)"
    $SUDO apt-get update -qq
    $SUDO apt-get install -y build-essential libcurl4-openssl-dev pkg-config netcat-openbsd
else
    warn "apt-get not found — skipping dependency install; ensure a C toolchain,"
    warn "libcurl development headers and pkg-config are present."
fi

# --- 2. build -----------------------------------------------------------------
log "building release binary"
make BUILD=release

# --- 3. install service (install.sh needs root) -------------------------------
log "installing service"
$SUDO ./install.sh

# install.sh never overwrites an existing /etc/tmbridge/tmbridge.conf; push our
# copy explicitly when the caller asked for it.
if [ "$FORCE_CONFIG" -eq 1 ]; then
    log "forcing /etc/tmbridge/tmbridge.conf"
    $SUDO install -m 644 "$CONF" /etc/tmbridge/tmbridge.conf
    $SUDO systemctl restart tmbridge
elif ! $SUDO cmp -s "$CONF" /etc/tmbridge/tmbridge.conf 2>/dev/null; then
    warn "/etc/tmbridge/tmbridge.conf differs from this repo's tmbridge.conf;"
    warn "re-run with --force-config to apply your changes."
fi

$SUDO systemctl --no-pager --full status tmbridge || true

# --- 4. optional CUPS raw queue ----------------------------------------------
if [ "$DO_CUPS" -eq 1 ]; then
    if command -v lpadmin >/dev/null 2>&1; then
        chost="$(conf_value listen_host)"; cport="$(conf_value listen_port)"
        [ "$chost" = "0.0.0.0" ] && chost="127.0.0.1"
        log "creating CUPS raw queue '$CUPS_NAME' -> socket://$chost:$cport"
        $SUDO lpadmin -p "$CUPS_NAME" -E -v "socket://$chost:$cport" -m raw
        $SUDO cupsenable "$CUPS_NAME" || true
        $SUDO cupsaccept "$CUPS_NAME" || true
    else
        warn "lpadmin not found (CUPS not installed) — skipping queue creation."
    fi
fi

# --- 5. live print test -------------------------------------------------------
if [ "$DO_TEST" -eq 1 ]; then
    thost="$(conf_value listen_host)"; tport="$(conf_value listen_port)"
    [ "$thost" = "0.0.0.0" ] && thost="127.0.0.1"
    log "sending test receipt to ${thost}:${tport}"
    # ESC @ | text | 3x LF | GS V A 16 (feed + cut)
    payload=$'\033@tmbridge deploy test\n\n\n\035VA\020'
    if command -v nc >/dev/null 2>&1; then
        printf '%s' "$payload" | nc -w3 "$thost" "$tport" || warn "nc send failed"
    else
        exec 3<>"/dev/tcp/${thost}/${tport}" || die "cannot connect to ${thost}:${tport}"
        printf '%s' "$payload" >&3
        sleep 1
        exec 3>&- 3<&-
    fi
    log "test sent — check the printer and: journalctl -u tmbridge -n 20"
fi

log "done."
