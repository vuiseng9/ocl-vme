CC=g++
CFLAGS=-I/opt/intel/opencl/include -I. -std=c++11  -Wno-deprecated-declarations
LDFLAGS=-l:libOpenCL.so.1 -L/opt/intel/opencl

EXEC=MotionEstimation

all:
	    $(CC) -o $(EXEC) *.cpp $(CFLAGS) $(LDFLAGS)

clean:
	    rm -f $(EXEC) output.yuv output_goal_1280x720.yuv




