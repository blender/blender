/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup bli
 *
 * \section aboutbli Blender Library external interface
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
