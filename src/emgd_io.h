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
 *  This file contains OS abstracted interfaces to common io operations.
 *-----------------------------------------------------------------------------
 */

#ifndef _OAL_IO_H
#define _OAL_IO_H


/*
 * Debug macros rely on the existence of igd_debug pointer. It is defined
 * in the HAL. If an IAL wishes to use the print routines and is not
 * linked with the HAL it will need to provide an igd_debug pointer.
 */

#ifndef NULL
#define NULL ((void *)0)
#endif

/*
 * All files including io.h should define MODULE_NAME to be a bit
 * within the igd_debug_t data structure. If MODULE_NAME is not defined
 * debug printing will be controlled by the global "debug" bit rather
 * than a per-module bit.
 */
#ifndef MODULE_NAME
#define MODULE_NAME ial.debug
#endif

/*
 * Debug Print Macros use os_get_funcname to get the name of the
 * function they are in. They cannot rely on __func__ alone because
 * the function name may be overridden by the caller.
 *
 * If the caller wants to override the printed function name they may
 * call os_set_funcname prior to each call to OS_DEBUG/ERROR/TRACE.
 */
static const char *_os_override_funcname = NULL;

static __inline int os_set_funcname( const char *name )
{
	_os_override_funcname = name;
	return 1;
}

static __inline const char *os_get_funcname( const char *name )
{
	const char *ret;
	ret = (_os_override_funcname)?_os_override_funcname:name;
	_os_override_funcname = NULL;
	return ret;
}

/*
 * Include OS dependent header that may implement required porting
 * layer or override these OS independent implementations.
 */
#include <os/debug.h>
#include <os/io.h>

/*
 * OS_ERROR()
 * Printing with OS_ERROR will be compiled in to all debug drivers and
 * all production drivers. OS_ERROR printing is used for
 * driver errors that should not happen under normal operation. As such,
 * these messages cannot be disabled at runtime.
 *
 * All OAL implementations should result in OS_ERROR messsages in the
 * following format:
 * <OPTIONAL OS ERROR PREFIX> FUNCTION_NAME Message
 *
 * The FUNCTION_NAME must be obtained by calling
 * os_get_funcname(__func__) to insure that any overridden function names
 * are respected.
 */
#ifndef OS_ERROR
#define OS_ERROR _OS_ERROR
#endif

/*
 * OS_DEBUG()
 * Printing with OS_DEBUG will be compiled in to all debug drivers and
 * removed from all production drivers. OS_DEBUG printing is used for
 * debug messages that give information useful for debugging problems.
 * OS_DEBUG messages can be enabled/disabled at compile time and runtime
 * on a per-module basis.
 *
 * Disabling/Enabling module bits in CONFIG_DEBUG_FLAGS will control the
 *  default printing of debug messages.
 * Disabling/Enabling the bit in igd_debug->MODULE_NAME will control
 *  the runtime printing of debug messages.
 *
 * All OAL implementations should result in OS_DEBUG messsages in the
 * following format:
 * <OPTIONAL OS PREFIX> function_name Message
 *
 * The FUNCTION_NAME must be obtained by calling
 * os_get_funcname(__func__) to insure that any overridden function names
 * are respected.
 */
#ifndef OS_DEBUG
#ifdef CONFIG_DEBUG
#ifdef PER_MODULE_DEBUG
#define OS_DEBUG if(igd_debug && igd_debug->MODULE_NAME) _OS_DEBUG
#else
#define OS_DEBUG _OS_DEBUG
#endif
#else
#define OS_DEBUG(arg...) do {} while(0)
#endif
#endif

/*
 * OS_TRACE_ENTER
 * Tracing with OS_TRACE_ENTER will be compiled in to all debug drivers and
 * removed from all production drivers. OS_TRACE_ENTER will print a fixed
 * "Enter" message when entering a function.
 * OS_TRACE_ENTER messages can be enabled/disabled at compile time and
 * runtime on a per-module basis. To Enable tracing in a module, the Global
 * igd_debug->trace bit must be enabled as well as the per-module
 * debug bit.
 *
 * Disabling/Enabling module bits in CONFIG_DEBUG_FLAGS will control the
 *  default printing of debug messages.
 * Disabling/Enabling the bit in igd_debug->MODULE_NAME will control
 *  the runtime printing of debug messages.
 *
 * All OAL implementations should result in OS_TRACE_ENTER messsages in the
 * following format:
 * <OPTIONAL OS PREFIX> function_name ENTER
 */
