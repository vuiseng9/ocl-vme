// Copyright (c) 2009-2016 Intel Corporation
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

#define USE_HD              0
#define USE_SD_1280_720     1
#define USE_SD_720_576      0
#define USE_QCIF            0

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

#define CL_EXT_DECLARE(name) static name##_fn pfn_##name = 0;

#define CL_EXT_INIT_WITH_PLATFORM(platform, name) { \
    pfn_##name = (name##_fn) clGetExtensionFunctionAddressForPlatform(platform, #name); \
    if (! pfn_##name ) \
        { \
        std::cout<<"ERROR: can't get handle to function pointer " <<#name<< ", wrong driver version?\n"; \
        } \
};

#define DIV(X,SIZE) (((X + (SIZE - 1))/ SIZE ))
#define PAD(X,SIZE) (DIV(X,SIZE) * SIZE)

using namespace YUVUtils;

// these values define dimensions of input pixel blocks (whcih are fixed in hardware)
// so, do not change these values to avoid errors
#define SRC_BLOCK_WIDTH 16
#define SRC_BLOCK_HEIGHT 16

typedef cl_short2 MotionVector;

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
    CmdOption<bool>                 out_to_bmp;
    CmdOption<bool>                 help;
    CmdOption<std::string>          fileName;
    CmdOption<std::string>          overlayFileName;
    CmdOption<int>                  width;
    CmdOption<int>                  height;
    CmdOption<int>                  frames;
    
    CmdParserMV  (int argc, const char** argv) :
    CmdParser(argc, argv),
        out_to_bmp(*this,       'b',"nobmp","","Do not output frames to the sequence of bmp files (in addition to the yuv file), by default the output is off", true, "nobmp"),
        help(*this,             'h',"help","","Show this help text and exit."),
#if USE_HD
        fileName(*this,         0,"input", "string", "Input video sequence filename (.yuv file format)","BasketballDrive_1920x1080_30.yuv"),
        overlayFileName(*this,  0,"output","string", "Output video sequence with overlaid motion vectors filename ","BasketballDrive_1920x1080_30_output.yuv"),
        width(*this,            0, "width", "<integer>", "Frame width for the input file", 1920),
        height(*this,           0, "height","<integer>", "Frame height for the input file", 1080),
        frames(*this,           30, "frames", "<integer>", "Number of frame to use for motion estimation -- 0 represents entire video", 30)
#elif USE_SD_1280_720
        fileName(*this,         0,"input", "string", "Input video sequence filename (.yuv file format)","goal_1280x720.yuv"),
        overlayFileName(*this,  0,"output","string", "Output video sequence with overlaid motion vectors filename ","goal_1280x720_output.yuv"),
        width(*this,            0, "width", "<integer>", "Frame width for the input file", 1280),
        height(*this,           0, "height","<integer>", "Frame height for the input file", 720),
        frames(*this,           5, "frames", "<integer>", "Number of frame to use for motion estimation -- 0 represents entire video", 5)
#elif USE_SD_720_576
        fileName(*this,         0,"input", "string", "Input video sequence filename (.yuv file format)","iceage_720_576_491frames.yuv"),
        overlayFileName(*this,  0,"output","string", "Output video sequence with overlaid motion vectors filename ","iceage_720_576_491frames_output.yuv"),
        width(*this,            0, "width", "<integer>", "Frame width for the input file", 720),
        height(*this,           0, "height","<integer>", "Frame height for the input file", 576),
        frames(*this,           50, "frames", "<integer>", "Number of frame to use for motion estimation -- 0 represents entire video", 50)
#else
        fileName(*this,         0,"input", "string", "Input video sequence filename (.yuv file format)","boat_qcif_176x144.yuv"),
        overlayFileName(*this,  0,"output","string", "Output video sequence with overlaid motion vectors filename ","boat_qcif_176x144_output.yuv"),
        width(*this,            0, "width", "<integer>", "Frame width for the input file", 176),
        height(*this,           0, "height","<integer>", "Frame height for the input file", 144),
        frames(*this,           100, "frames", "<integer>", "Number of frame to use for motion estimation -- 0 represents entire video", 100)
