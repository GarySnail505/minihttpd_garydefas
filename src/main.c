#include "server.h"

#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_EVENTOS 16

static int socket_no_bloqueante(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc > 2) {
        fprintf(stderr, "Uso: %s [puerto]\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    const char *puerto = argc == 2 ? argv[1] : "8080";
    int status = EXIT_SUCCESS; 

    int fd = passiveTCP(puerto, 8);
    if (fd == -1)
        return EXIT_FAILURE;

    // hacer socket fd como no bloqueante
    if (socket_no_bloqueante(fd) == -1) {
        close(fd);
        return EXIT_FAILURE;
    }

    // Crear un epoll para manejar eventos
    int epoll_fd;
    struct epoll_event evento; // sockets a vigilar 
    struct epoll_event eventos[MAX_EVENTOS]; // qué descriptores están listos
    
    epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        close(fd);
        return EXIT_FAILURE;
    }
    
    evento.events = EPOLLIN;
    evento.data.fd = fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &evento) == -1) {
        perror("epoll_ctl");
        close(fd);
        close(epoll_fd);
        return EXIT_FAILURE;
    }

    while(1){
        int n = epoll_wait(epoll_fd, eventos, MAX_EVENTOS, -1);
    
        if (n == -1) {
            if (errno == EINTR) // Interrupción por señal, continuar esperando
                continue;
            perror("epoll_wait");
            status = EXIT_FAILURE;
            break;
        }
    
        for (int i = 0; i < n; i++) {
            int actual_fd = eventos[i].data.fd;

            if (actual_fd == fd) { // socket escucha
                if (!(eventos[i].events & EPOLLIN)) {
                    fprintf(stderr, "Evento inesperado en socket de escucha\n");
                    status = EXIT_FAILURE;
                    break;
                }

                for (;;) {
                    // Procesar datos del socket de escucha
                    int sfd = accept(fd, NULL, NULL);

                    if (sfd >= 0) {
                        printf("Cliente aceptado (descriptor %d).\n", sfd);

                        if (socket_no_bloqueante(sfd) == -1) {
                            close(sfd);
                            continue; // si falla un cliente, no tiene porqué parar el servidor
                        }

                        // Agregar el socket del cliente al epoll
                        struct epoll_event evento_cliente = {0};
                        evento_cliente.events = EPOLLIN;
                        evento_cliente.data.fd = sfd;

                        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD,
                                      sfd, &evento_cliente) == -1) {
                            perror("epoll_ctl add cliente");
                            close(sfd);
                            continue;
                        }
                        continue;
                    }

                    if (errno == EINTR)
                        continue;

                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break;

                    perror("accept");
                    break;
                }
            } else {
                // Evento perteneciente a un socket cliente
                int sfd = actual_fd;

                if (eventos[i].events & EPOLLIN) {
                    char buffer[1024];
                    ssize_t bytes_leidos;

                    // Reintentar si una señal interrumpe read()
                    do {
                        bytes_leidos = read(sfd, buffer, sizeof(buffer) - 1);
                    } while (bytes_leidos == -1 && errno == EINTR);

                    if (bytes_leidos > 0) {
                        buffer[bytes_leidos] = '\0';
                        printf("Datos recibidos del cliente %d: %s\n",
                               sfd, buffer);
                    } else if (bytes_leidos == 0) {
                        printf("Cliente desconectado: %d\n", sfd);
                        close(sfd);
                    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        if (eventos[i].events & (EPOLLHUP | EPOLLERR))
                            close(sfd);
                    } else {
                        perror("read");
                        close(sfd);
                    }
                } else if (eventos[i].events & (EPOLLERR | EPOLLHUP)) {
                    // Error o cierre notificado sin EPOLLIN
                    close(sfd);
                }
            }
        }

        if (status == EXIT_FAILURE)
            break;
    }
    
    close(fd);
    close(epoll_fd);
    return status;
}
