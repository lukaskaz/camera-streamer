#!/bin/bash

STREAMER=streamer
SRCDIR=./src
INCDIR=./inc
BINDIR=./bin

echo "[$STREAMER] Building bin file ..."
g++ $SRCDIR/main.cpp -o $BINDIR/$STREAMER -std=c++17 -pthread -I${INCDIR} -l:libopencv_highgui.so -l:libopencv_core.so -l:libopencv_videoio.so -l:libopencv_imgproc.so -l:libopencv_imgcodecs.so 

if [ $? -eq 0 ]; then
    echo "[OK] Build successfully completed"
else
    echo "[ERROR] Build not done due to errors"
fi
