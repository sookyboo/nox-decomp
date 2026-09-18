/* Regression for ba20703: map-transfer sequence bytes are unsigned. */

#include "../src/proto.h"

#include <arpa/inet.h>

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
        callback_payload = *(unsigned char *)payload;
    ++callback_count;
}

int main(void)
{
    unsigned char connection[192] = {0};
    unsigned char transfer[8] = {0};
    unsigned char receive_buffer[256] = {0};
    unsigned char packet[5] = {0x80, 0x80, 0x20, 0x00, 0x20};
    struct sockaddr_in address = {0};
    struct sockaddr_in sender = {0};
    socklen_t sender_length = sizeof(sender);
    socklen_t receiver_length = sizeof(address);
    int sender_socket;
    int receiver_socket;
    int sent;

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
    *(unsigned char **)(connection + 32) = receive_buffer;
    *(unsigned char **)(connection + 36) = receive_buffer;
    *(unsigned char **)(connection + 40) = receive_buffer;
    *(unsigned char **)(connection + 44) = receive_buffer + sizeof(receive_buffer);
    *(unsigned char **)(connection + 48) = transfer;
    *(void (**)(unsigned int, int, int, unsigned int))(connection + 144) =
        receive_callback;
    transfer[0] = 1;
    transfer[1] = 0x80;
    *(int *)(connection + 152) = 1;
    connection[156] = 0;

    /* Make the source address match the connection table lookup. */
    *(unsigned short *)(connection + 6) = sender.sin_port;
    *(unsigned int *)(connection + 8) = sender.sin_addr.s_addr;
    *(unsigned int *)&byte_5D4594[3843788] = (unsigned int)connection;

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
