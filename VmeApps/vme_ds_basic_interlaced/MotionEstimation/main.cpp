// Copyright (c) 2009-2013 Intel Corporation
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

#define __CL_ENABLE_EXCEPTIONS

#define BUILD_GOLD_RESULTS  (0)

#include <iostream>
#include <vector>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <CL/cl.hpp>
#include <CL/cl_ext.h>

#include "yuv_utils.h"
#include "cmdparser.hpp"
#include "oclobject.hpp"

#ifdef __linux
void fopen_s(FILE **f, const char *name, const char *mode) {
  assert(f);
  *f = fopen(name, mode);
}
#endif

#define DIV(X,SIZE) (((X + (SIZE - 1))/ SIZE ))
#define PAD(X,SIZE) (DIV(X,SIZE) * SIZE)

using namespace YUVUtils;

// these values define dimensions of input pixel blocks (whcih are fixed in hardware)
// so, do not change these values to avoid errors
#define SRC_BLOCK_WIDTH 16
#define SRC_BLOCK_HEIGHT 16

typedef cl_short2 MotionVector;

static const cl_uint kMBBlockType = CL_ME_MB_TYPE_4x4_INTEL;

#ifdef _MSC_VER
#pragma warning (push)
#pragma warning (disable : 4355)    // 'this': used in base member initializer list
#endif

