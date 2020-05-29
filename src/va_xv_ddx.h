/*
 * Copyright (C) 2012 Intel Corporation. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef VA_XV_DDX_H
#define VA_XV_DDX_H

#include "va_xvcommon.h"

#define VAXV_ATTR_STR            "VAXV_SHMBO"
#define VAXV_VERSION_ATTR_STR    "VAXV_VERSION"
#define VAXV_CAPS_ATTR_STR       "VAXV_CAPS"
#define VAXV_FORMAT_ATTR_STR     "VAXV_FORMAT"

#define FOURCC_VAXV              (('V' << 24) | ('X' << 16) | ('A' << 8) | 'V')

typedef struct _va_xv_put_image {
    int size;
    va_xv_buf_t video_image;
    unsigned int flags;
} va_xv_put_image_t;

#endif
