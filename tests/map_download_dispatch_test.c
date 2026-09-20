/* Regression for ba20703: map-transfer sequence bytes are unsigned. */

#include "../src/proto.h"
#include "legacy_memory.h"

#include <arpa/inet.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>

#undef socket
#undef bind
#undef sendto
#undef close

static int callback_count;
static unsigned char callback_payload;

static void __cdecl receive_callback(unsigned int channel,
                                     int payload,
                                     int length,
                                     unsigned int user_data)
{
    (void)channel;
    (void)user_data;
    if (length == 1)
        callback_payload = *(unsigned char *)(uintptr_t)(uint32_t)payload;
    ++callback_count;
}

static int map_file_transfer_test(void)
{
    char original[PATH_MAX];
    char temporary[] = "/tmp/nox-map-download-XXXXXX";
    char transfer_name[] = "maps\\Fixture\\Fixture.nxz";
    unsigned char first[] = {'A', 'B'};
    unsigned char second[] = {'C', 'D'};
    unsigned char third[] = {'E', 'F'};
    unsigned char output[6];
    FILE *file;
    int success = 0;

    if (!getcwd(original, sizeof(original)) || !mkdtemp(temporary) ||
        chdir(temporary) != 0)
        return 0;
    if (!sub_4ABAD0(transfer_name, sizeof(output)))
        goto cleanup;

    /* Keep completion/UI callbacks out of this focused file fixture. */
    *(uint32_t *)&byte_5D4594[1309764] = 0;
    sub_4AB7C0(3, third, sizeof(third));
    sub_4AB7C0(1, first, sizeof(first));
    sub_4AB7C0(2, second, sizeof(second));
    sub_4AB580();

    file = fopen("maps/Fixture/Fixture.nxz", "rb");
    if (!file || fread(output, 1, sizeof(output), file) != sizeof(output))
        goto cleanup;
    fclose(file);
    success = memcmp(output, "ABCDEF", sizeof(output)) == 0;
    remove("maps/Fixture/Fixture.nxz");

cleanup:
    if (!success)
        sub_4AB720();
    rmdir("maps/Fixture");
    rmdir("maps");
    chdir(original);
    rmdir(temporary);
    return success;
}

int main(void)
{
    size_t network_record_args[10] = {0};
    void *network_record;
    unsigned char *connection = nox_test_legacy_alloc(192);
    unsigned char *transfer = nox_test_legacy_alloc(8);
    unsigned char *receive_buffer = nox_test_legacy_alloc(256);
    unsigned char packet[5] = {0x80, 0x80, 0x20, 0x00, 0x20};
    struct sockaddr_in address = {0};
    struct sockaddr_in sender = {0};
    socklen_t sender_length = sizeof(sender);
    socklen_t receiver_length = sizeof(address);
    int sender_socket;
    int receiver_socket;
    int sent;

    if (!map_file_transfer_test())
        return 1;

    /* A real transfer creates this production connection record before the
     * packet dispatcher runs.  Its recovered pointer slots must remain valid
     * on native builds, including through the normal destructor. */
    network_record_args[3] = 16;
    network_record_args[5] = 256;
    network_record = sub_553000(network_record_args);
    if (!network_record)
        return 1;
    if ((uintptr_t)network_record > UINT32_MAX)
    {
        sub_5531C0(network_record);
        return 1;
    }
    sub_5531C0(network_record);

    if (!connection || !transfer || !receive_buffer)
        return 1;

    receiver_socket = socket(AF_INET, SOCK_DGRAM, 0);
    sender_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (receiver_socket < 0 || sender_socket < 0)
        return 1;

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(0);
    if (bind(receiver_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
        return 1;
    if (getsockname(receiver_socket, (struct sockaddr *)&address,
                    &receiver_length) < 0)
        return 1;
    sender.sin_family = AF_INET;
    sender.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sender.sin_port = htons(0);
    if (bind(sender_socket, (struct sockaddr *)&sender, sizeof(sender)) < 0)
        return 1;
    if (getsockname(sender_socket, (struct sockaddr *)&sender,
                    &sender_length) < 0)
        return 1;

    connection[0] = (unsigned char)receiver_socket;
    *(int *)(connection + 20) = -1;
    *(uint32_t *)(connection + 32) = (uint32_t)(uintptr_t)receive_buffer;
    *(uint32_t *)(connection + 36) = (uint32_t)(uintptr_t)receive_buffer;
    *(uint32_t *)(connection + 40) = (uint32_t)(uintptr_t)receive_buffer;
    *(uint32_t *)(connection + 44) =
        (uint32_t)(uintptr_t)(receive_buffer + 256);
    *(uint32_t *)(connection + 48) = (uint32_t)(uintptr_t)transfer;
    *(uint32_t *)(connection + 144) = (uint32_t)(uintptr_t)receive_callback;
    transfer[0] = 1;
    transfer[1] = 0x80;
    *(int *)(connection + 152) = 1;
    connection[156] = 0;

    /* Make the source address match the connection table lookup. */
    *(unsigned short *)(connection + 6) = sender.sin_port;
    *(unsigned int *)(connection + 8) = sender.sin_addr.s_addr;
    *(unsigned int *)&byte_5D4594[3843788] = (uint32_t)(uintptr_t)connection;

    sent = sendto(sender_socket, packet, sizeof(packet), 0,
                  (struct sockaddr *)&address, sizeof(address));
    if (sent != (int)sizeof(packet))
        return 1;

    if (sub_552A80(0, 0) != (int)sizeof(packet))
        return 1;
    if (callback_count != 1 || callback_payload != 0x20)
        return 1;
    if (transfer[1] != 0x81)
        return 1;

    return 0;
}
