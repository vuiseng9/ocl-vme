#!/usr/bin/env bash

container=ocl-vme-dev

sudo xhost +local:`sudo docker inspect --format='{{ .Config.Hostname }}' $container`

sudo docker run $DOCKER_PROXY_RUN_ARGS \
    -e DISPLAY=$DISPLAY \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    -v /home/${USER}:/hosthome \
    --device=/dev/dri:/dev/dri \
    --privileged \
    -w /home \
    -it ${container} bash
