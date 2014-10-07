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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jason Wilkins, Tom Musgrove.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Implements the Sculpt Mode tools
 *
 */

/** \file blender/editors/sculpt_paint/sculpt.c
 *  \ingroup edsculpt
 */


#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_dial.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_threads.h"

#include "BLF_translation.h"

#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_brush_types.h"

#include "BKE_pbvh.h"
#include "BKE_brush.h"
#include "BKE_ccg.h"
#include "BKE_context.h"
#include "BKE_crazyspace.h"
#include "BKE_depsgraph.h"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_subsurf.h"
#include "BKE_colortools.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_sculpt.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_view3d.h"
#include "paint_intern.h"
#include "sculpt_intern.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "RE_render_ext.h"

#include "GPU_buffers.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#if defined(__APPLE__) && defined _OPENMP
#include <sys/sysctl.h>

/* Query how many cores not counting HT aka physical cores we've got. */
static int system_physical_thread_count(void)
{
	int pcount;
	size_t pcount_len = sizeof(pcount);
	sysctlbyname("hw.physicalcpu", &pcount, &pcount_len, NULL, 0);
	return pcount;
}
#endif  /* __APPLE__ */

void ED_sculpt_stroke_get_average(Object *ob, float stroke[3])
{
	if (ob->sculpt->last_stroke_valid && ob->sculpt->average_stroke_counter > 0) {
		float fac = 1.0f / ob->sculpt->average_stroke_counter;
		mul_v3_v3fl(stroke, ob->sculpt->average_stroke_accum, fac);
	}
	else {
		copy_v3_v3(stroke, ob->obmat[3]);
	}
}

bool ED_sculpt_minmax(bContext *C, float min[3], float max[3])
{
	Object *ob = CTX_data_active_object(C);

	if (ob && ob->sculpt && ob->sculpt->last_stroke_valid) {
		copy_v3_v3(min, ob->sculpt->last_stroke);
		copy_v3_v3(max, ob->sculpt->last_stroke);

		return 1;
	}
	else {
		return 0;
	}
}


/* Check if there are any active modifiers in stack (used for flushing updates at enter/exit sculpt mode) */
static bool sculpt_has_active_modifiers(Scene *scene, Object *ob)
{
	ModifierData *md;
	VirtualModifierData virtualModifierData;

	md = modifiers_getVirtualModifierList(ob, &virtualModifierData);

	/* exception for shape keys because we can edit those */
	for (; md; md = md->next) {
		if (modifier_isEnabled(scene, md, eModifierMode_Realtime))
			return 1;
	}

	return 0;
}



typedef enum StrokeFlags {
	CLIP_X = 1,
	CLIP_Y = 2,
	CLIP_Z = 4
} StrokeFlags;

/* Cache stroke properties. Used because
 * RNA property lookup isn't particularly fast.
 *
 * For descriptions of these settings, check the operator properties.
 */
typedef struct StrokeCache {
	/* Invariants */
	float initial_radius;
	float scale[3];
	int flag;
	float clip_tolerance[3];
	float initial_mouse[2];

	/* Pre-allocated temporary storage used during smoothing */
	int num_threads, init_num_threads;
	float (**tmpgrid_co)[3], (**tmprow_co)[3];
	float **tmpgrid_mask, **tmprow_mask;

	/* Variants */
	float radius;
	float radius_squared;
	float true_location[3];
	float location[3];

	bool pen_flip;
	bool invert;
	float pressure;
	float mouse[2];
	float bstrength;

	/* The rest is temporary storage that isn't saved as a property */

	bool first_time; /* Beginning of stroke may do some things special */

	/* from ED_view3d_ob_project_mat_get() */
	float projection_mat[4][4];

	/* Clean this up! */
	ViewContext *vc;
	Brush *brush;

	float (*face_norms)[3]; /* Copy of the mesh faces' normals */

	float special_rotation;
	float grab_delta[3], grab_delta_symmetry[3];
	float old_grab_location[3], orig_grab_location[3];

	int symmetry; /* Symmetry index between 0 and 7 bit combo 0 is Brush only;
	               * 1 is X mirror; 2 is Y mirror; 3 is XY; 4 is Z; 5 is XZ; 6 is YZ; 7 is XYZ */
	int mirror_symmetry_pass; /* the symmetry pass we are currently on between 0 and 7*/
	float true_view_normal[3];
	float view_normal[3];

	/* sculpt_normal gets calculated by calc_sculpt_normal(), then the
	 * sculpt_normal_symm gets updated quickly with the usual symmetry
	 * transforms */
	float sculpt_normal[3];
	float sculpt_normal_symm[3];

	/* Used for area texture mode, local_mat gets calculated by
	 * calc_brush_local_mat() and used in tex_strength(). */
	float brush_local_mat[4][4];
	
	float last_center[3];
	int radial_symmetry_pass;
	float symm_rot_mat[4][4];
	float symm_rot_mat_inv[4][4];
	bool original;
	float anchored_location[3];

	float vertex_rotation; /* amount to rotate the vertices when using rotate brush */
	Dial *dial;
	
	char saved_active_brush_name[MAX_ID_NAME];
	char saved_mask_brush_tool;
	int saved_smooth_size; /* smooth tool copies the size of the current tool */
	bool alt_smooth;

	float plane_trim_squared;

	bool supports_gravity;
	float true_gravity_direction[3];
	float gravity_direction[3];

	rcti previous_r; /* previous redraw rectangle */
	rcti current_r; /* current redraw rectangle */
} StrokeCache;

/************** Access to original unmodified vertex data *************/

typedef struct {
	BMLog *bm_log;

	SculptUndoNode *unode;
	float (*coords)[3];
	short (*normals)[3];
	const float *vmasks;

	/* Original coordinate, normal, and mask */
	const float *co;
	float mask;
	const short *no;
} SculptOrigVertData;


/* Initialize a SculptOrigVertData for accessing original vertex data;
 * handles BMesh, mesh, and multires */
static void sculpt_orig_vert_data_unode_init(SculptOrigVertData *data,
                                             Object *ob,
                                             SculptUndoNode *unode)
{
	SculptSession *ss = ob->sculpt;
	BMesh *bm = ss->bm;

	memset(data, 0, sizeof(*data));
	data->unode = unode;

	if (bm) {
		data->bm_log = ss->bm_log;
	}
	else {
		data->coords = data->unode->co;
		data->normals = data->unode->no;
		data->vmasks = data->unode->mask;
	}
}

/* Initialize a SculptOrigVertData for accessing original vertex data;
 * handles BMesh, mesh, and multires */
static void sculpt_orig_vert_data_init(SculptOrigVertData *data,
                                       Object *ob,
                                       PBVHNode *node)
{
	SculptUndoNode *unode;
	unode = sculpt_undo_push_node(ob, node, SCULPT_UNDO_COORDS);
	sculpt_orig_vert_data_unode_init(data, ob, unode);
}

/* Update a SculptOrigVertData for a particular vertex from the PBVH
 * iterator */
static void sculpt_orig_vert_data_update(SculptOrigVertData *orig_data,
                                         PBVHVertexIter *iter)
{
	if (orig_data->unode->type == SCULPT_UNDO_COORDS) {
		if (orig_data->coords) {
			orig_data->co = orig_data->coords[iter->i];
		}
		else {
			orig_data->co = BM_log_original_vert_co(orig_data->bm_log, iter->bm_vert);
		}

		if (orig_data->normals) {
			orig_data->no = orig_data->normals[iter->i];
		}
		else {
			orig_data->no = BM_log_original_vert_no(orig_data->bm_log, iter->bm_vert);
		}
	}
	else if (orig_data->unode->type == SCULPT_UNDO_MASK) {
		if (orig_data->vmasks) {
			orig_data->mask = orig_data->vmasks[iter->i];
		}
		else {
			orig_data->mask = BM_log_original_mask(orig_data->bm_log, iter->bm_vert);
		}
	}
}

/**********************************************************************/

/* Returns true if the stroke will use dynamic topology, false
 * otherwise.
 *
 * Factors: some brushes like grab cannot do dynamic topology.
 * Others, like smooth, are better without. Same goes for alt-
 * key smoothing. */
static int sculpt_stroke_dynamic_topology(const SculptSession *ss,
                                          const Brush *brush)
{
	return ((BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) &&

	        (!ss->cache || (!ss->cache->alt_smooth)) &&

	        /* Requires mesh restore, which doesn't work with
	         * dynamic-topology */
	        !(brush->flag & BRUSH_ANCHORED) &&
	        !(brush->flag & BRUSH_DRAG_DOT) &&
        
	        (!ELEM(brush->sculpt_tool,
	               /* These brushes, as currently coded, cannot
	                * support dynamic topology */
	               SCULPT_TOOL_GRAB,
	               SCULPT_TOOL_ROTATE,
	               SCULPT_TOOL_THUMB,
	               SCULPT_TOOL_LAYER,

	               /* These brushes could handle dynamic topology,
	                * but user feedback indicates it's better not
	                * to */
	               SCULPT_TOOL_SMOOTH,
	               SCULPT_TOOL_MASK)));
}

/*** paint mesh ***/

static void paint_mesh_restore_co(Sculpt *sd, Object *ob)
{
	SculptSession *ss = ob->sculpt;
	StrokeCache *cache = ss->cache;
	const Brush *brush = BKE_paint_brush(&sd->paint);
	int i;

	PBVHNode **nodes;
	int n, totnode;

#ifndef _OPENMP
	(void)sd; /* quied unused warning */
#endif

	BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);

	/* Disable OpenMP when dynamic-topology is enabled. Otherwise, new
	 * entries might be inserted by sculpt_undo_push_node() into the
	 * GHash used internally by BM_log_original_vert_co() by a
	 * different thread. [#33787] */
#pragma omp parallel for schedule(guided) if ((sd->flags & SCULPT_USE_OPENMP) && !ss->bm && totnode > SCULPT_OMP_LIMIT)
	for (n = 0; n < totnode; n++) {
		SculptUndoNode *unode;
		SculptUndoType type = (brush->sculpt_tool == SCULPT_TOOL_MASK ?
		                       SCULPT_UNDO_MASK : SCULPT_UNDO_COORDS);

		if (ss->bm) {
			unode = sculpt_undo_push_node(ob, nodes[n], type);
		}
		else {
			unode = sculpt_undo_get_node(nodes[n]);
		}
		if (unode) {
			PBVHVertexIter vd;
			SculptOrigVertData orig_data;

			sculpt_orig_vert_data_unode_init(&orig_data, ob, unode);
		
			BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
			{
				sculpt_orig_vert_data_update(&orig_data, &vd);

				if (orig_data.unode->type == SCULPT_UNDO_COORDS) {
					copy_v3_v3(vd.co, orig_data.co);
					if (vd.no) copy_v3_v3_short(vd.no, orig_data.no);
					else normal_short_to_float_v3(vd.fno, orig_data.no);
				}
				else if (orig_data.unode->type == SCULPT_UNDO_MASK) {
					*vd.mask = orig_data.mask;
				}
				if (vd.mvert) vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
			BKE_pbvh_vertex_iter_end;

			BKE_pbvh_node_mark_update(nodes[n]);
		}
	}

	if (ss->face_normals) {
		for (i = 0; i < ss->totpoly; i++) {
			copy_v3_v3(ss->face_normals[i], cache->face_norms[i]);
		}
	}

	if (nodes)
		MEM_freeN(nodes);
}

/*** BVH Tree ***/

static void sculpt_extend_redraw_rect_previous(Object *ob, rcti *rect)
{
	/* expand redraw rect with redraw rect from previous step to
	 * prevent partial-redraw issues caused by fast strokes. This is
	 * needed here (not in sculpt_flush_update) as it was before
	 * because redraw rectangle should be the same in both of
	 * optimized PBVH draw function and 3d view redraw (if not -- some
	 * mesh parts could disappear from screen (sergey) */
	SculptSession *ss = ob->sculpt;

	if (ss->cache) {
		if (!BLI_rcti_is_empty(&ss->cache->previous_r))
			BLI_rcti_union(rect, &ss->cache->previous_r);
	}
}

/* Get a screen-space rectangle of the modified area */
static bool sculpt_get_redraw_rect(ARegion *ar, RegionView3D *rv3d,
                                   Object *ob, rcti *rect)
{
	PBVH *pbvh = ob->sculpt->pbvh;
	float bb_min[3], bb_max[3];

	if (!pbvh)
		return 0;

	BKE_pbvh_redraw_BB(pbvh, bb_min, bb_max);

	/* convert 3D bounding box to screen space */
	if (!paint_convert_bb_to_rect(rect,
	                              bb_min,
	                              bb_max,
	                              ar,
	                              rv3d,
	                              ob))
	{
		return 0;
	}


	return 1;
}

void ED_sculpt_redraw_planes_get(float planes[4][4], ARegion *ar,
                                 RegionView3D *rv3d, Object *ob)
{
	PBVH *pbvh = ob->sculpt->pbvh;
	/* copy here, original will be used below */
	rcti rect = ob->sculpt->cache->current_r;

	sculpt_extend_redraw_rect_previous(ob, &rect);

	paint_calc_redraw_planes(planes, ar, rv3d, ob, &rect);

	/* we will draw this rect, so now we can set it as the previous partial rect.
	 * Note that we don't update with the union of previous/current (rect), only with
	 * the current. Thus we avoid the rectangle needlessly growing to include
	 * all the stroke area */
	ob->sculpt->cache->previous_r = ob->sculpt->cache->current_r;

	/* clear redraw flag from nodes */
	if (pbvh)
		BKE_pbvh_update(pbvh, PBVH_UpdateRedraw, NULL);
}

/************************ Brush Testing *******************/

typedef struct SculptBrushTest {
	float radius_squared;
	float location[3];
	float dist;

	/* View3d clipping - only set rv3d for clipping */
	RegionView3D *clip_rv3d;
} SculptBrushTest;

static void sculpt_brush_test_init(SculptSession *ss, SculptBrushTest *test)
{
	RegionView3D *rv3d = ss->cache->vc->rv3d;

	test->radius_squared = ss->cache->radius_squared;
	copy_v3_v3(test->location, ss->cache->location);
	test->dist = 0.0f;   /* just for initialize */


	if (rv3d->rflag & RV3D_CLIPPING) {
		test->clip_rv3d = rv3d;
	}
	else {
		test->clip_rv3d = NULL;
	}
}

BLI_INLINE bool sculpt_brush_test_clipping(SculptBrushTest *test, const float co[3])
{
	RegionView3D *rv3d = test->clip_rv3d;
	return (rv3d && (ED_view3d_clipping_test(rv3d, co, true)));
}

static bool sculpt_brush_test(SculptBrushTest *test, const float co[3])
{
	float distsq = len_squared_v3v3(co, test->location);

	if (distsq <= test->radius_squared) {
		if (sculpt_brush_test_clipping(test, co)) {
			return 0;
		}
		test->dist = sqrtf(distsq);
		return 1;
	}
	else {
		return 0;
	}
}

static bool sculpt_brush_test_sq(SculptBrushTest *test, const float co[3])
{
	float distsq = len_squared_v3v3(co, test->location);

	if (distsq <= test->radius_squared) {
		if (sculpt_brush_test_clipping(test, co)) {
			return 0;
		}
		test->dist = distsq;
		return 1;
	}
	else {
		return 0;
	}
}

static bool sculpt_brush_test_fast(SculptBrushTest *test, float co[3])
{
	if (sculpt_brush_test_clipping(test, co)) {
		return 0;
	}
	return len_squared_v3v3(co, test->location) <= test->radius_squared;
}

static bool sculpt_brush_test_cube(SculptBrushTest *test, float co[3], float local[4][4])
{
	float side = M_SQRT1_2;
	float local_co[3];

	if (sculpt_brush_test_clipping(test, co)) {
		return 0;
	}

	mul_v3_m4v3(local_co, local, co);

	local_co[0] = fabsf(local_co[0]);
	local_co[1] = fabsf(local_co[1]);
	local_co[2] = fabsf(local_co[2]);

	if (local_co[0] <= side && local_co[1] <= side && local_co[2] <= side) {
		float p = 4.0f;
		
		test->dist = ((powf(local_co[0], p) +
		               powf(local_co[1], p) +
		               powf(local_co[2], p)) / powf(side, p));

		return 1;
	}
	else {
		return 0;
	}
}

static float frontface(Brush *br, const float sculpt_normal[3],
                       const short no[3], const float fno[3])
{
	if (br->flag & BRUSH_FRONTFACE) {
		float dot;

		if (no) {
			float tmp[3];

			normal_short_to_float_v3(tmp, no);
			dot = dot_v3v3(tmp, sculpt_normal);
		}
		else {
			dot = dot_v3v3(fno, sculpt_normal);
		}
		return dot > 0 ? dot : 0;
	}
	else {
		return 1;
	}
}

#if 0

static bool sculpt_brush_test_cyl(SculptBrushTest *test, float co[3], float location[3], float an[3])
{
	if (sculpt_brush_test_fast(test, co)) {
		float t1[3], t2[3], t3[3], dist;

		sub_v3_v3v3(t1, location, co);
		sub_v3_v3v3(t2, x2, location);

		cross_v3_v3v3(t3, an, t1);

		dist = len_v3(t3) / len_v3(t2);

		test->dist = dist;

		return 1;
	}

	return 0;
}

#endif

/* ===== Sculpting =====
 *
 */
static void flip_v3(float v[3], const char symm)
{
	flip_v3_v3(v, v, symm);
}

static float calc_overlap(StrokeCache *cache, const char symm, const char axis, const float angle)
{
	float mirror[3];
	float distsq;
	
	/* flip_v3_v3(mirror, cache->traced_location, symm); */
	flip_v3_v3(mirror, cache->true_location, symm);

	if (axis != 0) {
		float mat[4][4];
		unit_m4(mat);
		rotate_m4(mat, axis, angle);
		mul_m4_v3(mat, mirror);
	}

	/* distsq = len_squared_v3v3(mirror, cache->traced_location); */
	distsq = len_squared_v3v3(mirror, cache->true_location);

	if (distsq <= 4.0f * (cache->radius_squared))
		return (2.0f * (cache->radius) - sqrtf(distsq)) / (2.0f * (cache->radius));
	else
		return 0;
}

static float calc_radial_symmetry_feather(Sculpt *sd, StrokeCache *cache, const char symm, const char axis)
{
	int i;
	float overlap;

	overlap = 0;
	for (i = 1; i < sd->radial_symm[axis - 'X']; ++i) {
		const float angle = 2 * M_PI * i / sd->radial_symm[axis - 'X'];
		overlap += calc_overlap(cache, symm, axis, angle);
	}

	return overlap;
}

static float calc_symmetry_feather(Sculpt *sd, StrokeCache *cache)
{
	if (sd->paint.symmetry_flags & PAINT_SYMMETRY_FEATHER) {
		float overlap;
		int symm = cache->symmetry;
		int i;

		overlap = 0;
		for (i = 0; i <= symm; i++) {
			if (i == 0 || (symm & i && (symm != 5 || i != 3) && (symm != 6 || (i != 3 && i != 5)))) {

				overlap += calc_overlap(cache, i, 0, 0);

				overlap += calc_radial_symmetry_feather(sd, cache, i, 'X');
				overlap += calc_radial_symmetry_feather(sd, cache, i, 'Y');
				overlap += calc_radial_symmetry_feather(sd, cache, i, 'Z');
			}
		}

		return 1 / overlap;
	}
	else {
		return 1;
	}
}

/* Return modified brush strength. Includes the direction of the brush, positive
 * values pull vertices, negative values push. Uses tablet pressure and a
 * special multiplier found experimentally to scale the strength factor. */
