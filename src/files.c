#include "files.h"

#include <errno.h>
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