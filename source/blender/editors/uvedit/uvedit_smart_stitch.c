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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Antony Riakiotakis.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/uvedit/uvedit_smart_stitch.c
 *  \ingroup eduv
 */


#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"

#include "BIF_gl.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_mesh_mapping.h"
#include "BKE_editmesh.h"

#include "ED_mesh.h"
#include "ED_uvedit.h"
#include "ED_screen.h"
#include "ED_space_api.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"
#include "UI_resources.h"

#include "uvedit_intern.h"

/* ********************** smart stitch operator *********************** */

/* object that stores display data for previewing before confirming stitching */
typedef struct StitchPreviewer {
	/* here we'll store the preview triangle indices of the mesh */
	float *preview_polys;
	/* uvs per polygon. */
	unsigned int *uvs_per_polygon;
	/*number of preview polygons */
	unsigned int num_polys;
	/* preview data. These will be either the previewed vertices or edges depending on stitch mode settings */
	float *preview_stitchable;
	float *preview_unstitchable;
	/* here we'll store the number of elements to be drawn */
	unsigned int num_stitchable;
	unsigned int num_unstitchable;
	unsigned int preview_uvs;
	/* ...and here we'll store the static island triangles*/
	float *static_tris;
	unsigned int num_static_tris;
} StitchPreviewer;


struct IslandStitchData;

/* This is a straightforward implementation, count the uv's in the island that will move and take the mean displacement/rotation and apply it to all
 * elements of the island except from the stitchable */
typedef struct IslandStitchData {
	/* rotation can be used only for edges, for vertices there is no such notion */
	float rotation;
	float rotation_neg;
	float translation[2];
	/* Used for rotation, the island will rotate around this point */
	float medianPoint[2];
	int numOfElements;
	int num_rot_elements;
	int num_rot_elements_neg;
	/* flag to remember if island has been added for preview */
	char addedForPreview;
	/* flag an island to be considered for determining static island */
	char stitchableCandidate;
	/* if edge rotation is used, flag so that vertex rotation is not used */
	bool use_edge_rotation;
} IslandStitchData;

/* just for averaging UVs */
typedef struct UVVertAverage {
	float uv[2];
	unsigned short count;
} UVVertAverage;

typedef struct UvEdge {
	/* index to uv buffer */
	unsigned int uv1;
	unsigned int uv2;
	/* general use flag (Used to check if edge is boundary here, and propagates to adjacency elements) */
	unsigned char flag;
	/* element that guarantees element->face has the edge on element->tfindex and element->tfindex+1 is the second uv */
	UvElement *element;
	/* next uv edge with the same exact vertices as this one.. Calculated at startup to save time */
	struct UvEdge *next;
	/* point to first of common edges. Needed for iteration */
	struct UvEdge *first;
} UvEdge;


/* stitch state object */
typedef struct StitchState {
	float aspect;
	/* use limit flag */
	bool use_limit;
	/* limit to operator, same as original operator */
	float limit_dist;
	/* snap uv islands together during stitching */
	bool snap_islands;
	/* stich at midpoints or at islands */
	bool midpoints;
	/* editmesh, cached for use in modal handler */
	BMEditMesh *em;
	/* clear seams of stitched edges after stitch */
	bool clear_seams;
	/* element map for getting info about uv connectivity */
	UvElementMap *element_map;
	/* edge container */
	UvEdge *uvedges;
	/* container of first of a group of coincident uvs, these will be operated upon */
	UvElement **uvs;
	/* maps uvelements to their first coincident uv */
	int *map;
	/* 2D normals per uv to calculate rotation for snapping */
	float *normals;
	/* edge storage */
	UvEdge *edges;
	/* hash for quick lookup of edges */
	GHash *edge_hash;

	/* count of separate uvs and edges */
	int total_separate_edges;
	int total_separate_uvs;
	/* hold selection related information */
	void **selection_stack;
	int selection_size;
	/* island that stays in place */
	int static_island;
	/* store number of primitives per face so that we can allocate the active island buffer later */
	unsigned int *tris_per_island;

	/* vert or edge mode used for stitching */
	char mode;
	/* handle for drawing */
	void *draw_handle;
	/* preview data */
	StitchPreviewer *stitch_preview;
} StitchState;

typedef struct PreviewPosition {
	int data_position;
	int polycount_position;
} PreviewPosition;
/*
 * defines for UvElement/UcEdge flags
 */
#define STITCH_SELECTED 1
#define STITCH_STITCHABLE 2
#define STITCH_PROCESSED 4
#define STITCH_BOUNDARY 8
#define STITCH_STITCHABLE_CANDIDATE 16

#define STITCH_NO_PREVIEW -1

enum StitchModes {
	STITCH_VERT,
	STITCH_EDGE
};

/* constructor */
static StitchPreviewer *stitch_preview_init(void)
{
	StitchPreviewer *stitch_preview;

	stitch_preview = MEM_mallocN(sizeof(StitchPreviewer), "stitch_previewer");
	stitch_preview->preview_polys = NULL;
	stitch_preview->preview_stitchable = NULL;
	stitch_preview->preview_unstitchable = NULL;
	stitch_preview->uvs_per_polygon = NULL;

	stitch_preview->preview_uvs = 0;
	stitch_preview->num_polys = 0;
	stitch_preview->num_stitchable = 0;
	stitch_preview->num_unstitchable = 0;

	stitch_preview->static_tris = NULL;

	stitch_preview->num_static_tris = 0;

	return stitch_preview;
}

/* destructor...yeah this should be C++ :) */
static void stitch_preview_delete(StitchPreviewer *stitch_preview)
{
	if (stitch_preview) {
		if (stitch_preview->preview_polys) {
			MEM_freeN(stitch_preview->preview_polys);
			stitch_preview->preview_polys = NULL;
		}
		if (stitch_preview->uvs_per_polygon) {
			MEM_freeN(stitch_preview->uvs_per_polygon);
			stitch_preview->uvs_per_polygon = NULL;
		}
		if (stitch_preview->preview_stitchable) {
			MEM_freeN(stitch_preview->preview_stitchable);
			stitch_preview->preview_stitchable = NULL;
		}
		if (stitch_preview->preview_unstitchable) {
			MEM_freeN(stitch_preview->preview_unstitchable);
			stitch_preview->preview_unstitchable = NULL;
		}
		if (stitch_preview->static_tris) {
			MEM_freeN(stitch_preview->static_tris);
			stitch_preview->static_tris = NULL;
		}
		MEM_freeN(stitch_preview);
	}
}

#define HEADER_LENGTH 256

/* This function updates the header of the UV editor when the stitch tool updates its settings */
static void stitch_update_header(StitchState *state, bContext *C)
{
	static char str[] = "Mode(TAB) %s, (S)nap %s, (M)idpoints %s, (L)imit %.2f (Alt Wheel adjust) %s, Switch (I)sland, shift select vertices";

	char msg[HEADER_LENGTH];
	ScrArea *sa = CTX_wm_area(C);

	if (sa) {
		BLI_snprintf(msg, HEADER_LENGTH, str,
		             state->mode == STITCH_VERT ? "Vertex" : "Edge",
		             WM_bool_as_string(state->snap_islands),
		             WM_bool_as_string(state->midpoints),
		             state->limit_dist,
		             WM_bool_as_string(state->use_limit));

		ED_area_headerprint(sa, msg);
	}
}

static int getNumOfIslandUvs(UvElementMap *elementMap, int island)
{
	if (island == elementMap->totalIslands - 1) {
		return elementMap->totalUVs - elementMap->islandIndices[island];
	}
	else {
		return elementMap->islandIndices[island + 1] - elementMap->islandIndices[island];
	}
}

static void stitch_uv_rotate(float mat[2][2], float medianPoint[2], float uv[2], float aspect)
{
	float uv_rotation_result[2];

	uv[1] /= aspect;

	sub_v2_v2(uv, medianPoint);
	mul_v2_m2v2(uv_rotation_result, mat, uv);
	add_v2_v2v2(uv, uv_rotation_result, medianPoint);

	uv[1] *= aspect;
}

/* check if two uvelements are stitchable. This should only operate on -different- separate UvElements */
static bool stitch_check_uvs_stitchable(UvElement *element, UvElement *element_iter, StitchState *state)
{
	BMesh *bm = state->em->bm;
	float limit;

	if (element_iter == element) {
		return 0;
	}

	limit = state->limit_dist;

	if (state->use_limit) {
		MLoopUV *luv, *luv_iter;
		BMLoop *l;


		l = element->l;
		luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);
		l = element_iter->l;
		luv_iter = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);

		if (fabsf(luv->uv[0] - luv_iter->uv[0]) < limit &&
		    fabsf(luv->uv[1] - luv_iter->uv[1]) < limit)
		{
			return 1;
		}
		else {
			return 0;
		}
	}
	else {
		return 1;
	}
}

static bool stitch_check_edges_stitchable(UvEdge *edge, UvEdge *edge_iter, StitchState *state)
{
	BMesh *bm = state->em->bm;
	float limit;

	if (edge_iter == edge) {
		return 0;
	}

	limit = state->limit_dist;

	if (state->use_limit) {
		BMLoop *l;
		MLoopUV *luv_orig1, *luv_iter1;
		MLoopUV *luv_orig2, *luv_iter2;

		l = state->uvs[edge->uv1]->l;
		luv_orig1 = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);
		l = state->uvs[edge_iter->uv1]->l;
		luv_iter1 = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);

		l = state->uvs[edge->uv2]->l;
		luv_orig2 = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);
		l = state->uvs[edge_iter->uv2]->l;
		luv_iter2 = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);

		if (fabsf(luv_orig1->uv[0] - luv_iter1->uv[0]) < limit &&
		    fabsf(luv_orig1->uv[1] - luv_iter1->uv[1]) < limit &&
		    fabsf(luv_orig2->uv[0] - luv_iter2->uv[0]) < limit &&
		    fabsf(luv_orig2->uv[1] - luv_iter2->uv[1]) < limit)
		{
			return 1;
		}
		else {
			return 0;
		}
	}
	else {
		return 1;
	}
}

