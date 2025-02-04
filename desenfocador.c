#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "bmp.h"
#include "filter.h"

#define SHM_NAME "/shared"

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Uso: %s <numero_de_threads>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int numThreads = atoi(argv[1]);
    if (numThreads <= 0)
    {
        printf("Número de hilos inválido.\n");
        return EXIT_FAILURE;
    }

    // Acceder a memoria compartida
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1)
    {
        perror("Error al abrir memoria compartida");
        return EXIT_FAILURE;
    }

    // BMP_Header header;
    // read(shm_fd, &header, sizeof(BMP_Header));
    // size_t image_size = sizeof(BMP_Header) + (header.height_px * header.width_px * sizeof(Pixel));
    // size_t image_size = sizeof(BMP_Header) + (image->header.width_px * abs(image->header.height_px) * image->bytes_per_pixel);
    // size_t image_size = sizeof(BMP_Header) + (header.width_px * abs(header.height_px) * sizeof(Pixel));
    // printf("Tamaño de memoria compartida configurado: %zu bytes\n", image_size);

    // Mapear memoria compartida
    BMP_Image *image = (BMP_Image *)mmap(NULL, sizeof(BMP_Image), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (image == MAP_FAILED)
    {
        printf("Error al mapear memoria2\n");
        close(shm_fd);
        return EXIT_FAILURE;
    }

    size_t image_size = sizeof(BMP_Header) + (image->header.width_px * abs(image->header.height_px) * sizeof(Pixel));
    printf("Tamaño de memoria compartida configurado: %zu bytes\n", image_size);

    munmap(image, sizeof(BMP_Image));
    // image = (BMP_Image*)mmap(NULL, image_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    // if (image == MAP_FAILED) {
    //     perror("Error al mapear memoria compartida correctamente");
    //     close(shm_fd);
    //     return EXIT_FAILURE;
    // }

    image->pixels = (Pixel **)malloc(image->norm_height * sizeof(Pixel *));
    Pixel *shm_pixels = (Pixel *)(image + sizeof(BMP_Header));

    for (int i = 0; i < image->norm_height; i++)
    {
        image->pixels[i] = shm_pixels + (i * image->header.width_px);
    }

    image->norm_height = abs(image->header.height_px);
    image->bytes_per_pixel = image->header.bits_per_pixel / 8; // 32 bits por pixel => 4 bytes

    printf("-----------------------Descaoasd\n");
    printf("Width: %d, Height: %d, Bits per pixel: %d\n", image->header.width_px, image->header.height_px, image->header.bits_per_pixel);
    printBMPImage(image);

    applyParallel(image, image, (int[3][3]){{1, 1, 1}, {1, 1, 1}, {1, 1, 1}}, numThreads, true);

    // Desvincular la memoria compartida
    close(shm_fd);

    printf("Desenfocador: Filtro aplicado correctamente a la mitad superior.\n");
    return EXIT_SUCCESS;
}