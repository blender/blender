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
 * The Original Code is Copyright (C) 2018 by Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file BKE_subdiv_ccg.h
 *  \ingroup bke
 *  \since July 2018
 *  \author Sergey Sharybin
 */

#ifndef __BKE_SUBDIV_CCG_H__
#define __BKE_SUBDIV_CCG_H__

#include "BKE_customdata.h"
#include "BLI_sys_types.h"

struct CCGElem;
struct CCGKey;
struct Mesh;
struct Subdiv;

typedef struct SubdivToCCGSettings {
	/* Resolution at which regular ptex (created for quad polygon) are being
	 * evaluated. This defines how many vertices final mesh will have: every
	 * regular ptex has resolution^2 vertices. Special (irregular, or ptex
	 * crated for a corner of non-quad polygon) will have resolution of
	 * `resolution - 1`.
	 */
	int resolution;
	/* Denotes which extra layers to be added to CCG elements. */
	bool need_normal;
	bool need_mask;
} SubdivToCCGSettings;

/* Representation of subdivision surface which uses CCG grids. */
typedef struct SubdivCCG {
	/* A level at which geometry was subdivided. This is what defines grid
	 * resolution. It is NOT the topology refinement level.
	 */
	int level;
	/* Resolution of grid. All grids have matching resolution, and resolution
	 * is same as ptex created for non-quad polygons.
	 */
	int grid_size;
	/* Grids represent limit surface, with displacement applied. Grids are
	 * corresponding to face-corners of coarse mesh, each grid has
	 * grid_size^2 elements.
	 */
	struct CCGElem **grids;
	int num_grids;
	/* Loose edges, each array element contains grid_size elements
	 * corresponding to vertices created by subdividing coarse edges.
	 */
	struct CCGElem **edges;
	int num_edges;
	/* Loose vertices. Every element corresponds to a loose vertex from a coarse
	 * mesh, every coarse loose vertex corresponds to a single sundivided
	 * element.
	 */
	struct CCGElem *vertices;
	int num_vertices;
	/* Denotes which layers present in the elements.
	 *
	 * Grids always has coordinates, followed by extra layers which are set to
	 * truth here.
	 */
	bool has_normal;
	bool has_mask;
	/* Offsets of corresponding data layers in the elements. */
	int normal_offset;
	int mask_offset;

	/* TODO(sergey): Consider adding some accessors to a "decoded" geometry,
	 * to make integration with draw manager and such easy.
	 */

	/* TODO(sergey): Consider adding CD layers here, so we can draw final mesh
	 * from grids, and have UVs and such work.
	 */
} SubdivCCG;

/* Create real hi-res CCG from subdivision. */
struct SubdivCCG *BKE_subdiv_to_ccg(
        struct Subdiv *subdiv,
        const SubdivToCCGSettings *settings,
        const struct Mesh *coarse_mesh);

/* Destroy CCG representation of subdivision surface. */
void BKE_subdiv_ccg_destroy(SubdivCCG *subdiv_ccg);

/* Create a key for accessing grid elements at a given level. */
void BKE_subdiv_ccg_key(
        struct CCGKey *key, const SubdivCCG *subdiv_ccg, int level);
void BKE_subdiv_ccg_key_top_level(
        struct CCGKey *key, const SubdivCCG *subdiv_ccg);

#endif  /* __BKE_SUBDIV_CCG_H__ */
