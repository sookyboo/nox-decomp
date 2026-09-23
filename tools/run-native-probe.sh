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

# Keep a postmortem core when the probed process crashes.  The hard limit on
# this host permits an unlimited soft limit; fail rather than silently running
# a diagnostic with core dumps disabled.
ulimit -c unlimited
if [[ "$(ulimit -c)" != unlimited ]]; then
    printf 'could not enable core dumps for native probe\n' >&2
    exit 2
fi

core_pattern=$(< /proc/sys/kernel/core_pattern)
if [[ "$core_pattern" == core && ( -e core || -L core ) ]]; then
    printf 'refusing probe: existing core file would be overwritten: %s/core\n' "$PWD" >&2
    exit 2
fi
printf 'native probe core limit: unlimited (kernel core_pattern: %s)\n' "$core_pattern" >&2

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