const char * GetErrorType(int err){
    switch (err)
    {
    case CL_SUCCESS:                                         return"CL_SUCCESS";
    case CL_DEVICE_NOT_FOUND:                                return"CL_DEVICE_NOT_FOUND";
    case CL_DEVICE_NOT_AVAILABLE:                            return"CL_DEVICE_NOT_AVAILABLE";
    case CL_COMPILER_NOT_AVAILABLE:                          return"CL_COMPILER_NOT_AVAILABLE";
    case CL_MEM_OBJECT_ALLOCATION_FAILURE:                   return"CL_MEM_OBJECT_ALLOCATION_FAILURE";
    case CL_OUT_OF_RESOURCES:                                return"CL_OUT_OF_RESOURCES";
    case CL_OUT_OF_HOST_MEMORY:                              return"CL_OUT_OF_HOST_MEMORY";
    case CL_PROFILING_INFO_NOT_AVAILABLE:                    return"CL_PROFILING_INFO_NOT_AVAILABLE";
    case CL_MEM_COPY_OVERLAP:                                return"CL_MEM_COPY_OVERLAP";
    case CL_IMAGE_FORMAT_MISMATCH:                           return"CL_IMAGE_FORMAT_MISMATCH";
    case CL_IMAGE_FORMAT_NOT_SUPPORTED:                      return"CL_IMAGE_FORMAT_NOT_SUPPORTED";
    case CL_BUILD_PROGRAM_FAILURE:                           return"CL_BUILD_PROGRAM_FAILURE";
    case CL_MAP_FAILURE:                                     return"CL_MAP_FAILURE";
    case CL_MISALIGNED_SUB_BUFFER_OFFSET:                    return"CL_MISALIGNED_SUB_BUFFER_OFFSET";
    case CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST:       return"CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
    case CL_COMPILE_PROGRAM_FAILURE:                         return"CL_COMPILE_PROGRAM_FAILURE";
    case CL_LINKER_NOT_AVAILABLE:                            return"CL_LINKER_NOT_AVAILABLE";
    case CL_LINK_PROGRAM_FAILURE:                            return"CL_LINK_PROGRAM_FAILURE";
    case CL_DEVICE_PARTITION_FAILED:                         return"CL_DEVICE_PARTITION_FAILED";
    case CL_KERNEL_ARG_INFO_NOT_AVAILABLE:                   return"CL_KERNEL_ARG_INFO_NOT_AVAILABLE";
    case CL_INVALID_VALUE:                                   return"CL_INVALID_VALUE";
    case CL_INVALID_DEVICE_TYPE:                             return"CL_INVALID_DEVICE_TYPE";
    case CL_INVALID_PLATFORM:                                return"CL_INVALID_PLATFORM";
    case CL_INVALID_DEVICE:                                  return"CL_INVALID_DEVICE";
    case CL_INVALID_CONTEXT:                                 return"CL_INVALID_CONTEXT";
    case CL_INVALID_QUEUE_PROPERTIES:                        return"CL_INVALID_QUEUE_PROPERTIES";
    case CL_INVALID_COMMAND_QUEUE:                           return"CL_INVALID_COMMAND_QUEUE";
    case CL_INVALID_HOST_PTR:                                return"CL_INVALID_HOST_PTR";
    case CL_INVALID_MEM_OBJECT:                              return"CL_INVALID_MEM_OBJECT";
    case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:                 return"CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
    case CL_INVALID_IMAGE_SIZE:                              return"CL_INVALID_IMAGE_SIZE";
    case CL_INVALID_SAMPLER:                                 return"CL_INVALID_SAMPLER";
    case CL_INVALID_BINARY:                                  return"CL_INVALID_BINARY";
    case CL_INVALID_BUILD_OPTIONS:                           return"CL_INVALID_BUILD_OPTIONS";
    case CL_INVALID_PROGRAM:                                 return"CL_INVALID_PROGRAM";
    case CL_INVALID_PROGRAM_EXECUTABLE:                      return"CL_INVALID_PROGRAM_EXECUTABLE";
    case CL_INVALID_KERNEL_NAME:                             return"CL_INVALID_KERNEL_NAME";
    case CL_INVALID_KERNEL_DEFINITION:                       return"CL_INVALID_KERNEL_DEFINITION";
    case CL_INVALID_KERNEL:                                  return"CL_INVALID_KERNEL";
    case CL_INVALID_ARG_INDEX:                               return"CL_INVALID_ARG_INDEX";
    case CL_INVALID_ARG_VALUE:                               return"CL_INVALID_ARG_VALUE";
    case CL_INVALID_ARG_SIZE:                                return"CL_INVALID_ARG_SIZE";
    case CL_INVALID_KERNEL_ARGS:                             return"CL_INVALID_KERNEL_ARGS";
    case CL_INVALID_WORK_DIMENSION:                          return"CL_INVALID_WORK_DIMENSION";
    case CL_INVALID_WORK_GROUP_SIZE:                         return"CL_INVALID_WORK_GROUP_SIZE";
    case CL_INVALID_WORK_ITEM_SIZE:                          return"CL_INVALID_WORK_ITEM_SIZE";
    case CL_INVALID_GLOBAL_OFFSET:                           return"CL_INVALID_GLOBAL_OFFSET";
    case CL_INVALID_EVENT_WAIT_LIST:                         return"CL_INVALID_EVENT_WAIT_LIST";
    case CL_INVALID_EVENT:                                   return"CL_INVALID_EVENT";
    case CL_INVALID_OPERATION:                               return"CL_INVALID_OPERATION";
    case CL_INVALID_GL_OBJECT:                               return"CL_INVALID_GL_OBJECT";
    case CL_INVALID_BUFFER_SIZE:                             return"CL_INVALID_BUFFER_SIZE";
    case CL_INVALID_MIP_LEVEL:                               return"CL_INVALID_MIP_LEVEL";
    case CL_INVALID_GLOBAL_WORK_SIZE:                        return"CL_INVALID_GLOBAL_WORK_SIZE";
    case CL_INVALID_PROPERTY:                                return"CL_INVALID_PROPERTY";
    case CL_INVALID_IMAGE_DESCRIPTOR:                        return"CL_INVALID_IMAGE_DESCRIPTOR";
    case CL_INVALID_COMPILER_OPTIONS:                        return"CL_INVALID_COMPILER_OPTIONS";
    case CL_INVALID_LINKER_OPTIONS:                          return"CL_INVALID_LINKER_OPTIONS";
    case CL_INVALID_DEVICE_PARTITION_COUNT:                  return"CL_INVALID_DEVICE_PARTITION_COUNT";
    case CL_INVALID_PIPE_SIZE:                               return"CL_INVALID_PIPE_SIZE";
    case CL_INVALID_DEVICE_QUEUE:                            return"CL_INVALID_DEVICE_QUEUE";
    default:                                                 return NULL;
    }
}
bool LoadSourceFromFile(
    const char* filename,
    char* & sourceCode )
{
    bool error = false;
    FILE* fp = NULL;
    int nsize = 0;
    
    // Open the shader file

    fopen_s( &fp, filename, "rb" );
    if( !fp )
    {
        error = true;
    }
    else
    {
        // Allocate a buffer for the file contents
        fseek( fp, 0, SEEK_END );
        nsize = ftell( fp );
        fseek( fp, 0, SEEK_SET );

        sourceCode = new char [ nsize + 1 ];
        if( sourceCode )
        {
            fread( sourceCode, 1, nsize, fp );
            sourceCode[ nsize ] = 0; // Don't forget the NULL terminator
        }
        else
        {
            error = true;
        }

        fclose( fp );
    }
    
    return error;
}


