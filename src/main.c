#include "server.h"

#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_EVENTOS 16

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
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        close(fd);
        return EXIT_FAILURE;
    }   
    
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
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
    
        // if (n == 0) {
        //     fprintf(stderr, "epoll_wait timeout\n");
        //     close(fd);
        //     close(epoll_fd);
        //     return EXIT_FAILURE;
        // }
        // Procesar los eventos
        for (int i = 0; i < n; i++) {
            if (eventos[i].data.fd == fd &&
                (eventos[i].events & EPOLLIN)) {
                    for(;;){
                        // Procesar datos del cliente
                        int sfd = accept(fd, NULL, NULL);
                        
                        if(sfd >=0){
                            printf("Cliente aceptado (descriptor %d).\n", sfd);
                            close(sfd);
                            continue;
                        }
            
                        if (errno == EAGAIN || errno == EWOULDBLOCK) 
                            break; 
            
                        if (errno == EINTR) 
                            continue; 
            
                        perror("accept");
                        break;
                    }
                }
        }
    }
    
    close(fd);
    close(epoll_fd);
    return status;
}
