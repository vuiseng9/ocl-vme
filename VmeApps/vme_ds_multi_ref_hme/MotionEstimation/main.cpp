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

#define USE_HD_1920_1080    0
#define USE_SD_1280_720     1
#define USE_SD_720_576      0
#define USE_CIF_352_288     0

#define SHOW_BLOCKS         0

#include <iostream>
#include <vector>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <CL/cl.hpp>
#include <CL/cl_ext.h>
#include <CL/cl_intel_device_side_avc_motion_estimation.h>

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

const cl_short4 SILVER = { 181, 128, 128, 255 };

const cl_short4 BLACK  = { 16, 128, 128, 255 }; 
const cl_short4 GRAY   = { 150, 128, 128,   0 };
const cl_short4 MAROON = { 49, 109, 184, 255 }; 
const cl_short4 TEAL   = { 93, 147, 72,  255 }; 
const cl_short4 TAN    = { 174, 106, 144, 255 };
const cl_short4 GOLD   = { 158, 62, 161, 255 };
const cl_short4 PINK   = { 198, 124, 155, 255 }; 
const cl_short4 LAVDR  = { 216, 137, 127, 255 }; 
const cl_short4 IVORY  = { 234, 121, 129, 255 }; 

const cl_short4 RED    = { 82, 90, 240,  255 }; 
const cl_short4 ORANGE = { 165, 42, 179, 255 };
const cl_short4 YELLOW = { 210, 16, 146, 255 };
const cl_short4 GREEN  = { 145, 54, 34,  255 };
const cl_short4 BLUE   = { 137, 184, 40, 255 };
const cl_short4 INDIGO = { 48, 174, 152, 255 };
const cl_short4 VIOLET = { 82, 216, 180, 255 };

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
#if USE_HD_1920_1080
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
        frames(*this,           50, "frames", "<integer>", "Number of frame to use for motion estimation -- 0 represents entire video", 5)
#elif USE_SD_720_576
        fileName(*this,         0,"input", "string", "Input video sequence filename (.yuv file format)","iceage_720_576_50.yuv"),
        overlayFileName(*this,  0,"output","string", "Output video sequence with overlaid motion vectors filename ","iceage_720_576_50_output.yuv"),
        width(*this,            0, "width", "<integer>", "Frame width for the input file", 720),
        height(*this,           0, "height","<integer>", "Frame height for the input file", 576),
        frames(*this,           50, "frames", "<integer>", "Number of frame to use for motion estimation -- 0 represents entire video", 50)
#else
        fileName(*this,         0,"input", "string", "Input video sequence filename (.yuv file format)","foreman_cif_352x288_100.yuv"),
        overlayFileName(*this,  0,"output","string", "Output video sequence with overlaid motion vectors filename ","foreman_cif_352x288_100_output.yuv"),
        width(*this,            0, "width", "<integer>", "Frame width for the input file", 352),
        height(*this,           0, "height","<integer>", "Frame height for the input file", 288),
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

