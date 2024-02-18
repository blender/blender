/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#ifndef WIN32
#  include <unistd.h>
#endif

#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <sys/stat.h>

#ifndef WIN32
#  include <sys/mman.h>
#  define O_BINARY 0
#endif

#define SWAP_SHORT(x) (((x & 0xff) << 8) | ((x >> 8) & 0xff))
#define SWAP_LONG(x) \
  (((x) << 24) | (((x) & 0xff00) << 8) | (((x) >> 8) & 0xff00) | (((x) >> 24) & 0xff))

#define ENDIAN_NOP(x) (x)

#ifdef __BIG_ENDIAN__
#  define LITTLE_SHORT SWAP_SHORT
#  define LITTLE_LONG SWAP_LONG
#  define BIG_SHORT ENDIAN_NOP
#  define BIG_LONG ENDIAN_NOP
#else
#  define LITTLE_SHORT ENDIAN_NOP
#  define LITTLE_LONG ENDIAN_NOP
#  define BIG_SHORT SWAP_SHORT
#  define BIG_LONG SWAP_LONG
#endif

#define IMB_DPI_DEFAULT 72.0
