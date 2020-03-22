# ocl-vme

This repo aims to provide a containerized development environment for OpenCL Applications with the extensions of Intel Video Motion Estimation (VME). We containerize the two sets of example in this article https://software.intel.com/en-us/articles/intro-ds-vme

> Tested on 6th Intel Core i7 with Gen 9 Graphics (i7-6770HQ - Skull Canyon NUC) 

### Build Docker Image

We rely on prebuilt Open Visual Cloud (OVC) development docker image that has Intel Graphics drivers, OpenCL runtime, OpenCV and some media stacks. We've patched the example codes accordingly.

```bash
git clone https://github.com/vuiseng9/ocl-vme

cd ocl-vme

sudo docker build . -f docker/Dockerfile -t ocl-vme-dev
```

### Prebuilt Docker Image
Alternatively, you can pull this image to run locally, however it only works on compatible hardware.
```bash
sudo docker pull vuiseng9/ocl-vme-dev
```
[Image Link](https://hub.docker.com/repository/docker/vuiseng9/ocl-vme-dev)

### Run Docker
The following script returns a docker runtime shell.
```bash
cd ocl-vme
./docker/run_docker.sh
```

### Run Examples
Following are performed in the container runtime.

```bash
# The basic usage of VME
cd /home/ocl-vme/MotionEstimation_ds_basic
./run.sh
```

For VmeApps which are advanced examples, please run thier respective ```README.txt``` and ```./MotionEstimation -h``` to find out the usage.

## **Motion Vector extraction**
```ime_mv_extract/``` is modified from to convert motion vectors (MVs) to linear format, ie in ascending x and y direction from the initial Macroblock-based raster scan order. Note that we use *VME* (Video Motion Estimation) and *IME* (Intel Motion Estimation) interchangeably.

```bash
### Build
cd ocl-vme/ime_mv_extract/
mkdir build && cd build
cmake ..
make

### Run
cd ocl-vme/ime_mv_extract/
./run_ime_mv_extract.sh
```

The example ```./run_ime_mv_extract.sh``` runs with two frames yuv - Dimetrodon.yuv. It creates Dimetrodon.MV.yuv which is a visualization of Motion Vector (this is function provided by Intel examples). On top of that, it creates a .flo and a dense .flo which a type of format representing MV in linear format. The difference between dense and non-dense .flo is that dense.flo has the MV upsampled to its original resolution and non-dense.flo is the VME resolution, say 1 MV per 4x4 pixel. Please see the code if you would like to understand the routine of unpacking MV to linear format. Caveat: The MV extraction currently does not consider the prediction mode of the macroblock yet.


