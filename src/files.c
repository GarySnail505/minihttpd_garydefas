#include "files.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>

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
    if (abierto == NULL)
        return errno == ENOENT ? 1 : -1;

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
