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

#define BIDIR_PRED   1  // 1 = Bidirectional Motion Estimation and Skip Check, 0 = only forward ME and SKC

#define NO_SKP_CHK 0   // Allows all major and minor partition sizes, no skip check performed
#define SKP_CHK_8  1   // Allows only 8x8 major partitions, skip check performed
#define SKP_CHK_16 2   // Allows only 16x16 major partitions, skip check performed

#define SHOW_BLOCKS  0

#include <iostream>
#include <vector>
#include <sstream>
#include <fstream>
#include <algorithm>

#include <CL/cl.hpp>
#include <CL/cl_ext.h>
#include <CL/cl_intel_device_side_avc_motion_estimation.h>

#include "../common/yuv_utils.h"
#include "../common/cmdparser.hpp"
#include "../common/oclobject.hpp"

#ifdef __linux
void fopen_s(FILE **f, const char *name, const char *mode) {
  assert(f);
  *f = fopen(name, mode);
}
#endif

using namespace YUVUtils;

const cl_short4 SILVER = { 181, 128, 128, 255 };
const cl_short4 RED    = { 76,   84, 255, 255 };    
const cl_short4 YELLOW = { 255,   0, 148, 255 };
const cl_short4 BLUE   = { 146, 189,  23, 255 };
const cl_short4 GREEN  = { 117,  61,  44, 255 };

// these values define dimensions of input pixel blocks (which are fixed in hardware)
// so, do not change these values to avoid errors
#define SRC_BLOCK_WIDTH 16
#define SRC_BLOCK_HEIGHT 16

typedef cl_short2 MotionVector;
typedef cl_uint2  BMotionVector;

// BMotionVector is cl_uint2
// cl_uint2 bmv :   bmv.s[0] = fwd mv, lower 32 bits, bmv.s[1] = bwd mv, upper 32 bits
// cl_ushort* ptr = (cl_short*)& ptr :  ptr[0] = fwd mv.s[0] (X coord), ptr[1] = fwd mv.s[1] (Y); ptr[2] = bwd mv.s[0] (X), ptr[3] = bwd mv.s[1] (Y)

#ifndef CL_AVC_ME_COST_PENALTY_NONE_INTEL
#define CL_AVC_ME_COST_PENALTY_NONE_INTEL                   0x0
#define CL_AVC_ME_COST_PENALTY_LOW_INTEL                    0x1
#define CL_AVC_ME_COST_PENALTY_NORMAL_INTEL                 0x2
#define CL_AVC_ME_COST_PENALTY_HIGH_INTEL                   0x3
#endif

static const cl_uint skp_check_type = SKP_CHK_8;

static cl_uchar intraPartMask =  CL_AVC_ME_INTRA_LUMA_PARTITION_MASK_16x16_INTEL & CL_AVC_ME_INTRA_LUMA_PARTITION_MASK_8x8_INTEL;  

static const cl_uint kCostPenalty = CL_AVC_ME_COST_PENALTY_NORMAL_INTEL;
static const cl_uint kCostPrecision = CL_AVC_ME_COST_PRECISION_HPEL_INTEL;

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
    CmdOption<std::string>         fileName;
    CmdOption<std::string>         overlayFileName;
    CmdOption<int>		width;
    CmdOption<int>      height;
	CmdOption<int>      frames;

    CmdOption<bool>		help;
    CmdOption<bool>		out_to_bmp;

    CmdParserMV  (int argc, const char** argv) :
    CmdParser(argc, argv),
        out_to_bmp(*this,		'b',"nobmp","","Do not output frames to the sequence of bmp files (in addition to the yuv file), by default the output is off", false, "nobmp"),
        help(*this,				'h',"help","","Show this help text and exit."),

#if USE_HD
        fileName(*this,			0,"input", "string", "Input video sequence filename (.yuv file format)","../BasketballDrive_1920x1080_15.yuv"),
		overlayFileName(*this,	0,"output","string", "Output video sequence with overlaid motion vectors filename ","BasketballDrive_1920x1080_15_output.yuv"),
        width(*this,			0, "width",	"<integer>", "Frame width for the input file", 1920),
        height(*this,			0, "height","<integer>", "Frame height for the input file", 1080),
		frames(*this,			15, "frames", "<integer>", "Number of frame to use for motion estimation -- 0 represents entire video", 15)
#elif USE_SD
		fileName(*this,			0,"input", "string", "Input video sequence filename (.yuv file format)","../goal_1280x720.yuv"),
        overlayFileName(*this,	0,"output","string", "Output video sequence with overlaid motion vectors filename ","goal_1280x720_output.yuv"),
        width(*this,			0, "width",	"<integer>", "Frame width for the input file", 1280),
        height(*this,			0, "height","<integer>", "Frame height for the input file", 720),
		frames(*this,			0, "frames", "<integer>", "Number of frame to use for motion estimation -- 0 represents entire video", 0)
#else
		fileName(*this,			0,"input", "string", "Input video sequence filename (.yuv file format)","../boat_qcif_176x144.yuv"),
        overlayFileName(*this,	0,"output","string", "Output video sequence with overlaid motion vectors filename ","boat_qcif_176x144_output.yuv"),
        width(*this,			0, "width",	"<integer>", "Frame width for the input file", 176),
        height(*this,			0, "height","<integer>", "Frame height for the input file", 144),
		frames(*this,			50, "frames", "<integer>", "Number of frame to use for motion estimation -- 0 represents entire video", 50)
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