static bool stitch_check_uvs_state_stitchable(UvElement *element, UvElement *element_iter, StitchState *state)
{
	if ((state->snap_islands && element->island == element_iter->island) ||
	    (!state->midpoints && element->island == element_iter->island))
	{
		return 0;
	}

	return stitch_check_uvs_stitchable(element, element_iter, state);
}


static bool stitch_check_edges_state_stitchable(UvEdge *edge, UvEdge *edge_iter, StitchState *state)
{
	if ((state->snap_islands && edge->element->island == edge_iter->element->island) ||
	    (!state->midpoints && edge->element->island == edge_iter->element->island))
	{
		return 0;
	}

	return stitch_check_edges_stitchable(edge, edge_iter, state);
}

/* calculate snapping for islands */
static void stitch_calculate_island_snapping(StitchState *state, PreviewPosition *preview_position, StitchPreviewer *preview, IslandStitchData *island_stitch_data, int final)
{
	BMesh *bm = state->em->bm;
	int i;
	UvElement *element;

	for (i = 0; i < state->element_map->totalIslands; i++) {
		if (island_stitch_data[i].addedForPreview) {
			int numOfIslandUVs = 0, j;
			int totelem = island_stitch_data[i].num_rot_elements_neg + island_stitch_data[i].num_rot_elements;
			float rotation;
			float rotation_mat[2][2];

			/* check to avoid divide by 0 */
			if (island_stitch_data[i].num_rot_elements > 1)
				island_stitch_data[i].rotation /= island_stitch_data[i].num_rot_elements;

			if (island_stitch_data[i].num_rot_elements_neg > 1)
				island_stitch_data[i].rotation_neg /= island_stitch_data[i].num_rot_elements_neg;

			if (island_stitch_data[i].numOfElements > 1) {
				island_stitch_data[i].medianPoint[0] /= island_stitch_data[i].numOfElements;
				island_stitch_data[i].medianPoint[1] /= island_stitch_data[i].numOfElements;

				island_stitch_data[i].translation[0] /= island_stitch_data[i].numOfElements;
				island_stitch_data[i].translation[1] /= island_stitch_data[i].numOfElements;
			}

			island_stitch_data[i].medianPoint[1] /= state->aspect;
			if ((island_stitch_data[i].rotation + island_stitch_data[i].rotation_neg < (float)M_PI_2) ||
			    island_stitch_data[i].num_rot_elements == 0 || island_stitch_data[i].num_rot_elements_neg == 0)
			{
				rotation = (island_stitch_data[i].rotation * island_stitch_data[i].num_rot_elements -
				            island_stitch_data[i].rotation_neg *
				            island_stitch_data[i].num_rot_elements_neg) / totelem;
			}
			else {
				rotation = (island_stitch_data[i].rotation * island_stitch_data[i].num_rot_elements +
				            (2.0f * (float)M_PI - island_stitch_data[i].rotation_neg) *
				            island_stitch_data[i].num_rot_elements_neg) / totelem;
			}

			rotate_m2(rotation_mat, rotation);
			numOfIslandUVs = getNumOfIslandUvs(state->element_map, i);
			element = &state->element_map->buf[state->element_map->islandIndices[i]];
			for (j = 0; j < numOfIslandUVs; j++, element++) {
				/* stitchable uvs have already been processed, don't process */
				if (!(element->flag & STITCH_PROCESSED)) {
					MLoopUV *luv;
					BMLoop *l;

					l = element->l;
					luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);

					if (final) {

						stitch_uv_rotate(rotation_mat, island_stitch_data[i].medianPoint, luv->uv, state->aspect);

						add_v2_v2(luv->uv, island_stitch_data[i].translation);
					}

					else {

						int face_preview_pos = preview_position[BM_elem_index_get(element->l->f)].data_position;

						stitch_uv_rotate(rotation_mat, island_stitch_data[i].medianPoint,
						                 preview->preview_polys + face_preview_pos + 2 * element->tfindex, state->aspect);

						add_v2_v2(preview->preview_polys + face_preview_pos + 2 * element->tfindex,
						          island_stitch_data[i].translation);
					}
				}
				/* cleanup */
				element->flag &= STITCH_SELECTED;
			}
		}
	}
}



static void stitch_island_calculate_edge_rotation(UvEdge *edge, StitchState *state, UVVertAverage *uv_average, unsigned int *uvfinal_map, IslandStitchData *island_stitch_data)
{
	BMesh *bm = state->em->bm;
	UvElement *element1, *element2;
	float uv1[2], uv2[2];
	float edgecos, edgesin;
	int index1, index2;
	float rotation;
	MLoopUV *luv1, *luv2;

	element1 = state->uvs[edge->uv1];
	element2 = state->uvs[edge->uv2];

	luv1 = CustomData_bmesh_get(&bm->ldata, element1->l->head.data, CD_MLOOPUV);
	luv2 = CustomData_bmesh_get(&bm->ldata, element2->l->head.data, CD_MLOOPUV);

	if (state->mode == STITCH_VERT) {
		index1 = uvfinal_map[element1 - state->element_map->buf];
		index2 = uvfinal_map[element2 - state->element_map->buf];
	}
	else {
		index1 = edge->uv1;
		index2 = edge->uv2;
	}
	/* the idea here is to take the directions of the edges and find the rotation between final and initial
	 * direction. This, using inner and outer vector products, gives the angle. Directions are differences so... */
	uv1[0] = luv2->uv[0] - luv1->uv[0];
	uv1[1] = luv2->uv[1] - luv1->uv[1];

	uv1[1] /= state->aspect;

	uv2[0] = uv_average[index2].uv[0] - uv_average[index1].uv[0];
	uv2[1] = uv_average[index2].uv[1] - uv_average[index1].uv[1];

	uv2[1] /= state->aspect;

	normalize_v2(uv1);
	normalize_v2(uv2);

	edgecos = dot_v2v2(uv1, uv2);
	edgesin = cross_v2v2(uv1, uv2);
	rotation = acosf(max_ff(-1.0f, min_ff(1.0f, edgecos)));

	if (edgesin > 0.0f) {
		island_stitch_data[element1->island].num_rot_elements++;
		island_stitch_data[element1->island].rotation += rotation;
	}
	else {
		island_stitch_data[element1->island].num_rot_elements_neg++;
		island_stitch_data[element1->island].rotation_neg += rotation;
	}
}


static void stitch_island_calculate_vert_rotation(UvElement *element, StitchState *state, IslandStitchData *island_stitch_data)
{
	float edgecos = 1.0f, edgesin = 0.0f;
	int index;
	UvElement *element_iter;
	float rotation = 0, rotation_neg = 0;
	int rot_elem = 0, rot_elem_neg = 0;
	BMLoop *l;

	if (element->island == state->static_island && !state->midpoints)
		return;

	l = element->l;

	index = BM_elem_index_get(l->v);

	element_iter = state->element_map->vert[index];

	for (; element_iter; element_iter = element_iter->next) {
		if (element_iter->separate && stitch_check_uvs_state_stitchable(element, element_iter, state)) {
			int index_tmp1, index_tmp2;
			float normal[2];

			/* only calculate rotation against static island uv verts */
			if (!state->midpoints && element_iter->island != state->static_island)
				continue;

			index_tmp1 = element_iter - state->element_map->buf;
			index_tmp1 = state->map[index_tmp1];
			index_tmp2 = element - state->element_map->buf;
			index_tmp2 = state->map[index_tmp2];

			negate_v2_v2(normal, state->normals + index_tmp2 * 2);
			edgecos = dot_v2v2(normal, state->normals + index_tmp1 * 2);
			edgesin = cross_v2v2(normal, state->normals + index_tmp1 * 2);
			if (edgesin > 0.0f) {
			    rotation += acosf(max_ff(-1.0f, min_ff(1.0f, edgecos)));
				rot_elem++;
			}
			else {
			    rotation_neg += acosf(max_ff(-1.0f, min_ff(1.0f, edgecos)));
				rot_elem_neg++;
			}
		}
	}

	if (state->midpoints) {
		rotation /= 2.0f;
		rotation_neg /= 2.0f;
	}
	island_stitch_data[element->island].num_rot_elements += rot_elem;
	island_stitch_data[element->island].rotation += rotation;
	island_stitch_data[element->island].num_rot_elements_neg += rot_elem_neg;
	island_stitch_data[element->island].rotation_neg += rotation_neg;
}


static void state_delete(StitchState *state)
{
	if (state) {
		if (state->element_map) {
			BM_uv_element_map_free(state->element_map);
		}
		if (state->uvs) {
			MEM_freeN(state->uvs);
		}
		if (state->selection_stack) {
			MEM_freeN(state->selection_stack);
		}
		if (state->tris_per_island) {
			MEM_freeN(state->tris_per_island);
		}
		if (state->map) {
			MEM_freeN(state->map);
		}
		if (state->normals) {
			MEM_freeN(state->normals);
		}
		if (state->edges) {
			MEM_freeN(state->edges);
		}
		if (state->stitch_preview) {
			stitch_preview_delete(state->stitch_preview);
		}
		if (state->edge_hash) {
			BLI_ghash_free(state->edge_hash, NULL, NULL);
		}
		MEM_freeN(state);
	}
}

