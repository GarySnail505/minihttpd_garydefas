#include "server.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc > 2) {
        fprintf(stderr, "Uso: %s [puerto]\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *puerto = argc == 2 ? argv[1] : "8080";

    int fd = passiveTCP(puerto, 8);
    if (fd == -1)
        return EXIT_FAILURE;

    puts("Escuchando en 127.0.0.1. Esperando un cliente TCP...");
    fflush(stdout);

    int sfd; // fd cliente

    do {
        sfd = accept(fd, NULL, NULL);
    } while (sfd == -1 && errno == EINTR);

    if (sfd == -1) {
        perror("accept");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("Cliente aceptado (descriptor %d).\n", sfd);

    close(sfd);
    close(fd);
    return EXIT_SUCCESS;
}