static float brush_strength(Sculpt *sd, StrokeCache *cache, float feather, UnifiedPaintSettings *ups)
{
	const Scene *scene = cache->vc->scene;
	Brush *brush = BKE_paint_brush(&sd->paint);

	/* Primary strength input; square it to make lower values more sensitive */
	const float root_alpha = BKE_brush_alpha_get(scene, brush);
	float alpha        = root_alpha * root_alpha;
	float dir          = brush->flag & BRUSH_DIR_IN ? -1 : 1;
	float pressure     = BKE_brush_use_alpha_pressure(scene, brush) ? cache->pressure : 1;
	float pen_flip     = cache->pen_flip ? -1 : 1;
	float invert       = cache->invert ? -1 : 1;
	float overlap       = ups->overlap_factor;
	/* spacing is integer percentage of radius, divide by 50 to get
	 * normalized diameter */

	float flip         = dir * invert * pen_flip;

	switch (brush->sculpt_tool) {
		case SCULPT_TOOL_CLAY:
		case SCULPT_TOOL_CLAY_STRIPS:
		case SCULPT_TOOL_DRAW:
		case SCULPT_TOOL_LAYER:
			return alpha * flip * pressure * overlap * feather;
			
		case SCULPT_TOOL_MASK:
			overlap = (1 + overlap) / 2;
			switch ((BrushMaskTool)brush->mask_tool) {
				case BRUSH_MASK_DRAW:
					return alpha * flip * pressure * overlap * feather;
				case BRUSH_MASK_SMOOTH:
					return alpha * pressure * feather;
			}

		case SCULPT_TOOL_CREASE:
		case SCULPT_TOOL_BLOB:
			return alpha * flip * pressure * overlap * feather;

		case SCULPT_TOOL_INFLATE:
			if (flip > 0) {
				return 0.250f * alpha * flip * pressure * overlap * feather;
			}
			else {
				return 0.125f * alpha * flip * pressure * overlap * feather;
			}

		case SCULPT_TOOL_FILL:
		case SCULPT_TOOL_SCRAPE:
		case SCULPT_TOOL_FLATTEN:
			if (flip > 0) {
				overlap = (1 + overlap) / 2;
				return alpha * flip * pressure * overlap * feather;
			}
			else {
				/* reduce strength for DEEPEN, PEAKS, and CONTRAST */
				return 0.5f * alpha * flip * pressure * overlap * feather; 
			}

		case SCULPT_TOOL_SMOOTH:
			return alpha * pressure * feather;

		case SCULPT_TOOL_PINCH:
			if (flip > 0) {
				return alpha * flip * pressure * overlap * feather;
			}
			else {
				return 0.25f * alpha * flip * pressure * overlap * feather;
			}

		case SCULPT_TOOL_NUDGE:
			overlap = (1 + overlap) / 2;
			return alpha * pressure * overlap * feather;

		case SCULPT_TOOL_THUMB:
			return alpha * pressure * feather;

		case SCULPT_TOOL_SNAKE_HOOK:
			return root_alpha * feather;

		case SCULPT_TOOL_GRAB:
			return root_alpha * feather;

		case SCULPT_TOOL_ROTATE:
			return alpha * pressure * feather;

		default:
			return 0;
	}
}

/* Return a multiplier for brush strength on a particular vertex. */
static float tex_strength(SculptSession *ss, Brush *br,
                          const float point[3],
                          const float len,
                          const short vno[3],
                          const float fno[3],
                          const float mask)
{
	StrokeCache *cache = ss->cache;
	const Scene *scene = cache->vc->scene;
	MTex *mtex = &br->mtex;
	float avg = 1;
	float rgba[4];
	int thread_num;

	if (!mtex->tex) {
		avg = 1;
	}
	else if (mtex->brush_map_mode == MTEX_MAP_MODE_3D) {
		/* Get strength by feeding the vertex 
		 * location directly into a texture */
		avg = BKE_brush_sample_tex_3D(scene, br, point, rgba, 0, ss->tex_pool);
	}
	else if (ss->texcache) {
		float symm_point[3], point_2d[2];
		float x = 0.0f, y = 0.0f; /* Quite warnings */

		/* if the active area is being applied for symmetry, flip it
		 * across the symmetry axis and rotate it back to the original
		 * position in order to project it. This insures that the 
		 * brush texture will be oriented correctly. */

		flip_v3_v3(symm_point, point, cache->mirror_symmetry_pass);

		if (cache->radial_symmetry_pass)
			mul_m4_v3(cache->symm_rot_mat_inv, symm_point);

		ED_view3d_project_float_v2_m4(cache->vc->ar, symm_point, point_2d, cache->projection_mat);

		/* still no symmetry supported for other paint modes.
		 * Sculpt does it DIY */
		if (mtex->brush_map_mode == MTEX_MAP_MODE_AREA) {
			/* Similar to fixed mode, but projects from brush angle
			 * rather than view direction */

			mul_m4_v3(cache->brush_local_mat, symm_point);

			x = symm_point[0];
			y = symm_point[1];

			x *= br->mtex.size[0];
			y *= br->mtex.size[1];

			x += br->mtex.ofs[0];
			y += br->mtex.ofs[1];

#ifdef _OPENMP
			thread_num = omp_get_thread_num();
#else
			thread_num = 0;
#endif
			avg = paint_get_tex_pixel(&br->mtex, x, y, ss->tex_pool, thread_num);

			avg += br->texture_sample_bias;
		}
		else {
			const float point_3d[3] = {point_2d[0], point_2d[1], 0.0f};
			avg = BKE_brush_sample_tex_3D(scene, br, point_3d, rgba, 0, ss->tex_pool);
		}
	}

	/* Falloff curve */
	avg *= BKE_brush_curve_strength(br, len, cache->radius);

	avg *= frontface(br, cache->view_normal, vno, fno);

	/* Paint mask */
	avg *= 1.0f - mask;

	return avg;
}

typedef struct {
	Sculpt *sd;
	SculptSession *ss;
	float radius_squared;
	bool original;
} SculptSearchSphereData;

/* Test AABB against sphere */
static bool sculpt_search_sphere_cb(PBVHNode *node, void *data_v)
{
	SculptSearchSphereData *data = data_v;
	float *center = data->ss->cache->location, nearest[3];
	float t[3], bb_min[3], bb_max[3];
	int i;

	if (data->original)
		BKE_pbvh_node_get_original_BB(node, bb_min, bb_max);
	else
		BKE_pbvh_node_get_BB(node, bb_min, bb_max);
	
	for (i = 0; i < 3; ++i) {
		if (bb_min[i] > center[i])
			nearest[i] = bb_min[i];
		else if (bb_max[i] < center[i])
			nearest[i] = bb_max[i];
		else
			nearest[i] = center[i]; 
	}
	
	sub_v3_v3v3(t, center, nearest);

	return len_squared_v3(t) < data->radius_squared;
}

/* Handles clipping against a mirror modifier and SCULPT_LOCK axis flags */
static void sculpt_clip(Sculpt *sd, SculptSession *ss, float co[3], const float val[3])
{
	int i;

	for (i = 0; i < 3; ++i) {
		if (sd->flags & (SCULPT_LOCK_X << i))
			continue;

		if ((ss->cache->flag & (CLIP_X << i)) && (fabsf(co[i]) <= ss->cache->clip_tolerance[i]))
			co[i] = 0.0f;
		else
			co[i] = val[i];
	}
}

static void add_norm_if(float view_vec[3], float out[3], float out_flip[3], float fno[3])
{
	if ((dot_v3v3(view_vec, fno)) > 0) {
		add_v3_v3(out, fno);
	}
	else {
		add_v3_v3(out_flip, fno); /* out_flip is used when out is {0,0,0} */
	}
}

static void calc_area_normal(Sculpt *sd, Object *ob, float an[3], PBVHNode **nodes, int totnode)
{
	float out_flip[3] = {0.0f, 0.0f, 0.0f};

	SculptSession *ss = ob->sculpt;
	const Brush *brush = BKE_paint_brush(&sd->paint);
	int n;
	bool original;

	/* Grab brush requires to test on original data (see r33888 and
	 * bug #25371) */
	original = (BKE_paint_brush(&sd->paint)->sculpt_tool == SCULPT_TOOL_GRAB ?
	            true : ss->cache->original);

	/* In general the original coords are not available with dynamic
	 * topology
	 *
	 * Mask tool could not use undo nodes to get coordinates from
	 * since the coordinates are not stored in those odes.
	 * And mask tool is not gonna to modify vertex coordinates,
	 * so we don't actually need to use modified coords.
	 */
	if (ss->bm || brush->sculpt_tool == SCULPT_TOOL_MASK)
		original = false;

	(void)sd; /* unused w/o openmp */
	
	zero_v3(an);

#pragma omp parallel for schedule(guided) if ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_OMP_LIMIT)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		SculptUndoNode *unode;
		float private_an[3] = {0.0f, 0.0f, 0.0f};
		float private_out_flip[3] = {0.0f, 0.0f, 0.0f};

		unode = sculpt_undo_push_node(ob, nodes[n], SCULPT_UNDO_COORDS);
		sculpt_brush_test_init(ss, &test);

		if (original) {
			BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
			{
				if (sculpt_brush_test_fast(&test, unode->co[vd.i])) {
					float fno[3];

					normal_short_to_float_v3(fno, unode->no[vd.i]);
					add_norm_if(ss->cache->view_normal, private_an, private_out_flip, fno);
				}
			}
			BKE_pbvh_vertex_iter_end;
		}
		else {
			BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
			{
				if (sculpt_brush_test_fast(&test, vd.co)) {
					if (vd.no) {
						float fno[3];

						normal_short_to_float_v3(fno, vd.no);
						add_norm_if(ss->cache->view_normal, private_an, private_out_flip, fno);
					}
					else {
						add_norm_if(ss->cache->view_normal, private_an, private_out_flip, vd.fno);
					}
				}
			}
			BKE_pbvh_vertex_iter_end;
		}

#pragma omp critical
		{
			add_v3_v3(an, private_an);
			add_v3_v3(out_flip, private_out_flip);
		}
	}

	if (is_zero_v3(an))
		copy_v3_v3(an, out_flip);

	normalize_v3(an);
}

/* Calculate primary direction of movement for many brushes */
static void calc_sculpt_normal(Sculpt *sd, Object *ob,
                               PBVHNode **nodes, int totnode,
                               float an[3])
{
	const Brush *brush = BKE_paint_brush(&sd->paint);
	const SculptSession *ss = ob->sculpt;

	switch (brush->sculpt_plane) {
		case SCULPT_DISP_DIR_VIEW:
			copy_v3_v3(an, ss->cache->true_view_normal);
			break;

		case SCULPT_DISP_DIR_X:
			an[1] = 0.0;
			an[2] = 0.0;
			an[0] = 1.0;
			break;

		case SCULPT_DISP_DIR_Y:
			an[0] = 0.0;
			an[2] = 0.0;
			an[1] = 1.0;
			break;

		case SCULPT_DISP_DIR_Z:
			an[0] = 0.0;
			an[1] = 0.0;
			an[2] = 1.0;
			break;

		case SCULPT_DISP_DIR_AREA:
			calc_area_normal(sd, ob, an, nodes, totnode);
			break;

		default:
			break;
	}
}

static void update_sculpt_normal(Sculpt *sd, Object *ob,
                                 PBVHNode **nodes, int totnode)
{
	const Brush *brush = BKE_paint_brush(&sd->paint);
	StrokeCache *cache = ob->sculpt->cache;
	
	if (cache->mirror_symmetry_pass == 0 &&
	    cache->radial_symmetry_pass == 0 &&
	    (cache->first_time || !(brush->flag & BRUSH_ORIGINAL_NORMAL)))
	{
		calc_sculpt_normal(sd, ob, nodes, totnode, cache->sculpt_normal);
		copy_v3_v3(cache->sculpt_normal_symm, cache->sculpt_normal);
	}
	else {
		copy_v3_v3(cache->sculpt_normal_symm, cache->sculpt_normal);
		flip_v3(cache->sculpt_normal_symm, cache->mirror_symmetry_pass);
		mul_m4_v3(cache->symm_rot_mat, cache->sculpt_normal_symm);
	}
}

static void calc_local_y(ViewContext *vc, const float center[3], float y[3])
{
	Object *ob = vc->obact;
	float loc[3], mval_f[2] = {0.0f, 1.0f};
	float zfac;

	mul_v3_m4v3(loc, ob->imat, center);
	zfac = ED_view3d_calc_zfac(vc->rv3d, loc, NULL);

	ED_view3d_win_to_delta(vc->ar, mval_f, y, zfac);
	normalize_v3(y);

	add_v3_v3(y, ob->loc);
	mul_m4_v3(ob->imat, y);
}

static void calc_brush_local_mat(const Brush *brush, Object *ob,
                                 float local_mat[4][4])
{
	const StrokeCache *cache = ob->sculpt->cache;
	float tmat[4][4];
	float mat[4][4];
	float scale[4][4];
	float angle, v[3];
	float up[3];

	/* Ensure ob->imat is up to date */
	invert_m4_m4(ob->imat, ob->obmat);

	/* Initialize last column of matrix */
	mat[0][3] = 0;
	mat[1][3] = 0;
	mat[2][3] = 0;
	mat[3][3] = 1;

	/* Get view's up vector in object-space */
	calc_local_y(cache->vc, cache->location, up);

	/* Calculate the X axis of the local matrix */
	cross_v3_v3v3(v, up, cache->sculpt_normal);
	/* Apply rotation (user angle, rake, etc.) to X axis */
	angle = brush->mtex.rot - cache->special_rotation;
	rotate_v3_v3v3fl(mat[0], v, cache->sculpt_normal, angle);

	/* Get other axes */
	cross_v3_v3v3(mat[1], cache->sculpt_normal, mat[0]);
	copy_v3_v3(mat[2], cache->sculpt_normal);

	/* Set location */
	copy_v3_v3(mat[3], cache->location);

	/* Scale by brush radius */
	normalize_m4(mat);
	scale_m4_fl(scale, cache->radius);
	mul_m4_m4m4(tmat, mat, scale);

	/* Return inverse (for converting from modelspace coords to local
	 * area coords) */
	invert_m4_m4(local_mat, tmat);
}

static void update_brush_local_mat(Sculpt *sd, Object *ob)
{
	StrokeCache *cache = ob->sculpt->cache;

	if (cache->mirror_symmetry_pass == 0 &&
	    cache->radial_symmetry_pass == 0)
	{
		calc_brush_local_mat(BKE_paint_brush(&sd->paint), ob,
		                     cache->brush_local_mat);
	}
}

/* Test whether the StrokeCache.sculpt_normal needs update in
 * do_brush_action() */
static int brush_needs_sculpt_normal(const Brush *brush)
{
	return ((ELEM(brush->sculpt_tool,
	              SCULPT_TOOL_GRAB,
	              SCULPT_TOOL_SNAKE_HOOK) &&
	         (brush->normal_weight > 0)) ||

	        ELEM(brush->sculpt_tool,
	             SCULPT_TOOL_BLOB,
	             SCULPT_TOOL_CREASE,
	             SCULPT_TOOL_DRAW,
	             SCULPT_TOOL_LAYER,
	             SCULPT_TOOL_NUDGE,
	             SCULPT_TOOL_ROTATE,
	             SCULPT_TOOL_THUMB) ||

	        (brush->mtex.brush_map_mode == MTEX_MAP_MODE_AREA));
}

/* For the smooth brush, uses the neighboring vertices around vert to calculate
 * a smoothed location for vert. Skips corner vertices (used by only one
 * polygon.) */
static void neighbor_average(SculptSession *ss, float avg[3], unsigned vert)
{
	const MeshElemMap *vert_map = &ss->pmap[vert];
	const MVert *mvert = ss->mvert;
	float (*deform_co)[3] = ss->deform_cos;

	zero_v3(avg);
		
	/* Don't modify corner vertices */
	if (vert_map->count > 1) {
		int i, total = 0;

		for (i = 0; i < vert_map->count; i++) {
			const MPoly *p = &ss->mpoly[vert_map->indices[i]];
			unsigned f_adj_v[3];

			if (poly_get_adj_loops_from_vert(f_adj_v, p, ss->mloop, vert) != -1) {
				int j;
			
				for (j = 0; j < 3; j++) {
					if (vert_map->count != 2 || ss->pmap[f_adj_v[j]].count <= 2) {
						add_v3_v3(avg, deform_co ? deform_co[f_adj_v[j]] :
						          mvert[f_adj_v[j]].co);

						total++;
					}
				}
			}
		}

		if (total > 0) {
			mul_v3_fl(avg, 1.0f / total);
			return;
		}
	}

	copy_v3_v3(avg, deform_co ? deform_co[vert] : mvert[vert].co);
}

/* Similar to neighbor_average(), but returns an averaged mask value
 * instead of coordinate. Also does not restrict based on border or
 * corner vertices. */
static float neighbor_average_mask(SculptSession *ss, unsigned vert)
{
	const float *vmask = ss->vmask;
	float avg = 0;
	int i, total = 0;

	for (i = 0; i < ss->pmap[vert].count; i++) {
		const MPoly *p = &ss->mpoly[ss->pmap[vert].indices[i]];
		unsigned f_adj_v[3];

		if (poly_get_adj_loops_from_vert(f_adj_v, p, ss->mloop, vert) != -1) {
			int j;
			
			for (j = 0; j < 3; j++) {
				avg += vmask[f_adj_v[j]];
				total++;
			}
		}
	}

	if (total > 0)
		return avg / (float)total;
	else
		return vmask[vert];
}

/* Same logic as neighbor_average(), but for bmesh rather than mesh */
static void bmesh_neighbor_average(float avg[3], BMVert *v)
{
	const int vfcount = BM_vert_face_count(v);

	zero_v3(avg);
		
	/* Don't modify corner vertices */
	if (vfcount > 1) {
		BMIter liter;
		BMLoop *l;
		int i, total = 0;

		BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
			BMVert *adj_v[3] = {l->prev->v, v, l->next->v};

			for (i = 0; i < 3; i++) {
				if (vfcount != 2 || BM_vert_face_count(adj_v[i]) <= 2) {
					add_v3_v3(avg, adj_v[i]->co);
					total++;
				}
			}
		}

		if (total > 0) {
			mul_v3_fl(avg, 1.0f / total);
			return;
		}
	}

	copy_v3_v3(avg, v->co);
}

/* Same logic as neighbor_average_mask(), but for bmesh rather than mesh */
static float bmesh_neighbor_average_mask(BMesh *bm, BMVert *v)
{
	BMIter liter;
	BMLoop *l;
	float avg = 0;
	int i, total = 0;

	BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
		BMVert *adj_v[3] = {l->prev->v, v, l->next->v};

		for (i = 0; i < 3; i++) {
			BMVert *v2 = adj_v[i];
			float *vmask = CustomData_bmesh_get(&bm->vdata,
			                                    v2->head.data,
			                                    CD_PAINT_MASK);
			avg += (*vmask);
			total++;
		}
	}

	if (total > 0) {
		return avg / (float)total;
	}
	else {
		float *vmask = CustomData_bmesh_get(&bm->vdata,
			                                v->head.data,
			                                CD_PAINT_MASK);
		return (*vmask);
	}
}

static void do_mesh_smooth_brush(Sculpt *sd, SculptSession *ss, PBVHNode *node, float bstrength, int smooth_mask)
{
	Brush *brush = BKE_paint_brush(&sd->paint);
	PBVHVertexIter vd;
	SculptBrushTest test;
	
	CLAMP(bstrength, 0.0f, 1.0f);

	sculpt_brush_test_init(ss, &test);

	BKE_pbvh_vertex_iter_begin(ss->pbvh, node, vd, PBVH_ITER_UNIQUE)
	{
		if (sculpt_brush_test(&test, vd.co)) {
			const float fade = bstrength * tex_strength(ss, brush, vd.co, test.dist,
			                                            vd.no, vd.fno,
			                                            smooth_mask ? 0 : (vd.mask ? *vd.mask : 0.0f));
			if (smooth_mask) {
				float val = neighbor_average_mask(ss, vd.vert_indices[vd.i]) - *vd.mask;
				val *= fade * bstrength;
				*vd.mask += val;
				CLAMP(*vd.mask, 0, 1);
			}
			else {
				float avg[3], val[3];

				neighbor_average(ss, avg, vd.vert_indices[vd.i]);
				sub_v3_v3v3(val, avg, vd.co);
				mul_v3_fl(val, fade);

				add_v3_v3(val, vd.co);

				sculpt_clip(sd, ss, vd.co, val);
			}

			if (vd.mvert)
				vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
		}
	}
	BKE_pbvh_vertex_iter_end;
}

