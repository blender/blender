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
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_TOOLS_H__
#define __BMESH_TOOLS_H__

/** \file blender/bmesh/bmesh_tools.h
 *  \ingroup bmesh
 *
 * Utility functions that operate directly on the BMesh,
 * These can be used by both Modifiers and BMesh-Operators.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "tools/bmesh_beautify.h"
#include "tools/bmesh_bevel.h"
#include "tools/bmesh_bisect_plane.h"
#include "tools/bmesh_decimate.h"
#include "tools/bmesh_edgenet.h"
#include "tools/bmesh_edgesplit.h"
#include "tools/bmesh_path.h"
#include "tools/bmesh_path_region.h"
#include "tools/bmesh_region_match.h"
#include "tools/bmesh_separate.h"
#include "tools/bmesh_triangulate.h"

#ifdef __cplusplus
}
#endif

#endif /* __BMESH_TOOLS_H__ */
