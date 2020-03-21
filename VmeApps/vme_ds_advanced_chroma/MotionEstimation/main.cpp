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

#define USE_HD   0
#define USE_SD   1
#define USE_QCIF 0

#define DO_INTRA (1)
#define DO_CHROMA_INTRA (1)

#define PRINT_INTRA_MODES (1)

#define ONLY_INTRA (0)
#define ONLY_SKIP (0)

#include <iostream>
#include <vector>
#include <sstream>
#include <fstream>
#include <algorithm>

#include <CL/cl.hpp>
#include <CL/cl_intel_planar_yuv.hpp>
#include <CL/cl_ext_intel.h>

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
#define SET(VAR, X, Y, Z) \
        VAR[0] = X; \
        VAR[1] = Y; \
        VAR[2] = Z;

using namespace YUVUtils;

// these values define dimensions of input pixel blocks (whcih are fixed in hardware)
// so, do not change these values to avoid errors
#define SRC_BLOCK_WIDTH 16
#define SRC_BLOCK_HEIGHT 16

typedef cl_short2 MotionVector;

#ifndef CL_ME_COST_PENALTY_NONE_INTEL
#define CL_ME_COST_PENALTY_NONE_INTEL                   0x0
#define CL_ME_COST_PENALTY_LOW_INTEL                    0x1
#define CL_ME_COST_PENALTY_NORMAL_INTEL                 0x2
#define CL_ME_COST_PENALTY_HIGH_INTEL                   0x3
#endif

#ifndef CL_ME_COST_PRECISION_QPEL_INTEL
#define CL_ME_COST_PRECISION_QPEL_INTEL                 0x0
#define CL_ME_COST_PRECISION_HPEL_INTEL                 0x1
#define CL_ME_COST_PRECISION_PEL_INTEL                  0x2
#define CL_ME_COST_PRECISION_DPEL_INTEL                 0x3
#endif

#define CL_AVC_ME_PARTITION_MASK_ALL_INTEL                0x0
#define CL_AVC_ME_PARTITION_MASK_16x16_INTEL              0x7E
#define CL_AVC_ME_PARTITION_MASK_16x8_INTEL               0x7D
#define CL_AVC_ME_PARTITION_MASK_8x16_INTEL               0x7B
#define CL_AVC_ME_PARTITION_MASK_8x8_INTEL                0x77
#define CL_AVC_ME_PARTITION_MASK_8x4_INTEL                0x6F
#define CL_AVC_ME_PARTITION_MASK_4x8_INTEL                0x5F
#define CL_AVC_ME_PARTITION_MASK_4x4_INTEL                0x3F


#define CL_AVC_ME_SUBPIXEL_MODE_INTEGER_INTEL             0x0
#define CL_AVC_ME_SUBPIXEL_MODE_HPEL_INTEL                0x1
#define CL_AVC_ME_SUBPIXEL_MODE_QPEL_INTEL                0x3

#define CL_AVC_ME_SAD_ADJUST_MODE_NONE_INTEL              0x0
#define CL_AVC_ME_SAD_ADJUST_MODE_HAAR_INTEL              0x2


// Specifies number of motion vectors per source pixel block (the value of CL_ME_MB_TYPE_16x16_INTEL specifies  just a single vector per block )
static const cl_uint kMBBlockType = CL_ME_MB_TYPE_16x16_INTEL;
static const cl_uint kPixelMode = CL_ME_SUBPIXEL_MODE_QPEL_INTEL;
static const cl_uint kSADAdjust = CL_ME_SAD_ADJUST_MODE_NONE_INTEL;
static const cl_uint kSearchRadius = CL_ME_SEARCH_PATH_RADIUS_16_12_INTEL;
static const cl_uint kCostPenalty = CL_ME_COST_PENALTY_HIGH_INTEL;
static const cl_uint kCostPrecision = CL_ME_COST_PRECISION_HPEL_INTEL;

static const cl_uint kCount = 8;

static const cl_short kPredictorX0 = 0;
static const cl_short kPredictorY0 = 0;
static const cl_short kPredictorX1 = 96;
static const cl_short kPredictorY1 = 24;

#ifdef _MSC_VER
#pragma warning (push)
#pragma warning (disable : 4355)    // 'this': used in base member initializer list
#endif

// All command-line options for the sample
class CmdParserMV : public CmdParser
{
public:
    CmdOption<bool>        out_to_bmp;
    CmdOption<bool>        help;
    CmdOption<std::string>         fileName;
    CmdOption<std::string>         overlayFileName;
    CmdOption<int>        width;
    CmdOption<int>      height;
    CmdOption<int>      frames;

    CmdParserMV(int argc, const char** argv) :
        CmdParser(argc, argv),
        out_to_bmp(*this, 'b', "nobmp", "", "Do not output frames to the sequence of bmp files (in addition to the yuv file), by default the output is off", true, "nobmp"),
        help(*this, 'h', "help", "", "Show this help text and exit."),

#if USE_HD
        fileName(*this, 0, "input", "string", "Input video sequence filename (.yuv file format)", "video_1920x1080_5frames.yuv"),
        overlayFileName(*this, 0, "output", "string", "Output video sequence with overlaid motion vectors filename ", "video_1920x1080_5frames_output.yuv"),
        width(*this,  0, "width", "<integer>", "Frame width for the input file", 1920),
        height(*this, 0, "height", "<integer>", "Frame height for the input file", 1080),
        frames(*this, 5, "frames", "<integer>", "Number of frame to use for motion estimation -- 0 represents entire video", 5)
#elif USE_SD
        fileName(*this, 0, "input", "string", "Input video sequence filename (.yuv file format)", "goal_1280x720.yuv"),
        overlayFileName(*this, 0, "output", "string", "Output video sequence with overlaid motion vectors filename ", "goal_1280x720_output.yuv"),
        width(*this,  0, "width", "<integer>", "Frame width for the input file", 1280),
        height(*this, 0, "height", "<integer>", "Frame height for the input file", 720),
        frames(*this, 5, "frames", "<integer>", "Number of frame to use for motion estimation -- 0 represents entire video", 5)
#else
        fileName(*this, 0, "input", "string", "Input video sequence filename (.yuv file format)", "boat_qcif_176x144.yuv"),
        overlayFileName(*this, 0, "output", "string", "Output video sequence with overlaid motion vectors filename ", "boat_qcif_176x144_output.yuv"),
        width(*this,  0, "width", "<integer>", "Frame width for the input file", 176),
        height(*this, 0, "height", "<integer>", "Frame height for the input file", 144),
        frames(*this, 100, "frames", "<integer>", "Number of frame to use for motion estimation -- 0 represents entire video", 100)
#endif      
    {
    }
    virtual void parse()
    {
        CmdParser::parse();
        if (help.isSet())
        {
            printUsage(std::cout);
        }
    }
};
#ifdef _MSC_VER
#pragma warning (pop)
#endif

