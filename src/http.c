#include "http.h"

#include <string.h>
#include <stdio.h>

size_t headers_end(const char *buffer, size_t used)
{
    if(used<4) {
        return 0; // No hay suficientes bytes para contener CRLF CRLF
    }

    for (size_t i = 0; i <= used-4; i++) {
        if (buffer[i] == '\r' &&
            buffer[i+1] == '\n' &&
            buffer[i+2] == '\r' &&
            buffer[i+3] == '\n') {
            return i + 4; // Retorna la posición después de los encabezados
        }
    }
    return 0; // No se encontraron los encabezados completos
}

int construir_respuesta_fija(
    char *buffer,
    size_t capacidad,
    size_t *longitud
){
    if (buffer == NULL || longitud == NULL) {
        return -1;
    }

    *longitud = 0; 
    const char *respuesta = "HTTP/1.1 200 OK\r\n"
                "Date: Mon, 09 Nov 2026 20:00:00 GMT\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: 11\r\n"
                "\r\n"
                "<p>Hola</p>";  
    size_t len = strlen(respuesta);
    if (len >= capacidad) {
        return -1; // No hay suficiente espacio
    }
    memcpy(buffer, respuesta, len + 1);
    *longitud = len;
    
    return 0;
}

int analizar_linea_solicitud( // analiza: "GET /index.html HTTP/1.1\r\n"
    const char *buffer,
    size_t longitud,
    char *uri,
    size_t capacidad_uri
) {
    if (buffer == NULL || uri == NULL) {
        return -1;
    }

    // 1. Buscar el final de la primera linea
    size_t fin = 0;

    while (fin < longitud &&
           buffer[fin] != '\r' &&
           buffer[fin] != '\n') {
        fin++;
    }

    if (fin == longitud ||
        buffer[fin] != '\r' ||
        fin + 1 >= longitud ||
        buffer[fin + 1] != '\n') {
        return 400;
    }

    const char *fin_linea = buffer + fin;

    //Primer espacio GET /index.html
    const char *espacio1 = memchr(buffer, ' ', fin);

    if (espacio1 == NULL || espacio1 == buffer) {
        return 400;
    }
    const char *inicio_uri = espacio1 + 1;

    // Segundo espacio /index.html HTTP/1.1\r\n
    const char *espacio2 = memchr(
        inicio_uri,
        ' ',
        fin_linea - inicio_uri
    );

    if (espacio2 == NULL || espacio2 == inicio_uri) {
        return 400;
    }

    // Comprobar la version HTTP
    const char *version = espacio2 + 1;

    if ((size_t)(fin_linea - version) != 8 ||
        memcmp(version, "HTTP/1.1", 8) != 0) {
        return 400;
    }

    // 5. Comprobar el metodo
    size_t longitud_metodo = espacio1 - buffer;

    if (longitud_metodo != 3 ||
        memcmp(buffer, "GET", 3) != 0) {
        return 405;
    }

    // 6. Copiar la URI
    size_t longitud_uri = espacio2 - inicio_uri;

    if (longitud_uri >= capacidad_uri) {
        return 400;
    }

    memcpy(uri, inicio_uri, longitud_uri);
    uri[longitud_uri] = '\0';

    return 0;
}