void PerformPerMBVMEWithScoreboarding( 
    Capture * pCapture, 
    std::vector<MotionVector> & MVs, std::vector<cl_ushort> & Residuals, std::vector<cl_ushort> & BestResiduals,
    std::vector<cl_uchar2> & Shapes, std::vector<cl_uint> & ReferenceIds, 
    std::vector<cl_uchar> & IntraShapes, std::vector<cl_ushort> & IntraResiduals, std::vector<cl_ulong> & IntraModes,
    const CmdParserMV& cmd)
{
    // OpenCL initialization

    OpenCLBasic init("Intel", "GPU", "0", CL_QUEUE_PROFILING_ENABLE);
    cl::Context context = cl::Context(init.context); clRetainContext(init.context);
    cl::Device device  = cl::Device(init.device);   clRetainDevice(init.device);
    cl::CommandQueue queue = cl::CommandQueue(init.queue);clRetainCommandQueue(init.queue);

    std::string ext = device.getInfo< CL_DEVICE_EXTENSIONS >();

    if (string::npos == ext.find("cl_intel_device_side_avc_motion_estimation"))
    {
        printf("WARNING: The selected device doesn't officially support motion estimation or accelerator extensions!");
    }

    char* programSource = NULL;
    if( LoadSourceFromFile( "vme_multi_ref_hme.cl", programSource ) )
    {
        printf("Error: Couldn't load kernel source from file.\n" );
    }    

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

    // Initializations.  

    const int numPics = pCapture->GetNumFrames();

    int width = cmd.width.getValue();
    int height = cmd.height.getValue();

    int mvImageWidth, mvImageHeight;
    int mbImageWidth, mbImageHeight;

    ComputeNumMVs(
        CL_ME_MB_TYPE_4x4_INTEL, DIV(width,4), DIV(height,4), 
        mvImageWidth, mvImageHeight, mbImageWidth, mbImageHeight);

    std::vector<MotionVector> predMVs;
    MotionVector zeroVector = { 0, 0 };
    predMVs.resize(mvImageWidth * mvImageHeight * numPics, zeroVector);

    //--------- Full frame VME on 4x downsampled frame to get initial predictors --------

    cl::Kernel downsample(p, "downsample4x");
    cl::Kernel tier1vme(p, "tier1_block_motion_estimate_intel");
    
    // Set up OpenCL surfaces
    
    cl::ImageFormat imageFormat(CL_R, CL_UNORM_INT8);

    cl::Image2D inImage(
        context, CL_MEM_READ_ONLY, imageFormat, 
        width, height, 0, 0);
    cl::Image2D src4xImage(
        context, CL_MEM_READ_WRITE, imageFormat, 
        DIV(width, 4), DIV(height, 4), 0, 0);
    cl::Image2D ref4xImage(
        context, CL_MEM_READ_WRITE, imageFormat, 
        DIV(width, 4), DIV(height, 4), 0, 0);        

    // Bootstrap video sequence reading
    
    PlanarImage * currImage = CreatePlanarImage(width, height);   

    cl::size_t<3> origin, region;   

    origin[0] = origin[1] = origin[2] = 0;
    region[0] = width; region[1] = height; region[2] = 1;

    double time = 0;
    vector<double> tpf(numPics);
    cl::Event evt;

    pCapture->GetSample(0, currImage);
    queue.enqueueWriteImage(inImage, CL_TRUE, origin, region, currImage->PitchY, 0, currImage->Y);

    // Perform down sample on the first frame.

    downsample.setArg(0, inImage);
    downsample.setArg(1, src4xImage);    
    queue.enqueueNDRangeKernel(
        downsample, cl::NullRange, 
        cl::NDRange(PAD(DIV(width, 4), 16), DIV(height, 16)), cl::NDRange(16, 1, 1), 
        NULL, &evt);
    evt.wait(); 
    tpf[0] += evt.getProfilingInfo<CL_PROFILING_COMMAND_END>() - evt.getProfilingInfo<CL_PROFILING_COMMAND_START>();
    time += tpf[0];
    queue.finish();

    // Perform down sample on the remaining frames and perform full frame VME.

    for (int i = 1; i < numPics; i++)
    {
        std::swap(ref4xImage, src4xImage);
        pCapture->GetSample(i, currImage);
        queue.enqueueWriteImage(inImage, CL_TRUE, origin, region, currImage->PitchY, 0, currImage->Y);
        downsample.setArg(0, inImage);
        downsample.setArg(1, src4xImage);
        queue.enqueueNDRangeKernel(
            downsample, cl::NullRange, 
            cl::NDRange(PAD(DIV(width, 4), 16), DIV(height, 16)), cl::NDRange(16, 1, 1), 
            NULL, &evt);
        evt.wait(); 
        tpf[i] += evt.getProfilingInfo<CL_PROFILING_COMMAND_END>() - evt.getProfilingInfo<CL_PROFILING_COMMAND_START>();
        time += tpf[i];
        queue.finish();
       
        cl::Buffer predBuffer(
            context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, 
            mvImageWidth * mvImageHeight * sizeof(MotionVector), 
            &predMVs[0] + mvImageWidth * mvImageHeight * i, NULL);

        tier1vme.setArg(0, src4xImage);
        tier1vme.setArg(1, ref4xImage);
        tier1vme.setArg(2, predBuffer);
        cl::Event evt;
        queue.enqueueNDRangeKernel(
            tier1vme, cl::NullRange, 
            cl::NDRange(PAD(DIV(width, 4), 16), mbImageHeight, 1), cl::NDRange(16, 1, 1), 
            NULL, &evt);
        evt.wait(); 
        tpf[i] += evt.getProfilingInfo<CL_PROFILING_COMMAND_END>() - evt.getProfilingInfo<CL_PROFILING_COMMAND_START>();
        time += tpf[i];
        queue.finish();

        // Read back results (in a sync way)
        void * pPredMVs = &predMVs[i * mvImageWidth * mvImageHeight];
        queue.enqueueReadBuffer(predBuffer, CL_TRUE, 0, sizeof(MotionVector) * mvImageWidth * mvImageHeight, pPredMVs, 0, 0);

        std::cout << "VME Down4x Time for Frame " << i << " is " << tpf[i] / (double)10e6 << " ms\n";
    }

    std::cout << "Total VME Down4x Time is " << time / (double)10e6 << " ms\n";

    //-------- VME using scoreboarding on the original frames using computed predictors ----------

    cl::Kernel kernel(p, "block_motion_estimate_intel");

    ComputeNumMVs(
        CL_ME_MB_TYPE_4x4_INTEL, width, height, 
        mvImageWidth, mvImageHeight, mbImageWidth, mbImageHeight);

    MVs.resize(numPics * mvImageWidth * mvImageHeight);
    Shapes.resize(numPics * mbImageWidth * mbImageHeight);
    // Set up OpenCL surfaces
    
    cl::Image2D ref0Image(context, CL_MEM_READ_WRITE, imageFormat, width, height, 0, 0);
    
    cl::Image2D refImage[1] = { ref0Image };

    cl::Image2D srcImage(context, CL_MEM_READ_WRITE, imageFormat, width, height, 0, 0);
    
    cl::Buffer mvBuffer(
        context, CL_MEM_READ_WRITE, 
        mvImageWidth * mvImageHeight * sizeof(MotionVector));

    cl::Buffer shapeBuffer(
        context, CL_MEM_READ_WRITE, 
        mbImageWidth * mbImageHeight * sizeof(cl_uchar2));
 
    // Bootstrap video sequence reading 6 reference frames and 1 source frame.
      
    int num_avail_refs = 0;

    for (int i = 0; i < numPics; i++)
    {   
        // Load next picture
        pCapture->GetSample(i, currImage);
        queue.enqueueWriteImage(srcImage, CL_TRUE, origin, region, currImage->PitchY, 0, currImage->Y);   

        // Convey the src frame.		
        int argIndex = 0;
        kernel.setArg(argIndex++, srcImage);
        kernel.setArg(argIndex++, refImage[0]);

        cl::Buffer predBuffer(
            context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, 
            mbImageWidth * mbImageHeight * sizeof(MotionVector), 
            &predMVs[0] + mbImageWidth * mbImageHeight * i, NULL);
        kernel.setArg(argIndex++, predBuffer);

        kernel.setArg(argIndex++, mvBuffer);
        kernel.setArg(argIndex++, shapeBuffer);

        queue.enqueueNDRangeKernel(
            kernel, cl::NullRange, 
            cl::NDRange(PAD(width,16), mbImageHeight, 1), cl::NDRange(16, 1, 1), 
            NULL, &evt);
        evt.wait(); 
        tpf[i] += evt.getProfilingInfo<CL_PROFILING_COMMAND_END>() - evt.getProfilingInfo<CL_PROFILING_COMMAND_START>();
        time += tpf[i];
        queue.finish();
        
        // Read back results (in a sync way)
        void * pMVs = &MVs[i * mvImageWidth * mvImageHeight];
        void * pShapes = &Shapes[i * mbImageWidth * mbImageHeight];

        queue.enqueueReadBuffer(mvBuffer,CL_TRUE,0,sizeof(MotionVector) * mvImageWidth * mvImageHeight,pMVs,0,0);
        queue.enqueueReadBuffer(shapeBuffer, CL_TRUE, 0, sizeof(cl_uchar2)* mbImageWidth * mbImageHeight, pShapes, 0, 0);        

         std::swap( refImage[0], srcImage );       

        std::cout << "CL Time for Frame " << i << " is " << tpf[i] / (double)10e6 << " ms\n";       
    }
    std::cout << "Total CL Time is " << time / (double)10e6 << " ms\n";
    
    ReleaseImage(currImage);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Overlay routines
//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Draw a pixel on Y picture
void DrawPixel(int x, int y, PlanarImage* srcImage, int nPicWidth, int nPicHeight, cl_short4 COLOR)
{
	int nYPixPos, nUVPixPos;

	if (x < 0 || x >= nPicWidth || y<0 || y >= nPicHeight)
    {
		return;         // Don't draw out of bound pixels
    }
	// Draw Luma channel 
	nYPixPos = y * nPicWidth + x;
	*(srcImage->Y + nYPixPos) = (uint8_t) COLOR.s[0];
	if (COLOR.s[3] == 255)
	{
		// Draw Chroma channels only for color
		nUVPixPos = ((y / 2) * (nPicWidth / 2) + (x / 2));
		*(srcImage->U + nUVPixPos) = (uint8_t) COLOR.s[1];
		*(srcImage->V + nUVPixPos) = (uint8_t) COLOR.s[2];
	}
}

// Bresenham's line algorithm
void DrawLine(int x0, int y0, int dx, int dy, PlanarImage* srcImage, int nPicWidth, int nPicHeight, cl_short4 COLOR)

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
	    	DrawPixel(y0, x0, srcImage, nPicWidth, nPicHeight, COLOR);
		else
			DrawPixel(x0, y0, srcImage, nPicWidth, nPicHeight, COLOR);

		nError -= nDeltaY;
		if (nError < 0)
		{
			y0 += nYStep;
			nError += nDeltaX;
		}
	}
}

