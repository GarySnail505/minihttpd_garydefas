#include "server.h"
#include "http.h"

#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h> //memove()
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

static void cerrar_cliente(int epoll_fd, struct conexion *cliente)
{
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL,
                  cliente->fd, NULL) == -1) {
        perror("epoll_ctl DEL");
    }

    close(cliente->fd);
    free(cliente);
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
    //evento.data.fd = fd; // ya no es necesario, se puede usar data.ptr para almacenar un puntero a la estructura de conexión
    evento.data.ptr = NULL;

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
            struct conexion *actual = eventos[i].data.ptr;

            if (actual == NULL) { // socket escucha
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

                        struct conexion *cliente = malloc(sizeof(struct conexion));
                        if (!cliente) {
                            perror("malloc");
                            close(sfd);
                            continue; // si falla un cliente, no tiene porqué parar el servidor
                        }

                        // INICIALIZANDO LA ESTRUCTURA DE CONEXIÓN PERSISTENTE CON EL SOCKET DEL CLIENTE
                        cliente->fd = sfd;
                        cliente->used = 0;
                        cliente->processed  = 0;
                        cliente->estado = READING_HEADERS;
                        cliente->salida_len = 0;
                        cliente->salida_enviados = 0;
                        cliente->keep_alive = 1;
                        
                        if (socket_no_bloqueante(sfd) == -1) {
                            close(sfd);
                            free(cliente);
                            continue; // si falla un cliente, no tiene porqué parar el servidor
                        }

                        // Agregar el socket del cliente al epoll
                        struct epoll_event evento_cliente = {0};
                        evento_cliente.events = EPOLLIN;
                        evento_cliente.data.ptr = cliente;

                        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD,
                                      sfd, &evento_cliente) == -1) {
                            perror("epoll_ctl add cliente");
                            close(sfd);
                            free(cliente);
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
                int sfd = actual->fd;

                if (eventos[i].events & EPOLLIN) { // Leer datos del cliente
                    size_t disponibles = sizeof(actual->buffer_recepcion) - actual->used;

                    if (disponibles == 0) {
                        fprintf(stderr, "Buffer de recepción lleno\n");
                        cerrar_cliente(epoll_fd, actual);
                        continue;
                    }

                    ssize_t bytes_leidos;

                    do {
                        bytes_leidos = read(
                            sfd,
                            actual->buffer_recepcion + actual->used,
                            disponibles
                        );
                    } while (bytes_leidos == -1 && errno == EINTR);
                                         
                    // Manejar el resultado de la lectura usando headers_end
                    if (bytes_leidos > 0) {
                        actual->used += (size_t)bytes_leidos;
                    
                        printf("Cliente %d: %zu bytes acumulados\n",
                               sfd, actual->used);
         
                    if (actual->estado == READING_HEADERS) {
                    
                        while (actual->estado == READING_HEADERS) {
                            if (actual->used == 0)
                                break;
                        
                            size_t fin = headers_end(
                                actual->buffer_recepcion,
                                actual->used
                            );
                        
                            if (fin == 0) {
                                printf("Cliente %d: encabezados incompletos, used=%zu\n",
                                       sfd, actual->used);
                                break;
                            }
                        
                            actual->processed = fin;
                            actual->estado = PROCESSING_REQUEST;
                        
                            printf("Cliente %d: solicitud delimitada, fin=%zu\n",
                                   sfd, fin);
                            
                            char uri[256];
                            char host[128];
                            char user_agent[256];
                            int keep_alive = 1;

                            // Analizar la primera línea: GET, URI y HTTP/1.1
                            int codigo = analizar_linea_solicitud(
                                actual->buffer_recepcion,
                                fin,
                                uri,
                                sizeof(uri)
                            );

                            // Si la primera línea es válida, analizar encabezados
                            if (codigo == 0) {
                                codigo = analizar_encabezados(
                                    actual->buffer_recepcion,
                                    fin,
                                    host,
                                    sizeof(host),
                                    user_agent,
                                    sizeof(user_agent),
                                    &keep_alive
                                );
                            }

                            // Mostrar los resultados del procesamiento
                            if (codigo == 0) {
                                printf("Cliente %d: GET válido\n", sfd);
                                printf("URI: %s\n", uri);
                                printf("Host: %s\n", host);
                                printf("User-Agent: %s\n", user_agent);
                                printf("Keep-alive: %d\n", keep_alive);

                                int preparado = construir_respuesta_fija(
                                    actual->buffer_salida,
                                    sizeof(actual->buffer_salida),
                                    &actual->salida_len
                                );
                                
                                if (preparado == 0) {
                                    actual->salida_enviados = 0;
                                    actual->keep_alive = keep_alive;
                                
                                    printf("Cliente %d: respuesta preparada, %zu bytes\n",
                                           sfd, actual->salida_len);
                                } else {
                                    fprintf(stderr,
                                            "Cliente %d: error al preparar respuesta\n",
                                            sfd);
                                }
                            } else {
                                printf("Cliente %d: error HTTP %d\n", sfd, codigo);
                            }
                            
                            size_t pendientes = actual->used - actual->processed;
                            
                            memmove(
                                actual->buffer_recepcion,
                                actual->buffer_recepcion + actual->processed,
                                pendientes
                            );
                        
                            actual->used = pendientes;
                            actual->processed = 0;
                            actual->estado = READING_HEADERS;
                        
                            printf("Cliente %d: bytes pendientes=%zu\n",
                                   sfd, actual->used);
                        }
                    }

                    } else if (bytes_leidos == 0) {
                        printf("Cliente desconectado: %d\n", sfd);
                        cerrar_cliente(epoll_fd, actual);

                    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        if (eventos[i].events & (EPOLLHUP | EPOLLERR))
                            cerrar_cliente(epoll_fd, actual);

                    } else {
                        perror("read");
                        cerrar_cliente(epoll_fd, actual);

                    }
                } else if (eventos[i].events & (EPOLLERR | EPOLLHUP)) {
                    // Error o cierre notificado sin EPOLLIN
                    cerrar_cliente(epoll_fd, actual);
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