static void stitch_uv_edge_generate_linked_edges(GHash *edge_hash, StitchState *state)
{
	UvEdge *edges = state->edges;
	const int *map = state->map;
	UvElementMap *element_map = state->element_map;
	UvElement *first_element = element_map->buf;
	int i;

	for (i = 0; i < state->total_separate_edges; i++) {
		UvEdge *edge = edges + i;

		if (edge->first)
			continue;

		/* only boundary edges can be stitched. Yes. Sorry about that :p */
		if (edge->flag & STITCH_BOUNDARY) {
			UvElement *element1 = state->uvs[edge->uv1];
			UvElement *element2 = state->uvs[edge->uv2];

			/* Now iterate through all faces and try to find edges sharing the same vertices */
			UvElement *iter1 = element_map->vert[BM_elem_index_get(element1->l->v)];
			UvEdge *last_set = edge;
			int elemindex2 = BM_elem_index_get(element2->l->v);

			edge->first = edge;

			for (; iter1; iter1 = iter1->next) {
				UvElement *iter2 = NULL;

				/* check to see if other vertex of edge belongs to same vertex as */
				if (BM_elem_index_get(iter1->l->next->v) == elemindex2)
					iter2 = BM_uv_element_get(element_map, iter1->l->f, iter1->l->next);
				else if (BM_elem_index_get(iter1->l->prev->v) == elemindex2)
					iter2 = BM_uv_element_get(element_map, iter1->l->f, iter1->l->prev);

				if (iter2) {
					int index1 = map[iter1 - first_element];
					int index2 = map[iter2 - first_element];
					UvEdge edgetmp;
					UvEdge *edge2, *eiter;
					bool valid = true;

					/* make sure the indices are well behaved */
					if (index1 > index2) {
						SWAP(int, index1, index2);
					}

					edgetmp.uv1 = index1;
					edgetmp.uv2 = index2;

					/* get the edge from the hash */
					edge2 = BLI_ghash_lookup(edge_hash, &edgetmp);

					/* more iteration to make sure non-manifold case is handled nicely */
					for (eiter = edge; eiter; eiter = eiter->next) {
						if (edge2 == eiter) {
							valid = false;
							break;
						}
					}

					if (valid) {
						/* here I am taking care of non manifold case, assuming more than two matching edges.
						 * I am not too sure we want this though */
						last_set->next = edge2;
						last_set = edge2;
						/* set first, similarly to uv elements. Now we can iterate among common edges easily */
						edge2->first = edge;
					}
				}
			}
		}
		else {
			/* so stitchability code works */
			edge->first = edge;
		}
	}
}


/* checks for remote uvs that may be stitched with a certain uv, flags them if stitchable. */
static void determine_uv_stitchability(UvElement *element, StitchState *state, IslandStitchData *island_stitch_data)
{
	int vert_index;
	UvElement *element_iter;
	BMLoop *l;

	l = element->l;

	vert_index = BM_elem_index_get(l->v);
	element_iter = state->element_map->vert[vert_index];

	for (; element_iter; element_iter = element_iter->next) {
		if (element_iter->separate) {
			if (stitch_check_uvs_stitchable(element, element_iter, state)) {
				island_stitch_data[element_iter->island].stitchableCandidate = 1;
				island_stitch_data[element->island].stitchableCandidate = 1;
				element->flag |= STITCH_STITCHABLE_CANDIDATE;
			}
		}
	}
}

static void determine_uv_edge_stitchability(UvEdge *edge, StitchState *state, IslandStitchData *island_stitch_data)
{
	UvEdge *edge_iter = edge->first;

	for (; edge_iter; edge_iter = edge_iter->next) {
		if (stitch_check_edges_stitchable(edge, edge_iter, state)) {
			island_stitch_data[edge_iter->element->island].stitchableCandidate = 1;
			island_stitch_data[edge->element->island].stitchableCandidate = 1;
			edge->flag |= STITCH_STITCHABLE_CANDIDATE;
		}
	}
}


/* set preview buffer position of UV face in editface->tmp.l */
static void stitch_set_face_preview_buffer_position(BMFace *efa, StitchPreviewer *preview, PreviewPosition *preview_position)
{
	int index = BM_elem_index_get(efa);

	if (preview_position[index].data_position == STITCH_NO_PREVIEW) {
		preview_position[index].data_position = preview->preview_uvs * 2;
		preview_position[index].polycount_position = preview->num_polys++;
		preview->preview_uvs += efa->len;
	}
}


/* setup face preview for all coincident uvs and their faces */
static void stitch_setup_face_preview_for_uv_group(UvElement *element, StitchState *state, IslandStitchData *island_stitch_data,
                                                   PreviewPosition *preview_position)
{
	StitchPreviewer *preview = state->stitch_preview;

	/* static island does not change so returning immediately */
	if (state->snap_islands && !state->midpoints && state->static_island == element->island)
		return;

	if (state->snap_islands) {
		island_stitch_data[element->island].addedForPreview = 1;
	}

	do {
		stitch_set_face_preview_buffer_position(element->l->f, preview, preview_position);
		element = element->next;
	} while (element && !element->separate);
}


/* checks if uvs are indeed stitchable and registers so that they can be shown in preview */
static void stitch_validate_uv_stichability(UvElement *element, StitchState *state, IslandStitchData *island_stitch_data,
                                            PreviewPosition *preview_position)
{
	UvElement *element_iter;
	StitchPreviewer *preview = state->stitch_preview;
	int vert_index;
	BMLoop *l;

	l = element->l;

	vert_index = BM_elem_index_get(l->v);

	element_iter = state->element_map->vert[vert_index];

	for (; element_iter; element_iter = element_iter->next) {
		if (element_iter->separate) {
			if (element_iter == element)
				continue;
			if (stitch_check_uvs_state_stitchable(element, element_iter, state)) {
				if ((element_iter->island == state->static_island) || (element->island == state->static_island)) {
					element->flag |= STITCH_STITCHABLE;
					preview->num_stitchable++;
					stitch_setup_face_preview_for_uv_group(element, state, island_stitch_data, preview_position);
					return;
				}
			}
		}
	}

	/* this can happen if the uvs to be stitched are not on a stitchable island */
	if (!(element->flag & STITCH_STITCHABLE)) {
		preview->num_unstitchable++;
	}
}


static void stitch_validate_edge_stichability(UvEdge *edge, StitchState *state, IslandStitchData *island_stitch_data,
                                              PreviewPosition *preview_position)
{
	UvEdge *edge_iter = edge->first;
	StitchPreviewer *preview = state->stitch_preview;

	for (; edge_iter; edge_iter = edge_iter->next) {
		if (edge_iter == edge)
			continue;
		if (stitch_check_edges_state_stitchable(edge, edge_iter, state)) {
			if ((edge_iter->element->island == state->static_island) || (edge->element->island == state->static_island)) {
				edge->flag |= STITCH_STITCHABLE;
				preview->num_stitchable++;
				stitch_setup_face_preview_for_uv_group(state->uvs[edge->uv1], state, island_stitch_data, preview_position);
				stitch_setup_face_preview_for_uv_group(state->uvs[edge->uv2], state, island_stitch_data, preview_position);
				return;
			}
		}
	}

	/* this can happen if the uvs to be stitched are not on a stitchable island */
	if (!(edge->flag & STITCH_STITCHABLE)) {
		preview->num_unstitchable++;
	}
}


static void stitch_propagate_uv_final_position(Scene *scene,
                                               UvElement *element, int index, PreviewPosition *preview_position,
                                               UVVertAverage *final_position, StitchState *state,
                                               const bool final)
{
	BMesh *bm = state->em->bm;
	StitchPreviewer *preview = state->stitch_preview;

	const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);

	if (element->flag & STITCH_STITCHABLE) {
		UvElement *element_iter = element;
		/* propagate to coincident uvs */
		do {
			BMLoop *l;
			MLoopUV *luv;

			l = element_iter->l;
			luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);

			element_iter->flag |= STITCH_PROCESSED;
			/* either flush to preview or to the MTFace, if final */
			if (final) {
				copy_v2_v2(luv->uv, final_position[index].uv);

				uvedit_uv_select_enable(state->em, scene, l, false, cd_loop_uv_offset);
			}
			else {
				int face_preview_pos = preview_position[BM_elem_index_get(element_iter->l->f)].data_position;
				if (face_preview_pos != STITCH_NO_PREVIEW) {
					copy_v2_v2(preview->preview_polys + face_preview_pos + 2 * element_iter->tfindex,
					           final_position[index].uv);
				}
			}

			/* end of calculations, keep only the selection flag */
			if ((!state->snap_islands) || ((!state->midpoints) && (element_iter->island == state->static_island))) {
				element_iter->flag &= STITCH_SELECTED;
			}

			element_iter = element_iter->next;
		} while (element_iter && !element_iter->separate);
	}
}