inline void ComputeNumMVs( cl_uint nMBType, 
						   int nPicWidth, int nPicHeight, 
						   int & nMVSurfWidth, int & nMVSurfHeight, 
						   int & nMBSurfWidth, int & nMBSurfHeight )
{
    // Size of the input frame in pixel blocks (SRC_BLOCK_WIDTH x SRC_BLOCK_HEIGHT each)
    int nPicWidthInBlk  = (nPicWidth + SRC_BLOCK_WIDTH - 1) / SRC_BLOCK_WIDTH;
    int nPicHeightInBlk = (nPicHeight + SRC_BLOCK_HEIGHT - 1) / SRC_BLOCK_HEIGHT;

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

void FillMV(	
	std::vector<BMotionVector> &MVs, 	
	std::vector<cl_uchar2> &Shapes,
	std::vector<cl_uchar>  &Dirs, 
	int width,
	int height,
    const CmdParserMV& cmd,
	std::vector<cl_char>  &modes,
	std::vector<cl_uchar>  &blksizes)
{
	int mvImageWidth, mvImageHeight;
	int mbImageWidth, mbImageHeight;

    ComputeNumMVs(CL_ME_MB_TYPE_4x4_INTEL, width, height, mvImageWidth, mvImageHeight, mbImageWidth, mbImageHeight);

	// Assumes MVs, Shapes, Dirs already allocated 
	// Fills MVs, Shapes, Dirs for Frame 0 only
    
	int Arrow = 16;

	for( int i = 0; i < mbImageWidth * mbImageHeight; i++ )
    {		
		Dirs[i] = 0;  // 0 = fwd dir
			
		switch (blksizes[i])
		{
		 case 0:  
		 {
          assert(unsigned(modes[i*22 + 0]) < 4);

		  cl_short* ptr = (cl_short*)&MVs[i*16];
		  ptr[2]= 0;   ptr[3] = 0; 

		  Shapes[i].s[0] = 0;

          switch (modes[i * 22 + 0]) 
		    {
			case CL_AVC_ME_LUMA_PREDICTOR_MODE_VERTICAL_INTEL:
				  ptr[0]= 0;   ptr[1] = -Arrow;       break;			
			case CL_AVC_ME_LUMA_PREDICTOR_MODE_HORIZONTAL_INTEL:
				  ptr[0]= Arrow;   ptr[1] = 0;        break;				
			case CL_AVC_ME_LUMA_PREDICTOR_MODE_DC_INTEL:
				  ptr[0]= 0;       ptr[1] = 0;         break;				
			case CL_AVC_ME_LUMA_PREDICTOR_MODE_PLANE_INTEL:
				  ptr[0]= Arrow;   ptr[1] = -Arrow;    break;
				
			default: break;
		    }
		  break;
		 }
		  case 1:
	     { 
            Shapes[i].s[0] = 3;
			Shapes[i].s[1] = 0;

            for(int j=0;j<4;j++)
			{
			  assert(unsigned(modes[i * 22 + j + 1]) < 9);

			  cl_short* ptr = (cl_short*)&MVs[i*16 + j*4];
		      ptr[2]= 0;   ptr[3] = 0; 

			  switch (modes[i * 22 + j + 1]) 
		      {
                case CL_AVC_ME_LUMA_PREDICTOR_MODE_VERTICAL_INTEL:
				    ptr[0] = 0;       ptr[1] = -Arrow;            break;				
				case CL_AVC_ME_LUMA_PREDICTOR_MODE_HORIZONTAL_INTEL:
					ptr[0] = Arrow;   ptr[1] = 0;                 break;					
				case CL_AVC_ME_LUMA_PREDICTOR_MODE_DC_INTEL:
					ptr[0] = 0;       ptr[1] = 0;                 break;					
				case CL_AVC_ME_LUMA_PREDICTOR_MODE_DIAGONAL_DOWN_LEFT_INTEL:
					ptr[0] = -Arrow;       ptr[1] = -Arrow;       break;					
				case CL_AVC_ME_LUMA_PREDICTOR_MODE_DIAGONAL_DOWN_RIGHT_INTEL:
					ptr[0] = Arrow;        ptr[1] = -Arrow;       break;					
				case CL_AVC_ME_LUMA_PREDICTOR_MODE_VERTICAL_RIGHT_INTEL:
					ptr[0] = Arrow/2;      ptr[1] = -Arrow;       break;					
				case CL_AVC_ME_LUMA_PREDICTOR_MODE_HORIZONTAL_DOWN_INTEL:
					ptr[0] = Arrow;        ptr[1] = -Arrow/2;       break;					
				case CL_AVC_ME_LUMA_PREDICTOR_MODE_VERTICAL_LEFT_INTEL:
					ptr[0] = -Arrow/2;     ptr[1] = -Arrow;       break;					
				case CL_AVC_ME_LUMA_PREDICTOR_MODE_HORIZONTAL_UP_INTEL:
					ptr[0] = Arrow;        ptr[1] = Arrow/4;       break;
					
				default: break;
			  }
			}

			break;
		  }
	
		  case 2:
	      {
			Shapes[i].s[0] = 3;
			Shapes[i].s[1] = 3;

            for(int j=0;j<16;j++)
			 {
             
			  assert(unsigned(modes[i * 22 + j + 5]) < 9);

			  cl_short* ptr = (cl_short*)&MVs[i*16 + j];
		      ptr[2]= 0;   ptr[3] = 0; 

			  switch (modes[i * 22 + j + 5]) 
		      {
                case CL_AVC_ME_LUMA_PREDICTOR_MODE_VERTICAL_INTEL:
					  ptr[0] = 0;       ptr[1] = -Arrow;            break;					
				case CL_AVC_ME_LUMA_PREDICTOR_MODE_HORIZONTAL_INTEL:
					  ptr[0] = Arrow;   ptr[1] = 0;                 break;					
				case CL_AVC_ME_LUMA_PREDICTOR_MODE_DC_INTEL:
					  ptr[0] = 0;       ptr[1] = 0;                 break;					
				case CL_AVC_ME_LUMA_PREDICTOR_MODE_DIAGONAL_DOWN_LEFT_INTEL:
					 ptr[0] = -Arrow;       ptr[1] = -Arrow;       break;					
				case CL_AVC_ME_LUMA_PREDICTOR_MODE_DIAGONAL_DOWN_RIGHT_INTEL:
					 ptr[0] =  Arrow;       ptr[1] = -Arrow;       break;					
				case CL_AVC_ME_LUMA_PREDICTOR_MODE_VERTICAL_RIGHT_INTEL:
					 ptr[0] = Arrow/2;       ptr[1] = -Arrow;       break;					
				case CL_AVC_ME_LUMA_PREDICTOR_MODE_HORIZONTAL_DOWN_INTEL:
					 ptr[0] = Arrow;       ptr[1] = -Arrow/2;       break;					
				case CL_AVC_ME_LUMA_PREDICTOR_MODE_VERTICAL_LEFT_INTEL:
					 ptr[0] = -Arrow/2;       ptr[1] = -Arrow;       break;					
				case CL_AVC_ME_LUMA_PREDICTOR_MODE_HORIZONTAL_UP_INTEL:
					 ptr[0] = Arrow;       ptr[1] = Arrow/4;        break;
					
				default: break;
			    }
		      }
			break;
		  }
		  default:  break;
		}
	}
}

void IntraPred(  
	Capture * pCapture, 
	std::vector<BMotionVector>& MV,
	std::vector<cl_uchar2>& Shapes,
	std::vector<cl_uchar>& Dirs,
	const CmdParserMV& cmd)
{
	 //MV, Shapes, Dirs assumed to be initialized earlier, this function only fills for Frame 0

	 // OpenCL initialization
    OpenCLBasic init("Intel", "GPU");
    
    //OpenCLBasic creates the platform/context and device for us, so all we need is to get an ownership (via incrementing ref counters with clRetainXXX)

    cl::Context context = cl::Context(init.context); clRetainContext(init.context);
    cl::Device device  = cl::Device(init.device);   clRetainDevice(init.device);
    cl::CommandQueue queue = cl::CommandQueue(init.queue); clRetainCommandQueue(init.queue);

    std::string ext = device.getInfo< CL_DEVICE_EXTENSIONS >();
	   
	if (string::npos == ext.find("cl_intel_device_side_avc_motion_estimation"))
    {
        printf("WARNING: The selected device doesn't officially support device side motion estimation extension!");
    }

    char* programSource = NULL;
    
    // Load the kernel source from the passed in file.
    if( LoadSourceFromFile( 
            "vme_ds_bidir.cl",
            programSource ) )
    {
        printf("Error: Couldn't load kernel source from file.\n" );
    }    

    // Create a built-in VME kernel
    cl_int err = 0;
    const cl_device_id & d = device();    
    cl::Program p(
        clCreateProgramWithSource(
            context(),
            1,
            ( const char** )&programSource,
            NULL,
            &err));

    if (err != CL_SUCCESS)
    {
        throw cl::Error(err, "Failed creating vme program(s)");
    }

    err = clBuildProgram(
            p(), 
            1, 
            &d,
            "",   
            NULL,
            NULL );

    size_t  buildLogSize = 0;
    clGetProgramBuildInfo(
        p(),
        d,
        CL_PROGRAM_BUILD_LOG,
        0,  
        NULL,
        &buildLogSize );

    cl_char*    buildLog = new cl_char[ buildLogSize ];
    if( buildLog )
    {
        clGetProgramBuildInfo(
            p(),
            d,
            CL_PROGRAM_BUILD_LOG,
            buildLogSize,
            buildLog,
            NULL );

        std::cout << ">>> Build Log:\n";
        std::cout << buildLog;
        std::cout << ">>>End of Build Log\n";
    }

    if (err != CL_SUCCESS)
    {
        throw cl::Error(err, "Failed building vme program(s)");
    }  

    cl::Kernel kernel(p, "block_intrapred_intel");

	int width = cmd.width.getValue();
    int height = cmd.height.getValue();

	int mvImageWidth, mvImageHeight;
	int mbImageWidth, mbImageHeight;

    ComputeNumMVs(CL_ME_MB_TYPE_4x4_INTEL, width, height, mvImageWidth, mvImageHeight, mbImageWidth, mbImageHeight);

	cl::ImageFormat imageFormat(CL_R, CL_UNORM_INT8);
    cl::Image2D srcImage(context, CL_MEM_READ_ONLY, imageFormat, width, height, 0, 0);    

	cl::Buffer modeBuffer(
		context, CL_MEM_WRITE_ONLY, 
		mbImageWidth * mbImageHeight * 22 *  sizeof(cl_uchar));

    cl::Buffer residualBuffer(
		context, CL_MEM_WRITE_ONLY, 
		mbImageWidth * mbImageHeight * sizeof(cl_ushort));

	cl::Buffer blksizeBuffer(
		context, CL_MEM_WRITE_ONLY, 
		mbImageWidth * mbImageHeight * sizeof(cl_uchar));

	//local arrays used to read back result from kernel and update MV, Shapes and Dir
	std::vector<cl_char>  modes;
	std::vector<cl_ushort>  dists;
	std::vector<cl_uchar>  blksizes;

	modes.resize(mbImageWidth * mbImageHeight * 22);
    dists.resize(mbImageWidth * mbImageHeight);
	blksizes.resize(mbImageWidth * mbImageHeight);
	
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

	double overallStart  = time_stamp();

    kernel.setArg(0, srcImage);
    kernel.setArg(1, sizeof(cl_uchar), &intraPartMask);  
	kernel.setArg(2, modeBuffer);                      // predictor modes
    kernel.setArg(3, residualBuffer );			       // search residuals       
	kernel.setArg(4, blksizeBuffer );			       // search residuals    
	kernel.setArg(5, srcImage);                        // arg 0 srcImage is used by VME, need to pass srcImage separately for media_block_read 
	kernel.setArg(6, sizeof(int), &mbImageHeight);     // iterations		

	unsigned numThreads = ( width + 15 ) / 16;
	queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(numThreads * 16, 1, 1), cl::NDRange(16, 1, 1));
	queue.finish();
	  	
	//read back modes and residuals
	
	cl_char* pModes = &modes[0];
	cl_ushort* pDist = &dists[0];
	cl_uchar* pBlkSizes = &blksizes[0];

	queue.enqueueReadBuffer(modeBuffer,CL_TRUE,0,sizeof(cl_char) * mbImageWidth * mbImageHeight * 22, pModes, 0,0);
    queue.enqueueReadBuffer(residualBuffer,CL_TRUE,0,sizeof(cl_ushort) * mbImageWidth * mbImageHeight, pDist ,0,0);
	queue.enqueueReadBuffer(blksizeBuffer,CL_TRUE,0,sizeof(cl_uchar) * mbImageWidth * mbImageHeight, pBlkSizes ,0,0);

	double overallStat  = time_stamp() - overallStart;	

	// Use returned modes and blk sizes to fill bidir MV, Shapes and Dirs for Frame 0, use to overlay 
	FillMV(MV, Shapes,Dirs,width,height,cmd, modes,blksizes);	 	
}


