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

#include "yuv_utils.h"

#include <cassert>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <CL/cl.h>

namespace YUVUtils
{
    class YUVCapture : public Capture
    {
    public:
        YUVCapture(const std::string & fn, int width, int height, int frames = 0);
        virtual void GetSample(int frameNum, PlanarImage * im);

    protected:
        std::ifstream m_file;
    };

    YUVCapture::YUVCapture( const std::string & fn, int width, int height, int frames )
        :    m_file (fn.c_str(), std::ios::binary | std::ios::ate)
    {

        if (!m_file.good())
        {
            std::stringstream ss;
            ss << "Unable to load YUV file: " << fn;
		    throw std::runtime_error(ss.str().c_str());
        }

        const size_t fileSize = static_cast<size_t>(m_file.tellg());
        const size_t frameSize = width * height * 3 / 2 * sizeof(uint8_t);
        if (fileSize % frameSize)
        {
		    throw std::runtime_error("YUV file file size error. Wrong dimensions?");
		}
        m_file.seekg(0, std::ios::beg);

        m_numFrames = (frames == 0)? ((int)(fileSize) / (int)frameSize) : frames;
        m_width = width;
        m_height = height;
    }

    void YUVCapture::GetSample( int frameNum, PlanarImage * im )
    {
        if (im->Width != (size_t)m_width || im->Height != (size_t)m_height)
        {
		    throw std::runtime_error("Capture::GetFrame: output image size mismatch.");
        }

        const size_t frameSize = m_width * m_height * 3 / 2 * sizeof(uint8_t);
        m_file.clear();
        m_file.seekg(frameNum * frameSize);

        size_t inRowSize = m_width * sizeof(uint8_t);
        size_t outRowSize = im->PitchY * sizeof(uint8_t);
        char * pOut = (char*)im->Y;
        for (int i = 0; i < m_height; ++i)
        {
            m_file.read(pOut, inRowSize);
            pOut += outRowSize;
        }
        assert(pOut == (char*)im->U);
        pOut = (char*)im->U;
        inRowSize = (m_width / 2) * sizeof(uint8_t);
        outRowSize = im->PitchU * sizeof(uint8_t);
        for (int i = 0; i < m_height / 2; ++i)
        {
            m_file.read(pOut, inRowSize);
            pOut += outRowSize;
        }
        assert(pOut == (char*)im->V);
        pOut = (char*)im->V;
        inRowSize = (m_width / 2) * sizeof(uint8_t);
        outRowSize = im->PitchV * sizeof(uint8_t);
        for (int i = 0; i < m_height / 2; ++i)
        {
            m_file.read(pOut, inRowSize);
            pOut += outRowSize;
        }
    }

    Capture * Capture::CreateFileCapture(const std::string & fn, int width, int height, int frames)
    {
        Capture * cap = NULL;

        if((strstr(fn.c_str(), ".yuv") != NULL) || (strstr(fn.c_str(), ".yv12") != NULL))
        {
            cap = new YUVCapture(fn, width, height, frames);
        }
        else
        {
            throw std::runtime_error("Unsupported capture file format.");
        }

        return cap;
    }

    void Capture::Release(Capture * cap)
    {
        delete cap;
    }

    PlanarImage * CreatePlanarImage(int width, int height, int pitchY)
    {
        PlanarImage * im = new PlanarImage;

        if (pitchY == 0)
        {
            pitchY = width;
        }

        const size_t num_pixels = pitchY * height + width * height / 2;
#ifdef __linux__
		int ret = 0;
		ret = posix_memalign((void**)(&(im->Y)), 0x1000, num_pixels);
		if (ret)
		{
			throw std::runtime_error("Allocation failed");
		}
#else
        im->Y = (uint8_t *)_aligned_malloc(num_pixels, 0x1000);
        if (!im->Y)
        {
		    throw std::runtime_error("Allocation failed");
        }
#endif

        im->U = im->Y + pitchY * height;
        im->V = im->U + width * height/4;

        im->Width = width;
        im->Height = height;
        im->PitchY = pitchY;
        im->PitchU = width/2;
        im->PitchV = width/2;

        return im;
    }

    void ReleaseImage(PlanarImage * im)
    {
#ifdef __linux__
        free(im->Y);
#else
        _aligned_free(im->Y);
#endif
        delete im;
        im = NULL;
    }


    class YUVWriter : public FrameWriter
    {
    public:
        YUVWriter(int width, int height, int frameNumHint, bool bToBMPs = false);
        virtual ~YUVWriter() {}

        void AppendFrame(PlanarImage * im);
        void WriteToFile(const char * fn);

    private:
        std::vector<uint8_t> m_data;
        bool m_bToBMPs;
    };

