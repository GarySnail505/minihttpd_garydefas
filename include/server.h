/* passiveTCP.c */
#ifndef SERVER_H
#define SERVER_H

#include <stddef.h>

struct conexion {
    int fd;
    char buffer_recepcion[4096];
    size_t used;
    size_t processed;
    enum estado_conexion {
        READING_HEADERS,
        PROCESSING_REQUEST,
        WRITING_RESPONSE
    } estado;
    char buffer_salida[1024]; // respuesta http
    size_t salida_len;
    size_t salida_enviados;
    int keep_alive;
};

int passivesock(const char *service, const char *transport, int qlen);
int passiveTCP(const char *service, int qlen);

int enviar_pendiente(struct conexion *cliente);

#endif /* SERVER_H */
