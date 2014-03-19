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
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_INTERSECT_H__
#define __BMESH_INTERSECT_H__

/** \file blender/bmesh/tools/bmesh_intersect.h
 *  \ingroup bmesh
 */

bool BM_mesh_intersect(
        BMesh *bm,
        struct BMLoop *(*looptris)[3], const int looptris_tot,
        int (*test_fn)(BMFace *f, void *user_data), void *user_data,
        const bool use_self, const bool use_separate,
        const float eps);

#endif /* __BMESH_INTERSECT_H__ */
