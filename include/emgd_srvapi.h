/*
 *-----------------------------------------------------------------------------
 * Filename: emgd_api.h
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
 *-----------------------------------------------------------------------------
 * Description:
 *  Include file for Intel's driver control protocol and client library
 *-----------------------------------------------------------------------------
 */
#ifndef _EMGD_API_H_
#define _EMGD_API_H_

#include <X11/Xfuncproto.h>
#include <X11/Xmd.h>

#define X_EMGDQueryVersion        0
#define X_EMGDSetClientVersion    1
#define X_EMGDControl             2

#ifndef _EMGD_SERVER_
/*
 * Function prototypes for client applications.
 */

_XFUNCPROTOBEGIN

Bool emgd_query_version(
	Display*	/* dpy */,
	int*		/* majorVersion */,
	int*		/* minorVersion */
);

Bool emgd_query_extension(
	Display*	/* dpy */,
	int*		/* event_base */,
	int*		/* error_base */
);

Bool emgd_set_client_version(
	Display*	/* dpy */
);

int emgd_api(
	Display*         /* dpy */,
	unsigned long    /* api function code */,
	unsigned long    /* input data size */,
	void*            /* input data pointer */,
	unsigned long    /* output data size */,
	void*            /* output data pointer */
) ;

int iegd_uninstall_extension(Display *);

_XFUNCPROTOEND

#endif

#endif
