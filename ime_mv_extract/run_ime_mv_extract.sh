#!/usr/bin/env bash

ROOTDIR="./"
APP=${ROOTDIR}/build/bin/ime_mv_extract

echo "Executing... $APP"

$APP --input ./Dimetrodon/Dimetrodon.yuv \
     --output ./Dimetrodon/Dimetrodon.MotionVector.yuv \
     --width 584 --height 388 

