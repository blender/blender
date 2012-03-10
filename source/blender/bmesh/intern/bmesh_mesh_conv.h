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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Geoffrey Bantle.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_MESH_CONV_H__
#define __BMESH_MESH_CONV_H__

/** \file blender/bmesh/intern/bmesh_mesh_conv.h
 *  \ingroup bmesh
 */

struct Mesh;

void BM_mesh_to_bmesh(BMesh *bm, struct Mesh *me, int set_key, int act_key_nr);
void BM_mesh_from_bmesh(BMesh *bm, struct Mesh *me, int dotess);

#endif /* __BMESH_MESH_CONV_H__ */
