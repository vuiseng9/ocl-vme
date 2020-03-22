// Copyright (c) 2009-2011 Intel Corporation
// All rights reserved.
//
// WARRANTY DISCLAIMER
//
// THESE MATERIALS ARE PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR ITS
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THESE
// MATERIALS, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Intel Corporation is the author of the Materials, and requests that all
// problem reports or change requests be submitted to it directly

#include <stdio.h>
#include <stdlib.h>
#ifndef __linux
#include <tchar.h>
#include <windows.h>
#endif
#include <memory.h>

#include <CL/cl.h>
#include <CL/cl_ext_intel.h>
#include "utils.h"
#include <assert.h>


#pragma warning( push )

char *ReadSources(const char *fileName)
{
    FILE *file = fopen(fileName, "rb");
    if (!file)
    {
        printf("ERROR: Failed to open file '%s'\n", fileName);
        return NULL;
    }

    if (fseek(file, 0, SEEK_END))
    {
        printf("ERROR: Failed to seek file '%s'\n", fileName);
        fclose(file);
        return NULL;
    }

    long size = ftell(file);
    if (size == 0)
    {
        printf("ERROR: Failed to check position on file '%s'\n", fileName);
        fclose(file);
        return NULL;
    }

    rewind(file);

    char *src = (char *)malloc(sizeof(char) * size + 1);
    if (!src)
    {
        printf("ERROR: Failed to allocate memory for file '%s'\n", fileName);
        fclose(file);
        return NULL;
    }

    printf("Reading file '%s' (size %ld bytes)\n", fileName, size);
    size_t res = fread(src, 1, sizeof(char) * size, file);
    if (res != sizeof(char) * size)
    {
        printf("ERROR: Failed to read file '%s'\n", fileName);
        fclose(file);
        free(src);
        return NULL;
    }

    src[size] = '\0'; /* NULL terminated */
    fclose(file);

    return src;
}

cl_platform_id GetIntelOCLPlatform()
{
    cl_platform_id pPlatforms[10] = { 0 };
    char pPlatformName[128] = { 0 };

    cl_uint uiPlatformsCount = 0;
    cl_int err = clGetPlatformIDs(10, pPlatforms, &uiPlatformsCount);
    for (cl_uint ui = 0; ui < uiPlatformsCount; ++ui)
    {
        err = clGetPlatformInfo(pPlatforms[ui], CL_PLATFORM_NAME, 128 * sizeof(char), pPlatformName, NULL);
        if ( err != CL_SUCCESS )
        {
            printf("ERROR: Failed to retreive platform vendor name. %d\n", ui);
            return NULL;
        }

        if (!strcmp(pPlatformName, "Intel(R) OpenCL"))
            return pPlatforms[ui];
    }

    return NULL;
}


void BuildFailLog( cl_program program,
                  cl_device_id device_id )
{
    size_t paramValueSizeRet = 0;
    clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &paramValueSizeRet);

    char* buildLogMsgBuf = (char *)malloc(sizeof(char) * paramValueSizeRet + 1);
    if( buildLogMsgBuf )
    {
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, paramValueSizeRet, buildLogMsgBuf, &paramValueSizeRet);
        buildLogMsgBuf[paramValueSizeRet] = '\0';    // mark end of message string

        printf("Build Log:\n");
        puts(buildLogMsgBuf);
        fflush(stdout);

        free(buildLogMsgBuf);
    }
}
#ifndef __linux__
bool SaveImageAsBMP ( unsigned int* ptr, int width, int height, const char* fileName)
{
    FILE* stream = NULL;
    int* ppix = (int*)ptr;
    printf("Save Image: %s \n", fileName);
    stream = fopen( fileName, "wb" );

    if( NULL == stream )
        return false;

    BITMAPFILEHEADER fileHeader;
    BITMAPINFOHEADER infoHeader;

    int alignSize  = width * 4;
    alignSize ^= 0x03;
    alignSize ++;
    alignSize &= 0x03;

    int rowLength = width * 4 + alignSize;

    fileHeader.bfReserved1  = 0x0000;
    fileHeader.bfReserved2  = 0x0000;

    infoHeader.biSize          = sizeof(BITMAPINFOHEADER);
    infoHeader.biWidth         = width;
    infoHeader.biHeight        = height;
    infoHeader.biPlanes        = 1;
    infoHeader.biBitCount      = 32;
    infoHeader.biCompression   = BI_RGB;
    infoHeader.biSizeImage     = rowLength * height;
    infoHeader.biXPelsPerMeter = 0;
    infoHeader.biYPelsPerMeter = 0;
    infoHeader.biClrUsed       = 0; // max available
    infoHeader.biClrImportant  = 0; // !!!
    fileHeader.bfType       = 0x4D42;
    fileHeader.bfSize       = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + rowLength * height;
    fileHeader.bfOffBits    = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    if( sizeof(BITMAPFILEHEADER) != fwrite( &fileHeader, 1, sizeof(BITMAPFILEHEADER), stream ) ) {
        // cann't write BITMAPFILEHEADER
        goto ErrExit;
    }

    if( sizeof(BITMAPINFOHEADER) != fwrite( &infoHeader, 1, sizeof(BITMAPINFOHEADER), stream ) ) {
        // cann't write BITMAPINFOHEADER
        goto ErrExit;
    }

    unsigned char buffer[4];
    int x, y;

    for (y=0; y<height; y++)
    {
        for (x=0; x<width; x++, ppix++)
        {
            if( 4 != fwrite(ppix, 1, 4, stream)) {
                goto ErrExit;
            }
        }
        memset( buffer, 0x00, 4 );

        fwrite( buffer, 1, alignSize, stream );
    }

    fclose( stream );
    return true;
ErrExit:
    fclose( stream );
    return false;
}