bool LoadSourceFromFile(
    const char* filename,
    char* & sourceCode)
{
    bool error = false;
    FILE* fp = NULL;
    int nsize = 0;

    // Open the shader file

    fopen_s(&fp, filename, "rb");
    if (!fp)
    {
        error = true;
    }
    else
    {
        // Allocate a buffer for the file contents
        fseek(fp, 0, SEEK_END);
        nsize = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        sourceCode = new char[nsize + 1];
        if (sourceCode)
        {
            fread(sourceCode, 1, nsize, fp);
            sourceCode[nsize] = 0; // Don't forget the NULL terminator
        }
        else
        {
            error = true;
        }

        fclose(fp);
    }

    return error;
}


typedef struct CLInit {
    cl::Context context;
    cl::Device device;
    cl::CommandQueue queue;
    cl::Program program;
    CLInit() {
        // OpenCL initialization
        OpenCLBasic init("Intel", "GPU", "0", CL_QUEUE_PROFILING_ENABLE);
        //OpenCLBasic creates the platform/context and device for us, so all we need is to get an ownership (via incrementing ref counters with clRetainXXX)

        context = cl::Context(init.context); clRetainContext(init.context);
        device = cl::Device(init.device);   clRetainDevice(init.device);
        queue = cl::CommandQueue(init.queue); clRetainCommandQueue(init.queue);

        std::string ext = device.getInfo< CL_DEVICE_EXTENSIONS >();
        if (string::npos == ext.find("cl_intel_device_side_avc_motion_estimation"))
        {
            printf("WARNING: The selected device doesn't officially support device-side motion estimation!");
        }
        char* programSource = NULL;
        if (LoadSourceFromFile("vme_advanced_chroma_ds.cl", programSource))
            // Load the kernel source from the passed in file.
        {
            printf("Error: Couldn't load kernel source from file.\n");
        }

        // Create a built-in VME kernel
        cl_int err = 0;
        const cl_device_id & d = device();
        program = cl::Program(clCreateProgramWithSource(context(), 1, (const char**)&programSource, NULL, &err));

        err = clBuildProgram(program(), 1, &d, "", NULL, NULL);

        if (err != CL_SUCCESS) {
            size_t  buildLogSize = 0;
            clGetProgramBuildInfo(program(), d, CL_PROGRAM_BUILD_LOG, 0, NULL, &buildLogSize);
            cl_char*    buildLog = new cl_char[buildLogSize];
            if (buildLog) {
                clGetProgramBuildInfo(program(), d, CL_PROGRAM_BUILD_LOG, buildLogSize, buildLog, NULL);
                std::cout << ">>> Build Log:\n" << buildLog << ">>>End of Build Log\n";
            }
        }
    }

}clInit;

inline void ComputeNumMVs(cl_uint nMBType,
    int nPicWidth, int nPicHeight,
    int & nMVSurfWidth, int & nMVSurfHeight,
    int & nMBSurfWidth, int & nMBSurfHeight)
{
    // Size of the input frame in pixel blocks (SRC_BLOCK_WIDTH x SRC_BLOCK_HEIGHT each)
    int nPicWidthInBlk = DIV(nPicWidth, SRC_BLOCK_WIDTH);
    int nPicHeightInBlk = DIV(nPicHeight, SRC_BLOCK_HEIGHT);

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

    nMBSurfWidth = nPicWidthInBlk;
    nMBSurfHeight = nPicHeightInBlk;
}

inline unsigned int ComputeSubBlockSize(cl_uint nMBType)
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

#ifdef DO_CHROMA_INTRA

cl_int WriteYUVImageToOCLNV12( 
	cl::Image2D& nv12Image, 
	cl::Image2DYPlane& nv12ImageY, 
	cl::Image2DUVPlane& nv12ImageUV,
	cl::Context& context, 
	cl::CommandQueue& queue, 
	PlanarImage * srcImage )
{
    cl_int err = CL_SUCCESS;	

    size_t pitchDestY = 0;    
    cl_uchar * mappedAddrY = NULL;	
    
	cl::size_t<3> origin;	// Init to 0.
	cl::size_t<3> region;	// Init to 0.
	
	// Write luma.
	
	region[0] = srcImage->Width; region[1] = srcImage->Height; region[2] = 1;
	mappedAddrY = ( cl_uchar* )queue.enqueueMapImage( nv12ImageY, CL_TRUE, CL_MAP_WRITE, origin, region, &pitchDestY, NULL, NULL, NULL, &err );
    

    const cl_uchar * uPlaneSrc = srcImage->U;
    const cl_uchar * vPlaneSrc = srcImage->V;

    for( cl_uint i = 0; i < srcImage->Height; ++i )
    {
        memcpy( mappedAddrY + i * pitchDestY, srcImage->Y + ( i * srcImage->PitchY ), srcImage->Width );
    }

	err = queue.enqueueUnmapMemObject( nv12ImageY, mappedAddrY );
    assert( err == CL_SUCCESS );    

	// Write chroma.

	size_t pitchDestUV = 0;
    cl_uchar * mappedAddrUV = NULL;
	
	region[0] = srcImage->Width / 2; region[1] = srcImage->Height / 2; region[2] = 1;
	mappedAddrUV = ( cl_uchar* )queue.enqueueMapImage( nv12ImageUV, CL_TRUE, CL_MAP_WRITE, origin, region, &pitchDestUV, NULL, NULL, NULL, &err );    

    cl_uchar * chromaDest = mappedAddrUV;
    for( cl_uint j = 0; j < srcImage->Height / 2; j += 1 )
    {
        cl_uchar * chromaTmp = chromaDest;
        for( cl_uint k = 0; k < srcImage->Width / 2; k += 1 )
        {
			chromaDest[0] = *( uPlaneSrc + ( j * srcImage->PitchU ) + k );
            chromaDest[1] = *( vPlaneSrc + ( j * srcImage->PitchV ) + k );
            chromaDest += 2;
        }
        chromaDest = chromaTmp + pitchDestUV;
    }

	err = queue.enqueueUnmapMemObject( nv12ImageUV, mappedAddrUV );
    assert( err == CL_SUCCESS );

    return err;
}

