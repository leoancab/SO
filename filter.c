#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "bmp.h"
#include "filter.h"
#include <stdbool.h>

void apply(BMP_Image * imageIn, BMP_Image * imageOut)
{
    if (imageIn == NULL || imageOut == NULL) {
        printf("Invalid Input or Output Image in apply()\n");
        return;
    }

    imageOut->header = imageIn->header;
    imageOut->norm_height = imageIn->norm_height;
    imageOut->bytes_per_pixel = imageIn->bytes_per_pixel;

    printf("Flag2.1\n");

    imageOut->pixels = (Pixel**)malloc(sizeof(Pixel*) * imageOut->norm_height);
    if (imageOut->pixels == NULL) {
        printf("Error while allocating memory for imageOut->pixels in apply()\n");
        return;
    }

    printf("Flag2.2\n");

    int row_size = imageOut->header.width_px * imageOut->bytes_per_pixel;

    for (int i = 0; i < imageOut->norm_height; i++) {
        imageOut->pixels[i] = (Pixel*)malloc(row_size);
        if (imageOut->pixels[i] == NULL) {
            printf("Memory allocation failed for imageOut->pixels[%d]", i);
            freeImage(imageOut);
            return;
        }
    }
    printf("Flag2.3\n");

    //Factor de normalizacion para el filtro
    const float factor = 9.0;

    //Iterar sobre cada pixel de la imagen de entrada
    for (int y = 0; y < imageIn->norm_height; y++) {
        for (int x = 0; x < imageIn->header.width_px; x++) {
            int red = 0, blue = 0, green = 0, alpha = 0;

            //Aplicar el filtro 3x3
            for (int fy = -1; fy <= 1; fy++) {
                for(int fx = -1; fx <= 1; fx++) {
                    int neighbor_y = y + fy;
                    int neighbor_x = x + fx;

                    //Verificar limites para no acceder fuera de la matriz
                    if (
                        neighbor_y >= 0 && 
                        neighbor_y < imageIn->norm_height &&
                        neighbor_x >= 0 && 
                        neighbor_x < imageIn->header.width_px
                    ) {
                        Pixel* neighbor = &imageIn->pixels[neighbor_y][neighbor_x];
                        red += neighbor->red;
                        green += neighbor->green;
                        blue += neighbor->blue;
                        alpha += neighbor->alpha;
                    }
                }
            }

            Pixel* output_pixel = &imageOut->pixels[y][x];
            output_pixel->red = (uint8_t)(red / factor);
            output_pixel->green = (uint8_t)(green / factor);
            output_pixel->blue = (uint8_t)(blue / factor);
            output_pixel->alpha = (uint8_t)(alpha / factor);
        }
    }
    printf("flag2.4\n");
}

void applyParallel(BMP_Image * imageIn, BMP_Image * imageOut, int boxFilter[3][3], int numThreads, bool top)
{
    if (imageIn == NULL || imageOut == NULL || numThreads <= 0) {
        printf("Invalid arguments for applyParallel()\n");
        return;
    }

    // imageOut->header = imageIn->header;
    // imageOut->norm_height = imageIn->norm_height;
    // imageOut->bytes_per_pixel = imageIn->bytes_per_pixel;

    // imageOut->pixels = (Pixel**)malloc(sizeof(Pixel*) * imageOut->norm_height);
    // if (imageOut->pixels == NULL) {
    //     printf("Error while allocating memory for imageOut->pixels in apply()\n");
    //     return;
    // }

    // int row_size = imageOut->header.width_px * imageOut->bytes_per_pixel;

    // for (int i = 0; i < imageOut->norm_height; i++) {
    //     imageOut->pixels[i] = (Pixel*)malloc(row_size);
    //     if (imageOut->pixels[i] == NULL) {
    //         printf("Memory allocation failed for imageOut->pixels[%d]", i);
    //         freeImage(imageOut);
    //         return;
    //     }
    // }

    pthread_t threads[numThreads];
    ThreadArgs threadArgs[numThreads];

    int altura_total = abs(imageIn->norm_height);
    int mitad_inicio, mitad_fin;

    //MODIFICACION
    if (top) {
        mitad_inicio = 0;
        mitad_fin = altura_total / 2;
    } else {
        mitad_inicio = altura_total / 2;
        mitad_fin = altura_total;
    }

    int rows_per_thread = (mitad_fin - mitad_inicio) / numThreads;
    int sobrante = (mitad_fin - mitad_inicio) % numThreads;

    for (int i = 0; i < numThreads; i++) {
        threadArgs[i].imageIn = imageIn;
        threadArgs[i].imageOut = imageIn;
        threadArgs[i].start_row = mitad_inicio + i * rows_per_thread;
        threadArgs[i].end_row = mitad_inicio + (i + 1) * rows_per_thread;

        if (i == numThreads - 1) {
            threadArgs[i].end_row += sobrante;
        }
        threadArgs[i].width = imageIn->header.width_px;
        threadArgs[i].height = imageIn->norm_height; 

        for (int fy = 0; fy < 3; fy++) {
            for (int fx = 0; fx < 3; fx++) {
                threadArgs[i].boxFilter[fy][fx] = boxFilter[fy][fx];
            }
        }

        if (pthread_create(&threads[i], NULL,  filterThreadWorker, &threadArgs[i]) != 0) {
            printf("Error while creating threads\n");
            return;
        }
    }
    
    for (int i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("Image filter applied in paralled succesfully with %d threads\n", numThreads);
}

void *filterThreadWorker(void * args)
{
    ThreadArgs* threadArgs = (ThreadArgs*)args;
    BMP_Image* imageIn = threadArgs->imageIn;
    BMP_Image* imageOut = threadArgs->imageOut;
    int start_row  = threadArgs->start_row;
    int end_row = threadArgs->end_row;
    int width = threadArgs->width;
    int height = threadArgs->height;
    double factor = 9.0;

    for (int y = start_row; y < end_row; y++) {
        for (int x = 0; x < width; x++) {
            int red = 0, green = 0, blue = 0, alpha = 0;

            //Aplicar el filtro
            for (int fy = -1; fy <= 1; fy++) {
                for (int fx = -1; fx <= 1; fx++) {
                    int neighbor_y = y + fy;
                    int neighbor_x = x + fx;

                    if (
                        neighbor_y >= 0 &&
                        neighbor_y < height &&
                        neighbor_x >= 0 &&
                        neighbor_x < width 
                    ) {
                        Pixel *neighbor = &imageIn->pixels[neighbor_y][neighbor_x];
                        red += neighbor->red * threadArgs->boxFilter[fy + 1][fx + 1];
                        green += neighbor->green * threadArgs->boxFilter[fy + 1][fx + 1];
                        blue += neighbor->blue * threadArgs->boxFilter[fy + 1][fx + 1];
                        alpha += neighbor->alpha * threadArgs->boxFilter[fy + 1][fx + 1];
                    }
                }
            }

            Pixel * output_pixel = &imageOut->pixels[y][x];
            output_pixel->red = (uint8_t)(red / factor);
            output_pixel->green = (uint8_t)(green / factor);
            output_pixel->blue = (uint8_t)(blue / factor);
            output_pixel->alpha = (uint8_t)(alpha / factor);
        }
    }

    return NULL;
}