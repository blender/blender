/*
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
 *
 * @mainpage BLI - Blender LIbrary external interface
 *
 * @section about About the BLI module
 *
 * This is the external interface of the Blender Library. If you find
 * a call to a BLI function that is not prototyped here, please add a
 * prototype here. The library offers mathematical operations (mainly
 * vector and matrix calculus), an abstraction layer for file i/o,
 * functions for calculating Perlin noise, scanfilling services for
 * triangles, and a system for guarded memory
 * allocation/deallocation. There is also a patch to make MS Windows
 * behave more or less Posix-compliant.
 *
 * @section issues Known issues with BLI
 *
 * - blenlib is written in C.
 * - The posix-compliancy may move to a separate lib that deals with 
 *   platform dependencies. (There are other platform-dependent 
 *   fixes as well.)
 * - The file i/o has some redundant code. It should be cleaned.
 * - arithb.c is a very messy matrix library. We need a better 
 *   solution.
 * - vectorops.c is close to superfluous. It may disappear in the 
 *   near future.
 * 
 * @section dependencies Dependencies
 *
 * - The blenlib uses type defines from makesdna/, and functions from
 * standard libraries.
 * 
 * $Id$ 
*/

#ifndef BLI_BLENLIB_H
#define BLI_BLENLIB_H

struct ListBase;

#include <stdlib.h>

extern char btempdir[]; /* creator.c temp dir used instead of U.tempdir, set with BLI_where_is_temp( btempdir, 1 ); */

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_listbase.h"

#include "BLI_dynamiclist.h"

#include "BLI_string.h"

#include "BLI_path_util.h"

#include "BLI_storage.h"

#include "BLI_fileops.h"

#include "BLI_rect.h"

#include "BLI_scanfill.h"

#include "BLI_noise.h"

/**
 * @param strct The structure of interest
 * @param member The name of a member field of @a strct
 * @retval The offset in bytes of @a member within @a strct
 */
#define BLI_STRUCT_OFFSET(strct, member)	((int)(intptr_t) &((strct*) 0)->member)

#ifdef __cplusplus
}
#endif

#endif
