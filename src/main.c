#define _POSIX_C_SOURCE 200809L
#include "server.h"
#include "http.h"
#include "files.h"
#include "mime.h"

#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
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

    if (cliente->archivo != NULL)
        fclose(cliente->archivo);
    close(cliente->fd);
    free(cliente);
}

static int cambiar_interes(int epoll_fd, struct conexion *cliente, uint32_t eventos)
{
    struct epoll_event evento = {0};
    evento.events = eventos;
    evento.data.ptr = cliente;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, cliente->fd, &evento) == -1) {
        perror("epoll_ctl MOD cliente");
        return -1;
    }
    return 0;
}

/* 0: faltan encabezados, 1: respuesta lista, -1: falló la preparación. */
static int preparar_siguiente(struct conexion *cliente)
{
    size_t fin = headers_end(cliente->buffer_recepcion, cliente->used);
    if (fin == 0 && cliente->used < sizeof(cliente->buffer_recepcion))
        return 0;

    cliente->estado = PROCESSING_REQUEST;
    cliente->processed = fin == 0 ? cliente->used : fin;

    const char *tipo_contenido = NULL;
    int codigo = 400;
    int keep_alive = 0;
    if (fin != 0) {
        char uri[256];
        char host[128];
        char user_agent[256];

        codigo = analizar_linea_solicitud(cliente->buffer_recepcion, fin,
                                         uri, sizeof(uri));
        if (codigo == 0) {
            codigo = analizar_encabezados(cliente->buffer_recepcion, fin,
                                          host, sizeof(host),
                                          user_agent, sizeof(user_agent),
                                          &keep_alive);
        }
        if (codigo == 0) {
            printf("Cliente %d: GET válido, URI=%s, Host=%s, User-Agent=%s\n",
                   cliente->fd, uri, host, user_agent);
            char ruta[PATH_MAX];
            codigo = resolver_ruta_www(uri, ruta);
            if (codigo == 0) {
                int resultado = abrir_archivo(ruta, &cliente->archivo,
                                              &cliente->archivo_restante);
                if (resultado == 1)
                    codigo = 404;
                else if (resultado != 0)
                    codigo = resultado;
                else
                    tipo_contenido = tipo_mime(ruta);
            }
        }
    }

    cliente->keep_alive = codigo == 0 && keep_alive;
    if (codigo == -1 ||
        construir_cabecera(cliente->buffer_salida, sizeof(cliente->buffer_salida),
                           &cliente->salida_len, codigo == 0 ? 200 : codigo,
                           cliente->keep_alive, cliente->archivo_restante,
                           tipo_contenido) == -1) {
        fprintf(stderr, "Cliente %d: error interno al preparar respuesta\n", cliente->fd);
        if (cliente->archivo != NULL) {
            fclose(cliente->archivo);
            cliente->archivo = NULL;
        }
        cliente->archivo_restante = 0;
        cliente->keep_alive = 0;
        if (construir_cabecera(cliente->buffer_salida, sizeof(cliente->buffer_salida),
                               &cliente->salida_len, 500, 0, 0, NULL) == -1)
            return -1;
    }

    size_t pendientes = cliente->used - cliente->processed;
    memmove(cliente->buffer_recepcion,
            cliente->buffer_recepcion + cliente->processed, pendientes);
    cliente->used = pendientes;
    cliente->processed = 0;

    cliente->salida_enviados = 0;
    cliente->estado = WRITING_RESPONSE;
    return 1;
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
                        cliente->archivo = NULL;
                        cliente->archivo_restante = 0;
                        
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
                continue;
            }

            uint32_t eventos_cliente = eventos[i].events;
            if (eventos_cliente & (EPOLLERR | EPOLLHUP)) {
                cerrar_cliente(epoll_fd, actual);
                continue;
            }

            if (actual->estado == WRITING_RESPONSE && (eventos_cliente & EPOLLOUT)) {
                int resultado = enviar_pendiente(actual);
                if (resultado == 0)
                    continue;

                if (resultado == -1) {
                    cerrar_cliente(epoll_fd, actual);
                    continue;
                }

                if (actual->archivo_restante > 0) {
                    size_t bloque = sizeof(actual->buffer_salida);
                    if (bloque > actual->archivo_restante)
                        bloque = actual->archivo_restante;

                    size_t leidos = fread(actual->buffer_salida, 1, bloque,
                                         actual->archivo);
                    if (leidos != bloque) {
                        if (ferror(actual->archivo))
                            perror("fread");
                        else
                            fprintf(stderr, "Cliente %d: fin de archivo inesperado\n", actual->fd);
                        cerrar_cliente(epoll_fd, actual);
                        continue;
                    }

                    actual->archivo_restante -= leidos;
                    actual->salida_len = leidos;
                    actual->salida_enviados = 0;
                    continue;
                }

                if (!actual->keep_alive) {
                    cerrar_cliente(epoll_fd, actual);
                    continue;
                }

                if (actual->archivo != NULL) {
                    fclose(actual->archivo);
                    actual->archivo = NULL;
                }

                actual->estado = READING_HEADERS;
                actual->salida_len = 0;
                actual->salida_enviados = 0;

                // Una solicitud completa en el búfer no produce otro EPOLLIN.
                int preparado = preparar_siguiente(actual);
                if (preparado == -1 ||
                    (preparado == 0 && cambiar_interes(epoll_fd, actual, EPOLLIN) == -1)) {
                    cerrar_cliente(epoll_fd, actual);
                }
                continue;
            }

            if (actual->estado != READING_HEADERS || !(eventos_cliente & EPOLLIN))
                continue;

            size_t disponibles = sizeof(actual->buffer_recepcion) - actual->used;
            ssize_t bytes_leidos;
            do {
                bytes_leidos = read(actual->fd,
                                    actual->buffer_recepcion + actual->used, disponibles);
            } while (bytes_leidos == -1 && errno == EINTR);

            if (bytes_leidos == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
                continue;

            if (bytes_leidos <= 0) {
                if (bytes_leidos == -1)
                    perror("read");
                cerrar_cliente(epoll_fd, actual);
                continue;
            }

            actual->used += (size_t)bytes_leidos;
            int preparado = preparar_siguiente(actual);
            if (preparado == -1 ||
                (preparado == 1 && cambiar_interes(epoll_fd, actual, EPOLLOUT) == -1)) {
                cerrar_cliente(epoll_fd, actual);
            }
        }
        if (status == EXIT_FAILURE)
            break;
    }
    
    close(fd);
    close(epoll_fd);
    return status;
}
