#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

/* Devuelve el limite tras CRLF CRLF, o 0 si los encabezados estan incompletos. */
size_t headers_end(const char *buffer, size_t used);

int construir_respuesta_fija(
    char *buffer,
    size_t capacidad,
    size_t *longitud
);

int analizar_linea_solicitud( // analiza: "GET /index.html HTTP/1.1\r\n"
    const char *buffer,
    size_t longitud,
    char *uri,
    size_t capacidad_uri
);

#endif /* HTTP_H */
