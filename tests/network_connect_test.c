/* Loopback integration test: the production client join request must reach a
 * local UDP server with the expected protocol type and fixed packet size.
 * Server-side acceptance and the transition into a joined game require the
 * broader normal-target integration fixture and are intentionally outside
 * this focused client-send test. */
#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

unsigned char byte_5D4594[3844309];
unsigned char byte_587000[400000];

int compatsendto(int socket_fd, const void *buffer, size_t length, int flags,
                 const struct sockaddr *address, int address_length)
{
    return (int)sendto(socket_fd, buffer, length, flags, address, address_length);
}

int sub_5550A0(int address, unsigned short port, char *buffer);

int main(void)
{
    int server = -1;
    int client = -1;
    struct sockaddr_in server_address;
    socklen_t server_length = sizeof(server_address);
    unsigned char packet[128] = {0};
    char join_buffer[100];
    ssize_t received;

    server = socket(AF_INET, SOCK_DGRAM, 0);
    client = socket(AF_INET, SOCK_DGRAM, 0);
    if (server < 0 || client < 0)
        goto fail;

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server_address.sin_port = 0;
    if (bind(server, (struct sockaddr *)&server_address, sizeof(server_address)) != 0 ||
        getsockname(server, (struct sockaddr *)&server_address, &server_length) != 0)
        goto fail;

    /* sub_555010 reads these as the active client socket state. */
    *(uint32_t *)(byte_5D4594 + 2513916) = 1;
    *(int *)(byte_5D4594 + 2513920) = client;

    if (sub_5550A0((int)server_address.sin_addr.s_addr,
                   ntohs(server_address.sin_port), join_buffer) != 100)
        goto fail;

    received = recv(server, packet, sizeof(packet), 0);
    if (received != 100 || packet[0] != 0 || packet[1] != 0 || packet[2] != 14)
        goto fail;

    close(client);
    close(server);
    return 0;

fail:
    if (client >= 0)
        close(client);
    if (server >= 0)
        close(server);
    return 1;
}