void MotionEstimationFwd( 
	Capture * pCapture, 
	std::vector<BMotionVector> &MVs, 
	std::vector<cl_ushort> &SADs,
	std::vector<cl_uchar2> &Shapes,
	std::vector<cl_uchar>  &Dirs, //needed for overlay, kernel will set all to fwd
    const CmdParserMV& cmd, 
	int skp_check_type)
{

    // OpenCL initialization
    OpenCLBasic init("Intel", "GPU");
    //OpenCLBasic creates the platform/context and device for us, so all we need is to get an ownership (via incrementing ref counters with clRetainXXX)

    cl::Context context = cl::Context(init.context); clRetainContext(init.context);
    cl::Device device  = cl::Device(init.device);   clRetainDevice(init.device);
    cl::CommandQueue queue = cl::CommandQueue(init.queue); clRetainCommandQueue(init.queue);

    std::string ext = device.getInfo< CL_DEVICE_EXTENSIONS >();
    if (string::npos == ext.find("cl_intel_device_side_avc_motion_estimation"))
    {
        printf("WARNING: The selected device doesn't officially support motion estimation or accelerator extensions!");
    }

    char* programSource = NULL;
    
    // Load the kernel source from the passed in file.
    if( LoadSourceFromFile( 
            "vme_ds_bidir.cl",
            programSource ) )
    {
        printf("Error: Couldn't load kernel source from file.\n" );
    }    

    // Create a built-in VME kernel
    cl_int err = 0;
    const cl_device_id & d = device();    
    cl::Program p(
        clCreateProgramWithSource(
            context(),
            1,
            ( const char** )&programSource,
            NULL,
            &err));

    if (err != CL_SUCCESS)
    {
        throw cl::Error(err, "Failed creating vme program(s)");
    }

    err = clBuildProgram(
            p(), 
            1, 
            &d,
            "",   
            NULL,
            NULL );

    size_t  buildLogSize = 0;
    clGetProgramBuildInfo(
        p(),
        d,
        CL_PROGRAM_BUILD_LOG,
        0,  
        NULL,
        &buildLogSize );

    cl_char*    buildLog = new cl_char[ buildLogSize ];
    if( buildLog )
    {
        clGetProgramBuildInfo(
            p(),
            d,
            CL_PROGRAM_BUILD_LOG,
            buildLogSize,
            buildLog,
            NULL );

        std::cout << ">>> Build Log:\n";
        std::cout << buildLog;
        std::cout << ">>>End of Build Log\n";
    }


    if (err != CL_SUCCESS)
    {
        throw cl::Error(err, "Failed building vme program(s)");
    }  

    cl::Kernel kernel(p, "block_motion_estimate_fwd_intel");

   
    int numPics = pCapture->GetNumFrames();
    int width = cmd.width.getValue();
    int height = cmd.height.getValue();

    int mvImageWidth, mvImageHeight;
	int mbImageWidth, mbImageHeight;

    ComputeNumMVs(CL_ME_MB_TYPE_4x4_INTEL, width, height, mvImageWidth, mvImageHeight, mbImageWidth, mbImageHeight);

    MVs.resize(numPics * mvImageWidth * mvImageHeight);
	SADs.resize(numPics * mvImageWidth * mvImageHeight);
	Shapes.resize(numPics * mbImageWidth * mbImageHeight); 
	Dirs.resize(numPics * mbImageWidth * mbImageHeight); 

	// Set up OpenCL surfaces
    cl::ImageFormat imageFormat(CL_R, CL_UNORM_INT8);
    cl::Image2D refImage(context, CL_MEM_READ_ONLY, imageFormat, width, height, 0, 0);
    cl::Image2D srcImage(context, CL_MEM_READ_ONLY, imageFormat, width, height, 0, 0);    
	cl::Buffer shapeBuffer(context, CL_MEM_WRITE_ONLY, mbImageWidth * mbImageHeight * sizeof(cl_uchar2));
	cl::Buffer DirBuffer(context, CL_MEM_WRITE_ONLY, mbImageWidth * mbImageHeight * sizeof(cl_uchar));

	cl_short2 *fwPredMem = new cl_short2[ mbImageWidth * mbImageHeight ];
	
    for( int i = 0; i < mbImageWidth * mbImageHeight; i++ )
    {		
		fwPredMem[ i ].s[ 0 ] = 0;
		fwPredMem[ i ].s[ 1 ] = 0;				
    }

	cl::Buffer fwPredBuffer(
        context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 
		mbImageWidth * mbImageHeight * sizeof(cl_short2), fwPredMem, NULL);
	
    cl::Buffer mvBuffer(
		context, CL_MEM_WRITE_ONLY, 
		mvImageWidth * mvImageHeight * sizeof(BMotionVector));

    cl::Buffer residualBuffer(
		context, CL_MEM_WRITE_ONLY, 
		mvImageWidth * mvImageHeight * sizeof(cl_ushort));
	
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
    double ioStat = 0;
	double ioTileStat = 0;
    double meStat = 0; // Motion estimation itself
	int count = 0;

    unsigned flags = 0;
    unsigned skipBlockType = 0;
	unsigned costPenalty = kCostPenalty;
	unsigned costPrecision = kCostPrecision;

    double overallStart  = time_stamp();

    // First frame is already in srcImg, so we start with the second frame
    for (int i = 1; i < numPics; i++, count++)
    {	            
		double ioStart = time_stamp();

		// Load next picture
        pCapture->GetSample(i, currImage);

        std::swap(refImage, srcImage);		

		double ioTileStart = time_stamp();
        // Copy to tiled image memory - this copy (and its overhead) is not necessary in a full GPU pipeline
        queue.enqueueWriteImage(srcImage, CL_TRUE, origin, region, currImage->PitchY, 0, currImage->Y);
				
		ioTileStat += (time_stamp() -ioTileStart);
        ioStat += (time_stamp() -ioStart);
        double meStart = time_stamp();
        // Schedule full-frame motion estimation
       
        kernel.setArg(0, srcImage);
        kernel.setArg(1, refImage);        
        kernel.setArg(2, sizeof(unsigned), &costPenalty);	// cost penalty
        kernel.setArg(3, sizeof(unsigned), &costPrecision); // cost precision                       
		kernel.setArg(4, fwPredBuffer);                     // fwd predictor
        kernel.setArg(5, mvBuffer);					        // search mvs        
        kernel.setArg(6, residualBuffer );			     	// search residuals       	
		kernel.setArg(7, shapeBuffer);                      // shapes
		kernel.setArg(8, DirBuffer);                        // best dir
        kernel.setArg(9, sizeof(int), &mbImageHeight);      // iterations
		kernel.setArg(10, sizeof(int), &skp_check_type);     // skp check block size		

		unsigned numThreads = ( width + 15 ) / 16;
		queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(numThreads * 16, 1, 1), cl::NDRange(16, 1, 1));

		queue.finish();

        meStat += (time_stamp() - meStart);
        ioStart = time_stamp();

        // Read back resulting motion vectors  
      
		BMotionVector * pMVs = &MVs[i * mvImageWidth * mvImageHeight];
		cl_ushort * pSADs = &SADs[i * mvImageWidth * mvImageHeight];
		cl_uchar2 * pShapes = &Shapes[i * mbImageWidth * mbImageHeight];		
		cl_uchar * pDirs = &Dirs[i * mbImageWidth * mbImageHeight];
	        
        queue.enqueueReadBuffer(mvBuffer,CL_TRUE,0,sizeof(BMotionVector) * mvImageWidth * mvImageHeight, pMVs, 0,0);
		queue.enqueueReadBuffer(residualBuffer,CL_TRUE,0,sizeof(cl_ushort) * mvImageWidth * mvImageHeight, pSADs ,0,0);
		queue.enqueueReadBuffer(shapeBuffer, CL_TRUE, 0, sizeof(cl_uchar2)* mbImageWidth * mbImageHeight, pShapes, 0, 0);
		queue.enqueueReadBuffer(DirBuffer, CL_TRUE, 0, sizeof(cl_uchar)* mbImageWidth * mbImageHeight, pDirs, 0, 0);

#define LIST_MV 0

#if LIST_MV
		for(int mbIndex = 0 ; mbIndex<mbImageWidth * mbImageHeight ; mbIndex++)
		{
		  int m0 = mbIndex * 16;		 

		  for(int ind=0;ind<16;ind++)
		  {
		   cl_short* ptr = (cl_short*)&MVs[i * mvImageWidth * mvImageHeight +  m0 + ind];
		   printf("\n mbIndex %d MV: %d %d %d %d SAD %d Shape %d %d",mbIndex,ptr[0],ptr[1],ptr[2],ptr[3],pSADs[m0+ind],pShapes[mbIndex].s[0],pShapes[mbIndex].s[1]);
		  }
		}
		
#endif		
	
        ioStat += (time_stamp() -ioStart);
    }

    double overallStat  = time_stamp() - overallStart;
    std::cout << std::setiosflags(std::ios_base::fixed) << std::setprecision(3);
    std::cout << "Overall time for " << numPics << " frames " << overallStat << " sec\n" ;
	std::cout << "Average frame tile I/O time per frame " << 1000*ioTileStat/count << " ms\n";
    std::cout << "Average frame file I/O time per frame " << 1000*ioStat/count << " ms\n";
    std::cout << "Average Motion Estimation time per frame is " << 1000*meStat/count << " ms\n";

  
    ReleaseImage(currImage);
}



