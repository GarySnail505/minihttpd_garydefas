#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

/* Devuelve el limite tras CRLF CRLF, o 0 si los encabezados estan incompletos. */
size_t headers_end(const char *buffer, size_t used);

/* longitud recibe los bytes de cabecera; longitud_cuerpo se anuncia al cliente. */
int construir_cabecera(
    char *buffer,
    size_t capacidad,
    size_t *longitud,
    int codigo,
    int keep_alive,
    size_t longitud_cuerpo,
    const char *tipo_contenido
);

int construir_respuesta(
    char *buffer,
    size_t capacidad,
    size_t *longitud,
    int codigo,
    int keep_alive,
    const char *cuerpo, 
    size_t longitud_cuerpo
);

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

int analizar_connection(
    const char *buffer,
    size_t longitud,
    int *keep_alive
);

int analizar_encabezados(
    const char *buffer,
    size_t longitud,
    char *host,
    size_t capacidad_host,
    char *user_agent,
    size_t capacidad_user_agent,
    int *keep_alive
);

#endif /* HTTP_H */