// All command-line options for the sample
class CmdParserMV : public CmdParser
{
public:
    CmdOption<bool>        out_to_bmp;
    CmdOption<bool>     help;
    CmdOption<std::string>         fileName;
    CmdOption<std::string>         overlayFileName;
    CmdOption<int>      width;
    CmdOption<int>      height;
    CmdOption<int>      frames;
    
    CmdParserMV  (int argc, const char** argv) :
    CmdParser(argc, argv),
        out_to_bmp(*this,        'b',"nobmp","","Do not output frames to the sequence of bmp files (in addition to the yuv file), by default the output is on", ""),
        help(*this,              'h',"help","","Show this help text and exit."),
        fileName(*this,          0,"input", "string", "Input video sequence filename (.yuv file format)","in_1280x720.yuv"),
        overlayFileName(*this,   0,"output","string", "Output video sequence with overlaid motion vectors filename ","out.yuv"),
        width(*this,             0, "width",    "<integer>", "Frame width for the input file", 1280),
        height(*this,            0, "height","<integer>", "Frame height for the input file",720),
        frames(*this,            0, "frames", "<integer>", "Number of frame to use for motion estimation -- 0 represents entire video", 30)
    {
    }
    virtual void parse ()
    {
        CmdParser::parse();
        if(help.isSet())
        {
            printUsage(std::cout);
        }
    }
};
#ifdef _MSC_VER
#pragma warning (pop)
#endif

inline void ComputeNumMVs( cl_uint nMBType, int nPicWidth, int nPicHeight, 
                           int & nMVSurfWidth, int & nMVSurfHeight,
                           int & nMBSurfWidth, int & nMBSurfHeight )
{
    // Size of the input frame in pixel blocks (SRC_BLOCK_WIDTH x SRC_BLOCK_HEIGHT each)
    int nPicWidthInBlk  = (nPicWidth + SRC_BLOCK_WIDTH - 1) / SRC_BLOCK_WIDTH;
    int nPicHeightInBlk = (nPicHeight + SRC_BLOCK_HEIGHT - 1) / SRC_BLOCK_HEIGHT;

    nMBSurfWidth = nPicWidthInBlk;
    nMBSurfHeight = nPicHeightInBlk;


    if (CL_ME_MB_TYPE_4x4_INTEL == nMBType) {         // Each Src block has 4x4 MVs
        nMVSurfWidth = nPicWidthInBlk * 4;
        nMVSurfHeight = nPicHeightInBlk * 4;
    }
    else if (CL_ME_MB_TYPE_8x8_INTEL == nMBType) {    // Each Src block has 2x2 MVs
        nMVSurfWidth = nPicWidthInBlk * 2;
        nMVSurfHeight = nPicHeightInBlk * 2;
    }
    else if (CL_ME_MB_TYPE_16x16_INTEL == nMBType) {  // Each Src block has 1 MV
        nMVSurfWidth = nPicWidthInBlk;
        nMVSurfHeight = nPicHeightInBlk;
    }
    else
    {
        throw std::runtime_error("Unknown macroblock type");
    }
}

inline unsigned int ComputeSubBlockSize( cl_uint nMBType )
{
    switch (nMBType)
    {
    case CL_ME_MB_TYPE_4x4_INTEL: return 4;
    case CL_ME_MB_TYPE_8x8_INTEL: return 8;
    case CL_ME_MB_TYPE_16x16_INTEL: return 16;
    default:
        throw std::runtime_error("Unknown macroblock type");
    }
}

void PrintInterDists( std::vector<cl_ushort>& interDists, cl_uchar polarity, int width, int height )
{
	std::ofstream file;
    const char *name = ( polarity == 0 ) ? "top_inter_dists.txt" : "bot_inter_dists.txt";
    file.open(name);
    unsigned numMBs = DIV(width, 16) * DIV(height, 16);
    for (unsigned i = 0; i < interDists.size() / 16; i++)
    {
        unsigned pic = i / numMBs;
        unsigned mb = i % numMBs;
        unsigned mb_x = mb % DIV(width, 16);
        unsigned mb_y = mb / DIV(width, 16);
        
        file << "pic=" << pic << " mb=(" << mb_x << "," << mb_y << "): ";

        for (unsigned j = 0; j < 15; j++) {
            file << unsigned(interDists[i * 16 + j]) << ", ";
        }
        file << unsigned(interDists[i * 16 + 15]);

        file << std::endl;
    }
    file.close();
}

