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

#define IMB_DPI_DEFAULT 72.0
