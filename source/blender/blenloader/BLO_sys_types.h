/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blenloader/BLO_sys_types.h
 *  \ingroup blenloader
 *
 * A platform-independent definition of [u]intXX_t
 * Plus the accompanying header include for htonl/ntohl
 *
 * This file includes <sys/types.h> to define [u]intXX_t types, where
 * XX can be 8, 16, 32 or 64. Unfortunately, not all systems have this
 * file.
 * - Windows uses __intXX compiler-builtin types. These are signed,
 *   so we have to flip the signs.
 * For these rogue platforms, we make the typedefs ourselves.
 *
 */

#ifndef __BLO_SYS_TYPES_H__
#define __BLO_SYS_TYPES_H__

#ifdef __cplusplus
extern "C" {
#endif
 
#if defined(_WIN32) && !defined(FREE_WINDOWS)

/* The __intXX are built-in types of the visual compiler! So we don't
 * need to include anything else here. */


typedef signed __int8  int8_t;
typedef signed __int16 int16_t;
typedef signed __int32 int32_t;
typedef signed __int64 int64_t;

typedef unsigned __int8  uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;

#ifndef _INTPTR_T_DEFINED
#ifdef _WIN64
typedef __int64 intptr_t;
#else
typedef long intptr_t;
#endif
#define _INTPTR_T_DEFINED
#endif

#ifndef _UINTPTR_T_DEFINED
#ifdef _WIN64
typedef unsigned __int64 uintptr_t;
#else
typedef unsigned long uintptr_t;
#endif
#define _UINTPTR_T_DEFINED
#endif

#elif defined(__linux__) || defined(__NetBSD__) || defined(__OpenBSD__)

	/* Linux-i386, Linux-Alpha, Linux-ppc */
#include <stdint.h>

/* XXX */
#ifndef UINT64_MAX
# define UINT64_MAX		18446744073709551615
typedef uint8_t   u_int8_t;
typedef uint16_t  u_int16_t;
typedef uint32_t  u_int32_t;
typedef uint64_t  u_int64_t;
#endif

#elif defined (__APPLE__)

#include <inttypes.h>

#elif defined(FREE_WINDOWS)
/* define htoln here, there must be a syntax error in winsock2.h in MinGW */
unsigned long __attribute__((__stdcall__)) htonl(unsigned long);
#include <stdint.h>

#else

	/* FreeBSD, Solaris */
#include <sys/types.h>

#endif /* ifdef platform for types */


#ifdef _WIN32
#ifndef FREE_WINDOWS
#ifndef htonl
#define htonl(x) correctByteOrder(x)
#endif
#ifndef ntohl
#define ntohl(x) correctByteOrder(x)
#endif
#endif
#elif defined (__FreeBSD__) || defined (__OpenBSD__) 
#include <sys/param.h>
#elif defined (__APPLE__)
#include <sys/types.h>
#else  /* sun linux */
#include <netinet/in.h>
#endif /* ifdef platform for htonl/ntohl */

#ifdef __cplusplus 
}
#endif

#endif /* eof */

