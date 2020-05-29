/*
 *-----------------------------------------------------------------------------
 * Filename: emgd_apistr.h
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
 *  EMGD extension protocol request/reply structures, etc.
 *
 *  Used by the ddx driver code that implements the protocol and for building
 *  the client library.
 *
 *  Do client applications need to include this?
 *-----------------------------------------------------------------------------
 */
#ifndef _EMGD_APISTR_H_
#define _EMGD_APISTR_H_

/* Include externally visable header file */
#include "emgd_srvapi.h"
#include "emgd_api.h"

#define EMGDNAME "Intel-EmbeddedGraphicsDriverExtension"

#define EGD_ESC_MAJOR_VERSION	1	/* current version numbers */
#define EGD_ESC_MINOR_VERSION	3
#define EGD_NUM_ERRORS 2
#define EGD_BAD_CONTEXT 0
#define EGD_BAD_SURFACE 1

typedef struct _emgd_query_version {
	CARD8	reqType;		/* always IegdReqCode */
	CARD8	emgdReqType;	/* always X_IegdQueryVersion */
	CARD16	length B16;
} xEMGDQueryVersionReq;
#define sz_xEMGDQueryVersionReq	4

typedef struct {
	BYTE	type;			/* X_Reply */
	BOOL	pad1;
	CARD16	sequenceNumber B16;
	CARD32	length B32;
	CARD16	major_version B16;	/* major version of EGD */
	CARD16	minor_version B16;	/* minor version of EGD */
	CARD32	pad2 B32;
	CARD32	pad3 B32;
	CARD32	pad4 B32;
	CARD32	pad5 B32;
	CARD32	pad6 B32;
} xEMGDQueryVersionReply;
#define sz_xEMGDQueryVersionReply	32

typedef struct _IegdSetClientVersion {
	CARD8	reqType;		/* always IegdReqCode */
	CARD8	emgdReqType;
	CARD16	length B16;
	CARD16	major B16;
	CARD16	minor B16;
} xEMGDSetClientVersionReq;
#define sz_xEMGDSetClientVersionReq	8

typedef struct _emgd_api {
	CARD8 reqType;
	CARD8 emgdReqType;
	CARD16 length B16;
	CARD32 api_function B32;
	CARD32 in_size B32;
	CARD32 do_reply B32;
} xEMGDControlReq;
#define sz_xEMGDControlReq 16

typedef struct {
	BYTE	type;			/* X_Reply */
	BOOL	pad1;
	CARD16	sequenceNumber B16;
	CARD32	length B32;
	CARD32	out_length  B32; /* output data size */
	CARD32	status B32;  /* status */
	CARD32	pad2 B32;
	CARD32	pad3 B32;
	CARD32	pad4 B32;
	CARD32	pad5 B32;
} xEMGDControlReply;
#define sz_xEMGDControlReply	32

#endif /* _EMGD_APISTR_H_ */