#endif

void ExtractMotionVectorsFullFrameWithOpenCL(
    CLInit * clInit, 
    Capture * pCapture,
    std::vector<MotionVector> &MVs,
    std::vector<cl_ushort> &SADs,
    const CmdParserMV& cmd)
{

    cl::Kernel kernel(clInit->program, "block_advanced_motion_estimate_check_intel");

    cl_uchar partitionMask = 0;
    cl_uchar sadAdjustment = 0;
    cl_uchar pixelMode = 0;

    switch (kMBBlockType) {
    case CL_ME_MB_TYPE_16x16_INTEL: partitionMask = CL_AVC_ME_PARTITION_MASK_16x16_INTEL; break;
    case CL_ME_MB_TYPE_8x8_INTEL:   partitionMask = CL_AVC_ME_PARTITION_MASK_8x8_INTEL; break;
    case CL_ME_MB_TYPE_4x4_INTEL:   partitionMask = CL_AVC_ME_PARTITION_MASK_4x4_INTEL; break;
    default:                        partitionMask = CL_AVC_ME_PARTITION_MASK_ALL_INTEL; break;
    }
    switch (kSADAdjust) {
    case CL_ME_SAD_ADJUST_MODE_HAAR_INTEL:  sadAdjustment = CL_AVC_ME_SAD_ADJUST_MODE_HAAR_INTEL; break;
    case CL_ME_SAD_ADJUST_MODE_NONE_INTEL: default:  sadAdjustment = CL_AVC_ME_SAD_ADJUST_MODE_NONE_INTEL;
    }
    //partitionMask = CL_AVC_ME_PARTITION_MASK_16x16;
    switch (kPixelMode) {
    case CL_ME_SUBPIXEL_MODE_QPEL_INTEL:    pixelMode = CL_AVC_ME_SUBPIXEL_MODE_QPEL_INTEL; break;
    case CL_ME_SUBPIXEL_MODE_HPEL_INTEL:    pixelMode = CL_AVC_ME_SUBPIXEL_MODE_HPEL_INTEL; break;
    case CL_ME_SUBPIXEL_MODE_INTEGER_INTEL: pixelMode = CL_AVC_ME_SUBPIXEL_MODE_INTEGER_INTEL; break;
    }

    int numPics = pCapture->GetNumFrames();
    int width = cmd.width.getValue();
    int height = cmd.height.getValue();
    int mvImageWidth, mvImageHeight;
    int mbImageWidth, mbImageHeight;
    ComputeNumMVs(kMBBlockType, width, height, mvImageWidth, mvImageHeight, mbImageWidth, mbImageHeight);

    MVs.resize(numPics * mvImageWidth * mvImageHeight);
    SADs.resize(numPics * mvImageWidth * mvImageHeight);

    // Set up OpenCL surfaces

    cl::ImageFormat imageFormat(CL_R, CL_UNORM_INT8);

	cl::Image2D srcImage(clInit->context, CL_MEM_READ_ONLY, imageFormat, width, height, 0, 0);
    cl::Image2D refImage(clInit->context, CL_MEM_READ_ONLY, imageFormat, width, height, 0, 0);
 

    cl_short2 *countMem = new cl_short2[mbImageWidth * mbImageHeight];
    for (int i = 0; i < mbImageWidth * mbImageHeight; i++)
    {
        countMem[i].s[0] = kCount;
        countMem[i].s[1] = 0;
    }

    cl_short2 *predMem = new cl_short2[mbImageWidth * mbImageHeight * 8];
    for (int i = 0; i < mbImageWidth * mbImageHeight; i++)
    {
        for (int j = 0; j < 1; j++)
        {
            predMem[i * 8 + j].s[0] = 0;
            predMem[i * 8 + j].s[1] = 0;
        }
        for (int j = 1; j < 2; j++)
        {
            predMem[i * 8 + j].s[0] = kPredictorX0;
            predMem[i * 8 + j].s[1] = kPredictorY0;
        }
        for (int j = 2; j < 3; j++)
        {
            predMem[i * 8 + j].s[0] = -kPredictorX0;
            predMem[i * 8 + j].s[1] = kPredictorY0;
        }
        for (int j = 3; j < 4; j++)
        {
            predMem[i * 8 + j].s[0] = kPredictorX0;
            predMem[i * 8 + j].s[1] = -kPredictorY0;
        }
        for (int j = 4; j < 5; j++)
        {
            predMem[i * 8 + j].s[0] = -kPredictorX0;
            predMem[i * 8 + j].s[1] = -kPredictorY0;
        }
        for (int j = 5; j < 8; j++)
        {
            predMem[i * 8 + j].s[0] = kPredictorX1;
            predMem[i * 8 + j].s[1] = kPredictorY1;
        }
    }

    cl::Buffer countBuffer(
        clInit->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        mbImageWidth * mbImageHeight * sizeof(cl_short2), countMem, NULL);
    cl::Buffer predBuffer(
        clInit->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        mbImageWidth * mbImageHeight * 8 * sizeof(cl_short2), predMem, NULL);

    cl::Buffer mvBuffer(
        clInit->context, CL_MEM_WRITE_ONLY /* | CL_MEM_ALLOC_HOST_PTR*/,
        mvImageWidth * mvImageHeight * sizeof(MotionVector));
    cl::Buffer residualBuffer(
        clInit->context, CL_MEM_WRITE_ONLY,
        mvImageWidth * mvImageHeight * sizeof(cl_ushort));

    // Bootstrap video sequence reading
    PlanarImage * currImage = CreatePlanarImage(width, height);

    pCapture->GetSample(0, currImage);
    cl::size_t<3> origin, region;
    SET(origin, 0, 0, 0);
    SET(region, width, height, 1);
    
    clInit->queue.enqueueWriteImage(srcImage, CL_TRUE, origin, region, currImage->PitchY, 0, currImage->Y, NULL, 0);

    // Process all frames
    double ioStat = 0;//file i/o
    double ioTileStat = 0;
    int count = 0;

    unsigned flags = 0;
    unsigned skipBlockType = 0;
    unsigned costPenalty = kCostPenalty;
    unsigned costPrecision = kCostPrecision;

    double overallStart = time_stamp();
    cl::Event evt;
    vector<double> tpf(numPics);
    double ndRangeTime = 0;

    // First frame is already in srcImg, so we start with the second frame
    for (int i = 1; i < numPics; i++, count++)
    {
        double ioStart = time_stamp();

        // Load next picture
        pCapture->GetSample(i, currImage);

        std::swap(refImage, srcImage);

        double ioTileStart = time_stamp();

	    // Copy to tiled image memory - this copy (and its overhead) is not necessary in a full GPU pipeline
        clInit->queue.enqueueWriteImage(srcImage, CL_TRUE, origin, region, currImage->PitchY, 0, currImage->Y, NULL, 0);

        ioTileStat += (time_stamp() - ioTileStart);

        ioStat += (time_stamp() - ioStart);

        // Schedule full-frame motion estimation

		int argIndex = 0;

        kernel.setArg(argIndex++, srcImage);                        // src image
        kernel.setArg(argIndex++, refImage);                        // ref image
        kernel.setArg(argIndex++, sizeof(unsigned), &flags);        // flags
        kernel.setArg(argIndex++, sizeof(unsigned), &skipBlockType); // skip block type
        kernel.setArg(argIndex++, sizeof(unsigned), &costPenalty);    // cost penalty
        kernel.setArg(argIndex++, sizeof(unsigned), &costPrecision); // cost precision
        kernel.setArg(argIndex++, countBuffer);
        kernel.setArg(argIndex++, predBuffer);

        kernel.setArg(argIndex++, sizeof(cl_mem), NULL);            // skip checks
        kernel.setArg(argIndex++, mvBuffer);                        // search mvs
        kernel.setArg(argIndex++, sizeof(cl_mem), NULL);        // intra modes
        kernel.setArg(argIndex++, residualBuffer);                // search residuals
        kernel.setArg(argIndex++, sizeof(cl_mem), NULL);        // skip residuals
        kernel.setArg(argIndex++, sizeof(cl_mem), NULL);        // intra residuals
        kernel.setArg(argIndex++, srcImage);                    // intra src luma image
		kernel.setArg(argIndex++, srcImage);                    // intra src chroma image
        kernel.setArg(argIndex++, sizeof(int), &mbImageHeight);  // iterations
        kernel.setArg(argIndex++, sizeof(cl_uchar), &partitionMask);
        kernel.setArg(argIndex++, sizeof(cl_uchar), &sadAdjustment);
        kernel.setArg(argIndex++, sizeof(cl_uchar), &pixelMode);
        
        clInit->queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(PAD(width, 16),1, 1), cl::NDRange(16, 1, 1), NULL, &evt);
        
        evt.wait(); tpf[i] = evt.getProfilingInfo<CL_PROFILING_COMMAND_END>() - evt.getProfilingInfo<CL_PROFILING_COMMAND_START>();
        ndRangeTime += (tpf[i] / (double)10e6);

        ioStart = time_stamp();
        // Read back resulting motion vectors (in a sync way)
        void * pMVs = &MVs[i * mvImageWidth * mvImageHeight];

        clInit->queue.enqueueReadBuffer(mvBuffer, CL_TRUE, 0, sizeof(MotionVector)* mvImageWidth * mvImageHeight, pMVs, NULL, 0);

        void * pSADs = &SADs[i * mvImageWidth * mvImageHeight];
        clInit->queue.enqueueReadBuffer(residualBuffer, CL_TRUE, 0, sizeof(cl_ushort)* mvImageWidth * mvImageHeight, pSADs, NULL, 0);

        ioStat += (time_stamp() - ioStart);
    }
    double overallStat = time_stamp() - overallStart;
    std::cout << std::setiosflags(std::ios_base::fixed) << std::setprecision(3);
    std::cout << "Overall time for " << numPics << " frames " << overallStat << " sec\n";
    std::cout << "Average frame tile I/O time per frame " << 1000 * ioTileStat / count << " ms\n";
    std::cout << "Average frame file I/O time per frame " << 1000 * ioStat / count << " ms\n";
    std::cout << "Average Motion Estimation time per frame is " << ndRangeTime / count << " ms\n";

    ReleaseImage(currImage);
}

