/*
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
 */

/** \file
 * \ingroup bli
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
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__linux__) || defined(__GNU__) || defined(__NetBSD__) || defined(__OpenBSD__) || \
    defined(__FreeBSD_kernel__) || defined(__HAIKU__)

/* Linux-i386, Linux-Alpha, Linux-PPC */
#  include <stdint.h>

/* XXX */
#  ifndef UINT64_MAX
#    define UINT64_MAX 18446744073709551615
typedef uint8_t u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
typedef uint64_t u_int64_t;
#  endif

#elif defined(__APPLE__)

#  include <inttypes.h>

/* MSVC >= 2010 */
#elif defined(_MSC_VER)
#  include <stdint.h>

#else

/* FreeBSD, Solaris */
#  include <stdint.h>
#  include <sys/types.h>

#endif /* ifdef platform for types */

#include <stdbool.h>
#include <stddef.h> /* size_t define */

#ifndef __cplusplus
#  if defined(__APPLE__) || defined(__NetBSD__)
/* The <uchar.h> standard header is missing on macOS. */
typedef unsigned int char32_t;
#  else
#    include <uchar.h>
#  endif
#endif

typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned long ulong;
typedef unsigned char uchar;

#ifdef __cplusplus
}
#endif
