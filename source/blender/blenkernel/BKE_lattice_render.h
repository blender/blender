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
#ifndef __BKE_LATTICE_RENDER_H__
#define __BKE_LATTICE_RENDER_H__

/** \file BKE_lattice_render.h
 *  \ingroup bke
 */

struct Batch;
struct Lattice;

void BKE_lattice_batch_cache_dirty(struct Lattice *lt);
void BKE_lattice_batch_selection_dirty(struct Lattice *lt);
void BKE_lattice_batch_cache_clear(struct Lattice *lt);
void BKE_lattice_batch_cache_free(struct Lattice *lt);
struct Batch *BKE_lattice_batch_cache_get_all_edges(struct Lattice *lt);
struct Batch *BKE_lattice_batch_cache_get_all_verts(struct Lattice *lt);
struct Batch *BKE_lattice_batch_cache_get_overlay_verts(struct Lattice *me);

#endif /* __BKE_LATTICE_RENDER_H__ */