static void do_bmesh_smooth_brush(Sculpt *sd, SculptSession *ss, PBVHNode *node, float bstrength, int smooth_mask)
{
	Brush *brush = BKE_paint_brush(&sd->paint);
	PBVHVertexIter vd;
	SculptBrushTest test;
	
	CLAMP(bstrength, 0.0f, 1.0f);

	sculpt_brush_test_init(ss, &test);

	BKE_pbvh_vertex_iter_begin(ss->pbvh, node, vd, PBVH_ITER_UNIQUE)
	{
		if (sculpt_brush_test(&test, vd.co)) {
			const float fade = bstrength * tex_strength(ss, brush, vd.co, test.dist,
			                                            vd.no, vd.fno,
			                                            smooth_mask ? 0 : *vd.mask);
			if (smooth_mask) {
				float val = bmesh_neighbor_average_mask(ss->bm, vd.bm_vert) - *vd.mask;
				val *= fade * bstrength;
				*vd.mask += val;
				CLAMP(*vd.mask, 0, 1);
			}
			else {
				float avg[3], val[3];

				bmesh_neighbor_average(avg, vd.bm_vert);
				sub_v3_v3v3(val, avg, vd.co);
				mul_v3_fl(val, fade);

				add_v3_v3(val, vd.co);

				sculpt_clip(sd, ss, vd.co, val);
			}

			if (vd.mvert)
				vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
		}
	}
	BKE_pbvh_vertex_iter_end;
}

static void do_multires_smooth_brush(Sculpt *sd, SculptSession *ss, PBVHNode *node,
                                     float bstrength, int smooth_mask)
{
	Brush *brush = BKE_paint_brush(&sd->paint);
	SculptBrushTest test;
	CCGElem **griddata, *data;
	CCGKey key;
	DMGridAdjacency *gridadj, *adj;
	float (*tmpgrid_co)[3], (*tmprow_co)[3];
	float *tmpgrid_mask, *tmprow_mask;
	int v1, v2, v3, v4;
	int thread_num;
	BLI_bitmap **grid_hidden;
	int *grid_indices, totgrid, gridsize, i, x, y;

	sculpt_brush_test_init(ss, &test);

	CLAMP(bstrength, 0.0f, 1.0f);

	BKE_pbvh_node_get_grids(ss->pbvh, node, &grid_indices, &totgrid,
	                        NULL, &gridsize, &griddata, &gridadj);
	BKE_pbvh_get_grid_key(ss->pbvh, &key);

	grid_hidden = BKE_pbvh_grid_hidden(ss->pbvh);

#ifdef _OPENMP
	thread_num = omp_get_thread_num();
#else
	thread_num = 0;
#endif
	tmpgrid_co = ss->cache->tmpgrid_co[thread_num];
	tmprow_co = ss->cache->tmprow_co[thread_num];
	tmpgrid_mask = ss->cache->tmpgrid_mask[thread_num];
	tmprow_mask = ss->cache->tmprow_mask[thread_num];

	for (i = 0; i < totgrid; ++i) {
		int gi = grid_indices[i];
		BLI_bitmap *gh = grid_hidden[gi];
		data = griddata[gi];
		adj = &gridadj[gi];

		if (smooth_mask)
			memset(tmpgrid_mask, 0, sizeof(float) * gridsize * gridsize);
		else
			memset(tmpgrid_co, 0, sizeof(float) * 3 * gridsize * gridsize);

		for (y = 0; y < gridsize - 1; y++) {
			v1 = y * gridsize;
			if (smooth_mask) {
				tmprow_mask[0] = (*CCG_elem_offset_mask(&key, data, v1) +
				                  *CCG_elem_offset_mask(&key, data, v1 + gridsize));
			}
			else {
				add_v3_v3v3(tmprow_co[0],
				            CCG_elem_offset_co(&key, data, v1),
				            CCG_elem_offset_co(&key, data, v1 + gridsize));
			}

			for (x = 0; x < gridsize - 1; x++) {
				v1 = x + y * gridsize;
				v2 = v1 + 1;
				v3 = v1 + gridsize;
				v4 = v3 + 1;

				if (smooth_mask) {
					float tmp;

					tmprow_mask[x + 1] = (*CCG_elem_offset_mask(&key, data, v2) +
					                      *CCG_elem_offset_mask(&key, data, v4));
					tmp = tmprow_mask[x + 1] + tmprow_mask[x];

					tmpgrid_mask[v1] += tmp;
					tmpgrid_mask[v2] += tmp;
					tmpgrid_mask[v3] += tmp;
					tmpgrid_mask[v4] += tmp;
				}
				else {
					float tmp[3];

					add_v3_v3v3(tmprow_co[x + 1],
					            CCG_elem_offset_co(&key, data, v2),
					            CCG_elem_offset_co(&key, data, v4));
					add_v3_v3v3(tmp, tmprow_co[x + 1], tmprow_co[x]);

					add_v3_v3(tmpgrid_co[v1], tmp);
					add_v3_v3(tmpgrid_co[v2], tmp);
					add_v3_v3(tmpgrid_co[v3], tmp);
					add_v3_v3(tmpgrid_co[v4], tmp);
				}
			}
		}

		/* blend with existing coordinates */
		for (y = 0; y < gridsize; ++y) {
			for (x = 0; x < gridsize; ++x) {
				float *co;
				const float *fno;
				float *mask;
				int index;

				if (gh) {
					if (BLI_BITMAP_TEST(gh, y * gridsize + x))
						continue;
				}

				if (x == 0 && adj->index[0] == -1)
					continue;

				if (x == gridsize - 1 && adj->index[2] == -1)
					continue;

				if (y == 0 && adj->index[3] == -1)
					continue;

				if (y == gridsize - 1 && adj->index[1] == -1)
					continue;

				index = x + y * gridsize;
				co = CCG_elem_offset_co(&key, data, index);
				fno = CCG_elem_offset_no(&key, data, index);
				mask = CCG_elem_offset_mask(&key, data, index);

				if (sculpt_brush_test(&test, co)) {
					const float strength_mask = (smooth_mask ? 0 : *mask);
					const float fade = bstrength * tex_strength(ss, brush, co, test.dist,
					                                            NULL, fno, strength_mask);
					float n = 1.0f / 16.0f;
					
					if (x == 0 || x == gridsize - 1)
						n *= 2;
					
					if (y == 0 || y == gridsize - 1)
						n *= 2;
					
					if (smooth_mask) {
						*mask += ((tmpgrid_mask[x + y * gridsize] * n) - *mask) * fade;
					}
					else {
						float *avg, val[3];

						avg = tmpgrid_co[x + y * gridsize];

						mul_v3_fl(avg, n);

						sub_v3_v3v3(val, avg, co);
						mul_v3_fl(val, fade);

						add_v3_v3(val, co);

						sculpt_clip(sd, ss, co, val);
					}
				}
			}
		}
	}
}

static void smooth(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode,
                   float bstrength, int smooth_mask)
{
	SculptSession *ss = ob->sculpt;
	const int max_iterations = 4;
	const float fract = 1.0f / max_iterations;
	PBVHType type = BKE_pbvh_type(ss->pbvh);
	int iteration, n, count;
	float last;

	CLAMP(bstrength, 0, 1);

	count = (int)(bstrength * max_iterations);
	last  = max_iterations * (bstrength - count * fract);

	if (type == PBVH_FACES && !ss->pmap) {
		BLI_assert(!"sculpt smooth: pmap missing");
		return;
	}

	for (iteration = 0; iteration <= count; ++iteration) {
		float strength = (iteration != count) ? 1.0f : last;

#pragma omp parallel for schedule(guided) if ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_OMP_LIMIT)
		for (n = 0; n < totnode; n++) {
			switch (type) {
				case PBVH_GRIDS:
					do_multires_smooth_brush(sd, ss, nodes[n], strength,
					                         smooth_mask);
					break;
				case PBVH_FACES:
					do_mesh_smooth_brush(sd, ss, nodes[n], strength,
					                     smooth_mask);
					break;
				case PBVH_BMESH:
					do_bmesh_smooth_brush(sd, ss, nodes[n], strength, smooth_mask);
					break;
			}
		}

		if (ss->multires)
			multires_stitch_grids(ob);
	}
}

static void do_smooth_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	smooth(sd, ob, nodes, totnode, ss->cache->bstrength, false);
}

static void do_mask_brush_draw(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = BKE_paint_brush(&sd->paint);
	float bstrength = ss->cache->bstrength;
	int n;

	/* threaded loop over nodes */
#pragma omp parallel for schedule(guided) if ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_OMP_LIMIT)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;

		sculpt_brush_test_init(ss, &test);

		BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
		{
			if (sculpt_brush_test(&test, vd.co)) {
				float fade = tex_strength(ss, brush, vd.co, test.dist,
				                          vd.no, vd.fno, 0);

				(*vd.mask) += fade * bstrength;
				CLAMP(*vd.mask, 0, 1);

				if (vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
			BKE_pbvh_vertex_iter_end;
		}
	}
}

static void do_mask_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = BKE_paint_brush(&sd->paint);
	
	switch ((BrushMaskTool)brush->mask_tool) {
		case BRUSH_MASK_DRAW:
			do_mask_brush_draw(sd, ob, nodes, totnode);
			break;
		case BRUSH_MASK_SMOOTH:
			smooth(sd, ob, nodes, totnode, ss->cache->bstrength, true);
			break;
	}
}

static void do_draw_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = BKE_paint_brush(&sd->paint);
	float offset[3];
	float bstrength = ss->cache->bstrength;
	int n;

	/* offset with as much as possible factored in already */
	mul_v3_v3fl(offset, ss->cache->sculpt_normal_symm, ss->cache->radius);
	mul_v3_v3(offset, ss->cache->scale);
	mul_v3_fl(offset, bstrength);

	/* threaded loop over nodes */