/* main processing function. It calculates preview and final positions. */
static int stitch_process_data(StitchState *state, Scene *scene, int final)
{
	int i;
	StitchPreviewer *preview;
	IslandStitchData *island_stitch_data = NULL;
	int previous_island = state->static_island;
	BMesh *bm = state->em->bm;
	BMFace *efa;
	BMIter iter;
	UVVertAverage *final_position = NULL;

	char stitch_midpoints = state->midpoints;
	/* used to map uv indices to uvaverage indices for selection */
	unsigned int *uvfinal_map = NULL;
	/* per face preview position in preview buffer */
	PreviewPosition *preview_position = NULL;

	/* cleanup previous preview */
	stitch_preview_delete(state->stitch_preview);
	preview = state->stitch_preview = stitch_preview_init();
	if (preview == NULL)
		return 0;

	preview_position = MEM_mallocN(bm->totface * sizeof(*preview_position), "stitch_face_preview_position");
	/* each face holds its position in the preview buffer in tmp. -1 is uninitialized */
	for (i = 0; i < bm->totface; i++) {
		preview_position[i].data_position = STITCH_NO_PREVIEW;
	}

	island_stitch_data = MEM_callocN(sizeof(*island_stitch_data) * state->element_map->totalIslands, "stitch_island_data");
	if (!island_stitch_data) {
		return 0;
	}

	/* store indices to editVerts and Faces. May be unneeded but ensuring anyway */
	BM_mesh_elem_index_ensure(bm, BM_VERT | BM_FACE);

	/*****************************************
	 *  First determine stitchability of uvs *
	 *****************************************/

	for (i = 0; i < state->selection_size; i++) {
		if (state->mode == STITCH_VERT) {
			UvElement *element = (UvElement *)state->selection_stack[i];
			determine_uv_stitchability(element, state, island_stitch_data);
		}
		else {
			UvEdge *edge = (UvEdge *)state->selection_stack[i];
			determine_uv_edge_stitchability(edge, state, island_stitch_data);
		}
	}

	/* set static island to one that is added for preview */
	state->static_island %= state->element_map->totalIslands;
	while (!(island_stitch_data[state->static_island].stitchableCandidate)) {
		state->static_island++;
		state->static_island %= state->element_map->totalIslands;
		/* this is entirely possible if for example limit stitching with no stitchable verts or no selection */
		if (state->static_island == previous_island)
			break;
	}

	for (i = 0; i < state->selection_size; i++) {
		if (state->mode == STITCH_VERT) {
			UvElement *element = (UvElement *)state->selection_stack[i];
			if (element->flag & STITCH_STITCHABLE_CANDIDATE) {
				element->flag &= ~STITCH_STITCHABLE_CANDIDATE;
				stitch_validate_uv_stichability(element, state, island_stitch_data, preview_position);
			}
			else {
				/* add to preview for unstitchable */
				preview->num_unstitchable++;
			}
		}
		else {
			UvEdge *edge = (UvEdge *)state->selection_stack[i];
			if (edge->flag & STITCH_STITCHABLE_CANDIDATE) {
				edge->flag &= ~STITCH_STITCHABLE_CANDIDATE;
				stitch_validate_edge_stichability(edge, state, island_stitch_data, preview_position);
			}
			else {
				preview->num_unstitchable++;
			}
		}
	}

	/*****************************************
	 *  Setup preview for stitchable islands *
	 *****************************************/
	if (state->snap_islands) {
		for (i = 0; i < state->element_map->totalIslands; i++) {
			if (island_stitch_data[i].addedForPreview) {
				int numOfIslandUVs = 0, j;
				UvElement *element;
				numOfIslandUVs = getNumOfIslandUvs(state->element_map, i);
				element = &state->element_map->buf[state->element_map->islandIndices[i]];
				for (j = 0; j < numOfIslandUVs; j++, element++) {
					stitch_set_face_preview_buffer_position(element->l->f, preview, preview_position);
				}
			}
		}
	}

	/*********************************************************************
	 * Setup the preview buffers and fill them with the appropriate data *
	 *********************************************************************/
	if (!final) {
		BMIter liter;
		BMLoop *l;
		MLoopUV *luv;
		unsigned int buffer_index = 0;
		int stitchBufferIndex = 0, unstitchBufferIndex = 0;
		int preview_size = (state->mode == STITCH_VERT) ? 2 : 4;
		/* initialize the preview buffers */
		preview->preview_polys = (float *)MEM_mallocN(preview->preview_uvs * sizeof(float) * 2, "tri_uv_stitch_prev");
		preview->uvs_per_polygon = MEM_mallocN(preview->num_polys * sizeof(*preview->uvs_per_polygon), "tri_uv_stitch_prev");
		preview->preview_stitchable = (float *)MEM_mallocN(preview->num_stitchable * sizeof(float) * preview_size, "stitch_preview_stichable_data");
		preview->preview_unstitchable = (float *)MEM_mallocN(preview->num_unstitchable * sizeof(float) * preview_size, "stitch_preview_unstichable_data");

		preview->static_tris = (float *)MEM_mallocN(state->tris_per_island[state->static_island] * sizeof(float) * 6, "static_island_preview_tris");

		preview->num_static_tris = state->tris_per_island[state->static_island];
		/* will cause cancel and freeing of all data structures so OK */
		if (!preview->preview_polys || !preview->preview_stitchable || !preview->preview_unstitchable) {
			return 0;
		}

		/* copy data from MLoopUVs to the preview display buffers */
		BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
			/* just to test if face was added for processing. uvs of inselected vertices will return NULL */
			UvElement *element = BM_uv_element_get(state->element_map, efa, BM_FACE_FIRST_LOOP(efa));

			if (element) {
				int numoftris = efa->len - 2;
				int index = BM_elem_index_get(efa);
				int face_preview_pos = preview_position[index].data_position;
				if (face_preview_pos != STITCH_NO_PREVIEW) {
					preview->uvs_per_polygon[preview_position[index].polycount_position] = efa->len;
					BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
						luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);
						copy_v2_v2(preview->preview_polys + face_preview_pos + i * 2, luv->uv);
					}
				}

				if (element->island == state->static_island) {
					BMLoop *fl = BM_FACE_FIRST_LOOP(efa);
					MLoopUV *fuv = CustomData_bmesh_get(&bm->ldata, fl->head.data, CD_MLOOPUV);

					BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
						if (i < numoftris) {
							/* using next since the first uv is already accounted for */
							BMLoop *lnext = l->next;
							MLoopUV *luvnext = CustomData_bmesh_get(&bm->ldata, lnext->next->head.data, CD_MLOOPUV);
							luv = CustomData_bmesh_get(&bm->ldata, lnext->head.data, CD_MLOOPUV);

							memcpy(preview->static_tris + buffer_index, fuv->uv, 2 * sizeof(float));
							memcpy(preview->static_tris + buffer_index + 2, luv->uv, 2 * sizeof(float));
							memcpy(preview->static_tris + buffer_index + 4, luvnext->uv, 2 * sizeof(float));
							buffer_index += 6;
						}
						else {
							break;
						}
					}
				}
			}
		}

		/* fill the appropriate preview buffers */
		if (state->mode == STITCH_VERT) {
			for (i = 0; i < state->total_separate_uvs; i++) {
				UvElement *element = (UvElement *)state->uvs[i];
				if (element->flag & STITCH_STITCHABLE) {
					l = element->l;
					luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);

					copy_v2_v2(&preview->preview_stitchable[stitchBufferIndex * 2], luv->uv);

					stitchBufferIndex++;
				}
				else if (element->flag & STITCH_SELECTED) {
					l = element->l;
					luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);

					copy_v2_v2(&preview->preview_unstitchable[unstitchBufferIndex * 2], luv->uv);
					unstitchBufferIndex++;
				}
			}
		}
		else {
			for (i = 0; i < state->total_separate_edges; i++) {
				UvEdge *edge = state->edges + i;
				UvElement *element1 = state->uvs[edge->uv1];
				UvElement *element2 = state->uvs[edge->uv2];

				if (edge->flag & STITCH_STITCHABLE) {
					l = element1->l;
					luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);
					copy_v2_v2(&preview->preview_stitchable[stitchBufferIndex * 4], luv->uv);

					l = element2->l;
					luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);
					copy_v2_v2(&preview->preview_stitchable[stitchBufferIndex * 4 + 2], luv->uv);

					stitchBufferIndex++;
					BLI_assert(stitchBufferIndex <= preview->num_stitchable);
				}
				else if (edge->flag & STITCH_SELECTED) {
					l = element1->l;
					luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);
					copy_v2_v2(&preview->preview_unstitchable[unstitchBufferIndex * 4], luv->uv);

					l = element2->l;
					luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);
					copy_v2_v2(&preview->preview_unstitchable[unstitchBufferIndex * 4 + 2], luv->uv);

					unstitchBufferIndex++;
					BLI_assert(unstitchBufferIndex <= preview->num_unstitchable);
				}
			}
		}
	}

	/******************************************************
	 * Here we calculate the final coordinates of the uvs *
	 ******************************************************/

	if (state->mode == STITCH_VERT) {
		final_position = MEM_callocN(state->selection_size * sizeof(*final_position), "stitch_uv_average");
		uvfinal_map = MEM_mallocN(state->element_map->totalUVs * sizeof(*uvfinal_map), "stitch_uv_final_map");
	}
	else {
		final_position = MEM_callocN(state->total_separate_uvs * sizeof(*final_position), "stitch_uv_average");
	}

	/* first pass, calculate final position for stitchable uvs of the static island */
	for (i = 0; i < state->selection_size; i++) {
		if (state->mode == STITCH_VERT) {
			UvElement *element = state->selection_stack[i];

			if (element->flag & STITCH_STITCHABLE) {
				BMLoop *l;
				MLoopUV *luv;
				UvElement *element_iter;

				l = element->l;
				luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);

				uvfinal_map[element - state->element_map->buf] = i;

				copy_v2_v2(final_position[i].uv, luv->uv);
				final_position[i].count = 1;

				if (state->snap_islands && element->island == state->static_island && !stitch_midpoints)
					continue;

				element_iter = state->element_map->vert[BM_elem_index_get(l->v)];

				for ( ; element_iter; element_iter = element_iter->next) {
					if (element_iter->separate) {
						if (stitch_check_uvs_state_stitchable(element, element_iter, state)) {
							l = element_iter->l;
							luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);
							if (stitch_midpoints) {
								add_v2_v2(final_position[i].uv, luv->uv);
								final_position[i].count++;
							}
							else if (element_iter->island == state->static_island) {
								/* if multiple uvs on the static island exist,
								 * last checked remains. to disambiguate we need to limit or use
								 * edge stitch */
								copy_v2_v2(final_position[i].uv, luv->uv);
							}
						}
					}
				}
			}
			if (stitch_midpoints) {
				final_position[i].uv[0] /= final_position[i].count;
				final_position[i].uv[1] /= final_position[i].count;
			}
		}
		else {
			UvEdge *edge = state->selection_stack[i];

			if (edge->flag & STITCH_STITCHABLE) {
				MLoopUV *luv2, *luv1;
				BMLoop *l;
				UvEdge *edge_iter;

				l = state->uvs[edge->uv1]->l;
				luv1 = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);
				l = state->uvs[edge->uv2]->l;
				luv2 = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);

				copy_v2_v2(final_position[edge->uv1].uv, luv1->uv);
				copy_v2_v2(final_position[edge->uv2].uv, luv2->uv);
				final_position[edge->uv1].count = 1;
				final_position[edge->uv2].count = 1;

				state->uvs[edge->uv1]->flag |= STITCH_STITCHABLE;
				state->uvs[edge->uv2]->flag |= STITCH_STITCHABLE;

				if (state->snap_islands && edge->element->island == state->static_island && !stitch_midpoints)
					continue;

				for (edge_iter = edge->first; edge_iter; edge_iter = edge_iter->next) {
					if (stitch_check_edges_state_stitchable (edge, edge_iter, state)) {
						l = state->uvs[edge_iter->uv1]->l;
						luv1 = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);
						l = state->uvs[edge_iter->uv2]->l;
						luv2 = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);

						if (stitch_midpoints) {
							add_v2_v2(final_position[edge->uv1].uv, luv1->uv);
							final_position[edge->uv1].count++;
							add_v2_v2(final_position[edge->uv2].uv, luv2->uv);
							final_position[edge->uv2].count++;
						}
						else if (edge_iter->element->island == state->static_island) {
							copy_v2_v2(final_position[edge->uv1].uv, luv1->uv);
							copy_v2_v2(final_position[edge->uv2].uv, luv2->uv);
						}
					}
				}
			}
		}
	}

	/* take mean position here. For edge case, this can't be done inside the loop for shared uvverts */
	if (state->mode == STITCH_EDGE && stitch_midpoints) {
		for (i = 0; i < state->total_separate_uvs; i++) {
			final_position[i].uv[0] /= final_position[i].count;
			final_position[i].uv[1] /= final_position[i].count;
		}
	}

	/* second pass, calculate island rotation and translation before modifying any uvs */
	if (state->snap_islands) {
		if (state->mode == STITCH_VERT) {
			for (i = 0; i < state->selection_size; i++) {
				UvElement *element = state->selection_stack[i];

				if (element->flag & STITCH_STITCHABLE) {
					BMLoop *l;
					MLoopUV *luv;

					l = element->l;
					luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);

					/* accumulate each islands' translation from stitchable elements. it is important to do here
					 * because in final pass MTFaces get modified and result is zero. */
					island_stitch_data[element->island].translation[0] += final_position[i].uv[0] - luv->uv[0];
					island_stitch_data[element->island].translation[1] += final_position[i].uv[1] - luv->uv[1];
					island_stitch_data[element->island].medianPoint[0] += luv->uv[0];
					island_stitch_data[element->island].medianPoint[1] += luv->uv[1];
					island_stitch_data[element->island].numOfElements++;
				}
			}

			/* only calculate rotation when an edge has been fully selected */
			for (i = 0; i < state->total_separate_edges; i++) {
				UvEdge *edge = state->edges + i;
				if ((edge->flag & STITCH_BOUNDARY) && (state->uvs[edge->uv1]->flag & STITCH_STITCHABLE) && (state->uvs[edge->uv2]->flag & STITCH_STITCHABLE)) {
					stitch_island_calculate_edge_rotation(edge, state, final_position, uvfinal_map, island_stitch_data);
					island_stitch_data[state->uvs[edge->uv1]->island].use_edge_rotation = true;
				}
			}

			/* clear seams of stitched edges */
			if (final && state->clear_seams) {
				for (i = 0; i < state->total_separate_edges; i++) {
					UvEdge *edge = state->edges + i;
					if ((state->uvs[edge->uv1]->flag & STITCH_STITCHABLE) && (state->uvs[edge->uv2]->flag & STITCH_STITCHABLE))
						BM_elem_flag_disable(edge->element->l->e, BM_ELEM_SEAM);
				}
			}

			for (i = 0; i < state->selection_size; i++) {
				UvElement *element = state->selection_stack[i];
				if (!island_stitch_data[element->island].use_edge_rotation) {
					if (element->flag & STITCH_STITCHABLE) {
						stitch_island_calculate_vert_rotation(element, state, island_stitch_data);
					}
				}
			}
		}
		else {
			for (i = 0; i < state->total_separate_uvs; i++) {
				UvElement *element = state->uvs[i];

				if (element->flag & STITCH_STITCHABLE) {
					BMLoop *l;
					MLoopUV *luv;

					l = element->l;
					luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);

					/* accumulate each islands' translation from stitchable elements. it is important to do here
					 * because in final pass MTFaces get modified and result is zero. */
					island_stitch_data[element->island].translation[0] += final_position[i].uv[0] - luv->uv[0];
					island_stitch_data[element->island].translation[1] += final_position[i].uv[1] - luv->uv[1];
					island_stitch_data[element->island].medianPoint[0] += luv->uv[0];
					island_stitch_data[element->island].medianPoint[1] += luv->uv[1];
					island_stitch_data[element->island].numOfElements++;
				}
			}

			for (i = 0; i < state->selection_size; i++) {
				UvEdge *edge = state->selection_stack[i];

				if (edge->flag & STITCH_STITCHABLE) {
					stitch_island_calculate_edge_rotation(edge, state, final_position, NULL, island_stitch_data);
					island_stitch_data[state->uvs[edge->uv1]->island].use_edge_rotation = true;
				}
			}

			/* clear seams of stitched edges */
			if (final && state->clear_seams) {
				for (i = 0; i < state->selection_size; i++) {
					UvEdge *edge = state->selection_stack[i];
					if (edge->flag & STITCH_STITCHABLE) {
						BM_elem_flag_disable(edge->element->l->e, BM_ELEM_SEAM);
					}
				}
			}
		}
	}

	/* third pass, propagate changes to coincident uvs */
	for (i = 0; i < state->selection_size; i++) {
		if (state->mode == STITCH_VERT) {
			UvElement *element = state->selection_stack[i];

			stitch_propagate_uv_final_position(scene, element, i, preview_position, final_position, state, final);
		}
		else {
			UvEdge *edge = state->selection_stack[i];

			stitch_propagate_uv_final_position(scene, state->uvs[edge->uv1], edge->uv1, preview_position, final_position, state, final);
			stitch_propagate_uv_final_position(scene, state->uvs[edge->uv2], edge->uv2, preview_position, final_position, state, final);

			edge->flag &= (STITCH_SELECTED | STITCH_BOUNDARY);
		}
	}

	/* final pass, calculate Island translation/rotation if needed */
	if (state->snap_islands) {
		stitch_calculate_island_snapping(state, preview_position, preview, island_stitch_data, final);
	}

	MEM_freeN(final_position);
	if (state->mode == STITCH_VERT) {
		MEM_freeN(uvfinal_map);
	}
	MEM_freeN(island_stitch_data);
	MEM_freeN(preview_position);

	return 1;
}

