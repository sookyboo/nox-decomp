#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 2 ]; then
    printf 'usage: %s SECONDS [--guard-profile-slots] COMMAND [ARG...]\n' "$0" >&2
    exit 2
fi

duration=$1
shift
case "$duration" in
    ''|*[!0-9]*) printf 'SECONDS must be a positive integer\n' >&2; exit 2 ;;
esac
[ "$duration" -gt 0 ] || { printf 'SECONDS must be greater than zero\n' >&2; exit 2; }

guard_profile_slots=0
if [[ "${1:-}" == --guard-profile-slots ]]; then
    guard_profile_slots=1
    shift
fi
[ "$#" -gt 0 ] || { printf 'missing probe command\n' >&2; exit 2; }

if (( guard_profile_slots )); then
    if [[ ! -d Save ]]; then
        printf 'profile-slot guard failed: %s has no Save/ directory\n' "$PWD" >&2
        exit 2
    fi

    next_profile_index=-1
    for ((profile_index = 0; profile_index < 100; ++profile_index)); do
        profile_path=$(printf 'Save/N%02d.plr' "$profile_index")
        if [[ ! -e "$profile_path" ]]; then
            next_profile_index=$profile_index
            break
        fi
    done

    if (( next_profile_index < 0 )); then
        printf 'profile-slot guard failed: Save/N00.plr through Save/N99.plr are occupied\n' >&2
        exit 2
    fi
    if (( next_profile_index >= 99 )); then
        printf 'profile-slot guard failed: next character profile would be Save/N%02d.plr; refusing to use final slot N99.plr\n' "$next_profile_index" >&2
        exit 2
    fi
    printf 'profile-slot guard: next character profile slot is N%02d.plr\n' "$next_profile_index" >&2
fi

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
