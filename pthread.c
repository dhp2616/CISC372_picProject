#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include "image.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

//An array of kernel matrices to be used for image convolution.  
//The indexes of these match the enumeration from the header file. ie. algorithms[BLUR] returns the kernel corresponding to a box blur.
Matrix algorithms[]={
    {{0,-1,0},{-1,4,-1},{0,-1,0}},
    {{0,-1,0},{-1,5,-1},{0,-1,0}},
    {{1/9.0,1/9.0,1/9.0},{1/9.0,1/9.0,1/9.0},{1/9.0,1/9.0,1/9.0}},
    {{1.0/16,1.0/8,1.0/16},{1.0/8,1.0/4,1.0/8},{1.0/16,1.0/8,1.0/16}},
    {{-2,-1,0},{-1,1,1},{0,1,2}},
    {{0,0,0},{0,1,0},{0,0,0}}
};


//getthreadidelValue - Computes the value of a specific pixel on a specific channel using the selected convolution kernel
//Paramters: srcImage:  An Image struct populated with the image being convoluted
//           x: The x coordinate of the pixel
//          y: The y coordinate of the pixel
//          bit: The color channel being manipulated
//          algorithm: The 3x3 kernel matrix to use for the convolution
//Returns: The new value for this x,y pixel and bit channel
uint8_t getPixelValue(Image* srcImage,int x,int y,int bit,Matrix algorithm){
    int px,mx,py,my,i,span;
    span=srcImage->width*srcImage->bpp;
    // for the edge pixes, just reuse the edge pixel
    px=x+1; py=y+1; mx=x-1; my=y-1;
    if (mx<0) mx=0;
    if (my<0) my=0;
    if (px>=srcImage->width) px=srcImage->width-1;
    if (py>=srcImage->height) py=srcImage->height-1;
    uint8_t result=
        algorithm[0][0]*srcImage->data[Index(mx,my,srcImage->width,bit,srcImage->bpp)]+
        algorithm[0][1]*srcImage->data[Index(x,my,srcImage->width,bit,srcImage->bpp)]+
        algorithm[0][2]*srcImage->data[Index(px,my,srcImage->width,bit,srcImage->bpp)]+
        algorithm[1][0]*srcImage->data[Index(mx,y,srcImage->width,bit,srcImage->bpp)]+
        algorithm[1][1]*srcImage->data[Index(x,y,srcImage->width,bit,srcImage->bpp)]+
        algorithm[1][2]*srcImage->data[Index(px,y,srcImage->width,bit,srcImage->bpp)]+
        algorithm[2][0]*srcImage->data[Index(mx,py,srcImage->width,bit,srcImage->bpp)]+
        algorithm[2][1]*srcImage->data[Index(x,py,srcImage->width,bit,srcImage->bpp)]+
        algorithm[2][2]*srcImage->data[Index(px,py,srcImage->width,bit,srcImage->bpp)];
    return result;
}

// a struct to be used to pass multiple paramters to the helper function pthread_conv
struct helper {
    int row;
    Image *srcImage;
    Image *destImage;
    Matrix algorithm;
};


// pthread_conv is a helper function that uses pthread libs to parallelzie the convolution process. Could've done it within convolute and tied them up together but doens't look as clean. 
// each thread proccesses the pixels in each row from a given image (i.e pic1.jpg). The results will be stored in the corresponding row of the desination image. Finally it frees after its done. 
// takes in pointer helper (struct above). and returns nothing. 
void *pthread_conv(void *helper) {
    struct helper *temp = (struct helper *) helper;
    int pixel, bit;
    for (pixel = 0; pixel < temp->srcImage->width; pixel++) {
        for (bit = 0; bit < temp->srcImage->bpp; bit++) {
            temp->destImage
                ->data[Index(pixel, temp->row, temp->srcImage->width, bit, temp->srcImage->bpp)]
                = getPixelValue(temp->srcImage, pixel, temp->row, bit, temp->algorithm);
        }
    }
    free(temp);
}

//convolute:  Applies a kernel matrix to an image
//Parameters: srcImage: The image being convoluted
//            destImage: A pointer to a  pre-allocated (including space for the pixel array) structure to receive the convoluted image.  It should be the same size as srcImage
//            algorithm: The kernel matrix to use for the convolution
//Returns: Nothing
// changed function to use pthread_conv (helper func) it will first copy the input parameters, then it'll create the thread with the help of pthread_conv and finally free all unique threads) 
void convolute(Image *srcImage, Image *destImage, Matrix algorithm) {
    int row,span,i,j;
    span = srcImage->bpp * srcImage->bpp;
    pthread_t *threadid = (pthread_t *) malloc(sizeof(pthread_t) * srcImage->height);
    for (row = 0; row < srcImage->height; row++) {
        struct helper *temp = (struct helper *) malloc(sizeof(struct helper));
        temp->row = row;
        temp->srcImage = srcImage;
        temp->destImage = destImage;
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 3; j++) {
                temp->algorithm[i][j] = algorithm[i][j];
            }
        }
        pthread_create(threadid + row, NULL, pthread_conv, temp);
    }
    for (row = 0; row < srcImage->height; row++) {
        pthread_join(threadid[row], NULL);
    }
    free(threadid);
}

//Usage: Prints usage information for the program
//Returns: -1
int Usage(){
    printf("Usage: image <filename> <type>\n\twhere type is one of (edge,sharpen,blur,gauss,emboss,identity)\n");
    return -1;
}

//GetKernelType: Converts the string name of a convolution into a value from the KernelTypes enumeration
//Parameters: type: A string representation of the type
//Returns: an appropriate entry from the KernelTypes enumeration, defaults to IDENTITY, which does nothing but copy the image.
enum KernelTypes GetKernelType(char* type){
    if (!strcmp(type,"edge")) return EDGE;
    else if (!strcmp(type,"sharpen")) return SHARPEN;
    else if (!strcmp(type,"blur")) return BLUR;
    else if (!strcmp(type,"gauss")) return GAUSE_BLUR;
    else if (!strcmp(type,"emboss")) return EMBOSS;
    else return IDENTITY;
}

//main:
//argv is expected to take 2 arguments.  First is the source file name (can be jpg, png, bmp, tga).  Second is the lower case name of the algorithm.
int main(int argc,char** argv){
    long t1,t2;
    t1=time(NULL);

    stbi_set_flip_vertically_on_load(0); 
    if (argc!=3) return Usage();
    char* fileName=argv[1];
    if (!strcmp(argv[1],"pic4.jpg")&&!strcmp(argv[2],"gauss")){
        printf("You have applied a gaussian filter to Gauss which has caused a tear in the time-space continum.\n");
    }
    enum KernelTypes type=GetKernelType(argv[2]);

    Image srcImage,destImage,bwImage;   
    srcImage.data=stbi_load(fileName,&srcImage.width,&srcImage.height,&srcImage.bpp,0);
    if (!srcImage.data){
        printf("Error loading file %s.\n",fileName);
        return -1;
    }
    destImage.bpp=srcImage.bpp;
    destImage.height=srcImage.height;
    destImage.width=srcImage.width;
    destImage.data=malloc(sizeof(uint8_t)*destImage.width*destImage.bpp*destImage.height);
    convolute(&srcImage,&destImage,algorithms[type]);
    stbi_write_png("output.png",destImage.width,destImage.height,destImage.bpp,destImage.data,destImage.bpp*destImage.width);
    stbi_image_free(srcImage.data);
    
    free(destImage.data);
    t2=time(NULL);
    printf("Took %ld seconds\n",t2-t1);
   return 0;
}
