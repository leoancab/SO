#include "bmp.h"
#include <stdbool.h>

typedef struct {
    BMP_Image* imageIn;
    BMP_Image* imageOut;
    int start_row;
    int end_row;
    int width;
    int height;
    int boxFilter[3][3];
} ThreadArgs;

void apply(BMP_Image * imageIn, BMP_Image * imageOut);

void applyParallel(BMP_Image * imageIn, BMP_Image * imageOut, int boxFilter[3][3], int numThreads, bool top);

void *filterThreadWorker(void * args);