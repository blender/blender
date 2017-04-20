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
#ifndef __BKE_CURVE_RENDER_H__
#define __BKE_CURVE_RENDER_H__

/** \file BKE_curve_render.h
 *  \ingroup bke
 */

struct Batch;
struct Curve;

void BKE_curve_batch_cache_dirty(struct Curve *cu);
void BKE_curve_batch_selection_dirty(struct Curve *cu);
void BKE_curve_batch_cache_clear(struct Curve *cu);
void BKE_curve_batch_cache_free(struct Curve *cu);
struct Batch *BKE_curve_batch_cache_get_wire_edge(struct Curve *cu, struct CurveCache *ob_curve_cache);
struct Batch *BKE_curve_batch_cache_get_normal_edge(
        struct Curve *cu, struct CurveCache *ob_curve_cache, float normal_size);
struct Batch *BKE_curve_batch_cache_get_overlay_edges(struct Curve *cu);
struct Batch *BKE_curve_batch_cache_get_overlay_verts(struct Curve *cu);

struct Batch *BKE_curve_batch_cache_get_triangles_with_normals(struct Curve *cu, struct CurveCache *ob_curve_cache);

#endif /* __BKE_CURVE_RENDER_H__ */
