#include "http.h"

#include <string.h>
#include <stdio.h>
#include <strings.h>

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

int analizar_connection(
    const char *buffer,
    size_t longitud,
    int *keep_alive
){
    if (buffer == NULL || keep_alive == NULL) {
        return -1;
    }

    *keep_alive = 1; // Valor por defecto (conexión persistente)

    const char *inicio = buffer;
    const char *fin = buffer + longitud;

    while (inicio < fin) {
        const char *linea_fin = memchr(inicio, '\n', fin - inicio);    
        if (linea_fin == NULL) {
            return 400;
        }
        // Comprobar que la línea termine en \r\n
        if (linea_fin == inicio || linea_fin[-1] != '\r') {
            return 400;
        }
        // Detectar la línea vacía que termina los encabezados
        if (linea_fin == inicio + 1) {
            return 0;
        }

        size_t linea_longitud = linea_fin - inicio + 1;
        
        if (linea_longitud >= 11 &&
            strncasecmp(inicio, "Connection:", 11) == 0) {
            
            const char *valor_inicio = inicio + 11;
            const char *valor_fin = linea_fin - 1;

            const char *cursor = valor_inicio;

            while (cursor < valor_fin) {
            
                // Buscar la siguiente coma
                const char *coma = memchr(
                    cursor,
                    ',',
                    valor_fin - cursor
                );
            
                // Determinar dónde termina el token actual
                const char *token_fin = (coma != NULL) ? coma : valor_fin;
                const char *token_inicio = cursor;
            
                // Eliminar espacios iniciales
                while (token_inicio < token_fin &&
                       (*token_inicio == ' ' || *token_inicio == '\t')) {
                    token_inicio++;
                }
            
                // Eliminar espacios finales
                while (token_fin > token_inicio &&
                       (token_fin[-1] == ' ' || token_fin[-1] == '\t')) {
                    token_fin--;
                }
            
                size_t longitud_token = token_fin - token_inicio;
            
                // Comprobar si el token es close
                if (longitud_token == 5 &&
                    strncasecmp(token_inicio, "close", 5) == 0) {
                    
                    *keep_alive = 0;
                }
            
                // Si no hay más comas, terminar
                if (coma == NULL) {
                    break;
                }
            
                // Continuar después de la coma
                cursor = coma + 1;
            }
        }
                inicio = linea_fin + 1;
    }    
        return 400;
}

int analizar_encabezados(
    const char *buffer,
    size_t longitud,
    char *host,
    size_t capacidad_host,
    char *user_agent,
    size_t capacidad_user_agent,
    int *keep_alive
) {
    if (buffer == NULL || host == NULL ||
        user_agent == NULL || keep_alive == NULL ||
        capacidad_host == 0 || capacidad_user_agent == 0) {
        return -1;
    }

    host[0] = '\0';
    user_agent[0] = '\0';

    const char *inicio = buffer;
    const char *fin = buffer + longitud;
    int host_encontrado = 0;

    // 1. Saltar la primera línea HTTP (GET /... HTTP/1.1)
    const char *linea_fin = memchr(inicio, '\n', fin - inicio);

    if (linea_fin == NULL || linea_fin == inicio ||
        linea_fin[-1] != '\r') {
        return 400;
    }

    inicio = linea_fin + 1;

    // 2. Recorrer las líneas de encabezados
    while (inicio < fin) {

        linea_fin = memchr(inicio, '\n', fin - inicio);

        if (linea_fin == NULL || linea_fin == inicio ||
            linea_fin[-1] != '\r') {
            return 400;
        }

        const char *fin_contenido = linea_fin - 1;

        // 3. Detectar la línea vacía final
        if (fin_contenido == inicio) {

            if (!host_encontrado) {
                return 400;
            }

            // Reutilizar nuestro analizador existente
            return analizar_connection(
                buffer, longitud, keep_alive
            );
        }

        // 4. Separar el nombre y el valor mediante ':'
        const char *dos_puntos = memchr(
            inicio, ':', fin_contenido - inicio
        );

        if (dos_puntos == NULL || dos_puntos == inicio) {
            return 400;
        }

        size_t longitud_nombre = dos_puntos - inicio;

        const char *valor_inicio = dos_puntos + 1;
        const char *valor_fin = fin_contenido;

        // Eliminar espacios iniciales
        while (valor_inicio < valor_fin &&
               (*valor_inicio == ' ' || *valor_inicio == '\t')) {
            valor_inicio++;
        }

        // Eliminar espacios finales
        while (valor_fin > valor_inicio &&
               (valor_fin[-1] == ' ' || valor_fin[-1] == '\t')) {
            valor_fin--;
        }

        // 5. Identificar el encabezado
        char *destino = NULL;
        size_t capacidad = 0;

        if (longitud_nombre == 4 &&
            strncasecmp(inicio, "Host", 4) == 0) {

            if (host_encontrado || valor_inicio == valor_fin) {
                return 400;
            }

            host_encontrado = 1;
            destino = host;
            capacidad = capacidad_host;

        } else if (longitud_nombre == 10 &&
                   strncasecmp(inicio, "User-Agent", 10) == 0) {

            destino = user_agent;
            capacidad = capacidad_user_agent;
        }

        // 6. Copiar el valor si es un encabezado reconocido
        if (destino != NULL) {

            size_t n = valor_fin - valor_inicio;

            if (n >= capacidad ||
                memchr(valor_inicio, '\0', n) != NULL) {
                return 400;
            }

            memcpy(destino, valor_inicio, n);
            destino[n] = '\0';
        }

        // 7. Continuar con el siguiente encabezado
        inicio = linea_fin + 1;
    }

    return 400;
}
