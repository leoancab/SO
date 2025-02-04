#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

// Estructuras para leer el encabezado de la imagen BMP
#pragma pack(push, 1)
typedef struct {
    unsigned short bfType;         // Tipo de archivo
    unsigned int bfSize;           // Tamaño del archivo
    unsigned short bfReserved1;
    unsigned short bfReserved2;
    unsigned int bfOffBits;        // Desplazamiento hasta los datos de la imagen
} BMPHeader;

typedef struct {
    unsigned int biSize;           // Tamaño de la estructura
    int biWidth;                   // Ancho de la imagen
    int biHeight;                  // Alto de la imagen
    unsigned short biPlanes;       // Número de planos de color
    unsigned short biBitCount;     // Número de bits por píxel
    unsigned int biCompression;    // Tipo de compresión
    unsigned int biSizeImage;      // Tamaño de la imagen
    int biXPelsPerMeter;           // Resolución horizontal
    int biYPelsPerMeter;           // Resolución vertical
    unsigned int biClrUsed;        // Número de colores utilizados
    unsigned int biClrImportant;   // Colores importantes
} BMPInfoHeader;
#pragma pack(pop)

typedef struct {
    BMPHeader header;
    BMPInfoHeader infoHeader;
    unsigned char *data; // Píxeles de la imagen
} BMPImage;

// Estructura para pasar información a los hilos
typedef struct {
    BMPImage *imagen;
    int start_row;
    int end_row;
} ThreadData;

// Kernel de desenfoque (promedio 3x3)
float kernel[3][3] = {
    {1.0/9, 1.0/9, 1.0/9},
    {1.0/9, 1.0/9, 1.0/9},
    {1.0/9, 1.0/9, 1.0/9}
};

// Función para aplicar el desenfoque a un píxel específico
void aplicar_desenfoque(BMPImage *imagen, int x, int y) {
    int width = imagen->infoHeader.biWidth;
    int height = imagen->infoHeader.biHeight;

    if (x <= 0 || x >= width - 1 || y <= 0 || y >= height - 1) {
        return; // Evitar bordes de la imagen
    }

    float r = 0, g = 0, b = 0;

    // Aplicar el kernel 3x3
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            int nx = x + i;
            int ny = y + j;
            int idx = (ny * width + nx) * 3;
            r += imagen->data[idx] * kernel[i + 1][j + 1];
            g += imagen->data[idx + 1] * kernel[i + 1][j + 1];
            b += imagen->data[idx + 2] * kernel[i + 1][j + 1];
        }
    }

    // Asignar el nuevo valor al píxel desenfocado
    int idx = (y * width + x) * 3;
    imagen->data[idx] = (unsigned char)r;
    imagen->data[idx + 1] = (unsigned char)g;
    imagen->data[idx + 2] = (unsigned char)b;
}

// Función que ejecutará cada hilo para desenfocar una parte de la imagen
void* desenfocar_parte(void* arg) {
    ThreadData *data = (ThreadData *)arg;
    BMPImage *imagen = data->imagen;
    int start_row = data->start_row;
    int end_row = data->end_row;
    int width = imagen->infoHeader.biWidth;

    // Aplicar el desenfoque a la primera mitad de la imagen (especificado por las filas)
    for (int y = start_row; y < end_row; y++) {
        for (int x = 0; x < width; x++) {
            aplicar_desenfoque(imagen, x, y);
        }
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <num_hilos>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int num_hilos = atoi(argv[1]);

    const char *shm_name = "/imagen_shm";
    int shm_fd;
    BMPImage *imagen;

    // Abrir la memoria compartida
    shm_fd = shm_open(shm_name, O_RDONLY, 0666);
    if (shm_fd == -1) {
        perror("Error al abrir la memoria compartida");
        exit(EXIT_FAILURE);
    }

    // Mapear la memoria compartida
    imagen = (BMPImage *)mmap(NULL, sizeof(BMPImage), PROT_READ, MAP_SHARED, shm_fd, 0);
    if (imagen == MAP_FAILED) {
        perror("Error al mapear la memoria compartida");
        exit(EXIT_FAILURE);
    }

    int height = imagen->infoHeader.biHeight;
    int rows_per_thread = height / 2 / num_hilos; // Dividir la mitad superior de la imagen entre los hilos

    pthread_t threads[num_hilos];
    ThreadData thread_data[num_hilos];

    // Crear los hilos para desenfocar la primera mitad de la imagen
    for (int i = 0; i < num_hilos; i++) {
        thread_data[i].imagen = imagen;
        thread_data[i].start_row = i * rows_per_thread;
        thread_data[i].end_row = (i == num_hilos - 1) ? (height / 2) : (i + 1) * rows_per_thread;
        pthread_create(&threads[i], NULL, desenfocar_parte, &thread_data[i]);
    }

    // Esperar a que todos los hilos terminen
    for (int i = 0; i < num_hilos; i++) {
        pthread_join(threads[i], NULL);
    }

    // Guardar la imagen desenfocada en un nuevo archivo BMP (opcional)
    FILE *salida = fopen("imagen_desenfocada.bmp", "wb");
    if (!salida) {
        perror("Error al guardar la imagen");
        exit(EXIT_FAILURE);
    }
    fwrite(&imagen->header, sizeof(BMPHeader), 1, salida);
    fwrite(&imagen->infoHeader, sizeof(BMPInfoHeader), 1, salida);
    fwrite(imagen->data, imagen->infoHeader.biSizeImage, 1, salida);
    fclose(salida);

    // Desvincular y cerrar la memoria compartida
    if (munmap(imagen, sizeof(BMPImage)) == -1) {
        perror("Error al desvincular la memoria compartida");
    }

    close(shm_fd);

    return 0;
}