void OverlayVectors(unsigned int subBlockSize, std::vector<MotionVector>& MVs,
                    std::vector<cl_uchar2>& InterShapes,
                    PlanarImage* srcImage,
                    int frame, int width, int height) {
  int mvImageWidth, mvImageHeight;
  int mbImageWidth, mbImageHeight;
  ComputeNumMVs(CL_ME_MB_TYPE_4x4_INTEL, width, height, mvImageWidth, mvImageHeight,
                mbImageWidth, mbImageHeight);
  MotionVector* pMV = &MVs[frame * mvImageWidth * mvImageHeight];
  cl_uchar2* pInterShapes = &InterShapes[frame * mbImageWidth * mbImageHeight];

#define OFF(P) (P + 2) >> 2
  for (int i = 0; i < mbImageHeight; i++) {
    for (int j = 0; j < mbImageWidth; j++) {
      int mbIndex = j + i * mbImageWidth;
      // Selectively Draw motion vectors for different sub block sizes
      int j0 = j * 16; int i0 = i * 16; int m0 = mbIndex * 16;
      
      {
          switch (pInterShapes[mbIndex].s[0]){
            case CL_AVC_ME_MAJOR_16x16_INTEL: {
                cl_short4 color = VIOLET;
    #if SHOW_BLOCKS
                DrawLine(j0, i0, 16, 0, srcImage, width, height, color);
                DrawLine(j0, i0, 0, 16, srcImage, width, height, color);
                DrawLine(j0 + 16, i0, 0, 16, srcImage, width, height, color);
                DrawLine(j0, i0 + 16, 16, 0, srcImage, width, height, color);
    #else
                DrawLine(j0 + 8, i0 + 8, OFF(pMV[m0].s[0]), OFF(pMV[m0].s[1]), srcImage, width, height, color);
    #endif
                break;
            }
            case CL_AVC_ME_MAJOR_16x8_INTEL: {
                cl_short4 color0 = VIOLET;
                cl_short4 color1 = VIOLET;
    #if SHOW_BLOCKS
                DrawLine(j0, i0, 16, 0, srcImage, width, height, color0);
                DrawLine(j0, i0, 0, 8, srcImage, width, height, color0);
                DrawLine(j0 + 16, i0, 0, 8, srcImage, width, height, color0);                       

                DrawLine(j0, i0 + 8, 16, 0, srcImage, width, height, color1);

                DrawLine(j0, i0 + 8, 0, 8, srcImage, width, height, color1);
                DrawLine(j0 + 16, i0 + 8, 0, 8, srcImage, width, height, color1);
                DrawLine(j0, i0 + 16, 16, 0, srcImage, width, height, color1);
    #else
                DrawLine(j0 + 8, i0 + 4,  OFF(pMV[m0].s[0]), OFF(pMV[m0].s[1]), srcImage, width, height, color0);
                DrawLine(j0 + 8, i0 + 12, OFF(pMV[m0 + 8].s[0]), OFF(pMV[m0 + 8].s[1]), srcImage, width, height, color1);
    #endif
                break;
            }
            case CL_AVC_ME_MAJOR_8x16_INTEL: {
                cl_short4 color0 = VIOLET;
                cl_short4 color1 = VIOLET;
    #if SHOW_BLOCKS
                DrawLine(j0, i0, 8, 0, srcImage, width, height, color0);
                DrawLine(j0, i0, 0, 16, srcImage, width, height, color0);      
                DrawLine(j0, i0 + 16, 8, 0, srcImage, width, height, color0);

                DrawLine(j0 + 8, i0, 8, 0, srcImage, width, height, color1);
                DrawLine(j0 + 16, i0, 0, 16, srcImage, width, height, color1);            
                DrawLine(j0 + 8, i0 + 16, 8, 0, srcImage, width, height, color1);

                DrawLine(j0 + 8, i0, 0, 16, srcImage, width, height, color1);

    #else
                DrawLine(j0 + 4, i0 + 8, OFF(pMV[m0].s[0]), OFF(pMV[m0].s[1]), srcImage, width, height, color0);
                DrawLine(j0 + 12, i0 + 8, OFF(pMV[m0 + 8].s[0]), OFF(pMV[m0 + 8].s[1]), srcImage, width, height, color1);
    #endif
                break;
            }
            case CL_AVC_ME_MAJOR_8x8_INTEL: {
                cl_short4 color0 = VIOLET;
                cl_short4 color1 = VIOLET;
                cl_short4 color2 = VIOLET;
                cl_short4 color3 = VIOLET;
                cl_uchar4 minor_shapes;
                minor_shapes.s[0] = (pInterShapes[mbIndex].s[1]) & 0x03;
                minor_shapes.s[1] = (pInterShapes[mbIndex].s[1] >> 2) & 0x03;
                minor_shapes.s[2] = (pInterShapes[mbIndex].s[1] >> 4) & 0x03;
                minor_shapes.s[3] = (pInterShapes[mbIndex].s[1] >> 6) & 0x03;
    #if SHOW_BLOCKS
                DrawLine(j0, i0, 8, 0, srcImage, width, height, color0);
                DrawLine(j0, i0, 0, 8, srcImage, width, height, color0);

                DrawLine(j0 + 8, i0, 8, 0, srcImage, width, height, color1);
                DrawLine(j0 + 16, i0, 0, 8, srcImage, width, height, color1);

                DrawLine(j0, i0 + 8, 8, 0, srcImage, width, height, color2);
                DrawLine(j0, i0 + 8, 0, 8, srcImage, width, height, color2);
                DrawLine(j0, i0 + 16, 8, 0, srcImage, width, height, color2);

                DrawLine(j0 + 8, i0 + 8, 8, 0, srcImage, width, height, color3);
                DrawLine(j0 + 16, i0 + 8, 0, 8, srcImage, width, height, color3);
                DrawLine(j0 + 8, i0 + 16, 8, 0, srcImage, width, height, color3);
    #endif
                for (int m = 0; m < 4; ++m) {
                    cl_short4 color = VIOLET;
                    int mdiv = m / 2;
                    int mmod = m % 2;
                    switch (minor_shapes.s[m]) {
                    case CL_AVC_ME_MINOR_8x8_INTEL: {
    #if !SHOW_BLOCKS
                        DrawLine(j0 + mmod * 8 + 4, i0 + mdiv * 8 + 4, OFF(pMV[m0 + m * 4].s[0]), OFF(pMV[m0 + m * 4].s[1]), srcImage, width, height, color);
    #endif
                        break;
                    }
                    case CL_AVC_ME_MINOR_8x4_INTEL: {                    
    #if SHOW_BLOCKS 
                        DrawLine(j0 + mmod * 8, i0 + mdiv * 8 + 4, 8, 0, srcImage, width, height, color);
    #else
                        for (int n = 0; n < 2; ++n) {
                            DrawLine(j0 + mmod * 8 + 4, i0 + (mdiv * 8 + n * 4 + 2), OFF(pMV[m0 + m * 4 + n * 2].s[0]), OFF(pMV[m0 + m * 4 + n * 2].s[1]), srcImage, width, height, color);
                        }
    #endif
                        break;
                    }
                    case CL_AVC_ME_MINOR_4x8_INTEL: {
    #if SHOW_BLOCKS
                        DrawLine(j0 + mmod * 8 + 4, i0 + mdiv * 8, 8, 0, srcImage, width, height, color);
    #else                   
                        for (int n = 0; n < 2; ++n) {
                            DrawLine(j0 + (mmod * 8 + n * 4 + 2), i0 + mdiv * 8 + 4, OFF(pMV[m0 + m * 4 + n * 2].s[0]), OFF(pMV[m0 + m * 4 + n * 2].s[1]), srcImage, width, height, color);
                        }
    #endif
                        break;
                    }
                    case CL_AVC_ME_MINOR_4x4_INTEL: {
    #if SHOW_BLOCKS
                        DrawLine(j0 + mmod * 8, i0 + mdiv * 8 + 4, 8, 0, srcImage, width, height, color);
                        DrawLine(j0 + mmod * 8 + 4, i0 + mdiv * 8, 8, 0, srcImage, width, height, color);
    #else
                        for (int n = 0; n < 4; ++n) {
                            DrawLine(j0 + n * 4 + 2, i0 + m * 4 + 2, OFF(pMV[m0 + m * 4 + n].s[0]), OFF(pMV[m0 + m * 4 + n].s[1]), srcImage, width, height, color);
                        }
    #endif
                        break;
                    }
                  }
                }
                break;
            }
          }
       }
    }
  }
}


