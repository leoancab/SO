#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <string.h>
#include "bmp.h"

// Constantes para acceso compartido
#define SHM_NAME "/shared"
#define SEM_NAME "/sem"

// ./publicador <imagen_origen> <titulo_salida> <numero_de_threads>
int main(int argc, char **argv)
{
    if (argc != 4)
    {
        printf("Uso: %s <imagen_origen> <salida> <numero_de_threads>\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *origen;
    FILE *salida;
    BMP_Image *imagen_origen = (BMP_Image *)malloc(sizeof(BMP_Image));
    BMP_Image *imagen_salida = (BMP_Image *)malloc(sizeof(BMP_Image));

    /*Proceso para cargar imagen BMP en memoria*/
    if ((origen = fopen(argv[1], "rb")) == NULL)
    {
        printError(FILE_ERROR);
        freeImage(imagen_origen);
        freeImage(imagen_salida);
        return EXIT_FAILURE;
    }

    if ((salida = fopen(argv[2], "wb")) == NULL)
    {
        printError(FILE_ERROR);
        freeImage(imagen_origen);
        freeImage(imagen_salida);
        return EXIT_FAILURE;
    }

    int num_threads = atoi(argv[3]);
    if (num_threads <= 0)
    {
        printf("Número de hilos inválido\n");
        freeImage(imagen_origen);
        freeImage(imagen_salida);
    }

    readImage(origen, imagen_origen);
    if (!checkBMPValid(&imagen_origen->header))
    {
        printError(VALID_ERROR);
        freeImage(imagen_origen);
        freeImage(imagen_salida);
        return EXIT_FAILURE;
    }

    printBMPHeader(&imagen_origen->header);
    printBMPImage(imagen_origen);
    fclose(origen);
    /*Fin proceso de carga imagen BMP en memoria*/

    // Tamaño de la imagen (Igual que en pa3)
    size_t image_size = sizeof(BMP_Header) + (imagen_origen->norm_height * imagen_origen->header.width_px * sizeof(Pixel));

    // Crear Memoria compartida
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1)
    {
        printf("Error al crear memoria compartida1\n");
        freeImage(imagen_origen);
        freeImage(imagen_salida);
        close(shm_fd);
        shm_unlink(SHM_NAME);
        return EXIT_FAILURE;
    }

    // Ajustar tamaño de memoria compartida
    if (ftruncate(shm_fd, image_size) == -1)
    {
        perror("Error al configurar tamaño de memoria compartida");
        freeImage(imagen_origen);
        freeImage(imagen_salida);
        close(shm_fd);
        shm_unlink(SHM_NAME);
        return EXIT_FAILURE;
    }

    // Ajustar tamaño de la memoria compartida
    void *shm_ptr = mmap(NULL, image_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED)
    {
        printf("Error al mapear memoria compartida2\n");
        freeImage(imagen_origen);
        freeImage(imagen_salida);
        close(shm_fd);
        shm_unlink(SHM_NAME);
        return EXIT_FAILURE;
    }

    // Copiar la imagen a la memoria compartida
    // memcpy(shm_ptr, &imagen_origen->header, sizeof(BMP_Header));
    // memcpy(shm_ptr + sizeof(BMP_Header), imagen_origen->pixels, imagen_origen->norm_height * imagen_origen->header.width_px * sizeof(Pixel));

    memcpy(shm_ptr, &imagen_origen->header, sizeof(BMP_Header));

    Pixel *shm_pixels = (Pixel *)(shm_ptr + sizeof(BMP_Header));

    for (int i = 0; i < imagen_origen->norm_height; i++)
    {
        memcpy(shm_pixels + (i * imagen_origen->header.width_px),
               imagen_origen->pixels[i],
               imagen_origen->header.width_px * sizeof(Pixel));
    }

    printf("Imagen cargada en memoria compartida.\n");

    // Crear el semaforo para sincronizar los procesos
    // Y controlar acceso a memoria compartida
    sem_t *sem = sem_open(SEM_NAME, O_CREAT, 0644, 0);
    if (sem == SEM_FAILED)
    {
        perror("Error al crear semáforo");
        freeImage(imagen_origen);
        freeImage(imagen_salida);
        munmap(shm_ptr, image_size);
        close(shm_fd);
        shm_unlink(SHM_NAME);
        return EXIT_FAILURE;
    }

    // Fin memoria compartida

    /*Lanzar los demas procesos*/
    pid_t pid_desenfocador = fork();
    if (pid_desenfocador == 0)
    {
        execlp("./desenfocador", "./desenfocador", argv[3], NULL);
        perror("Error al ejecutar el Desenfocador");
        return EXIT_FAILURE;
    }

    pid_t pid_realzador = fork();
    if (pid_realzador == 0)
    {
        execlp("./realzador", "./realzador", argv[3], NULL);
        perror("Error al ejecutar el Realzador");
        return EXIT_FAILURE;
    }

    /*Esperar que los procesos terminen*/
    waitpid(pid_desenfocador, NULL, 0);
    waitpid(pid_realzador, NULL, 0);

    printf("Procesamiento completado. Recuperando imagen de la memoria compartida...\n");

    sem_post(sem);

    BMP_Image *image_procesada = (BMP_Image *)mmap(NULL, image_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (image_procesada == MAP_FAILED)
    {
        perror("Error al mapear memoria compartida para guardar imagen");
        close(shm_fd);
        shm_unlink(SHM_NAME);
        return EXIT_FAILURE;
    }

    writeImage(salida, image_procesada);
    fclose(salida);

    // Liberar memoria y recursos
    freeImage(imagen_origen);
    freeImage(image_procesada);
    sem_close(sem);
    sem_unlink(SEM_NAME);
    munmap(shm_ptr, image_size);
    close(shm_fd);
    shm_unlink(SHM_NAME);

    return EXIT_SUCCESS;
}