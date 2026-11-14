#ifndef FILES_H
#define FILES_H

#include <stddef.h>

// 0: leído, 1: no existe, -1: error o espacio insuficiente.
int leer_archivo(const char *ruta, char *buffer, size_t capacidad,
                 size_t *longitud);

#endif