#pragma omp parallel for schedule(guided) if ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_OMP_LIMIT)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy = BKE_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
		{
			if (sculpt_brush_test(&test, vd.co)) {
				/* offset vertex */
				float fade = tex_strength(ss, brush, vd.co, test.dist, vd.no,
				                          vd.fno, vd.mask ? *vd.mask : 0.0f);

				mul_v3_v3fl(proxy[vd.i], offset, fade);

				if (vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BKE_pbvh_vertex_iter_end;
	}
}

static void do_crease_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	const Scene *scene = ss->cache->vc->scene;
	Brush *brush = BKE_paint_brush(&sd->paint);
	float offset[3];
	float bstrength = ss->cache->bstrength;
	float flippedbstrength, crease_correction;
	float brush_alpha;
	int n;

	/* offset with as much as possible factored in already */
	mul_v3_v3fl(offset, ss->cache->sculpt_normal_symm, ss->cache->radius);
	mul_v3_v3(offset, ss->cache->scale);
	mul_v3_fl(offset, bstrength);
	
	/* we divide out the squared alpha and multiply by the squared crease to give us the pinch strength */
	crease_correction = brush->crease_pinch_factor * brush->crease_pinch_factor;
	brush_alpha = BKE_brush_alpha_get(scene, brush);
	if (brush_alpha > 0.0f)
		crease_correction /= brush_alpha * brush_alpha;

	/* we always want crease to pinch or blob to relax even when draw is negative */
	flippedbstrength = (bstrength < 0) ? -crease_correction * bstrength : crease_correction * bstrength;

	if (brush->sculpt_tool == SCULPT_TOOL_BLOB) flippedbstrength *= -1.0f;

	/* threaded loop over nodes */
#pragma omp parallel for schedule(guided) if ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_OMP_LIMIT)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy = BKE_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
		{
			if (sculpt_brush_test(&test, vd.co)) {
				/* offset vertex */
				const float fade = tex_strength(ss, brush, vd.co, test.dist,
				                                vd.no, vd.fno, vd.mask ? *vd.mask : 0.0f);
				float val1[3];
				float val2[3];

				/* first we pinch */
				sub_v3_v3v3(val1, test.location, vd.co);
				mul_v3_fl(val1, fade * flippedbstrength);

				/* then we draw */
				mul_v3_v3fl(val2, offset, fade);

				add_v3_v3v3(proxy[vd.i], val1, val2);

				if (vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BKE_pbvh_vertex_iter_end;
	}
}

static void do_pinch_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = BKE_paint_brush(&sd->paint);
	float bstrength = ss->cache->bstrength;
	int n;

#pragma omp parallel for schedule(guided) if ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_OMP_LIMIT)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy = BKE_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
		{
			if (sculpt_brush_test(&test, vd.co)) {
				float fade = bstrength * tex_strength(ss, brush, vd.co, test.dist, vd.no,
				                                      vd.fno, vd.mask ? *vd.mask : 0.0f);
				float val[3];

				sub_v3_v3v3(val, test.location, vd.co);
				mul_v3_v3fl(proxy[vd.i], val, fade);

				if (vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BKE_pbvh_vertex_iter_end;
	}
}

static void do_grab_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = BKE_paint_brush(&sd->paint);
	float bstrength = ss->cache->bstrength;
	float grab_delta[3];
	int n;
	float len;

	copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

	len = len_v3(grab_delta);

	if (brush->normal_weight > 0) {
		mul_v3_fl(ss->cache->sculpt_normal_symm, len * brush->normal_weight);
		mul_v3_fl(grab_delta, 1.0f - brush->normal_weight);
		add_v3_v3(grab_delta, ss->cache->sculpt_normal_symm);
	}

#pragma omp parallel for schedule(guided) if ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_OMP_LIMIT)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		SculptOrigVertData orig_data;
		float (*proxy)[3];

		sculpt_orig_vert_data_init(&orig_data, ob, nodes[n]);

		proxy = BKE_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
		{
			sculpt_orig_vert_data_update(&orig_data, &vd);

			if (sculpt_brush_test(&test, orig_data.co)) {
				const float fade = bstrength * tex_strength(ss, brush,
				                                            orig_data.co,
				                                            test.dist,
				                                            orig_data.no,
				                                            NULL, vd.mask ? *vd.mask : 0.0f);

				mul_v3_v3fl(proxy[vd.i], grab_delta, fade);

				if (vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BKE_pbvh_vertex_iter_end;
	}
}

static void do_nudge_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = BKE_paint_brush(&sd->paint);
	float bstrength = ss->cache->bstrength;
	float grab_delta[3];
	float tmp[3], cono[3];
	int n;

	copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

	cross_v3_v3v3(tmp, ss->cache->sculpt_normal_symm, grab_delta);
	cross_v3_v3v3(cono, tmp, ss->cache->sculpt_normal_symm);

#pragma omp parallel for schedule(guided) if ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_OMP_LIMIT)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy = BKE_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
		{
			if (sculpt_brush_test(&test, vd.co)) {
				const float fade = bstrength * tex_strength(ss, brush, vd.co, test.dist,
				                                            vd.no, vd.fno, vd.mask ? *vd.mask : 0.0f);

				mul_v3_v3fl(proxy[vd.i], cono, fade);

				if (vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BKE_pbvh_vertex_iter_end;
	}
}

static void do_snake_hook_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = BKE_paint_brush(&sd->paint);
	float bstrength = ss->cache->bstrength;
	float grab_delta[3];
	int n;
	float len;

	copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

	len = len_v3(grab_delta);

	if (bstrength < 0)
		negate_v3(grab_delta);

	if (brush->normal_weight > 0) {
		mul_v3_fl(ss->cache->sculpt_normal_symm, len * brush->normal_weight);
		mul_v3_fl(grab_delta, 1.0f - brush->normal_weight);
		add_v3_v3(grab_delta, ss->cache->sculpt_normal_symm);
	}

#pragma omp parallel for schedule(guided) if ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_OMP_LIMIT)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy = BKE_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
		{
			if (sculpt_brush_test(&test, vd.co)) {
				const float fade = bstrength * tex_strength(ss, brush, vd.co, test.dist,
				                                            vd.no, vd.fno, vd.mask ? *vd.mask : 0.0f);

				mul_v3_v3fl(proxy[vd.i], grab_delta, fade);

				if (vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BKE_pbvh_vertex_iter_end;
	}
}

static void do_thumb_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = BKE_paint_brush(&sd->paint);
	float bstrength = ss->cache->bstrength;
	float grab_delta[3];
	float tmp[3], cono[3];
	int n;

	copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

	cross_v3_v3v3(tmp, ss->cache->sculpt_normal_symm, grab_delta);
	cross_v3_v3v3(cono, tmp, ss->cache->sculpt_normal_symm);

#pragma omp parallel for schedule(guided) if ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_OMP_LIMIT)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		SculptOrigVertData orig_data;
		float (*proxy)[3];

		sculpt_orig_vert_data_init(&orig_data, ob, nodes[n]);

		proxy = BKE_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
		{
			sculpt_orig_vert_data_update(&orig_data, &vd);

			if (sculpt_brush_test(&test, orig_data.co)) {
				const float fade = bstrength * tex_strength(ss, brush,
				                                            orig_data.co,
				                                            test.dist,
				                                            orig_data.no,
				                                            NULL, vd.mask ? *vd.mask : 0.0f);

				mul_v3_v3fl(proxy[vd.i], cono, fade);

				if (vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BKE_pbvh_vertex_iter_end;
	}
}

static void do_rotate_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = BKE_paint_brush(&sd->paint);
	float bstrength = ss->cache->bstrength;
	int n;
	static const int flip[8] = { 1, -1, -1, 1, -1, 1, 1, -1 };
	float angle = ss->cache->vertex_rotation * flip[ss->cache->mirror_symmetry_pass];

#pragma omp parallel for schedule(guided) if ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_OMP_LIMIT)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		SculptOrigVertData orig_data;
		float (*proxy)[3];

		sculpt_orig_vert_data_init(&orig_data, ob, nodes[n]);

		proxy = BKE_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
		{
			sculpt_orig_vert_data_update(&orig_data, &vd);

			if (sculpt_brush_test(&test, orig_data.co)) {
				float vec[3], rot[3][3];
				const float fade = bstrength * tex_strength(ss, brush,
				                                            orig_data.co,
				                                            test.dist,
				                                            orig_data.no,
				                                            NULL, vd.mask ? *vd.mask : 0.0f);

				sub_v3_v3v3(vec, orig_data.co, ss->cache->location);
				axis_angle_normalized_to_mat3(rot, ss->cache->sculpt_normal_symm, angle * fade);
				mul_v3_m3v3(proxy[vd.i], rot, vec);
				add_v3_v3(proxy[vd.i], ss->cache->location);
				sub_v3_v3(proxy[vd.i], orig_data.co);

				if (vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BKE_pbvh_vertex_iter_end;
	}
}

static void do_layer_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = BKE_paint_brush(&sd->paint);
	float bstrength = ss->cache->bstrength;
	float offset[3];
	float lim = brush->height;
	int n;

	if (bstrength < 0)
		lim = -lim;

	mul_v3_v3v3(offset, ss->cache->scale, ss->cache->sculpt_normal_symm);

#pragma omp parallel for schedule(guided) if ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_OMP_LIMIT)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		SculptOrigVertData orig_data;
		float *layer_disp;
		/* XXX: layer brush needs conversion to proxy but its more complicated */
		/* proxy = BKE_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co; */
		
		sculpt_orig_vert_data_init(&orig_data, ob, nodes[n]);

#pragma omp critical
		{
			layer_disp = BKE_pbvh_node_layer_disp_get(ss->pbvh, nodes[n]);
		}
		
		sculpt_brush_test_init(ss, &test);

		BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
		{
			sculpt_orig_vert_data_update(&orig_data, &vd);

			if (sculpt_brush_test(&test, orig_data.co)) {
				const float fade = bstrength * tex_strength(ss, brush, vd.co, test.dist,
				                                            vd.no, vd.fno, vd.mask ? *vd.mask : 0.0f);
				float *disp = &layer_disp[vd.i];
				float val[3];

				*disp += fade;

				/* Don't let the displacement go past the limit */
				if ((lim < 0 && *disp < lim) || (lim >= 0 && *disp > lim))
					*disp = lim;

				mul_v3_v3fl(val, offset, *disp);

				if (!ss->multires && !ss->bm && ss->layer_co && (brush->flag & BRUSH_PERSISTENT)) {
					int index = vd.vert_indices[vd.i];

					/* persistent base */
					add_v3_v3(val, ss->layer_co[index]);
				}
				else {
					add_v3_v3(val, orig_data.co);
				}

				sculpt_clip(sd, ss, vd.co, val);

				if (vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BKE_pbvh_vertex_iter_end;
	}
}

static void do_inflate_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = BKE_paint_brush(&sd->paint);
	float bstrength = ss->cache->bstrength;
	int n;

#pragma omp parallel for schedule(guided) if ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_OMP_LIMIT)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy = BKE_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
		{
			if (sculpt_brush_test(&test, vd.co)) {
				const float fade = bstrength * tex_strength(ss, brush, vd.co, test.dist,
				                                            vd.no, vd.fno, vd.mask ? *vd.mask : 0.0f);
				float val[3];

				if (vd.fno) copy_v3_v3(val, vd.fno);
				else normal_short_to_float_v3(val, vd.no);
				
				mul_v3_fl(val, fade * ss->cache->radius);
				mul_v3_v3v3(proxy[vd.i], val, ss->cache->scale);

				if (vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BKE_pbvh_vertex_iter_end;
	}
}

static void calc_flatten_center(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float fc[3])
{
	SculptSession *ss = ob->sculpt;
	int n;

	int count = 0;
	int count_flip = 0;

	float fc_flip[3] = {0.0, 0.0, 0.0};

	(void)sd; /* unused w/o openmp */

	zero_v3(fc);

#pragma omp parallel for schedule(guided) if ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_OMP_LIMIT)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		SculptUndoNode *unode;
		float private_fc[3] = {0.0f, 0.0f, 0.0f};
		float private_fc_flip[3] = {0.0f, 0.0f, 0.0f};
		int private_count = 0;
		int private_count_flip = 0;

		unode = sculpt_undo_push_node(ob, nodes[n], SCULPT_UNDO_COORDS);
		sculpt_brush_test_init(ss, &test);

		if (ss->cache->original && unode->co) {
			BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
			{
				if (sculpt_brush_test_fast(&test, unode->co[vd.i])) {
					float fno[3];

					normal_short_to_float_v3(fno, unode->no[vd.i]);
					if (dot_v3v3(ss->cache->view_normal, fno) > 0) {
						add_v3_v3(private_fc, unode->co[vd.i]);
						private_count++;
					}
					else {
						add_v3_v3(private_fc_flip, unode->co[vd.i]);
						private_count_flip++;
					}
				}
			}
			BKE_pbvh_vertex_iter_end;
		}
		else {
			BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
			{
				if (sculpt_brush_test_fast(&test, vd.co)) {
					/* for area normal */
					if (vd.no) {
						float fno[3];

						normal_short_to_float_v3(fno, vd.no);

						if (dot_v3v3(ss->cache->view_normal, fno) > 0) {
							add_v3_v3(private_fc, vd.co);
							private_count++;
						}
						else {
							add_v3_v3(private_fc_flip, vd.co);
							private_count_flip++;
						}
					}
					else {
						if (dot_v3v3(ss->cache->view_normal, vd.fno) > 0) {
							add_v3_v3(private_fc, vd.co);
							private_count++;
						}
						else {
							add_v3_v3(private_fc_flip, vd.co);
							private_count_flip++;
						}
					}
				}
			}
			BKE_pbvh_vertex_iter_end;
		}

#pragma omp critical
		{
			add_v3_v3(fc, private_fc);
			add_v3_v3(fc_flip, private_fc_flip);
			count += private_count;
			count_flip += private_count_flip;
		}
	}
	if (count != 0)
		mul_v3_fl(fc, 1.0f / count);
	else if (count_flip != 0)
		mul_v3_v3fl(fc, fc_flip, 1.0f / count_flip);
	else
		zero_v3(fc);
}

/* this calculates flatten center and area normal together, 
 * amortizing the memory bandwidth and loop overhead to calculate both at the same time */
static void calc_area_normal_and_flatten_center(Sculpt *sd, Object *ob,
                                                PBVHNode **nodes, int totnode,
                                                float an[3], float fc[3])
{
	SculptSession *ss = ob->sculpt;
	int n;

	/* for area normal */
	float out_flip[3] = {0.0f, 0.0f, 0.0f};
	float fc_flip[3] = {0.0f, 0.0f, 0.0f};

	/* for flatten center */
	int count = 0;
	int count_flipped = 0;

	(void)sd; /* unused w/o openmp */
	
	/* for area normal */
	zero_v3(an);

	/* for flatten center */
	zero_v3(fc);

#pragma omp parallel for schedule(guided) if ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_OMP_LIMIT)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		SculptUndoNode *unode;
		float private_an[3] = {0.0f, 0.0f, 0.0f};
		float private_out_flip[3] = {0.0f, 0.0f, 0.0f};
		float private_fc[3] = {0.0f, 0.0f, 0.0f};
		float private_fc_flip[3] = {0.0f, 0.0f, 0.0f};
		int private_count = 0;
		int private_count_flip = 0;

		unode = sculpt_undo_push_node(ob, nodes[n], SCULPT_UNDO_COORDS);
		sculpt_brush_test_init(ss, &test);

		if (ss->cache->original && unode->co) {
			BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
			{
				if (sculpt_brush_test_fast(&test, unode->co[vd.i])) {
					/* for area normal */
					float fno[3];

					normal_short_to_float_v3(fno, unode->no[vd.i]);

					if (dot_v3v3(ss->cache->view_normal, fno) > 0) {
						add_v3_v3(private_an, fno);
						add_v3_v3(private_fc, unode->co[vd.i]);
						private_count++;
					}
					else {
						add_v3_v3(private_out_flip, fno);
						add_v3_v3(private_fc_flip, unode->co[vd.i]);
						private_count_flip++;
					}
				}
			}
			BKE_pbvh_vertex_iter_end;
		}
		else {
			BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
			{
				if (sculpt_brush_test_fast(&test, vd.co)) {
					/* for area normal */
					if (vd.no) {
						float fno[3];

						normal_short_to_float_v3(fno, vd.no);

						if (dot_v3v3(ss->cache->view_normal, fno) > 0) {
							add_v3_v3(private_an, fno);
							add_v3_v3(private_fc, vd.co);
							private_count++;
						}
						else {
							add_v3_v3(private_out_flip, fno);
							add_v3_v3(private_fc_flip, vd.co);
							private_count_flip++;
						}
					}
					else {
						if (dot_v3v3(ss->cache->view_normal, vd.fno) > 0) {
							add_v3_v3(private_an, vd.fno);
							add_v3_v3(private_fc, vd.co);
							private_count++;
						}
						else {
							add_v3_v3(private_out_flip, vd.fno);
							add_v3_v3(private_fc_flip, vd.co);
							private_count_flip++;
						}
					}
				}
			}
			BKE_pbvh_vertex_iter_end;
		}

#pragma omp critical
		{
			/* for area normal */
			add_v3_v3(an, private_an);
			add_v3_v3(out_flip, private_out_flip);

			/* for flatten center */
			add_v3_v3(fc, private_fc);
			add_v3_v3(fc_flip, private_fc_flip);
			count += private_count;
			count_flipped += private_count_flip;
		}
	}

	/* for area normal */
	if (is_zero_v3(an))
		copy_v3_v3(an, out_flip);

	normalize_v3(an);

	/* for flatten center */
	if (count != 0)
		mul_v3_fl(fc, 1.0f / count);
	else if (count_flipped != 0)
		mul_v3_v3fl(fc, fc_flip, 1.0f / count_flipped);
	else
		zero_v3(fc);
}

static void calc_sculpt_plane(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float an[3], float fc[3])
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = BKE_paint_brush(&sd->paint);

	if (ss->cache->mirror_symmetry_pass == 0 &&
	    ss->cache->radial_symmetry_pass == 0 &&
	    (ss->cache->first_time || !(brush->flag & BRUSH_ORIGINAL_NORMAL)))
	{
		switch (brush->sculpt_plane) {
			case SCULPT_DISP_DIR_VIEW:
				copy_v3_v3(an, ss->cache->true_view_normal);
				break;

			case SCULPT_DISP_DIR_X:
				an[1] = 0.0;
				an[2] = 0.0;
				an[0] = 1.0;
				break;

			case SCULPT_DISP_DIR_Y:
				an[0] = 0.0;
				an[2] = 0.0;
				an[1] = 1.0;
				break;

			case SCULPT_DISP_DIR_Z:
				an[0] = 0.0;
				an[1] = 0.0;
				an[2] = 1.0;
				break;

			case SCULPT_DISP_DIR_AREA:
				calc_area_normal_and_flatten_center(sd, ob, nodes, totnode, an, fc);
				break;

			default:
				break;
		}

		/* for flatten center */
		/* flatten center has not been calculated yet if we are not using the area normal */
		if (brush->sculpt_plane != SCULPT_DISP_DIR_AREA)
			calc_flatten_center(sd, ob, nodes, totnode, fc);

		/* for area normal */
		copy_v3_v3(ss->cache->sculpt_normal, an);

		/* for flatten center */
		copy_v3_v3(ss->cache->last_center, fc);
	}
	else {
		/* for area normal */
		copy_v3_v3(an, ss->cache->sculpt_normal);

		/* for flatten center */
		copy_v3_v3(fc, ss->cache->last_center);

		/* for area normal */
		flip_v3(an, ss->cache->mirror_symmetry_pass);

		/* for flatten center */
		flip_v3(fc, ss->cache->mirror_symmetry_pass);

		/* for area normal */
		mul_m4_v3(ss->cache->symm_rot_mat, an);

		/* for flatten center */
		mul_m4_v3(ss->cache->symm_rot_mat, fc);
	}
}

/* Projects a point onto a plane along the plane's normal */
static void point_plane_project(float intr[3], float co[3], float plane_normal[3], float plane_center[3])
{
	sub_v3_v3v3(intr, co, plane_center);
	mul_v3_v3fl(intr, plane_normal, dot_v3v3(plane_normal, intr));
	sub_v3_v3v3(intr, co, intr);
}

static int plane_trim(StrokeCache *cache, Brush *brush, float val[3])
{
	return (!(brush->flag & BRUSH_PLANE_TRIM) ||
	        ((dot_v3v3(val, val) <= cache->radius_squared * cache->plane_trim_squared)));
}

static int plane_point_side_flip(float co[3], float plane_normal[3], float plane_center[3], int flip)
{
	float delta[3];
	float d;

	sub_v3_v3v3(delta, co, plane_center);
	d = dot_v3v3(plane_normal, delta);

	if (flip) d = -d;

	return d <= 0.0f;
}

static int plane_point_side(float co[3], float plane_normal[3], float plane_center[3])
{
	float delta[3];

	sub_v3_v3v3(delta, co, plane_center);
	return dot_v3v3(plane_normal, delta) <= 0.0f;
}

static float get_offset(Sculpt *sd, SculptSession *ss)
{
	Brush *brush = BKE_paint_brush(&sd->paint);

	float rv = brush->plane_offset;

	if (brush->flag & BRUSH_OFFSET_PRESSURE) {
		rv *= ss->cache->pressure;
	}

	return rv;
}

static void do_flatten_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = BKE_paint_brush(&sd->paint);

	float bstrength = ss->cache->bstrength;
	const float radius = ss->cache->radius;

	float an[3];
	float fc[3];

	float offset = get_offset(sd, ss);

	float displace;

	int n;

	float temp[3];

	calc_sculpt_plane(sd, ob, nodes, totnode, an, fc);

	displace = radius * offset;

	mul_v3_v3v3(temp, an, ss->cache->scale);
	mul_v3_fl(temp, displace);
	add_v3_v3(fc, temp);

#pragma omp parallel for schedule(guided) if ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_OMP_LIMIT)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy = BKE_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
		{
			if (sculpt_brush_test_sq(&test, vd.co)) {
				float intr[3];
				float val[3];

				point_plane_project(intr, vd.co, an, fc);

				sub_v3_v3v3(val, intr, vd.co);

				if (plane_trim(ss->cache, brush, val)) {
					const float fade = bstrength * tex_strength(ss, brush, vd.co, sqrtf(test.dist),
					                                            vd.no, vd.fno, vd.mask ? *vd.mask : 0.0f);

					mul_v3_v3fl(proxy[vd.i], val, fade);

					if (vd.mvert)
						vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
				}
			}
		}
		BKE_pbvh_vertex_iter_end;
	}
}

static void do_clay_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = BKE_paint_brush(&sd->paint);

	float bstrength = ss->cache->bstrength;
	float radius    = ss->cache->radius;
	float offset    = get_offset(sd, ss);
	
	float displace;

	float an[3];
	float fc[3];

	int n;

	float temp[3];

	int flip;

	calc_sculpt_plane(sd, ob, nodes, totnode, an, fc);

	flip = bstrength < 0;

	if (flip) {
		bstrength = -bstrength;
		radius    = -radius;
	}

	displace = radius * (0.25f + offset);

	mul_v3_v3v3(temp, an, ss->cache->scale);
	mul_v3_fl(temp, displace);
	add_v3_v3(fc, temp);

	/* add_v3_v3v3(p, ss->cache->location, an); */

#pragma omp parallel for schedule(guided) if ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_OMP_LIMIT)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy = BKE_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
		{
			if (sculpt_brush_test_sq(&test, vd.co)) {
				if (plane_point_side_flip(vd.co, an, fc, flip)) {
					float intr[3];
					float val[3];

					point_plane_project(intr, vd.co, an, fc);

					sub_v3_v3v3(val, intr, vd.co);

					if (plane_trim(ss->cache, brush, val)) {
						const float fade = bstrength * tex_strength(ss, brush, vd.co, sqrtf(test.dist),
						                                            vd.no, vd.fno, vd.mask ? *vd.mask : 0.0f);

						mul_v3_v3fl(proxy[vd.i], val, fade);

						if (vd.mvert)
							vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
					}
				}
			}
		}
		BKE_pbvh_vertex_iter_end;
	}
}

static void do_clay_strips_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = BKE_paint_brush(&sd->paint);

	float bstrength = ss->cache->bstrength;
	float radius    = ss->cache->radius;
	float offset    = get_offset(sd, ss);
	
	float displace;

	float sn[3];
	float an[3];
	float fc[3];

	int n;

	float temp[3];
	float mat[4][4];
	float scale[4][4];
	float tmat[4][4];

	int flip;

	calc_sculpt_plane(sd, ob, nodes, totnode, sn, fc);

	if (brush->sculpt_plane != SCULPT_DISP_DIR_AREA || (brush->flag & BRUSH_ORIGINAL_NORMAL))
		calc_area_normal(sd, ob, an, nodes, totnode);
	else
		copy_v3_v3(an, sn);

	/* delay the first daub because grab delta is not setup */
	if (ss->cache->first_time)
		return;

	flip = bstrength < 0;

	if (flip) {
		bstrength = -bstrength;
		radius    = -radius;
	}

	displace = radius * (0.25f + offset);

	mul_v3_v3v3(temp, sn, ss->cache->scale);
	mul_v3_fl(temp, displace);
	add_v3_v3(fc, temp);

	/* init mat */
	cross_v3_v3v3(mat[0], an, ss->cache->grab_delta_symmetry);
	mat[0][3] = 0;
	cross_v3_v3v3(mat[1], an, mat[0]);
	mat[1][3] = 0;
	copy_v3_v3(mat[2], an);
	mat[2][3] = 0;
	copy_v3_v3(mat[3], ss->cache->location);
	mat[3][3] = 1;
	normalize_m4(mat);

	/* scale mat */
	scale_m4_fl(scale, ss->cache->radius);
	mul_m4_m4m4(tmat, mat, scale);
	invert_m4_m4(mat, tmat);

#pragma omp parallel for schedule(guided) if ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_OMP_LIMIT)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy = BKE_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
		{
			if (sculpt_brush_test_cube(&test, vd.co, mat)) {
				if (plane_point_side_flip(vd.co, sn, fc, flip)) {
					float intr[3];
					float val[3];

					point_plane_project(intr, vd.co, sn, fc);

					sub_v3_v3v3(val, intr, vd.co);

					if (plane_trim(ss->cache, brush, val)) {
						const float fade = bstrength * tex_strength(ss, brush, vd.co,
						                                            ss->cache->radius * test.dist,
						                                            vd.no, vd.fno, vd.mask ? *vd.mask : 0.0f);

						mul_v3_v3fl(proxy[vd.i], val, fade);

						if (vd.mvert)
							vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
					}
				}
			}
		}
		BKE_pbvh_vertex_iter_end;
	}
}

static void do_fill_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = BKE_paint_brush(&sd->paint);

	float bstrength = ss->cache->bstrength;
	const float radius = ss->cache->radius;

	float an[3];
	float fc[3];
	float offset = get_offset(sd, ss);

	float displace;

	int n;

	float temp[3];

	calc_sculpt_plane(sd, ob, nodes, totnode, an, fc);

	displace = radius * offset;

	mul_v3_v3v3(temp, an, ss->cache->scale);
	mul_v3_fl(temp, displace);
	add_v3_v3(fc, temp);

#pragma omp parallel for schedule(guided) if ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_OMP_LIMIT)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy = BKE_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
		{
			if (sculpt_brush_test_sq(&test, vd.co)) {
				if (plane_point_side(vd.co, an, fc)) {
					float intr[3];
					float val[3];

					point_plane_project(intr, vd.co, an, fc);

					sub_v3_v3v3(val, intr, vd.co);

					if (plane_trim(ss->cache, brush, val)) {
						const float fade = bstrength * tex_strength(ss, brush, vd.co,
						                                            sqrtf(test.dist),
						                                            vd.no, vd.fno, vd.mask ? *vd.mask : 0.0f);

						mul_v3_v3fl(proxy[vd.i], val, fade);

						if (vd.mvert)
							vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
					}
				}
			}
		}
		BKE_pbvh_vertex_iter_end;
	}
}

static void do_scrape_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = BKE_paint_brush(&sd->paint);

	float bstrength = ss->cache->bstrength;
	const float radius = ss->cache->radius;

	float an[3];
	float fc[3];
	float offset = get_offset(sd, ss);

	float displace;

	int n;

	float temp[3];

	calc_sculpt_plane(sd, ob, nodes, totnode, an, fc);

	displace = -radius * offset;

	mul_v3_v3v3(temp, an, ss->cache->scale);
	mul_v3_fl(temp, displace);
	add_v3_v3(fc, temp);

#pragma omp parallel for schedule(guided) if ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_OMP_LIMIT)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy = BKE_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
		{
			if (sculpt_brush_test_sq(&test, vd.co)) {
				if (!plane_point_side(vd.co, an, fc)) {
					float intr[3];
					float val[3];

					point_plane_project(intr, vd.co, an, fc);

					sub_v3_v3v3(val, intr, vd.co);

					if (plane_trim(ss->cache, brush, val)) {
						const float fade = bstrength * tex_strength(ss, brush, vd.co,
						                                            sqrtf(test.dist),
						                                            vd.no, vd.fno, vd.mask ? *vd.mask : 0.0f);

						mul_v3_v3fl(proxy[vd.i], val, fade);

						if (vd.mvert)
							vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
					}
				}
			}
		}
		BKE_pbvh_vertex_iter_end;
	}
}

static void do_gravity(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float bstrength)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = BKE_paint_brush(&sd->paint);

	float offset[3]/*, an[3]*/;
	int n;
	float gravity_vector[3];

	mul_v3_v3fl(gravity_vector, ss->cache->gravity_direction, -ss->cache->radius_squared);

	/* offset with as much as possible factored in already */
	mul_v3_v3v3(offset, gravity_vector, ss->cache->scale);
	mul_v3_fl(offset, bstrength);

	/* threaded loop over nodes */
