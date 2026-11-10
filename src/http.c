#include "http.h"

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
) {
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
    strcpy(buffer, respuesta);
    *longitud = len;
    return 0;
}
