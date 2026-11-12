
#include <stdio.h>
#include "http.h"

int main(void)
{
    const char solicitud[] =
        "GET /index.html HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "User-Agent: curl/8.0\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";

    char host[128];
    char user_agent[128];
    int keep_alive = -1;

    int resultado = analizar_encabezados(
        solicitud,
        sizeof(solicitud) - 1,
        host,
        sizeof(host),
        user_agent,
        sizeof(user_agent),
        &keep_alive
    );

    printf("Resultado: %d\n", resultado);

    if (resultado == 0) {
        printf("Host: %s\n", host);
        printf("User-Agent: %s\n", user_agent);
        printf("Keep-alive: %d\n", keep_alive);
    }

    return 0;
}