#pragma omp parallel for schedule(guided) if ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_OMP_LIMIT)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy = BKE_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if (sculpt_brush_test_sq(&test, vd.co)) {
				const float fade = tex_strength(ss, brush, vd.co, sqrtf(test.dist), vd.no,
				                                vd.fno, vd.mask ? *vd.mask : 0.0f);

				mul_v3_v3fl(proxy[vd.i], offset, fade);

				if (vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BKE_pbvh_vertex_iter_end;
	}
}


void sculpt_vertcos_to_key(Object *ob, KeyBlock *kb, float (*vertCos)[3])
{
	Mesh *me = (Mesh *)ob->data;
	float (*ofs)[3] = NULL;
	int a, is_basis = 0;
	KeyBlock *currkey;

	/* for relative keys editing of base should update other keys */
	if (me->key->type == KEY_RELATIVE)
		for (currkey = me->key->block.first; currkey; currkey = currkey->next)
			if (ob->shapenr - 1 == currkey->relative) {
				is_basis = 1;
				break;
			}

	if (is_basis) {
		ofs = BKE_key_convert_to_vertcos(ob, kb);

		/* calculate key coord offsets (from previous location) */
		for (a = 0; a < me->totvert; a++) {
			sub_v3_v3v3(ofs[a], vertCos[a], ofs[a]);
		}

		/* apply offsets on other keys */
		currkey = me->key->block.first;
		while (currkey) {
			int apply_offset = ((currkey != kb) && (ob->shapenr - 1 == currkey->relative));

			if (apply_offset)
				BKE_key_convert_from_offset(ob, currkey, ofs);

			currkey = currkey->next;
		}

		MEM_freeN(ofs);
	}

	/* modifying of basis key should update mesh */
	if (kb == me->key->refkey) {
		MVert *mvert = me->mvert;

		for (a = 0; a < me->totvert; a++, mvert++)
			copy_v3_v3(mvert->co, vertCos[a]);

		BKE_mesh_calc_normals(me);
	}

	/* apply new coords on active key block */
	BKE_key_convert_from_vertcos(ob, kb, vertCos);
}

/* Note: we do the topology update before any brush actions to avoid
 * issues with the proxies. The size of the proxy can't change, so
 * topology must be updated first. */
static void sculpt_topology_update(Sculpt *sd, Object *ob, Brush *brush)
{
	SculptSession *ss = ob->sculpt;
	SculptSearchSphereData data;
	PBVHNode **nodes = NULL;
	float radius;
	int n, totnode;

	/* Build a list of all nodes that are potentially within the
	 * brush's area of influence */
	data.ss = ss;
	data.sd = sd;

	radius = ss->cache->radius * 1.25f;

	data.radius_squared = radius * radius;
	data.original = ELEM(brush->sculpt_tool,
	                     SCULPT_TOOL_GRAB,
	                     SCULPT_TOOL_ROTATE,
	                     SCULPT_TOOL_THUMB,
	                     SCULPT_TOOL_LAYER) ? true : ss->cache->original;

	BKE_pbvh_search_gather(ss->pbvh, sculpt_search_sphere_cb, &data, &nodes, &totnode);

	/* Only act if some verts are inside the brush area */
	if (totnode) {
		PBVHTopologyUpdateMode mode = 0;
		float location[3];

		if (sd->flags & SCULPT_DYNTOPO_SUBDIVIDE)
			mode |= PBVH_Subdivide;

		if ((sd->flags & SCULPT_DYNTOPO_COLLAPSE) ||
		    (brush->sculpt_tool == SCULPT_TOOL_SIMPLIFY))
		{
			mode |= PBVH_Collapse;
		}

		for (n = 0; n < totnode; n++) {
			sculpt_undo_push_node(ob, nodes[n],
			                      brush->sculpt_tool == SCULPT_TOOL_MASK ?
			                      SCULPT_UNDO_MASK : SCULPT_UNDO_COORDS);
			BKE_pbvh_node_mark_update(nodes[n]);

			if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
				BKE_pbvh_node_mark_topology_update(nodes[n]);
				BKE_pbvh_bmesh_node_save_orig(nodes[n]);
			}
		}

		if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
			BKE_pbvh_bmesh_update_topology(ss->pbvh, mode,
			                               ss->cache->location,
			                               ss->cache->radius);
		}

		MEM_freeN(nodes);

		/* update average stroke position */
		copy_v3_v3(location, ss->cache->true_location);
		mul_m4_v3(ob->obmat, location);

		add_v3_v3(ob->sculpt->average_stroke_accum, location);
		ob->sculpt->average_stroke_counter++;
	}
}

static void do_brush_action(Sculpt *sd, Object *ob, Brush *brush)
{
	SculptSession *ss = ob->sculpt;
	SculptSearchSphereData data;
	PBVHNode **nodes = NULL;
	int n, totnode;

	/* Build a list of all nodes that are potentially within the brush's area of influence */
	data.ss = ss;
	data.sd = sd;
	data.radius_squared = ss->cache->radius_squared;
	data.original = ELEM(brush->sculpt_tool,
	                     SCULPT_TOOL_GRAB,
	                     SCULPT_TOOL_ROTATE,
	                     SCULPT_TOOL_THUMB,
	                     SCULPT_TOOL_LAYER) ? true : ss->cache->original;
	BKE_pbvh_search_gather(ss->pbvh, sculpt_search_sphere_cb, &data, &nodes, &totnode);

	/* Only act if some verts are inside the brush area */
	if (totnode) {
		float location[3];

#pragma omp parallel for schedule(guided) if ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_OMP_LIMIT)
		for (n = 0; n < totnode; n++) {
			sculpt_undo_push_node(ob, nodes[n],
			                      brush->sculpt_tool == SCULPT_TOOL_MASK ?
			                      SCULPT_UNDO_MASK : SCULPT_UNDO_COORDS);
			BKE_pbvh_node_mark_update(nodes[n]);
		}

		if (brush_needs_sculpt_normal(brush))
			update_sculpt_normal(sd, ob, nodes, totnode);

		if (brush->mtex.brush_map_mode == MTEX_MAP_MODE_AREA)
			update_brush_local_mat(sd, ob);

		/* Apply one type of brush action */
		switch (brush->sculpt_tool) {
			case SCULPT_TOOL_DRAW:
				do_draw_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_SMOOTH:
				do_smooth_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_CREASE:
				do_crease_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_BLOB:
				do_crease_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_PINCH:
				do_pinch_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_INFLATE:
				do_inflate_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_GRAB:
				do_grab_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_ROTATE:
				do_rotate_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_SNAKE_HOOK:
				do_snake_hook_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_NUDGE:
				do_nudge_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_THUMB:
				do_thumb_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_LAYER:
				do_layer_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_FLATTEN:
				do_flatten_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_CLAY:
				do_clay_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_CLAY_STRIPS:
				do_clay_strips_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_FILL:
				do_fill_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_SCRAPE:
				do_scrape_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_MASK:
				do_mask_brush(sd, ob, nodes, totnode);
				break;
		}

		if (!ELEM(brush->sculpt_tool, SCULPT_TOOL_SMOOTH, SCULPT_TOOL_MASK) &&
		    brush->autosmooth_factor > 0)
		{
			if (brush->flag & BRUSH_INVERSE_SMOOTH_PRESSURE) {
				smooth(sd, ob, nodes, totnode, brush->autosmooth_factor * (1 - ss->cache->pressure), false);
			}
			else {
				smooth(sd, ob, nodes, totnode, brush->autosmooth_factor, false);
			}
		}

		if (ss->cache->supports_gravity)
			do_gravity(sd, ob, nodes, totnode, sd->gravity_factor);

		MEM_freeN(nodes);

		/* update average stroke position */
		copy_v3_v3(location, ss->cache->true_location);
		mul_m4_v3(ob->obmat, location);

		add_v3_v3(ob->sculpt->average_stroke_accum, location);
		ob->sculpt->average_stroke_counter++;
	}
}

/* flush displacement from deformed PBVH vertex to original mesh */
static void sculpt_flush_pbvhvert_deform(Object *ob, PBVHVertexIter *vd)
{
	SculptSession *ss = ob->sculpt;
	Mesh *me = ob->data;
	float disp[3], newco[3];
	int index = vd->vert_indices[vd->i];

	sub_v3_v3v3(disp, vd->co, ss->deform_cos[index]);
	mul_m3_v3(ss->deform_imats[index], disp);
	add_v3_v3v3(newco, disp, ss->orig_cos[index]);

	copy_v3_v3(ss->deform_cos[index], vd->co);
	copy_v3_v3(ss->orig_cos[index], newco);

	if (!ss->kb)
		copy_v3_v3(me->mvert[index].co, newco);
}

static void sculpt_combine_proxies(Sculpt *sd, Object *ob)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = BKE_paint_brush(&sd->paint);
	PBVHNode **nodes;
	int totnode, n;

	BKE_pbvh_gather_proxies(ss->pbvh, &nodes, &totnode);

	/* first line is tools that don't support proxies */
	if (!ELEM(brush->sculpt_tool, SCULPT_TOOL_SMOOTH, SCULPT_TOOL_LAYER) ||
	    ss->cache->supports_gravity)
	{
		/* these brushes start from original coordinates */
		const bool use_orco = ELEM(brush->sculpt_tool, SCULPT_TOOL_GRAB,
		                           SCULPT_TOOL_ROTATE, SCULPT_TOOL_THUMB);

#pragma omp parallel for schedule(guided) if ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_OMP_LIMIT)
		for (n = 0; n < totnode; n++) {
			PBVHVertexIter vd;
			PBVHProxyNode *proxies;
			int proxy_count;
			float (*orco)[3] = NULL;

			if (use_orco && !ss->bm)
				orco = sculpt_undo_push_node(ob, nodes[n], SCULPT_UNDO_COORDS)->co;

			BKE_pbvh_node_get_proxies(nodes[n], &proxies, &proxy_count);

			BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
			{
				float val[3];
				int p;

				if (use_orco) {
					if (ss->bm) {
						copy_v3_v3(val,
						           BM_log_original_vert_co(ss->bm_log,
						           vd.bm_vert));
					}
					else
						copy_v3_v3(val, orco[vd.i]);
				}
				else
					copy_v3_v3(val, vd.co);

				for (p = 0; p < proxy_count; p++)
					add_v3_v3(val, proxies[p].co[vd.i]);

				sculpt_clip(sd, ss, vd.co, val);

				if (ss->modifiers_active)
					sculpt_flush_pbvhvert_deform(ob, &vd);
			}
			BKE_pbvh_vertex_iter_end;

			BKE_pbvh_node_free_proxies(nodes[n]);
		}
	}

	if (nodes)
		MEM_freeN(nodes);
}

/* copy the modified vertices from bvh to the active key */
static void sculpt_update_keyblock(Object *ob)
{
	SculptSession *ss = ob->sculpt;
	float (*vertCos)[3];

	/* Keyblock update happens after handling deformation caused by modifiers,
	 * so ss->orig_cos would be updated with new stroke */
	if (ss->orig_cos) vertCos = ss->orig_cos;
	else vertCos = BKE_pbvh_get_vertCos(ss->pbvh);

	if (vertCos) {
		sculpt_vertcos_to_key(ob, ss->kb, vertCos);

		if (vertCos != ss->orig_cos)
			MEM_freeN(vertCos);
	}
}

/* flush displacement from deformed PBVH to original layer */
static void sculpt_flush_stroke_deform(Sculpt *sd, Object *ob)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = BKE_paint_brush(&sd->paint);

	if (ELEM(brush->sculpt_tool, SCULPT_TOOL_SMOOTH, SCULPT_TOOL_LAYER)) {
		/* this brushes aren't using proxies, so sculpt_combine_proxies() wouldn't
		 * propagate needed deformation to original base */

		int n, totnode;
		Mesh *me = (Mesh *)ob->data;
		PBVHNode **nodes;
		float (*vertCos)[3] = NULL;

		if (ss->kb) {
			vertCos = MEM_mallocN(sizeof(*vertCos) * me->totvert, "flushStrokeDeofrm keyVerts");

			/* mesh could have isolated verts which wouldn't be in BVH,
			 * to deal with this we copy old coordinates over new ones
			 * and then update coordinates for all vertices from BVH
			 */
			memcpy(vertCos, ss->orig_cos, 3 * sizeof(float) * me->totvert);
		}

		BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);

#pragma omp parallel for schedule(guided) if ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_OMP_LIMIT)
		for (n = 0; n < totnode; n++) {
			PBVHVertexIter vd;

			BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
			{
				sculpt_flush_pbvhvert_deform(ob, &vd);

				if (vertCos) {
					int index = vd.vert_indices[vd.i];
					copy_v3_v3(vertCos[index], ss->orig_cos[index]);
				}
			}
			BKE_pbvh_vertex_iter_end;
		}

		if (vertCos) {
			sculpt_vertcos_to_key(ob, ss->kb, vertCos);
			MEM_freeN(vertCos);
		}

		MEM_freeN(nodes);

		/* Modifiers could depend on mesh normals, so we should update them/
		 * Note, then if sculpting happens on locked key, normals should be re-calculated
		 * after applying coords from keyblock on base mesh */
		BKE_mesh_calc_normals(me);
	}
	else if (ss->kb) {
		sculpt_update_keyblock(ob);
	}
}

/* Flip all the editdata across the axis/axes specified by symm. Used to
 * calculate multiple modifications to the mesh when symmetry is enabled. */
static void calc_brushdata_symm(Sculpt *sd, StrokeCache *cache, const char symm,
                                const char axis, const float angle,
                                const float UNUSED(feather))
{
	(void)sd; /* unused */

	flip_v3_v3(cache->location, cache->true_location, symm);
	flip_v3_v3(cache->grab_delta_symmetry, cache->grab_delta, symm);
	flip_v3_v3(cache->view_normal, cache->true_view_normal, symm);

	/* XXX This reduces the length of the grab delta if it approaches the line of symmetry
	 * XXX However, a different approach appears to be needed */
#if 0
	if (sd->paint.symmetry_flags & SCULPT_SYMMETRY_FEATHER) {
		float frac = 1.0f / max_overlap_count(sd);
		float reduce = (feather - frac) / (1 - frac);

		printf("feather: %f frac: %f reduce: %f\n", feather, frac, reduce);

		if (frac < 1)
			mul_v3_fl(cache->grab_delta_symmetry, reduce);
	}
#endif

	unit_m4(cache->symm_rot_mat);
	unit_m4(cache->symm_rot_mat_inv);

	if (axis) { /* expects XYZ */
		rotate_m4(cache->symm_rot_mat, axis, angle);
		rotate_m4(cache->symm_rot_mat_inv, axis, -angle);
	}

	mul_m4_v3(cache->symm_rot_mat, cache->location);
	mul_m4_v3(cache->symm_rot_mat, cache->grab_delta_symmetry);

	if (cache->supports_gravity) {
		flip_v3_v3(cache->gravity_direction, cache->true_gravity_direction, symm);
		mul_m4_v3(cache->symm_rot_mat, cache->gravity_direction);
	}
}

typedef void (*BrushActionFunc)(Sculpt *sd, Object *ob, Brush *brush);

static void do_radial_symmetry(Sculpt *sd, Object *ob, Brush *brush,
                               BrushActionFunc action,
                               const char symm, const int axis,
                               const float feather)
{
	SculptSession *ss = ob->sculpt;
	int i;

	for (i = 1; i < sd->radial_symm[axis - 'X']; ++i) {
		const float angle = 2 * M_PI * i / sd->radial_symm[axis - 'X'];
		ss->cache->radial_symmetry_pass = i;
		calc_brushdata_symm(sd, ss->cache, symm, axis, angle, feather);
		action(sd, ob, brush);
	}
}

/* noise texture gives different values for the same input coord; this
 * can tear a multires mesh during sculpting so do a stitch in this
 * case */
static void sculpt_fix_noise_tear(Sculpt *sd, Object *ob)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = BKE_paint_brush(&sd->paint);
	MTex *mtex = &brush->mtex;

	if (ss->multires && mtex->tex && mtex->tex->type == TEX_NOISE)
		multires_stitch_grids(ob);
}

static void do_symmetrical_brush_actions(Sculpt *sd, Object *ob,
                                         BrushActionFunc action, UnifiedPaintSettings *ups)
{
	Brush *brush = BKE_paint_brush(&sd->paint);
	SculptSession *ss = ob->sculpt;
	StrokeCache *cache = ss->cache;
	const char symm = sd->paint.symmetry_flags & PAINT_SYMM_AXIS_ALL;
	int i;

	float feather = calc_symmetry_feather(sd, ss->cache);

	cache->bstrength = brush_strength(sd, cache, feather, ups);
	cache->symmetry = symm;

	/* symm is a bit combination of XYZ - 1 is mirror X; 2 is Y; 3 is XY; 4 is Z; 5 is XZ; 6 is YZ; 7 is XYZ */ 
	for (i = 0; i <= symm; ++i) {
		if (i == 0 || (symm & i && (symm != 5 || i != 3) && (symm != 6 || (i != 3 && i != 5)))) {
			cache->mirror_symmetry_pass = i;
			cache->radial_symmetry_pass = 0;

			calc_brushdata_symm(sd, cache, i, 0, 0, feather);
			action(sd, ob, brush);

			do_radial_symmetry(sd, ob, brush, action, i, 'X', feather);
			do_radial_symmetry(sd, ob, brush, action, i, 'Y', feather);
			do_radial_symmetry(sd, ob, brush, action, i, 'Z', feather);
		}
	}
}

static void sculpt_update_tex(const Scene *scene, Sculpt *sd, SculptSession *ss)
{
	Brush *brush = BKE_paint_brush(&sd->paint);
	const int radius = BKE_brush_size_get(scene, brush);

	if (ss->texcache) {
		MEM_freeN(ss->texcache);
		ss->texcache = NULL;
	}

	if (ss->tex_pool) {
		BKE_image_pool_free(ss->tex_pool);
		ss->tex_pool = NULL;
	}

	/* Need to allocate a bigger buffer for bigger brush size */
	ss->texcache_side = 2 * radius;
	if (!ss->texcache || ss->texcache_side > ss->texcache_actual) {
		ss->texcache = BKE_brush_gen_texture_cache(brush, radius, false);
		ss->texcache_actual = ss->texcache_side;
		ss->tex_pool = BKE_image_pool_new();
	}
}



int sculpt_mode_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);
	return ob && ob->mode & OB_MODE_SCULPT;
}

int sculpt_mode_poll_view3d(bContext *C)
{
	return (sculpt_mode_poll(C) &&
	        CTX_wm_region_view3d(C));
}

int sculpt_poll_view3d(bContext *C)
{
	return (sculpt_poll(C) &&
	        CTX_wm_region_view3d(C));
}

int sculpt_poll(bContext *C)
{
	return sculpt_mode_poll(C) && paint_poll(C);
}

static const char *sculpt_tool_name(Sculpt *sd)
{
	Brush *brush = BKE_paint_brush(&sd->paint);

	switch ((BrushSculptTool)brush->sculpt_tool) {
		case SCULPT_TOOL_DRAW:
			return "Draw Brush";
		case SCULPT_TOOL_SMOOTH:
			return "Smooth Brush";
		case SCULPT_TOOL_CREASE:
			return "Crease Brush";
		case SCULPT_TOOL_BLOB:
			return "Blob Brush";
		case SCULPT_TOOL_PINCH:
			return "Pinch Brush";
		case SCULPT_TOOL_INFLATE:
			return "Inflate Brush";
		case SCULPT_TOOL_GRAB:
			return "Grab Brush";
		case SCULPT_TOOL_NUDGE:
			return "Nudge Brush";
		case SCULPT_TOOL_THUMB:
			return "Thumb Brush";
		case SCULPT_TOOL_LAYER:
			return "Layer Brush";
		case SCULPT_TOOL_FLATTEN:
			return "Flatten Brush";
		case SCULPT_TOOL_CLAY:
			return "Clay Brush";
		case SCULPT_TOOL_CLAY_STRIPS:
			return "Clay Strips Brush";
		case SCULPT_TOOL_FILL:
			return "Fill Brush";
		case SCULPT_TOOL_SCRAPE:
			return "Scrape Brush";
		case SCULPT_TOOL_SNAKE_HOOK:
			return "Snake Hook Brush";
		case SCULPT_TOOL_ROTATE:
			return "Rotate Brush";
		case SCULPT_TOOL_MASK:
			return "Mask Brush";
		case SCULPT_TOOL_SIMPLIFY:
			return "Simplify Brush";
	}

	return "Sculpting";
}

