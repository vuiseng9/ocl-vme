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

For VmeApps which are advanced examples, please run ```./MotionEstimation -h``` to find out the usage.
