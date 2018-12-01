#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>
	
#ifndef PUERTO
#define PUERTO 5000
#endif

void enviar(char *address, char* archivo);
void recibir(char *address, char* archivo);

int main(int argc, char **argv)
{
	if (argc < 4)
	{
		puts("Faltan parametros");
		exit(EXIT_FAILURE);
	}

	int opcion = getopt(argc, argv, "er");
	switch(opcion){
		case 'e':
			printf("Enviando %s a %s\n", argv[3], argv[2]);
			enviar(argv[2], argv[3]);
			break;
		case 'r':
			recibir(argv[2], argv[3]);
			break;
	}
	return 0;
}

void enviar(char* address, char* archivo)
{
	int cliente_socket, fd;
	struct stat file_stat;
    ssize_t tamanio;
    struct sockaddr_in direccion_remota;
    char tamanio_archivo[256];
    int bytes_enviados = 0;
    long int datos_restantes = 0, offset;

    /* Rellenando de ceros direccion_remota */
    memset(&direccion_remota, 0, sizeof(direccion_remota));

    /* Construyendo la direccion remota */
    direccion_remota.sin_family = AF_INET;
    inet_pton(AF_INET, address, &(direccion_remota.sin_addr));
    direccion_remota.sin_port = htons(PUERTO);

    /* Creando socket cliente */
    cliente_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (cliente_socket == -1)
    {
        fprintf(stderr, "Error creating socket --> %s\n", strerror(errno));

        exit(EXIT_FAILURE);
	}
	
    /* Conectando al servidor */
    if (connect(cliente_socket, (struct sockaddr *)&direccion_remota, sizeof(struct sockaddr)) == -1)
    {
        fprintf(stderr, "Error on connect --> %s\n", strerror(errno));

        exit(EXIT_FAILURE);
    }

    /* Abriendo archivo para lectura */
    fd = open(archivo, O_RDONLY);
    if (fd == -1)
    {
            fprintf(stderr, "Error abriendo archivo --> %s", strerror(errno));

            exit(EXIT_FAILURE);
    }

    /* Obteniendo informacion del archivo */
    if (fstat(fd, &file_stat) < 0)
    {
            fprintf(stderr, "Error obteniendo informacion del archivo --> %s", strerror(errno));

            exit(EXIT_FAILURE);
    }

    fprintf(stdout, "Tamaño del archivo: \n%ld bytes\n", file_stat.st_size);

    sprintf(tamanio_archivo, "%ld", file_stat.st_size);

    tamanio = send(cliente_socket, tamanio_archivo, sizeof(tamanio_archivo), 0);

    if (tamanio < 0)
    {
          fprintf(stderr, "Error enviando saludo --> %s", strerror(errno));

          exit(EXIT_FAILURE);
    }

    fprintf(stdout, "Enviados %ld bytes con tamaño al servidor\n", tamanio);

    offset = 0;
    datos_restantes = file_stat.st_size;

    /* Enviando archivo */
    while (((bytes_enviados = sendfile(cliente_socket, fd, &offset, BUFSIZ)) > 0) && (datos_restantes > 0))
    {
            fprintf(stdout, "1. Enviados %d bytes del archivo, offset es ahora : %ld y resta %ld por enviar \n", bytes_enviados, offset, datos_restantes);
            datos_restantes -= bytes_enviados;
            fprintf(stdout, "2. Enviados %d bytes del archivo, offset es ahora : %ld y resta %ld por enviar \n", bytes_enviados, offset, datos_restantes);
    }

    close(fd);
    close(cliente_socket);
}

void recibir(char* address, char* archivo)
{
	int servidor_socket, cliente_socket;
	struct sockaddr_in servidor_addr, cliente_addr;
	socklen_t sock_tam;
	char buffer[BUFSIZ];
	FILE* archivo_recibido;
	struct hostent *host;
	int sockopt_val = 1;
	int tamanio_archivo;
	int datos_restantes;
	ssize_t tamanio;

    /* Crear socket */
    servidor_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (servidor_socket == -1)
    {
        fprintf(stderr, "Error creando socket --> %s", strerror(errno));

        exit(EXIT_FAILURE);
    }

    setsockopt(servidor_socket, SOL_SOCKET,SO_REUSEADDR, &sockopt_val, sizeof(int));	

    /* Rellenar de zeros el address*/
    memset(&servidor_addr, 0, sizeof(servidor_addr));

    /* Construir la address */
    servidor_addr.sin_family = AF_INET;

    //inet_pton(AF_INET, address, &(servidor_addr.sin_addr));
    host = gethostbyname(address);
    bcopy(host->h_addr, &servidor_addr.sin_addr, host->h_length);


    servidor_addr.sin_port = htons(PUERTO);

    /* Bind */
    if ((bind(servidor_socket, (struct sockaddr *)&servidor_addr, sizeof(struct sockaddr))) == -1)
    {
        fprintf(stderr, "Error en bind --> %s", strerror(errno));

        exit(EXIT_FAILURE); 
    }
    
    /* Escuchando el puerto para conexiones entrantes */
    if ((listen(servidor_socket, 5)) == -1)
    {
        fprintf(stderr, "Error on listen --> %s", strerror(errno));

        exit(EXIT_FAILURE);
    }

    printf("Corriendo en %s:%d REUSE:%d\n", address, PUERTO, sockopt_val);

    /* Aceptando conexiones entrantes */
    sock_tam = sizeof(struct sockaddr_in);

    cliente_socket = accept(servidor_socket, (struct sockaddr *)&cliente_addr, &sock_tam);
    if (cliente_socket == -1)
    {
            fprintf(stderr, "Error on accept --> %s", strerror(errno));

            exit(EXIT_FAILURE);
    }
    
    fprintf(stdout, "Cliente aceptado --> %s\n", inet_ntoa(cliente_addr.sin_addr));

    /* Recibiendo tamaño del archivo */
    recv(cliente_socket, buffer, BUFSIZ, 0);
    tamanio_archivo = atoi(buffer);

    printf("Recibido tamaño del archivo: %d\n", tamanio_archivo);

    archivo_recibido = fopen(archivo, "w");
    if (archivo_recibido == NULL)
    {
            fprintf(stderr, "Error creando archivo --> %s\n", strerror(errno));

            exit(EXIT_FAILURE);
    }

    datos_restantes = tamanio_archivo;

    while (((tamanio = recv(cliente_socket, buffer, BUFSIZ, 0)) > 0) && (datos_restantes > 0))
    {
            fwrite(buffer, sizeof(char), tamanio, archivo_recibido);
            datos_restantes -= tamanio;
            fprintf(stdout, "Recibidos %ld bytes y se esperan :- %d bytes\n", tamanio, datos_restantes);
    }
    fclose(archivo_recibido);

    close(cliente_socket);
    close(servidor_socket);
}