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
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"

#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_mesh.h"
#include "BKE_tessmesh.h"

#include "ED_mesh.h"
#include "ED_uvedit.h"
#include "ED_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"

#include "uvedit_intern.h"

/* ********************** smart stitch operator *********************** */


struct IslandStitchData;

/* This is a straightforward implementation, count the uv's in the island that will move and take the mean displacement/rotation and apply it to all
 * elements of the island except from the stitchable */
typedef struct IslandStitchData {
	/* rotation can be used only for edges, for vertices there is no such notion */
	float rotation;
	float translation[2];
	/* Used for rotation, the island will rotate around this point */
	float medianPoint[2];
	int numOfElements;
	int num_rot_elements;
	/* flag to remember if island has been added for preview */
	char addedForPreview;
	/* flag an island to be considered for determining static island */
	char stitchableCandidate;
	/* if edge rotation is used, flag so that vertex rotation is not used */
	char use_edge_rotation;
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
	char flag;
	/* element that guarantees element->face has the face on element->tfindex and element->tfindex+1 is the second uv */
	UvElement *element;
} UvEdge;


/* stitch state object */
typedef struct StitchState {
	/* use limit flag */
	char use_limit;
	/* limit to operator, same as original operator */
	float limit_dist;
	/* snap uv islands together during stitching */
	char snap_islands;
	/* stich at midpoints or at islands */
	char midpoints;
	/* editmesh, cached for use in modal handler */
	BMEditMesh *em;
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

	/* count of separate uvs and edges */
	int total_boundary_edges;
	int total_separate_uvs;
	/* hold selection related information */
	UvElement **selection_stack;
	int selection_size;
	/* island that stays in place */
	int static_island;
	/* store number of primitives per face so that we can allocate the active island buffer later */
	unsigned int *tris_per_island;
} StitchState;

typedef struct PreviewPosition {
	int data_position;
	int polycount_position;
} PreviewPosition;
/*
 * defines for UvElement flags
 */
#define STITCH_SELECTED 1
#define STITCH_STITCHABLE 2
#define STITCH_PROCESSED 4
#define STITCH_BOUNDARY 8
#define STITCH_STITCHABLE_CANDIDATE 16

#define STITCH_NO_PREVIEW -1

/* previewer stuff (see uvedit_intern.h for more info) */
static StitchPreviewer *_stitch_preview;

/* constructor */
static StitchPreviewer *stitch_preview_init(void)
{
	_stitch_preview = MEM_mallocN(sizeof(StitchPreviewer), "stitch_previewer");
	_stitch_preview->preview_polys = NULL;
	_stitch_preview->preview_stitchable = NULL;
	_stitch_preview->preview_unstitchable = NULL;
	_stitch_preview->uvs_per_polygon = NULL;

	_stitch_preview->preview_uvs = 0;
	_stitch_preview->num_polys = 0;
	_stitch_preview->num_stitchable = 0;
	_stitch_preview->num_unstitchable = 0;

	_stitch_preview->static_tris = NULL;

	_stitch_preview->num_static_tris = 0;

	return _stitch_preview;
}

/* destructor...yeah this should be C++ :) */
static void stitch_preview_delete(void)
{
	if (_stitch_preview) {
		if (_stitch_preview->preview_polys) {
			MEM_freeN(_stitch_preview->preview_polys);
			_stitch_preview->preview_polys = NULL;
		}
		if (_stitch_preview->uvs_per_polygon) {
			MEM_freeN(_stitch_preview->uvs_per_polygon);
			_stitch_preview->uvs_per_polygon = NULL;
		}
		if (_stitch_preview->preview_stitchable) {
			MEM_freeN(_stitch_preview->preview_stitchable);
			_stitch_preview->preview_stitchable = NULL;
		}
		if (_stitch_preview->preview_unstitchable) {
			MEM_freeN(_stitch_preview->preview_unstitchable);
			_stitch_preview->preview_unstitchable = NULL;
		}
		if (_stitch_preview->static_tris) {
			MEM_freeN(_stitch_preview->static_tris);
			_stitch_preview->static_tris = NULL;
		}

		MEM_freeN(_stitch_preview);
		_stitch_preview = NULL;
	}
}


/* "getter method" */
StitchPreviewer *uv_get_stitch_previewer(void)
{
	return _stitch_preview;
}

#define HEADER_LENGTH 256