void ComputeCheckMotionVectorsFullFrameWithOpenCL(
    CLInit * clInit, Capture * pCapture, 
    std::vector<MotionVector> & searchMVs,
    std::vector<MotionVector> &skipMVs,
    std::vector<cl_ushort> &searchSADs,
    std::vector<cl_ushort> &skipSADs,
    std::vector<cl_uchar> &intraModes,
    std::vector<cl_ushort> &intraSADs,
    const CmdParserMV& cmd)
{
    cl::Kernel kernel(clInit->program, "block_advanced_motion_estimate_check_intel");

    cl_uchar partitionMask = 0;
    cl_uchar sadAdjustment = 0;
    cl_uchar pixelMode = 0;

    switch (kMBBlockType) {
    case CL_ME_MB_TYPE_16x16_INTEL: partitionMask = CL_AVC_ME_PARTITION_MASK_16x16_INTEL; break;
    case CL_ME_MB_TYPE_8x8_INTEL:   partitionMask = CL_AVC_ME_PARTITION_MASK_8x8_INTEL; break;
    case CL_ME_MB_TYPE_4x4_INTEL:   partitionMask = CL_AVC_ME_PARTITION_MASK_4x4_INTEL; break;
    default:                        partitionMask = CL_AVC_ME_PARTITION_MASK_ALL_INTEL; break;
    }
    switch (kSADAdjust) {
    case CL_ME_SAD_ADJUST_MODE_HAAR_INTEL:  sadAdjustment = CL_AVC_ME_SAD_ADJUST_MODE_HAAR_INTEL; break;
    case CL_ME_SAD_ADJUST_MODE_NONE_INTEL: default:  sadAdjustment = CL_AVC_ME_SAD_ADJUST_MODE_NONE_INTEL;
    }
    //partitionMask = CL_AVC_ME_PARTITION_MASK_16x16;
    switch (kPixelMode) {
    case CL_ME_SUBPIXEL_MODE_QPEL_INTEL:    pixelMode = CL_AVC_ME_SUBPIXEL_MODE_QPEL_INTEL; break;
    case CL_ME_SUBPIXEL_MODE_HPEL_INTEL:    pixelMode = CL_AVC_ME_SUBPIXEL_MODE_HPEL_INTEL; break;
    case CL_ME_SUBPIXEL_MODE_INTEGER_INTEL: pixelMode = CL_AVC_ME_SUBPIXEL_MODE_INTEGER_INTEL; break;
    }

    int numPics = pCapture->GetNumFrames();
    int width = cmd.width.getValue();
    int height = cmd.height.getValue();
    int mvImageWidth, mvImageHeight;
    int mbImageWidth, mbImageHeight;
    ComputeNumMVs(kMBBlockType,
        width, height,
        mvImageWidth, mvImageHeight,
        mbImageWidth, mbImageHeight);

    searchMVs.resize(numPics * mvImageWidth * mvImageHeight);
    skipSADs.resize(numPics * mvImageWidth * mvImageHeight * 8);
    searchSADs.resize(numPics * mvImageWidth * mvImageHeight);

    intraModes.resize(numPics * mbImageWidth * mbImageHeight * 22);
    intraSADs.resize(numPics * mbImageWidth * mbImageHeight * 4);

    // Set up OpenCL surfaces    
#if !DO_CHROMA_INTRA
	cl::ImageFormat imageFormat(CL_R, CL_UNORM_INT8);
	cl::Image2D srcImage(clInit->context, CL_MEM_READ_ONLY, imageFormat, width, height, 0, 0);
    cl::Image2D refImage(clInit->context, CL_MEM_READ_ONLY, imageFormat, width, height, 0, 0);
#else
	cl::ImageFormat imageFormatNV12(CL_NV12_INTEL, CL_UNORM_INT8);
	cl::Image2D srcImage(clInit->context, 
                         CL_MEM_READ_ONLY | CL_MEM_HOST_NO_ACCESS | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL,
                         imageFormatNV12, width, height);
	cl::Image2D refImage(clInit->context,
                         CL_MEM_READ_ONLY | CL_MEM_HOST_NO_ACCESS | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL,
                         imageFormatNV12, width, height);
	
	cl::Image2DYPlane srcYImage(clInit->context, CL_MEM_READ_ONLY, srcImage());
	cl::Image2DYPlane refYImage(clInit->context, CL_MEM_READ_ONLY, refImage());

	cl::Image2DUVPlane srcUVImage(clInit->context, CL_MEM_READ_ONLY, srcImage());
	cl::Image2DUVPlane refUVImage(clInit->context, CL_MEM_READ_ONLY, refImage());
#endif

    cl_short2 *countSkipMem = new cl_short2[mbImageWidth * mbImageHeight];
    for (int i = 0; i < mbImageWidth * mbImageHeight; i++)
    {
#if ONLY_INTRA
	countSkipMem[i].s[0] = 0;
	countSkipMem[i].s[1] = 0;
#else
#if ONLY_SKIP
        countSkipMem[i].s[0] = 0;
#else
        countSkipMem[i].s[0] = kCount;
#endif
        countSkipMem[i].s[1] = kCount;
#endif
    }

    cl::Buffer countSkipBuffer(
        clInit->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        mbImageWidth * mbImageHeight * sizeof(cl_short2), countSkipMem, NULL);

    cl_short2 *predMem = new cl_short2[mbImageWidth * mbImageHeight * 8];
    for (int i = 0; i < mbImageWidth * mbImageHeight; i++)
    {
        for (int j = 0; j < 1; j++)
        {
            predMem[i * 8 + j].s[0] = 0;
            predMem[i * 8 + j].s[1] = 0;
        }
        for (int j = 1; j < 2; j++)
        {
            predMem[i * 8 + j].s[0] = kPredictorX0;
            predMem[i * 8 + j].s[1] = kPredictorY0;
        }
        for (int j = 2; j < 3; j++)
        {
            predMem[i * 8 + j].s[0] = -kPredictorX0;
            predMem[i * 8 + j].s[1] = kPredictorY0;
        }
        for (int j = 3; j < 4; j++)
        {
            predMem[i * 8 + j].s[0] = kPredictorX0;
            predMem[i * 8 + j].s[1] = -kPredictorY0;
        }
        for (int j = 4; j < 5; j++)
        {
            predMem[i * 8 + j].s[0] = -kPredictorX0;
            predMem[i * 8 + j].s[1] = -kPredictorY0;
        }
        for (int j = 5; j < 8; j++)
        {
            predMem[i * 8 + j].s[0] = kPredictorX1;
            predMem[i * 8 + j].s[1] = kPredictorY1;
        }
    }

    cl::Buffer predBuffer(
        clInit->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        mbImageWidth * mbImageHeight * 8 * sizeof(cl_short2), predMem, NULL);
    cl::Buffer searchMVBuffer(
        clInit->context, CL_MEM_WRITE_ONLY,
        mvImageWidth * mvImageHeight * sizeof(MotionVector));
    cl::Buffer searchResidualBuffer(
        clInit->context, CL_MEM_WRITE_ONLY,
        mvImageWidth * mvImageHeight * sizeof(cl_ushort));

    cl::Buffer skipResidualBuffer(
        clInit->context, CL_MEM_WRITE_ONLY,
        mvImageWidth * mvImageHeight * 8 * sizeof(cl_ushort));

    cl::Buffer intraModeBuffer(
        clInit->context, CL_MEM_WRITE_ONLY,
        mbImageWidth * mbImageHeight * 22 * sizeof(cl_uchar));
    cl::Buffer intraResidualBuffer(
        clInit->context, CL_MEM_WRITE_ONLY,
        mbImageWidth * mbImageHeight * 4 * sizeof(cl_ushort));

    cl_short2 *skipMVMem = new cl_short2[mvImageWidth * mvImageHeight * 8];

    // Bootstrap video sequence reading
    PlanarImage * currImage = CreatePlanarImage(width, height);

    pCapture->GetSample(0, currImage);

    cl::size_t<3> origin, region;
    SET(origin, 0, 0, 0);
    SET(region, width, height, 1);

    // Copy to tiled image memory - this copy (and its overhead) is not necessary in a full GPU pipeline   
#if !DO_CHROMA_INTRA
    clInit->queue.enqueueWriteImage(srcImage, CL_TRUE, origin, region, currImage->PitchY, 0, currImage->Y, NULL, 0);
#else
	WriteYUVImageToOCLNV12(srcImage, srcYImage, srcUVImage, clInit->context, clInit->queue, currImage);
#endif

    // Process all frames
    double ioTileStat = 0;
    double ioStat = 0;//file i/o
#if DO_INTRA
#if !DO_CHROMA_INTRA
    unsigned flags = 0x2;
#else
	unsigned flags = 0x3;
#endif
#else
    unsigned flags = 0x0;
#endif
    unsigned skipBlockType = kMBBlockType;
    unsigned costPenalty = kCostPenalty;
    unsigned costPrecision = kCostPrecision;

    double overallStart = time_stamp();
    cl::Event evt;
    vector<double> tpf(numPics);
    double ndRangeTime = 0;

    // First frame is already in srcImg, so we start with the second frame
    for (int i = 1; i < numPics; i++)
    {
#if !ONLY_INTRA
        for (int j = 0; j < mbImageWidth * mbImageHeight; j++)
        {
            unsigned offset = mvImageWidth * mvImageHeight;
            for (cl_uint k = 0; k < kCount; k++)
            {
                int numComponents = kMBBlockType ? 4 : 1;
                for (int l = 0; l < numComponents; l++)
                {
                    skipMVMem[j * 8 * numComponents + k * numComponents + l].s[0] =
                        skipMVs[i * offset + j * numComponents + l].s[0] + 0;
                    skipMVMem[j * 8 * numComponents + k * numComponents + l].s[1] =
                        skipMVs[i * offset + j * numComponents + l].s[1] + 0;
                }
            }
        }
#endif

        cl::Buffer skipMVBuffer(
            clInit->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
            mvImageWidth * mvImageHeight * 8 * sizeof(cl_short2), skipMVMem, NULL);

        // Load next picture

        double ioStart = time_stamp();

        pCapture->GetSample(i, currImage);

        std::swap(refImage, srcImage);

#if DO_CHROMA_INTRA
		std::swap(refYImage, srcYImage);
		std::swap(refUVImage, srcUVImage);
#endif

        double ioTileStart = time_stamp();

        // Copy to tiled image memory - this copy (and its overhead) is not necessary in a full GPU pipeline
#if !DO_CHROMA_INTRA
        // Copy to tiled image memory - this copy (and its overhead) is not necessary in a full GPU pipeline
        clInit->queue.enqueueWriteImage(srcImage, CL_TRUE, origin, region, currImage->PitchY, 0, currImage->Y, NULL, 0);
#else		
		WriteYUVImageToOCLNV12(srcImage, srcYImage, srcUVImage, clInit->context, clInit->queue, currImage);
#endif
		ioTileStat += (time_stamp() - ioTileStart);
        ioStat += (time_stamp() - ioStart);

        // Schedule full-frame motion estimation
		int argIndex = 0;
        kernel.setArg(argIndex++, srcImage);                            // src image
        kernel.setArg(argIndex++, refImage);                            // ref image
        kernel.setArg(argIndex++, sizeof(unsigned), &flags);            // flags
        kernel.setArg(argIndex++, sizeof(unsigned), &skipBlockType);    // skip block type
        kernel.setArg(argIndex++, sizeof(unsigned), &costPenalty);      // cost penalty
        kernel.setArg(argIndex++, sizeof(unsigned), &costPrecision);    // cost precision
        kernel.setArg(argIndex++, countSkipBuffer);
        kernel.setArg(argIndex++, predBuffer);                  // no predictors

        kernel.setArg(argIndex++, skipMVBuffer);                // use fbr result as skip check i/p
        kernel.setArg(argIndex++, searchMVBuffer);              // search mvs
        kernel.setArg(argIndex++, intraModeBuffer);             // intra modes
        kernel.setArg(argIndex++, searchResidualBuffer);        // search residuals
        kernel.setArg(argIndex++, skipResidualBuffer);          // skip residuals
        kernel.setArg(argIndex++, intraResidualBuffer);         // intra residuals
#if !DO_CHROMA_INTRA
		kernel.setArg(argIndex++, srcImage);                    // intra src luma image
		kernel.setArg(argIndex++, srcImage);                    // intra src luma image
#else
        kernel.setArg(argIndex++, srcYImage);                    // intra src luma image
        kernel.setArg(argIndex++, srcUVImage);                   // intra src chroma image
#endif
        kernel.setArg(argIndex++, sizeof(int), &mbImageHeight); // iterations
        kernel.setArg(argIndex++, sizeof(cl_uchar), &partitionMask);
        kernel.setArg(argIndex++, sizeof(cl_uchar), &sadAdjustment);
        kernel.setArg(argIndex++, sizeof(cl_uchar), &pixelMode);

        clInit->queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(PAD(width, 16),1, 1), cl::NDRange(16, 1, 1), NULL, &evt);

        evt.wait(); tpf[i] = evt.getProfilingInfo<CL_PROFILING_COMMAND_END>() - evt.getProfilingInfo<CL_PROFILING_COMMAND_START>();
        ndRangeTime += (tpf[i] / (double)10e6);

        ioStart = time_stamp();

        // Read back resulting MVs (in a sync way)  
        void * pSearchMVs = &searchMVs[i * mvImageWidth * mvImageHeight];
        clInit->queue.enqueueReadBuffer(searchMVBuffer, CL_TRUE, 0, sizeof(MotionVector)* mvImageWidth * mvImageHeight, pSearchMVs, 0, 0);

        // Read back resulting SADs (in a sync way)       
        void * pSearchSADs = &searchSADs[i * mvImageWidth * mvImageHeight];
        clInit->queue.enqueueReadBuffer(searchResidualBuffer, CL_TRUE, 0, sizeof(cl_ushort)* mvImageWidth * mvImageHeight, pSearchSADs, 0, 0);

        // Read back resulting SADs (in a sync way)       
        void * pSkipSADs = &skipSADs[i * mvImageWidth * mvImageHeight * 8];
        clInit->queue.enqueueReadBuffer(skipResidualBuffer, CL_TRUE, 0, sizeof(cl_ushort)* mvImageWidth * mvImageHeight * 8, pSkipSADs, 0, 0);

        // Read back resulting intra modes (in a sync way)  
        void * pIntraModes = &intraModes[i * mbImageWidth * mbImageHeight * 22];
        clInit->queue.enqueueReadBuffer(intraModeBuffer, CL_TRUE, 0, sizeof(cl_uchar)* 22 * mbImageWidth * mbImageHeight, pIntraModes, 0, 0);

        // Read back resulting intra SADs (in a sync way)  
        void * pIntraSADs = &intraSADs[i * mbImageWidth * mbImageHeight * 4];
        clInit->queue.enqueueReadBuffer(intraResidualBuffer, CL_TRUE, 0, sizeof(cl_ushort)* 4 * mbImageWidth * mbImageHeight, pIntraSADs, 0, 0);

        ioStat += (time_stamp() - ioStart);
    }
    double overallStat = time_stamp() - overallStart;
    std::cout << std::setiosflags(std::ios_base::fixed) << std::setprecision(3);
    int count = numPics - 1;
    std::cout << "Overall time for " << numPics << " frames " << overallStat << " sec\n";
    std::cout << "Average frame tile I/O time per frame " << 1000 * ioTileStat / count << " ms\n";
    std::cout << "Average frame file I/O time per frame " << 1000 * ioStat / count << " ms\n";
    std::cout << "Average Motion Estimation time per frame is " << ndRangeTime / count << " ms\n";

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

    if (x<0 || x >= nPicWidth || y<0 || y >= nPicHeight)
        return;         // Don't draw out of bound pixels
    nPixPos = y * nPicWidth + x;
    *(pPic + nPixPos) = u8Pixel;
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

