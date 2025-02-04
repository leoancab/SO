#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <errno.h>
#include <string.h>

#define SHM_NAME "/imagen_shm"
#define SEM_NAME "/sem_imagen"

// Estructuras BMP (como definidas antes, para manejar las imágenes)
#pragma pack(push, 1)
typedef struct {
    unsigned short bfType;
    unsigned int bfSize;
    unsigned short bfReserved1;
    unsigned short bfReserved2;
    unsigned int bfOffBits;
} BMPHeader;

typedef struct {
    unsigned int biSize;
    int biWidth;
    int biHeight;
    unsigned short biPlanes;
    unsigned short biBitCount;
    unsigned int biCompression;
    unsigned int biSizeImage;
    int biXPelsPerMeter;
    int biYPelsPerMeter;
    unsigned int biClrUsed;
    unsigned int biClrImportant;
} BMPInfoHeader;
#pragma pack(pop)

typedef struct {
    BMPHeader header;
    BMPInfoHeader infoHeader;
    unsigned char *data; // Píxeles de la imagen
} BMPImage;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <ruta_de_guardado>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *output_path = argv[1]; // Ruta para guardar la imagen final

    // Abre el archivo de memoria compartida
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Error al abrir la memoria compartida");
        exit(EXIT_FAILURE);
    }

    // Definir el tamaño de la imagen (esto debe coincidir con el tamaño de la imagen real)
    size_t image_size = sizeof(BMPImage); // Ajustar esto según el tamaño de la imagen

    // Configurar el tamaño de la memoria compartida
    if (ftruncate(shm_fd, image_size) == -1) {
        perror("Error al configurar el tamaño de la memoria compartida");
        exit(EXIT_FAILURE);
    }

    // Mapear la memoria compartida
    BMPImage *imagen = (BMPImage *)mmap(NULL, image_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (imagen == MAP_FAILED) {
        perror("Error al mapear la memoria compartida");
        exit(EXIT_FAILURE);
    }

    // Inicializar el semáforo
    sem_t *sem = sem_open(SEM_NAME, O_CREAT, 0644, 1);
    if (sem == SEM_FAILED) {
        perror("Error al crear semáforo");
        exit(EXIT_FAILURE);
    }

    // Cargar la imagen en la memoria compartida (esto se asume que ya está hecho en el Publicador)
    // Aquí es donde la imagen se carga en la memoria compartida por el Publicador.

    // Crear procesos para el Desenfocador y Realzador
    pid_t pid_des = fork();
    if (pid_des == 0) {
        // Proceso hijo - Desenfocador
        execlp("./desenfocador", "./desenfocador", SEM_NAME, NULL);
        perror("Error al ejecutar el desenfocador");
        exit(EXIT_FAILURE);
    } else if (pid_des < 0) {
        perror("Error en fork() para desenfocador");
        exit(EXIT_FAILURE);
    }

    pid_t pid_realz = fork();
    if (pid_realz == 0) {
        // Proceso hijo - Realzador
        execlp("./realzador", "./realzador", SEM_NAME, NULL);
        perror("Error al ejecutar el realzador");
        exit(EXIT_FAILURE);
    } else if (pid_realz < 0) {
        perror("Error en fork() para realzador");
        exit(EXIT_FAILURE);
    }

    // Esperar a que los procesos hijos terminen
    waitpid(pid_des, NULL, 0);
    waitpid(pid_realz, NULL, 0);

    // Aquí es donde se realiza la combinación de las dos porciones procesadas

    // Combinamos las dos mitades de la imagen procesada
    int height = imagen->infoHeader.biHeight;
    int width = imagen->infoHeader.biWidth;
    int half_height = height / 2;

    // Asegurarse de que la imagen fue procesada correctamente
    // La primera mitad de la imagen ya fue procesada por el Desenfocador
    // La segunda mitad fue procesada por el Realzador
    // Ahora se guardan ambas mitades en la misma imagen

    // Ruta y nombre de archivo para guardar la imagen final
    FILE *output_file = fopen(output_path, "wb");
    if (!output_file) {
        perror("Error al abrir el archivo de salida");
        exit(EXIT_FAILURE);
    }

    // Escribir el encabezado BMP y la información
    fwrite(&imagen->header, sizeof(BMPHeader), 1, output_file);
    fwrite(&imagen->infoHeader, sizeof(BMPInfoHeader), 1, output_file);

    // Escribir los datos de la imagen combinada
    fwrite(imagen->data, imagen->infoHeader.biSizeImage, 1, output_file);
    fclose(output_file);

    // Desvincular la memoria compartida
    if (munmap(imagen, image_size) == -1) {
        perror("Error al desvincular la memoria compartida");
    }

    close(shm_fd);

    // Cerrar el semáforo
    sem_close(sem);