void PrintIntraModes( std::vector<cl_ulong>& intraModes, std::vector<cl_uchar>& intraShapes, int width, int height )
{
	std::ofstream file;
    file.open("intra.txt");
    unsigned numMBs = DIV(width, 16) * DIV(height, 16);
    for (unsigned i = 0; i < intraModes.size(); i++)
    {
        unsigned pic = i / numMBs;
        unsigned mb = i % numMBs;
        unsigned mb_x = mb % DIV(width, 16);
        unsigned mb_y = mb / DIV(width, 16);
        
        file << "pic=" << pic << " mb=(" << mb_x << "," << mb_y << "): ";

        if (intraShapes[i] == CL_AVC_ME_INTRA_16x16_INTEL) {
            assert(unsigned(intraModes[i] & 0xF) < 4);
            file << unsigned(intraModes[i] & 0xF);
        }
        else if (intraShapes[i] == CL_AVC_ME_INTRA_8x8_INTEL) {
            for (unsigned j = 0; j < 12; j += 4) {
                assert(unsigned((intraModes[i] >> j * 4) & 0xF) < 9);
                file << unsigned((intraModes[i] >> j * 4) & 0xF) << ", ";
            }          
            file << unsigned((intraModes[i] >> 48) & 0xF);
        }
        else if (intraShapes[i] == CL_AVC_ME_INTRA_4x4_INTEL) {
            for (unsigned j = 0; j < 15; j++) {
                assert(unsigned((intraModes[i] >> j * 4) & 0xF) < 9);
                file << unsigned((intraModes[i] >> j * 4) & 0xF) << ", ";
            }
            file << unsigned((intraModes[i] >> 60) & 0xF);
        }
        else {
            throw std::runtime_error("Invalid intra shape returned.");
        }

        file << std::endl;
    }
    file.close();
}