/* This function updates the header of the UV editor when the stitch tool updates its settings */
static void stitch_update_header(StitchState *stitch_state, bContext *C)
{
	static char str[] = "(S)nap %s, (M)idpoints %s, (L)imit %.2f (Alt Wheel adjust) %s, Switch (I)sland, shift select vertices";

	char msg[HEADER_LENGTH];
	ScrArea *sa = CTX_wm_area(C);

	if (sa) {
		BLI_snprintf(msg, HEADER_LENGTH, str,
		             stitch_state->snap_islands ? "On" : "Off",
		             stitch_state->midpoints    ? "On" : "Off",
		             stitch_state->limit_dist,
		             stitch_state->use_limit    ? "On" : "Off");

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

static void stitch_uv_rotate(float rotation, float medianPoint[2], float uv[2])
{
	float uv_rotation_result[2];

	uv[0] -= medianPoint[0];
	uv[1] -= medianPoint[1];

	uv_rotation_result[0] = cos(rotation) * uv[0] - sin(rotation) * uv[1];
	uv_rotation_result[1] = sin(rotation) * uv[0] + cos(rotation) * uv[1];

	uv[0] = uv_rotation_result[0] + medianPoint[0];
	uv[1] = uv_rotation_result[1] + medianPoint[1];
}

static int stitch_check_uvs_stitchable(UvElement *element, UvElement *element_iter, StitchState *state)
{
	float limit;
	int do_limit;

	if (element_iter == element) {
		return 0;
	}

	limit = state->limit_dist;
	do_limit = state->use_limit;

	if (do_limit) {
		MLoopUV *luv_orig, *luv_iter;
		BMLoop *l_orig, *l_iter;


		l_orig = element->l;
		luv_orig = CustomData_bmesh_get(&state->em->bm->ldata, l_orig->head.data, CD_MLOOPUV);
		l_iter = element_iter->l;
		luv_iter = CustomData_bmesh_get(&state->em->bm->ldata, l_iter->head.data, CD_MLOOPUV);

		if (fabs(luv_orig->uv[0] - luv_iter->uv[0]) < limit &&
		    fabs(luv_orig->uv[1] - luv_iter->uv[1]) < limit)
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


static int stitch_check_uvs_state_stitchable(UvElement *element, UvElement *element_iter, StitchState *state)
{
	if ((state->snap_islands && element->island == element_iter->island) ||
	    (!state->midpoints && element->island == element_iter->island))
	{
		return 0;
	}

	return stitch_check_uvs_stitchable(element, element_iter, state);
}


/* calculate snapping for islands */
static void stitch_calculate_island_snapping(StitchState *state, PreviewPosition *preview_position, StitchPreviewer *preview, IslandStitchData *island_stitch_data, int final)
{
	int i;
	UvElement *element;

	for (i = 0; i <  state->element_map->totalIslands; i++) {
		if (island_stitch_data[i].addedForPreview) {
			int numOfIslandUVs = 0, j;

			/* check to avoid divide by 0 */
			if (island_stitch_data[i].num_rot_elements > 0) {
				island_stitch_data[i].rotation /= island_stitch_data[i].num_rot_elements;
				island_stitch_data[i].medianPoint[0] /= island_stitch_data[i].numOfElements;
				island_stitch_data[i].medianPoint[1] /= island_stitch_data[i].numOfElements;
			}
			island_stitch_data[i].translation[0] /= island_stitch_data[i].numOfElements;
			island_stitch_data[i].translation[1] /= island_stitch_data[i].numOfElements;
			numOfIslandUVs = getNumOfIslandUvs(state->element_map, i);
			element = &state->element_map->buf[state->element_map->islandIndices[i]];
			for (j = 0; j < numOfIslandUVs; j++, element++) {
				/* stitchable uvs have already been processed, don't process */
				if (!(element->flag & STITCH_PROCESSED)) {
					MLoopUV *luv;
					BMLoop *l;

					l = element->l;
					luv = CustomData_bmesh_get(&state->em->bm->ldata, l->head.data, CD_MLOOPUV);

					if (final) {

						stitch_uv_rotate(island_stitch_data[i].rotation, island_stitch_data[i].medianPoint, luv->uv);

						add_v2_v2(luv->uv, island_stitch_data[i].translation);
					}

					else {
						int face_preview_pos = preview_position[BM_elem_index_get(element->face)].data_position;

						stitch_uv_rotate(island_stitch_data[i].rotation, island_stitch_data[i].medianPoint,
						                 preview->preview_polys + face_preview_pos + 2 * element->tfindex);

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
	UvElement *element1, *element2;
	float uv1[2], uv2[2];
	float edgecos, edgesin;
	int index1, index2;
	float rotation;
	MLoopUV *luv1, *luv2;
	BMLoop *l1, *l2;

	element1 = state->uvs[edge->uv1];
	element2 = state->uvs[edge->uv2];

	l1 = element1->l;
	luv1 = CustomData_bmesh_get(&state->em->bm->ldata, l1->head.data, CD_MLOOPUV);
	l2 = element2->l;
	luv2 = CustomData_bmesh_get(&state->em->bm->ldata, l2->head.data, CD_MLOOPUV);

	index1 = uvfinal_map[element1 - state->element_map->buf];
	index2 = uvfinal_map[element2 - state->element_map->buf];

	/* the idea here is to take the directions of the edges and find the rotation between final and initial
	 * direction. This, using inner and outer vector products, gives the angle. Directions are differences so... */
	uv1[0] = luv2->uv[0] - luv1->uv[0];
	uv1[1] = luv2->uv[1] - luv1->uv[1];

	uv2[0] = uv_average[index2].uv[0] - uv_average[index1].uv[0];
	uv2[1] = uv_average[index2].uv[1] - uv_average[index1].uv[1];

	normalize_v2(uv1);
	normalize_v2(uv2);

	edgecos = uv1[0] * uv2[0] + uv1[1] * uv2[1];
	edgesin = uv1[0] * uv2[1] - uv2[0] * uv1[1];

	rotation = (edgesin > 0) ? acos(MAX2(-1.0, MIN2(1.0, edgecos))) : -acos(MAX2(-1.0, MIN2(1.0, edgecos)));

	island_stitch_data[element1->island].num_rot_elements++;
	island_stitch_data[element1->island].rotation += rotation;
}


static void stitch_island_calculate_vert_rotation(UvElement *element, StitchState *state, IslandStitchData *island_stitch_data)
{
	float edgecos = 1, edgesin = 0;
	int index;
	UvElement *element_iter;
	float rotation = 0;
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
			/* easily possible*/

			index_tmp1 = element_iter - state->element_map->buf;
			index_tmp1 = state->map[index_tmp1];
			index_tmp2 = element - state->element_map->buf;
			index_tmp2 = state->map[index_tmp2];

			negate_v2_v2(normal, state->normals + index_tmp2 * 2);
			edgecos = dot_v2v2(normal, state->normals + index_tmp1 * 2);
			edgesin = cross_v2v2(normal, state->normals + index_tmp1 * 2);
			rotation += (edgesin > 0) ? acos(edgecos) : -acos(edgecos);
		}
	}

	if (state->midpoints)
		rotation /= 2.0;
	island_stitch_data[element->island].num_rot_elements++;
	island_stitch_data[element->island].rotation += rotation;
}


static void stitch_state_delete(StitchState *stitch_state)
{
	if (stitch_state) {
		if (stitch_state->element_map) {
			EDBM_uv_element_map_free(stitch_state->element_map);
		}
		if (stitch_state->uvs) {
			MEM_freeN(stitch_state->uvs);
		}
		if (stitch_state->selection_stack) {
			MEM_freeN(stitch_state->selection_stack);
		}
		if (stitch_state->tris_per_island) {
			MEM_freeN(stitch_state->tris_per_island);
		}
		if (stitch_state->map) {
			MEM_freeN(stitch_state->map);
		}
		if (stitch_state->normals) {
			MEM_freeN(stitch_state->normals);
		}
		if (stitch_state->edges) {
			MEM_freeN(stitch_state->edges);
		}
		MEM_freeN(stitch_state);
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
			if (element_iter == element) {
				continue;
			}
			if (stitch_check_uvs_stitchable(element, element_iter, state)) {
				island_stitch_data[element_iter->island].stitchableCandidate = 1;
				island_stitch_data[element->island].stitchableCandidate = 1;
				element->flag |= STITCH_STITCHABLE_CANDIDATE;
			}
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
                                                   PreviewPosition *preview_position) {
	StitchPreviewer *preview = uv_get_stitch_previewer();

	/* static island does not change so returning immediately */
	if (state->snap_islands && !state->midpoints && state->static_island == element->island)
		return;

	if (state->snap_islands) {
		island_stitch_data[element->island].addedForPreview = 1;
	}

	do {
		stitch_set_face_preview_buffer_position(element->face, preview, preview_position);
		element = element->next;
	} while (element && !element->separate);
}


/* checks if uvs are indeed stitchable and registers so that they can be shown in preview */
static void stitch_validate_stichability(UvElement *element, StitchState *state, IslandStitchData *island_stitch_data,
                                         PreviewPosition *preview_position) {
	UvElement *element_iter;
	StitchPreviewer *preview;
	int vert_index;
	BMLoop *l;

	l = element->l;

	vert_index = BM_elem_index_get(l->v);

	preview = uv_get_stitch_previewer();
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

/* main processing function. It calculates preview and final positions. */
static int stitch_process_data(StitchState *state, Scene *scene, int final)
{
	int i;
	StitchPreviewer *preview;
	IslandStitchData *island_stitch_data = NULL;
	int previous_island = state->static_island;
	BMFace *efa;
	BMIter iter;
	UVVertAverage *final_position;
	char stitch_midpoints = state->midpoints;
	/* used to map uv indices to uvaverage indices for selection */
	unsigned int *uvfinal_map;
	/* per face preview position in preview buffer */
	PreviewPosition *preview_position;

	/* cleanup previous preview */
	stitch_preview_delete();
	preview = stitch_preview_init();
	if (preview == NULL)
		return 0;

	preview_position = MEM_mallocN(state->em->bm->totface * sizeof(*preview_position), "stitch_face_preview_position");
	/* each face holds its position in the preview buffer in tmp. -1 is uninitialized */
	for (i = 0; i < state->em->bm->totface; i++) {
		preview_position[i].data_position = STITCH_NO_PREVIEW;
	}

	island_stitch_data = MEM_callocN(sizeof(*island_stitch_data) * state->element_map->totalIslands, "stitch_island_data");
	if (!island_stitch_data) {
		return 0;
	}

	/* store indices to editVerts and Faces. May be unneeded but ensuring anyway */
	BM_mesh_elem_index_ensure(state->em->bm, BM_VERT | BM_FACE);

	/*****************************************
	 *  First determine stitchability of uvs *
	 *****************************************/

	for (i = 0; i < state->selection_size; i++) {
		UvElement *element = state->selection_stack[i];
		determine_uv_stitchability(element, state, island_stitch_data);
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
		UvElement *element = state->selection_stack[i];
		if (element->flag & STITCH_STITCHABLE_CANDIDATE) {
			element->flag &= ~STITCH_STITCHABLE_CANDIDATE;
			stitch_validate_stichability(element, state, island_stitch_data, preview_position);
		}
		else {
			/* add to preview for unstitchable */
			preview->num_unstitchable++;
		}
	}

	/*****************************************
	 *  Setup preview for stitchable islands *
	 *****************************************/
	if (state->snap_islands) {
		for (i = 0; i <  state->element_map->totalIslands; i++) {
			if (island_stitch_data[i].addedForPreview) {
				int numOfIslandUVs = 0, j;
				UvElement *element;
				numOfIslandUVs = getNumOfIslandUvs(state->element_map, i);
				element = &state->element_map->buf[state->element_map->islandIndices[i]];
				for (j = 0; j < numOfIslandUVs; j++, element++) {
					stitch_set_face_preview_buffer_position(element->face, preview, preview_position);
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
		/* initialize the preview buffers */
		preview->preview_polys = (float *)MEM_mallocN(preview->preview_uvs * sizeof(float) * 2, "tri_uv_stitch_prev");
		preview->uvs_per_polygon = MEM_mallocN(preview->num_polys * sizeof(*preview->uvs_per_polygon), "tri_uv_stitch_prev");
		preview->preview_stitchable = (float *)MEM_mallocN(preview->num_stitchable * sizeof(float) * 2, "stitch_preview_stichable_data");
		preview->preview_unstitchable = (float *)MEM_mallocN(preview->num_unstitchable * sizeof(float) * 2, "stitch_preview_unstichable_data");

		preview->static_tris = (float *)MEM_mallocN(state->tris_per_island[state->static_island] * sizeof(float) * 6, "static_island_preview_tris");

		preview->num_static_tris = state->tris_per_island[state->static_island];
		/* will cause cancel and freeing of all data structures so OK */
		if (!preview->preview_polys || !preview->preview_stitchable || !preview->preview_unstitchable) {
			return 0;
		}

		/* copy data from MTFaces to the preview display buffers */
		BM_ITER_MESH (efa, &iter, state->em->bm, BM_FACES_OF_MESH) {
			/* just to test if face was added for processing. uvs of inselected vertices will return NULL */
			UvElement *element = ED_uv_element_get(state->element_map, efa, BM_FACE_FIRST_LOOP(efa));

			if (element) {
				int numoftris = efa->len - 2;
				int index = BM_elem_index_get(efa);
				int face_preview_pos = preview_position[index].data_position;
				if (face_preview_pos != STITCH_NO_PREVIEW) {
					preview->uvs_per_polygon[preview_position[index].polycount_position] = efa->len;
					BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
						luv = CustomData_bmesh_get(&state->em->bm->ldata, l->head.data, CD_MLOOPUV);
						copy_v2_v2(preview->preview_polys + face_preview_pos + i * 2, luv->uv);
					}
				}

				if (element->island == state->static_island) {
					BMLoop *fl = BM_FACE_FIRST_LOOP(efa);
					MLoopUV *fuv = CustomData_bmesh_get(&state->em->bm->ldata, fl->head.data, CD_MLOOPUV);

					BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
						if (i < numoftris) {
							/* using next since the first uv is already accounted for */
							BMLoop *lnext = l->next;
							MLoopUV *luvnext = CustomData_bmesh_get(&state->em->bm->ldata, lnext->next->head.data, CD_MLOOPUV);
							luv = CustomData_bmesh_get(&state->em->bm->ldata, lnext->head.data, CD_MLOOPUV);

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
		for (i = 0; i < state->total_separate_uvs; i++) {
			UvElement *element = (UvElement *)state->uvs[i];
			if (element->flag & STITCH_STITCHABLE) {
				l = element->l;
				luv = CustomData_bmesh_get(&state->em->bm->ldata, l->head.data, CD_MLOOPUV);

				copy_v2_v2(&preview->preview_stitchable[stitchBufferIndex * 2], luv->uv);

				stitchBufferIndex++;
			}
			else if (element->flag & STITCH_SELECTED) {
				l = element->l;
				luv = CustomData_bmesh_get(&state->em->bm->ldata, l->head.data, CD_MLOOPUV);

				copy_v2_v2(&preview->preview_unstitchable[unstitchBufferIndex * 2], luv->uv);
				unstitchBufferIndex++;
			}
		}
	}

	/******************************************************
	 * Here we calculate the final coordinates of the uvs *
	 ******************************************************/

	final_position = MEM_callocN(state->selection_size * sizeof(*final_position), "stitch_uv_average");
	uvfinal_map = MEM_mallocN(state->element_map->totalUVs * sizeof(*uvfinal_map), "stitch_uv_final_map");

	/* first pass, calculate final position for stitchable uvs of the static island */
	for (i = 0; i < state->selection_size; i++) {
		UvElement *element = state->selection_stack[i];
		if (element->flag & STITCH_STITCHABLE) {
			BMLoop *l;
			MLoopUV *luv;
			UvElement *element_iter;

			l = element->l;
			luv = CustomData_bmesh_get(&state->em->bm->ldata, l->head.data, CD_MLOOPUV);


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
						luv = CustomData_bmesh_get(&state->em->bm->ldata, l->head.data, CD_MLOOPUV);
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

	/* second pass, calculate island rotation and translation before modifying any uvs */
	if (state->snap_islands) {
		for (i = 0; i < state->selection_size; i++) {
			UvElement *element = state->selection_stack[i];
			if (element->flag & STITCH_STITCHABLE) {
				BMLoop *l;
				MLoopUV *luv;

				l = element->l;
				luv = CustomData_bmesh_get(&state->em->bm->ldata, l->head.data, CD_MLOOPUV);

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
		for (i = 0; i < state->total_boundary_edges; i++) {
			UvEdge *edge = state->edges + i;
			if ((state->uvs[edge->uv1]->flag & STITCH_STITCHABLE) && (state->uvs[edge->uv2]->flag & STITCH_STITCHABLE)) {
				stitch_island_calculate_edge_rotation(edge, state, final_position, uvfinal_map, island_stitch_data);
				island_stitch_data[state->uvs[edge->uv1]->island].use_edge_rotation = 1;
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

	/* third pass, propagate changes to coincident uvs */
	for (i = 0; i < state->selection_size; i++) {
		UvElement *element = state->selection_stack[i];
		if (element->flag & STITCH_STITCHABLE) {
			UvElement *element_iter = element;
			/* propagate to coincident uvs */
			do {
				BMLoop *l;
				MLoopUV *luv;

				l = element_iter->l;
				luv = CustomData_bmesh_get(&state->em->bm->ldata, l->head.data, CD_MLOOPUV);

				element_iter->flag |= STITCH_PROCESSED;
				/* either flush to preview or to the MTFace, if final */
				if (final) {
					copy_v2_v2(luv->uv, final_position[i].uv);

					uvedit_uv_select_enable(state->em, scene, l, FALSE);
				}
				else {
					int face_preview_pos = preview_position[BM_elem_index_get(element_iter->face)].data_position;
					if (face_preview_pos != STITCH_NO_PREVIEW) {
						copy_v2_v2(preview->preview_polys + face_preview_pos + 2 * element_iter->tfindex,
						           final_position[i].uv);
					}
				}

				/* end of calculations, keep only the selection flag */
				if ( (!state->snap_islands) || ((!stitch_midpoints) && (element_iter->island == state->static_island))) {
					element_iter->flag &= STITCH_SELECTED;
				}

				element_iter = element_iter->next;
			} while (element_iter && !element_iter->separate);
		}
	}

	/* final pass, calculate Island translation/rotation if needed */
	if (state->snap_islands) {
		stitch_calculate_island_snapping(state, preview_position, preview, island_stitch_data, final);
	}

	MEM_freeN(final_position);
	MEM_freeN(uvfinal_map);
	MEM_freeN(island_stitch_data);
	MEM_freeN(preview_position);

	return 1;
}

/* Stitch hash initialization functions */
static unsigned int uv_edge_hash(const void *key)
{
	UvEdge *edge = (UvEdge *)key;
	return
	    BLI_ghashutil_inthash(SET_INT_IN_POINTER(edge->uv2)) +
	    BLI_ghashutil_inthash(SET_INT_IN_POINTER(edge->uv1));
}

static int uv_edge_compare(const void *a, const void *b)
{
	UvEdge *edge1 = (UvEdge *)a;
	UvEdge *edge2 = (UvEdge *)b;

	if ((edge1->uv1 == edge2->uv1) && (edge1->uv2 == edge2->uv2)) {
		return 0;
	}
	return 1;
}


/* Select all common uvs */
static void stitch_select_uv(UvElement *element, StitchState *state, int always_select)
{
	BMLoop *l;
	UvElement *element_iter;
	UvElement **selection_stack = state->selection_stack;

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

static void stitch_calculate_edge_normal(BMEditMesh *em, UvEdge *edge, float *normal)
{
	BMLoop *l1 = edge->element->l;
	BMLoop *l2 = l1->next;
	MLoopUV *luv1, *luv2;
	float tangent[2];

	luv1 = CustomData_bmesh_get(&em->bm->ldata, l1->head.data, CD_MLOOPUV);
	luv2 = CustomData_bmesh_get(&em->bm->ldata, l2->head.data, CD_MLOOPUV);

	sub_v2_v2v2(tangent, luv2->uv,  luv1->uv);

	normal[0] = tangent[1];
	normal[1] = -tangent[0];

	normalize_v2(normal);
}

static int stitch_init(bContext *C, wmOperator *op)
{
	/* for fast edge lookup... */
	GHash *edgeHash;
	/* ...and actual edge storage */
	UvEdge *edges;
	int total_edges;
	/* maps uvelements to their first coincident uv */
	int *map;
	int counter = 0, i;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	BMEditMesh *em;
	GHashIterator *ghi;
	UvEdge *all_edges;
	StitchState *state = MEM_mallocN(sizeof(StitchState), "stitch state");
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = scene->toolsettings;

	Object *obedit = CTX_data_edit_object(C);

	op->customdata = state;

	if (!state)
		return 0;

	/* initialize state */
	state->use_limit = RNA_boolean_get(op->ptr, "use_limit");
	state->limit_dist = RNA_float_get(op->ptr, "limit");
	state->em = em = BMEdit_FromObject(obedit);
	state->snap_islands = RNA_boolean_get(op->ptr, "snap_islands");
	state->static_island = RNA_int_get(op->ptr, "static_island");
	state->midpoints = RNA_boolean_get(op->ptr, "midpoint_snap");
	/* in uv synch selection, all uv's are visible */
	if (ts->uv_flag & UV_SYNC_SELECTION) {
		state->element_map = EDBM_uv_element_map_create(state->em, 0, 1);
	}
	else {
		state->element_map = EDBM_uv_element_map_create(state->em, 1, 1);
	}
	if (!state->element_map) {
		stitch_state_delete(state);
		return 0;
	}

	/* Entirely possible if redoing last operator that static island is bigger than total number of islands.
	 * This ensures we get no hang in the island checking code in stitch_process_data. */
	state->static_island %= state->element_map->totalIslands;

	/* Count 'unique' uvs */
	for (i = 0; i < state->element_map->totalUVs; i++) {
		if (state->element_map->buf[i].separate) {
			counter++;
		}
	}

	/* Allocate the unique uv buffers */
	state->uvs = MEM_mallocN(sizeof(*state->uvs) * counter, "uv_stitch_unique_uvs");
	/* internal uvs need no normals but it is hard and slow to keep a map of
	 * normals only for boundary uvs, so allocating for all uvs */
	state->normals = MEM_callocN(sizeof(*state->normals) * counter * 2, "uv_stitch_normals");
	state->total_separate_uvs = counter;
	/* we can at most have totalUVs edges or uvs selected. Actually they are less, considering we store only
	 * unique uvs for processing but I am accounting for all bizarre cases, especially for edges, this way */
	state->selection_stack = MEM_mallocN(sizeof(*state->selection_stack) * counter, "uv_stitch_selection_stack");
	state->map = map = MEM_mallocN(sizeof(*map) * state->element_map->totalUVs, "uv_stitch_unique_map");
	/* Allocate the edge stack */
	edgeHash = BLI_ghash_new(uv_edge_hash, uv_edge_compare, "stitch_edge_hash");
	all_edges = MEM_mallocN(sizeof(*all_edges) * state->element_map->totalUVs, "stitch_all_edges");

	if (!state->selection_stack || !state->uvs || !map || !edgeHash || !all_edges) {
		stitch_state_delete(state);
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
			UvElement *element = ED_uv_element_get(state->element_map, efa, l);
			int offset1, itmp1 = element - state->element_map->buf;
			int offset2, itmp2 = ED_uv_element_get(state->element_map, efa, l->next) - state->element_map->buf;

			offset1 = map[itmp1];
			offset2 = map[itmp2];

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

			if (BLI_ghash_haskey(edgeHash, &all_edges[counter])) {
				char *flag = BLI_ghash_lookup(edgeHash, &all_edges[counter]);
				*flag = 0;
			}
			else {
				BLI_ghash_insert(edgeHash, &all_edges[counter], &(all_edges[counter].flag));
				all_edges[counter].flag = STITCH_BOUNDARY;
			}
			counter++;
		}
	}


	ghi = BLI_ghashIterator_new(edgeHash);
	total_edges = 0;
	/* fill the edges with data */
	for (; !BLI_ghashIterator_isDone(ghi); BLI_ghashIterator_step(ghi)) {
		UvEdge *edge = ((UvEdge *)BLI_ghashIterator_getKey(ghi));
		if (edge->flag & STITCH_BOUNDARY) {
			total_edges++;
		}
	}
	state->edges = edges = MEM_mallocN(sizeof(*edges) * total_edges, "stitch_edges");
	if (!ghi || !edges) {
		MEM_freeN(all_edges);
		stitch_state_delete(state);
		return 0;
	}

	state->total_boundary_edges = total_edges;

	/* fill the edges with data */
	for (i = 0, BLI_ghashIterator_init(ghi, edgeHash); !BLI_ghashIterator_isDone(ghi); BLI_ghashIterator_step(ghi)) {
		UvEdge *edge = ((UvEdge *)BLI_ghashIterator_getKey(ghi));
		if (edge->flag & STITCH_BOUNDARY) {
			edges[i++] = *((UvEdge *)BLI_ghashIterator_getKey(ghi));
		}
	}

	/* cleanup temporary stuff */
	BLI_ghashIterator_free(ghi);
	MEM_freeN(all_edges);

	/* refill hash with new pointers to cleanup duplicates */
	BLI_ghash_free(edgeHash, NULL, NULL);

	/***** calculate 2D normals for boundary uvs *****/

	/* we use boundary edges to calculate 2D normals.
	 * to disambiguate the direction of the normal, we also need
	 * a point "inside" the island, that can be provided by
	 * the opposite uv for a quad, or the next uv for a triangle. */

	for (i = 0; i < total_edges; i++) {
		float normal[2];
		stitch_calculate_edge_normal(em, edges + i, normal);

		add_v2_v2(state->normals + edges[i].uv1 * 2, normal);
		add_v2_v2(state->normals + edges[i].uv2 * 2, normal);

		normalize_v2(state->normals + edges[i].uv1 * 2);
		normalize_v2(state->normals + edges[i].uv2 * 2);
	}


	/***** fill selection stack *******/

	state->selection_size = 0;

	/* Load old selection if redoing operator with different settings */
	if (RNA_struct_property_is_set(op->ptr, "selection")) {
		int faceIndex, elementIndex;
		UvElement *element;

		EDBM_index_arrays_init(em, 0, 0, 1);

		RNA_BEGIN (op->ptr, itemptr, "selection") {
			faceIndex = RNA_int_get(&itemptr, "face_index");
			elementIndex = RNA_int_get(&itemptr, "element_index");
			efa = EDBM_face_at_index(em, faceIndex);
			element = ED_uv_element_get(state->element_map, efa, BM_iter_at_index(NULL, BM_LOOPS_OF_FACE, efa, elementIndex));
			stitch_select_uv(element, state, 1);
		}
		RNA_END;

		EDBM_index_arrays_free(em);
		/* Clear the selection */
		RNA_collection_clear(op->ptr, "selection");

	}
	else {
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			i = 0;
			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				if (uvedit_uv_select_test(em, scene, l)) {
					UvElement *element = ED_uv_element_get(state->element_map, efa, l);
					stitch_select_uv(element, state, 1);
				}
				i++;
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
		UvElement *element = ED_uv_element_get(state->element_map, efa, BM_FACE_FIRST_LOOP(efa));

		if (element) {
			state->tris_per_island[element->island] += (efa->len > 2) ? efa->len - 2 : 0;
		}
	}

	if (!stitch_process_data(state, scene, 0)) {
		stitch_state_delete(state);
		return 0;
	}

	stitch_update_header(state, C);
	return 1;
}

static int stitch_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
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
	StitchState *stitch_state;
	Scene *scene;
	SpaceImage *sima;
	ScrArea *sa = CTX_wm_area(C);
	Object *obedit;

	scene = CTX_data_scene(C);
	obedit = CTX_data_edit_object(C);
	sima = CTX_wm_space_image(C);

	stitch_state = (StitchState *)op->customdata;

	if (finished) {
		int i;

		RNA_float_set(op->ptr, "limit", stitch_state->limit_dist);
		RNA_boolean_set(op->ptr, "use_limit", stitch_state->use_limit);
		RNA_boolean_set(op->ptr, "snap_islands", stitch_state->snap_islands);
		RNA_int_set(op->ptr, "static_island", stitch_state->static_island);
		RNA_boolean_set(op->ptr, "midpoint_snap", stitch_state->midpoints);

		/* Store selection for re-execution of stitch */
		for (i = 0; i < stitch_state->selection_size; i++) {
			PointerRNA itemptr;
			UvElement *element = stitch_state->selection_stack[i];

			RNA_collection_add(op->ptr, "selection", &itemptr);

			RNA_int_set(&itemptr, "face_index", BM_elem_index_get(element->face));

			RNA_int_set(&itemptr, "element_index", element->tfindex);
		}


		uvedit_live_unwrap_update(sima, scene, obedit);
	}

	if (sa)
		ED_area_headerprint(sa, NULL);

	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);

	stitch_state_delete(stitch_state);
	op->customdata = NULL;

	stitch_preview_delete();
}


static int stitch_cancel(bContext *C, wmOperator *op)
{
	stitch_exit(C, op, 0);
	return OPERATOR_CANCELLED;
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
		return stitch_cancel(C, op);
	}
}

static void stitch_select(bContext *C, Scene *scene, wmEvent *event, StitchState *stitch_state)
{
	/* add uv under mouse to processed uv's */
	float co[2];
	NearestHit hit;
	ARegion *ar = CTX_wm_region(C);
	Image *ima = CTX_data_edit_image(C);

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &co[0], &co[1]);
	uv_find_nearest_vert(scene, ima, stitch_state->em, co, NULL, &hit);

	if (hit.efa) {
		/* Add vertex to selection, deselect all common uv's of vert other
		 * than selected and update the preview. This behavior was decided so that
		 * you can do stuff like deselect the opposite stitchable vertex and the initial still gets deselected */

		/* This works due to setting of tmp in find nearest uv vert */
		UvElement *element = ED_uv_element_get(stitch_state->element_map, hit.efa, hit.l);
		stitch_select_uv(element, stitch_state, 0);

	}
}

static int stitch_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	StitchState *stitch_state;
	Scene *scene = CTX_data_scene(C);

	stitch_state = (StitchState *)op->customdata;

	switch (event->type) {
		case MIDDLEMOUSE:
			return OPERATOR_PASS_THROUGH;

		/* Cancel */
		case ESCKEY:
			return stitch_cancel(C, op);


		case LEFTMOUSE:
			if (event->shift && (U.flag & USER_LMOUSESELECT)) {
				if (event->val == KM_RELEASE) {
					stitch_select(C, scene, event, stitch_state);

					if (!stitch_process_data(stitch_state, scene, 0)) {
						return stitch_cancel(C, op);
					}
				}
				break;
			}
		case PADENTER:
		case RETKEY:
			if (stitch_process_data(stitch_state, scene, 1)) {
				stitch_exit(C, op, 1);
				return OPERATOR_FINISHED;
			}
			else {
				return stitch_cancel(C, op);
			}

		/* Increase limit */
		case PADPLUSKEY:
		case WHEELUPMOUSE:
			if (event->alt) {
				stitch_state->limit_dist += 0.01;
				if (!stitch_process_data(stitch_state, scene, 0)) {
					return stitch_cancel(C, op);
				}
				break;
			}
			else {
				return OPERATOR_PASS_THROUGH;
			}
		/* Decrease limit */
		case PADMINUS:
		case WHEELDOWNMOUSE:
			if (event->alt) {
				stitch_state->limit_dist -= 0.01;
				stitch_state->limit_dist = MAX2(0.01, stitch_state->limit_dist);
				if (!stitch_process_data(stitch_state, scene, 0)) {
					return stitch_cancel(C, op);
				}
				break;
			}
			else {
				return OPERATOR_PASS_THROUGH;
			}

		/* Use Limit (Default off)*/
		case LKEY:
			if (event->val == KM_PRESS) {
				stitch_state->use_limit = !stitch_state->use_limit;
				if (!stitch_process_data(stitch_state, scene, 0)) {
					return stitch_cancel(C, op);
				}
				break;
			}
			return OPERATOR_RUNNING_MODAL;

		case IKEY:
			if (event->val == KM_PRESS) {
				stitch_state->static_island++;
				stitch_state->static_island %= stitch_state->element_map->totalIslands;

				if (!stitch_process_data(stitch_state, scene, 0)) {
					return stitch_cancel(C, op);
				}
				break;
			}
			return OPERATOR_RUNNING_MODAL;

		case MKEY:
			if (event->val == KM_PRESS) {
				stitch_state->midpoints = !stitch_state->midpoints;
				if (!stitch_process_data(stitch_state, scene, 0)) {
					return stitch_cancel(C, op);
				}
			}
			break;

		/* Select geometry*/
		case RIGHTMOUSE:
			if (!event->shift) {
				return stitch_cancel(C, op);
			}
			if (event->val == KM_RELEASE && !(U.flag & USER_LMOUSESELECT)) {
				stitch_select(C, scene, event, stitch_state);

				if (!stitch_process_data(stitch_state, scene, 0)) {
					return stitch_cancel(C, op);
				}
				break;
			}
			return OPERATOR_RUNNING_MODAL;

		/* snap islands on/off */
		case SKEY:
			if (event->val == KM_PRESS) {
				stitch_state->snap_islands = !stitch_state->snap_islands;
				if (!stitch_process_data(stitch_state, scene, 0)) {
					return stitch_cancel(C, op);
				}
				break;
			}
			else {
				return OPERATOR_RUNNING_MODAL;
			}

		default:
			return OPERATOR_RUNNING_MODAL;
	}

	/* if updated settings, renew feedback message */
	stitch_update_header(stitch_state, C);
	ED_region_tag_redraw(CTX_wm_region(C));
	return OPERATOR_RUNNING_MODAL;
}

void UV_OT_stitch(wmOperatorType *ot)
{
	PropertyRNA *prop;

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
	prop = RNA_def_collection_runtime(ot->srna, "selection", &RNA_SelectedUvElement, "Selection", "");
	/* Selection should not be editable or viewed in toolbar */
	RNA_def_property_flag(prop, PROP_HIDDEN);
}
