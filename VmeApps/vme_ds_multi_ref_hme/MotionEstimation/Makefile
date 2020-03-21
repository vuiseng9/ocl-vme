all:
	g++  -I../../Include -I/opt/intel/opencl/include -I../common -std=c++11 -Wall -O3 -mfpmath=sse -msse4.1 -fpermissive -fexceptions -Wno-deprecated-declarations -Wno-unknown-pragmas -L/opt/intel/opencl main.cpp ../common/*.cpp -o MotionEstimation -l:libOpenCL.so.1
