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

#define PAD(X,SIZE) (((X + (SIZE - 1))/ SIZE ) * SIZE)

using namespace YUVUtils;

// these values define dimensions of input pixel blocks (whcih are fixed in hardware)
// so, do not change these values to avoid errors
#define SRC_BLOCK_WIDTH 16
#define SRC_BLOCK_HEIGHT 16

typedef cl_short2 MotionVector;

// Specifies number of motion vectors per source pixel block (the value of CL_ME_MB_TYPE_16x16_INTEL specifies  just a single vector per block )
static const cl_uint kMBBlockType = CL_ME_MB_TYPE_4x4_INTEL;
static const cl_uint kMSubPixelMode = CL_ME_SUBPIXEL_MODE_QPEL_INTEL;
static const cl_uint kMSadAdjustMode = CL_ME_SAD_ADJUST_MODE_NONE_INTEL;
static const cl_uint kMSearchPathRadius = CL_ME_SEARCH_PATH_RADIUS_16_12_INTEL;

#ifdef _MSC_VER
#pragma warning (push)
#pragma warning (disable : 4355)    // 'this': used in base member initializer list
#endif


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
        fileName(*this,          0,"input", "string", "Input video sequence filename (.yuv file format)","video_1920x1080_5frames.yuv"),
        overlayFileName(*this,   0,"output","string", "Output video sequence with overlaid motion vectors filename ","output.yuv"),
        width(*this,             0, "width",    "<integer>", "Frame width for the input file", 1920),
        height(*this,            0, "height","<integer>", "Frame height for the input file",1080),
        frames(*this,            0, "frames", "<integer>", "Number of frame to use for motion estimation -- 0 represents entire video", 0)
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

void ExtractMotionVectorsFullFrameWithOpenCL( 
    Capture * pCapture, std::vector<MotionVector> & MVs, std::vector<cl_ushort> & SADs, std::vector<cl_uchar2> & Shapes, const CmdParserMV& cmd)
{

    // OpenCL initialization
    OpenCLBasic init("Intel", "GPU");
    //OpenCLBasic creates the platform/context and device for us, so all we need is to get an ownership (via incrementing ref counters with clRetainXXX)

    cl::Context context = cl::Context(init.context); clRetainContext(init.context);
    cl::Device device  = cl::Device(init.device);   clRetainDevice(init.device);
    cl::CommandQueue queue = cl::CommandQueue(init.queue);clRetainCommandQueue(init.queue);

    std::string ext = device.getInfo< CL_DEVICE_EXTENSIONS >();
    if (string::npos == ext.find("cl_intel_device_side_avc_motion_estimation"))
    {
        printf("WARNING: The selected device doesn't support offically device-side motion estimation!");
    }

    char* programSource = NULL;
    if( LoadSourceFromFile( "vme_basic.cl", programSource ) )
    // Load the kernel source from the passed in file.
    {
        printf("Error: Couldn't load kernel source from file.\n" );
    }    

    // Create a built-in VME kernel
    cl_int err = 0;
    const cl_device_id & d = device();    
    cl::Program p(clCreateProgramWithSource(context(),1,( const char** )&programSource,NULL,&err));

    err = clBuildProgram(p(), 1, &d, "", NULL, NULL);

     size_t  buildLogSize = 0;
    clGetProgramBuildInfo(p(),d,CL_PROGRAM_BUILD_LOG,0,NULL,&buildLogSize );

    cl_char*    buildLog = new cl_char[ buildLogSize ];
    if( buildLog )
    {
        clGetProgramBuildInfo(p(),d,CL_PROGRAM_BUILD_LOG,buildLogSize,buildLog,NULL );

        std::cout << ">>> Build Log:\n";
        std::cout << buildLog;;
        std::cout << ">>>End of Build Log\n";
    }


    cl::Kernel kernel(p, "block_motion_estimate_intel");

    // VME API configuration knobs
    cl_motion_estimation_desc_intel desc = {
        kMBBlockType,                                     // Number of motion vectors per source pixel block (the value of CL_ME_MB_TYPE_16x16_INTEL specifies  just a single vector per block )
        kMSubPixelMode,                                      // Motion vector precision
        kMSadAdjustMode,                                  // SAD Adjust (none/Haar transform) for the residuals, but we don't compute them in this tutorial anyway
        kMSearchPathRadius                                  // Search window radius
    };

    const int numPics = pCapture->GetNumFrames();
    const int width = cmd.width.getValue();
    const int height = cmd.height.getValue();
    int mvImageWidth, mvImageHeight;
    int mbImageWidth, mbImageHeight;
    ComputeNumMVs(desc.mb_block_type, width, height, mvImageWidth, mvImageHeight, mbImageWidth, mbImageHeight);
    MVs.resize(numPics * mvImageWidth * mvImageHeight);
    SADs.resize(numPics * mvImageWidth * mvImageHeight);
    Shapes.resize(numPics * mbImageWidth * mbImageHeight);

    std::cout << "mvImageWidth=" << mvImageWidth << std::endl;
    std::cout << "mvImageHeight=" << mvImageHeight << std::endl;

    // Set up OpenCL surfaces
    cl::ImageFormat imageFormat(CL_R, CL_UNORM_INT8);
    cl::Image2D refImage(context, CL_MEM_READ_ONLY, imageFormat, width, height, 0, 0);
    cl::Image2D srcImage(context, CL_MEM_READ_ONLY, imageFormat, width, height, 0, 0);
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

    // Process all frames
    double ioStat = 0;//file i/o
    double meStat = 0;//motion estimation itself

    double overallStart  = time_stamp();
    // First frame is already in srcImg, so we start with the second frame
    for (int i = 1; i < numPics; i++)
    {
        double ioStart = time_stamp();
        // Load next picture
        pCapture->GetSample(i, currImage);

        std::swap(refImage, srcImage);
        // Copy to tiled image memory - this copy (and its overhead) is not necessary in a full GPU pipeline
        queue.enqueueWriteImage(srcImage, CL_TRUE, origin, region, currImage->PitchY, 0, currImage->Y);
        ioStat += (time_stamp() -ioStart);

        double meStart = time_stamp();
        // Schedule full-frame motion estimation
        kernel.setArg(0, srcImage);
        kernel.setArg(1, refImage);
        kernel.setArg(2, predBuffer);
        kernel.setArg(3, mvBuffer);
        kernel.setArg(4, sad);
        kernel.setArg(5, ShapeBuffer);
        kernel.setArg(6, sizeof(cl_int), &mbImageHeight);
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
        std::vector<MotionVector> MVs;
        std::vector<cl_ushort> SADs;
        std::vector<cl_uchar2> Shapes;
        ExtractMotionVectorsFullFrameWithOpenCL(pCapture, MVs, SADs, Shapes, cmd);
#if 0
        for (int i = 8160*16; i < 8176*16; i++ )
        {
            std::cout << "(" << MVs[i].s[0] << "," << MVs[i].s[1] << ") --- " << SADs[i] << std::endl;

        }
#endif
        // Generate sequence with overlaid motion vectors
        FrameWriter * pWriter = FrameWriter::CreateFrameWriter(width, height, pCapture->GetNumFrames(), cmd.out_to_bmp.getValue());
        PlanarImage * srcImage = CreatePlanarImage(width, height);

        int mvImageWidth, mvImageHeight;
        int mbImageWidth, mbImageHeight;
        ComputeNumMVs(kMBBlockType, width, height, mvImageWidth, mvImageHeight, mbImageWidth, mbImageHeight);
        unsigned int subBlockSize = ComputeSubBlockSize(kMBBlockType);

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
