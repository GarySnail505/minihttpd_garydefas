#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

/* Devuelve el limite tras CRLF CRLF, o 0 si los encabezados estan incompletos. */
size_t headers_end(const char *buffer, size_t used);

#endif /* HTTP_H */
