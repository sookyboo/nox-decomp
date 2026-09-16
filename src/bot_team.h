#ifndef NOX_BOT_TEAM_H
#define NOX_BOT_TEAM_H

/*
 * Shared high-level CTF steering for native player bots.
 *
 * Flag ownership, pickup/drop/capture/scoring, carrier state, teams, pathing,
 * and guard/fight actions remain authoritative in Nox. These helpers only
 * reproduce Bot-Script's attack/defend destination choice.
 */
int nox_bot_team_ctf_attack_or_defend(int object);
void nox_bot_team_ctf_walk_to_own_flag(int object);

#endif