void MotionEstimationBiDir( 
	Capture * pCapture, 
	std::vector<BMotionVector> &MVs, 
	std::vector<cl_ushort> &SADs, 
	std::vector<cl_uchar2> &Shapes,
	std::vector<cl_uchar>  &Dirs,
	const CmdParserMV& cmd, 
	int skp_check_type)
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
        printf("Error: The selected device doesn't offcially support device side motion estimation extensions!");
    }
	
    char* programSource = NULL;
    
    // Load the kernel source from the passed in file.
    if( LoadSourceFromFile("vme_ds_bidir.cl", programSource ) )
    {
        printf("Error: Couldn't load kernel source from file.\n" );
    }    

    // Create a built-in VME kernel
    cl_int err = 0;
    const cl_device_id & d = device();    
    cl::Program p(
		    clCreateProgramWithSource(
            context(),
            1,
            ( const char** )&programSource,
            NULL,
            &err));

    if (err != CL_SUCCESS)
    {
        throw cl::Error(err, "Failed creating vme program(s)");
    }

    err = clBuildProgram(
            p(), 
            1, 
            &d,
            "",
            NULL,
            NULL );

    size_t  buildLogSize = 0;
    clGetProgramBuildInfo(
        p(),
        d,
        CL_PROGRAM_BUILD_LOG,
        0,
        NULL,
        &buildLogSize );

    cl_char*    buildLog = new cl_char[ buildLogSize ];
    if( buildLog )
    {
        clGetProgramBuildInfo(
            p(),
            d,
            CL_PROGRAM_BUILD_LOG,
            buildLogSize,
            buildLog,
            NULL );

        std::cout << ">>> Build Log:\n";
        std::cout << buildLog;
        std::cout << ">>>End of Build Log\n";
    }


    if (err != CL_SUCCESS)
    {
        throw cl::Error(err, "Failed building vme program(s)");
    }  

    cl::Kernel kernel(p, "block_motion_estimate_bidir_intel");

   
    int numPics = pCapture->GetNumFrames();
    int width = cmd.width.getValue();
    int height = cmd.height.getValue();

    int mvImageWidth, mvImageHeight;
	int mbImageWidth, mbImageHeight;

    ComputeNumMVs(CL_ME_MB_TYPE_4x4_INTEL, width, height, mvImageWidth, mvImageHeight, mbImageWidth, mbImageHeight);

    MVs.resize(numPics * mvImageWidth * mvImageHeight);      //16 MV per mb
	SADs.resize(numPics * mvImageWidth * mvImageHeight);     //16 SAD per mb
	Shapes.resize(numPics * mbImageWidth * mbImageHeight); 
	Dirs.resize(numPics * mbImageWidth * mbImageHeight); 

    // Set up OpenCL surfaces
    cl::ImageFormat imageFormat(CL_R, CL_UNORM_INT8);
    cl::Image2D refImage0(context, CL_MEM_READ_ONLY, imageFormat, width, height, 0, 0);
    cl::Image2D srcImage(context, CL_MEM_READ_ONLY, imageFormat, width, height, 0, 0);  
	cl::Image2D refImage1(context, CL_MEM_READ_ONLY, imageFormat, width, height, 0, 0);
    cl::Buffer mvBuffer(context, CL_MEM_WRITE_ONLY, mvImageWidth * mvImageHeight * sizeof(BMotionVector));
    cl::Buffer residualBuffer(context, CL_MEM_WRITE_ONLY, mvImageWidth * mvImageHeight * sizeof(cl_ushort));
	cl::Buffer ShapeBuffer(context, CL_MEM_WRITE_ONLY, mbImageWidth * mbImageHeight * sizeof(cl_uchar2));
	cl::Buffer DirBuffer(context, CL_MEM_WRITE_ONLY, mbImageWidth * mbImageHeight * sizeof(cl_uchar));

	cl_short2 *fwPredMem = new cl_short2[ mbImageWidth * mbImageHeight ];
	cl_short2 *bwPredMem = new cl_short2[ mbImageWidth * mbImageHeight ];

    for( int i = 0; i < mbImageWidth * mbImageHeight; i++ )
    {		
		fwPredMem[ i ].s[ 0 ] = 0;
		fwPredMem[ i ].s[ 1 ] = 0;		

		bwPredMem[ i ].s[ 0 ] = 0;
		bwPredMem[ i ].s[ 1 ] = 0;	
    }

	cl::Buffer fwPredBuffer(
        context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 
		mbImageWidth * mbImageHeight * sizeof(cl_short2), fwPredMem, NULL);

	cl::Buffer bwPredBuffer(
        context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 
		mbImageWidth * mbImageHeight * sizeof(cl_short2), bwPredMem, NULL);


	cl::size_t<3> origin;
    origin[0] = 0;
    origin[1] = 0;
    origin[2] = 0;

    cl::size_t<3> region;
    region[0] = width;
    region[1] = height;
    region[2] = 1;

    // Bootstrap video sequence reading
    PlanarImage * currImage = CreatePlanarImage(width, height);
    
	pCapture->GetSample(0, currImage);
	queue.enqueueWriteImage(refImage0, CL_TRUE, origin, region, currImage->PitchY, 0, currImage->Y);

	pCapture->GetSample(1, currImage);
	queue.enqueueWriteImage(srcImage, CL_TRUE, origin, region, currImage->PitchY, 0, currImage->Y);

   
    // Process all frames
    double ioStat = 0;//file i/o
	double ioTileStat = 0;
    double meStat = 0;//motion estimation itself
	int count = 0;

   
	unsigned costPenalty = kCostPenalty;
	unsigned costPrecision = kCostPrecision;

    double overallStart  = time_stamp();
    

	// Frame 0 is already in refImg0, Frame 1 is in srcImage, start with Frame 2
   	// fw prediction uses refImg0 as reference, bw prediction uses refImg1 as reference
	// Motion estimates done for Frame1 through FrameNumPics-2, Frame0, FrameNumPics-1 used only for reference

    for (int i = 2; i < numPics; i++, count++)
    {	            
		double ioStart = time_stamp();

		// Load next picture
        pCapture->GetSample(i, currImage);

		double ioTileStart = time_stamp();
		queue.enqueueWriteImage(refImage1, CL_TRUE, origin, region, currImage->PitchY, 0, currImage->Y);
       				        		
		ioTileStat += (time_stamp() -ioTileStart);

        ioStat += (time_stamp() -ioStart);

        double meStart = time_stamp();
        // Schedule full-frame motion estimation
       
        kernel.setArg(0, srcImage);
        kernel.setArg(1, refImage0);        
		kernel.setArg(2, refImage1);  
        kernel.setArg(3, sizeof(unsigned), &costPenalty);	// cost penalty
        kernel.setArg(4, sizeof(unsigned), &costPrecision); // cost precision       
		kernel.setArg(5, fwPredBuffer);                     // fwd predictor 
	    kernel.setArg(6, bwPredBuffer);                     // bwd predictor
        kernel.setArg(7, mvBuffer);					        // search mvs        
        kernel.setArg(8, residualBuffer );				    // search residuals   
		kernel.setArg(9, ShapeBuffer);                      // shapes
	    kernel.setArg(10, DirBuffer);                       // best dir
        kernel.setArg(11, sizeof(int), &mbImageHeight);      // iterations
		kernel.setArg(12, sizeof(int), &skp_check_type); 
		

		unsigned numThreads = ( width + 15 ) / 16;
		queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(numThreads * 16, 1, 1), cl::NDRange(16, 1, 1));

        queue.finish();
        meStat += (time_stamp() - meStart);

        ioStart = time_stamp();
        // Read back resulting motion vectors 
       
		BMotionVector * pMVs = &MVs[(i-1) * mvImageWidth * mvImageHeight];
		cl_ushort * pSADs = &SADs[(i-1) * mvImageWidth * mvImageHeight];
		cl_uchar2 * pShapes = &Shapes[(i-1) * mbImageWidth * mbImageHeight];
		cl_uchar * pDirs = &Dirs[(i-1) * mbImageWidth * mbImageHeight];
	
        queue.enqueueReadBuffer(mvBuffer,CL_TRUE,0,sizeof(BMotionVector) * mvImageWidth * mvImageHeight, pMVs, 0,0);
		queue.enqueueReadBuffer(residualBuffer,CL_TRUE,0,sizeof(cl_ushort) * mvImageWidth * mvImageHeight, pSADs ,0,0);
		queue.enqueueReadBuffer(ShapeBuffer, CL_TRUE, 0, sizeof(cl_uchar2)* mbImageWidth * mbImageHeight, pShapes, 0, 0);
		queue.enqueueReadBuffer(DirBuffer, CL_TRUE, 0, sizeof(cl_uchar)* mbImageWidth * mbImageHeight, pDirs, 0, 0);

