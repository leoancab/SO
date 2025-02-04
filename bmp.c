#include <stdlib.h>
#include <stdio.h>

#include "bmp.h"
/* USE THIS FUNCTION TO PRINT ERROR MESSAGES
   DO NOT MODIFY THIS FUNCTION
*/
void printError(int error){
  switch(error){
  case ARGUMENT_ERROR:
    printf("Usage:ex5 <source> <destination>\n");
    break;
  case FILE_ERROR:
    printf("Unable to open file!\n");
    break;
  case MEMORY_ERROR:
    printf("Unable to allocate memory!\n");
    break;
  case VALID_ERROR:
    printf("BMP file not valid!\n");
    break;
  default:
    break;
  }
}

/* The input argument is the source file pointer. The function will first construct a BMP_Image image by allocating memory to it.
 * Then the function read the header from source image to the image's header.
 * Compute data size, width, height, and bytes_per_pixel of the image and stores them as image's attributes.
 * Finally, allocate menory for image's data according to the image size.
 * Return image;
*/
BMP_Image* createBMPImage(FILE* fptr) {

  if (fptr == NULL) {
    printf("Error with file pointer on createBMPImage()\n");
    return NULL;
  }

  //Allocate memory for BMP_Image*;
  BMP_Image* image = (BMP_Image*)malloc(sizeof(BMP_Image));

  if (image == NULL) {
    printf("Could not allocate memory for image in createBMPImage()\n");
    return NULL;
  }

  //Read the first 54 bytes of the source into the header
  fread(&(image->header), sizeof(BMP_Header), 1, fptr);
  
  if(!checkBMPValid(&image->header)) {
    printError(VALID_ERROR);
    exit(EXIT_FAILURE);
  }
  

  //Compute data size, width, height, and bytes per pixel;
  // image->bytes_per_pixel = image->header.bits_per_pixel / 8; 
  image->bytes_per_pixel = 4; //Supongo que si no hay canal alpha es porque es 1111111
  image->norm_height = abs(image->header.height_px);

  int row_size = image->header.width_px * image->bytes_per_pixel;
  int data_size = image->norm_height * row_size + 54;
  printf("Image size is = %d\n", data_size);

  //Allocate memory for image data
  image->pixels = (Pixel**)malloc(sizeof(Pixel*) * image->norm_height); //Size of pixel es de 32 bits o 4 bytes

  
  
  if (image->pixels == NULL) {
    printf("Error while allocating memory for pixel pointer to pointers in createBMPImage()\n");
    return NULL;
  }

  for (int i = 0; i < image->norm_height; i++) {
    image->pixels[i] = (Pixel*)malloc(row_size); // Largo en pixeles por 4 (Una fila de pixeles)
    if (image->pixels[i] == NULL) {
      printf("Error while allocating memory for pixel row in createBMPImage()\n");
      freeImage(image);
      return NULL;
    }
  }

  readImageData(fptr, image, row_size);

  if(!checkBMPValid(&image->header)) {
    printError(VALID_ERROR);
    exit(EXIT_FAILURE);
  }

  printf("imageBMPImage() created correctly\n");

  return image;
}

/* The input arguments are the source file pointer, the image data pointer, and the size of image data.
 * The functions reads data from the source into the image data matriz of pixels.
*/
void readImageData(FILE* srcFile, BMP_Image * image, int row_size) {
  if (srcFile == NULL || image == NULL || image->pixels == NULL) {
    printError(ARGUMENT_ERROR);
    return;
  }

  fseek(srcFile, image->header.offset, SEEK_SET);


  if (image->header.height_px < 0) {
    for (int i = 0; i < image->norm_height; i++) {
      fread(image->pixels[i], sizeof(Pixel), image->header.width_px, srcFile);
    }
  } else {
    for (int i = image->norm_height - 1; i >= 0; i--) {
      fread(image->pixels[i], sizeof(Pixel), image->header.width_px, srcFile);
    }
  }
}

/* The input arguments are the pointer of the binary file, and the image data pointer.
 * The functions open the source file and call to CreateBMPImage to load de data image.
*/
void readImage(FILE *srcFile, BMP_Image * dataImage) {
  if (srcFile == NULL) {
    printf("File path is not correct in readImage()\n");
    return;
  }

  BMP_Image * image = createBMPImage(srcFile);

  if (image == NULL) {
    printf("Something happened while tryig to open the file\n");
    return;
  }

  *dataImage = *image;

  free(image);

  printf("Flag1\n");
}

/* The input arguments are the destination file name, and BMP_Image pointer.
 * The function write the header and image data into the destination file.
*/
void writeImage(FILE* destFile, BMP_Image* dataImage) {
  if (destFile == NULL || dataImage == NULL) {
    printf("Invalid destination file or dataImage\n");
    return;
  }

  if (fwrite(&dataImage->header, sizeof(BMP_Header), 1, destFile) != 1) {
    printf("Error while writing image header in file\n");
    return;
  }

  int row_size = dataImage->header.width_px * dataImage->bytes_per_pixel;

  for (int i = 0; i < dataImage->norm_height; i++) {
    if(fwrite(dataImage->pixels[i], row_size, 1, destFile) != 1) {
      printf("Error while writing pixel data dataImage->pixels[%d]\n", i);
      return;
    }
  }
  printf("BMP_Image data written succesfully\n");
}

/* The input argument is the BMP_Image pointer. The function frees memory of the BMP_Image.
*/
void freeImage(BMP_Image* image) {
  if (image == NULL) {
    return;
  }

  if (image->pixels != NULL) {
    for (int i = 0; i < image->norm_height; i++) {
      if (image->pixels[i] != NULL) {
        free(image->pixels[i]);
      }
    }
    free(image->pixels);
  }
  free(image);
}

/* The functions checks if the source image has a valid format.
 * It returns TRUE if the image is valid, and returns FASLE if the image is not valid.
 * DO NOT MODIFY THIS FUNCTION
*/
int checkBMPValid(BMP_Header* header) {
  // Make sure this is a BMP file
  
  if (header->type != 0x4d42) {
    printf("%x\n", header->type);
    return FALSE;
  }
  // Make sure we are getting 32 bits per pixel
  if (header->bits_per_pixel != 32) {
    return FALSE;
  }
  // Make sure there is only one image plane
  if (header->planes != 1) {
    return FALSE;
  }
  // Make sure there is no compression
  if (header->compression != 0) {
    return FALSE;
  }

  
  return TRUE;
}

/* The function prints all information of the BMP_Header.
   DO NOT MODIFY THIS FUNCTION
*/
void printBMPHeader(BMP_Header* header) {
  printf("file type (should be 0x4d42): %x\n", header->type);
  printf("file size: %d\n", header->size);
  printf("offset to image data: %d\n", header->offset);
  printf("header size: %d\n", header->header_size);
  printf("width_px: %d\n", header->width_px);
  printf("height_px: %d\n", header->height_px);
  printf("planes: %d\n", header->planes);
  printf("bits: %d\n", header->bits_per_pixel);
}

/* The function prints information of the BMP_Image.
   DO NOT MODIFY THIS FUNCTION
*/
void printBMPImage(BMP_Image* image) {
  printf("data size is %ld\n", sizeof(image->pixels));
  printf("norm_height size is %d\n", image->norm_height);
  printf("bytes per pixel is %d\n", image->bytes_per_pixel);
}
