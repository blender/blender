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
 * \section aboutbli Blender LIbrary external interface
 *
 * \subsection about About the BLI module
 *
 * This is the external interface of the Blender Library. If you find
 * a call to a BLI function that is not prototyped here, please add a
 * prototype here. The library offers mathematical operations (mainly
 * vector and matrix calculus), an abstraction layer for file i/o,
 * functions for calculating Perlin noise, scan-filling services for
 * triangles, and a system for guarded memory
 * allocation/deallocation. There is also a patch to make MS Windows
 * behave more or less Posix-compliant.
 *
 * \subsection issues Known issues with BLI
 *
 * - blenlib is written in C.
 * - The posix-compliance may move to a separate lib that deals with
 *   platform dependencies. (There are other platform-dependent
 *   fixes as well.)
 * - The file i/o has some redundant code. It should be cleaned.
 *
 * \subsection dependencies Dependencies
 *
 * - The blenlib uses type defines from \ref DNA, and functions from
 * standard libraries.
 */

#pragma once

#include <stdlib.h>

#include "BLI_listbase.h"

#include "BLI_string.h"

#include "BLI_string_utf8.h"

#include "BLI_path_util.h"

#include "BLI_fileops.h"

#include "BLI_rect.h"