#define LIST_BI_MV 0

#if LIST_BI_MV
		for(int mbIndex = 0 ; mbIndex<mbImageWidth * mbImageHeight ; mbIndex++)
		{
		  int m0 = mbIndex * 16;		 

		  for(int ind=0;ind<16;ind++)
		  {
		   cl_short* ptr = (cl_short*)&MVs[(i-1) * mvImageWidth * mvImageHeight +  m0 + ind];
		   printf("\n mbIndex %d MV: %d %d %d %d SAD %d Shape %d %d Dir %d",mbIndex,ptr[0],ptr[1],ptr[2],ptr[3],pSADs[m0+ind],pShapes[mbIndex].s[0],pShapes[mbIndex].s[1] ,pDirs[mbIndex]);
		  }
		}
#endif
	
		std::swap(refImage1, srcImage);
		std::swap(refImage0, refImage1);

        ioStat += (time_stamp() -ioStart);
    }

    double overallStat  = time_stamp() - overallStart;
    std::cout << std::setiosflags(std::ios_base::fixed) << std::setprecision(3);
    std::cout << "Overall time for " << numPics << " frames " << overallStat << " sec\n" ;
	std::cout << "Average frame tile I/O time per frame " << 1000*ioTileStat/count << " ms\n";
    std::cout << "Average frame file I/O time per frame " << 1000*ioStat/count << " ms\n";
    std::cout << "Average Motion Estimation time per frame is " << 1000*meStat/count << " ms\n";

  
    ReleaseImage(currImage);
}


void ComputeCheckMotionVectorsBiDir( 
	Capture * pCapture, 
	std::vector<BMotionVector> & searchBMVs, 
	std::vector<cl_ushort> &skipSADs, 	
	std::vector<cl_uchar>  &Dirs,
    const CmdParserMV& cmd,
	int skp_check_type)
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
        printf("WARNING: The selected device doesn't officially support motion estimation or accelerator extensions!");
    }

    char* programSource = NULL;
    
    // Load the kernel source from the passed in file.
    if( LoadSourceFromFile( 
            "vme_ds_bidir.cl",
            programSource ) )
    {
        printf("Error: Couldn't load kernel source from file.\n" );
    }    
	  
    // Create a built-in VME kernel
    cl_int err = 0;
    const cl_device_id & d = device();    
    cl::Program p(
        clCreateProgramWithSource(
            context(),
            1,
            ( const char** )&programSource,
            NULL,
            &err));

    if (err != CL_SUCCESS)
    {
        throw cl::Error(err, "Failed creating vme program(s)");
    }

    err = clBuildProgram(
            p(), 
            1, 
            &d,
            "",
            NULL,
            NULL );

    size_t  buildLogSize = 0;
    clGetProgramBuildInfo(
        p(),
        d,
        CL_PROGRAM_BUILD_LOG,
        0,
        NULL,
        &buildLogSize );

    cl_char*    buildLog = new cl_char[ buildLogSize ];
    if( buildLog )
    {
        clGetProgramBuildInfo(
            p(),
            d,
            CL_PROGRAM_BUILD_LOG,
            buildLogSize,
            buildLog,
            NULL );

        std::cout << ">>> Build Log:\n";
        std::cout << buildLog;
        std::cout << ">>>End of Build Log\n";
    }

	cl::Kernel kernel(p, "block_skip_check_bidir_intel");
	
    int numPics = pCapture->GetNumFrames();
    int width = cmd.width.getValue();
    int height = cmd.height.getValue();
    int mvImageWidth, mvImageHeight;
	int mbImageWidth, mbImageHeight;
   
	ComputeNumMVs(CL_ME_MB_TYPE_4x4_INTEL, width, height, mvImageWidth, mvImageHeight,  mbImageWidth, mbImageHeight);
				
    // Set up OpenCL surfaces
    cl::ImageFormat imageFormat(CL_R, CL_UNORM_INT8);
    cl::Image2D refImage0(context, CL_MEM_READ_ONLY, imageFormat, width, height, 0, 0);
	 cl::Image2D refImage1(context, CL_MEM_READ_ONLY, imageFormat, width, height, 0, 0);
    cl::Image2D srcImage(context, CL_MEM_READ_ONLY, imageFormat, width, height, 0, 0);    
	cl::Buffer skipResidualBuffer(context, CL_MEM_WRITE_ONLY, mvImageWidth * mvImageHeight * sizeof(cl_ushort)); 

	skipSADs.resize(numPics * mvImageWidth * mvImageHeight ); 
		
	int numComponents = (skp_check_type == SKP_CHK_16) ? 1 : 4;
	unsigned skipBlockType = (skp_check_type == SKP_CHK_16) ? 0 : 1;

	cl_uint2 *bidirMV = new cl_uint2[ mbImageWidth * mbImageHeight * numComponents];   //packed format
		
	cl::size_t<3> origin;
    origin[0] = 0;
    origin[1] = 0;
    origin[2] = 0;
    cl::size_t<3> region;
    region[0] = width;
    region[1] = height;
    region[2] = 1;

    // Bootstrap video sequence reading
    PlanarImage * currImage = CreatePlanarImage(width, height);

    pCapture->GetSample(0, currImage);
    queue.enqueueWriteImage(refImage0, CL_TRUE, origin, region, currImage->PitchY, 0, currImage->Y);
    
	pCapture->GetSample(1, currImage);
    queue.enqueueWriteImage(srcImage, CL_TRUE, origin, region, currImage->PitchY, 0, currImage->Y);

    // Process all frames
	double ioTileStat = 0;
    double ioStat = 0;//file i/o
    double meStat = 0;//motion estimation itself

    
	unsigned costPenalty = kCostPenalty;
	unsigned costPrecision = kCostPrecision;

    double overallStart  = time_stamp();

	// Frame 0 is already in refImg0, Frame 1 is in srcImage, start with Frame 2
   	// fw prediction uses refImg0 as reference, bw prediction uses refImg1 as reference
	// Motion estimates done for Frame1 through FrameNumPics-2, Frame0, FrameNumPics-1 used only for reference

    int count = 0;

	for (int i = 2; i < numPics; i++,count++)
    {		 
		unsigned offset = mvImageWidth * mvImageHeight;

		for( int j = 0; j < mbImageWidth * mbImageHeight; j++ )
		{			
			for( int l = 0; l < numComponents; l++ )
	 	        bidirMV[j*numComponents + l] =  searchBMVs[(i-1)*offset + j*16 + l*4];
		}		

		cl_uchar* pDirs = (cl_uchar*) &Dirs[(i-1) * mbImageWidth * mbImageHeight];	

		cl::Buffer skipMVBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 
			mbImageWidth * mbImageHeight * numComponents * sizeof(cl_uint2), bidirMV, NULL);     	    	

		cl::Buffer DirBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 
		mbImageWidth * mbImageHeight * sizeof(cl_uchar), pDirs, NULL);

        // Load next picture

		double ioStart = time_stamp();

        pCapture->GetSample(i, currImage);

		double ioTileStart = time_stamp();
		       
        queue.enqueueWriteImage(refImage1, CL_TRUE, origin, region, currImage->PitchY, 0, currImage->Y);

		ioTileStat += (time_stamp() -ioTileStart);

        ioStat += (time_stamp() -ioStart);

        double meStart = time_stamp();
        // Schedule full-frame motion estimation
		      
        kernel.setArg(0, srcImage);
        kernel.setArg(1, refImage0);                
		kernel.setArg(2, refImage1);    
        kernel.setArg(3, sizeof(unsigned), &skipBlockType); // skip block type
		kernel.setArg(4, sizeof(unsigned), &costPenalty);	// cost penalty
        kernel.setArg(5, sizeof(unsigned), &costPrecision); // cost precision                         
        kernel.setArg(6, skipMVBuffer);				        // use ME result as skip check i/p
		kernel.setArg(7, DirBuffer);                        // use directions for input
        kernel.setArg(8, skipResidualBuffer);               // skip residuals       
        kernel.setArg(9, sizeof(int), &mbImageHeight);     //  iterations	

		unsigned numThreads = ( width + 15 ) / 16;
		queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(numThreads * 16, 1, 1), cl::NDRange(16, 1, 1));
		
        queue.finish();
        
		meStat += (time_stamp() - meStart);
		ioStart = time_stamp();		

        // Read back resulting SADs  
		void * pSkipSADs = &skipSADs[(i-1) * mvImageWidth * mvImageHeight]; 
		queue.enqueueReadBuffer(skipResidualBuffer,CL_TRUE,0,sizeof(cl_ushort) * mvImageWidth * mvImageHeight,pSkipSADs,0,0);		

		std::swap(refImage1, srcImage);
		std::swap(refImage0, refImage1);

        ioStat += (time_stamp() -ioStart);
    }

    double overallStat  = time_stamp() - overallStart;
    std::cout << std::setiosflags(std::ios_base::fixed) << std::setprecision(3);
	
    std::cout << "Overall time for " << numPics << " frames " << overallStat << " sec\n" ;
	std::cout << "Average frame tile I/O time per frame " << 1000*ioTileStat/count << " ms\n";
    std::cout << "Average frame file I/O time per frame " << 1000*ioStat/count << " ms\n";
    std::cout << "Average Skip Check time per frame is " << 1000*meStat/count << " ms\n";
   
    ReleaseImage(currImage);
}

