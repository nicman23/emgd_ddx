#----------------------------------------------------------------------------
# Filename: Makefile.gnu
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

export EGD_ROOT ?= $(CURDIR)/..
export EGD_PKG_EXT ?= pkg
export EGD_PKG ?= $(EGD_ROOT)/$(EGD_PKG_EXT)

export EGD_TOPLEVEL = X Driver

export XFREE=/usr/include/xorg/

# Determine the version and build numbers
BUILDNUM=$(shell grep BUILD_NUM     ddx_version.h | awk '{ print $$3 }')
MAJORNUM=$(shell grep IGD_MAJOR_NUM ddx_version.h | awk '{ print $$3 }')
MINORNUM=$(shell grep IGD_MINOR_NUM ddx_version.h | awk '{ print $$3 }')
RELEASENUM="$(IGD_MAJOR_NUM).$(IGD_MINOR_NUM)"

KERNELVER= $(shell uname -r)
KERNELDIR= /lib/modules/$(KERNELVER)/build

SOURCES = emgd_ddx.c  \
		  emgd_options.c \
		  emgd_api.c \
		  emgd_crtc.c \
		  emgd_output.c \
		  emgd_uxa.c \
		  emgd_drm_bo.c \
		  emgd_dri2.c \
		  emgd_video.c \
		  intel_batchbuffer.c \
		  i965_3d.c \
		  i965_render.c \
		  i965_video.c \
		  emgd_sprite.c \

#
# Header files that cause source files to be recompiled.
#
HEADERS = emgd.h       \
	emgd_uxa.h \
	emgd_crtc.h \
	emgd_dri2.h \
	emgd_drm_bo.h \
	emgd_video.h \
	brw_defines.h \
	brw_structs.h \
	intel_batchbuffer.h \
	i830_reg.h \
	i965_reg.h \

#
# External packages that EMGD requires to build against.  pkgconfig will
# be used to fetch the proper cflags and ldflags.  As usual, PKG_CONFIG_PATH
# can be used to point to a separate pkgconfig cache if newer versions are
# provided in a nonstandard location.
#
BUILD_DEPENDENCIES = \
  xorg-server \
	pixman-1 \
	xcomposite \
	xpm \
	libdrm \
	xproto

#
# EMGD-internal header file locations
#
PROJECT_INCLUDES = \
  $(KERNELDIR)/include/uapi \
  $(KERNELDIR)/include/ \
	$(EGD_ROOT)/include \
	$(EGD_ROOT)/include/oal \
	$(EGD_ROOT)/include/drv \
	$(EGD_ROOT)/include/drv/os \
	$(EGD_ROOT)/include/os \
	$(EGD_ROOT)/include/shared \
	$(EGD_ROOT)/uxa \
	$(EGD_ROOT)/src/render_program 

PROJECT_OUTPUT_PATH := $(EGD_ROOT)/src
PROJECT_BUILD_XDRIVER = 1

NOSTDLIB=-nostdlib

include $(EGD_ROOT)/build/Makefile.include


PTHREAD = -lpthread
EGD_DRV = emgd_drv.so

EGD_LIBS = \
	$(PTHREAD) \
	-ldrm_intel \
	$(EGD_ROOT)/uxa/libuxa.a

clean::
	rm -f $(EGD_DRV) $(OBJECTS)


all:: $(EGD_DRV)

ifneq ($(origin EGD_INS), undefined)
DRV_DEST = $(EGD_INS)/$(EGD_DRV)
endif

$(EGD_DRV): $(OBJECTS) $(LIBS) Makefile.gnu
	$(MAKE) -C $(EGD_ROOT)/uxa BUILD=$(BUILD)
	@echo -e " $(GREEN)Linking $@$(OFF)"
	$(CC) -pthread -Wl,--version-script=symbols.map -fvisibility=hidden \
		  $(LDFLAGS) \
	      $(NOSTDLIB) -Wl,-soname,$(EGD_DRV) -shared -o $@ \
	      $(OBJECTS) $(EGD_LIBS) $(SERVER_LIBS)
	if [ "$(IGD_DEBUG)" != "-DCONFIG_DEBUG" ]; then \
		strip -x $@; \
	fi

clean::
	$(MAKE) -C $(EGD_ROOT)/uxa clean

install::
	echo -e "$(GREEN)Installing $(EGD_TOPLEVEL)$(OFF)"
	echo -e " $(GREEN)Installing $(DRV_DEST) $(OFF)"
	install -D -c -m 0755 $(EGD_DRV) $(DRV_DEST);

package::
	mkdir -p $(EGD_PKG)/driver/$(XDDK_TYPE)
	$(MAKE) EGD_INS=$(EGD_PKG)/driver/$(XDDK_TYPE) install