/* Stitch hash initialization functions */
static unsigned int uv_edge_hash(const void *key)
{
	UvEdge *edge = (UvEdge *)key;
	return
	        BLI_ghashutil_uinthash(edge->uv2) +
	        BLI_ghashutil_uinthash(edge->uv1);
}

static bool uv_edge_compare(const void *a, const void *b)
{
	UvEdge *edge1 = (UvEdge *)a;
	UvEdge *edge2 = (UvEdge *)b;

	if ((edge1->uv1 == edge2->uv1) && (edge1->uv2 == edge2->uv2)) {
		return 0;
	}
	return 1;
}

/* select all common edges */
static void stitch_select_edge(UvEdge *edge, StitchState *state, int always_select)
{
	UvEdge *eiter;
	UvEdge **selection_stack = (UvEdge **)state->selection_stack;

	for (eiter = edge->first; eiter; eiter = eiter->next) {
		if (eiter->flag & STITCH_SELECTED) {
			int i;
			if (always_select)
				continue;

			eiter->flag &= ~STITCH_SELECTED;
			for (i = 0; i < state->selection_size; i++) {
				if (selection_stack[i] == eiter) {
					(state->selection_size)--;
					selection_stack[i] = selection_stack[state->selection_size];
					break;
				}
			}
		}
		else {
			eiter->flag |= STITCH_SELECTED;
			selection_stack[state->selection_size++] = eiter;
		}
	}
}


/* Select all common uvs */
static void stitch_select_uv(UvElement *element, StitchState *state, int always_select)
{
	BMLoop *l;
	UvElement *element_iter;
	UvElement **selection_stack = (UvElement **)state->selection_stack;

	l = element->l;

	element_iter = state->element_map->vert[BM_elem_index_get(l->v)];
	/* first deselect all common uvs */
	for (; element_iter; element_iter = element_iter->next) {
		if (element_iter->separate) {
			/* only separators go to selection */
			if (element_iter->flag & STITCH_SELECTED) {
				int i;
				if (always_select)
					continue;

				element_iter->flag &= ~STITCH_SELECTED;
				for (i = 0; i < state->selection_size; i++) {
					if (selection_stack[i] == element_iter) {
						(state->selection_size)--;
						selection_stack[i] = selection_stack[state->selection_size];
						break;
					}
				}
			}
			else {
				element_iter->flag |= STITCH_SELECTED;
				selection_stack[state->selection_size++] = element_iter;
			}
		}
	}
}

