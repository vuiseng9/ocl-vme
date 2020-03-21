******************************************************************************
**              Intel(R) SDK for OpenCL* Applications - Samples             **
**                                 README                                   **
******************************************************************************




*****  Overview  *****

This package contains the sample that targets the Intel Processor Graphics
device. The sample is supported on Microsoft Windows* OS.

Refer to the sample User's Guide for details about the sample.

For the complete list of supported operating systems and hardware, refer to
the release notes.


*****  Sample Directory Content  *****

Sample files reside in the dedicated sample directory and in the 'common'
directory in the root-level (where the sample is extracted) directory.

The sample directory contains the following:

  - common                 -- directory with common utilities and helpers;
                              this functionality is used as a basic
                              infrastructure in the sample code
  - MotionEstimation       -- directory with sample files:

        - main.cpp         -- host code for the sample, including the
                              application entry point, extensions
                              initialization, resources management and VME
                              kernel invocation.
        - yuv_utils.h      -- basic routine to read/write simple YUV streams
                              (YV12)
        - yuv_utils.cpp    -- YV12 is 8 bit Y plane followed by 8 bit 2x2
                              subsampled (which is a value per 4 pixels)
                              V and U planes
 
        - cmdparser.cpp    -- command-line parameters parsing routines
          cmdparser.hpp
        - utils.cpp        -- general routines like writing bmp files
          utils.h
        - oclobject.cpp    -- general OpenCL* initialization routine
          oclobject.hpp
        - vme_scoreboard.cl -- OpenCL kernel file that performs multi-
          reference VME operations with shape, direction, motion vector cost 
          and intra mode costing. It uses a software-based scoreboarding 
          mechainism to determine macro-block dependency and use the results 
          from neighboring macroblocks to process the current macro-block.

  - templates              -- project Property files
  - user_guide.pdf         -- sample User's Guide
  - README.TXT             -- readme file





***** Running the Sample *****

You can run the sample application using the standard interface of the
Microsoft Visual Studio IDE, or using the command line.

To run the sample using the Visual Studio IDE, do the following:

1. Open and build the appropriate solution file.
2. Press Ctrl+F5 to run the application. To run the application in debug mode,
   press F5.

To run the sample using command line, do the following:

1. Optionally edit the sample source file main.cpp to configure for using:
	- HD or SD input source (macro USE_HD or USE_SD_720_576 or )
	- Output block partitions or motion vectors (macro SHOW_BLOCKS). 
	  The output blocks and motion vectors are color coded to indicate the
          reference frame that the block/motion vector was predicted from:
	      * violet indicates 1st reference frame
	      * indigo indicates 2nd reference frame
	      * blue indicates 3rd reference frame
	      * green indicates 4th reference frame
	      * yellow indicates 5th reference frame
	      * orange indicates 6th reference frame
	      * red indicates 7th reference frame
          * ivory indicates 8th reference frame
          * lavander indicates 9th reference frame
          * pink indicates 10th reference frame
          * gold indicates 11th reference frame
          * tan indicates 12th reference frame
          * teal indicates 13th reference frame
          * maroon indicates 14th reference frame
          * gray indicates 15th reference frame
          * black indicates 16th reference frame

   Optionally edit the sample source file vme_scoreboard.cl to vary the QP
   (local variable "uchar qp"). Since the video samples are high motion samples
   a high QP value of 45 has been used.

   It can be observed that at higher QPs there are noticeably lesser number 
   of predictions from reference frames that are more distant from the source
   frame. This is because it takes more bits to signal encoding of frames that
   are more distant which is counter-intutive for higher QPs when we want to 
   restrict the bit rate more aggressively.

   It can also be observed that at higher QPs there are noticeably higher
   number of larger partition shapes. This is because it takes more bits to
   signal encoding of smaller partitions which is counter-intutive for higher
   QPs when we want to restrict the bit rate more aggressively.

2. Open the command prompt.

