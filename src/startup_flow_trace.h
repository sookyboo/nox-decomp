#ifndef NOX_STARTUP_FLOW_TRACE_H
#define NOX_STARTUP_FLOW_TRACE_H

#include <stdio.h>

/* Build with -DNOX_TRACE_STARTUP_FLOW=ON to include startup and map-flow
 * diagnostics. Normal builds compile these call sites out completely. */
#ifdef NOX_TRACE_STARTUP_FLOW
#define NOX_FLOW_TRACE(...)                                                     \
    do {                                                                        \
        fprintf(stderr, "[flow] %s:%d: ", __func__, __LINE__);                  \
        fprintf(stderr, __VA_ARGS__);                                           \
        fputc('\n', stderr);                                                    \
        fflush(stderr);                                                         \
    } while (0)
#else
#define NOX_FLOW_TRACE(...) ((void)0)
#endif

#endif
