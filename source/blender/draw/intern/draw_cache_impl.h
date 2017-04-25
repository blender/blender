/*
 * Copyright 2016, Blender Foundation.
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
 * Contributor(s): Blender Institute
 *
 */

/** \file draw_cache_impl.h
 *  \ingroup draw
 */

#ifndef __DRAW_CACHE_IMPL_H__
#define __DRAW_CACHE_IMPL_H__

struct Batch;
struct ListBase;
struct CurveCache;

struct Curve;
struct Lattice;
struct Mesh;

/* Expose via BKE callbacks */
void DRW_curve_batch_cache_dirty(struct Curve *cu, int mode);
void DRW_curve_batch_cache_free(struct Curve *cu);

void DRW_mesh_batch_cache_dirty(struct Mesh *me, int mode);
void DRW_mesh_batch_cache_free(struct Mesh *me);

void DRW_lattice_batch_cache_dirty(struct Lattice *lt, int mode);
void DRW_lattice_batch_cache_free(struct Lattice *lt);

/* Curve */
struct Batch *DRW_curve_batch_cache_get_wire_edge(struct Curve *cu, struct CurveCache *ob_curve_cache);
struct Batch *DRW_curve_batch_cache_get_normal_edge(
        struct Curve *cu, struct CurveCache *ob_curve_cache, float normal_size);
struct Batch *DRW_curve_batch_cache_get_overlay_edges(struct Curve *cu);
struct Batch *DRW_curve_batch_cache_get_overlay_verts(struct Curve *cu);

struct Batch *DRW_curve_batch_cache_get_triangles_with_normals(struct Curve *cu, struct CurveCache *ob_curve_cache);

/* Curve (Font) */
struct Batch *DRW_curve_batch_cache_get_overlay_cursor(struct Curve *cu);
struct Batch *DRW_curve_batch_cache_get_overlay_select(struct Curve *cu);

/* DispList */
struct Batch *BLI_displist_batch_calc_surface(struct ListBase *lb);

/* Lattice */
struct Batch *DRW_lattice_batch_cache_get_all_edges(struct Lattice *lt);
struct Batch *DRW_lattice_batch_cache_get_all_verts(struct Lattice *lt);
struct Batch *DRW_lattice_batch_cache_get_overlay_verts(struct Lattice *lt);

/* Mesh */

struct Batch **DRW_mesh_batch_cache_get_surface_shaded(struct Mesh *me);
struct Batch *DRW_mesh_batch_cache_get_all_edges(struct Mesh *me);
struct Batch *DRW_mesh_batch_cache_get_all_triangles(struct Mesh *me);
struct Batch *DRW_mesh_batch_cache_get_triangles_with_normals(struct Mesh *me);
struct Batch *DRW_mesh_batch_cache_get_points_with_normals(struct Mesh *me);
struct Batch *DRW_mesh_batch_cache_get_all_verts(struct Mesh *me);
struct Batch *DRW_mesh_batch_cache_get_fancy_edges(struct Mesh *me);
struct Batch *DRW_mesh_batch_cache_get_overlay_triangles(struct Mesh *me);
struct Batch *DRW_mesh_batch_cache_get_overlay_loose_edges(struct Mesh *me);
struct Batch *DRW_mesh_batch_cache_get_overlay_loose_verts(struct Mesh *me);
struct Batch *DRW_mesh_batch_cache_get_overlay_facedots(struct Mesh *me);

#endif /* __DRAW_CACHE_IMPL_H__ */