void ComputeCheckMotionVectorsFwd( 
	Capture * pCapture,
	std::vector<BMotionVector> & searchBMVs, 	
	std::vector<cl_ushort> &skipSADs, 	
    const CmdParserMV& cmd,
	int skp_check_type)
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
        printf("WARNING: The selected device doesn't officially support motion estimation or accelerator extensions!");
    }

    char* programSource = NULL;
    
    // Load the kernel source from the passed in file.
    if( LoadSourceFromFile( 
            "vme_ds_bidir.cl",
            programSource ) )
    {
        printf("Error: Couldn't load kernel source from file.\n" );
    }    
	  
    // Create a built-in VME kernel
    cl_int err = 0;
    const cl_device_id & d = device();    
    cl::Program p(
        clCreateProgramWithSource(
            context(),
            1,
            ( const char** )&programSource,
            NULL,
            &err));

    if (err != CL_SUCCESS)
    {
        throw cl::Error(err, "Failed creating vme program(s)");
    }

    err = clBuildProgram(
            p(), 
            1, 
            &d,
            "",
            NULL,
            NULL );

    size_t  buildLogSize = 0;
    clGetProgramBuildInfo(
        p(),
        d,
        CL_PROGRAM_BUILD_LOG,
        0,
        NULL,
        &buildLogSize );

    cl_char*    buildLog = new cl_char[ buildLogSize ];
    if( buildLog )
    {
        clGetProgramBuildInfo(
            p(),
            d,
            CL_PROGRAM_BUILD_LOG,
            buildLogSize,
            buildLog,
            NULL );

        std::cout << ">>> Build Log:\n";
        std::cout << buildLog;
        std::cout << ">>>End of Build Log\n";
    }

	cl::Kernel kernel(p, "block_skip_check_fwd_intel");
	
    int numPics = pCapture->GetNumFrames();
    int width = cmd.width.getValue();
    int height = cmd.height.getValue();
    int mvImageWidth, mvImageHeight;
	int mbImageWidth, mbImageHeight;
   
	ComputeNumMVs(CL_ME_MB_TYPE_4x4_INTEL, width, height, mvImageWidth, mvImageHeight,  mbImageWidth, mbImageHeight);
				
    // Set up OpenCL surfaces
    cl::ImageFormat imageFormat(CL_R, CL_UNORM_INT8);
    cl::Image2D refImage(context, CL_MEM_READ_ONLY, imageFormat, width, height, 0, 0);
    cl::Image2D srcImage(context, CL_MEM_READ_ONLY, imageFormat, width, height, 0, 0);    
	cl::Buffer skipResidualBuffer(context, CL_MEM_WRITE_ONLY, mvImageWidth * mvImageHeight * sizeof(cl_ushort)); 

    skipSADs.resize(numPics * mvImageWidth * mvImageHeight ); 
		
	int numComponents = (skp_check_type == SKP_CHK_16) ? 1 : 4;
	unsigned skipBlockType = (skp_check_type == SKP_CHK_16) ? 0 : 1;

	cl_uint2 *bidirMV = new cl_uint2[ mbImageWidth * mbImageHeight * numComponents];   //packed format
		
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
	double ioTileStat = 0;
    double ioStat = 0; // File i/o
    double meStat = 0; // Motion estimation itself
    
	unsigned costPenalty = kCostPenalty;
	unsigned costPrecision = kCostPrecision;

	int count = 0;

    double overallStart  = time_stamp();
    // First frame is already in srcImg, so we start with the second frame
	for (int i = 1; i < numPics; i++, count++)
    {		 
		unsigned offset = mvImageWidth * mvImageHeight;

		for( int j = 0; j < mbImageWidth * mbImageHeight; j++ )
		{			
			for( int l = 0; l < numComponents; l++ )
	 	        bidirMV[j*numComponents + l] =  searchBMVs[i*offset + j*16 + l*4];
		}

		cl::Buffer skipMVBuffer( 
			context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 
			mbImageWidth * mbImageHeight * numComponents * sizeof(cl_uint2), bidirMV, NULL);     

        // Load next picture

		double ioStart = time_stamp();

        pCapture->GetSample(i, currImage);

        std::swap(refImage, srcImage);

		double ioTileStart = time_stamp();

        // Copy to tiled image memory - this copy (and its overhead) is not necessary in a full GPU pipeline
        queue.enqueueWriteImage(srcImage, CL_TRUE, origin, region, currImage->PitchY, 0, currImage->Y);

		ioTileStat += (time_stamp() -ioTileStart);

        ioStat += (time_stamp() -ioStart);

        double meStart = time_stamp();
        // Schedule full-frame motion estimation
       
        kernel.setArg(0, srcImage);
        kernel.setArg(1, refImage);                
        kernel.setArg(2, sizeof(unsigned), &skipBlockType); // skip block type
		kernel.setArg(3, sizeof(unsigned), &costPenalty);	// cost penalty
        kernel.setArg(4, sizeof(unsigned), &costPrecision); // cost precision                         
        kernel.setArg(5, skipMVBuffer);				        // use ME result as skip check i/p
        kernel.setArg(6, skipResidualBuffer);               // skip residuals       
        kernel.setArg(7, sizeof(int), &mbImageHeight);      // iterations

		unsigned numThreads = ( width + 15 ) / 16;
		queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(numThreads * 16, 1, 1), cl::NDRange(16, 1, 1));

        queue.finish();

        meStat += (time_stamp() - meStart);
        ioStart = time_stamp();		

        // Read back resulting SADs    
		void * pSkipSADs = &skipSADs[i * mvImageWidth * mvImageHeight]; 
		queue.enqueueReadBuffer(skipResidualBuffer,CL_TRUE,0,sizeof(cl_ushort) * mvImageWidth * mvImageHeight,pSkipSADs,0,0);		

        ioStat += (time_stamp() -ioStart);
    }
    double overallStat  = time_stamp() - overallStart;
    std::cout << std::setiosflags(std::ios_base::fixed) << std::setprecision(3);
	
    std::cout << "Overall time for " << numPics << " frames " << overallStat << " sec\n" ;
	std::cout << "Average frame tile I/O time per frame " << 1000*ioTileStat/count << " ms\n";
    std::cout << "Average frame file I/O time per frame " << 1000*ioStat/count << " ms\n";
    std::cout << "Average Motion Estimation time per frame is " << 1000*meStat/count << " ms\n";
   
    ReleaseImage(currImage);
}