/**
 * Operator for applying a stroke (various attributes including mouse path)
 * using the current brush. */

static void sculpt_cache_free(StrokeCache *cache)
{
	if (cache->face_norms)
		MEM_freeN(cache->face_norms);
	if (cache->dial)
		MEM_freeN(cache->dial);
	MEM_freeN(cache);
}

/* Initialize mirror modifier clipping */
static void sculpt_init_mirror_clipping(Object *ob, SculptSession *ss)
{
	ModifierData *md;
	int i;

	for (md = ob->modifiers.first; md; md = md->next) {
		if (md->type == eModifierType_Mirror &&
		    (md->mode & eModifierMode_Realtime))
		{
			MirrorModifierData *mmd = (MirrorModifierData *)md;
			
			if (mmd->flag & MOD_MIR_CLIPPING) {
				/* check each axis for mirroring */
				for (i = 0; i < 3; ++i) {
					if (mmd->flag & (MOD_MIR_AXIS_X << i)) {
						/* enable sculpt clipping */
						ss->cache->flag |= CLIP_X << i;
						
						/* update the clip tolerance */
						if (mmd->tolerance >
						    ss->cache->clip_tolerance[i])
						{
							ss->cache->clip_tolerance[i] =
							    mmd->tolerance;
						}
					}
				}
			}
		}
	}
}

static void sculpt_omp_start(Sculpt *sd, SculptSession *ss)
{
	StrokeCache *cache = ss->cache;

#ifdef _OPENMP

#if defined(__APPLE__)
	cache->init_num_threads = BLI_system_thread_count();
#else
	cache->init_num_threads = omp_get_max_threads();
#endif
	/* If using OpenMP then create a number of threads two times the
	 * number of processor cores.
	 * Justification: Empirically I've found that two threads per
	 * processor gives higher throughput. */
	if (sd->flags & SCULPT_USE_OPENMP) {
#if defined(__APPLE__)
		cache->num_threads = system_physical_thread_count();
#else
		cache->num_threads = 2 * omp_get_num_procs();
#endif
	}
	else {
		cache->num_threads = 1;
	}
	omp_set_num_threads(cache->num_threads);
#else
	(void)sd;
	cache->num_threads = 1;
#endif
	if (ss->multires) {
		int i, gridsize, array_mem_size;
		BKE_pbvh_node_get_grids(ss->pbvh, NULL, NULL, NULL, NULL,
		                        &gridsize, NULL, NULL);

		array_mem_size = cache->num_threads * sizeof(void *);

		cache->tmpgrid_co = MEM_mallocN(array_mem_size, "tmpgrid_co array");
		cache->tmprow_co = MEM_mallocN(array_mem_size, "tmprow_co array");
		cache->tmpgrid_mask = MEM_mallocN(array_mem_size, "tmpgrid_mask array");
		cache->tmprow_mask = MEM_mallocN(array_mem_size, "tmprow_mask array");

		for (i = 0; i < cache->num_threads; i++) {
			const size_t row_size = sizeof(float) * gridsize;
			const size_t co_row_size = 3 * row_size;

			cache->tmprow_co[i] = MEM_mallocN(co_row_size, "tmprow_co");
			cache->tmpgrid_co[i] = MEM_mallocN(co_row_size * gridsize, "tmpgrid_co");
			cache->tmprow_mask[i] = MEM_mallocN(row_size, "tmprow_mask");
			cache->tmpgrid_mask[i] = MEM_mallocN(row_size * gridsize, "tmpgrid_mask");
		}
	}
}

static void sculpt_omp_done(SculptSession *ss)
{
#ifdef _OPENMP
	omp_set_num_threads(ss->cache->init_num_threads);
#endif

	if (ss->multires) {
		int i;

		for (i = 0; i < ss->cache->num_threads; i++) {
			MEM_freeN(ss->cache->tmpgrid_co[i]);
			MEM_freeN(ss->cache->tmprow_co[i]);
			MEM_freeN(ss->cache->tmpgrid_mask[i]);
			MEM_freeN(ss->cache->tmprow_mask[i]);
		}

		MEM_freeN(ss->cache->tmpgrid_co);
		MEM_freeN(ss->cache->tmprow_co);
		MEM_freeN(ss->cache->tmpgrid_mask);
		MEM_freeN(ss->cache->tmprow_mask);
	}
}

/* Initialize the stroke cache invariants from operator properties */
static void sculpt_update_cache_invariants(bContext *C, Sculpt *sd, SculptSession *ss, wmOperator *op, const float mouse[2])
{
	StrokeCache *cache = MEM_callocN(sizeof(StrokeCache), "stroke cache");
	Scene *scene = CTX_data_scene(C);
	UnifiedPaintSettings *ups = &CTX_data_tool_settings(C)->unified_paint_settings;
	Brush *brush = BKE_paint_brush(&sd->paint);
	ViewContext *vc = paint_stroke_view_context(op->customdata);
	Object *ob = CTX_data_active_object(C);
	float mat[3][3];
	float viewDir[3] = {0.0f, 0.0f, 1.0f};
	float max_scale;
	int i;
	int mode;

	ss->cache = cache;

	/* Set scaling adjustment */
	if (brush->sculpt_tool == SCULPT_TOOL_LAYER) {
		max_scale = 1.0f;
	}
	else {
		max_scale = 0.0f;
		for (i = 0; i < 3; i ++) {
			max_scale = max_ff(max_scale, fabsf(ob->size[i]));
		}
	}
	cache->scale[0] = max_scale / ob->size[0];
	cache->scale[1] = max_scale / ob->size[1];
	cache->scale[2] = max_scale / ob->size[2];

	cache->plane_trim_squared = brush->plane_trim * brush->plane_trim;

	cache->flag = 0;

	sculpt_init_mirror_clipping(ob, ss);

	/* Initial mouse location */
	if (mouse)
		copy_v2_v2(cache->initial_mouse, mouse);
	else
		zero_v2(cache->initial_mouse);

	mode = RNA_enum_get(op->ptr, "mode");
	cache->invert = mode == BRUSH_STROKE_INVERT;
	cache->alt_smooth = mode == BRUSH_STROKE_SMOOTH;

	/* not very nice, but with current events system implementation
	 * we can't handle brush appearance inversion hotkey separately (sergey) */
	if (cache->invert) ups->draw_inverted = true;
	else ups->draw_inverted = false;

	/* Alt-Smooth */
	if (cache->alt_smooth) {
		if (brush->sculpt_tool == SCULPT_TOOL_MASK) {
			cache->saved_mask_brush_tool = brush->mask_tool;
			brush->mask_tool = BRUSH_MASK_SMOOTH;
		}
		else {
			Paint *p = &sd->paint;
			Brush *br;
			int size = BKE_brush_size_get(scene, brush);
		
			BLI_strncpy(cache->saved_active_brush_name, brush->id.name + 2,
			            sizeof(cache->saved_active_brush_name));

			br = (Brush *)BKE_libblock_find_name(ID_BR, "Smooth");
			if (br) {
				BKE_paint_brush_set(p, br);
				brush = br;
				cache->saved_smooth_size = BKE_brush_size_get(scene, brush);
				BKE_brush_size_set(scene, brush, size);
				curvemapping_initialize(brush->curve);
			}
		}
	}

	copy_v2_v2(cache->mouse, cache->initial_mouse);
	copy_v2_v2(ups->tex_mouse, cache->initial_mouse);

	/* Truly temporary data that isn't stored in properties */

	cache->vc = vc;

	cache->brush = brush;

	/* cache projection matrix */
	ED_view3d_ob_project_mat_get(cache->vc->rv3d, ob, cache->projection_mat);

	invert_m4_m4(ob->imat, ob->obmat);
	copy_m3_m4(mat, cache->vc->rv3d->viewinv);
	mul_m3_v3(mat, viewDir);
	copy_m3_m4(mat, ob->imat);
	mul_m3_v3(mat, viewDir);
	normalize_v3_v3(cache->true_view_normal, viewDir);

	cache->supports_gravity = (!ELEM(brush->sculpt_tool, SCULPT_TOOL_MASK, SCULPT_TOOL_SMOOTH, SCULPT_TOOL_SIMPLIFY) &&
	                           (sd->gravity_factor > 0.0f));
	/* get gravity vector in world space */
	if (cache->supports_gravity) {
		if (sd->gravity_object) {
			Object *gravity_object = sd->gravity_object;

			copy_v3_v3(cache->true_gravity_direction, gravity_object->obmat[2]);
		}
		else {
			cache->true_gravity_direction[0] = cache->true_gravity_direction[1] = 0.0;
			cache->true_gravity_direction[2] = 1.0;
		}

		/* transform to sculpted object space */
		mul_m3_v3(mat, cache->true_gravity_direction);
		normalize_v3(cache->true_gravity_direction);
	}

	/* Initialize layer brush displacements and persistent coords */
	if (brush->sculpt_tool == SCULPT_TOOL_LAYER) {
		/* not supported yet for multires or dynamic topology */
		if (!ss->multires && !ss->bm && !ss->layer_co &&
		    (brush->flag & BRUSH_PERSISTENT))
		{
			if (!ss->layer_co)
				ss->layer_co = MEM_mallocN(sizeof(float) * 3 * ss->totvert,
				                           "sculpt mesh vertices copy");

			if (ss->deform_cos) {
				memcpy(ss->layer_co, ss->deform_cos, ss->totvert);
			}
			else {
				for (i = 0; i < ss->totvert; ++i) {
					copy_v3_v3(ss->layer_co[i], ss->mvert[i].co);
				}
			}
		}

		if (ss->bm) {
			/* Free any remaining layer displacements from nodes. If not and topology changes
			 * from using another tool, then next layer toolstroke can access past disp array bounds */
			BKE_pbvh_free_layer_disp(ss->pbvh);
		}
	}

	/* Make copies of the mesh vertex locations and normals for some tools */
	if (brush->flag & BRUSH_ANCHORED) {
		if (ss->face_normals) {
			cache->face_norms = MEM_mallocN(sizeof(float) * 3 * ss->totpoly, "Sculpt face norms");
			for (i = 0; i < ss->totpoly; ++i) {
				copy_v3_v3(cache->face_norms[i], ss->face_normals[i]);
			}
		}

		cache->original = 1;
	}

	if (ELEM(brush->sculpt_tool,
	         SCULPT_TOOL_DRAW, SCULPT_TOOL_CREASE, SCULPT_TOOL_BLOB,
	         SCULPT_TOOL_LAYER, SCULPT_TOOL_INFLATE, SCULPT_TOOL_CLAY,
	         SCULPT_TOOL_CLAY_STRIPS, SCULPT_TOOL_ROTATE, SCULPT_TOOL_FLATTEN))
	{
		if (!(brush->flag & BRUSH_ACCUMULATE)) {
			cache->original = 1;
		}
	}

	cache->first_time = 1;

#define PIXEL_INPUT_THRESHHOLD 5
	if (brush->sculpt_tool == SCULPT_TOOL_ROTATE)
		cache->dial = BLI_dial_initialize(cache->initial_mouse, PIXEL_INPUT_THRESHHOLD);
		
#undef PIXEL_INPUT_THRESHHOLD
	
	sculpt_omp_start(sd, ss);
}

static void sculpt_update_brush_delta(UnifiedPaintSettings *ups, Object *ob, Brush *brush)
{
	SculptSession *ss = ob->sculpt;
	StrokeCache *cache = ss->cache;
	const float mouse[2] = {
		cache->mouse[0],
		cache->mouse[1]
	};
	int tool = brush->sculpt_tool;

	if (ELEM(tool,
	         SCULPT_TOOL_GRAB, SCULPT_TOOL_NUDGE,
	         SCULPT_TOOL_CLAY_STRIPS, SCULPT_TOOL_SNAKE_HOOK,
	         SCULPT_TOOL_THUMB))
	{
		float grab_location[3], imat[4][4], delta[3], loc[3];

		if (cache->first_time) {
			copy_v3_v3(cache->orig_grab_location,
			           cache->true_location);
		}
		else if (tool == SCULPT_TOOL_SNAKE_HOOK)
			add_v3_v3(cache->true_location, cache->grab_delta);

		/* compute 3d coordinate at same z from original location + mouse */
		mul_v3_m4v3(loc, ob->obmat, cache->orig_grab_location);
		ED_view3d_win_to_3d(cache->vc->ar, loc, mouse, grab_location);

		/* compute delta to move verts by */
		if (!cache->first_time) {
			switch (tool) {
				case SCULPT_TOOL_GRAB:
				case SCULPT_TOOL_THUMB:
					sub_v3_v3v3(delta, grab_location, cache->old_grab_location);
					invert_m4_m4(imat, ob->obmat);
					mul_mat3_m4_v3(imat, delta);
					add_v3_v3(cache->grab_delta, delta);
					break;
				case SCULPT_TOOL_CLAY_STRIPS:
				case SCULPT_TOOL_NUDGE:
				case SCULPT_TOOL_SNAKE_HOOK:
					if (brush->flag & BRUSH_ANCHORED) {
						float orig[3];
						mul_v3_m4v3(orig, ob->obmat, cache->orig_grab_location);
						sub_v3_v3v3(cache->grab_delta, grab_location, orig);
					}
					else {
						sub_v3_v3v3(cache->grab_delta, grab_location,
						            cache->old_grab_location);
					}
				
					invert_m4_m4(imat, ob->obmat);
					mul_mat3_m4_v3(imat, cache->grab_delta);
					break;
			}
		}
		else {
			zero_v3(cache->grab_delta);
		}

		copy_v3_v3(cache->old_grab_location, grab_location);

		if (tool == SCULPT_TOOL_GRAB)
			copy_v3_v3(cache->anchored_location, cache->true_location);
		else if (tool == SCULPT_TOOL_THUMB)
			copy_v3_v3(cache->anchored_location, cache->orig_grab_location);

		if (ELEM(tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_THUMB)) {
			/* location stays the same for finding vertices in brush radius */
			copy_v3_v3(cache->true_location, cache->orig_grab_location);

			ups->draw_anchored = true;
			copy_v2_v2(ups->anchored_initial_mouse, cache->initial_mouse);
			ups->anchored_size = ups->pixel_radius;
		}
	}
}

/* Initialize the stroke cache variants from operator properties */
static void sculpt_update_cache_variants(bContext *C, Sculpt *sd, Object *ob,
                                         PointerRNA *ptr)
{
	Scene *scene = CTX_data_scene(C);
	UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
	SculptSession *ss = ob->sculpt;
	StrokeCache *cache = ss->cache;
	Brush *brush = BKE_paint_brush(&sd->paint);

	/* RNA_float_get_array(ptr, "location", cache->traced_location); */

	if (cache->first_time ||
	    !((brush->flag & BRUSH_ANCHORED) ||
	      (brush->sculpt_tool == SCULPT_TOOL_SNAKE_HOOK) ||
	      (brush->sculpt_tool == SCULPT_TOOL_ROTATE))
	    )
	{
		RNA_float_get_array(ptr, "location", cache->true_location);
	}

	cache->pen_flip = RNA_boolean_get(ptr, "pen_flip");
	RNA_float_get_array(ptr, "mouse", cache->mouse);

	/* XXX: Use pressure value from first brush step for brushes which don't
	 *      support strokes (grab, thumb). They depends on initial state and
	 *      brush coord/pressure/etc.
	 *      It's more an events design issue, which doesn't split coordinate/pressure/angle
	 *      changing events. We should avoid this after events system re-design */
	if (paint_supports_dynamic_size(brush, PAINT_SCULPT) || cache->first_time) {
		cache->pressure = RNA_float_get(ptr, "pressure");
	}

	/* Truly temporary data that isn't stored in properties */
	if (cache->first_time) {
		if (!BKE_brush_use_locked_size(scene, brush)) {
			cache->initial_radius = paint_calc_object_space_radius(cache->vc,
			                                                       cache->true_location,
			                                                       BKE_brush_size_get(scene, brush));
			BKE_brush_unprojected_radius_set(scene, brush, cache->initial_radius);
		}
		else {
			cache->initial_radius = BKE_brush_unprojected_radius_get(scene, brush);
		}
	}

	if (BKE_brush_use_size_pressure(scene, brush) && paint_supports_dynamic_size(brush, PAINT_SCULPT)) {
		cache->radius = cache->initial_radius * cache->pressure;
	}
	else {
		cache->radius = cache->initial_radius;
	}

	cache->radius_squared = cache->radius * cache->radius;

	if (brush->flag & BRUSH_ANCHORED) {
		/* true location has been calculated as part of the stroke system already here */
		if (brush->flag & BRUSH_EDGE_TO_EDGE) {
			RNA_float_get_array(ptr, "location", cache->true_location);
		}

		cache->radius = paint_calc_object_space_radius(cache->vc,
		                                               cache->true_location,
		                                               ups->pixel_radius);
		cache->radius_squared = cache->radius * cache->radius;

		copy_v3_v3(cache->anchored_location, cache->true_location);
	}

	sculpt_update_brush_delta(ups, ob, brush);

	if (brush->sculpt_tool == SCULPT_TOOL_ROTATE) {
		cache->vertex_rotation = -BLI_dial_angle(cache->dial, cache->mouse) * cache->bstrength;

		ups->draw_anchored = true;
		copy_v2_v2(ups->anchored_initial_mouse, cache->initial_mouse);
		copy_v3_v3(cache->anchored_location, cache->true_location);
		ups->anchored_size = ups->pixel_radius;
	}

	cache->special_rotation = ups->brush_rotation;
}

/* Returns true if any of the smoothing modes are active (currently
 * one of smooth brush, autosmooth, mask smooth, or shift-key
 * smooth) */
static bool sculpt_any_smooth_mode(const Brush *brush,
                                   StrokeCache *cache,
                                   int stroke_mode)
{
	return ((stroke_mode == BRUSH_STROKE_SMOOTH) ||
	        (cache && cache->alt_smooth) ||
	        (brush->sculpt_tool == SCULPT_TOOL_SMOOTH) ||
	        (brush->autosmooth_factor > 0) ||
	        ((brush->sculpt_tool == SCULPT_TOOL_MASK) &&
	         (brush->mask_tool == BRUSH_MASK_SMOOTH)));
}

static void sculpt_stroke_modifiers_check(const bContext *C, Object *ob)
{
	SculptSession *ss = ob->sculpt;

	if (ss->kb || ss->modifiers_active) {
		Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
		Brush *brush = BKE_paint_brush(&sd->paint);

		BKE_sculpt_update_mesh_elements(CTX_data_scene(C), sd, ob,
		                            sculpt_any_smooth_mode(brush, ss->cache, 0), false);
	}
}

typedef struct {
	SculptSession *ss;
	const float *ray_start, *ray_normal;
	bool hit;
	float dist;
	int original;
} SculptRaycastData;

typedef struct {
	const float *ray_start, *ray_normal;
	bool hit;
	float dist;
	float detail;
} SculptDetailRaycastData;

