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

#ifndef VA_XVCOMMON_H
#define VA_XVCOMMON_H

#define VAXV_MAJOR               0
#define VAXV_MINOR               1

#define VAXV_VERSION             ((VAXV_MAJOR << 16) | VAXV_MINOR)

/* VAXV port cababilities
 */
/* The shared buffer object in put image are directly used on overlay/sprite */
#define VAXV_CAP_DIRECT_BUFFER   1

/* Scalling support */
#define VAXV_CAP_SCALLING_UP     (1 << 4)
#define VAXV_CAP_SCALLING_DOWN   (1 << 5)
#define VAXV_CAP_SCALLING_FLAG   VAXV_CAP_SCALLING_UP | VAXV_CAP_SCALLING_DOWN 

/* Rotation support */
#define VAXV_CAP_ROTATION_90     (1 << 8)
#define VAXV_CAP_ROTATION_180    (1 << 9)
#define VAXV_CAP_ROTATION_270    (1 << 10)
#define VAXV_CAP_ROTATION_FLAG   VAXV_CAP_ROTATION_90 | VAXV_CAP_ROTATION_180 | VAXV_CAP_ROTATION_270

typedef struct _va_xv_buf {
    unsigned int  buf_handle;
    unsigned long pixel_format;
    int buf_width, buf_height;
    unsigned int pitches[3];
    unsigned int offsets[3];
} va_xv_buf_t;

#endif
