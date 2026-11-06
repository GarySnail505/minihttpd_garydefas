/* passivesock.c - passivesock moderno con getaddrinfo() */
#define _POSIX_C_SOURCE 200809L
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>


int passivesock(const char *service, const char *transport, int qlen) {

    struct addrinfo hints, *res, *p;
    int sockfd = -1, socktype, yes = 1;

    if (strcmp(transport, "udp") == 0)
        socktype = SOCK_DGRAM;
    else
        socktype = SOCK_STREAM;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;    /* Soporta IPv4 e IPv6 */
    hints.ai_socktype = socktype;
    hints.ai_flags = 0;    /* Escuchar en INADDR_ANY / cualquier interfaz */

    int err = getaddrinfo("127.0.0.1", service, &hints, &res);
    if (err != 0) {
        fprintf(stderr, "getaddrinfo error (%s): %s\n", service, gai_strerror(err));
        return -1;
    }

    /* Iterar sobre las direcciones posibles */
    for (p = res; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd < 0)
            continue;

        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == 0)
            break;

        close(sockfd);
        sockfd = -1;
    }

    freeaddrinfo(res);

    if (sockfd < 0) {
        fprintf(stderr, "No se pudo hacer bind en el puerto/servicio %s: %s\n",
                service, strerror(errno));
        return -1;
    }

    if (socktype == SOCK_STREAM && listen(sockfd, qlen) < 0) {
        int error = errno;
        close(sockfd);
        fprintf(stderr, "No se pudo hacer listen en %s: %s\n", service, strerror(error));
        return -1;
    }

    return sockfd; /* UDP: listo para recibir tramas | TCP: listo para accept() */
}

int passiveTCP(const char *service, int qlen) {
    return passivesock(service, "tcp", qlen);
}

