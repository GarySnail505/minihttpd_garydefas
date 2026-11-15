# Proyecto: MiniHTTPd GR2CC
Servidor HTTP/1.1 de archivos estáticos escrito en C para el proyecto de Computación Distribuida.

Estudiante: GARY MATEO DEFAS CONDOR

## Estructura

- `src/` contiene la implementación del servidor.
- `include/` contiene los encabezados.
- `www/` contiene los archivos que sirve el servidor.

## Compilación

Desde la raíz del repositorio, ejecuta `make` para generar `minihttpd`. Usa `make clean` para eliminar el ejecutable.

## Ejecución

Ejecuta el servidor desde la raíz del repositorio. El programa usa `realpath("www")` para resolver `www/` a partir del directorio de trabajo.
El servidor escucha en `127.0.0.1:8080` por defecto. Puedes indicar otro puerto como argumento.

```sh
./minihttpd
```
```sh
./minihttpd 8081
```

## Funcionalidades

- Procesamiento incremental de solicitudes GET HTTP/1.1 con encabezado Host obligatorio.
- Concurrencia con epoll y envío por sockets no bloqueantes. Los archivos se envían en bloques de hasta 1024 bytes.
- Conexiones persistentes para respuestas OK 200, salvo que el cliente solicite `Connection: close`. 
- Archivos estáticos con tipos MIME para `.html`, `.css`, `.js`, `.png` y `.jpg`. Las demás extensiones se sirven como `application/octet-stream`.
- Validación de rutas con `realpath()`. Se rechazan componentes `..` y rutas que resuelvan fuera de `www/`.

## Códigos HTTP

| Código | Motivo |
| --- | --- |
| `200 OK` | Archivo servido correctamente. |
| `400 Bad Request` | Solicitud inválida. |
| `403 Forbidden` | Acceso denegado. |
| `404 Not Found` | Recurso ausente. |
| `405 Method Not Allowed` | Método distinto de `GET`. |
| `500 Internal Server Error` | Fallo interno al preparar la respuesta. |
