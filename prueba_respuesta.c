#include <stdio.h>
#include <string.h>
#include "http.h"

int main(void)
{
    char uri[256];

    const char solicitud[] = "GET /index.html.com.ex HTTP/1.1\r\n";

    int resultado = analizar_linea_solicitud(
        solicitud,
        sizeof(solicitud) - 1,
        uri,
        sizeof(uri)
    );

    printf("Resultado: %d\n", resultado);

    if (resultado == 0) {
        printf("URI: %s\n", uri);
    }
    return 0;
}