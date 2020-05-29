#----------------------------------------------------------------------------
# Filename: Makefile.gnu
# $Revision: 1.3.18.6 $
#----------------------------------------------------------------------------
# Copyright (c) 2002-2013, Intel Corporation.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
# 
#----------------------------------------------------------------------------
export EGD_TOPLEVEL = Xorg UXA Module

LIB = libuxa
SOURCES = uxa.c \
		  uxa-render.c \
		  uxa-accel.c  \
		  uxa-glyphs.c \
		  uxa-unaccel.c

BUILD_DEPENDENCIES = \
	pixman-1 \
	libdrm

PROJECT_INCLUDES = \
	$(EGD_ROOT)/include \
	$(EGD_ROOT)/include/oal \
	$(EGD_ROOT)/include/drv \
	$(EGD_ROOT)/include/drv/os \
	$(EGD_ROOT)/uxa \
	$(EGD_ROOT)/src

PROJECT_OUTPUT_PATH := $(EGD_ROOT)/uxa

PROJECT_BUILD_XDRIVER = 1

include $(EGD_ROOT)/build/Makefile.include