void OverlayVectors(
    unsigned int subBlockSize,
    const MotionVector* pMV, PlanarImage* srcImage,
    int mbImageWidth, int mbImageHeight,
    int width, int height)
{
    const int nHalfBlkSize = subBlockSize / 2;

    int subBlockHeight = 16 / subBlockSize;
    int subBlockWidth = 16 / subBlockSize;

    for (int i = 0; i < mbImageHeight; i++)
    {
        for (int j = 0; j < mbImageWidth; j++)
        {
            for (int l = 0; l < subBlockHeight; l++)
            {
                for (int m = 0; m < subBlockWidth; m++)
                {
                    DrawLine(
                        j * 16 + m*subBlockSize + nHalfBlkSize,
                        i * 16 + l*subBlockSize + nHalfBlkSize,
                        (pMV[(j + i*mbImageWidth)*subBlockWidth*subBlockHeight + l*subBlockWidth + m].s[0] + 2) >> 2,
                        (pMV[(j + i*mbImageWidth)*subBlockWidth*subBlockHeight + l*subBlockWidth + m].s[1] + 2) >> 2,
                        srcImage->Y, width, height, 200);
                }
            }
        }
    }
}

void PrintIntraModes( std::vector<cl_uchar>& intraModes, int width, int height )
{
	std::ofstream file;
    file.open("intra_output.txt");
    unsigned numMBs = DIV(width, 16)*DIV(height, 16);
    for (unsigned i = 22 * numMBs; i < intraModes.size(); i += 22)
    {
        file << unsigned(intraModes[i + 0]) << std::endl;
        assert(unsigned(intraModes[i + 0]) < 9);
        for (unsigned j = 1; j < 5; j++)
        {
            assert(unsigned(intraModes[i + j]) < 9);
            file << unsigned(intraModes[i + j]) << " ";
        }
        file << std::endl;
        for (unsigned j = 5; j < 21; j++)
        {
            assert(unsigned(intraModes[i + j]) < 9);
            file << unsigned(intraModes[i + j]) << " ";
        }
        file << std::endl;
		//file << unsigned(0) << std::endl;
		file << unsigned(intraModes[i + 21]) << std::endl;
		assert(unsigned(intraModes[i + 21]) < 4);
    }
    file.close();
}

