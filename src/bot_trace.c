#include "bot_trace.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int nox_bot_trace_override = -1;
static int nox_bot_trace_environment = -1;
static int nox_bot_trace_spawn_join;

static int nox_bot_trace_env_enabled(void)
{
    const char *value;

    if (nox_bot_trace_environment >= 0)
        return nox_bot_trace_environment;
    value = getenv("NOX_BOT_LIFECYCLE_TRACE");
    nox_bot_trace_environment = value && *value && strcmp(value, "0") != 0;
    return nox_bot_trace_environment;
}

int nox_bot_trace_enabled(void)
{
    if (nox_bot_trace_override >= 0)
        return nox_bot_trace_override;
    return nox_bot_trace_env_enabled();
}

void nox_bot_trace_set_enabled(int enabled)
{
    nox_bot_trace_override = enabled != 0;
}

const char *nox_bot_trace_join_path(void)
{
    return nox_bot_trace_spawn_join ? "spawn" : "network";
}

void nox_bot_trace_set_spawn_join(int enabled)
{
    nox_bot_trace_spawn_join = enabled != 0;
}

void nox_bot_tracef(const char *path, const char *phase, const char *fmt, ...)
{
    va_list args;

    if (!nox_bot_trace_enabled())
        return;
    fprintf(stderr, "[bot-lifecycle] path=%s phase=%s", path ? path : "?", phase ? phase : "?");
    if (fmt && *fmt) {
        fputc(' ', stderr);
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
    fputc('\n', stderr);
    fflush(stderr);
}
