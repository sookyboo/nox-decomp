/* Regression for 9d08e53: public-game responses must parse into safe server
 * rows and apply the configured bad-server filters. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../src/netextras_types.h"

size_t nox_parse_games_list_json(const char *json, nox_server_row *out, size_t cap);
int nox_is_bad_server_ip(const char *ip);
int nox_is_bad_server_name(const char *name);

int main(void)
{
    const char json[] =
        "{\"data\":["
        "{\"name\":\"Visible Game\",\"addr\":\"192.0.2.10\","
        "\"port\":19000,\"map\":\"Wizard03\",\"mode\":\"coop\","
        "\"players\":{\"cur\":2,\"max\":8}},"
        "{\"name\":\"Malformed trailing entry\",\"addr\":null},"
        "{\"name\":\"Second Game\",\"addr\":\"198.51.100.4\"}]}";
    nox_server_row rows[2];

    memset(rows, 0, sizeof(rows));
    if (nox_parse_games_list_json(json, rows, 2) != 2)
        return 1;
    if (strcmp(rows[0].name, "Visible Game") != 0 ||
        strcmp(rows[0].addr, "192.0.2.10") != 0 ||
        rows[0].port != 19000 || strcmp(rows[0].map, "Wizard03") != 0 ||
        strcmp(rows[0].mode, "coop") != 0 || rows[0].players_cur != 2 ||
        rows[0].players_max != 8)
        return 2;
    if (strcmp(rows[1].name, "Malformed trailing entry") != 0 ||
        rows[1].port != 18590 || rows[1].players_max != 31)
        return 3;

    if (setenv("NOX_BAD_SERVER_IPS", "192.0.2.10", 1) != 0 ||
        setenv("NOX_BAD_SERVER_NAMES", "second game", 1) != 0)
        return 4;
    if (!nox_is_bad_server_ip("192.0.2.10") ||
        nox_is_bad_server_ip("192.0.2.11") ||
        !nox_is_bad_server_name("SECOND GAME") ||
        nox_is_bad_server_name("Visible Game"))
        return 5;
    return 0;
}
