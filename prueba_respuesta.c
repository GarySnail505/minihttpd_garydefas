#include <stdio.h>
#include "http.h"

int main(void)
{
    const char solicitud[] =
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: keep-alive, close\r\n"
        "\r\n";

    int keep_alive = -1;

    int resultado = analizar_connection(
        solicitud,
        sizeof(solicitud) - 1,
        &keep_alive
    );

    printf("Resultado: %d\n", resultado);
    printf("Keep-alive: %d\n", keep_alive);

    return 0;
}