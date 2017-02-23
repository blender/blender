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
 * The Original Code is Copyright (C) 2017 by Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, Mike Erwin, Dalai Felinto
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BKE_MESH_RENDER_H__
#define __BKE_MESH_RENDER_H__

/** \file BKE_mesh_render.h
 *  \ingroup bke
 */

struct Batch;
struct Mesh;

void BKE_mesh_batch_cache_dirty(struct Mesh *me);
void BKE_mesh_batch_cache_clear(struct Mesh *me);
void BKE_mesh_batch_cache_free(struct Mesh *me);
struct Batch *BKE_mesh_batch_cache_get_all_edges(struct Mesh *me);
struct Batch *BKE_mesh_batch_cache_get_all_triangles(struct Mesh *me);
struct Batch *BKE_mesh_batch_cache_get_triangles_with_normals(struct Mesh *me);
struct Batch *BKE_mesh_batch_cache_get_all_verts(struct Mesh *me);
struct Batch *BKE_mesh_batch_cache_get_fancy_edges(struct Mesh *me);
struct Batch *BKE_mesh_batch_cache_get_overlay_edges(struct Mesh *me);

#endif /* __BKE_MESH_RENDER_H__ */
