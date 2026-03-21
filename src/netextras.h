#pragma once

#include <stddef.h> /* size_t */

#ifdef _WIN32
  /* IMPORTANT: winsock2.h must come before windows.h in any TU */
  #include <winsock2.h>
  #include <ws2tcpip.h>

  /* socklen_t isn't always defined depending on toolchain headers */
  #ifndef _SSIZE_T_DEFINED
    /* not strictly needed here */
  #endif

#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

void nox_netextras_on_discovery_ping_send(int sockfd,
                                         const void *buf,
                                         size_t len,
                                         unsigned dest_port);

int nox_netextras_try_fake_recvfrom(int sockfd,
                                   void *buffer,
                                   size_t length,
                                   struct sockaddr *addr,
                                   socklen_t *addrlen);

void nox_netextras_on_host_bind_success(int sockfd, unsigned bound_port);

int nox_netextras_fake_pending(int sockfd);

void nox_netextras_on_host_game_stop(void);


#ifdef __cplusplus
}
#endif
