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

#ifndef __BMESH_REGION_MATCH_H__
#define __BMESH_REGION_MATCH_H__

/** \file blender/bmesh/tools/bmesh_region_match.h
 *  \ingroup bmesh
 */

int BM_mesh_region_match(
        BMesh *bm,
        BMFace **faces_region, unsigned int faces_region_len,
        ListBase *r_face_regions);

#endif /* __BMESH_REGION_MATCH_H__ */