// this function convert float RGBA data into uchar RGBA data and save it into BMP file as image
bool SaveImageAsBMP_32FC4(cl_float* p_buf, cl_float scale, cl_uint array_width, cl_uint array_height, const char* p_file_name)
{
    // save results in bitmap files
    float fTmpFVal = 0.0f;
    cl_uint* outUIntBuf = (cl_uint*)malloc(array_width*array_height*sizeof(cl_uint));
    if(!outUIntBuf)
    {
        printf("Failed to allocate memory for output BMP image!\n");
        return false;
    }

    for(cl_uint y = 0; y < array_height; y++)
    {
        for(cl_uint x = 0; x < array_width; x++)
        {
            // Ensure that no value is greater than 255.0
            cl_uint uiTmp[4];
            fTmpFVal = (scale*p_buf[(y*array_width+x)*4+0]);
            if(fTmpFVal>255.0f)
                fTmpFVal=255.0f;
            uiTmp[0] = (cl_uint)(fTmpFVal);

            fTmpFVal = (scale*p_buf[(y*array_width+x)*4+1]);
            if(fTmpFVal>255.0f)
                fTmpFVal=255.0f;
            uiTmp[1] = (cl_uint)(fTmpFVal);

            fTmpFVal = (scale*p_buf[(y*array_width+x)*4+2]);
            if(fTmpFVal>255.0f)
                fTmpFVal=255.0f;
            uiTmp[2] = (cl_uint)(fTmpFVal);

            fTmpFVal = (scale*p_buf[(y*array_width+x)*4+3]);
            if(fTmpFVal>255.0f)
                fTmpFVal=255.0f;
            uiTmp[3] = 1;    //Alfa

            outUIntBuf[(array_height-1-y)*array_width+x] = 0x000000FF & uiTmp[2];
            outUIntBuf[(array_height-1-y)*array_width+x] |= 0x0000FF00 & ((uiTmp[1]) << 8);
            outUIntBuf[(array_height-1-y)*array_width+x] |= 0x00FF0000 & ((uiTmp[0]) << 16);
            outUIntBuf[(array_height-1-y)*array_width+x] |= 0xFF000000 & ((uiTmp[3]) << 24);
        }
    }
    //----
    bool res = SaveImageAsBMP( outUIntBuf, array_width, array_height, p_file_name);
    free(outUIntBuf);
    return res;
}
#endif

cl_kernel createKernelFromString(cl_context* context,
                                 OCL_DeviceAndQueue* cl_devandqueue,
                                 const char* codeString,
                                 const char* kernelName,
                                 const char* options,
                                 cl_program* programOut,
                                 cl_int* err)
{
    cl_program program;
    cl_kernel kernel;

    const char* strings[] = {codeString};

    OCL_ABORT_ON_ERR((
        program = clCreateProgramWithSource(    *context,
        1,
        (const char **) strings,
        NULL,
        err),
        *err
        ));


    // build program
    *err = clBuildProgram(program, 1, &cl_devandqueue->mID, options, NULL, NULL);
    {
        char  *build_info;
        size_t build_info_size=0;

        // get build log
        OCL_ABORT_ON_ERR(
            clGetProgramBuildInfo(program,
                cl_devandqueue->mID,
                CL_PROGRAM_BUILD_LOG,
                0,
                NULL,
                &build_info_size)
            );

        // print build log
        if(build_info_size>0)
        {
            build_info = new char[build_info_size];
            OCL_ABORT_ON_ERR(
                clGetProgramBuildInfo(program,
                    cl_devandqueue->mID,
                    CL_PROGRAM_BUILD_LOG,
                    build_info_size,
                    build_info,
                    NULL)
                );
            printf("Build log:\n%s\n", build_info);
            delete[] build_info;
        }
    }

    *programOut = program;

    if(*err!=CL_SUCCESS)
    {
        return NULL;
    }

    // create kernel
    OCL_ABORT_ON_ERR(( kernel = clCreateKernel (program, kernelName, err), *err ));

    return kernel;
}

