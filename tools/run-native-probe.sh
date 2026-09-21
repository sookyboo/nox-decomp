#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 2 ]; then
    printf 'usage: %s SECONDS COMMAND [ARG...]\n' "$0" >&2
    exit 2
fi

duration=$1
shift
case "$duration" in
    ''|*[!0-9]*) printf 'SECONDS must be a positive integer\n' >&2; exit 2 ;;
esac
[ "$duration" -gt 0 ] || { printf 'SECONDS must be greater than zero\n' >&2; exit 2; }

setsid --wait "$@" &
probe_pid=$!
cleanup() {
    kill -TERM -- "-$probe_pid" 2>/dev/null || true
    sleep 1
    kill -KILL -- "-$probe_pid" 2>/dev/null || true
}
trap cleanup INT TERM

for ((tick = 0; tick < duration * 10; ++tick)); do
    if ! kill -0 "$probe_pid" 2>/dev/null; then
        wait "$probe_pid"
        status=$?
        trap - INT TERM
        exit "$status"
    fi
    sleep 0.1
done

cleanup
wait "$probe_pid" 2>/dev/null || true
printf 'probe timed out after %ss; process group terminated\n' "$duration" >&2
exit 124