    void YUVWriter::WriteToFile( const char * fn )
    {
		// Disable writing to BMP
		m_bToBMPs = 0;
        if(m_bToBMPs)
        {
            std::string outfile(fn);
            std::size_t found  = outfile.find('.');
            //crop the name
            outfile =  outfile.substr(0, found);
            cl_uchar4* frame = (cl_uchar4*)malloc(m_width * m_height * sizeof(cl_uchar4));
            if (!frame)
            {
			    throw std::runtime_error("Failed to allocate a buffer for the bitmap.");
            }
            const int UVwidth  = m_width/2;
            const int UVheight = m_height/2;
            for (int y = 0; y < m_currFrame; ++y)
            {
                //Y (a value per pixel)
                const uint8_t * pImgY = &m_data[y* m_width * m_height * 3 / 2];
                //U (a value per 4 pixels) and V (a value per 4 pixels)
                const uint8_t * pImgU = pImgY + m_width * m_height;  //U plane is after Y plane (which is m_width * m_height)
                const uint8_t * pImgV = pImgU + UVwidth * UVheight;//V plane is after U plane (which is UVwidth * UVheight)

                memset(frame, 0, m_width * m_height * sizeof(cl_uchar4));
                for (int i = 0; i < m_height; ++i)
                {
                    for (int j = 0; j < m_width; ++j)
                    {
                       //Y value
                       unsigned char Y = pImgY[j + m_width*(m_height-1-i)];
                       //the same U value 4 times, thus both i and j are divided by 2
                       unsigned char U = pImgU[j/2 + UVwidth*(UVheight-1-i/2)];
                       unsigned char V = pImgV[j/2 + UVwidth*(UVheight-1-i/2)];

					   using namespace std;
					   //R is the 3rd component in the bitmap (which is actualy stored as BGRA)
                       const int R = (int)(1.164f*(float(Y) - 16) + 1.596f*(float(V) - 128));
                       frame[j + m_width*i].s[2] = min(255, max(R,0));
                       //G
                       const int G = (int)(1.164f*(float(Y) - 16) - 0.813f*(float(V) - 128) - 0.391f*(float(U) - 128));
                       frame[j + m_width*i].s[1] = min(255, max(G,0));
                       //B
                       const int B = (int)(1.164f*(float(Y) - 16) + 2.018f*(float(U) - 128));
                       frame[j + m_width*i].s[0] = min(255, max(B,0));
                    }
                }
                std::stringstream number; number<<y;
                std::string filename = outfile + number.str() + std::string(".bmp");
#ifndef __linux__
                if (!SaveImageAsBMP((unsigned int*)frame, m_width, m_height, filename.c_str()))
                {
					throw std::exception("Failed to write output bitmap file.");
                }
#endif
            }
            free(frame);
        }

        //YUV file
        std::ofstream outfile(fn, std::ios::binary);
        if (!outfile.good())
        {
		    throw std::runtime_error("Failed opening output file.");
        }
        outfile.write((char*)&m_data[0], m_data.size());
        outfile.close();
    }

    void YUVWriter::AppendFrame( PlanarImage * im )
    {
        m_data.resize((m_currFrame+1) * m_width * m_height * 3 / 2);

        uint8_t * pSrc = (uint8_t*)im->Y;
        uint8_t * pDst = &m_data[m_currFrame * m_width * m_height * 3 / 2];
        for (unsigned int y = 0; y < im->Height; ++y)
        {
            memcpy(pDst, pSrc, im->Width);
            pSrc += im->PitchY;
            pDst += im->Width;
        }

        pSrc = (uint8_t*)im->U;
        for (unsigned int y = 0; y < im->Height / 2; ++y)
        {
            memcpy(pDst, pSrc, im->Width / 2);
            pSrc += im->PitchU;
            pDst += im->Width / 2;
        }

        pSrc = (uint8_t*)im->V;
        for (unsigned int y = 0; y < im->Height / 2; ++y)
        {
            memcpy(pDst, pSrc, im->Width / 2);
            pSrc += im->PitchV;
            pDst += im->Width / 2;
        }

        ++m_currFrame;
    }

    YUVWriter::YUVWriter( int width, int height, int frameNumHint, bool bToBMPs )
        : FrameWriter(width, height), m_bToBMPs (bToBMPs)
    {
        if (frameNumHint > 0)
        {
            m_data.reserve(frameNumHint * m_width * m_height * 3 / 2);
        }
    }

    FrameWriter * FrameWriter::CreateFrameWriter(int width, int height, int frameNumHint, bool bFormatBMPHint)
    {
        return new YUVWriter(width, height, frameNumHint, bFormatBMPHint);
    }

    void FrameWriter::Release(FrameWriter * writer)
    {
        delete writer;
    }

} // namespace