cl_kernel createKernelFromFile(cl_context* context,
                               OCL_DeviceAndQueue* cl_devandqueue,
                               const char* fileName,
                               const char* kernelName,
                               const char* options,
                               cl_program* programOut,
                               cl_int* err)
{
    FILE* pFile = fopen( fileName, "rb" );
    if(!pFile)
    {
        printf("Error opening file: %s\n",fileName);
        abort();
    }

    // get size of file, in chars
    fseek( pFile, 0, SEEK_END );
    long fileLen = ftell( pFile );

    char* codeString = (char*)malloc(fileLen+1); // +1 for "\0" end of a string
    if(!codeString)
    {
        printf("Error allocating buffer\n");
        fclose(pFile);
        abort();
    }

    // read into string
    rewind(pFile);
    fread( codeString, 1, fileLen, pFile );

    codeString[fileLen] = '\0';

    fclose(pFile);

    cl_kernel tmpKernel = createKernelFromString( context, cl_devandqueue, codeString, kernelName, options, programOut, err );
    free(codeString);

    return tmpKernel;
}

// return random number of any size
#define RAND_FLOAT(max) max*2.0f*((float)rand() / (float)RAND_MAX) - max
void rand_clfloatn(void* out, size_t type_size,float max)
{
    cl_types val;
    switch(type_size)
    {
    case(sizeof(cl_float)):
        val.f_val = RAND_FLOAT(max) ;
        break;
    case(sizeof(cl_float2)):
        for(UINT i=0; i<2; i++)
            val.f2_val.s[i] = RAND_FLOAT(max) ;
        break;
    case(sizeof(cl_float4)):
        for(UINT i=0; i<4; i++)
            val.f4_val.s[i] = RAND_FLOAT(max) ;
        break;
    case(sizeof(cl_float8)):
        for(UINT i=0; i<8; i++)
            val.f8_val.s[i] = RAND_FLOAT(max) ;
        break;
    case(sizeof(cl_float16)):
        for(UINT i=0; i<16; i++)
            val.f16_val.s[i] = RAND_FLOAT(max) ;
        break;
    default:
        break;
    }

    memcpy(out,&val,type_size);
}

// return random number of any size
void line_clfloatn(void* out, float frand, size_t type_size)
{
    cl_types val;
    switch(type_size)
    {
    case(sizeof(cl_float)):
        val.f_val = frand;
        break;
    case(sizeof(cl_float2)):
        for(UINT i=0; i<2; i++)
            val.f2_val.s[i] = frand;
        break;
    case(sizeof(cl_float4)):
        for(UINT i=0; i<4; i++)
            val.f4_val.s[i] = frand;
        break;
    case(sizeof(cl_float8)):
        for(UINT i=0; i<8; i++)
            val.f8_val.s[i] = frand;
        break;
    case(sizeof(cl_float16)):
        for(UINT i=0; i<16; i++)
            val.f16_val.s[i] = frand;
        break;
    default:
        break;
    }

    memcpy(out,&val,type_size);
}

cl_mem createRandomFloatVecBuffer(cl_context* context,
                                  cl_mem_flags flags,
                                  size_t atomic_size,
                                  cl_uint num,
                                  cl_int *errcode_ret,
                                  float randmax )
{

    // fill input buffer with random values
    BYTE* randTmp;
    BYTE* randomInput = (BYTE*)malloc(atomic_size*num);
    for(UINT i=0; i<num; i++)
    {
        randTmp = randomInput + i*atomic_size;
        rand_clfloatn(randTmp,atomic_size,randmax);
    }

    // create input/output buffers
    cl_mem outBuff;
    outBuff = clCreateBuffer(*context,
        CL_MEM_COPY_HOST_PTR | flags,
        num*atomic_size,
        randomInput,
        errcode_ret);

    free(randomInput);

    return outBuff;
}



cl_int fillRandomFloatVecBuffer(cl_command_queue* cmdqueue,
                                cl_mem* buffer,
                                size_t atomic_size,
                                cl_uint num,
                                cl_event *ev,
                                float randmax)
{

    // fill input buffer with random values
    BYTE* randTmp;
    BYTE* randomInput = (BYTE*)malloc(atomic_size*num);
    for(UINT i=0; i<num; i++)
    {
        randTmp = randomInput + i*atomic_size;
        rand_clfloatn(randTmp,atomic_size,randmax);
    }

    // create input/output buffers
    cl_int err = clEnqueueWriteBuffer(*cmdqueue,
        *buffer,
        1,
        0,
        num*atomic_size,
        randomInput,
        0,
        NULL,
        ev);

    free(randomInput);

    return err;
}



#pragma warning( pop )