void PrintIntraDists( std::vector<cl_ushort>& intraDists, int width, int height )
{
	std::ofstream file;
    file.open("intra_dists.txt");
    unsigned numMBs = DIV(width, 16) * DIV(height, 16);
    for (unsigned i = 0; i < intraDists.size(); i++)
    {
        unsigned pic = i / numMBs;
        unsigned mb = i % numMBs;
        unsigned mb_x = mb % DIV(width, 16);
        unsigned mb_y = mb / DIV(width, 16);
        file << "pic=" << pic << " mb=(" << mb_x << "," << mb_y << "): ";
        file << unsigned(intraDists[i]);
        file << std::endl;
    }
    file.close();
}

void PrintInterDists( std::vector<cl_ushort>& interDists, int width, int height )
{
	std::ofstream file;
    file.open("inter_dists.txt");
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

void PrintInterBestDists( std::vector<cl_ushort>& interDists, int width, int height )
{
	std::ofstream file;
    file.open("inter_best_dists.txt");
    unsigned numMBs = DIV(width, 16) * DIV(height, 16);
    for (unsigned i = 0; i < interDists.size(); i++)
    {
        unsigned pic = i / numMBs;
        unsigned mb = i % numMBs;
        unsigned mb_x = mb % DIV(width, 16);
        unsigned mb_y = mb / DIV(width, 16);
        file << "pic=" << pic << " mb=(" << mb_x << "," << mb_y << "): ";
        file << unsigned(interDists[i]);
        file << std::endl;
    }
    file.close();
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
        std::vector<cl_ushort> BestResiduals;
        std::vector<cl_uchar2> Shapes;
        std::vector<cl_uint> ReferenceIds;

        std::vector<cl_uchar> IntraShapes;
        std::vector<cl_ushort> IntraResiduals; 
        std::vector<cl_ulong> IntraModes;        

        PerformPerMBVMEWithScoreboarding(
            pCapture, MVs, Residuals, BestResiduals, Shapes, ReferenceIds, 
            IntraShapes, IntraResiduals, IntraModes,
            cmd);

        // Generate sequence with overlaid motion vectors
        FrameWriter * pWriter = 
            FrameWriter::CreateFrameWriter(width, height, pCapture->GetNumFrames(), cmd.out_to_bmp.getValue());
        PlanarImage * srcImage = CreatePlanarImage(width, height);

        int mvImageWidth, mvImageHeight;
        int mbImageWidth, mbImageHeight;
        ComputeNumMVs(CL_ME_MB_TYPE_4x4_INTEL, width, height, mvImageWidth, mvImageHeight, mbImageWidth, mbImageHeight);       

        unsigned int subBlockSize = ComputeSubBlockSize(CL_ME_MB_TYPE_4x4_INTEL);
        for (int k = 0; k < pCapture->GetNumFrames(); k++)
        {
            pCapture->GetSample(k, srcImage);
            {
                OverlayVectors(
                    subBlockSize, MVs, 
                    Shapes, 
                    srcImage, k, width, height);
            }
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