void PrintIntraDists( std::vector<cl_ushort>& intraDists, int width, int height )
{
	std::ofstream file;
    file.open("intra_dists_output.txt");
    unsigned numMBs = DIV(width, 16)*DIV(height, 16);
    for (unsigned i = 4 * numMBs; i < intraDists.size(); i += 4)
    {
        file << unsigned(intraDists[i + 0]) << ", ";
		file << unsigned(intraDists[i + 1]) << ", ";
		file << unsigned(intraDists[i + 2]) << ", ";
		file << unsigned(intraDists[i + 3]);
        file << std::endl;
    }
    file.close();
}

int main(int argc, const char** argv)
{
    try
    {
        CmdParserMV cmd(argc, argv);
        cmd.parse();

        // Immediatly exit if user wanted to see the usage information only.
        if (cmd.help.isSet())
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

        bool differs = false;

        // Process sequence
        std::cout << "Processing " << pCapture->GetNumFrames() << " frames ..." << std::endl;
        std::vector<MotionVector> MVs1;
        std::vector<cl_ushort> SADs;
        CLInit *clInit = new CLInit();

#if ONLY_INTRA
		std::vector<MotionVector> MVs2;
        std::vector<cl_ushort> searchSADs;
        std::vector<cl_ushort> skipSADs;
        std::vector<cl_uchar> intraModes;
        std::vector<cl_ushort> intraSADs;
        ComputeCheckMotionVectorsFullFrameWithOpenCL(
            clInit, pCapture, MVs2, MVs1, searchSADs, skipSADs, intraModes, intraSADs, cmd);
#if PRINT_INTRA_MODES
        PrintIntraModes( intraModes, width, height );
		PrintIntraDists( intraSADs, width, height );
#endif
#else
        ExtractMotionVectorsFullFrameWithOpenCL(clInit, pCapture, MVs1, SADs, cmd);
        if (kMBBlockType < CL_ME_MB_TYPE_4x4_INTEL)
        {
            std::vector<MotionVector> MVs2;
            std::vector<cl_ushort> searchSADs;
            std::vector<cl_ushort> skipSADs;
            std::vector<cl_uchar> intraModes;
            std::vector<cl_ushort> intraSADs;
            ComputeCheckMotionVectorsFullFrameWithOpenCL(
                clInit, pCapture, MVs2, MVs1, searchSADs, skipSADs, intraModes, intraSADs, cmd);
#if !ONLY_SKIP
            for (unsigned i = 0; i < MVs1.size(); i++)
            {
                differs = differs || MVs2[i].s[0] != MVs1[i].s[0];
                differs = differs || MVs2[i].s[1] != MVs1[i].s[1];
                differs = differs || SADs[i] != searchSADs[i];
            }
#endif    
            unsigned mbCount = unsigned(SADs.size() / 4);
            for (unsigned i = 0; i < mbCount; i++)
            {
                for (unsigned k = 0; k < kCount; k++)
                {
                    unsigned numComponents = kMBBlockType ? 4 : 1;
                    for (unsigned l = 0; l < numComponents; l++)
                    {
                        if (SADs[i * numComponents + l] ||
                            skipSADs[i * 8 * numComponents + k * numComponents + l])
                        {
                            if (SADs[i * numComponents + l] !=
                                skipSADs[i * 8 * numComponents + k * numComponents + l])
                            {
                                differs = 1;
                            }
                        }
                    }
                }
            }

            if (!differs)
            {
                std::cout << "Skip test PASSED!\n";
            }
            else if (kCostPenalty)
            {
                std::cout << "Skip test FAILED with cost penalty used.\n";
            }
            else
            {
                std::cout << "Skip test FAILED!\n";
            }
#if PRINT_INTRA_MODES
            PrintIntraModes( intraModes, width, height );
			PrintIntraDists( intraSADs, width, height );
#endif
        }
        else {
            std::cout << "Skip test not enabled for 4x4 Block Size !\n";
        }
#endif

        // Generate sequence with overlaid motion vectors
        FrameWriter * pWriter = FrameWriter::CreateFrameWriter(width, height, pCapture->GetNumFrames(), cmd.out_to_bmp.getValue());
        PlanarImage * srcImage = CreatePlanarImage(width, height);

        int mvImageWidth, mvImageHeight;
        int mbImageWidth, mbImageHeight;
        ComputeNumMVs(kMBBlockType,
            width, height,
            mvImageWidth, mvImageHeight,
            mbImageWidth, mbImageHeight);
        unsigned int subBlockSize = ComputeSubBlockSize(kMBBlockType);

#if !ONLY_INTRA
        for (int k = 0; k < pCapture->GetNumFrames(); k++)
        {
            pCapture->GetSample(k, srcImage);
            // Overlay MVs on Src picture, except the very first one
            if (k>0)
                OverlayVectors(subBlockSize, &MVs1[k*mvImageWidth*mvImageHeight], srcImage, mbImageWidth, mbImageHeight, width, height);
            pWriter->AppendFrame(srcImage);
        }

        std::cout << "Writing " << pCapture->GetNumFrames() << " frames to " << cmd.overlayFileName.getValue() << "..." << std::endl;
        pWriter->WriteToFile(cmd.overlayFileName.getValue().c_str());
#endif
        FrameWriter::Release(pWriter);
        Capture::Release(pCapture);
        ReleaseImage(srcImage);
    }
    catch (cl::Error & err)
    {
        std::cout << err.what() << "(" << err.err() << ")" << std::endl;
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
