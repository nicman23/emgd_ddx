#!/bin/sh
export EGD_ROOT=`pwd`/../
export PROJECT_OUTPUT_PATH=`pwd`
export XFREE=/usr/include/xorg/
export KBUILD_NOPEDANTIC=1

make $@