static void stitch_switch_selection_mode(StitchState *state)
{
	void **old_selection_stack = state->selection_stack;
	int old_selection_size = state->selection_size;
	state->selection_size = 0;

	if (state->mode == STITCH_VERT) {
		int i;
		state->selection_stack = MEM_mallocN(state->total_separate_edges * sizeof(*state->selection_stack),
		                                     "stitch_new_edge_selection_stack");

		/* check if both elements of an edge are selected */
		for (i = 0; i < state->total_separate_edges; i++) {
			UvEdge *edge = state->edges + i;
			UvElement *element1 = state->uvs[edge->uv1];
			UvElement *element2 = state->uvs[edge->uv2];

			if ((element1->flag & STITCH_SELECTED) && (element2->flag & STITCH_SELECTED))
				stitch_select_edge(edge, state, true);
		}

		/* unselect selected uvelements */
		for (i = 0; i < old_selection_size; i++) {
			UvElement *element = old_selection_stack[i];

			element->flag &= ~STITCH_SELECTED;
		}
		state->mode = STITCH_EDGE;
	}
	else {
		int i;
		state->selection_stack = MEM_mallocN(state->total_separate_uvs * sizeof(*state->selection_stack),
		                                     "stitch_new_vert_selection_stack");

		for (i = 0; i < old_selection_size; i++) {
			UvEdge *edge = old_selection_stack[i];
			UvElement *element1 = state->uvs[edge->uv1];
			UvElement *element2 = state->uvs[edge->uv2];

			stitch_select_uv(element1, state, true);
			stitch_select_uv(element2, state, true);

			edge->flag &= ~STITCH_SELECTED;
		}
		state->mode = STITCH_VERT;
	}
	MEM_freeN(old_selection_stack);
}

static void stitch_calculate_edge_normal(BMEditMesh *em, UvEdge *edge, float *normal, float aspect)
{
	BMLoop *l1 = edge->element->l;
	MLoopUV *luv1, *luv2;
	float tangent[2];

	luv1 = CustomData_bmesh_get(&em->bm->ldata, l1->head.data, CD_MLOOPUV);
	luv2 = CustomData_bmesh_get(&em->bm->ldata, l1->next->head.data, CD_MLOOPUV);

	sub_v2_v2v2(tangent, luv2->uv,  luv1->uv);

	tangent[1] /= aspect;

	normal[0] = tangent[1];
	normal[1] = -tangent[0];

	normalize_v2(normal);
}

static void stitch_draw(const bContext *UNUSED(C), ARegion *UNUSED(ar), void *arg)
{
	int i, index = 0;
	float pointsize = UI_GetThemeValuef(TH_VERTEX_SIZE);
	StitchState *state = (StitchState *)arg;
	StitchPreviewer *stitch_preview = state->stitch_preview;

	glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
	glEnableClientState(GL_VERTEX_ARRAY);

	glPointSize(pointsize * 2.0f);

	glEnable(GL_BLEND);

	UI_ThemeColor4(TH_STITCH_PREVIEW_ACTIVE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glVertexPointer(2, GL_FLOAT, 0, stitch_preview->static_tris);
	glDrawArrays(GL_TRIANGLES, 0, stitch_preview->num_static_tris * 3);

	glVertexPointer(2, GL_FLOAT, 0, stitch_preview->preview_polys);
	for (i = 0; i < stitch_preview->num_polys; i++) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		UI_ThemeColor4(TH_STITCH_PREVIEW_FACE);
		glDrawArrays(GL_POLYGON, index, stitch_preview->uvs_per_polygon[i]);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		UI_ThemeColor4(TH_STITCH_PREVIEW_EDGE);
		glDrawArrays(GL_POLYGON, index, stitch_preview->uvs_per_polygon[i]);
#if 0
		glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
		UI_ThemeColor4(TH_STITCH_PREVIEW_VERT);
		glDrawArrays(GL_POLYGON, index, stitch_preview->uvs_per_polygon[i]);
#endif

		index += stitch_preview->uvs_per_polygon[i];
	}
	glDisable(GL_BLEND);

	/* draw vert preview */
	if (state->mode == STITCH_VERT) {
		UI_ThemeColor4(TH_STITCH_PREVIEW_STITCHABLE);
		glVertexPointer(2, GL_FLOAT, 0, stitch_preview->preview_stitchable);
		glDrawArrays(GL_POINTS, 0, stitch_preview->num_stitchable);

		UI_ThemeColor4(TH_STITCH_PREVIEW_UNSTITCHABLE);
		glVertexPointer(2, GL_FLOAT, 0, stitch_preview->preview_unstitchable);
		glDrawArrays(GL_POINTS, 0, stitch_preview->num_unstitchable);
	}
	else {
		UI_ThemeColor4(TH_STITCH_PREVIEW_STITCHABLE);
		glVertexPointer(2, GL_FLOAT, 0, stitch_preview->preview_stitchable);
		glDrawArrays(GL_LINES, 0, 2 * stitch_preview->num_stitchable);

		UI_ThemeColor4(TH_STITCH_PREVIEW_UNSTITCHABLE);
		glVertexPointer(2, GL_FLOAT, 0, stitch_preview->preview_unstitchable);
		glDrawArrays(GL_LINES, 0, 2 * stitch_preview->num_unstitchable);
	}

	glPopClientAttrib();
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glPointSize(1.0);
}

static UvEdge *uv_edge_get(BMLoop *l, StitchState *state)
{
	UvEdge tmp_edge;

	UvElement *element1 = BM_uv_element_get(state->element_map, l->f, l);
	UvElement *element2 = BM_uv_element_get(state->element_map, l->f, l->next);

	int uv1 = state->map[element1 - state->element_map->buf];
	int uv2 = state->map[element2 - state->element_map->buf];

	if (uv1 < uv2) {
		tmp_edge.uv1 = uv1;
		tmp_edge.uv2 = uv2;
	}
	else {
		tmp_edge.uv1 = uv2;
		tmp_edge.uv2 = uv1;
	}

	return BLI_ghash_lookup(state->edge_hash, &tmp_edge);
}

