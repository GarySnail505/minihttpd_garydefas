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
};

int passivesock(const char *service, const char *transport, int qlen);
int passiveTCP(const char *service, int qlen);

#endif /* SERVER_H */
