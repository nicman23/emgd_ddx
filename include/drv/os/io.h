/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: io.h
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
 *  This file contains XFree86 implementations for the OAL io.h
 *  abstractions.
 *-----------------------------------------------------------------------------
 */

#ifndef _OAL_OS_IO_H
#define _OAL_OS_IO_H

#include <xf86.h>
#include <compiler.h>
#include <pthread.h>

/*
 * XFree86 printing services take a third non-constant parameter.
 * Since that doesn't fit well with our Macros we have used the
 * non-driver printing facilities. We can make the macros look
 * just like the driver versions for consistency.
 */
static char _tmp_str[512];
static int oal_screen = 0;
#ifdef CONFIG_DEBUG
extern pthread_mutex_t *debug_log_mutex;
extern unsigned long *dropped_debug_messages;
#endif

/*
 * This is an unused inline function. It is just here to refernce the
 * static vars so that there is no compiler warning in the case that
 * they are not referenced.
 */
static __inline int unused_func( void ) {
	_tmp_str[0] = 0;
	return oal_screen;
}

/*
 * OS_PRINT is not an official OAL interface. Only the IAL uses it but it
 * is here for consistency.
 */
#ifdef CONFIG_DEBUG
#define OS_PRINT(arg...)													\
	do {																	\
		if (!pthread_mutex_trylock( debug_log_mutex )) {					\
			snprintf(_tmp_str, 512, arg);									\
			xf86Msg(X_INFO, "EMGD(%d): %s\n", oal_screen, _tmp_str);		\
			if (*dropped_debug_messages > 0) {								\
				xf86Msg(X_INFO,												\
					"EMGD: %lu debug message(s) were dropped.\n",			\
					*dropped_debug_messages);								\
				*dropped_debug_messages = 0;								\
			}																\
			pthread_mutex_unlock( debug_log_mutex );						\
		} else {															\
			(*dropped_debug_messages)++;									\
		}																	\
	} while(0);
#else
#define OS_PRINT(arg...)													\
	do {																	\
		snprintf(_tmp_str, 512, arg);										\
		xf86Msg(X_INFO, "EMGD(%d): %s\n", oal_screen, _tmp_str);			\
	} while(0);
#endif

#ifdef CONFIG_DEBUG
#define _OS_DEBUG(arg...)													\
	do {																	\
		if (!pthread_mutex_trylock( debug_log_mutex )) {					\
			snprintf(_tmp_str, 512, arg);									\
			xf86Msg(X_INFO, "EMGD(%d): %s %s\n", oal_screen,				\
				os_get_funcname(__FUNCTION__), _tmp_str);					\
			if (*dropped_debug_messages > 0) {								\
				xf86Msg(X_INFO,												\
					"EMGD: %lu debug message(s) were dropped.\n",			\
					*dropped_debug_messages);								\
				*dropped_debug_messages = 0;								\
			}																\
			pthread_mutex_unlock( debug_log_mutex );						\
		} else {															\
			(*dropped_debug_messages)++;									\
		}																	\
	} while(0);
#else
#define _OS_DEBUG(arg...)													\
	do {																	\
		snprintf(_tmp_str, 512, arg);										\
		xf86Msg(X_INFO, "EMGD(%d): %s %s\n", oal_screen,					\
			os_get_funcname(__FUNCTION__), _tmp_str);						\
	} while(0);
#endif

#ifdef CONFIG_DEBUG
#define _OS_ERROR(arg...)													\
	do {																	\
		if (!pthread_mutex_trylock( debug_log_mutex )) {					\
			snprintf(_tmp_str, 512, arg);									\
			xf86Msg(X_ERROR, "EMGD(%d): ERROR %s %s\n", oal_screen,			\
				os_get_funcname(__FUNCTION__), _tmp_str);					\
			if (*dropped_debug_messages > 0) {								\
				xf86Msg(X_INFO,												\
					"EMGD: %lu debug message(s) were dropped.\n",			\
					*dropped_debug_messages);								\
				*dropped_debug_messages = 0;								\
			}																\
			pthread_mutex_unlock( debug_log_mutex );						\
		} else {															\
			(*dropped_debug_messages)++;									\
		}																	\
	} while(0);
#else
#define _OS_ERROR(arg...)													\
	do {																	\
		snprintf(_tmp_str, 512, arg);										\
		xf86Msg(X_ERROR, "EMGD(%d): ERROR %s %s\n", oal_screen,				\
			os_get_funcname(__FUNCTION__), _tmp_str);						\
	} while(0);
#endif

#endif