void ExtractMotionVectorsFullFrameWithOpenCL( 
    Capture * pCapture, 
    cl_uchar polarity,
    std::vector<MotionVector> & MVs, 
    std::vector<cl_ushort> & SADs, 
    std::vector<cl_uchar2> & Shapes, 
    const CmdParserMV& cmd)
{

    // OpenCL initialization
    OpenCLBasic init("Intel", "GPU");
    
    cl::Context context = cl::Context(init.context); clRetainContext(init.context);
    cl::Device device  = cl::Device(init.device);   clRetainDevice(init.device);
    cl::CommandQueue queue = cl::CommandQueue(init.queue);clRetainCommandQueue(init.queue);

    std::string ext = device.getInfo< CL_DEVICE_EXTENSIONS >();
    if (string::npos == ext.find("cl_intel_device_side_avc_motion_estimation"))
    {
        printf("WARNING: The selected device doesn't officially support device-side motion estimation!\n");
    }

    char* programSource = NULL;
    if( LoadSourceFromFile( "vme_basic.cl", programSource ) )
    {
        printf("Error: Couldn't load kernel source from file.\n" );
    }    

    // Create a built-in VME kernel
    cl_int err = 0;
    const cl_device_id & d = device();    
    cl::Program p(clCreateProgramWithSource(context(),1,( const char** )&programSource,NULL,&err));

    err = clBuildProgram(p(), 1, &d, "", NULL, NULL);

    size_t  buildLogSize = 0;
    clGetProgramBuildInfo(p(), d, CL_PROGRAM_BUILD_LOG,0,NULL, &buildLogSize );

    cl_char*    buildLog = new cl_char[ buildLogSize ];
    if( buildLog )
    {
        clGetProgramBuildInfo(p(), d, CL_PROGRAM_BUILD_LOG, buildLogSize, buildLog, NULL );

        std::cout << ">>> Build Log:\n";
        std::cout << buildLog;;
        std::cout << ">>>End of Build Log\n";
    }

    cl::Kernel kernel(p, "block_motion_estimate_intel");

    const int numPics = pCapture->GetNumFrames();
    const int width = cmd.width.getValue();
    const int height = cmd.height.getValue();
    int mvImageWidth, mvImageHeight;
    int mbImageWidth, mbImageHeight;
    ComputeNumMVs(kMBBlockType, width, height / 2, mvImageWidth, mvImageHeight, mbImageWidth, mbImageHeight);
    MVs.resize(numPics * mvImageWidth * mvImageHeight);
    SADs.resize(numPics * mvImageWidth * mvImageHeight);
    Shapes.resize(numPics * mbImageWidth * mbImageHeight);

    std::cout << "mvImageWidth=" << mvImageWidth << std::endl;
    std::cout << "mvImageHeight=" << mvImageHeight << std::endl;

    // Set up OpenCL surfaces
    cl::ImageFormat imageFormat(CL_R, CL_UNORM_INT8);
#if BUILD_GOLD_RESULTS
    cl::Image2D refImage(context, CL_MEM_READ_ONLY, imageFormat, width, height / 2, 0, 0);
    cl::Image2D srcImage(context, CL_MEM_READ_ONLY, imageFormat, width, height / 2, 0, 0);
#else
    cl::Image2D refImage(context, CL_MEM_READ_ONLY, imageFormat, width, height, 0, 0);
    cl::Image2D srcImage(context, CL_MEM_READ_ONLY, imageFormat, width, height, 0, 0);
#endif
    cl::Buffer mvBuffer(context, CL_MEM_WRITE_ONLY, mvImageWidth * mvImageHeight * sizeof(MotionVector));
    cl::Buffer sad(context, CL_MEM_WRITE_ONLY, mvImageWidth * mvImageHeight * sizeof(cl_ushort));
    cl::Buffer ShapeBuffer(context, CL_MEM_WRITE_ONLY, mvImageWidth * mvImageHeight * sizeof(cl_uchar2));

    cl_short2 *predMem = new cl_short2[ mbImageWidth * mbImageHeight ];
    for( int i = 0; i < mbImageWidth * mbImageHeight; i++ )
    {        
        predMem[ i ].s[ 0 ] = 0;
        predMem[ i ].s[ 1 ] = 0;        
    }

    cl::Buffer predBuffer(
        context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 
        mbImageWidth * mbImageHeight * sizeof(cl_short2), predMem, NULL);

    // Bootstrap video sequence reading
#if BUILD_GOLD_RESULTS
    PlanarImage * currImage = CreatePlanarImage(width, height / 2);
    pCapture->GetSample(0, currImage, true, polarity);
#else
    PlanarImage * currImage = CreatePlanarImage(width, height);
    pCapture->GetSample(0, currImage);
#endif
    cl::size_t<3> origin;
    origin[0] = 0;
    origin[1] = 0;
    origin[2] = 0;
    cl::size_t<3> region;
    region[0] = width;
#if BUILD_GOLD_RESULTS
    region[1] = height / 2;
#else
    region[1] = height;
#endif
    region[2] = 1;
    // Copy to tiled image memory - this copy (and its overhead) is not necessary in a full GPU pipeline
    queue.enqueueWriteImage(srcImage, CL_TRUE, origin, region, currImage->PitchY, 0, currImage->Y);

    // Process all frames
    double ioStat = 0;//file i/o
    double meStat = 0;//motion estimation itself

    double overallStart  = time_stamp();
    // First frame is already in srcImg, so we start with the second frame
    for (int i = 1; i < numPics; i++)
    {
        double ioStart = time_stamp();
        // Load next picture
#if BUILD_GOLD_RESULTS
        pCapture->GetSample(i, currImage, true, polarity);
#else
        pCapture->GetSample(i, currImage);
#endif
        std::swap(refImage, srcImage);
        // Copy to tiled image memory - this copy (and its overhead) is not necessary in a full GPU pipeline
        queue.enqueueWriteImage(srcImage, CL_TRUE, origin, region, currImage->PitchY, 0, currImage->Y);
        ioStat += (time_stamp() - ioStart);

        double meStart = time_stamp();
        // Schedule full-frame motion estimation
        int argIdx = 0;
        kernel.setArg(argIdx++, srcImage);
        kernel.setArg(argIdx++, refImage);        
#if BUILD_GOLD_RESULTS
        cl_uchar interlaced = false;
#else
        cl_uchar interlaced = true;
#endif
        kernel.setArg(argIdx++, interlaced);
        kernel.setArg(argIdx++, polarity);
        kernel.setArg(argIdx++, predBuffer);
        kernel.setArg(argIdx++, mvBuffer);
        kernel.setArg(argIdx++, sad);

        kernel.setArg(argIdx++, ShapeBuffer);
        kernel.setArg(argIdx++, sizeof(cl_int), &mbImageHeight);
        queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(PAD(width,16), 1, 1), cl::NDRange(16, 1, 1));

        queue.finish();
        meStat += (time_stamp() - meStart);

        ioStart = time_stamp();
        // Read back resulting motion vectors (in a sync way)
        void * pMVs = &MVs[i * mvImageWidth * mvImageHeight];
        void * pSADs = &SADs[i * mvImageWidth * mvImageHeight];
        void * pShapes = &Shapes[i * mbImageWidth * mbImageHeight];

        queue.enqueueReadBuffer(mvBuffer,CL_TRUE,0,sizeof(MotionVector) * mvImageWidth * mvImageHeight,pMVs,0,0);
        queue.enqueueReadBuffer(sad,CL_TRUE,0,sizeof(cl_ushort) * mvImageWidth * mvImageHeight,pSADs,0,0);
        queue.enqueueReadBuffer(ShapeBuffer, CL_TRUE, 0, sizeof(cl_uchar2)* mbImageWidth * mbImageHeight, pShapes, 0, 0);

        ioStat += (time_stamp() -ioStart);
    }
    double overallStat  = time_stamp() - overallStart;
    std::cout << std::setiosflags(std::ios_base::fixed) << std::setprecision(3);
    std::cout << "Overall time for " << numPics << " frames " << overallStat << " sec\n" ;
    std::cout << "Average frame file I/O time per frame " << 1000*ioStat/numPics << " ms\n";
    std::cout << "Average Motion Estimation time per frame is " << 1000*meStat/numPics << " ms\n";
    ReleaseImage(currImage);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Overlay routines