#ifndef OS_TRACE_ENTER
#ifdef CONFIG_DEBUG
#ifdef PER_MODULE_DEBUG
#define OS_TRACE_ENTER if(igd_debug && igd_debug->ial.trace) OS_DEBUG("ENTER")
#else
#define OS_TRACE_ENTER OS_DEBUG("ENTER")
#endif
#else
#define OS_TRACE_ENTER
#endif
#endif

/*
 * OS_TRACE_EXIT
 * Tracing with OS_TRACE_EXIT will be compiled in to all debug drivers and
 * removed from all production drivers. OS_TRACE_EXIT will print a fixed
 * "Exit" message when exiting a function without error.
 * OS_TRACE_EXIT messages can be enabled/disabled at compile time and
 * runtime on a per-module basis. To Enable tracing in a module, the Global
 * igd_debug->trace bit must be enabled as well as the per-module
 * debug bit.
 *
 * Disabling/Enabling module bits in CONFIG_DEBUG_FLAGS will control the
 *  default printing of debug messages.
 * Disabling/Enabling the bit in igd_debug->MODULE_NAME will control
 *  the runtime printing of debug messages.
 *
 * All OAL implementations should result in OS_TRACE_EXIT messsages in the
 * following format:
 * <OPTIONAL OS PREFIX> function_name EXIT
 */
#ifndef OS_TRACE_EXIT
#ifdef CONFIG_DEBUG
#ifdef PER_MODULE_DEBUG
#define OS_TRACE_EXIT if(igd_debug && igd_debug->ial.trace) OS_DEBUG("EXIT")
#else
#define OS_TRACE_EXIT OS_DEBUG("EXIT")
#endif
#else
#define OS_TRACE_EXIT
#endif
#endif

/*
 * OS_ERROR_EXIT()
 * Tracing with OS_ERROR_EXIT will be compiled in to all debug drivers and
 * removed from all production drivers. OS_ERROR_EXIT will print an error
 * as well as a fixed "Exit" message when exiting a function without error.
 * OS_ERROR_EXIT messages can be enabled/disabled at compile time and
 * runtime on a per-module basis. To Enable tracing in a module, the Global
 * igd_debug->trace bit must be enabled as well as the per-module
 * debug bit.
 *
 * Note: Only the Tracing message can be disabled, the error message will
 * still print as with OS_ERROR().
 *
 * Disabling/Enabling module bits in CONFIG_DEBUG_FLAGS will control the
 *  default printing of debug messages.
 * Disabling/Enabling the bit in igd_debug->MODULE_NAME will control
 *  the runtime printing of debug messages.
 *
 * All OAL implementations should result in OS_ERROR_EXIT messsages in the
 * following format:
 * <OPTIONAL OS ERROR PREFIX> function_name EXIT Message
 */
#ifndef OS_ERROR_EXIT
#ifdef CONFIG_DEBUG
#ifdef PER_MODULE_DEBUG
#define OS_ERROR_EXIT													\
	if(igd_debug && igd_debug->ial.trace) OS_DEBUG("EXIT With Error..."); \
	OS_ERROR
#else
#define OS_ERROR_EXIT 													\
	OS_DEBUG("EXIT With Error...");										\
	OS_ERROR
#endif
#else
#define OS_ERROR_EXIT OS_ERROR
#endif
#endif


/*
 * OS_VERBOSE()
 * Verbose printing that is useful only when debugging specific problems
 * or contains lots of text should be implemented in a single function and
 * called from within the OS_VERBOSE macro. The default and runtime
 * control of this output can be altered by changing the value of the
 * global igd_debug->verbose.
 */
#ifndef OS_VERBOSE
#ifdef CONFIG_DEBUG
/* FIXME - THIS ISN'T CORRECTLY IMPLEMENTED, PER THE PAST, BUT ISN'T USED EITHER
 */
#define OS_VERBOSE(opt, func)  func;
#else
#define OS_VERBOSE(opt, func)
#endif
#endif

/*
 * OS_ASSERT( Statement, Message, Error Code)
 * OS_ASSERT should be used to verify the values of statements that
 * should always be true. OS_ASSERT will generate code only in debug drivers
 * and therefore should only be used for things that should never be false
 * in production drivers. For example, testing for null parameters in
 * an internal interface.
 *
 * Usage: OS_ASSERT(char_ptr, "Char Pointer is NULL", -IGD_ERROR_INVAL)
 * Usage: OS_ASSERT(char_ptr, "Char Pointer is NULL", )
 */
#ifndef OS_ASSERT
#ifdef OS_DEBUG
#define OS_ASSERT(a, m, e) if(!(a)) { OS_ERROR_EXIT("ASSERT: " m); return e; }
#else
#define OS_ASSERT(a, m, e)
#endif
#endif

#endif
