/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: debug.h
 *-----------------------------------------------------------------------------
 * Copyright (c) 2002-2013, Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 *
 *-----------------------------------------------------------------------------
 * Description:
 *  This file contains IAL debug bits definition for debug printing.
 *-----------------------------------------------------------------------------
 */

#ifndef _OS_DEBUG_H
#define _OS_DEBUG_H

typedef struct _os_debug {
	unsigned long driver    :1;
	unsigned long buffer    :1;
	unsigned long escape    :1;
	unsigned long accel     :1;
	unsigned long dri       :1;
	unsigned long native    :1;
	unsigned long options   :1;
	unsigned long overlay   :1;
	unsigned long video     :1;
	unsigned long crtc      :1;
	unsigned long output    :1;
	/* Global Debug Bits */
	unsigned long trace     :1;
	unsigned long debug     :1;
	
	unsigned long _unused   :19;
} os_debug_t;

typedef struct _igd_drv_debug {
	os_debug_t ial;
} igd_drv_debug_t;

extern igd_drv_debug_t *igd_debug;

#endif
