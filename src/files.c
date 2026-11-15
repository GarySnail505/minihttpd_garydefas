#define _XOPEN_SOURCE 700
#include "files.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int leer_archivo(const char *ruta, char *buffer, size_t capacidad,
                 size_t *longitud)
{
    *longitud = 0;

    FILE *archivo = fopen(ruta, "rb"); // leer contenido binario
    if (archivo == NULL)
        return errno == ENOENT ? 1 : -1;

    size_t leidos = fread(buffer, 1, capacidad, archivo);
    int sobra = leidos == capacidad && fgetc(archivo) != EOF;
    int error = ferror(archivo);

    if (fclose(archivo) != 0 || error || sobra)
        return -1;

    *longitud = leidos;
    return 0;
}

int abrir_archivo(const char *ruta, FILE **archivo, size_t *longitud)
{
    *archivo = NULL;
    *longitud = 0;

    FILE *abierto = fopen(ruta, "rb");
    if (abierto == NULL) {
        if (errno == ENOENT || errno == ENOTDIR)
            return 1;
        if (errno == EACCES || errno == EPERM || errno == ELOOP)
            return 403;
        return -1;
    }

    long tamanio = -1;
    if (fseek(abierto, 0, SEEK_END) == 0)
        tamanio = ftell(abierto); // obtener el tamaño del archivo (content-length)

    if (tamanio < 0 || (uintmax_t)tamanio > SIZE_MAX ||
        fseek(abierto, 0, SEEK_SET) != 0) { // restablecer el puntero al inicio
        fclose(abierto);
        return -1;
    }

    *archivo = abierto;
    *longitud = (size_t)tamanio;
    return 0;
}

int resolver_ruta_www(const char *uri, char *ruta)
{
    char candidata[PATH_MAX] = "www";
    size_t longitud = strcspn(uri, "?");
    if (uri[0] != '/' || longitud > sizeof(candidata) - sizeof("www"))
        return 400;

    size_t usados = strlen(candidata);
    for (size_t i = 0; i < longitud; i++) {
        unsigned char byte = (unsigned char)uri[i];
        if (byte == '%') {
            if (i + 2 >= longitud || !isxdigit((unsigned char)uri[i + 1]) ||
                !isxdigit((unsigned char)uri[i + 2]))
                return 400;

            char hex[] = {uri[i + 1], uri[i + 2], '\0'};
            byte = (unsigned char)strtol(hex, NULL, 16);
            i += 2;
        }
        if (byte < ' ' || byte == 127)
            return 400;
        candidata[usados++] = (char)byte;
    }
    candidata[usados] = '\0';

    if (strcmp(candidata, "www/") == 0)
        memcpy(candidata, "www/index.html", sizeof("www/index.html"));

    for (const char *componente = candidata + 4; *componente != '\0';) {
        size_t n = strcspn(componente, "/");
        if (n == 2 && memcmp(componente, "..", 2) == 0)
            return 403;
        componente += n;
        if (*componente == '/')
            componente++;
    }

    char raiz[PATH_MAX];
    if (realpath("www", raiz) == NULL)
        return -1;
    size_t longitud_raiz = strlen(raiz);

    int falta_archivo = 0;
    while (realpath(candidata, ruta) == NULL) {
        if (errno == EACCES || errno == EPERM || errno == ELOOP)
            return 403;
        if (errno != ENOENT && errno != ENOTDIR)
            return -1;

        // Un recurso ausente aún puede estar bajo un enlace a un directorio externo.
        char *barra = strrchr(candidata, '/');
        if (barra == NULL)
            return 404;
        *barra = '\0';
        falta_archivo = 1;
    }

    if (strcmp(ruta, raiz) == 0)
        return falta_archivo ? 404 : 403;
    if (strncmp(ruta, raiz, longitud_raiz) != 0 ||
        (longitud_raiz > 1 && ruta[longitud_raiz] != '/'))
        return 403;
    if (falta_archivo)
        return 404;

    struct stat datos;
    if (stat(ruta, &datos) == -1) {
        if (errno == ENOENT || errno == ENOTDIR)
            return 404;
        if (errno == EACCES || errno == EPERM || errno == ELOOP)
            return 403;
        return -1;
    }

    return S_ISREG(datos.st_mode) ? 0 : 403;
}
