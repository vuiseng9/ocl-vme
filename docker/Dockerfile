FROM openvisualcloud/xeone3-ubuntu1604-dev:20.1

WORKDIR /home

COPY ./MotionEstimation_ds_basic /home/ocl-vme/MotionEstimation_ds_basic/
COPY ./VmeApps /home/ocl-vme/VmeApps/
COPY ./ime_mv_extract /home/ocl-vme/ime_mv_extract

RUN apt-get update && apt-get install -y \
    git vim curl wget zip qtcreator; \
    rm -rf /var/lib/apt/lists/*

# Install CMake
RUN wget https://github.com/Kitware/CMake/releases/download/v3.15.3/cmake-3.15.3-Linux-x86_64.sh && chmod +x cmake-3.15.3-Linux-x86_64.sh && ./cmake-3.15.3-Linux-x86_64.sh --prefix=/usr/local --skip-license && rm -f cmake-3.15.3-Linux-x86_64.sh

# OpenCL headers
# Do note that cl_ext.h which has been used by Intel VME examples is renamed to cl_ext_intel.h in OpenCL-Headers
# c++ headers are downloaded from github releases
RUN git clone https://github.com/KhronosGroup/OpenCL-Headers.git && \
    cd OpenCL-Headers/CL && \
    wget https://raw.githubusercontent.com/crosswalk-project/khronos-cl-api-1.2/master/cl.hpp && \
    wget https://github.com/KhronosGroup/OpenCL-CLHPP/releases/download/v2.0.10/cl2.hpp && \
    cd .. && cp -r CL /usr/include/


# Rebuild OpenCV with Contrib Modules
ARG OPENCV_VER=4.1.0
ARG OPENCV_REPO=https://github.com/opencv/opencv/archive/${OPENCV_VER}.tar.gz
ARG OPENCV_CONTRIB_REPO=https://github.com/opencv/opencv_contrib/archive/${OPENCV_VER}.tar.gz

RUN DEBIAN_FRONTEND=noninteractive apt-get update && apt-get install -y \
    libv4l-dev \
    python-dev python-pip python-numpy \
    python3-dev python3-pip python3-numpy \
    libjpeg-dev libtiff-dev libjasper-dev libdc1394-22-dev \
    libatlas-base-dev libeigen3-dev liblapacke-dev \
    libgoogle-glog-dev libgflags-dev \
    && rm -rf /var/lib/apt/lists/*

RUN pip uninstall -y numpy && pip install numpy

RUN wget ${OPENCV_REPO} && \
    tar -zxvf ${OPENCV_VER}.tar.gz && rm -f ${OPENCV_VER}.tar.gz && \
    wget ${OPENCV_CONTRIB_REPO} && \
    tar -zxvf ${OPENCV_VER}.tar.gz && rm -f ${OPENCV_VER}.tar.gz && \
    cd opencv-${OPENCV_VER} && \
    mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release -DOPENCV_EXTRA_MODULES_PATH=/home/opencv_contrib-${OPENCV_VER}/modules -DCMAKE_INSTALL_PREFIX=/usr/local -D BUILD_EXAMPLES=ON -D BUILD_PERF_TESTS=OFF -D BUILD_DOCS=OFF -D BUILD_TESTS=OFF .. && \
    make -j $(nproc) && \
    make install && \
    mkdir /home/opencv-examples && cp bin/* /home/opencv-examples/ && \
    make clean

RUN cd /home/ocl-vme/MotionEstimation_ds_basic && make && \
    cd /home/ocl-vme/VmeApps/vme_ds_basic_interlaced/MotionEstimation && make && \
    ln -sv /home/ocl-vme/MotionEstimation_ds_basic/video_1920x1080_5frames.yuv . && \
    cd /home/ocl-vme/VmeApps/vme_ds_multi_ref_hme_swsb/MotionEstimation && make && \
    ln -sv /home/ocl-vme/MotionEstimation_ds_basic/video_1920x1080_5frames.yuv . && \
    cd /home/ocl-vme/VmeApps/vme_ds_advanced_chroma/MotionEstimation && make  \
    ln -sv /home/ocl-vme/MotionEstimation_ds_basic/video_1920x1080_5frames.yuv . && \
    cd /home/ocl-vme/VmeApps/vme_ds_ime_swsb/MotionEstimation && make && \
    ln -sv /home/ocl-vme/MotionEstimation_ds_basic/video_1920x1080_5frames.yuv . && \
    cd /home/ocl-vme/VmeApps/vme_ds_bidir/MotionEstimation && make && \
    ln -sv /home/ocl-vme/MotionEstimation_ds_basic/video_1920x1080_5frames.yuv . && \
    cd /home/ocl-vme/VmeApps/vme_ds_multi_ref_hme/MotionEstimation && make && \
    ln -sv /home/ocl-vme/MotionEstimation_ds_basic/video_1920x1080_5frames.yuv . 

RUN cd /home/ocl-vme/ime_mv_extract && \
    mkdir build && cd build && \
    cmake .. && make

WORKDIR /home/ocl-vme
