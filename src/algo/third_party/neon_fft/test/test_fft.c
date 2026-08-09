/*
* Name: fft-test.c
* Description: neon fft test
* Author: Ryuk
* Date: 04/05/2021
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "NE10_dsp.h"

#if __ARM_NEON
#warning neon
#else
#warning c
#endif

#define DEBUG       (1)
#define WAV         (1)
#define FRAME_SIZE  (64)
#define INT_16      (32768)

int main()
{
    char inFileName[512];
    char outFileName[512];
    
    short input[FRAME_SIZE];
    short output[FRAME_SIZE];

    FILE *in;
    FILE *out;

#if WAV
    sprintf(inFileName, "%s", "./sample/fft-test.wav");
    sprintf(outFileName, "%s", "./sample/output.wav");
#else
    sprintf(inFileName, "%s", "../sample/fft-test.pcm");
    sprintf(outFileName, "%s", "../sample/output.pcm");
#endif

    printf("Open %s\n", inFileName);
    in = fopen(inFileName, "rb");
    if(in == NULL)
    {
        perror("Error: ");
        return -1;
    }

    printf("Open %s\n", outFileName);
    out = fopen(outFileName, "wb");
    if(out == NULL)
    {
        perror("Error: ");
        return -1;
    }

#if WAV
    fread(input, 44, sizeof(short), in);
    fwrite(input, 44, sizeof(short), out);   
#endif

    int nfft = FRAME_SIZE;
    float *inData = (float *)calloc(FRAME_SIZE, sizeof(float));
    float *outData = (float *)calloc(FRAME_SIZE + 2, sizeof(float));

    ne10_fft_cfg_float32_t cfg;
    cfg = ne10_fft_alloc_r2c_float32(nfft);

    while(fread(input, FRAME_SIZE, sizeof(short), in))
    {   
        memset(inData, 0, sizeof(inData));
        memset(outData, 0, sizeof(outData));

#if DEBUG
        printf("Before FFT\n");
        for(int i=0; i<FRAME_SIZE; i++)
        {
            printf("%d:%d\t", i, input[i]);
        }
        printf("\n");
#endif       

        for(int i=0; i<FRAME_SIZE; i++)
        {
            inData[i] = (float)(input[i]) / INT_16;
        }
        
#if __ARM_NEON
        ne10_fft_r2c_1d_float32_neon(outData, inData, cfg);
#else
	    ne10_fft_r2c_1d_float32_c(outData, inData, cfg);
#endif

#if DEBUG
        printf("After FFT\n");
        for(int i=0; i<FRAME_SIZE+2; i++)
        {
            printf("%d:%f\t", i, outData[i]);
        }
        printf("\n");
#endif

#if __ARM_NEON
        ne10_fft_c2r_1d_float32_neon(inData, outData, cfg);
#else
	ne10_fft_r2c_1d_float32_c(outData, inData, cfg);
#endif

#if DEBUG
        printf("After IFFT\n");
        for(int i=0; i<FRAME_SIZE; i++)
        {
            output[i] = (short)(inData[i] * INT_16);
            printf("%d:%d\t", i, output[i]);
        }
        printf("\n");
#endif
        fwrite(output, FRAME_SIZE, sizeof(short), out);
    }

    free(inData);
    free(outData);
    ne10_fft_destroy_r2c_float32(cfg);

    fclose(in);
    fclose(out);
    
    printf("Program Finished\n");
    return 0;
}