static void sculpt_raycast_cb(PBVHNode *node, void *data_v, float *tmin)
{
	if (BKE_pbvh_node_get_tmin(node) < *tmin) {
		SculptRaycastData *srd = data_v;
		float (*origco)[3] = NULL;
		bool use_origco = false;

		if (srd->original && srd->ss->cache) {
			if (BKE_pbvh_type(srd->ss->pbvh) == PBVH_BMESH) {
				use_origco = true;
			}
			else {
				/* intersect with coordinates from before we started stroke */
				SculptUndoNode *unode = sculpt_undo_get_node(node);
				origco = (unode) ? unode->co : NULL;
				use_origco = origco ? true : false;
			}
		}

		if (BKE_pbvh_node_raycast(srd->ss->pbvh, node, origco, use_origco,
		                          srd->ray_start, srd->ray_normal, &srd->dist))
		{
			srd->hit = 1;
			*tmin = srd->dist;
		}
	}
}

static void sculpt_raycast_detail_cb(PBVHNode *node, void *data_v, float *tmin)
{
	if (BKE_pbvh_node_get_tmin(node) < *tmin) {
		SculptDetailRaycastData *srd = data_v;
		if (BKE_pbvh_bmesh_node_raycast_detail(node, srd->ray_start, srd->ray_normal,
		                                       &srd->detail, &srd->dist))
		{
			srd->hit = 1;
			*tmin = srd->dist;
		}
	}
}

static float sculpt_raycast_init(ViewContext *vc, const float mouse[2], float ray_start[3], float ray_end[3], float ray_normal[3], bool original)
{
	float obimat[4][4];
	float dist;
	Object *ob = vc->obact;
	RegionView3D *rv3d = vc->ar->regiondata;

	/* TODO: what if the segment is totally clipped? (return == 0) */
	ED_view3d_win_to_segment(vc->ar, vc->v3d, mouse, ray_start, ray_end, true);

	invert_m4_m4(obimat, ob->obmat);
	mul_m4_v3(obimat, ray_start);
	mul_m4_v3(obimat, ray_end);

	sub_v3_v3v3(ray_normal, ray_end, ray_start);
	dist = normalize_v3(ray_normal);

	if (!rv3d->is_persp) {
		BKE_pbvh_raycast_project_ray_root(ob->sculpt->pbvh, original, ray_start, ray_end, ray_normal);

		/* recalculate the normal */
		sub_v3_v3v3(ray_normal, ray_end, ray_start);
		dist = normalize_v3(ray_normal);
	}

	return dist;
}

/* Do a raycast in the tree to find the 3d brush location
 * (This allows us to ignore the GL depth buffer)
 * Returns 0 if the ray doesn't hit the mesh, non-zero otherwise
 */
bool sculpt_stroke_get_location(bContext *C, float out[3], const float mouse[2])
{
	Object *ob;
	SculptSession *ss;
	StrokeCache *cache;
	float ray_start[3], ray_end[3], ray_normal[3], dist;
	SculptRaycastData srd;
	bool original;
	ViewContext vc;

	view3d_set_viewcontext(C, &vc);

	ob = vc.obact;

	ss = ob->sculpt;
	cache = ss->cache;
	original = (cache) ? cache->original : 0;

	sculpt_stroke_modifiers_check(C, ob);

	dist = sculpt_raycast_init(&vc, mouse, ray_start, ray_end, ray_normal, original);

	srd.original = original;
	srd.ss = ob->sculpt;
	srd.hit = 0;
	srd.ray_start = ray_start;
	srd.ray_normal = ray_normal;
	srd.dist = dist;

	BKE_pbvh_raycast(ss->pbvh, sculpt_raycast_cb, &srd,
	                 ray_start, ray_normal, srd.original);

	copy_v3_v3(out, ray_normal);
	mul_v3_fl(out, srd.dist);
	add_v3_v3(out, ray_start);

	return srd.hit;
}

static void sculpt_brush_init_tex(const Scene *scene, Sculpt *sd, SculptSession *ss)
{
	Brush *brush = BKE_paint_brush(&sd->paint);
	MTex *mtex = &brush->mtex;

	/* init mtex nodes */
	if (mtex->tex && mtex->tex->nodetree)
		ntreeTexBeginExecTree(mtex->tex->nodetree);  /* has internal flag to detect it only does it once */

	/* TODO: Shouldn't really have to do this at the start of every
	 * stroke, but sculpt would need some sort of notification when
	 * changes are made to the texture. */
	sculpt_update_tex(scene, sd, ss);
}

static bool sculpt_brush_stroke_init(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
	SculptSession *ss = CTX_data_active_object(C)->sculpt;
	Brush *brush = BKE_paint_brush(&sd->paint);
	int mode = RNA_enum_get(op->ptr, "mode");
	bool is_smooth;
	bool need_mask = false;

	if (brush->sculpt_tool == SCULPT_TOOL_MASK) {
		need_mask = true;
	}

	view3d_operator_needs_opengl(C);
	sculpt_brush_init_tex(scene, sd, ss);

	is_smooth = sculpt_any_smooth_mode(brush, NULL, mode);
	BKE_sculpt_update_mesh_elements(scene, sd, ob, is_smooth, need_mask);

	zero_v3(ob->sculpt->average_stroke_accum);
	ob->sculpt->average_stroke_counter = 0;

	return 1;
}

static void sculpt_restore_mesh(Sculpt *sd, Object *ob)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = BKE_paint_brush(&sd->paint);

	/* Restore the mesh before continuing with anchored stroke */
	if ((brush->flag & BRUSH_ANCHORED) ||
	    (brush->sculpt_tool == SCULPT_TOOL_GRAB &&
	     BKE_brush_use_size_pressure(ss->cache->vc->scene, brush)) ||
	    (brush->flag & BRUSH_DRAG_DOT))
	{
		paint_mesh_restore_co(sd, ob);
	}
}

/* Copy the PBVH bounding box into the object's bounding box */
void sculpt_update_object_bounding_box(Object *ob)
{
	if (ob->bb) {
		float bb_min[3], bb_max[3];

		BKE_pbvh_bounding_box(ob->sculpt->pbvh, bb_min, bb_max);
		BKE_boundbox_init_from_minmax(ob->bb, bb_min, bb_max);
	}
}

static void sculpt_flush_update(bContext *C)
{
	Object *ob = CTX_data_active_object(C);
	SculptSession *ss = ob->sculpt;
	ARegion *ar = CTX_wm_region(C);
	MultiresModifierData *mmd = ss->multires;

	if (mmd)
		multires_mark_as_modified(ob, MULTIRES_COORDS_MODIFIED);
	if (ob->derivedFinal) /* VBO no longer valid */
		GPU_drawobject_free(ob->derivedFinal);

	if (ss->kb || ss->modifiers_active) {
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		ED_region_tag_redraw(ar);
	}
	else {
		rcti r;

		BKE_pbvh_update(ss->pbvh, PBVH_UpdateBB, NULL);
		/* Update the object's bounding box too so that the object
		 * doesn't get incorrectly clipped during drawing in
		 * draw_mesh_object(). [#33790] */
		sculpt_update_object_bounding_box(ob);

		if (sculpt_get_redraw_rect(ar, CTX_wm_region_view3d(C), ob, &r)) {
			if (ss->cache) {
				ss->cache->current_r = r;
			}

			/* previous is not set in the current cache else
			 * the partial rect will always grow */
			sculpt_extend_redraw_rect_previous(ob, &r);

			r.xmin += ar->winrct.xmin - 2;
			r.xmax += ar->winrct.xmin + 2;
			r.ymin += ar->winrct.ymin - 2;
			r.ymax += ar->winrct.ymin + 2;

			ss->partial_redraw = 1;
			ED_region_tag_redraw_partial(ar, &r);
		}
	}
}

/* Returns whether the mouse/stylus is over the mesh (1)
 * or over the background (0) */
static bool over_mesh(bContext *C, struct wmOperator *UNUSED(op), float x, float y)
{
	float mouse[2], co[3];

	mouse[0] = x;
	mouse[1] = y;

	return sculpt_stroke_get_location(C, co, mouse);
}

static bool sculpt_stroke_test_start(bContext *C, struct wmOperator *op,
                                    const float mouse[2])
{
	/* Don't start the stroke until mouse goes over the mesh.
	 * note: mouse will only be null when re-executing the saved stroke. */
	if (!mouse || over_mesh(C, op, mouse[0], mouse[1])) {
		Object *ob = CTX_data_active_object(C);
		SculptSession *ss = ob->sculpt;
		Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

		ED_view3d_init_mats_rv3d(ob, CTX_wm_region_view3d(C));

		sculpt_update_cache_invariants(C, sd, ss, op, mouse);

		sculpt_undo_push_begin(sculpt_tool_name(sd));

		return 1;
	}
	else
		return 0;
}

static void sculpt_stroke_update_step(bContext *C, struct PaintStroke *UNUSED(stroke), PointerRNA *itemptr)
{
	UnifiedPaintSettings *ups = &CTX_data_tool_settings(C)->unified_paint_settings;
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
	Object *ob = CTX_data_active_object(C);
	SculptSession *ss = ob->sculpt;
	const Brush *brush = BKE_paint_brush(&sd->paint);
	
	sculpt_stroke_modifiers_check(C, ob);
	sculpt_update_cache_variants(C, sd, ob, itemptr);
	sculpt_restore_mesh(sd, ob);

	if (sd->flags & SCULPT_DYNTOPO_DETAIL_CONSTANT) {
		BKE_pbvh_bmesh_detail_size_set(ss->pbvh,
			sd->constant_detail / 100.0f);
	}
	else {
		BKE_pbvh_bmesh_detail_size_set(ss->pbvh,
		                               (ss->cache->radius /
		                                (float)ups->pixel_radius) *
		                               (float)sd->detail_size / 0.4f);
	}

	if (sculpt_stroke_dynamic_topology(ss, brush)) {
		do_symmetrical_brush_actions(sd, ob, sculpt_topology_update, ups);
	}

	do_symmetrical_brush_actions(sd, ob, do_brush_action, ups);

	sculpt_combine_proxies(sd, ob);

	/* hack to fix noise texture tearing mesh */
	sculpt_fix_noise_tear(sd, ob);

	/* TODO(sergey): This is not really needed for the solid shading,
	 * which does use pBVH drawing anyway, but texture and wireframe
	 * requires this.
	 *
	 * Could be optimized later, but currently don't think it's so
	 * much common scenario.
	 *
	 * Same applies to the DAG_id_tag_update() invoked from
	 * sculpt_flush_update().
	 */
	if (ss->modifiers_active) {
		sculpt_flush_stroke_deform(sd, ob);
	}
	else if (ss->kb) {
		sculpt_update_keyblock(ob);
	}

	ss->cache->first_time = false;

	/* Cleanup */
	sculpt_flush_update(C);
}

static void sculpt_brush_exit_tex(Sculpt *sd)
{
	Brush *brush = BKE_paint_brush(&sd->paint);
	MTex *mtex = &brush->mtex;

	if (mtex->tex && mtex->tex->nodetree)
		ntreeTexEndExecTree(mtex->tex->nodetree->execdata);
}

static void sculpt_stroke_done(const bContext *C, struct PaintStroke *UNUSED(stroke))
{
	Object *ob = CTX_data_active_object(C);
	Scene *scene = CTX_data_scene(C);
	SculptSession *ss = ob->sculpt;
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

	sculpt_omp_done(ss);

	/* Finished */
	if (ss->cache) {
		UnifiedPaintSettings *ups = &CTX_data_tool_settings(C)->unified_paint_settings;
		Brush *brush = BKE_paint_brush(&sd->paint);
		ups->draw_inverted = false;

		sculpt_stroke_modifiers_check(C, ob);

		/* Alt-Smooth */
		if (ss->cache->alt_smooth) {
			if (brush->sculpt_tool == SCULPT_TOOL_MASK) {
				brush->mask_tool = ss->cache->saved_mask_brush_tool;
			}
			else {
				Paint *p = &sd->paint;
				BKE_brush_size_set(scene, ss->cache->brush, ss->cache->saved_smooth_size);
				brush = (Brush *)BKE_libblock_find_name(ID_BR, ss->cache->saved_active_brush_name);
				if (brush) {
					BKE_paint_brush_set(p, brush);
				}
			}
		}

		/* update last stroke position */
		ob->sculpt->last_stroke_valid = 1;
		ED_sculpt_stroke_get_average(ob, ob->sculpt->last_stroke);

		sculpt_cache_free(ss->cache);
		ss->cache = NULL;

		sculpt_undo_push_end();

		BKE_pbvh_update(ss->pbvh, PBVH_UpdateOriginalBB, NULL);
		
		if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH)
			BKE_pbvh_bmesh_after_stroke(ss->pbvh);

		/* optimization: if there is locked key and active modifiers present in */
		/* the stack, keyblock is updating at each step. otherwise we could update */
		/* keyblock only when stroke is finished */
		if (ss->kb && !ss->modifiers_active) sculpt_update_keyblock(ob);

		ss->partial_redraw = 0;

		/* try to avoid calling this, only for e.g. linked duplicates now */
		if (((Mesh *)ob->data)->id.us > 1)
			DAG_id_tag_update(&ob->id, OB_RECALC_DATA);

		WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	}

	sculpt_brush_exit_tex(sd);
}

static int sculpt_brush_stroke_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	struct PaintStroke *stroke;
	int ignore_background_click;
	int retval;

	if (!sculpt_brush_stroke_init(C, op))
		return OPERATOR_CANCELLED;

	stroke = paint_stroke_new(C, op, sculpt_stroke_get_location,
	                          sculpt_stroke_test_start,
	                          sculpt_stroke_update_step, NULL,
	                          sculpt_stroke_done, event->type);

	op->customdata = stroke;

	/* For tablet rotation */
	ignore_background_click = RNA_boolean_get(op->ptr, "ignore_background_click");

	if (ignore_background_click && !over_mesh(C, op, event->x, event->y)) {
		paint_stroke_data_free(op);
		return OPERATOR_PASS_THROUGH;
	}
	
	if ((retval = op->type->modal(C, op, event)) == OPERATOR_FINISHED) {
		paint_stroke_data_free(op);
		return OPERATOR_FINISHED;
	}
	/* add modal handler */
	WM_event_add_modal_handler(C, op);

	OPERATOR_RETVAL_CHECK(retval);
	BLI_assert(retval == OPERATOR_RUNNING_MODAL);
	
	return OPERATOR_RUNNING_MODAL;
}

static int sculpt_brush_stroke_exec(bContext *C, wmOperator *op)
{
	if (!sculpt_brush_stroke_init(C, op))
		return OPERATOR_CANCELLED;

	op->customdata = paint_stroke_new(C, op, sculpt_stroke_get_location, sculpt_stroke_test_start,
	                                  sculpt_stroke_update_step, NULL, sculpt_stroke_done, 0);

	/* frees op->customdata */
	paint_stroke_exec(C, op);

	return OPERATOR_FINISHED;
}

static void sculpt_brush_stroke_cancel(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	SculptSession *ss = ob->sculpt;
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

	if (ss->cache) {
		paint_mesh_restore_co(sd, ob);
	}

	paint_stroke_cancel(C, op);

	if (ss->cache) {
		sculpt_cache_free(ss->cache);
		ss->cache = NULL;
	}

	sculpt_brush_exit_tex(sd);
}

static void SCULPT_OT_brush_stroke(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Sculpt";
	ot->idname = "SCULPT_OT_brush_stroke";
	ot->description = "Sculpt a stroke into the geometry";
	
	/* api callbacks */
	ot->invoke = sculpt_brush_stroke_invoke;
	ot->modal = paint_stroke_modal;
	ot->exec = sculpt_brush_stroke_exec;
	ot->poll = sculpt_poll;
	ot->cancel = sculpt_brush_stroke_cancel;

	/* flags (sculpt does own undo? (ton) */
	ot->flag = OPTYPE_BLOCKING;

	/* properties */

	paint_stroke_operator_properties(ot);

	RNA_def_boolean(ot->srna, "ignore_background_click", 0,
	                "Ignore Background Click",
	                "Clicks on the background do not start the stroke");	
}

/**** Reset the copy of the mesh that is being sculpted on (currently just for the layer brush) ****/

static int sculpt_set_persistent_base_exec(bContext *C, wmOperator *UNUSED(op))
{
	SculptSession *ss = CTX_data_active_object(C)->sculpt;

	if (ss) {
		if (ss->layer_co)
			MEM_freeN(ss->layer_co);
		ss->layer_co = NULL;
	}

	return OPERATOR_FINISHED;
}

static void SCULPT_OT_set_persistent_base(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Persistent Base";
	ot->idname = "SCULPT_OT_set_persistent_base";
	ot->description = "Reset the copy of the mesh that is being sculpted on";
	
	/* api callbacks */
	ot->exec = sculpt_set_persistent_base_exec;
	ot->poll = sculpt_mode_poll;
	
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/************************** Dynamic Topology **************************/

static void sculpt_dynamic_topology_triangulate(BMesh *bm)
{
	if (bm->totloop != bm->totface * 3) {
		BM_mesh_triangulate(bm, MOD_TRIANGULATE_QUAD_FIXED, MOD_TRIANGULATE_NGON_EARCLIP, false, NULL, NULL);
	}
}

void sculpt_pbvh_clear(Object *ob)
{
	SculptSession *ss = ob->sculpt;
	DerivedMesh *dm = ob->derivedFinal;

	/* Clear out any existing DM and PBVH */
	if (ss->pbvh)
		BKE_pbvh_free(ss->pbvh);
	ss->pbvh = NULL;
	if (dm)
		dm->getPBVH(NULL, dm);
	BKE_object_free_derived_caches(ob);
}

void sculpt_dyntopo_node_layers_add(SculptSession *ss)
{
	int cd_node_layer_index;

	char layer_id[] = "_dyntopo_node_id";

	cd_node_layer_index = CustomData_get_named_layer_index(&ss->bm->vdata, CD_PROP_INT, layer_id);
	if (cd_node_layer_index == -1) {
		BM_data_layer_add_named(ss->bm, &ss->bm->vdata, CD_PROP_INT, layer_id);
		cd_node_layer_index = CustomData_get_named_layer_index(&ss->bm->vdata, CD_PROP_INT, layer_id);
	}

	ss->cd_vert_node_offset = CustomData_get_n_offset(&ss->bm->vdata, CD_PROP_INT,
	                                                  cd_node_layer_index - CustomData_get_layer_index(&ss->bm->vdata, CD_PROP_INT));

	ss->bm->vdata.layers[cd_node_layer_index].flag |= CD_FLAG_TEMPORARY;

	cd_node_layer_index = CustomData_get_named_layer_index(&ss->bm->pdata, CD_PROP_INT, layer_id);
	if (cd_node_layer_index == -1) {
		BM_data_layer_add_named(ss->bm, &ss->bm->pdata, CD_PROP_INT, layer_id);
		cd_node_layer_index = CustomData_get_named_layer_index(&ss->bm->pdata, CD_PROP_INT, layer_id);
	}

	ss->cd_face_node_offset = CustomData_get_n_offset(&ss->bm->pdata, CD_PROP_INT,
	                                                  cd_node_layer_index - CustomData_get_layer_index(&ss->bm->pdata, CD_PROP_INT));

	ss->bm->pdata.layers[cd_node_layer_index].flag |= CD_FLAG_TEMPORARY;
}


void sculpt_update_after_dynamic_topology_toggle(bContext *C)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	Sculpt *sd = scene->toolsettings->sculpt;

	/* Create the PBVH */
	BKE_sculpt_update_mesh_elements(scene, sd, ob, false, false);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
}

void sculpt_dynamic_topology_enable(bContext *C)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	SculptSession *ss = ob->sculpt;
	Mesh *me = ob->data;
	const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(me);

	sculpt_pbvh_clear(ob);

	ss->bm_smooth_shading = (scene->toolsettings->sculpt->flags & SCULPT_DYNTOPO_SMOOTH_SHADING) != 0;

	/* Dynamic topology doesn't ensure selection state is valid, so remove [#36280] */
	BKE_mesh_mselect_clear(me);

	/* Create triangles-only BMesh */
	ss->bm = BM_mesh_create(&allocsize);

	BM_mesh_bm_from_me(ss->bm, me, true, true, ob->shapenr);
	sculpt_dynamic_topology_triangulate(ss->bm);
	BM_data_layer_add(ss->bm, &ss->bm->vdata, CD_PAINT_MASK);
	sculpt_dyntopo_node_layers_add(ss);
	/* make sure the data for existing faces are initialized */
	BM_mesh_normals_update(ss->bm);

	/* Enable dynamic topology */
	me->flag |= ME_SCULPT_DYNAMIC_TOPOLOGY;
	
	/* Enable logging for undo/redo */
	ss->bm_log = BM_log_create(ss->bm);

	/* Refresh */
	sculpt_update_after_dynamic_topology_toggle(C);
}

