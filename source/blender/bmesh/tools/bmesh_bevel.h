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

#ifndef __BMESH_BEVEL_H__
#define __BMESH_BEVEL_H__

/** \file blender/bmesh/tools/bmesh_bevel.h
 *  \ingroup bmesh
 */

struct MDeformVert;

void BM_mesh_bevel(BMesh *bm, const float offset, const int offset_type, const float segments,
                   const float profile, const bool vertex_only, const bool use_weights,
                   const bool limit_offset, const struct MDeformVert *dvert, const int vertex_group,
                   const int mat);

#endif /* __BMESH_BEVEL_H__ */
