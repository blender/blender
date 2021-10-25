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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_BISECT_PLANE_H__
#define __BMESH_BISECT_PLANE_H__

/** \file blender/bmesh/tools/bmesh_bisect_plane.h
 *  \ingroup bmesh
 */

void BM_mesh_bisect_plane(
        BMesh *bm, const float plane[4],
        const bool use_snap_center, const bool use_tag,
        const short oflag_center, const short oflag_new, const float eps);

#endif /* __BMESH_BISECT_PLANE_H__ */
