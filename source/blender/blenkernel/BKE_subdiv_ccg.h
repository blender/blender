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
#include "BLI_bitmap.h"
#include "BLI_sys_types.h"

struct CCGElem;
struct CCGFace;
struct CCGKey;
struct DMFlagMat;
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

/* This is actually a coarse face, which consists of multiple CCG grids. */
typedef struct SubdivCCGFace {
	/* Total number of grids in this face.
	 *
	 * This 1:1 corresponds to a number of corners (or loops) from a coarse
	 * face.
	 */
	int num_grids;
	/* Index of first grid from this face in SubdivCCG->grids array. */
	int start_grid_index;
} SubdivCCGFace;

/* Definition of an edge which is adjacent to at least one of the faces. */
typedef struct SubdivCCGAdjacentEdge {
	int num_adjacent_faces;
	/* Indexed by adjacent face index. */
	SubdivCCGFace **faces;
	/* Indexed by adjacent face index, then by point index on the edge.
	 * points to a grid element.
	 */
	struct CCGElem ***boundary_elements;
} SubdivCCGAdjacentEdge;

/* Definition of a vertex which is adjacent to at least one of the faces. */
typedef struct SubdivCCGAdjacentVertex {
	int num_adjacent_faces;
	/* Indexed by adjacent face index. */
	SubdivCCGFace **faces;
	/* Indexed by adjacent face index, points to a grid element.
	 */
	struct CCGElem **corner_elements;
} SubdivCCGAdjacentVertex;

/* Representation of subdivision surface which uses CCG grids. */
typedef struct SubdivCCG {
	/* This is a subdivision surface this CCG was created for.
	 *
	 * TODO(sergey): Make sure the whole descriptor is valid, including all the
	 * displacement attached to the surface.
	 */
	struct Subdiv *subdiv;
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
	/* Indexed by a grid index, points to a grid data which is stored in
	 * grids_storage.
	 */
	struct CCGElem **grids;
	/* Flat array of all grids' data. */
	unsigned char *grids_storage;
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

	/* Faces from which grids are emitted. */
	int num_faces;
	SubdivCCGFace *faces;
	/* Indexed by grid index, points to corresponding face from `faces`. */
	SubdivCCGFace **grid_faces;

	/* Edges which are adjacent to faces.
	 * Used for faster grid stitching, in the cost of extra memory.
	 */
	int num_adjacent_edges;
	SubdivCCGAdjacentEdge *adjacent_edges;

	/* Vertices which are adjacent to faces
	 * Used for faster grid stitching, in the cost of extra memory.
	 */
	int num_adjacent_vertices;
	SubdivCCGAdjacentVertex *adjacent_vertices;

	struct DMFlagMat *grid_flag_mats;
	BLI_bitmap **grid_hidden;

	/* TODO(sergey): Consider adding some accessors to a "decoded" geometry,
	 * to make integration with draw manager and such easy.
	 */

	/* TODO(sergey): Consider adding CD layers here, so we can draw final mesh
	 * from grids, and have UVs and such work.
	 */

	/* Integration with sculpting. */
	/* TODO(sergey): Is this really best way to go? Kind of annoying to have
	 * such use-related flags in a more or less generic structure.
	 */
	struct {
		/* Corresponds to MULTIRES_COORDS_MODIFIED. */
		bool coords;
		/* Corresponds to MULTIRES_HIDDEN_MODIFIED. */
		bool hidden;
	} dirty;
} SubdivCCG;

/* Create real hi-res CCG from subdivision.
 *
 * NOTE: Subdiv is expected to be refined and ready for evaluation.
 * NOTE: CCG becomes an owner of subdiv.
 *
 * TODO(sergey): Allow some user-counter or more explicit control over who owns
 * the Subdiv. The goal should be to allow viewport GL Mesh and CCG to share
 * same Subsurf without conflicts.
 */
struct SubdivCCG *BKE_subdiv_to_ccg(
        struct Subdiv *subdiv,
        const SubdivToCCGSettings *settings);

/* Destroy CCG representation of subdivision surface. */
void BKE_subdiv_ccg_destroy(SubdivCCG *subdiv_ccg);

/* Helper function, creates Mesh structure which is properly setup to use
 * grids.
 */
struct Mesh *BKE_subdiv_to_ccg_mesh(
        struct Subdiv *subdiv,
        const SubdivToCCGSettings *settings,
        const struct Mesh *coarse_mesh);

/* Create a key for accessing grid elements at a given level. */
void BKE_subdiv_ccg_key(
        struct CCGKey *key, const SubdivCCG *subdiv_ccg, int level);
void BKE_subdiv_ccg_key_top_level(
        struct CCGKey *key, const SubdivCCG *subdiv_ccg);

/* Recalculate all normals based on grid element coordinates. */
void BKE_subdiv_ccg_recalc_normals(SubdivCCG *subdiv_ccg);

/* Average grid coordinates and normals along the grid boundatries. */
void BKE_subdiv_ccg_average_grids(SubdivCCG *subdiv_ccg);

/* Similar to above, but only updates given faces. */
void BKE_subdiv_ccg_average_stitch_faces(SubdivCCG *subdiv_ccg,
                                         struct CCGFace **effected_faces,
                                         int num_effected_faces);

#endif  /* __BKE_SUBDIV_CCG_H__ */