void VerifySkipCheckSAD( 
	Capture * pCapture, 
	std::vector<cl_ushort> &searchSADs,
	std::vector<cl_ushort> &skipSADs, 	
	const CmdParserMV& cmd,
	int skp_check_type)
{

	int mvImageWidth, mvImageHeight;
	int mbImageWidth, mbImageHeight;
        
	int width = cmd.width.getValue();
    int height = cmd.height.getValue();

	ComputeNumMVs(CL_ME_MB_TYPE_4x4_INTEL, width, height, mvImageWidth, mvImageHeight, mbImageWidth, mbImageHeight);

	bool differs = false;

	for(int f=1; f<pCapture->GetNumFrames(); f++)
	   for (int r = 0; r < mbImageHeight; r++) 
		    for (int c = 0; c < mbImageWidth; c++)
			 {  
  	            int mbIndex = f*mbImageWidth*mbImageHeight + r*mbImageWidth + c ;				 
	            int m0 = mbIndex * 16;
				
				if(skp_check_type == SKP_CHK_16)
				{
				   if(searchSADs[m0] != skipSADs[m0])
			    	{
				     printf("\nf r c %d %d %d : mbIndex %d : 16x16 : ME sad %d SKC sad %d\n",f,r,c,mbIndex,searchSADs[m0],skipSADs[m0]);
				     differs = true;
				    }
				}	

				if(skp_check_type == SKP_CHK_8)
				{
				   if(searchSADs[m0] != skipSADs[m0])
			    	{
				     printf("\nf r c %d %d %d : mbIndex %d : 8x8 partition 0 : ME sad %d SKC sad %d\n",f,r,c,mbIndex,searchSADs[m0],skipSADs[m0]);
				     differs = true;
				    }

			   	   if(searchSADs[m0+4] != skipSADs[m0+4])
				   {
				     printf("\nf r c %d %d %d : mbIndex %d : 8x8 partition 1 : ME sad %d SKC sad %d\n",f,r,c,mbIndex,searchSADs[m0+4],skipSADs[m0+4]); 
				     differs = true;
				   }

				  if(searchSADs[m0+8] != skipSADs[m0+8])
				  {
					printf("\nf r c %d %d %d : mbIndex %d : 8x8 partition 2 : ME sad %d SKC sad %d\n",f,r,c,mbIndex,searchSADs[m0+8],skipSADs[m0+8]);
					differs = true;
				  }

				  if(searchSADs[m0+12] != skipSADs[m0+12])
				  {
					printf("\nf r c %d %d %d : mbIndex %d : 8x8 partition 3 : ME sad %d SKC sad %d\n",f,r,c,mbIndex,searchSADs[m0+12],skipSADs[m0+12]);
					
					differs = true;
				  }
			  }	
		}

	if(differs)  printf("\n SAD differences found between Motion Estimation and Skip Check.\n");
	else printf("\n No SAD differences found between Motion Estimation and Skip Check.\n");

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Overlay routines
//////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef uint8_t U8;


//void DrawLine(int x0, int y0, int dx, int dy, U8 *pPic, int nPicWidth, int nPicHeight, U8 u8Pixel)  use for single char color

void DrawPixel(int x, int y, PlanarImage* srcImage, int nPicWidth, int nPicHeight, cl_short4 COLOR)
{
	int nYPixPos, nUVPixPos;

	if (x<0 || x >= nPicWidth || y<0 || y >= nPicHeight)
		return;         // Don't draw out of bound pixels
	// Draw Luma channel 
	nYPixPos = y * nPicWidth + x;
	*(srcImage->Y + nYPixPos) = COLOR.s[0];
	if (COLOR.s[3] == 255)
	{
		// Draw Chroma channels only for color
		nUVPixPos = ((y / 2) * (nPicWidth / 2) + (x / 2));
		*(srcImage->U + nUVPixPos) = COLOR.s[1];
		*(srcImage->V + nUVPixPos) = COLOR.s[2];
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



#define OFF(P) (P + 2) >> 2

#define PRINT_MV 0

void DrawFwBwLine(int x0, int y0, BMotionVector Mv, PlanarImage* srcImage, int width, int height,cl_uchar dir, bool intra)
{
   if (dir  == 0 ) // CLK_AVC_ME_MAJOR_FORWARD_INTEL
   {
    MotionVector* fwMv = (MotionVector*) &(Mv.s[0]);
	DrawLine(x0, y0, OFF((*fwMv).s[0]), OFF((*fwMv).s[1]), srcImage, width, height, intra? SILVER: RED);

#if PRINT_MV
	printf("\nfwd MV x0 %d y0 %d : %d %d", x0,y0, (*fwMv).s[0],(*fwMv).s[1]);
#endif

   }

   else if (dir  == 1) //CLK_AVC_ME_MAJOR_BACKWARD_INTEL
   {
     MotionVector* bwMv = (MotionVector*) &(Mv.s[1]);
	 DrawLine(x0, y0, OFF((*bwMv).s[0]), OFF((*bwMv).s[1]), srcImage, width, height, GREEN);

#if PRINT_MV
	printf("\nbwd MV x0 %d y0 %d : %d %d",x0,y0, (*bwMv).s[0],(*bwMv).s[1]);
#endif

   }

   else if (dir  == 2) //CLK_AVC_ME_MAJOR_BIDIRECTIONAL_INTEL
   {
     MotionVector* fwMv = (MotionVector*) &(Mv.s[0]);
	 MotionVector* bwMv = (MotionVector*) &(Mv.s[1]);

     DrawLine(x0, y0, OFF((*fwMv).s[0]), OFF((*fwMv).s[1]), srcImage, width, height, YELLOW);
	 DrawLine(x0, y0, OFF((*bwMv).s[0]), OFF((*bwMv).s[1]), srcImage, width, height,BLUE); 

#if PRINT_MV
	printf("\nbidir MV x0 %d y0 %d : fwd %d %d bwd %d %d",x0,y0,(*fwMv).s[0],(*fwMv).s[1], (*bwMv).s[0],(*bwMv).s[1]);
#endif
	
   }
   else
   {
    throw std::runtime_error("Unknown mv direction in DrawFwBwLine()");
   }
}

void OverlayVectorsBiDir(unsigned int subBlockSize, bool intra, std::vector<BMotionVector>& MVs,
                         std::vector<cl_uchar2>& Shapes, std::vector<cl_uchar>& Dirs, PlanarImage* srcImage,
                         int frame, int width, int height)


{
  int mvImageWidth, mvImageHeight;
  int mbImageWidth, mbImageHeight;

  ComputeNumMVs(CL_ME_MB_TYPE_4x4_INTEL, width, height, mvImageWidth, mvImageHeight,
                mbImageWidth, mbImageHeight);

  BMotionVector* pMV = &MVs[frame * mvImageWidth * mvImageHeight];  
  cl_uchar2* pShapes = &Shapes[frame * mbImageWidth * mbImageHeight];
  cl_uchar*  pDirs   = &Dirs[frame * mbImageWidth * mbImageHeight];
  cl_uchar dir;

 
  for (int i = 0; i < mbImageHeight; i++) {
    for (int j = 0; j < mbImageWidth; j++) {
  
	  int mbIndex = j + i * mbImageWidth;
	  // Selectively Draw motion vectors for different sub block sizes
	  int j0 = j * 16; int i0 = i * 16; int m0 = mbIndex * 16;
  
	  switch (pShapes[mbIndex].s[0]) {   //major shape
        case 0:                          //16x16 
#if SHOW_BLOCKS
			DrawLine(j0, i0, 16, 0, srcImage, width, height, GRAY);
			DrawLine(j0, i0, 0, 16, srcImage, width, height, GRAY);
			DrawLine(j0 + 16, i0, 0, 16, srcImage, width, height, GRAY);
			DrawLine(j0, i0 + 16, 16, 0, srcImage, width, height, GRAY);
#else
			
			dir = pDirs[mbIndex] & 0x03;     		
			DrawFwBwLine(j0 + 8, i0 + 8, pMV[m0], srcImage, width, height, dir, intra);
#endif
			break;
        case 1:                          //16wx8h
#if SHOW_BLOCKS
			DrawLine(j0, i0, 16, 0, srcImage, width, height, GRAY);
			DrawLine(j0, i0, 0, 16, srcImage, width, height, GRAY);
			DrawLine(j0 + 8, i0, 0, 16, srcImage, width, height, GRAY);
			DrawLine(j0 + 16, i0, 0, 16, srcImage, width, height, GRAY);
			DrawLine(j0, i0 + 16, 16, 0, srcImage, width, height, GRAY);
#else
			dir = pDirs[mbIndex] & 0x03;
			DrawFwBwLine(j0 + 8, i0 + 4,  pMV[m0], srcImage, width, height,dir, intra);
			
			dir = (pDirs[mbIndex] >> 4) & 0x03;
			DrawFwBwLine(j0 + 8, i0 + 12, pMV[m0], srcImage, width, height,dir, intra);

#endif
			break;
        case 2:                         //8wx16h
#if SHOW_BLOCKS
			DrawLine(j0, i0, 16, 0, srcImage, width, height, GRAY);
			DrawLine(j0, i0, 0, 16, srcImage, width, height, GRAY);
			DrawLine(j0, i0 + 8, 16, 0, srcImage, width, height, GRAY);
			DrawLine(j0 + 16, i0, 0, 16, srcImage, width, height, GRAY);
			DrawLine(j0, i0 + 16, 16, 0, srcImage, width, height, GRAY);
#else			
			dir = pDirs[mbIndex] & 0x03;
			DrawFwBwLine(j0 + 4, i0 + 8, pMV[m0], srcImage, width, height,dir, intra);
			
			dir = (pDirs[mbIndex] >> 4) & 0x03;
			DrawFwBwLine(j0 + 12, i0 + 8, pMV[m0], srcImage, width, height,dir, intra);

#endif
			break;
        case 3:                        //8x8
			cl_uchar4 minor_shapes;
			minor_shapes.s[0] = (pShapes[mbIndex].s[1]) & 0x03;
			minor_shapes.s[1] = (pShapes[mbIndex].s[1] >> 2) & 0x03;
			minor_shapes.s[2] = (pShapes[mbIndex].s[1] >> 4) & 0x03;
			minor_shapes.s[3] = (pShapes[mbIndex].s[1] >> 6) & 0x03;
#if SHOW_BLOCKS
			DrawLine(j0, i0, 16, 0, srcImage, width, height, GRAY);
			DrawLine(j0, i0, 0, 16, srcImage, width, height, GRAY);
			DrawLine(j0 + 8, i0, 0, 16, srcImage, width, height, GRAY);
			DrawLine(j0, i0 + 8, 16, 0, srcImage, width, height, GRAY);
			DrawLine(j0 + 16, i0, 0, 16, srcImage, width, height, GRAY);
			DrawLine(j0, i0 + 16, 16, 0, srcImage, width, height, GRAY);
#endif
			for (int m = 0; m < 4; ++m) 
				{	
				int mdiv = m / 2;
				int mmod = m % 2;

				dir = (pDirs[mbIndex] >> (m*2)) & 0x03; //common for all minor shapes within 8x8 block
				
				switch (minor_shapes.s[m])
				{
				  case 0:	// 8 x 8
#if !SHOW_BLOCKS
				
					DrawFwBwLine(j0 + mmod * 8 + 4, i0 + mdiv * 8 + 4, pMV[m0 + m * 4], srcImage, width, height,dir, intra);
#endif
					break;
				  case 1: // 8w x 4h
#if SHOW_BLOCKS
					DrawLine(j0 + mmod * 8, i0 + mdiv * 8 + 4, 8, 0, srcImage, width, height, GRAY);
#else
					for (int n = 0; n < 2; ++n) {
				     DrawFwBwLine(j0 + mmod * 8 + 4, i0 + (mdiv * 8 + n * 4 + 2), pMV[m0 + m * 4 + n * 2], srcImage, width, height, dir, intra);
				   	}
#endif
					break;
				  case 2:	// 4w x 8h
#if SHOW_BLOCKS
					DrawLine(j0 + mmod * 8 + 4, i0 + mdiv * 8, 8, 0, srcImage, width, height, GRAY);
#else					
					for (int n = 0; n < 2; ++n) {
				     DrawFwBwLine(j0 + (mmod * 8 + n * 4 + 2), i0 + mdiv * 8 + 4, pMV[m0 + m * 4 + n * 2], srcImage, width, height, dir, intra);
					}
#endif
					break;
				  case 3: // 4 x 4
#if SHOW_BLOCKS
					DrawLine(j0 + mmod * 8, i0 + mdiv * 8 + 4, 8, 0, srcImage, width, height, GRAY);
					DrawLine(j0 + mmod * 8 + 4, i0 + mdiv * 8, 8, 0, srcImage, width, height, GRAY);
#else
					for (int n = 0; n < 4; ++n) {
				   DrawFwBwLine(j0 + n * 4 + 2, i0 + m * 4 + 2, pMV[m0 + m * 4 + n], srcImage, width, height, dir, intra);
					}
#endif
					break;

				  default:
					 printf("Unknown minor shape in OverlayVectorsBiDir() i j %d %d val %d\n",i,j,minor_shapes.s[m]);
					 throw std::runtime_error("Unknown minor shape in OverlayVectorsBiDir()");
				   
					 break;    
				}
			}
			break;

		default:
          printf("Unknown major shape in OverlayVectorsBiDir() i j %d %d val %d\n",i,j,pShapes[mbIndex].s[0]);
		  throw std::runtime_error("Unknown major shape in OverlayVectorsBiDir()");

		  break;

         }
	  }
   }
}


void OverlayMV(Capture * pCapture, int width,int height, std::vector<BMotionVector> & MVs, std::vector<cl_uchar2> & Shapes, std::vector<cl_uchar> & Dirs,const CmdParserMV& cmd)
{
	//Overlay MVs on images

	int mvImageWidth, mvImageHeight;
	int mbImageWidth, mbImageHeight;
        
	ComputeNumMVs(CL_ME_MB_TYPE_4x4_INTEL, width, height, mvImageWidth, mvImageHeight, mbImageWidth, mbImageHeight);
				       
	FrameWriter * pWriter = FrameWriter::CreateFrameWriter(width, height, pCapture->GetNumFrames(), cmd.out_to_bmp.getValue());
    PlanarImage * srcImage = CreatePlanarImage(width, height);
		        
    unsigned int subBlockSize = ComputeSubBlockSize(CL_ME_MB_TYPE_4x4_INTEL);

    for (int k = 0; k < pCapture->GetNumFrames(); k++)
    {
            pCapture->GetSample(k, srcImage);
            //For Frame 0,  overlay Intraprediction directions on Src picture, for later frames, overlay Interprediction MVs
#if BIDIR_PRED
            if(k< pCapture->GetNumFrames()-1)             //for bidir prediction, leave out the last frame
#else
			if(k< pCapture->GetNumFrames())
#endif					  
            OverlayVectorsBiDir(subBlockSize, k == 0, MVs, Shapes, Dirs, srcImage, k, width, height);
			
            pWriter->AppendFrame(srcImage);
     }
    
	std::cout << "Writing " << pCapture->GetNumFrames() << " frames to " << cmd.overlayFileName.getValue() << "..." << std::endl;
    pWriter->WriteToFile(cmd.overlayFileName.getValue().c_str());

    FrameWriter::Release(pWriter);
       
    ReleaseImage(srcImage);
	
}


int main( int argc, const char** argv )
{

    try
    {
        CmdParserMV cmd(argc, argv);
        cmd.parse();

        // Immediately exit if user wanted to see the usage information only.
        if(cmd.help.isSet())
        {
            return 0;
        }

        const int width = cmd.width.getValue();
        const int height = cmd.height.getValue();
		const int frames = cmd.frames.getValue();

        // Open input sequence
        Capture * pCapture = Capture::CreateFileCapture(cmd.fileName.getValue(), width, height,frames);
        if (!pCapture)
        {
            throw std::runtime_error("Failed opening video input sequence...");
        }

		bool differs = false;

        // Process sequence
        std::cout << "Processing " << pCapture->GetNumFrames() << " frames ..." << std::endl;

        std::vector<BMotionVector> searchMVs;
		std::vector<cl_ushort> searchSADs;
		std::vector<cl_ushort> skipSADs;
		std::vector<cl_uchar2> Shapes;
		std::vector<cl_uchar>  Dirs;


#if BIDIR_PRED
		MotionEstimationBiDir(pCapture, searchMVs, searchSADs, Shapes, Dirs, cmd,skp_check_type);
#else
	    MotionEstimationFwd(pCapture, searchMVs, searchSADs, Shapes,Dirs, cmd,skp_check_type);
#endif
		
		IntraPred(pCapture, searchMVs, Shapes, Dirs, cmd);  // Intramode prediction for Frame 0 only

		if(skp_check_type == SKP_CHK_8 || skp_check_type == SKP_CHK_16)  // Do skip check kernel only for partition sizes of 8x8 and 16x16
		{
#if BIDIR_PRED		 
            ComputeCheckMotionVectorsBiDir(pCapture, searchMVs, skipSADs,Dirs,cmd,skp_check_type);
#else
	        ComputeCheckMotionVectorsFwd(pCapture, searchMVs, skipSADs,cmd,skp_check_type);
#endif
		    VerifySkipCheckSAD(pCapture,searchSADs, skipSADs,cmd,skp_check_type);		
		}

		OverlayMV(pCapture,width,height, searchMVs,Shapes, Dirs,cmd);

		Capture::Release(pCapture);
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