#endif        
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
    int nPicWidthInBlk  = DIV(nPicWidth, SRC_BLOCK_WIDTH);
    int nPicHeightInBlk = DIV(nPicHeight, SRC_BLOCK_HEIGHT);

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

void get_raster_launch(cl_short *launch, int width, int height)
{
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            launch[y*width * 2 + 2 * x] = x;
            launch[y*width * 2 + 2 * x + 1] = y;
            //printf("%d %d %d\n",launch[id*2],launch[id*2+1],id);
        }
    }
}

void get_45_launch(cl_short *launch, int width, int height)
{
    int stx = 0, sty = 0, id = 0;
    for (int i = 0; i < width + height - 1; i++)
    {
        for (int x = stx, y = sty; x >= 0 && y < height; x--, y++)
        {
            launch[id * 2] = x;
            launch[id * 2 + 1] = y;
            //printf("%d %d %d\n",launch[id*2],launch[id*2+1],id);
            id++;
        }
        if (stx < (width - 1)) stx++;
        else sty++;
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

void ExtractMotionVectorsFullFrameWithOpenCL( 
    Capture * pCapture, std::vector<MotionVector> & MVs, std::vector<cl_ushort> & Residuals, std::vector<cl_uchar2> & Shapes, const CmdParserMV& cmd)
{

    // OpenCL initialization
    OpenCLBasic init("Intel", "GPU", "0", CL_QUEUE_PROFILING_ENABLE);
    //OpenCLBasic creates the platform/context and device for us, so all we need is to get an ownership (via incrementing ref counters with clRetainXXX)

    cl::Context context = cl::Context(init.context); clRetainContext(init.context);
    cl::Device device  = cl::Device(init.device);   clRetainDevice(init.device);
    cl::CommandQueue queue = cl::CommandQueue(init.queue);clRetainCommandQueue(init.queue);

    std::string ext = device.getInfo< CL_DEVICE_EXTENSIONS >();

    if (string::npos == ext.find("cl_intel_device_side_avc_motion_estimation"))
    {
        printf("WARNING: The selected device doesn't officially support motion estimation or accelerator extensions!");
    }

    char* programSource = NULL;
    if( LoadSourceFromFile( "vme_scoreboard.cl", programSource ) )
    // Load the kernel source from the passed in file.
    {
        printf("Error: Couldn't load kernel source from file.\n" );
    }    

    // Create a built-in VME kernel
    cl_int err = 0;
    const cl_device_id & d = device();    
    cl::Program p(clCreateProgramWithSource(context(),1,( const char** )&programSource,NULL,&err));

    err = clBuildProgram(p(), 1, &d,"-cl-std=CL2.0", NULL, NULL );

    if (err != CL_SUCCESS) {
        size_t  buildLogSize = 0;
        clGetProgramBuildInfo(p(), d, CL_PROGRAM_BUILD_LOG, 0, NULL, &buildLogSize);
        cl_char*    buildLog = new cl_char[buildLogSize];
        if( buildLog ) {
            clGetProgramBuildInfo(p(),d,CL_PROGRAM_BUILD_LOG,buildLogSize,buildLog,NULL );
            std::cout << ">>> Build Log:\n" << buildLog << ">>>End of Build Log\n";
        }
        
    }

    cl::Kernel initialize(p, "initialize_scoreboard");
    cl::Kernel kernel(p, "block_motion_estimate_intel");

    const int numPics = pCapture->GetNumFrames();
    const int width = cmd.width.getValue();
    const int height = cmd.height.getValue();
    int mvImageWidth, mvImageHeight;
    int mbImageWidth, mbImageHeight;
    ComputeNumMVs(CL_ME_MB_TYPE_4x4_INTEL, width, height, mvImageWidth, mvImageHeight, mbImageWidth, mbImageHeight);
    std::vector<cl_int> Scoreboard;
    Scoreboard.resize(mbImageWidth * mbImageHeight);

    MVs.resize(numPics * mvImageWidth * mvImageHeight);
    Residuals.resize(numPics * mvImageWidth * mvImageHeight);
    Shapes.resize(numPics * mbImageWidth * mbImageHeight);    

    // Set up OpenCL surfaces
    cl::ImageFormat imageFormat(CL_R, CL_UNORM_INT8);
    cl::Image2D refImage(context, CL_MEM_READ_ONLY, imageFormat, width, height, 0, 0);
    cl::Image2D srcImage(context, CL_MEM_READ_ONLY, imageFormat, width, height, 0, 0);
    cl::Buffer mvBuffer(context, CL_MEM_WRITE_ONLY, mvImageWidth * mvImageHeight * sizeof(MotionVector));
    cl::Buffer residualBuffer(context, CL_MEM_WRITE_ONLY, mvImageWidth * mvImageHeight * sizeof(cl_ushort));
    cl::Buffer shapeBuffer(context, CL_MEM_WRITE_ONLY, mvImageWidth * mvImageHeight * sizeof(cl_uchar2));

    cl_short2 *predMem = new cl_short2[ mbImageWidth * mbImageHeight ];
    for( int i = 0; i < mbImageWidth * mbImageHeight; i++ )
    {       
        predMem[ i ].s[ 0 ] = 0;
        predMem[ i ].s[ 1 ] = 0;        
    }
    cl::Buffer predBuffer(
        context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 
        mbImageWidth * mbImageHeight * sizeof(cl_short2), predMem, NULL);

    cl_short *launchMem = new cl_short[ mbImageWidth * mbImageHeight * 2 ];
    get_45_launch( launchMem, mbImageWidth, mbImageHeight );
    //get_raster_launch( launchMem, mbImageWidth, mbImageHeight );

    cl::Buffer launchBuffer(
        context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
        mbImageWidth * mbImageHeight * sizeof(cl_short2), launchMem, NULL);

    // Bootstrap video sequence reading
    PlanarImage * currImage = CreatePlanarImage(width, height);
    pCapture->GetSample(0, currImage);
    cl::size_t<3> origin;
    origin[0] = 0;
    origin[1] = 0;
    origin[2] = 0;
    cl::size_t<3> region;
    region[0] = width;
    region[1] = height;
    region[2] = 1;
    // Copy to tiled image memory - this copy (and its overhead) is not necessary in a full GPU pipeline
    queue.enqueueWriteImage(srcImage, CL_TRUE, origin, region, currImage->PitchY, 0, currImage->Y);

    // First frame is already in srcImg, so we start with the second frame
    double time = 0;
    vector<double> tpf(numPics);

    for (int i = 1; i < numPics; i++)
    {
        // Load next picture
        pCapture->GetSample(i, currImage);

        std::swap(refImage, srcImage);
        // Copy to tiled image memory - this copy (and its overhead) is not necessary in a full GPU pipeline
        queue.enqueueWriteImage(srcImage, CL_TRUE, origin, region, currImage->PitchY, 0, currImage->Y);

        void * pScoreboard = &Scoreboard[0];

        cl::Buffer scoreboardBuffer(
            context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
            mbImageWidth * mbImageHeight * sizeof(cl_int), pScoreboard, NULL);

        // Reset the Scoreboard buffer
        initialize.setArg(0, scoreboardBuffer);
        initialize.setArg(1, sizeof(int), &mbImageWidth);
        cl::Event evt;
        queue.enqueueNDRangeKernel(initialize, cl::NullRange, cl::NDRange(mbImageWidth, mbImageHeight, 1), cl::NullRange, NULL, &evt);
        evt.wait(); tpf[i] = evt.getProfilingInfo<CL_PROFILING_COMMAND_END>() - evt.getProfilingInfo<CL_PROFILING_COMMAND_START>();

        // Schedule full-frame motion estimation
        kernel.setArg(0, srcImage);
        kernel.setArg(1, refImage);
        kernel.setArg(2, predBuffer);
        kernel.setArg(3, mvBuffer);
        kernel.setArg(4, residualBuffer);
        kernel.setArg(5, shapeBuffer);
        kernel.setArg(6, scoreboardBuffer);
        kernel.setArg(7, launchBuffer);

        queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(PAD(width,16), mbImageHeight, 1), cl::NDRange(16, 1, 1), NULL, &evt);
        evt.wait(); tpf[i] += evt.getProfilingInfo<CL_PROFILING_COMMAND_END>() - evt.getProfilingInfo<CL_PROFILING_COMMAND_START>();
        time += tpf[i];
        queue.finish();
        
        // Read back resulting motion vectors (in a sync way)
        void * pMVs = &MVs[i * mvImageWidth * mvImageHeight];
        void * pResiduals = &Residuals[i * mvImageWidth * mvImageHeight];
        void * pShapes = &Shapes[i * mbImageWidth * mbImageHeight];

        queue.enqueueReadBuffer(mvBuffer,CL_TRUE,0,sizeof(MotionVector) * mvImageWidth * mvImageHeight,pMVs,0,0);
        queue.enqueueReadBuffer(residualBuffer,CL_TRUE,0,sizeof(cl_ushort) * mvImageWidth * mvImageHeight,pResiduals,0,0);
        queue.enqueueReadBuffer(shapeBuffer, CL_TRUE, 0, sizeof(cl_uchar2)* mbImageWidth * mbImageHeight, pShapes, 0, 0);
        queue.enqueueReadBuffer(scoreboardBuffer, CL_TRUE, 0, sizeof(cl_int)* mbImageWidth * mbImageHeight, pScoreboard, 0, 0);

        std::cout << "CL Time for Frame " << i << " is " << tpf[i] / (double)10e6 << " ms\n";

        for (int i = 0; i < mbImageWidth * mbImageHeight; i++)
        {
            if( Scoreboard[i] != 7 ) {
                throw std::runtime_error("Error detected in software scorebaording");
            }
        }
    }
    std::cout << "Total CL Time is " << time / (double)10e6 << " ms\n";
    
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
  ComputeNumMVs(CL_ME_MB_TYPE_4x4_INTEL, width, height, mvImageWidth, mvImageHeight,
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
                case 0: // 8 x 8
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
                case 2: // 4 x 8
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
        std::vector<MotionVector> MVs;
        std::vector<cl_ushort> Residuals;
        std::vector<cl_uchar2> Shapes;

        ExtractMotionVectorsFullFrameWithOpenCL(pCapture, MVs, Residuals, Shapes, cmd);

        // Generate sequence with overlaid motion vectors
        FrameWriter * pWriter = FrameWriter::CreateFrameWriter(width, height, pCapture->GetNumFrames(), cmd.out_to_bmp.getValue());
        PlanarImage * srcImage = CreatePlanarImage(width, height);

        int mvImageWidth, mvImageHeight;
        int mbImageWidth, mbImageHeight;
        ComputeNumMVs(CL_ME_MB_TYPE_4x4_INTEL, width, height, mvImageWidth, mvImageHeight, mbImageWidth, mbImageHeight);       

        unsigned int subBlockSize = ComputeSubBlockSize(CL_ME_MB_TYPE_4x4_INTEL);
        for (int k = 0; k < pCapture->GetNumFrames(); k++)
        {
            pCapture->GetSample(k, srcImage);
            // Overlay MVs on Src picture, except the very first one
            if(k>0)
                OverlayVectors(subBlockSize, MVs, Shapes, srcImage, k, width, height);
            pWriter->AppendFrame(srcImage);
        }
        std::cout << "Writing " << pCapture->GetNumFrames() << " frames to " << cmd.overlayFileName.getValue() << "..." << std::endl;
        pWriter->WriteToFile(cmd.overlayFileName.getValue().c_str());

        FrameWriter::Release(pWriter);
        Capture::Release(pCapture);
        ReleaseImage(srcImage);
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
