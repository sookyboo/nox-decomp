#ifndef NOX_BOT_CONSOLE_H
#define NOX_BOT_CONSOLE_H

#include <wchar.h>

/*
 * Optional in-game console surface for native player bots.
 *
 * Returns non-zero only when argv[0] is the bot command and the command was
 * consumed. Unknown non-bot commands continue through Nox's original command
 * table unchanged.
 */
int nox_bot_console_command(int argc, const wchar_t *const *argv);

#endif