static int stitch_init(bContext *C, wmOperator *op)
{
	/* for fast edge lookup... */
	GHash *edge_hash;
	/* ...and actual edge storage */
	UvEdge *edges;
	int total_edges;
	/* maps uvelements to their first coincident uv */
	int *map;
	int counter = 0, i;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	GHashIterator *ghi;
	UvEdge *all_edges;
	StitchState *state;
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = scene->toolsettings;
	ARegion *ar = CTX_wm_region(C);
	float aspx, aspy;
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

	if (!ar)
		return 0;

	state = MEM_callocN(sizeof(StitchState), "stitch state");

	op->customdata = state;

	/* initialize state */
	state->use_limit = RNA_boolean_get(op->ptr, "use_limit");
	state->limit_dist = RNA_float_get(op->ptr, "limit");
	state->em = em;
	state->snap_islands = RNA_boolean_get(op->ptr, "snap_islands");
	state->static_island = RNA_int_get(op->ptr, "static_island");
	state->midpoints = RNA_boolean_get(op->ptr, "midpoint_snap");
	state->clear_seams = RNA_boolean_get(op->ptr, "clear_seams");
	if (RNA_struct_property_is_set(op->ptr, "mode")) {
		state->mode = RNA_enum_get(op->ptr, "mode");
	}
	else {
		if (ts->uv_flag & UV_SYNC_SELECTION) {
			if (ts->selectmode & SCE_SELECT_VERTEX)
				state->mode = STITCH_VERT;
			else
				state->mode = STITCH_EDGE;
		}
		else {
			if (ts->uv_selectmode & UV_SELECT_VERTEX) {
				state->mode = STITCH_VERT;
			}
			else {
				state->mode = STITCH_EDGE;
			}
		}
	}

	/* in uv synch selection, all uv's are visible */
	if (ts->uv_flag & UV_SYNC_SELECTION) {
		state->element_map = BM_uv_element_map_create(state->em->bm, false, true);
	}
	else {
		state->element_map = BM_uv_element_map_create(state->em->bm, true, true);
	}
	if (!state->element_map) {
		state_delete(state);
		return 0;
	}

	ED_uvedit_get_aspect(scene, obedit, em->bm, &aspx, &aspy);
	state->aspect = aspx / aspy;

	/* Entirely possible if redoing last operator that static island is bigger than total number of islands.
	 * This ensures we get no hang in the island checking code in stitch_stitch_process_data. */
	state->static_island %= state->element_map->totalIslands;

	/* Count 'unique' uvs */
	for (i = 0; i < state->element_map->totalUVs; i++) {
		if (state->element_map->buf[i].separate) {
			counter++;
		}
	}

	/* explicitly set preview to NULL, to avoid deleting an invalid pointer on stitch_process_data */
	state->stitch_preview = NULL;
	/* Allocate the unique uv buffers */
	state->uvs = MEM_mallocN(sizeof(*state->uvs) * counter, "uv_stitch_unique_uvs");
	/* internal uvs need no normals but it is hard and slow to keep a map of
	 * normals only for boundary uvs, so allocating for all uvs */
	state->normals = MEM_callocN(sizeof(*state->normals) * counter * 2, "uv_stitch_normals");
	state->total_separate_uvs = counter;
	state->map = map = MEM_mallocN(sizeof(*map) * state->element_map->totalUVs, "uv_stitch_unique_map");
	/* Allocate the edge stack */
	edge_hash = BLI_ghash_new(uv_edge_hash, uv_edge_compare, "stitch_edge_hash");
	all_edges = MEM_mallocN(sizeof(*all_edges) * state->element_map->totalUVs, "stitch_all_edges");

	if (!state->uvs || !map || !edge_hash || !all_edges) {
		state_delete(state);
		return 0;
	}

	/* So that we can use this as index for the UvElements */
	counter = -1;
	/* initialize the unique UVs and map */
	for (i = 0; i < em->bm->totvert; i++) {
		UvElement *element = state->element_map->vert[i];
		for (; element; element = element->next) {
			if (element->separate) {
				counter++;
				state->uvs[counter] = element;
			}
			/* pointer arithmetic to the rescue, as always :)*/
			map[element - state->element_map->buf] = counter;
		}
	}

	counter = 0;
	/* Now, on to generate our uv connectivity data */
	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		if (!(ts->uv_flag & UV_SYNC_SELECTION) && ((BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) || !BM_elem_flag_test(efa, BM_ELEM_SELECT)))
			continue;

		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			UvElement *element = BM_uv_element_get(state->element_map, efa, l);
			int offset1, itmp1 = element - state->element_map->buf;
			int offset2, itmp2 = BM_uv_element_get(state->element_map, efa, l->next) - state->element_map->buf;
			UvEdge *edge;

			offset1 = map[itmp1];
			offset2 = map[itmp2];

			all_edges[counter].next = NULL;
			all_edges[counter].first = NULL;
			all_edges[counter].flag = 0;
			all_edges[counter].element = element;
			/* using an order policy, sort uvs according to address space. This avoids
			 * Having two different UvEdges with the same uvs on different positions  */
			if (offset1 < offset2) {
				all_edges[counter].uv1 = offset1;
				all_edges[counter].uv2 = offset2;
			}
			else {
				all_edges[counter].uv1 = offset2;
				all_edges[counter].uv2 = offset1;
			}

			edge = BLI_ghash_lookup(edge_hash, &all_edges[counter]);
			if (edge) {
				edge->flag = 0;
			}
			else {
				BLI_ghash_insert(edge_hash, &all_edges[counter], &all_edges[counter]);
				all_edges[counter].flag = STITCH_BOUNDARY;
			}
			counter++;
		}
	}


	ghi = BLI_ghashIterator_new(edge_hash);
	total_edges = BLI_ghash_size(edge_hash);
	state->edges = edges = MEM_mallocN(sizeof(*edges) * total_edges, "stitch_edges");

	/* I assume any system will be able to at least allocate an iterator :p */
	if (!edges) {
		BLI_ghashIterator_free(ghi);
		state_delete(state);
		return 0;
	}

	state->total_separate_edges = total_edges;

	/* fill the edges with data */
	for (i = 0, BLI_ghashIterator_init(ghi, edge_hash); !BLI_ghashIterator_done(ghi); BLI_ghashIterator_step(ghi)) {
		edges[i++] = *((UvEdge *)BLI_ghashIterator_getKey(ghi));
	}

	/* cleanup temporary stuff */
	BLI_ghashIterator_free(ghi);
	MEM_freeN(all_edges);

	BLI_ghash_free(edge_hash, NULL, NULL);

	/* refill an edge hash to create edge connnectivity data */
	state->edge_hash = edge_hash = BLI_ghash_new(uv_edge_hash, uv_edge_compare, "stitch_edge_hash");
	for (i = 0; i < total_edges; i++) {
		BLI_ghash_insert(edge_hash, edges + i, edges + i);
	}
	stitch_uv_edge_generate_linked_edges(edge_hash, state);

	/***** calculate 2D normals for boundary uvs *****/

	/* we use boundary edges to calculate 2D normals.
	 * to disambiguate the direction of the normal, we also need
	 * a point "inside" the island, that can be provided by
	 * the winding of the polygon (assuming counter-clockwise flow). */

	for (i = 0; i < total_edges; i++) {
		UvEdge *edge = edges + i;
		float normal[2];
		if (edge->flag & STITCH_BOUNDARY) {
			stitch_calculate_edge_normal(em, edge, normal, state->aspect);

			add_v2_v2(state->normals + edge->uv1 * 2, normal);
			add_v2_v2(state->normals + edge->uv2 * 2, normal);

			normalize_v2(state->normals + edge->uv1 * 2);
			normalize_v2(state->normals + edge->uv2 * 2);
		}
	}


	/***** fill selection stack *******/

	state->selection_size = 0;

	/* Load old selection if redoing operator with different settings */
	if (RNA_struct_property_is_set(op->ptr, "selection")) {
		int faceIndex, elementIndex;
		UvElement *element;
		enum StitchModes stored_mode = RNA_enum_get(op->ptr, "stored_mode");

		BM_mesh_elem_table_ensure(em->bm, BM_FACE);

		if (stored_mode == STITCH_VERT) {
			state->selection_stack = MEM_mallocN(sizeof(*state->selection_stack) * state->total_separate_uvs, "uv_stitch_selection_stack");

			RNA_BEGIN (op->ptr, itemptr, "selection")
			{
				faceIndex = RNA_int_get(&itemptr, "face_index");
				elementIndex = RNA_int_get(&itemptr, "element_index");
				efa = BM_face_at_index(em->bm, faceIndex);
				element = BM_uv_element_get(state->element_map, efa, BM_iter_at_index(NULL, BM_LOOPS_OF_FACE, efa, elementIndex));
				stitch_select_uv(element, state, 1);
			}
			RNA_END;
		}
		else {
			state->selection_stack = MEM_mallocN(sizeof(*state->selection_stack) * state->total_separate_edges, "uv_stitch_selection_stack");

			RNA_BEGIN (op->ptr, itemptr, "selection")
			{
				UvEdge tmp_edge, *edge;
				int uv1, uv2;
				faceIndex = RNA_int_get(&itemptr, "face_index");
				elementIndex = RNA_int_get(&itemptr, "element_index");
				efa = BM_face_at_index(em->bm, faceIndex);
				element = BM_uv_element_get(state->element_map, efa, BM_iter_at_index(NULL, BM_LOOPS_OF_FACE, efa, elementIndex));
				uv1 = map[element - state->element_map->buf];

				element = BM_uv_element_get(state->element_map, efa, BM_iter_at_index(NULL, BM_LOOPS_OF_FACE, efa, (elementIndex + 1) % efa->len));
				uv2 = map[element - state->element_map->buf];

				if (uv1 < uv2) {
					tmp_edge.uv1 = uv1;
					tmp_edge.uv2 = uv2;
				}
				else {
					tmp_edge.uv1 = uv2;
					tmp_edge.uv2 = uv1;
				}

				edge = BLI_ghash_lookup(edge_hash, &tmp_edge);

				stitch_select_edge(edge, state, true);
			}
			RNA_END;
		}
		/* if user has switched the operator mode after operation, we need to convert
		 * the stored format */
		if (state->mode != stored_mode) {
			state->mode = stored_mode;
			stitch_switch_selection_mode(state);
		}
		/* Clear the selection */
		RNA_collection_clear(op->ptr, "selection");

	}
	else {
		if (state->mode == STITCH_VERT) {
			state->selection_stack = MEM_mallocN(sizeof(*state->selection_stack) * state->total_separate_uvs, "uv_stitch_selection_stack");

			BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
				BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
					if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
						UvElement *element = BM_uv_element_get(state->element_map, efa, l);
						if (element) {
							stitch_select_uv(element, state, 1);
						}
					}
				}
			}
		}
		else {
			state->selection_stack = MEM_mallocN(sizeof(*state->selection_stack) * state->total_separate_edges, "uv_stitch_selection_stack");

			BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
				if (!(ts->uv_flag & UV_SYNC_SELECTION) && ((BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) || !BM_elem_flag_test(efa, BM_ELEM_SELECT)))
					continue;

				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					if (uvedit_edge_select_test(scene, l, cd_loop_uv_offset)) {
						UvEdge *edge = uv_edge_get(l, state);
						if (edge) {
							stitch_select_edge(edge, state, true);
						}
					}
				}
			}
		}
	}

	/***** initialize static island preview data *****/

	state->tris_per_island = MEM_mallocN(sizeof(*state->tris_per_island) * state->element_map->totalIslands,
	                                     "stitch island tris");
	for (i = 0; i < state->element_map->totalIslands; i++) {
		state->tris_per_island[i] = 0;
	}

	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		UvElement *element = BM_uv_element_get(state->element_map, efa, BM_FACE_FIRST_LOOP(efa));

		if (element) {
			state->tris_per_island[element->island] += (efa->len > 2) ? efa->len - 2 : 0;
		}
	}

	if (!stitch_process_data(state, scene, false)) {

		state_delete(state);
		return 0;
	}

	state->draw_handle = ED_region_draw_cb_activate(ar->type, stitch_draw, state, REGION_DRAW_POST_VIEW);

	stitch_update_header(state, C);
	return 1;
}

static int stitch_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	Object *obedit = CTX_data_edit_object(C);
	if (!stitch_init(C, op))
		return OPERATOR_CANCELLED;

	WM_event_add_modal_handler(C, op);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
	return OPERATOR_RUNNING_MODAL;
}

