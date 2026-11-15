#include "mime.h"

#include <string.h>

const char *tipo_mime(const char *ruta)
{
    const char *nombre = strrchr(ruta, '/'); // ultima '/' en la ruta
    const char *extension = strrchr(nombre != NULL ? nombre + 1 : ruta, '.');

    if (extension != NULL) {
        if (strcmp(extension, ".html") == 0)
            return "text/html";
        if (strcmp(extension, ".css") == 0)
            return "text/css";
        if (strcmp(extension, ".js") == 0)
            return "application/javascript";
        if (strcmp(extension, ".png") == 0)
            return "image/png";
        if (strcmp(extension, ".jpg") == 0)
            return "image/jpeg";
    }

    return "application/octet-stream";
}