/* Free the sculpt BMesh and BMLog
 *
 * If 'unode' is given, the BMesh's data is copied out to the unode
 * before the BMesh is deleted so that it can be restored from */
void sculpt_dynamic_topology_disable(bContext *C,
                                     SculptUndoNode *unode)
{
	Object *ob = CTX_data_active_object(C);
	SculptSession *ss = ob->sculpt;
	Mesh *me = ob->data;

	sculpt_pbvh_clear(ob);

	if (unode) {
		/* Free all existing custom data */
		CustomData_free(&me->vdata, me->totvert);
		CustomData_free(&me->edata, me->totedge);
		CustomData_free(&me->fdata, me->totface);
		CustomData_free(&me->ldata, me->totloop);
		CustomData_free(&me->pdata, me->totpoly);

		/* Copy over stored custom data */
		me->totvert = unode->bm_enter_totvert;
		me->totloop = unode->bm_enter_totloop;
		me->totpoly = unode->bm_enter_totpoly;
		me->totedge = unode->bm_enter_totedge;
		me->totface = 0;
		CustomData_copy(&unode->bm_enter_vdata, &me->vdata, CD_MASK_MESH,
		                CD_DUPLICATE, unode->bm_enter_totvert);
		CustomData_copy(&unode->bm_enter_edata, &me->edata, CD_MASK_MESH,
		                CD_DUPLICATE, unode->bm_enter_totedge);
		CustomData_copy(&unode->bm_enter_ldata, &me->ldata, CD_MASK_MESH,
		                CD_DUPLICATE, unode->bm_enter_totloop);
		CustomData_copy(&unode->bm_enter_pdata, &me->pdata, CD_MASK_MESH,
		                CD_DUPLICATE, unode->bm_enter_totpoly);

		BKE_mesh_update_customdata_pointers(me, false);
	}
	else {
		BKE_sculptsession_bm_to_me(ob, true);
	}

	/* Clear data */
	me->flag &= ~ME_SCULPT_DYNAMIC_TOPOLOGY;

	/* typically valid but with global-undo they can be NULL, [#36234] */
	if (ss->bm) {
		BM_mesh_free(ss->bm);
		ss->bm = NULL;
	}
	if (ss->bm_log) {
		BM_log_free(ss->bm_log);
		ss->bm_log = NULL;
	}

	/* Refresh */
	sculpt_update_after_dynamic_topology_toggle(C);
}


static int sculpt_dynamic_topology_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = CTX_data_active_object(C);
	SculptSession *ss = ob->sculpt;

	if (ss->bm) {
		sculpt_undo_push_begin("Dynamic topology disable");
		sculpt_undo_push_node(ob, NULL, SCULPT_UNDO_DYNTOPO_END);
		sculpt_dynamic_topology_disable(C, NULL);
	}
	else {
		sculpt_undo_push_begin("Dynamic topology enable");
		sculpt_dynamic_topology_enable(C);
		sculpt_undo_push_node(ob, NULL, SCULPT_UNDO_DYNTOPO_BEGIN);
	}
	sculpt_undo_push_end();

	return OPERATOR_FINISHED;
}


static int dyntopo_warning_popup(bContext *C, wmOperatorType *ot, bool vdata, bool modifiers)
{
	uiPopupMenu *pup = uiPupMenuBegin(C, IFACE_("Warning!"), ICON_ERROR);
	uiLayout *layout = uiPupMenuLayout(pup);

	if (vdata) {
		const char *msg_error = TIP_("Vertex Data Detected!");
		const char *msg = TIP_("Dyntopo will not preserve vertex colors, UVs, or other customdata");
		uiItemL(layout, msg_error, ICON_INFO);
		uiItemL(layout, msg, ICON_NONE);
		uiItemS(layout);
	}

	if (modifiers) {
		const char *msg_error = TIP_("Generative Modifiers Detected!");
		const char *msg = TIP_("Keeping the modifiers will increase polycount when returning to object mode");

		uiItemL(layout, msg_error, ICON_INFO);
		uiItemL(layout, msg, ICON_NONE);
		uiItemS(layout);
	}

	uiItemFullO_ptr(layout, ot, IFACE_("OK"), ICON_NONE, NULL, WM_OP_EXEC_DEFAULT, 0);

	uiPupMenuEnd(C, pup);

	return OPERATOR_CANCELLED;
}


static int sculpt_dynamic_topology_toggle_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	Object *ob = CTX_data_active_object(C);
	Mesh *me = ob->data;
	SculptSession *ss = ob->sculpt;

	if (!ss->bm) {
		Scene *scene = CTX_data_scene(C);
		ModifierData *md;
		VirtualModifierData virtualModifierData;
		int i;
		bool vdata = false;
		bool modifiers = false;

		for (i = 0; i < CD_NUMTYPES; i++) {
			if (!ELEM(i, CD_MVERT, CD_MEDGE, CD_MFACE, CD_MLOOP, CD_MPOLY, CD_PAINT_MASK, CD_ORIGINDEX) &&
			    (CustomData_has_layer(&me->vdata, i) ||
			     CustomData_has_layer(&me->edata, i) ||
			     CustomData_has_layer(&me->fdata, i)))
			{
				vdata = true;
				break;
			}
		}

		md = modifiers_getVirtualModifierList(ob, &virtualModifierData);

		/* exception for shape keys because we can edit those */
		for (; md; md = md->next) {
			ModifierTypeInfo *mti = modifierType_getInfo(md->type);
			if (!modifier_isEnabled(scene, md, eModifierMode_Realtime)) continue;

			if (mti->type == eModifierTypeType_Constructive) {
				modifiers = true;
				break;
			}
		}

		if (vdata || modifiers) {
			/* The mesh has customdata that will be lost, let the user confirm this is OK */
			return dyntopo_warning_popup(C, op->type, vdata, modifiers);
		}
	}

	return sculpt_dynamic_topology_toggle_exec(C, op);
}

static void SCULPT_OT_dynamic_topology_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Dynamic Topology Toggle";
	ot->idname = "SCULPT_OT_dynamic_topology_toggle";
	ot->description = "Dynamic topology alters the mesh topology while sculpting";
	
	/* api callbacks */
	ot->invoke = sculpt_dynamic_topology_toggle_invoke;
	ot->exec = sculpt_dynamic_topology_toggle_exec;
	ot->poll = sculpt_mode_poll;
	
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/************************* SCULPT_OT_optimize *************************/

static int sculpt_optimize_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = CTX_data_active_object(C);

	sculpt_pbvh_clear(ob);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

	return OPERATOR_FINISHED;
}

static int sculpt_and_dynamic_topology_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	return sculpt_mode_poll(C) && ob->sculpt->bm;
}

/* The BVH gets less optimal more quickly with dynamic topology than
 * regular sculpting. There is no doubt more clever stuff we can do to
 * optimize it on the fly, but for now this gives the user a nicer way
 * to recalculate it than toggling modes. */
static void SCULPT_OT_optimize(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Optimize";
	ot->idname = "SCULPT_OT_optimize";
	ot->description = "Recalculate the sculpt BVH to improve performance";
	
	/* api callbacks */
	ot->exec = sculpt_optimize_exec;
	ot->poll = sculpt_and_dynamic_topology_poll;
	
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************* Dynamic topology symmetrize ********************/

static int sculpt_symmetrize_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = CTX_data_active_object(C);
	const Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
	SculptSession *ss = ob->sculpt;

	/* To simplify undo for symmetrize, all BMesh elements are logged
	 * as deleted, then after symmetrize operation all BMesh elements
	 * are logged as added (as opposed to attempting to store just the
	 * parts that symmetrize modifies) */
	sculpt_undo_push_begin("Dynamic topology symmetrize");
	sculpt_undo_push_node(ob, NULL, SCULPT_UNDO_DYNTOPO_SYMMETRIZE);
	BM_log_before_all_removed(ss->bm, ss->bm_log);

	/* Symmetrize and re-triangulate */
	BMO_op_callf(ss->bm, BMO_FLAG_DEFAULTS,
	             "symmetrize input=%avef direction=%i  dist=%f",
	             sd->symmetrize_direction, 0.00001f);
	sculpt_dynamic_topology_triangulate(ss->bm);

	/* Finish undo */
	BM_log_all_added(ss->bm, ss->bm_log);
	sculpt_undo_push_end();

	/* Redraw */
	sculpt_pbvh_clear(ob);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

	return OPERATOR_FINISHED;
}

static void SCULPT_OT_symmetrize(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Symmetrize";
	ot->idname = "SCULPT_OT_symmetrize";
	ot->description = "Symmetrize the topology modifications";
	
	/* api callbacks */
	ot->exec = sculpt_symmetrize_exec;
	ot->poll = sculpt_and_dynamic_topology_poll;
}

/**** Toggle operator for turning sculpt mode on or off ****/

static void sculpt_init_session(Scene *scene, Object *ob)
{
	ob->sculpt = MEM_callocN(sizeof(SculptSession), "sculpt session");

	BKE_sculpt_update_mesh_elements(scene, scene->toolsettings->sculpt, ob, 0, false);
}


static int sculpt_mode_toggle_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = CTX_data_tool_settings(C);
	Object *ob = CTX_data_active_object(C);
	const int mode_flag = OB_MODE_SCULPT;
	const bool is_mode_set = (ob->mode & mode_flag) != 0;
	Mesh *me;
	MultiresModifierData *mmd = BKE_sculpt_multires_active(scene, ob);
	int flush_recalc = 0;

	if (!is_mode_set) {
		if (!ED_object_mode_compat_set(C, ob, mode_flag, op->reports)) {
			return OPERATOR_CANCELLED;
		}
	}

	me = BKE_mesh_from_object(ob);

	/* multires in sculpt mode could have different from object mode subdivision level */
	flush_recalc |= mmd && mmd->sculptlvl != mmd->lvl;
	/* if object has got active modifiers, it's dm could be different in sculpt mode  */
	flush_recalc |= sculpt_has_active_modifiers(scene, ob);

	if (is_mode_set) {
		if (mmd)
			multires_force_update(ob);

		if (flush_recalc || (ob->sculpt && ob->sculpt->bm))
			DAG_id_tag_update(&ob->id, OB_RECALC_DATA);

		if (me->flag & ME_SCULPT_DYNAMIC_TOPOLOGY) {
			/* Dynamic topology must be disabled before exiting sculpt
			 * mode to ensure the undo stack stays in a consistent
			 * state */
			sculpt_dynamic_topology_toggle_exec(C, NULL);
		}

		/* Leave sculptmode */
		ob->mode &= ~mode_flag;

		BKE_free_sculptsession(ob);

		paint_cursor_delete_textures();
	}
	else {
		/* Enter sculptmode */
		ob->mode |= mode_flag;

		/* Remove dynamic-topology flag; this will be enabled if the
		 * file was saved with dynamic topology on, but we don't
		 * automatically re-enter dynamic-topology mode when loading a
		 * file. */
		me->flag &= ~ME_SCULPT_DYNAMIC_TOPOLOGY;

		if (flush_recalc)
			DAG_id_tag_update(&ob->id, OB_RECALC_DATA);

		/* Create persistent sculpt mode data */
		if (!ts->sculpt) {
			ts->sculpt = MEM_callocN(sizeof(Sculpt), "sculpt mode data");

			/* Turn on X plane mirror symmetry by default */
			ts->sculpt->paint.symmetry_flags |= PAINT_SYMM_X;
			ts->sculpt->paint.flags |= PAINT_SHOW_BRUSH;

			/* Make sure at least dyntopo subdivision is enabled */
			ts->sculpt->flags |= SCULPT_DYNTOPO_SUBDIVIDE | SCULPT_DYNTOPO_COLLAPSE;
		}

		if (!ts->sculpt->detail_size) {
			ts->sculpt->detail_size = 12;
		}

		if (ts->sculpt->constant_detail == 0.0f)
			ts->sculpt->constant_detail = 30.0f;

		/* Create sculpt mode session data */
		if (ob->sculpt)
			BKE_free_sculptsession(ob);

		sculpt_init_session(scene, ob);

		/* Mask layer is required */
		if (mmd) {
			/* XXX, we could attempt to support adding mask data mid-sculpt mode (with multi-res)
			 * but this ends up being quite tricky (and slow) */
			BKE_sculpt_mask_layers_ensure(ob, mmd);
		}

		if (!(fabsf(ob->size[0] - ob->size[1]) < 1e-4f && fabsf(ob->size[1] - ob->size[2]) < 1e-4f)) {
			BKE_report(op->reports, RPT_WARNING,
			           "Object has non-uniform scale, sculpting may be unpredictable");
		}
		else if (is_negative_m4(ob->obmat)) {
			BKE_report(op->reports, RPT_WARNING,
			           "Object has negative scale, sculpting may be unpredictable");
		}

		BKE_paint_init(&ts->sculpt->paint, PAINT_CURSOR_SCULPT);

		paint_cursor_start(C, sculpt_poll_view3d);
	}

	WM_event_add_notifier(C, NC_SCENE | ND_MODE, scene);

	return OPERATOR_FINISHED;
}

static void SCULPT_OT_sculptmode_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Sculpt Mode";
	ot->idname = "SCULPT_OT_sculptmode_toggle";
	ot->description = "Toggle sculpt mode in 3D view";
	
	/* api callbacks */
	ot->exec = sculpt_mode_toggle_exec;
	ot->poll = ED_operator_object_active_editable_mesh;
	
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


static int sculpt_and_dynamic_topology_constant_detail_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

	return sculpt_mode_poll(C) && ob->sculpt->bm && (sd->flags & SCULPT_DYNTOPO_DETAIL_CONSTANT);
}

static int sculpt_detail_flood_fill_exec(bContext *C, wmOperator *UNUSED(op))
{
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
	Object *ob = CTX_data_active_object(C);
	SculptSession *ss = ob->sculpt;
	float size;
	float bb_min[3], bb_max[3];
	int i, totnodes;
	PBVHNode **nodes;

	BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnodes);

	if (!totnodes)
		return OPERATOR_CANCELLED;

	for (i = 0; i < totnodes; i++) {
		BKE_pbvh_node_mark_topology_update(nodes[i]);
	}
	/* get the bounding box, store the size to bb_max and center (zero) to bb_min */
	BKE_pbvh_bounding_box(ob->sculpt->pbvh, bb_min, bb_max);
	sub_v3_v3(bb_max, bb_min);
	zero_v3(bb_min);
	size = max_fff(bb_max[0], bb_max[1], bb_max[2]);

	/* update topology size */
	BKE_pbvh_bmesh_detail_size_set(ss->pbvh, sd->constant_detail / 100.0f);

	sculpt_undo_push_begin("Dynamic topology flood fill");
	sculpt_undo_push_node(ob, NULL, SCULPT_UNDO_COORDS);

	while (BKE_pbvh_bmesh_update_topology(ss->pbvh, PBVH_Collapse | PBVH_Subdivide, bb_min, size)) {
		for (i = 0; i < totnodes; i++)
			BKE_pbvh_node_mark_topology_update(nodes[i]);
	}

	MEM_freeN(nodes);
	sculpt_undo_push_end();

	/* force rebuild of pbvh for better BB placement */
	sculpt_pbvh_clear(ob);
	/* Redraw */
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

	return OPERATOR_FINISHED;
}

static void SCULPT_OT_detail_flood_fill(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Detail Flood Fill";
	ot->idname = "SCULPT_OT_detail_flood_fill";
	ot->description = "Flood fill the mesh with the selected detail setting";

	/* api callbacks */
	ot->exec = sculpt_detail_flood_fill_exec;
	ot->poll = sculpt_and_dynamic_topology_constant_detail_poll;

	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static void sample_detail(bContext *C, int ss_co[2])
{
	ViewContext vc;
	Object *ob;
	Sculpt *sd;
	float ray_start[3], ray_end[3], ray_normal[3], dist;
	SculptDetailRaycastData srd;
	float mouse[2] = {ss_co[0], ss_co[1]};
	view3d_set_viewcontext(C, &vc);

	sd = CTX_data_tool_settings(C)->sculpt;
	ob = vc.obact;

	sculpt_stroke_modifiers_check(C, ob);

	dist = sculpt_raycast_init(&vc, mouse, ray_start, ray_end, ray_normal, false);

	srd.hit = 0;
	srd.ray_start = ray_start;
	srd.ray_normal = ray_normal;
	srd.dist = dist;
	srd.detail = sd->constant_detail;

	BKE_pbvh_raycast(ob->sculpt->pbvh, sculpt_raycast_detail_cb, &srd,
	                 ray_start, ray_normal, false);

	if (srd.hit) {
		sd->constant_detail = srd.detail * 100.0f;
	}
}

static int sculpt_sample_detail_size_exec(bContext *C, wmOperator *op)
{
	int ss_co[2];
	RNA_int_get_array(op->ptr, "location", ss_co);
	sample_detail(C, ss_co);
	return OPERATOR_FINISHED;
}


static int sculpt_sample_detail_size_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(e))
{
	ScrArea *sa = CTX_wm_area(C);
	ED_area_headerprint(sa, "Click on the mesh to set the detail");
	WM_cursor_modal_set(CTX_wm_window(C), BC_EYEDROPPER_CURSOR);
	WM_event_add_modal_handler(C, op);
	return OPERATOR_RUNNING_MODAL;
}

static int sculpt_sample_detail_size_modal(bContext *C, wmOperator *op, const wmEvent *e)
{
	switch (e->type) {
		case LEFTMOUSE:
			if (e->val == KM_PRESS) {
				ScrArea *sa = CTX_wm_area(C);
				int ss_co[2] = {e->mval[0], e->mval[1]};

				sample_detail(C, ss_co);

				RNA_int_set_array(op->ptr, "location", ss_co);
				WM_cursor_modal_restore(CTX_wm_window(C));
				ED_area_headerprint(sa, NULL);
				WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, NULL);

				return OPERATOR_FINISHED;
			}
			break;

		case RIGHTMOUSE:
		{
			ScrArea *sa = CTX_wm_area(C);
			WM_cursor_modal_restore(CTX_wm_window(C));
			ED_area_headerprint(sa, NULL);

			return OPERATOR_CANCELLED;
		}
	}

	return OPERATOR_RUNNING_MODAL;
}


static void SCULPT_OT_sample_detail_size(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Sample Detail Size";
	ot->idname = "SCULPT_OT_sample_detail_size";
	ot->description = "Sample the mesh detail on clicked point";

	/* api callbacks */
	ot->invoke = sculpt_sample_detail_size_invoke;
	ot->exec = sculpt_sample_detail_size_exec;
	ot->modal = sculpt_sample_detail_size_modal;
	ot->poll = sculpt_and_dynamic_topology_constant_detail_poll;

	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_int_array(ot->srna, "location", 2, NULL, 0, SHRT_MAX, "Location", "Screen Coordinates of sampling", 0, SHRT_MAX);
}

void ED_operatortypes_sculpt(void)
{
	WM_operatortype_append(SCULPT_OT_brush_stroke);
	WM_operatortype_append(SCULPT_OT_sculptmode_toggle);
	WM_operatortype_append(SCULPT_OT_set_persistent_base);
	WM_operatortype_append(SCULPT_OT_dynamic_topology_toggle);
	WM_operatortype_append(SCULPT_OT_optimize);
	WM_operatortype_append(SCULPT_OT_symmetrize);
	WM_operatortype_append(SCULPT_OT_detail_flood_fill);
	WM_operatortype_append(SCULPT_OT_sample_detail_size);
}