3. Navigate to the sample directory.

4. Then go to the directory according to the configuration you built in the
   previous step:
    - \Win32 - for Win32 configuration
    - \x64 - for x64 configuration

5. Select the appropriate configuration folder  (Debug or Release).

6. Run the sample by entering the name of the executable.

7. You can run the sample with command-line option -h or --help to print
   available command line options for the sample.


*****  Disclaimer and Legal Information *****

THESE MATERIALS ARE PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR ITS
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THESE
MATERIALS, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

INFORMATION IN THIS DOCUMENT IS PROVIDED IN CONNECTION WITH INTEL
PRODUCTS. NO LICENSE, EXPRESS OR IMPLIED, BY ESTOPPEL OR OTHERWISE,
TO ANY INTELLECTUAL PROPERTY RIGHTS IS GRANTED BY THIS DOCUMENT.
EXCEPT AS PROVIDED IN INTEL'S TERMS AND CONDITIONS OF SALE FOR SUCH
PRODUCTS, INTEL ASSUMES NO LIABILITY WHATSOEVER AND INTEL DISCLAIMS
ANY EXPRESS OR IMPLIED WARRANTY, RELATING TO SALE AND/OR USE OF INTEL
PRODUCTS INCLUDING LIABILITY OR WARRANTIES RELATING TO FITNESS FOR
A PARTICULAR PURPOSE, MERCHANTABILITY, OR INFRINGEMENT OF ANY PATENT,
COPYRIGHT OR OTHER INTELLECTUAL PROPERTY RIGHT.

A "Mission Critical Application" is any application in which failure
of the Intel Product could result, directly or indirectly, in personal
injury or death. SHOULD YOU PURCHASE OR USE INTEL'S PRODUCTS FOR ANY
SUCH MISSION CRITICAL APPLICATION, YOU SHALL INDEMNIFY AND HOLD INTEL
AND ITS SUBSIDIARIES, SUBCONTRACTORS AND AFFILIATES, AND THE DIRECTORS,
OFFICERS, AND EMPLOYEES OF EACH, HARMLESS AGAINST ALL CLAIMS COSTS,
DAMAGES, AND EXPENSES AND REASONABLE ATTORNEYS' FEES ARISING OUT OF,
DIRECTLY OR INDIRECTLY, ANY CLAIM OF PRODUCT LIABILITY, PERSONAL INJURY,
OR DEATH ARISING IN ANY WAY OUT OF SUCH MISSION CRITICAL APPLICATION,
WHETHER OR NOT INTEL OR ITS SUBCONTRACTOR WAS NEGLIGENT IN THE DESIGN,
MANUFACTURE, OR WARNING OF THE INTEL PRODUCT OR ANY OF ITS PARTS.

Intel may make changes to specifications and product descriptions at
any time, without notice. Designers must not rely on the absence or
characteristics of any features or instructions marked "reserved" or
"undefined". Intel reserves these for future definition and shall have
no responsibility whatsoever for conflicts or incompatibilities arising
from future changes to them. The information here is subject to change
without notice. Do not finalize a design with this information.

The products described in this document may contain design defects or
errors known as errata which may cause the product to deviate from
published specifications. Current characterized errata are available
on request.

Contact your local Intel sales office or your distributor to obtain the
latest specifications and before placing your product order.

Copies of documents which have an order number and are referenced in
this document, or other Intel literature, may be obtained
by calling 1-800-548-4725, or go to:
http://www.intel.com/design/literature.htm

Intel Corporation is the author of the Materials, and requests that all
problem reports or change requests be submitted to it directly.

Intel Core, HD Graphics and Iris Graphics are trademarks of Intel
Corporation in the U.S. and/or other countries.

* Other names and brands may be claimed as the property of others.

OpenCL and the OpenCL logo are trademarks of Apple Inc. used by
permission from Khronos.

Copyright (c) 2016 Intel Corporation. All rights reserved.
