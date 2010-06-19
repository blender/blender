/**
 * imbuf.h (mar-2001 nzc)
 *
 * This header might have to become external...
 *
 * $Id$ 
 *
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

#ifndef IMBUF_H
#define IMBUF_H

#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h> 

#ifndef WIN32
#include <unistd.h> 
#endif

#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <math.h>

#ifndef WIN32
#include <sys/mman.h>
#endif

#include "MEM_guardedalloc.h"

#if !defined(WIN32)
#define O_BINARY 0
#endif

#define SWAP_SHORT(x) (((x & 0xff) << 8) | ((x >> 8) & 0xff))
#define SWAP_LONG(x) (((x) << 24) | (((x) & 0xff00) << 8) | (((x) >> 8) & 0xff00) | (((x) >> 24) & 0xff))

#define ENDIAN_NOP(x) (x)

#if defined(__sgi) || defined(__sparc) || defined(__sparc__) || defined (__PPC__) || defined (__hppa__) || (defined (__APPLE__) && !defined(__LITTLE_ENDIAN__))
#define LITTLE_SHORT SWAP_SHORT
#define LITTLE_LONG SWAP_LONG
#define BIG_SHORT ENDIAN_NOP
#define BIG_LONG ENDIAN_NOP
#else
#define LITTLE_SHORT ENDIAN_NOP
#define LITTLE_LONG ENDIAN_NOP
#define BIG_SHORT SWAP_SHORT
#define BIG_LONG SWAP_LONG
#endif

typedef unsigned char uchar;

#define TRUE 1
#define FALSE 0

#endif	/* IMBUF_H */