//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Draw a pixel on Y picture
typedef uint8_t U8;
void DrawPixel(int x, int y, U8 *pPic, int nPicWidth, int nPicHeight, U8 u8Pixel)
{
    int nPixPos;

    if (x<0 || x>=nPicWidth || y<0 || y>=nPicHeight)
        return;         // Don't draw out of bound pixels
    nPixPos = y * nPicWidth + x;
    *(pPic+nPixPos) = u8Pixel;
}
// Bresenham's line algorithm
void DrawLine(int x0, int y0, int dx, int dy, U8 *pPic, int nPicWidth, int nPicHeight, U8 u8Pixel)
{
    using std::swap;

    int x1 = x0 + dx;
    int y1 = y0 + dy;
    bool bSteep = abs(dy) > abs(dx);
    if (bSteep)
    {
        swap(x0, y0);
        swap(x1, y1);
    }
    if (x0 > x1)
    {
        swap(x0, x1);
        swap(y0, y1);
    }
    int nDeltaX = x1 - x0;
    int nDeltaY = abs(y1 - y0);
    int nError = nDeltaX / 2;
    int nYStep;
    if (y0 < y1)
        nYStep = 1;
    else
        nYStep = -1;

    for (; x0 <= x1; x0++)
    {
        if (bSteep)
            DrawPixel(y0, x0, pPic, nPicWidth, nPicHeight, u8Pixel);
        else
            DrawPixel(x0, y0, pPic, nPicWidth, nPicHeight, u8Pixel);

        nError -= nDeltaY;
        if (nError < 0)
        {
            y0 += nYStep;
            nError += nDeltaX;
        }
    }
}