static void stitch_exit(bContext *C, wmOperator *op, int finished)
{
	StitchState *state;
	Scene *scene;
	SpaceImage *sima;
	ScrArea *sa = CTX_wm_area(C);
	Object *obedit;

	scene = CTX_data_scene(C);
	obedit = CTX_data_edit_object(C);
	sima = CTX_wm_space_image(C);

	state = (StitchState *)op->customdata;

	if (finished) {
		int i;

		RNA_float_set(op->ptr, "limit", state->limit_dist);
		RNA_boolean_set(op->ptr, "use_limit", state->use_limit);
		RNA_boolean_set(op->ptr, "snap_islands", state->snap_islands);
		RNA_int_set(op->ptr, "static_island", state->static_island);
		RNA_boolean_set(op->ptr, "midpoint_snap", state->midpoints);
		RNA_enum_set(op->ptr, "mode", state->mode);
		RNA_enum_set(op->ptr, "stored_mode", state->mode);

		/* Store selection for re-execution of stitch */
		for (i = 0; i < state->selection_size; i++) {
			UvElement *element;
			PointerRNA itemptr;
			if (state->mode == STITCH_VERT) {
				element = state->selection_stack[i];
			}
			else {
				element = ((UvEdge *)state->selection_stack[i])->element;
			}
			RNA_collection_add(op->ptr, "selection", &itemptr);

			RNA_int_set(&itemptr, "face_index", BM_elem_index_get(element->l->f));
			RNA_int_set(&itemptr, "element_index", element->tfindex);
		}

		uvedit_live_unwrap_update(sima, scene, obedit);
	}

	if (sa)
		ED_area_headerprint(sa, NULL);

	ED_region_draw_cb_exit(CTX_wm_region(C)->type, state->draw_handle);

	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);

	state_delete(state);
	op->customdata = NULL;
}


static void stitch_cancel(bContext *C, wmOperator *op)
{
	stitch_exit(C, op, 0);
}


static int stitch_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);

	if (!stitch_init(C, op))
		return OPERATOR_CANCELLED;
	if (stitch_process_data((StitchState *)op->customdata, scene, 1)) {
		stitch_exit(C, op, 1);
		return OPERATOR_FINISHED;
	}
	else {
		stitch_cancel(C, op);
		return OPERATOR_CANCELLED;
	}
}

static void stitch_select(bContext *C, Scene *scene, const wmEvent *event, StitchState *state)
{
	/* add uv under mouse to processed uv's */
	float co[2];
	NearestHit hit;
	ARegion *ar = CTX_wm_region(C);
	Image *ima = CTX_data_edit_image(C);

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &co[0], &co[1]);

	if (state->mode == STITCH_VERT) {
		uv_find_nearest_vert(scene, ima, state->em, co, NULL, &hit);

		if (hit.efa) {
			/* Add vertex to selection, deselect all common uv's of vert other
			 * than selected and update the preview. This behavior was decided so that
			 * you can do stuff like deselect the opposite stitchable vertex and the initial still gets deselected */

			/* This works due to setting of tmp in find nearest uv vert */
			UvElement *element = BM_uv_element_get(state->element_map, hit.efa, hit.l);
			stitch_select_uv(element, state, false);

		}
	}
	else {
		uv_find_nearest_edge(scene, ima, state->em, co, &hit);

		if (hit.efa) {
			UvEdge *edge = uv_edge_get(hit.l, state);
			stitch_select_edge(edge, state, false);
		}
	}
}

static int stitch_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	StitchState *state;
	Scene *scene = CTX_data_scene(C);

	state = (StitchState *)op->customdata;

	switch (event->type) {
		case MIDDLEMOUSE:
			return OPERATOR_PASS_THROUGH;

			/* Cancel */
		case ESCKEY:
			stitch_cancel(C, op);
			return OPERATOR_CANCELLED;

		case LEFTMOUSE:
			if (event->shift && (U.flag & USER_LMOUSESELECT)) {
				if (event->val == KM_PRESS) {
					stitch_select(C, scene, event, state);

					if (!stitch_process_data(state, scene, false)) {
						stitch_cancel(C, op);
						return OPERATOR_CANCELLED;
					}
				}
				break;
			}
		case PADENTER:
		case RETKEY:
			if (event->val == KM_PRESS) {
				if (stitch_process_data(state, scene, true)) {
					stitch_exit(C, op, 1);
					return OPERATOR_FINISHED;
				}
				else {
					stitch_cancel(C, op);
					return OPERATOR_CANCELLED;
				}
			}
			else {
				return OPERATOR_PASS_THROUGH;
			}
			/* Increase limit */
		case PADPLUSKEY:
		case WHEELUPMOUSE:
			if (event->val == KM_PRESS && event->alt) {
				state->limit_dist += 0.01f;
				if (!stitch_process_data(state, scene, false)) {
					stitch_cancel(C, op);
					return OPERATOR_CANCELLED;
				}
				break;
			}
			else {
				return OPERATOR_PASS_THROUGH;
			}
			/* Decrease limit */
		case PADMINUS:
		case WHEELDOWNMOUSE:
			if (event->val == KM_PRESS && event->alt) {
				state->limit_dist -= 0.01f;
				state->limit_dist = MAX2(0.01f, state->limit_dist);
				if (!stitch_process_data(state, scene, false)) {
					stitch_cancel(C, op);
					return OPERATOR_CANCELLED;
				}
				break;
			}
			else {
				return OPERATOR_PASS_THROUGH;
			}

			/* Use Limit (Default off)*/
		case LKEY:
			if (event->val == KM_PRESS) {
				state->use_limit = !state->use_limit;
				if (!stitch_process_data(state, scene, false)) {
					stitch_cancel(C, op);
					return OPERATOR_CANCELLED;
				}
				break;
			}
			return OPERATOR_RUNNING_MODAL;

		case IKEY:
			if (event->val == KM_PRESS) {
				state->static_island++;
				state->static_island %= state->element_map->totalIslands;

				if (!stitch_process_data(state, scene, false)) {
					stitch_cancel(C, op);
					return OPERATOR_CANCELLED;
				}
				break;
			}
			return OPERATOR_RUNNING_MODAL;

		case MKEY:
			if (event->val == KM_PRESS) {
				state->midpoints = !state->midpoints;
				if (!stitch_process_data(state, scene, false)) {
					stitch_cancel(C, op);
					return OPERATOR_CANCELLED;
				}
			}
			break;

			/* Select geometry*/
		case RIGHTMOUSE:
			if (!event->shift) {
				stitch_cancel(C, op);
				return OPERATOR_CANCELLED;
			}
			if (event->val == KM_PRESS && !(U.flag & USER_LMOUSESELECT)) {
				stitch_select(C, scene, event, state);

				if (!stitch_process_data(state, scene, false)) {
					stitch_cancel(C, op);
					return OPERATOR_CANCELLED;
				}
				break;
			}
			return OPERATOR_RUNNING_MODAL;

			/* snap islands on/off */
		case SKEY:
			if (event->val == KM_PRESS) {
				state->snap_islands = !state->snap_islands;
				if (!stitch_process_data(state, scene, false)) {
					stitch_cancel(C, op);
					return OPERATOR_CANCELLED;
				}
				break;
			}
			else {
				return OPERATOR_RUNNING_MODAL;
			}

			/* switch between edge/vertex mode */
		case TABKEY:
			if (event->val == KM_PRESS) {
				stitch_switch_selection_mode(state);

				if (!stitch_process_data(state, scene, false)) {
					stitch_cancel(C, op);
					return OPERATOR_CANCELLED;
				}
			}
			break;

		default:
			return OPERATOR_RUNNING_MODAL;
	}

	/* if updated settings, renew feedback message */
	stitch_update_header(state, C);
	ED_region_tag_redraw(CTX_wm_region(C));
	return OPERATOR_RUNNING_MODAL;
}

void UV_OT_stitch(wmOperatorType *ot)
{
	PropertyRNA *prop;

	static EnumPropertyItem stitch_modes[] = {
	    {STITCH_VERT, "VERTEX", 0, "Vertex", ""},
	    {STITCH_EDGE, "EDGE", 0, "Edge", ""},
	    {0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Stitch";
	ot->description = "Stitch selected UV vertices by proximity";
	ot->idname = "UV_OT_stitch";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* api callbacks */
	ot->invoke = stitch_invoke;
	ot->modal = stitch_modal;
	ot->exec = stitch_exec;
	ot->cancel = stitch_cancel;
	ot->poll = ED_operator_uvedit;

	/* properties */
	RNA_def_boolean(ot->srna, "use_limit", 0, "Use Limit", "Stitch UVs within a specified limit distance");
	RNA_def_boolean(ot->srna, "snap_islands", 1, "Snap Islands",
	                "Snap islands together (on edge stitch mode, rotates the islands too)");

	RNA_def_float(ot->srna, "limit", 0.01f, 0.0f, FLT_MAX, "Limit",
	              "Limit distance in normalized coordinates", 0.0, FLT_MAX);
	RNA_def_int(ot->srna, "static_island", 0, 0, INT_MAX, "Static Island",
	            "Island that stays in place when stitching islands", 0, INT_MAX);
	RNA_def_boolean(ot->srna, "midpoint_snap", 0, "Snap At Midpoint",
	                "UVs are stitched at midpoint instead of at static island");
	RNA_def_boolean(ot->srna, "clear_seams", 1, "Clear Seams",
	                "Clear seams of stitched edges");
	RNA_def_enum(ot->srna, "mode", stitch_modes, STITCH_VERT, "Operation Mode",
	             "Use vertex or edge stitching");
	prop = 	RNA_def_enum(ot->srna, "stored_mode", stitch_modes, STITCH_VERT, "Stored Operation Mode",
	                     "Use vertex or edge stitching");
	RNA_def_property_flag(prop, PROP_HIDDEN);
	prop = RNA_def_collection_runtime(ot->srna, "selection", &RNA_SelectedUvElement, "Selection", "");
	/* Selection should not be editable or viewed in toolbar */
	RNA_def_property_flag(prop, PROP_HIDDEN);
}
