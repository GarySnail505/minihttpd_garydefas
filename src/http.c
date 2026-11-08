#include "http.h"

size_t headers_end(const char *buffer, size_t used)
{
    for (size_t i = 0; i < used && i + 4 <= used; i++) {
        if (buffer[i] == '\r' &&
            buffer[i+1] == '\n' &&
            buffer[i+2] == '\r' &&
            buffer[i+3] == '\n') {
            return i + 4; // Retorna la posición después de los encabezados
        }
    }
    return 0; // No se encontraron los encabezados completos
}