void OverlayVectors(unsigned int subBlockSize, std::vector<MotionVector>& MVs,
                    std::vector<cl_uchar2>& Shapes, PlanarImage* srcImage,
                    int frame, int width, int height) {
  int mvImageWidth, mvImageHeight;
  int mbImageWidth, mbImageHeight;
  ComputeNumMVs(kMBBlockType, width, height, mvImageWidth, mvImageHeight,
                mbImageWidth, mbImageHeight);
  MotionVector* pMV = &MVs[frame * mvImageWidth * mvImageHeight];
  cl_uchar2* pShapes = &Shapes[frame * mbImageWidth * mbImageHeight];

#define OFF(P) (P + 2) >> 2
  for (int i = 0; i < mbImageHeight; i++) {
    for (int j = 0; j < mbImageWidth; j++) {
      int mbIndex = j + i * mbImageWidth;
      // Selectively Draw motion vectors for different sub block sizes
      int j0 = j * 16; int i0 = i * 16; int m0 = mbIndex * 16;
#define SHOW_BLOCKS 0
      switch (pShapes[mbIndex].s[0]) {
        case 0:
#if SHOW_BLOCKS
            DrawLine(j0, i0, 16, 0, srcImage->Y, width, height, 180);
            DrawLine(j0, i0, 0, 16, srcImage->Y, width, height, 180);
            DrawLine(j0 + 16, i0, 0, 16, srcImage->Y, width, height, 180);
            DrawLine(j0, i0 + 16, 16, 0, srcImage->Y, width, height, 180);
#else
            DrawLine(j0 + 8, i0 + 8, OFF(pMV[m0].s[0]), OFF(pMV[m0].s[1]), srcImage->Y, width, height, 180);
#endif
            break;
        case 1: 
#if SHOW_BLOCKS
            DrawLine(j0, i0, 16, 0, srcImage->Y, width, height, 180);
            DrawLine(j0, i0, 0, 16, srcImage->Y, width, height, 180);
            DrawLine(j0 + 8, i0, 0, 16, srcImage->Y, width, height, 180);
            DrawLine(j0 + 16, i0, 0, 16, srcImage->Y, width, height, 180);
            DrawLine(j0, i0 + 16, 16, 0, srcImage->Y, width, height, 180);
#else
            DrawLine(j0 + 8, i0 + 4,  OFF(pMV[m0].s[0]), OFF(pMV[m0].s[1]), srcImage->Y, width, height, 180);
            DrawLine(j0 + 8, i0 + 12, OFF(pMV[m0 + 8].s[0]), OFF(pMV[m0 + 8].s[1]), srcImage->Y, width, height, 180);
#endif
            break;
        case 2:
#if SHOW_BLOCKS
            DrawLine(j0, i0, 16, 0, srcImage->Y, width, height, 180);
            DrawLine(j0, i0, 0, 16, srcImage->Y, width, height, 180);
            DrawLine(j0, i0 + 8, 16, 0, srcImage->Y, width, height, 180);
            DrawLine(j0 + 16, i0, 0, 16, srcImage->Y, width, height, 180);
            DrawLine(j0, i0 + 16, 16, 0, srcImage->Y, width, height, 180);
#else
            DrawLine(j0 + 4, i0 + 8, OFF(pMV[m0].s[0]), OFF(pMV[m0].s[1]), srcImage->Y, width, height, 180);
            DrawLine(j0 + 12, i0 + 8, OFF(pMV[m0 + 8].s[0]), OFF(pMV[m0 + 8].s[1]), srcImage->Y, width, height, 180);
#endif
            break;
        case 3:
            cl_uchar4 minor_shapes;
            minor_shapes.s[0] = (pShapes[mbIndex].s[1]) & 0x03;
            minor_shapes.s[1] = (pShapes[mbIndex].s[1] >> 2) & 0x03;
            minor_shapes.s[2] = (pShapes[mbIndex].s[1] >> 4) & 0x03;
            minor_shapes.s[3] = (pShapes[mbIndex].s[1] >> 6) & 0x03;
#if SHOW_BLOCKS
            DrawLine(j0, i0, 16, 0, srcImage->Y, width, height, 180);
            DrawLine(j0, i0, 0, 16, srcImage->Y, width, height, 180);
            DrawLine(j0 + 8, i0, 0, 16, srcImage->Y, width, height, 180);
            DrawLine(j0, i0 + 8, 16, 0, srcImage->Y, width, height, 180);
            DrawLine(j0 + 16, i0, 0, 16, srcImage->Y, width, height, 180);
            DrawLine(j0, i0 + 16, 16, 0, srcImage->Y, width, height, 180);
#endif
            for (int m = 0; m < 4; ++m) {
                int mdiv = m / 2;
                int mmod = m % 2;
                switch (minor_shapes.s[m]) {
                case 0:    // 8 x 8
#if !SHOW_BLOCKS
                    DrawLine(j0 + mmod * 8 + 4, i0 + mdiv * 8 + 4, OFF(pMV[m0 + m * 4].s[0]), OFF(pMV[m0 + m * 4].s[1]), srcImage->Y, width, height, 180);
#endif
                    break;
                case 1: // 8 x 4
#if SHOW_BLOCKS
                    DrawLine(j0 + mmod * 8, i0 + mdiv * 8 + 4, 8, 0, srcImage->Y, width, height, 180);
#else
                    for (int n = 0; n < 2; ++n) {
                        DrawLine(j0 + mmod * 8 + 4, i0 + (mdiv * 8 + n * 4 + 2), OFF(pMV[m0 + m * 4 + n * 2].s[0]), OFF(pMV[m0 + m * 4 + n * 2].s[1]), srcImage->Y, width, height, 180);
                    }
#endif
                    break;
                case 2:    // 4 x 8
#if SHOW_BLOCKS
                    DrawLine(j0 + mmod * 8 + 4, i0 + mdiv * 8, 8, 0, srcImage->Y, width, height, 180);
#else                    
                    for (int n = 0; n < 2; ++n) {
                        DrawLine(j0 + (mmod * 8 + n * 4 + 2), i0 + mdiv * 8 + 4, OFF(pMV[m0 + m * 4 + n * 2].s[0]), OFF(pMV[m0 + m * 4 + n * 2].s[1]), srcImage->Y, width, height, 180);
                    }
#endif
                    break;
                case 3: // 4 x 4
#if SHOW_BLOCKS
                    DrawLine(j0 + mmod * 8, i0 + mdiv * 8 + 4, 8, 0, srcImage->Y, width, height, 180);
                    DrawLine(j0 + mmod * 8 + 4, i0 + mdiv * 8, 8, 0, srcImage->Y, width, height, 180);
#else
                    for (int n = 0; n < 4; ++n) {
                        DrawLine(j0 + n * 4 + 2, i0 + m * 4 + 2, OFF(pMV[m0 + m * 4 + n].s[0]), OFF(pMV[m0 + m * 4 + n].s[1]), srcImage->Y, width, height, 180);
                    }
#endif
                    break;
                }
            }
            break;
         }
      }
   }
}
int main( int argc, const char** argv )
{
    try
    {
        CmdParserMV cmd(argc, argv);
        cmd.parse();

        // Immediatly exit if user wanted to see the usage information only.
        if(cmd.help.isSet())
        {
            return 0;
        }

        const int width = cmd.width.getValue();
        const int height = cmd.height.getValue();
        const int frames = cmd.frames.getValue();
        // Open input sequence
        Capture * pCapture = Capture::CreateFileCapture(cmd.fileName.getValue(), width, height, frames);
        if (!pCapture)
        {
            throw std::runtime_error("Failed opening video input sequence...");
        }

        // Process sequence
        std::cout << "Processing " << pCapture->GetNumFrames() << " frames ..." << std::endl;
        std::vector<MotionVector> TopMVs, BotMVs;
        std::vector<cl_ushort> TopSADs, BotSADs;
        std::vector<cl_uchar2> TopShapes, BotShapes;
        ExtractMotionVectorsFullFrameWithOpenCL(pCapture, 0, TopMVs, TopSADs, TopShapes, cmd);
        ExtractMotionVectorsFullFrameWithOpenCL(pCapture, 1, BotMVs, BotSADs, BotShapes, cmd);

        // Generate sequence with overlaid motion vectors
        FrameWriter * pTopWriter = FrameWriter::CreateFrameWriter(width, height / 2, pCapture->GetNumFrames(), cmd.out_to_bmp.getValue());
        FrameWriter * pBotWriter = FrameWriter::CreateFrameWriter(width, height / 2, pCapture->GetNumFrames(), cmd.out_to_bmp.getValue());
        PlanarImage * srcTopFieldImage = CreatePlanarImage(width, height / 2);
        PlanarImage * srcBotFieldImage = CreatePlanarImage(width, height / 2);

        int mvImageWidth, mvImageHeight;
        int mbImageWidth, mbImageHeight;
        ComputeNumMVs(kMBBlockType, width, height / 2, mvImageWidth, mvImageHeight, mbImageWidth, mbImageHeight);
        unsigned int subBlockSize = ComputeSubBlockSize(kMBBlockType);

        for (int k = 0; k < pCapture->GetNumFrames(); k++)
        {
            pCapture->GetSample(k, srcTopFieldImage, true, 0);
            pCapture->GetSample(k, srcBotFieldImage, true, 1);
            // Overlay MVs on Top/Bot Src picture, except the very first one
            if(k>0) {
                OverlayVectors(subBlockSize, TopMVs, TopShapes, srcTopFieldImage, k, width, height / 2);
                OverlayVectors(subBlockSize, BotMVs, BotShapes, srcBotFieldImage, k, width, height / 2);
            }
            pTopWriter->AppendFrame(srcTopFieldImage);
            pBotWriter->AppendFrame(srcBotFieldImage);
        }

        std::string topOverlayFileName = "top_fields_";
        topOverlayFileName += cmd.overlayFileName.getValue();
        std::cout << "Writing " << pCapture->GetNumFrames() << " frames to " << topOverlayFileName << "..." << std::endl;
        pTopWriter->WriteToFile(topOverlayFileName.c_str());

        std::string botOverlayFileName = "bot_fields_";
        botOverlayFileName += cmd.overlayFileName.getValue();
        std::cout << "Writing " << pCapture->GetNumFrames() << " frames to " << botOverlayFileName << "..." << std::endl;
        pBotWriter->WriteToFile(botOverlayFileName.c_str());

        FrameWriter::Release(pTopWriter);
        FrameWriter::Release(pBotWriter);
        Capture::Release(pCapture);
        ReleaseImage(srcTopFieldImage);
        ReleaseImage(srcBotFieldImage);

        PrintInterDists(TopSADs, 0, width, height / 2);
        PrintInterDists(BotSADs, 1, width, height / 2);
    }
    catch (cl::Error & err)
    {
        std::cout << err.what() << "(" << GetErrorType(err.err()) << ")" << std::endl;
        return 1;
    }
    catch (std::exception & err)
    {
        std::cout << err.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cout << "Unknown exception! Exit...";
        return 1;
    }

    std::cout << "Done!" << std::endl;

    return 0;
}
