#ifndef NOX_BOT_TRACE_H
#define NOX_BOT_TRACE_H

/*
 * Bounded lifecycle diagnostic for comparing normal network player joins,
 * summoned-object creation, server-created bot attempts, and native player-bot
 * attachment in one run. Disabled by default; enable with
 * NOX_BOT_LIFECYCLE_TRACE=1 or `bot trace on`.
 */
int nox_bot_trace_enabled(void);
void nox_bot_trace_set_enabled(int enabled);
const char *nox_bot_trace_join_path(void);
void nox_bot_trace_set_spawn_join(int enabled);
void nox_bot_tracef(const char *path, const char *phase, const char *fmt, ...);

#endif
