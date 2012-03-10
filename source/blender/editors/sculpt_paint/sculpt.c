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
#include "BLI_utildefines.h"
#include "BLI_dynstr.h"
#include "BLI_ghash.h"
#include "BLI_pbvh.h"
#include "BLI_threads.h"
#include "BLI_rand.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_brush_types.h"

#include "BKE_brush.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_lattice.h" /* for armature_deform_verts */
#include "BKE_node.h"

#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_sculpt.h"
#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_util.h" /* for crazyspace correction */
#include "paint_intern.h"
#include "sculpt_intern.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "RE_render_ext.h"

#include "GPU_buffers.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef _OPENMP
#include <omp.h>
#endif

void ED_sculpt_force_update(bContext *C)
{
	Object *ob= CTX_data_active_object(C);

	if(ob && (ob->mode & OB_MODE_SCULPT))
		multires_force_update(ob);
}

/* Sculpt mode handles multires differently from regular meshes, but only if
 * it's the last modifier on the stack and it is not on the first level */
struct MultiresModifierData *sculpt_multires_active(Scene *scene, Object *ob)
{
	Mesh *me= (Mesh*)ob->data;
	ModifierData *md;

	if(!CustomData_get_layer(&me->ldata, CD_MDISPS)) {
		/* multires can't work without displacement layer */
		return NULL;
	}

	for(md= modifiers_getVirtualModifierList(ob); md; md= md->next) {
		if(md->type == eModifierType_Multires) {
			MultiresModifierData *mmd= (MultiresModifierData*)md;

			if(!modifier_isEnabled(scene, md, eModifierMode_Realtime))
				continue;

			if(mmd->sculptlvl > 0) return mmd;
			else return NULL;
		}
	}

	return NULL;
}

/* Check if there are any active modifiers in stack (used for flushing updates at enter/exit sculpt mode) */
static int sculpt_has_active_modifiers(Scene *scene, Object *ob)
{
	ModifierData *md;

	md= modifiers_getVirtualModifierList(ob);

	/* exception for shape keys because we can edit those */
	for(; md; md= md->next) {
		if(modifier_isEnabled(scene, md, eModifierMode_Realtime))
			return 1;
	}

	return 0;
}

/* Checks if there are any supported deformation modifiers active */
static int sculpt_modifiers_active(Scene *scene, Sculpt *sd, Object *ob)
{
	ModifierData *md;
	Mesh *me= (Mesh*)ob->data;
	MultiresModifierData *mmd= sculpt_multires_active(scene, ob);

	if(mmd) return 0;

	/* non-locked shape keys could be handled in the same way as deformed mesh */
	if((ob->shapeflag&OB_SHAPE_LOCK)==0 && me->key && ob->shapenr)
		return 1;

	md= modifiers_getVirtualModifierList(ob);

	/* exception for shape keys because we can edit those */
	for(; md; md= md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);
		if(!modifier_isEnabled(scene, md, eModifierMode_Realtime)) continue;
		if(md->type==eModifierType_ShapeKey) continue;

		if(mti->type==eModifierTypeType_OnlyDeform) return 1;
		else if((sd->flags & SCULPT_ONLY_DEFORM)==0) return 1;
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

	/* Variants */
	float radius;
	float radius_squared;
	//float traced_location[3];
	float true_location[3];
	float location[3];

	float pen_flip;
	float invert;
	float pressure;
	float mouse[2];
	float bstrength;
	float tex_mouse[2];

	/* The rest is temporary storage that isn't saved as a property */

	int first_time; /* Beginning of stroke may do some things special */

	/* from ED_view3d_ob_project_mat_get() */
	float projection_mat[4][4];

	/* Clean this up! */
	ViewContext *vc;
	Brush *brush;

	float (*face_norms)[3]; /* Copy of the mesh faces' normals */
	float special_rotation; /* Texture rotation (radians) for anchored and rake modes */
	int pixel_radius, previous_pixel_radius;
	float grab_delta[3], grab_delta_symmetry[3];
	float old_grab_location[3], orig_grab_location[3];

	int symmetry; /* Symmetry index between 0 and 7 bit combo 0 is Brush only;
	               * 1 is X mirror; 2 is Y mirror; 3 is XY; 4 is Z; 5 is XZ; 6 is YZ; 7 is XYZ */
	int mirror_symmetry_pass; /* the symmetry pass we are currently on between 0 and 7*/
	float true_view_normal[3];
	float view_normal[3];
	float last_area_normal[3];
	float last_center[3];
	int radial_symmetry_pass;
	float symm_rot_mat[4][4];
	float symm_rot_mat_inv[4][4];
	float last_rake[2]; /* Last location of updating rake rotation */
	int original;

	float vertex_rotation;

	char saved_active_brush_name[MAX_ID_NAME];
	int alt_smooth;

	float plane_trim_squared;

	rcti previous_r; /* previous redraw rectangle */
} StrokeCache;

/*** BVH Tree ***/

/* Get a screen-space rectangle of the modified area */
static int sculpt_get_redraw_rect(ARegion *ar, RegionView3D *rv3d,
								  Object *ob, rcti *rect)
{
	SculptSession *ss;
	PBVH *pbvh= ob->sculpt->pbvh;
	float bb_min[3], bb_max[3];

	if(!pbvh)
		return 0;

	BLI_pbvh_redraw_BB(pbvh, bb_min, bb_max);

	/* convert 3D bounding box to screen space */
	if(!paint_convert_bb_to_rect(rect,
								 bb_min,
								 bb_max,
								 ar,
								 rv3d,
								 ob)) {
		return 0;
	}

	/* expand redraw rect with redraw rect from previous step to
	 * prevent partial-redraw issues caused by fast strokes. This is
	 * needed here (not in sculpt_flush_update) as it was before
	 * because redraw rectangle should be the same in both of
	 * optimized PBVH draw function and 3d view redraw (if not -- some
	 * mesh parts could disappear from screen (sergey) */
	ss = ob->sculpt;
	if(ss->cache) {
		if(!BLI_rcti_is_empty(&ss->cache->previous_r))
			BLI_union_rcti(rect, &ss->cache->previous_r);
	}

	return 1;
}

void sculpt_get_redraw_planes(float planes[4][4], ARegion *ar,
				  RegionView3D *rv3d, Object *ob)
{
	PBVH *pbvh= ob->sculpt->pbvh;
	rcti rect;

	sculpt_get_redraw_rect(ar, rv3d, ob, &rect);

	paint_calc_redraw_planes(planes, ar, rv3d, ob, &rect);

	/* clear redraw flag from nodes */
	if(pbvh)
		BLI_pbvh_update(pbvh, PBVH_UpdateRedraw, NULL);
}

/************************ Brush Testing *******************/

typedef struct SculptBrushTest {
	float radius_squared;
	float location[3];
	float dist;
} SculptBrushTest;

static void sculpt_brush_test_init(SculptSession *ss, SculptBrushTest *test)
{
	test->radius_squared= ss->cache->radius_squared;
	copy_v3_v3(test->location, ss->cache->location);
	test->dist= 0.0f;	/* just for initialize */
}

static int sculpt_brush_test(SculptBrushTest *test, float co[3])
{
	float distsq = len_squared_v3v3(co, test->location);

	if(distsq <= test->radius_squared) {
		test->dist = sqrt(distsq);
		return 1;
	}
	else {
		return 0;
	}
}

static int sculpt_brush_test_sq(SculptBrushTest *test, float co[3])
{
	float distsq = len_squared_v3v3(co, test->location);

	if(distsq <= test->radius_squared) {
		test->dist = distsq;
		return 1;
	}
	else {
		return 0;
	}
}

static int sculpt_brush_test_fast(SculptBrushTest *test, float co[3])
{
	return len_squared_v3v3(co, test->location) <= test->radius_squared;
}

static int sculpt_brush_test_cube(SculptBrushTest *test, float co[3], float local[4][4])
{
	static const float side = 0.70710678118654752440084436210485; // sqrt(.5);

	float local_co[3];

	mul_v3_m4v3(local_co, local, co);

	local_co[0] = fabs(local_co[0]);
	local_co[1] = fabs(local_co[1]);
	local_co[2] = fabs(local_co[2]);

	if (local_co[0] <= side && local_co[1] <= side && local_co[2] <= side) {
		test->dist = MAX3(local_co[0], local_co[1], local_co[2]) / side;

		return 1;
	}
	else {
		return 0;
	}
}

static float frontface(Brush *brush, const float sculpt_normal[3],
		       const short no[3], const float fno[3])
{
	if (brush->flag & BRUSH_FRONTFACE) {
		float dot;

		if (no) {
			float tmp[3];

			normal_short_to_float_v3(tmp, no);
			dot= dot_v3v3(tmp, sculpt_normal);
		}
		else {
			dot= dot_v3v3(fno, sculpt_normal);
		}
		return dot > 0 ? dot : 0;
	}
	else {
		return 1;
	}
}

#if 0

static int sculpt_brush_test_cyl(SculptBrushTest *test, float co[3], float location[3], float an[3])
{
	if (sculpt_brush_test_fast(test, co)) {
		float t1[3], t2[3], t3[3], dist;

		sub_v3_v3v3(t1, location, co);
		sub_v3_v3v3(t2, x2, location);

		cross_v3_v3v3(t3, an, t1);

		dist = len_v3(t3)/len_v3(t2);

		test->dist = dist;

		return 1;
	}

	return 0;
}

#endif

/* ===== Sculpting =====
 *
 */
  

static float overlapped_curve(Brush* br, float x)
{
	int i;
	const int n = 100 / br->spacing;
	const float h = br->spacing / 50.0f;
	const float x0 = x-1;

	float sum;

	sum = 0;
	for (i= 0; i < n; i++) {
		float xx;

		xx = fabs(x0 + i*h);

		if (xx < 1.0f)
			sum += brush_curve_strength(br, xx, 1);
	}

	return sum;
}

static float integrate_overlap(Brush* br)
{
	int i;
	int m= 10;
	float g = 1.0f/m;
	float max;

	max= 0;
	for(i= 0; i < m; i++) {
		float overlap= overlapped_curve(br, i*g);

		if (overlap > max)
			max = overlap;
	}

	return max;
}

/* Uses symm to selectively flip any axis of a coordinate. */
static void flip_coord(float out[3], float in[3], const char symm)
{
	if(symm & SCULPT_SYMM_X)
		out[0]= -in[0];
	else
		out[0]= in[0];
	if(symm & SCULPT_SYMM_Y)
		out[1]= -in[1];
	else
		out[1]= in[1];
	if(symm & SCULPT_SYMM_Z)
		out[2]= -in[2];
	else
		out[2]= in[2];
}

static float calc_overlap(StrokeCache *cache, const char symm, const char axis, const float angle)
{
	float mirror[3];
	float distsq;
	
	//flip_coord(mirror, cache->traced_location, symm);
	flip_coord(mirror, cache->true_location, symm);

	if(axis != 0) {
		float mat[4][4]= MAT4_UNITY;
		rotate_m4(mat, axis, angle);
		mul_m4_v3(mat, mirror);
	}

	//distsq = len_squared_v3v3(mirror, cache->traced_location);
	distsq = len_squared_v3v3(mirror, cache->true_location);

	if (distsq <= 4.0f*(cache->radius_squared))
		return (2.0f*(cache->radius) - sqrtf(distsq))  /  (2.0f*(cache->radius));
	else
		return 0;
}

static float calc_radial_symmetry_feather(Sculpt *sd, StrokeCache *cache, const char symm, const char axis)
{
	int i;
	float overlap;

	overlap = 0;
	for(i = 1; i < sd->radial_symm[axis-'X']; ++i) {
		const float angle = 2*M_PI*i/sd->radial_symm[axis-'X'];
		overlap += calc_overlap(cache, symm, axis, angle);
	}

	return overlap;
}

static float calc_symmetry_feather(Sculpt *sd, StrokeCache* cache)
{
	if (sd->flags & SCULPT_SYMMETRY_FEATHER) {
		float overlap;
		int symm = cache->symmetry;
		int i;

		overlap = 0;
		for (i = 0; i <= symm; i++) {
			if(i == 0 || (symm & i && (symm != 5 || i != 3) && (symm != 6 || (i != 3 && i != 5)))) {

				overlap += calc_overlap(cache, i, 0, 0);

				overlap += calc_radial_symmetry_feather(sd, cache, i, 'X');
				overlap += calc_radial_symmetry_feather(sd, cache, i, 'Y');
				overlap += calc_radial_symmetry_feather(sd, cache, i, 'Z');
			}
		}

		return 1/overlap;
	}
	else {
		return 1;
	}
}

/* Return modified brush strength. Includes the direction of the brush, positive
 * values pull vertices, negative values push. Uses tablet pressure and a
 * special multiplier found experimentally to scale the strength factor. */
static float brush_strength(Sculpt *sd, StrokeCache *cache, float feather)
{
	const Scene *scene = cache->vc->scene;
	Brush *brush = paint_brush(&sd->paint);

	/* Primary strength input; square it to make lower values more sensitive */
	const float root_alpha = brush_alpha(scene, brush);
	float alpha        = root_alpha*root_alpha;
	float dir          = brush->flag & BRUSH_DIR_IN ? -1 : 1;
	float pressure     = brush_use_alpha_pressure(scene, brush) ? cache->pressure : 1;
	float pen_flip     = cache->pen_flip ? -1 : 1;
	float invert       = cache->invert ? -1 : 1;
	float accum        = integrate_overlap(brush);
	/* spacing is integer percentage of radius, divide by 50 to get
	 * normalized diameter */
	float overlap      = (brush->flag & BRUSH_SPACE_ATTEN &&
						  brush->flag & BRUSH_SPACE &&
						  !(brush->flag & BRUSH_ANCHORED) &&
						  (brush->spacing < 100)) ? 1.0f/accum : 1;
	float flip         = dir * invert * pen_flip;

	switch(brush->sculpt_tool){
		case SCULPT_TOOL_CLAY:
		case SCULPT_TOOL_CLAY_TUBES:
		case SCULPT_TOOL_DRAW:
		case SCULPT_TOOL_LAYER:
			return alpha * flip * pressure * overlap * feather;

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
				overlap = (1+overlap) / 2;
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
			overlap = (1+overlap) / 2;
			return alpha * pressure * overlap * feather;

		case SCULPT_TOOL_THUMB:
			return alpha*pressure*feather;

		case SCULPT_TOOL_SNAKE_HOOK:
			return feather;

		case SCULPT_TOOL_GRAB:
			return feather;

		case SCULPT_TOOL_ROTATE:
			return alpha*pressure*feather;

		default:
			return 0;
	}
}

/* Return a multiplier for brush strength on a particular vertex. */
static float tex_strength(SculptSession *ss, Brush *br, float point[3],
			  const float len,
			  const float sculpt_normal[3],
			  const short vno[3],
			  const float fno[3])
{
	MTex *mtex = &br->mtex;
	float avg= 1;

	if(!mtex->tex) {
		avg= 1;
	}
	else if(mtex->brush_map_mode == MTEX_MAP_MODE_3D) {
		float jnk;

		/* Get strength by feeding the vertex 
		 * location directly into a texture */
		externtex(mtex, point, &avg,
			  &jnk, &jnk, &jnk, &jnk, 0);
	}
	else if(ss->texcache) {
		float rotation = -mtex->rot;
		float symm_point[3], point_2d[2];
		float x, y;
		float radius;

		/* if the active area is being applied for symmetry, flip it
		 * across the symmetry axis and rotate it back to the original
		 * position in order to project it. This insures that the 
		 * brush texture will be oriented correctly. */

		flip_coord(symm_point, point, ss->cache->mirror_symmetry_pass);

		if (ss->cache->radial_symmetry_pass)
			mul_m4_v3(ss->cache->symm_rot_mat_inv, symm_point);

		ED_view3d_project_float_v2(ss->cache->vc->ar, symm_point, point_2d, ss->cache->projection_mat);

		/* if fixed mode, keep coordinates relative to mouse */
		if(mtex->brush_map_mode == MTEX_MAP_MODE_FIXED) {
			rotation += ss->cache->special_rotation;

			point_2d[0] -= ss->cache->tex_mouse[0];
			point_2d[1] -= ss->cache->tex_mouse[1];

			radius = ss->cache->pixel_radius; // use pressure adjusted size for fixed mode

			x = point_2d[0] + ss->cache->vc->ar->winrct.xmin;
			y = point_2d[1] + ss->cache->vc->ar->winrct.ymin;
		}
		else /* else (mtex->brush_map_mode == MTEX_MAP_MODE_TILED) */
			 /* leave the coordinates relative to the screen */
		{
			radius = brush_size(ss->cache->vc->scene, br); // use unadjusted size for tiled mode
		
			x = point_2d[0];
			y = point_2d[1];
		}

		x /= ss->cache->vc->ar->winx;
		y /= ss->cache->vc->ar->winy;

		if (mtex->brush_map_mode == MTEX_MAP_MODE_TILED) {
			x -= 0.5f;
			y -= 0.5f;
		}
		
		x *= ss->cache->vc->ar->winx / radius;
		y *= ss->cache->vc->ar->winy / radius;

		/* it is probably worth optimizing for those cases where 
		 * the texture is not rotated by skipping the calls to
		 * atan2, sqrtf, sin, and cos. */
		if (rotation > 0.001f || rotation < -0.001f) {
			const float angle    = atan2f(y, x) + rotation;
			const float flen     = sqrtf(x*x + y*y);

			x = flen * cosf(angle);
			y = flen * sinf(angle);
		}

		x *= br->mtex.size[0];
		y *= br->mtex.size[1];

		x += br->mtex.ofs[0];
		y += br->mtex.ofs[1];

		avg = paint_get_tex_pixel(br, x, y);
	}

	avg += br->texture_sample_bias;

	/* Falloff curve */
	avg *= brush_curve_strength(br, len, ss->cache->radius);

	avg *= frontface(br, sculpt_normal, vno, fno);

	return avg;
}

typedef struct {
	Sculpt *sd;
	SculptSession *ss;
	float radius_squared;
	int original;
} SculptSearchSphereData;

/* Test AABB against sphere */
static int sculpt_search_sphere_cb(PBVHNode *node, void *data_v)
{
	SculptSearchSphereData *data = data_v;
	float *center = data->ss->cache->location, nearest[3];
	float t[3], bb_min[3], bb_max[3];
	int i;

	if(data->original)
		BLI_pbvh_node_get_original_BB(node, bb_min, bb_max);
	else
		BLI_pbvh_node_get_BB(node, bb_min, bb_max);
	
	for(i = 0; i < 3; ++i) {
		if(bb_min[i] > center[i])
			nearest[i] = bb_min[i];
		else if(bb_max[i] < center[i])
			nearest[i] = bb_max[i];
		else
			nearest[i] = center[i]; 
	}
	
	sub_v3_v3v3(t, center, nearest);

	return dot_v3v3(t, t) < data->radius_squared;
}

/* Handles clipping against a mirror modifier and SCULPT_LOCK axis flags */
static void sculpt_clip(Sculpt *sd, SculptSession *ss, float *co, const float val[3])
{
	int i;

	for(i=0; i<3; ++i) {
		if(sd->flags & (SCULPT_LOCK_X << i))
			continue;

		if((ss->cache->flag & (CLIP_X << i)) && (fabsf(co[i]) <= ss->cache->clip_tolerance[i]))
			co[i]= 0.0f;
		else
			co[i]= val[i];
	}
}

static void add_norm_if(float view_vec[3], float out[3], float out_flip[3], float fno[3])
{
	if((dot_v3v3(view_vec, fno)) > 0) {
		add_v3_v3(out, fno);
	} else {
		add_v3_v3(out_flip, fno); /* out_flip is used when out is {0,0,0} */
	}
}

static void calc_area_normal(Sculpt *sd, Object *ob, float an[3], PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	int n;

	float out_flip[3] = {0.0f, 0.0f, 0.0f};

	(void)sd; /* unused w/o openmp */
	
	zero_v3(an);

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		SculptUndoNode *unode;
		float private_an[3] = {0.0f, 0.0f, 0.0f};
		float private_out_flip[3] = {0.0f, 0.0f, 0.0f};

		unode = sculpt_undo_push_node(ob, nodes[n]);
		sculpt_brush_test_init(ss, &test);

		if(ss->cache->original) {
			BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
				if(sculpt_brush_test_fast(&test, unode->co[vd.i])) {
					float fno[3];

					normal_short_to_float_v3(fno, unode->no[vd.i]);
					add_norm_if(ss->cache->view_normal, private_an, private_out_flip, fno);
				}
			}
			BLI_pbvh_vertex_iter_end;
		}
		else {
			BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
				if(sculpt_brush_test_fast(&test, vd.co)) {
					if(vd.no) {
						float fno[3];

						normal_short_to_float_v3(fno, vd.no);
						add_norm_if(ss->cache->view_normal, private_an, private_out_flip, fno);
					}
					else {
						add_norm_if(ss->cache->view_normal, private_an, private_out_flip, vd.fno);
					}
				}
			}
			BLI_pbvh_vertex_iter_end;
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

/* This initializes the faces to be moved for this sculpt for draw/layer/flatten; then it
 * finds average normal for all active vertices - note that this is called once for each mirroring direction */
static void calc_sculpt_normal(Sculpt *sd, Object *ob, float an[3], PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = paint_brush(&sd->paint);

	if (ss->cache->mirror_symmetry_pass == 0 &&
	    ss->cache->radial_symmetry_pass == 0 &&
	   (ss->cache->first_time || !(brush->flag & BRUSH_ORIGINAL_NORMAL)))
	{
		switch (brush->sculpt_plane) {
			case SCULPT_DISP_DIR_VIEW:
				ED_view3d_global_to_vector(ss->cache->vc->rv3d, ss->cache->vc->rv3d->twmat[3], an);
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

			default:
				break;
		}

		copy_v3_v3(ss->cache->last_area_normal, an);
	}
	else {
		copy_v3_v3(an, ss->cache->last_area_normal);
		flip_coord(an, an, ss->cache->mirror_symmetry_pass);
		mul_m4_v3(ss->cache->symm_rot_mat, an);
	}
}

/* For the smooth brush, uses the neighboring vertices around vert to calculate
 * a smoothed location for vert. Skips corner vertices (used by only one
 * polygon.) */
static void neighbor_average(SculptSession *ss, float avg[3], unsigned vert)
{
	const int ncount = BLI_countlist(&ss->pmap[vert]);
	const MVert *mvert = ss->mvert;
	float (*deform_co)[3] = ss->deform_cos;

	zero_v3(avg);
		
	/* Don't modify corner vertices */
	if(ncount != 1) {
		IndexNode *node;
		int total = 0;

		for(node = ss->pmap[vert].first; node; node = node->next) {
			const MPoly *p= &ss->mpoly[node->index];
			unsigned f_adj_v[3];

			if(poly_get_adj_loops_from_vert(f_adj_v, p, ss->mloop, vert) != -1) {
				int i;
			
				for (i = 0; i < 3; i++) {
					if (ncount != 2 || BLI_countlist(&ss->pmap[f_adj_v[i]]) <= 2) {
						add_v3_v3(avg, deform_co ? deform_co[f_adj_v[i]] :
												       mvert[f_adj_v[i]].co);

						total++;
					}
				}
			}
		}

		if(total > 0) {
			mul_v3_fl(avg, 1.0f / total);
			return;
		}
	}

	copy_v3_v3(avg, deform_co ? deform_co[vert] : mvert[vert].co);
}

static void do_mesh_smooth_brush(Sculpt *sd, SculptSession *ss, PBVHNode *node, float bstrength)
{
	Brush *brush = paint_brush(&sd->paint);
	PBVHVertexIter vd;
	SculptBrushTest test;
	
	CLAMP(bstrength, 0.0f, 1.0f);

	sculpt_brush_test_init(ss, &test);

	BLI_pbvh_vertex_iter_begin(ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
		if(sculpt_brush_test(&test, vd.co)) {
			const float fade = bstrength*tex_strength(ss, brush, vd.co, test.dist,
								  ss->cache->view_normal, vd.no, vd.fno);
			float avg[3], val[3];

			neighbor_average(ss, avg, vd.vert_indices[vd.i]);
			sub_v3_v3v3(val, avg, vd.co);
			mul_v3_fl(val, fade);

			add_v3_v3(val, vd.co);

			sculpt_clip(sd, ss, vd.co, val);

			if(vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
		}
	}
	BLI_pbvh_vertex_iter_end;
}

static void do_multires_smooth_brush(Sculpt *sd, SculptSession *ss, PBVHNode *node, float bstrength)
{
	Brush *brush = paint_brush(&sd->paint);
	SculptBrushTest test;
	DMGridData **griddata, *data;
	DMGridAdjacency *gridadj, *adj;
	float (*tmpgrid)[3], (*tmprow)[3];
	int v1, v2, v3, v4;
	int *grid_indices, totgrid, gridsize, i, x, y;

	sculpt_brush_test_init(ss, &test);

	CLAMP(bstrength, 0.0f, 1.0f);

	BLI_pbvh_node_get_grids(ss->pbvh, node, &grid_indices, &totgrid,
		NULL, &gridsize, &griddata, &gridadj);

	#pragma omp critical
	{
		tmpgrid= MEM_mallocN(sizeof(float)*3*gridsize*gridsize, "tmpgrid");
		tmprow=  MEM_mallocN(sizeof(float)*3*gridsize, "tmprow");
	}

	for(i = 0; i < totgrid; ++i) {
		data = griddata[grid_indices[i]];
		adj = &gridadj[grid_indices[i]];

		memset(tmpgrid, 0, sizeof(float)*3*gridsize*gridsize);

		for (y= 0; y < gridsize-1; y++) {
			float tmp[3];

			v1 = y*gridsize;
			add_v3_v3v3(tmprow[0], data[v1].co, data[v1+gridsize].co);

			for (x= 0; x < gridsize-1; x++) {
				v1 = x + y*gridsize;
				v2 = v1 + 1;
				v3 = v1 + gridsize;
				v4 = v3 + 1;

				add_v3_v3v3(tmprow[x+1], data[v2].co, data[v4].co);
				add_v3_v3v3(tmp, tmprow[x+1], tmprow[x]);

				add_v3_v3(tmpgrid[v1], tmp);
				add_v3_v3(tmpgrid[v2], tmp);
				add_v3_v3(tmpgrid[v3], tmp);
				add_v3_v3(tmpgrid[v4], tmp);
			}
		}

		/* blend with existing coordinates */
		for(y = 0; y < gridsize; ++y) {
			for(x = 0; x < gridsize; ++x) {
				float *co;
				float *fno;
				int index;

				if(x == 0 && adj->index[0] == -1)
					continue;

				if(x == gridsize - 1 && adj->index[2] == -1)
					continue;

				if(y == 0 && adj->index[3] == -1)
					continue;

				if(y == gridsize - 1 && adj->index[1] == -1)
					continue;

				index = x + y*gridsize;
				co=  data[index].co;
				fno= data[index].no;

				if(sculpt_brush_test(&test, co)) {
					const float fade = bstrength*tex_strength(ss, brush, co, test.dist,
										  ss->cache->view_normal, NULL, fno);
					float *avg, val[3];
					float n;

					avg = tmpgrid[x + y*gridsize];

					n = 1/16.0f;

					if(x == 0 || x == gridsize - 1)
						n *= 2;

					if(y == 0 || y == gridsize - 1)
						n *= 2;

					mul_v3_fl(avg, n);

					sub_v3_v3v3(val, avg, co);
					mul_v3_fl(val, fade);

					add_v3_v3(val, co);

					sculpt_clip(sd, ss, co, val);
				}
			}
		}
	}

	#pragma omp critical
	{
		MEM_freeN(tmpgrid);
		MEM_freeN(tmprow);
	}
}

static void smooth(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float bstrength)
{
	SculptSession *ss = ob->sculpt;
	const int max_iterations = 4;
	const float fract = 1.0f/max_iterations;
	int iteration, n, count;
	float last;

	CLAMP(bstrength, 0, 1);

	count = (int)(bstrength*max_iterations);
	last  = max_iterations*(bstrength - count*fract);

	for(iteration = 0; iteration <= count; ++iteration) {
		#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
		for(n=0; n<totnode; n++) {
			if(ss->multires) {
				do_multires_smooth_brush(sd, ss, nodes[n], iteration != count ? 1.0f : last);
			}
			else if(ss->pmap)
				do_mesh_smooth_brush(sd, ss, nodes[n], iteration != count ? 1.0f : last);
		}

		if(ss->multires)
			multires_stitch_grids(ob);
	}
}

static void do_smooth_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	smooth(sd, ob, nodes, totnode, ss->cache->bstrength);
}

static void do_draw_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = paint_brush(&sd->paint);
	float offset[3], area_normal[3];
	float bstrength= ss->cache->bstrength;
	int n;

	calc_sculpt_normal(sd, ob, area_normal, nodes, totnode);
	
	/* offset with as much as possible factored in already */
	mul_v3_v3fl(offset, area_normal, ss->cache->radius);
	mul_v3_v3(offset, ss->cache->scale);
	mul_v3_fl(offset, bstrength);

	/* threaded loop over nodes */
	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if (sculpt_brush_test(&test, vd.co)) {
				/* offset vertex */
				float fade = tex_strength(ss, brush, vd.co, test.dist,
							  area_normal, vd.no, vd.fno);

				mul_v3_v3fl(proxy[vd.i], offset, fade);

				if(vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_crease_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	const Scene *scene = ss->cache->vc->scene;
	Brush *brush = paint_brush(&sd->paint);
	float offset[3], area_normal[3];
	float bstrength= ss->cache->bstrength;
	float flippedbstrength, crease_correction;
	int n;

	calc_sculpt_normal(sd, ob, area_normal, nodes, totnode);
	
	/* offset with as much as possible factored in already */
	mul_v3_v3fl(offset, area_normal, ss->cache->radius);
	mul_v3_v3(offset, ss->cache->scale);
	mul_v3_fl(offset, bstrength);
	
	/* we divide out the squared alpha and multiply by the squared crease to give us the pinch strength */
	
	if(brush_alpha(scene, brush) > 0.0f)
		crease_correction = brush->crease_pinch_factor*brush->crease_pinch_factor/(brush_alpha(scene, brush)*brush_alpha(scene, brush));
	else
		crease_correction = brush->crease_pinch_factor*brush->crease_pinch_factor;

	/* we always want crease to pinch or blob to relax even when draw is negative */
	flippedbstrength = (bstrength < 0) ? -crease_correction*bstrength : crease_correction*bstrength;

	if(brush->sculpt_tool == SCULPT_TOOL_BLOB) flippedbstrength *= -1.0f;

	/* threaded loop over nodes */
	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(sculpt_brush_test(&test, vd.co)) {
				/* offset vertex */
				const float fade = tex_strength(ss, brush, vd.co, test.dist,
				                                area_normal, vd.no, vd.fno);
				float val1[3];
				float val2[3];

				/* first we pinch */
				sub_v3_v3v3(val1, test.location, vd.co);
				//mul_v3_v3(val1, ss->cache->scale);
				mul_v3_fl(val1, fade*flippedbstrength);

				/* then we draw */
				mul_v3_v3fl(val2, offset, fade);

				add_v3_v3v3(proxy[vd.i], val1, val2);

				if(vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_pinch_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = paint_brush(&sd->paint);
	float bstrength= ss->cache->bstrength;
	int n;

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(sculpt_brush_test(&test, vd.co)) {
				float fade = bstrength*tex_strength(ss, brush, vd.co, test.dist,
								    ss->cache->view_normal, vd.no, vd.fno);
				float val[3];

				sub_v3_v3v3(val, test.location, vd.co);
				mul_v3_v3fl(proxy[vd.i], val, fade);

				if(vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_grab_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush= paint_brush(&sd->paint);
	float bstrength= ss->cache->bstrength;
	float grab_delta[3], an[3];
	int n;
	float len;

	if (brush->normal_weight > 0 || brush->flag & BRUSH_FRONTFACE) {
		int cache= 1;
		/* grab brush requires to test on original data */
		SWAP(int, ss->cache->original, cache);
		calc_sculpt_normal(sd, ob, an, nodes, totnode);
		SWAP(int, ss->cache->original, cache);
	}
	
	copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

	len = len_v3(grab_delta);

	if (brush->normal_weight > 0) {
		mul_v3_fl(an, len*brush->normal_weight);
		mul_v3_fl(grab_delta, 1.0f - brush->normal_weight);
		add_v3_v3(grab_delta, an);
	}

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		SculptUndoNode* unode;
		SculptBrushTest test;
		float (*origco)[3];
		short (*origno)[3];
		float (*proxy)[3];

		unode=  sculpt_undo_push_node(ob, nodes[n]);
		origco= unode->co;
		origno= unode->no;

		proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(sculpt_brush_test(&test, origco[vd.i])) {
				const float fade = bstrength*tex_strength(ss, brush, origco[vd.i], test.dist,
				                                          an, origno[vd.i], NULL);

				mul_v3_v3fl(proxy[vd.i], grab_delta, fade);

				if(vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_nudge_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = paint_brush(&sd->paint);
	float bstrength = ss->cache->bstrength;
	float grab_delta[3];
	int n;
	float an[3];
	float tmp[3], cono[3];

	copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

	calc_sculpt_normal(sd, ob, an, nodes, totnode);

	cross_v3_v3v3(tmp, an, grab_delta);
	cross_v3_v3v3(cono, tmp, an);

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for(n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(sculpt_brush_test(&test, vd.co)) {
				const float fade = bstrength*tex_strength(ss, brush, vd.co, test.dist,
									  an, vd.no, vd.fno);

				mul_v3_v3fl(proxy[vd.i], cono, fade);

				if(vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_snake_hook_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = paint_brush(&sd->paint);
	float bstrength = ss->cache->bstrength;
	float grab_delta[3], an[3];
	int n;
	float len;

	if (brush->normal_weight > 0 || brush->flag & BRUSH_FRONTFACE)
		calc_sculpt_normal(sd, ob, an, nodes, totnode);

	copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

	len = len_v3(grab_delta);

	if (bstrength < 0)
		negate_v3(grab_delta);

	if (brush->normal_weight > 0) {
		mul_v3_fl(an, len*brush->normal_weight);
		mul_v3_fl(grab_delta, 1.0f - brush->normal_weight);
		add_v3_v3(grab_delta, an);
	}

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for(n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(sculpt_brush_test(&test, vd.co)) {
				const float fade = bstrength*tex_strength(ss, brush, vd.co, test.dist,
				                                          an, vd.no, vd.fno);

				mul_v3_v3fl(proxy[vd.i], grab_delta, fade);

				if(vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_thumb_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = paint_brush(&sd->paint);
	float bstrength = ss->cache->bstrength;
	float grab_delta[3];
	int n;
	float an[3];
	float tmp[3], cono[3];

	copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

	calc_sculpt_normal(sd, ob, an, nodes, totnode);

	cross_v3_v3v3(tmp, an, grab_delta);
	cross_v3_v3v3(cono, tmp, an);

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for(n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptUndoNode* unode;
		SculptBrushTest test;
		float (*origco)[3];
		short (*origno)[3];
		float (*proxy)[3];

		unode=  sculpt_undo_push_node(ob, nodes[n]);
		origco= unode->co;
		origno= unode->no;

		proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(sculpt_brush_test(&test, origco[vd.i])) {
				const float fade = bstrength*tex_strength(ss, brush, origco[vd.i], test.dist,
									  an, origno[vd.i], NULL);

				mul_v3_v3fl(proxy[vd.i], cono, fade);

				if(vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_rotate_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush= paint_brush(&sd->paint);
	float bstrength= ss->cache->bstrength;
	float an[3];
	int n;
	float m[4][4], rot[4][4], lmat[4][4], ilmat[4][4];
	static const int flip[8] = { 1, -1, -1, 1, -1, 1, 1, -1 };
	float angle = ss->cache->vertex_rotation * flip[ss->cache->mirror_symmetry_pass];

	calc_sculpt_normal(sd, ob, an, nodes, totnode);

	unit_m4(m);
	unit_m4(lmat);

	copy_v3_v3(lmat[3], ss->cache->location);
	invert_m4_m4(ilmat, lmat);
	axis_angle_to_mat4(rot, an, angle);

	mul_serie_m4(m, lmat, rot, ilmat, NULL, NULL, NULL, NULL, NULL);

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		SculptUndoNode* unode;
		SculptBrushTest test;
		float (*origco)[3];
		short (*origno)[3];
		float (*proxy)[3];

		unode=  sculpt_undo_push_node(ob, nodes[n]);
		origco= unode->co;
		origno= unode->no;

		proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(sculpt_brush_test(&test, origco[vd.i])) {
				const float fade = bstrength*tex_strength(ss, brush, origco[vd.i], test.dist,
				                                          an, origno[vd.i], NULL);

				mul_v3_m4v3(proxy[vd.i], m, origco[vd.i]);
				sub_v3_v3(proxy[vd.i], origco[vd.i]);
				mul_v3_fl(proxy[vd.i], fade);

				if(vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_layer_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = paint_brush(&sd->paint);
	float bstrength= ss->cache->bstrength;
	float area_normal[3], offset[3];
	float lim= brush->height;
	int n;

	if(bstrength < 0)
		lim = -lim;

	calc_sculpt_normal(sd, ob, area_normal, nodes, totnode);

	mul_v3_v3v3(offset, ss->cache->scale, area_normal);

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		SculptUndoNode *unode;
		float (*origco)[3], *layer_disp;
		//float (*proxy)[3]; // XXX layer brush needs conversion to proxy but its more complicated

		//proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;
		
		unode= sculpt_undo_push_node(ob, nodes[n]);
		origco=unode->co;
		if (!unode->layer_disp) {
			#pragma omp critical 
			unode->layer_disp= MEM_callocN(sizeof(float)*unode->totvert, "layer disp");
		}

		layer_disp= unode->layer_disp;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(sculpt_brush_test(&test, origco[vd.i])) {
				const float fade = bstrength*tex_strength(ss, brush, vd.co, test.dist,
									  area_normal, vd.no, vd.fno);
				float *disp= &layer_disp[vd.i];
				float val[3];

				*disp+= fade;

				/* Don't let the displacement go past the limit */
				if((lim < 0 && *disp < lim) || (lim >= 0 && *disp > lim))
					*disp = lim;

				mul_v3_v3fl(val, offset, *disp);

				if(ss->layer_co && (brush->flag & BRUSH_PERSISTENT)) {
					int index= vd.vert_indices[vd.i];

					/* persistent base */
					add_v3_v3(val, ss->layer_co[index]);
				}
				else {
					add_v3_v3(val, origco[vd.i]);
				}

				sculpt_clip(sd, ss, vd.co, val);

				if(vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_inflate_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = paint_brush(&sd->paint);
	float bstrength= ss->cache->bstrength;
	int n;

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(sculpt_brush_test(&test, vd.co)) {
				const float fade = bstrength*tex_strength(ss, brush, vd.co, test.dist,
				                                          ss->cache->view_normal, vd.no, vd.fno);
				float val[3];

				if(vd.fno) copy_v3_v3(val, vd.fno);
				else normal_short_to_float_v3(val, vd.no);
				
				mul_v3_fl(val, fade * ss->cache->radius);
				mul_v3_v3v3(proxy[vd.i], val, ss->cache->scale);

				if(vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void calc_flatten_center(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float fc[3])
{
	SculptSession *ss = ob->sculpt;
	int n;

	float count = 0;

	(void)sd; /* unused w/o openmp */

	zero_v3(fc);

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		SculptUndoNode *unode;
		float private_fc[3] = {0.0f, 0.0f, 0.0f};
		int private_count = 0;

		unode = sculpt_undo_push_node(ob, nodes[n]);
		sculpt_brush_test_init(ss, &test);

		if(ss->cache->original) {
			BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
				if(sculpt_brush_test_fast(&test, unode->co[vd.i])) {
					add_v3_v3(private_fc, unode->co[vd.i]);
					private_count++;
				}
			}
			BLI_pbvh_vertex_iter_end;
		}
		else {
			BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
				if(sculpt_brush_test_fast(&test, vd.co)) {
					add_v3_v3(private_fc, vd.co);
					private_count++;
				}
			}
			BLI_pbvh_vertex_iter_end;
		}

		#pragma omp critical
		{
			add_v3_v3(fc, private_fc);
			count += private_count;
		}
	}

	mul_v3_fl(fc, 1.0f / count);
}

/* this calculates flatten center and area normal together, 
 * amortizing the memory bandwidth and loop overhead to calculate both at the same time */
static void calc_area_normal_and_flatten_center(Sculpt *sd, Object *ob,
												PBVHNode **nodes, int totnode,
												float an[3], float fc[3])
{
	SculptSession *ss = ob->sculpt;
	int n;

	// an
	float out_flip[3] = {0.0f, 0.0f, 0.0f};

	// fc
	float count = 0;

	(void)sd; /* unused w/o openmp */
	
	// an
	zero_v3(an);

	// fc
	zero_v3(fc);

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		SculptUndoNode *unode;
		float private_an[3] = {0.0f, 0.0f, 0.0f};
		float private_out_flip[3] = {0.0f, 0.0f, 0.0f};
		float private_fc[3] = {0.0f, 0.0f, 0.0f};
		int private_count = 0;

		unode = sculpt_undo_push_node(ob, nodes[n]);
		sculpt_brush_test_init(ss, &test);

		if(ss->cache->original) {
			BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
				if(sculpt_brush_test_fast(&test, unode->co[vd.i])) {
					// an
					float fno[3];

					normal_short_to_float_v3(fno, unode->no[vd.i]);
					add_norm_if(ss->cache->view_normal, private_an, private_out_flip, fno);

					// fc
					add_v3_v3(private_fc, unode->co[vd.i]);
					private_count++;
				}
			}
			BLI_pbvh_vertex_iter_end;
		}
		else {
			BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
				if(sculpt_brush_test_fast(&test, vd.co)) {
					// an
					if(vd.no) {
						float fno[3];

						normal_short_to_float_v3(fno, vd.no);
						add_norm_if(ss->cache->view_normal, private_an, private_out_flip, fno);
					}
					else {
						add_norm_if(ss->cache->view_normal, private_an, private_out_flip, vd.fno);
					}

					// fc
					add_v3_v3(private_fc, vd.co);
					private_count++;
				}
			}
			BLI_pbvh_vertex_iter_end;
		}

		#pragma omp critical
		{
			// an
			add_v3_v3(an, private_an);
			add_v3_v3(out_flip, private_out_flip);

			// fc
			add_v3_v3(fc, private_fc);
			count += private_count;
		}
	}

	// an
	if (is_zero_v3(an))
		copy_v3_v3(an, out_flip);

	normalize_v3(an);

	// fc
	if (count != 0) {
		mul_v3_fl(fc, 1.0f / count);
	}
	else {
		zero_v3(fc);
	}
}

static void calc_sculpt_plane(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float an[3], float fc[3])
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = paint_brush(&sd->paint);

	if (ss->cache->mirror_symmetry_pass == 0 &&
	    ss->cache->radial_symmetry_pass == 0 &&
	   (ss->cache->first_time || !(brush->flag & BRUSH_ORIGINAL_NORMAL)))
	{
		switch (brush->sculpt_plane) {
			case SCULPT_DISP_DIR_VIEW:
				ED_view3d_global_to_vector(ss->cache->vc->rv3d, ss->cache->vc->rv3d->twmat[3], an);
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

			default:
				break;
		}

		// fc
		/* flatten center has not been calculated yet if we are not using the area normal */
		if (brush->sculpt_plane != SCULPT_DISP_DIR_AREA)
			calc_flatten_center(sd, ob, nodes, totnode, fc);

		// an
		copy_v3_v3(ss->cache->last_area_normal, an);

		// fc
		copy_v3_v3(ss->cache->last_center, fc);
	}
	else {
		// an
		copy_v3_v3(an, ss->cache->last_area_normal);

		// fc
		copy_v3_v3(fc, ss->cache->last_center);

		// an
		flip_coord(an, an, ss->cache->mirror_symmetry_pass);

		// fc
		flip_coord(fc, fc, ss->cache->mirror_symmetry_pass);

		// an
		mul_m4_v3(ss->cache->symm_rot_mat, an);

		// fc
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
	return !(brush->flag & BRUSH_PLANE_TRIM) || (dot_v3v3(val, val) <= cache->radius_squared*cache->plane_trim_squared);
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
	return  dot_v3v3(plane_normal, delta) <= 0.0f;
}

static float get_offset(Sculpt *sd, SculptSession *ss)
{
	Brush* brush = paint_brush(&sd->paint);

	float rv = brush->plane_offset;

	if (brush->flag & BRUSH_OFFSET_PRESSURE) {
		rv *= ss->cache->pressure;
	}

	return rv;
}

static void do_flatten_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = paint_brush(&sd->paint);

	float bstrength = ss->cache->bstrength;
	const float radius = ss->cache->radius;

	float an[3];
	float fc[3];

	float offset = get_offset(sd, ss);

	float displace;

	int n;

	float temp[3];

	calc_sculpt_plane(sd, ob, nodes, totnode, an, fc);

	displace = radius*offset;

	mul_v3_v3v3(temp, an, ss->cache->scale);
	mul_v3_fl(temp, displace);
	add_v3_v3(fc, temp);

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for(n = 0; n < totnode; n++) {
		PBVHVertexIter  vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if (sculpt_brush_test_sq(&test, vd.co)) {
				float intr[3];
				float val[3];

				point_plane_project(intr, vd.co, an, fc);

				sub_v3_v3v3(val, intr, vd.co);

				if (plane_trim(ss->cache, brush, val)) {
					const float fade = bstrength*tex_strength(ss, brush, vd.co, sqrt(test.dist),
					                                          an, vd.no, vd.fno);

					mul_v3_v3fl(proxy[vd.i], val, fade);

					if(vd.mvert)
						vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
				}
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_clay_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = paint_brush(&sd->paint);

	float bstrength = ss->cache->bstrength;
	float radius    = ss->cache->radius;
	float offset    = get_offset(sd, ss);
	
	float displace;

	float an[3]; // area normal
	float fc[3]; // flatten center

	int n;

	float temp[3];
	//float p[3];

	int flip;

	calc_sculpt_plane(sd, ob, nodes, totnode, an, fc);

	flip = bstrength < 0;

	if (flip) {
		bstrength = -bstrength;
		radius    = -radius;
	}

	displace = radius * (0.25f+offset);

	mul_v3_v3v3(temp, an, ss->cache->scale);
	mul_v3_fl(temp, displace);
	add_v3_v3(fc, temp);

	//add_v3_v3v3(p, ss->cache->location, an);

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if (sculpt_brush_test_sq(&test, vd.co)) {
				if (plane_point_side_flip(vd.co, an, fc, flip)) {
				//if (sculpt_brush_test_cyl(&test, vd.co, ss->cache->location, p)) {
					float intr[3];
					float val[3];

					point_plane_project(intr, vd.co, an, fc);

					sub_v3_v3v3(val, intr, vd.co);

					if (plane_trim(ss->cache, brush, val)) {
						const float fade = bstrength*tex_strength(ss, brush, vd.co,
											  sqrt(test.dist),
											  an, vd.no, vd.fno);

						mul_v3_v3fl(proxy[vd.i], val, fade);

						if(vd.mvert)
							vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
					}
				}
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_clay_tubes_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = paint_brush(&sd->paint);

	float bstrength = ss->cache->bstrength;
	float radius    = ss->cache->radius;
	float offset    = get_offset(sd, ss);
	
	float displace;

	float sn[3]; // sculpt normal
	float an[3]; // area normal
	float fc[3]; // flatten center

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

	if (ss->cache->first_time)
		return; // delay the first daub because grab delta is not setup

	flip = bstrength < 0;

	if (flip) {
		bstrength = -bstrength;
		radius    = -radius;
	}

	displace = radius * (0.25f+offset);

	mul_v3_v3v3(temp, sn, ss->cache->scale);
	mul_v3_fl(temp, displace);
	add_v3_v3(fc, temp);

	cross_v3_v3v3(mat[0], an, ss->cache->grab_delta_symmetry); mat[0][3] = 0;
	cross_v3_v3v3(mat[1], an, mat[0]); mat[1][3] = 0;
	copy_v3_v3(mat[2], an); mat[2][3] = 0;
	copy_v3_v3(mat[3], ss->cache->location);  mat[3][3] = 1;
	normalize_m4(mat);
	scale_m4_fl(scale, ss->cache->radius);
	mult_m4_m4m4(tmat, mat, scale);
	invert_m4_m4(mat, tmat);

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if (sculpt_brush_test_cube(&test, vd.co, mat)) {
				if (plane_point_side_flip(vd.co, sn, fc, flip)) {
					float intr[3];
					float val[3];

					point_plane_project(intr, vd.co, sn, fc);

					sub_v3_v3v3(val, intr, vd.co);

					if (plane_trim(ss->cache, brush, val)) {
						const float fade = bstrength*tex_strength(ss, brush, vd.co,
						                                          ss->cache->radius*test.dist,
						                                          an, vd.no, vd.fno);

						mul_v3_v3fl(proxy[vd.i], val, fade);

						if(vd.mvert)
							vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
					}
				}
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_fill_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = paint_brush(&sd->paint);

	float bstrength = ss->cache->bstrength;
	const float radius = ss->cache->radius;

	float an[3];
	float fc[3];
	float offset = get_offset(sd, ss);

	float displace;

	int n;

	float temp[3];

	calc_sculpt_plane(sd, ob, nodes, totnode, an, fc);

	displace = radius*offset;

	mul_v3_v3v3(temp, an, ss->cache->scale);
	mul_v3_fl(temp, displace);
	add_v3_v3(fc, temp);

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if (sculpt_brush_test_sq(&test, vd.co)) {
				if (plane_point_side(vd.co, an, fc)) {
					float intr[3];
					float val[3];

					point_plane_project(intr, vd.co, an, fc);

					sub_v3_v3v3(val, intr, vd.co);

					if (plane_trim(ss->cache, brush, val)) {
						const float fade = bstrength*tex_strength(ss, brush, vd.co,
											  sqrt(test.dist),
											  an, vd.no, vd.fno);

						mul_v3_v3fl(proxy[vd.i], val, fade);

						if(vd.mvert)
							vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
					}
				}
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_scrape_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = paint_brush(&sd->paint);

	float bstrength = ss->cache->bstrength;
	const float radius = ss->cache->radius;

	float an[3];
	float fc[3];
	float offset = get_offset(sd, ss);

	float displace;

	int n;

	float temp[3];

	calc_sculpt_plane(sd, ob, nodes, totnode, an, fc);

	displace = -radius*offset;

	mul_v3_v3v3(temp, an, ss->cache->scale);
	mul_v3_fl(temp, displace);
	add_v3_v3(fc, temp);

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if (sculpt_brush_test_sq(&test, vd.co)) {
				if (!plane_point_side(vd.co, an, fc)) {
					float intr[3];
					float val[3];

					point_plane_project(intr, vd.co, an, fc);

					sub_v3_v3v3(val, intr, vd.co);

					if (plane_trim(ss->cache, brush, val)) {
						const float fade = bstrength*tex_strength(ss, brush, vd.co,
						                                          sqrt(test.dist),
						                                          an, vd.no, vd.fno);

						mul_v3_v3fl(proxy[vd.i], val, fade);

						if(vd.mvert)
							vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
					}
				}
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

void sculpt_vertcos_to_key(Object *ob, KeyBlock *kb, float (*vertCos)[3])
{
	Mesh *me= (Mesh*)ob->data;
	float (*ofs)[3]= NULL;
	int a, is_basis= 0;
	KeyBlock *currkey;

	/* for relative keys editing of base should update other keys */
	if (me->key->type == KEY_RELATIVE)
		for (currkey = me->key->block.first; currkey; currkey= currkey->next)
			if(ob->shapenr-1 == currkey->relative) {
				is_basis= 1;
				break;
			}

	if (is_basis) {
		ofs= key_to_vertcos(ob, kb);

		/* calculate key coord offsets (from previous location) */
		for (a= 0; a < me->totvert; a++) {
			sub_v3_v3v3(ofs[a], vertCos[a], ofs[a]);
		}

		/* apply offsets on other keys */
		currkey = me->key->block.first;
		while (currkey) {
			int apply_offset = ((currkey != kb) && (ob->shapenr-1 == currkey->relative));

			if (apply_offset)
				offset_to_key(ob, currkey, ofs);

			currkey= currkey->next;
		}

		MEM_freeN(ofs);
	}

	/* modifying of basis key should update mesh */
	if (kb == me->key->refkey) {
		MVert *mvert= me->mvert;

		for (a= 0; a < me->totvert; a++, mvert++)
			copy_v3_v3(mvert->co, vertCos[a]);

		mesh_calc_normals_mapping(me->mvert, me->totvert, me->mloop, me->mpoly, me->totloop, me->totpoly, NULL, NULL, 0, NULL, NULL);
	}

	/* apply new coords on active key block */
	vertcos_to_key(ob, kb, vertCos);
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
	data.original = ELEM4(brush->sculpt_tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_ROTATE, SCULPT_TOOL_THUMB, SCULPT_TOOL_LAYER);
	BLI_pbvh_search_gather(ss->pbvh, sculpt_search_sphere_cb, &data, &nodes, &totnode);

	/* Only act if some verts are inside the brush area */
	if (totnode) {
		#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
		for (n= 0; n < totnode; n++) {
			sculpt_undo_push_node(ob, nodes[n]);
			BLI_pbvh_node_mark_update(nodes[n]);
		}

		/* Apply one type of brush action */
		switch(brush->sculpt_tool){
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
		case SCULPT_TOOL_CLAY_TUBES:
			do_clay_tubes_brush(sd, ob, nodes, totnode);
			break;
		case SCULPT_TOOL_FILL:
			do_fill_brush(sd, ob, nodes, totnode);
			break;
		case SCULPT_TOOL_SCRAPE:
			do_scrape_brush(sd, ob, nodes, totnode);
			break;
		}

		if (brush->sculpt_tool != SCULPT_TOOL_SMOOTH && brush->autosmooth_factor > 0) {
			if (brush->flag & BRUSH_INVERSE_SMOOTH_PRESSURE) {
				smooth(sd, ob, nodes, totnode, brush->autosmooth_factor*(1-ss->cache->pressure));
			}
			else {
				smooth(sd, ob, nodes, totnode, brush->autosmooth_factor);
			}
		}

		MEM_freeN(nodes);
	}
}

/* flush displacement from deformed PBVH vertex to original mesh */
static void sculpt_flush_pbvhvert_deform(Object *ob, PBVHVertexIter *vd)
{
	SculptSession *ss = ob->sculpt;
	Mesh *me= ob->data;
	float disp[3], newco[3];
	int index= vd->vert_indices[vd->i];

	sub_v3_v3v3(disp, vd->co, ss->deform_cos[index]);
	mul_m3_v3(ss->deform_imats[index], disp);
	add_v3_v3v3(newco, disp, ss->orig_cos[index]);

	copy_v3_v3(ss->deform_cos[index], vd->co);
	copy_v3_v3(ss->orig_cos[index], newco);

	if(!ss->kb)
		copy_v3_v3(me->mvert[index].co, newco);
}

static void sculpt_combine_proxies(Sculpt *sd, Object *ob)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush= paint_brush(&sd->paint);
	PBVHNode** nodes;
	int totnode, n;

	BLI_pbvh_gather_proxies(ss->pbvh, &nodes, &totnode);

	if(!ELEM(brush->sculpt_tool, SCULPT_TOOL_SMOOTH, SCULPT_TOOL_LAYER)) {
		/* these brushes start from original coordinates */
		const int use_orco = (ELEM3(brush->sculpt_tool, SCULPT_TOOL_GRAB,
		                            SCULPT_TOOL_ROTATE, SCULPT_TOOL_THUMB));

		#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
		for (n= 0; n < totnode; n++) {
			PBVHVertexIter vd;
			PBVHProxyNode* proxies;
			int proxy_count;
			float (*orco)[3];

			if(use_orco)
				orco= sculpt_undo_push_node(ob, nodes[n])->co;

			BLI_pbvh_node_get_proxies(nodes[n], &proxies, &proxy_count);

			BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
				float val[3];
				int p;

				if(use_orco)
					copy_v3_v3(val, orco[vd.i]);
				else
					copy_v3_v3(val, vd.co);

				for (p= 0; p < proxy_count; p++)
					add_v3_v3(val, proxies[p].co[vd.i]);

				sculpt_clip(sd, ss, vd.co, val);

				if(ss->modifiers_active)
					sculpt_flush_pbvhvert_deform(ob, &vd);
			}
			BLI_pbvh_vertex_iter_end;

			BLI_pbvh_node_free_proxies(nodes[n]);
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

	/* Keyblock update happens after hadning deformation caused by modifiers,
	 * so ss->orig_cos would be updated with new stroke */
	if(ss->orig_cos) vertCos = ss->orig_cos;
	else vertCos = BLI_pbvh_get_vertCos(ss->pbvh);

	if (vertCos) {
		sculpt_vertcos_to_key(ob, ss->kb, vertCos);

		if(vertCos != ss->orig_cos)
			MEM_freeN(vertCos);
	}
}

/* flush displacement from deformed PBVH to original layer */
static void sculpt_flush_stroke_deform(Sculpt *sd, Object *ob)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush= paint_brush(&sd->paint);

	if(ELEM(brush->sculpt_tool, SCULPT_TOOL_SMOOTH, SCULPT_TOOL_LAYER)) {
		/* this brushes aren't using proxies, so sculpt_combine_proxies() wouldn't
		 * propagate needed deformation to original base */

		int n, totnode;
		Mesh *me= (Mesh*)ob->data;
		PBVHNode** nodes;
		float (*vertCos)[3]= NULL;

		if(ss->kb)
			vertCos= MEM_callocN(sizeof(*vertCos)*me->totvert, "flushStrokeDeofrm keyVerts");

		BLI_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);

		#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
		for (n= 0; n < totnode; n++) {
			PBVHVertexIter vd;

			BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
				sculpt_flush_pbvhvert_deform(ob, &vd);

				if(vertCos) {
					int index= vd.vert_indices[vd.i];
					copy_v3_v3(vertCos[index], ss->orig_cos[index]);
				}
			}
			BLI_pbvh_vertex_iter_end;
		}

		if(vertCos) {
			sculpt_vertcos_to_key(ob, ss->kb, vertCos);
			MEM_freeN(vertCos);
		}

		MEM_freeN(nodes);

		/* Modifiers could depend on mesh normals, so we should update them/
		 * Note, then if sculpting happens on locked key, normals should be re-calculated
		 * after applying coords from keyblock on base mesh */
		mesh_calc_normals(me->mvert, me->totvert, me->mloop, me->mpoly, me->totloop, me->totpoly, NULL);
	} else if (ss->kb)
		sculpt_update_keyblock(ob);
}

//static int max_overlap_count(Sculpt *sd)
//{
//	int count[3];
//	int i, j;
//
//	for (i= 0; i < 3; i++) {
//		count[i] = sd->radial_symm[i];
//
//		for (j= 0; j < 3; j++) {
//			if (i != j && sd->flags & (SCULPT_SYMM_X<<i))
//				count[i] *= 2;
//		}
//	}
//
//	return MAX3(count[0], count[1], count[2]);
//}

/* Flip all the editdata across the axis/axes specified by symm. Used to
 * calculate multiple modifications to the mesh when symmetry is enabled. */
static void calc_brushdata_symm(Sculpt *sd, StrokeCache *cache, const char symm,
								const char axis, const float angle,
								const float UNUSED(feather))
{
	(void)sd; /* unused */

	flip_coord(cache->location, cache->true_location, symm);
	flip_coord(cache->grab_delta_symmetry, cache->grab_delta, symm);
	flip_coord(cache->view_normal, cache->true_view_normal, symm);

	// XXX This reduces the length of the grab delta if it approaches the line of symmetry
	// XXX However, a different approach appears to be needed
	//if (sd->flags & SCULPT_SYMMETRY_FEATHER) {
	//	float frac = 1.0f/max_overlap_count(sd);
	//	float reduce = (feather-frac)/(1-frac);

	//	printf("feather: %f frac: %f reduce: %f\n", feather, frac, reduce);

	//	if (frac < 1)
	//		mul_v3_fl(cache->grab_delta_symmetry, reduce);
	//}

	unit_m4(cache->symm_rot_mat);
	unit_m4(cache->symm_rot_mat_inv);

	if(axis) { /* expects XYZ */
		rotate_m4(cache->symm_rot_mat, axis, angle);
		rotate_m4(cache->symm_rot_mat_inv, axis, -angle);
	}

	mul_m4_v3(cache->symm_rot_mat, cache->location);
	mul_m4_v3(cache->symm_rot_mat, cache->grab_delta_symmetry);
}

static void do_radial_symmetry(Sculpt *sd, Object *ob, Brush *brush,
							   const char symm, const int axis,
							   const float feather)
{
	SculptSession *ss = ob->sculpt;
	int i;

	for(i = 1; i < sd->radial_symm[axis-'X']; ++i) {
		const float angle = 2*M_PI*i/sd->radial_symm[axis-'X'];
		ss->cache->radial_symmetry_pass= i;
		calc_brushdata_symm(sd, ss->cache, symm, axis, angle, feather);
		do_brush_action(sd, ob, brush);
	}
}

/* noise texture gives different values for the same input coord; this
 * can tear a multires mesh during sculpting so do a stitch in this
 * case */
static void sculpt_fix_noise_tear(Sculpt *sd, Object *ob)
{
	SculptSession *ss = ob->sculpt;
	Brush *brush = paint_brush(&sd->paint);
	MTex *mtex = &brush->mtex;

	if(ss->multires && mtex->tex && mtex->tex->type == TEX_NOISE)
		multires_stitch_grids(ob);
}

static void do_symmetrical_brush_actions(Sculpt *sd, Object *ob)
{
	Brush *brush = paint_brush(&sd->paint);
	SculptSession *ss = ob->sculpt;
	StrokeCache *cache = ss->cache;
	const char symm = sd->flags & 7;
	int i;

	float feather = calc_symmetry_feather(sd, ss->cache);

	cache->bstrength= brush_strength(sd, cache, feather);

	cache->symmetry= symm;

	/* symm is a bit combination of XYZ - 1 is mirror X; 2 is Y; 3 is XY; 4 is Z; 5 is XZ; 6 is YZ; 7 is XYZ */ 
	for(i = 0; i <= symm; ++i) {
		if(i == 0 || (symm & i && (symm != 5 || i != 3) && (symm != 6 || (i != 3 && i != 5)))) {
			cache->mirror_symmetry_pass= i;
			cache->radial_symmetry_pass= 0;

			calc_brushdata_symm(sd, cache, i, 0, 0, feather);
			do_brush_action(sd, ob, brush);

			do_radial_symmetry(sd, ob, brush, i, 'X', feather);
			do_radial_symmetry(sd, ob, brush, i, 'Y', feather);
			do_radial_symmetry(sd, ob, brush, i, 'Z', feather);
		}
	}

	sculpt_combine_proxies(sd, ob);

	/* hack to fix noise texture tearing mesh */
	sculpt_fix_noise_tear(sd, ob);

	if (ss->modifiers_active)
		sculpt_flush_stroke_deform(sd, ob);

	cache->first_time= 0;
}

static void sculpt_update_tex(const Scene *scene, Sculpt *sd, SculptSession *ss)
{
	Brush *brush = paint_brush(&sd->paint);
	const int radius= brush_size(scene, brush);

	if(ss->texcache) {
		MEM_freeN(ss->texcache);
		ss->texcache= NULL;
	}

	/* Need to allocate a bigger buffer for bigger brush size */
	ss->texcache_side = 2*radius;
	if(!ss->texcache || ss->texcache_side > ss->texcache_actual) {
		ss->texcache = brush_gen_texture_cache(brush, radius);
		ss->texcache_actual = ss->texcache_side;
	}
}

void sculpt_update_mesh_elements(Scene *scene, Sculpt *sd, Object *ob, int need_pmap)
{
	DerivedMesh *dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
	SculptSession *ss = ob->sculpt;
	MultiresModifierData *mmd= sculpt_multires_active(scene, ob);

	ss->modifiers_active= sculpt_modifiers_active(scene, sd, ob);

	if(!mmd) ss->kb= ob_get_keyblock(ob);
	else ss->kb= NULL;

	if(mmd) {
		ss->multires = mmd;
		ss->totvert = dm->getNumVerts(dm);
		ss->totpoly = dm->getNumPolys(dm);
		ss->mvert= NULL;
		ss->mpoly= NULL;
		ss->mloop= NULL;
		ss->face_normals= NULL;
	}
	else {
		Mesh *me = get_mesh(ob);
		ss->totvert = me->totvert;
		ss->totpoly = me->totpoly;
		ss->mvert = me->mvert;
		ss->mpoly = me->mpoly;
		ss->mloop = me->mloop;
		ss->face_normals = NULL;
		ss->multires = NULL;
	}

	/* BMESH ONLY --- at some point we should move sculpt code to use polygons only - but for now it needs tessfaces */
	BKE_mesh_tessface_ensure(ob->data);

	ss->pbvh = dm->getPBVH(ob, dm);
	ss->pmap = (need_pmap && dm->getPolyMap)? dm->getPolyMap(ob, dm): NULL;

	if(ss->modifiers_active) {
		if(!ss->orig_cos) {
			int a;

			free_sculptsession_deformMats(ss);

			if(ss->kb) ss->orig_cos = key_to_vertcos(ob, ss->kb);
			else ss->orig_cos = mesh_getVertexCos(ob->data, NULL);

			crazyspace_build_sculpt(scene, ob, &ss->deform_imats, &ss->deform_cos);
			BLI_pbvh_apply_vertCos(ss->pbvh, ss->deform_cos);

			for(a = 0; a < ((Mesh*)ob->data)->totvert; ++a)
				invert_m3(ss->deform_imats[a]);
		}
	} else free_sculptsession_deformMats(ss);

	/* if pbvh is deformed, key block is already applied to it */
	if (ss->kb && !BLI_pbvh_isDeformed(ss->pbvh)) {
		float (*vertCos)[3]= key_to_vertcos(ob, ss->kb);

		if (vertCos) {
			/* apply shape keys coordinates to PBVH */
			BLI_pbvh_apply_vertCos(ss->pbvh, vertCos);
			MEM_freeN(vertCos);
		}
	}
}

static int sculpt_mode_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);
	return ob && ob->mode & OB_MODE_SCULPT;
}

int sculpt_poll(bContext *C)
{
	return sculpt_mode_poll(C) && paint_poll(C);
}

static const char *sculpt_tool_name(Sculpt *sd)
{
	Brush *brush = paint_brush(&sd->paint);

	switch(brush->sculpt_tool) {
	case SCULPT_TOOL_DRAW:
		return "Draw Brush"; break;
	case SCULPT_TOOL_SMOOTH:
		return "Smooth Brush"; break;
	case SCULPT_TOOL_CREASE:
		return "Crease Brush"; break;
	case SCULPT_TOOL_BLOB:
		return "Blob Brush"; break;
	case SCULPT_TOOL_PINCH:
		return "Pinch Brush"; break;
	case SCULPT_TOOL_INFLATE:
		return "Inflate Brush"; break;
	case SCULPT_TOOL_GRAB:
		return "Grab Brush"; break;
	case SCULPT_TOOL_NUDGE:
		return "Nudge Brush"; break;
	case SCULPT_TOOL_THUMB:
		return "Thumb Brush"; break;
	case SCULPT_TOOL_LAYER:
		return "Layer Brush"; break;
	case SCULPT_TOOL_FLATTEN:
		return "Flatten Brush"; break;
	case SCULPT_TOOL_CLAY:
		return "Clay Brush"; break;
	case SCULPT_TOOL_CLAY_TUBES:
		return "Clay Tubes Brush"; break;
	case SCULPT_TOOL_FILL:
		return "Fill Brush"; break;
	case SCULPT_TOOL_SCRAPE:
		return "Scrape Brush"; break;
	default:
		return "Sculpting"; break;
	}
}

/**
 * Operator for applying a stroke (various attributes including mouse path)
 * using the current brush. */

static void sculpt_cache_free(StrokeCache *cache)
{
	if(cache->face_norms)
		MEM_freeN(cache->face_norms);
	MEM_freeN(cache);
}

/* Initialize mirror modifier clipping */
static void sculpt_init_mirror_clipping(Object *ob, SculptSession *ss)
{
	ModifierData *md;
	int i;

	for(md= ob->modifiers.first; md; md= md->next) {
		if(md->type==eModifierType_Mirror &&
		   (md->mode & eModifierMode_Realtime)) {
			MirrorModifierData *mmd = (MirrorModifierData*)md;
			
			if(mmd->flag & MOD_MIR_CLIPPING) {
				/* check each axis for mirroring */
				for(i = 0; i < 3; ++i) {
					if(mmd->flag & (MOD_MIR_AXIS_X << i)) {
						/* enable sculpt clipping */
						ss->cache->flag |= CLIP_X << i;
						
						/* update the clip tolerance */
						if(mmd->tolerance >
						   ss->cache->clip_tolerance[i])
							ss->cache->clip_tolerance[i] =
								mmd->tolerance;
					}
				}
			}
		}
	}
}

/* Initialize the stroke cache invariants from operator properties */
static void sculpt_update_cache_invariants(bContext* C, Sculpt *sd, SculptSession *ss, wmOperator *op, wmEvent *event)
{
	StrokeCache *cache = MEM_callocN(sizeof(StrokeCache), "stroke cache");
	Brush *brush = paint_brush(&sd->paint);
	ViewContext *vc = paint_stroke_view_context(op->customdata);
	Object *ob= CTX_data_active_object(C);
	int i;
	int mode;

	ss->cache = cache;

	/* Set scaling adjustment */
	ss->cache->scale[0] = 1.0f / ob->size[0];
	ss->cache->scale[1] = 1.0f / ob->size[1];
	ss->cache->scale[2] = 1.0f / ob->size[2];

	ss->cache->plane_trim_squared = brush->plane_trim * brush->plane_trim;

	ss->cache->flag = 0;

	sculpt_init_mirror_clipping(ob, ss);

	/* Initial mouse location */
	if (event) {
		ss->cache->initial_mouse[0] = event->x;
		ss->cache->initial_mouse[1] = event->y;
	}
	else {
		ss->cache->initial_mouse[0] = 0;
		ss->cache->initial_mouse[1] = 0;
	}

	mode = RNA_enum_get(op->ptr, "mode");
	cache->invert = mode == BRUSH_STROKE_INVERT;
	cache->alt_smooth = mode == BRUSH_STROKE_SMOOTH;

	/* not very nice, but with current events system implementation
	 * we can't handle brush appearance inversion hotkey separately (sergey) */
	if(cache->invert) brush->flag |= BRUSH_INVERTED;
	else brush->flag &= ~BRUSH_INVERTED;

	/* Alt-Smooth */
	if (ss->cache->alt_smooth) {
		Paint *p= &sd->paint;
		Brush *br;
		
		BLI_strncpy(cache->saved_active_brush_name, brush->id.name+2, sizeof(cache->saved_active_brush_name));

		br= (Brush *)find_id("BR", "Smooth");
		if(br) {
			paint_brush_set(p, br);
			brush = br;
		}
	}

	copy_v2_v2(cache->mouse, cache->initial_mouse);
	copy_v2_v2(cache->tex_mouse, cache->initial_mouse);

	/* Truly temporary data that isn't stored in properties */

	cache->vc = vc;

	cache->brush = brush;

	/* cache projection matrix */
	ED_view3d_ob_project_mat_get(cache->vc->rv3d, ob, cache->projection_mat);

	ED_view3d_global_to_vector(cache->vc->rv3d, cache->vc->rv3d->twmat[3], cache->true_view_normal);
	/* Initialize layer brush displacements and persistent coords */
	if(brush->sculpt_tool == SCULPT_TOOL_LAYER) {
		/* not supported yet for multires */
		if(!ss->multires && !ss->layer_co && (brush->flag & BRUSH_PERSISTENT)) {
			if(!ss->layer_co)
				ss->layer_co= MEM_mallocN(sizeof(float) * 3 * ss->totvert,
									   "sculpt mesh vertices copy");

			if(ss->deform_cos) memcpy(ss->layer_co, ss->deform_cos, ss->totvert);
			else {
				for(i = 0; i < ss->totvert; ++i) {
					copy_v3_v3(ss->layer_co[i], ss->mvert[i].co);
				}
			}
		}
	}

	/* Make copies of the mesh vertex locations and normals for some tools */
	if(brush->flag & BRUSH_ANCHORED) {
		if(ss->face_normals) {
			float *fn = ss->face_normals;
			cache->face_norms= MEM_mallocN(sizeof(float) * 3 * ss->totpoly, "Sculpt face norms");
			for(i = 0; i < ss->totpoly; ++i, fn += 3)
				copy_v3_v3(cache->face_norms[i], fn);
		}

		cache->original = 1;
	}

	if(ELEM8(brush->sculpt_tool,
			 SCULPT_TOOL_DRAW, SCULPT_TOOL_CREASE, SCULPT_TOOL_BLOB,
			 SCULPT_TOOL_LAYER, SCULPT_TOOL_INFLATE, SCULPT_TOOL_CLAY,
			 SCULPT_TOOL_CLAY_TUBES, SCULPT_TOOL_ROTATE))
		if(!(brush->flag & BRUSH_ACCUMULATE))
			cache->original = 1;

	cache->special_rotation = (brush->flag & BRUSH_RAKE) ? sd->last_angle : 0;
	//cache->last_rake[0] = sd->last_x;
	//cache->last_rake[1] = sd->last_y;

	cache->first_time= 1;

	cache->vertex_rotation= 0;
}

static void sculpt_update_brush_delta(Sculpt *sd, Object *ob, Brush *brush)
{
	SculptSession *ss = ob->sculpt;
	StrokeCache *cache = ss->cache;
	int tool = brush->sculpt_tool;

	if(ELEM5(tool,
		 SCULPT_TOOL_GRAB, SCULPT_TOOL_NUDGE,
		 SCULPT_TOOL_CLAY_TUBES, SCULPT_TOOL_SNAKE_HOOK,
		 SCULPT_TOOL_THUMB)) {
		float grab_location[3], imat[4][4], delta[3], loc[3];

		if(cache->first_time) {
			copy_v3_v3(cache->orig_grab_location,
			           cache->true_location);
		}
		else if(tool == SCULPT_TOOL_SNAKE_HOOK)
			add_v3_v3(cache->true_location, cache->grab_delta);

		/* compute 3d coordinate at same z from original location + mouse */
		mul_v3_m4v3(loc, ob->obmat, cache->orig_grab_location);
		initgrabz(cache->vc->rv3d, loc[0], loc[1], loc[2]);

		ED_view3d_win_to_delta(cache->vc->ar, cache->mouse, grab_location);

		/* compute delta to move verts by */
		if(!cache->first_time) {
			switch(tool) {
			case SCULPT_TOOL_GRAB:
			case SCULPT_TOOL_THUMB:
				sub_v3_v3v3(delta, grab_location, cache->old_grab_location);
				invert_m4_m4(imat, ob->obmat);
				mul_mat3_m4_v3(imat, delta);
				add_v3_v3(cache->grab_delta, delta);
				break;
			case SCULPT_TOOL_CLAY_TUBES:
			case SCULPT_TOOL_NUDGE:
				sub_v3_v3v3(cache->grab_delta, grab_location, cache->old_grab_location);
				invert_m4_m4(imat, ob->obmat);
				mul_mat3_m4_v3(imat, cache->grab_delta);
				break;
			case SCULPT_TOOL_SNAKE_HOOK:
				sub_v3_v3v3(cache->grab_delta, grab_location, cache->old_grab_location);
				invert_m4_m4(imat, ob->obmat);
				mul_mat3_m4_v3(imat, cache->grab_delta);
				break;
			}
		}
		else {
			zero_v3(cache->grab_delta);
		}

		copy_v3_v3(cache->old_grab_location, grab_location);

		if(tool == SCULPT_TOOL_GRAB)
			copy_v3_v3(sd->anchored_location, cache->true_location);
		else if(tool == SCULPT_TOOL_THUMB)
			copy_v3_v3(sd->anchored_location, cache->orig_grab_location);			

		if(ELEM(tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_THUMB)) {
			/* location stays the same for finding vertices in brush radius */
			copy_v3_v3(cache->true_location, cache->orig_grab_location);

			sd->draw_anchored = 1;
			copy_v2_v2(sd->anchored_initial_mouse, cache->initial_mouse);
			sd->anchored_size = cache->pixel_radius;
		}
	}
}

/* Initialize the stroke cache variants from operator properties */
static void sculpt_update_cache_variants(bContext *C, Sculpt *sd, Object *ob,
										 struct PaintStroke *stroke,
										 PointerRNA *ptr)
{
	Scene *scene = CTX_data_scene(C);
	SculptSession *ss = ob->sculpt;
	StrokeCache *cache = ss->cache;
	Brush *brush = paint_brush(&sd->paint);

	int dx, dy;

	//RNA_float_get_array(ptr, "location", cache->traced_location);

	if (cache->first_time ||
	    !((brush->flag & BRUSH_ANCHORED)||
	      (brush->sculpt_tool == SCULPT_TOOL_SNAKE_HOOK)||
	      (brush->sculpt_tool == SCULPT_TOOL_ROTATE))
		 )
	{
		RNA_float_get_array(ptr, "location", cache->true_location);
	}

	cache->pen_flip = RNA_boolean_get(ptr, "pen_flip");
	RNA_float_get_array(ptr, "mouse", cache->mouse);

	/* XXX: Use preassure value from first brush step for brushes which don't
	 *      support strokes (grab, thumb). They depends on initial state and
	 *      brush coord/pressure/etc.
	 *      It's more an events design issue, which doesn't split coordinate/pressure/angle
	 *      changing events. We should avoid this after events system re-design */
	if(paint_space_stroke_enabled(brush) || cache->first_time)
		cache->pressure = RNA_float_get(ptr, "pressure");

	/* Truly temporary data that isn't stored in properties */

	sd->draw_pressure=  1;
	sd->pressure_value= cache->pressure;

	cache->previous_pixel_radius = cache->pixel_radius;
	cache->pixel_radius = brush_size(scene, brush);

	if(cache->first_time) {
		if (!brush_use_locked_size(scene, brush)) {
			cache->initial_radius= paint_calc_object_space_radius(cache->vc, cache->true_location, brush_size(scene, brush));
			brush_set_unprojected_radius(scene, brush, cache->initial_radius);
		}
		else {
			cache->initial_radius= brush_unprojected_radius(scene, brush);
		}
	}

	if(brush_use_size_pressure(scene, brush)) {
		cache->pixel_radius *= cache->pressure;
		cache->radius= cache->initial_radius * cache->pressure;
	}
	else
		cache->radius= cache->initial_radius;

	cache->radius_squared = cache->radius*cache->radius;

	if(!(brush->flag & BRUSH_ANCHORED ||
		 ELEM4(brush->sculpt_tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_SNAKE_HOOK,
			   SCULPT_TOOL_THUMB, SCULPT_TOOL_ROTATE))) {
		copy_v2_v2(cache->tex_mouse, cache->mouse);

		if  ( (brush->mtex.brush_map_mode == MTEX_MAP_MODE_FIXED) &&
			  (brush->flag & BRUSH_RANDOM_ROTATION) &&
			 !(brush->flag & BRUSH_RAKE))
		{
			cache->special_rotation = 2.0f*(float)M_PI*BLI_frand();
		}
	}

	if(brush->flag & BRUSH_ANCHORED) {
		int hit = 0;

		dx = cache->mouse[0] - cache->initial_mouse[0];
		dy = cache->mouse[1] - cache->initial_mouse[1];

		sd->anchored_size = cache->pixel_radius = sqrt(dx*dx + dy*dy);

		cache->special_rotation = atan2(dx, dy) + M_PI;

		if (brush->flag & BRUSH_EDGE_TO_EDGE) {
			float halfway[2];
			float out[3];

			halfway[0] = (float)dx * 0.5f + cache->initial_mouse[0];
			halfway[1] = (float)dy * 0.5f + cache->initial_mouse[1];

			if (sculpt_stroke_get_location(C, out, halfway)) {
				copy_v3_v3(sd->anchored_location, out);
				copy_v2_v2(sd->anchored_initial_mouse, halfway);
				copy_v2_v2(cache->tex_mouse, halfway);
				copy_v3_v3(cache->true_location, sd->anchored_location);
				sd->anchored_size /= 2.0f;
				cache->pixel_radius  /= 2.0f;
				hit = 1;
			}
		}

		if (!hit)
			copy_v2_v2(sd->anchored_initial_mouse, cache->initial_mouse);

		cache->radius= paint_calc_object_space_radius(paint_stroke_view_context(stroke),
													  cache->true_location,
													  cache->pixel_radius);
		cache->radius_squared = cache->radius*cache->radius;

		copy_v3_v3(sd->anchored_location, cache->true_location);

		sd->draw_anchored = 1;
	}
	else if(brush->flag & BRUSH_RAKE) {
		const float u = 0.5f;
		const float v = 1 - u;
		const float r = 20;

		const float dx = cache->last_rake[0] - cache->mouse[0];
		const float dy = cache->last_rake[1] - cache->mouse[1];

		if (cache->first_time) {
			copy_v2_v2(cache->last_rake, cache->mouse);
		}
		else if (dx*dx + dy*dy >= r*r) {
			cache->special_rotation = atan2(dx, dy);

			cache->last_rake[0] = u*cache->last_rake[0] + v*cache->mouse[0];
			cache->last_rake[1] = u*cache->last_rake[1] + v*cache->mouse[1];
		}
	}

	sculpt_update_brush_delta(sd, ob, brush);

	if(brush->sculpt_tool == SCULPT_TOOL_ROTATE) {
		dx = cache->mouse[0] - cache->initial_mouse[0];
		dy = cache->mouse[1] - cache->initial_mouse[1];

		cache->vertex_rotation = -atan2f(dx, dy) * cache->bstrength;

		sd->draw_anchored = 1;
		copy_v2_v2(sd->anchored_initial_mouse, cache->initial_mouse);
		copy_v3_v3(sd->anchored_location, cache->true_location);
		sd->anchored_size = cache->pixel_radius;
	}

	sd->special_rotation = cache->special_rotation;
}

static void sculpt_stroke_modifiers_check(bContext *C, Object *ob)
{
	SculptSession *ss = ob->sculpt;

	if(ss->modifiers_active) {
		Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
		Brush *brush = paint_brush(&sd->paint);

		sculpt_update_mesh_elements(CTX_data_scene(C), sd, ob, brush->sculpt_tool == SCULPT_TOOL_SMOOTH);
	}
}

typedef struct {
	SculptSession *ss;
	float *ray_start, *ray_normal;
	int hit;
	float dist;
	int original;
} SculptRaycastData;

static void sculpt_raycast_cb(PBVHNode *node, void *data_v, float* tmin)
{
	if (BLI_pbvh_node_get_tmin(node) < *tmin) {
		SculptRaycastData *srd = data_v;
		float (*origco)[3]= NULL;

		if(srd->original && srd->ss->cache) {
			/* intersect with coordinates from before we started stroke */
			SculptUndoNode *unode= sculpt_undo_get_node(node);
			origco= (unode)? unode->co: NULL;
		}

		if (BLI_pbvh_node_raycast(srd->ss->pbvh, node, origco, srd->ray_start, srd->ray_normal, &srd->dist)) {
			srd->hit = 1;
			*tmin = srd->dist;
		}
	}
}

/* Do a raycast in the tree to find the 3d brush location
 * (This allows us to ignore the GL depth buffer)
 * Returns 0 if the ray doesn't hit the mesh, non-zero otherwise
 */
int sculpt_stroke_get_location(bContext *C, float out[3], float mouse[2])
{
	ViewContext vc;
	Object *ob;
	SculptSession *ss;
	StrokeCache *cache;
	float ray_start[3], ray_end[3], ray_normal[3], dist;
	float obimat[4][4];
	float mval[2];
	SculptRaycastData srd;

	view3d_set_viewcontext(C, &vc);
	
	ob = vc.obact;
	ss = ob->sculpt;
	cache = ss->cache;

	sculpt_stroke_modifiers_check(C, ob);

	mval[0] = mouse[0] - vc.ar->winrct.xmin;
	mval[1] = mouse[1] - vc.ar->winrct.ymin;

	ED_view3d_win_to_segment_clip(vc.ar, vc.v3d, mval, ray_start, ray_end);

	invert_m4_m4(obimat, ob->obmat);
	mul_m4_v3(obimat, ray_start);
	mul_m4_v3(obimat, ray_end);

	sub_v3_v3v3(ray_normal, ray_end, ray_start);
	dist= normalize_v3(ray_normal);

	srd.ss = vc.obact->sculpt;
	srd.ray_start = ray_start;
	srd.ray_normal = ray_normal;
	srd.dist = dist;
	srd.hit = 0;
	srd.original = (cache)? cache->original: 0;
	BLI_pbvh_raycast(ss->pbvh, sculpt_raycast_cb, &srd,
	                 ray_start, ray_normal, srd.original);
	
	copy_v3_v3(out, ray_normal);
	mul_v3_fl(out, srd.dist);
	add_v3_v3(out, ray_start);

	return srd.hit;
}

static void sculpt_brush_init_tex(const Scene *scene, Sculpt *sd, SculptSession *ss)
{
	Brush *brush = paint_brush(&sd->paint);
	MTex *mtex= &brush->mtex;

	/* init mtex nodes */
	if(mtex->tex && mtex->tex->nodetree)
		ntreeTexBeginExecTree(mtex->tex->nodetree, 1); /* has internal flag to detect it only does it once */

	/* TODO: Shouldn't really have to do this at the start of every
	 * stroke, but sculpt would need some sort of notification when
	 * changes are made to the texture. */
	sculpt_update_tex(scene, sd, ss);
}

static int sculpt_brush_stroke_init(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C);
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
	SculptSession *ss = CTX_data_active_object(C)->sculpt;
	Brush *brush = paint_brush(&sd->paint);
	int mode= RNA_enum_get(op->ptr, "mode");
	int is_smooth= 0;

	view3d_operator_needs_opengl(C);
	sculpt_brush_init_tex(scene, sd, ss);

	is_smooth|= mode == BRUSH_STROKE_SMOOTH;
	is_smooth|= brush->sculpt_tool == SCULPT_TOOL_SMOOTH;

	sculpt_update_mesh_elements(scene, sd, ob, is_smooth);

	return 1;
}

static void sculpt_restore_mesh(Sculpt *sd, SculptSession *ss)
{
	Brush *brush = paint_brush(&sd->paint);

	/* Restore the mesh before continuing with anchored stroke */
	if((brush->flag & BRUSH_ANCHORED) ||
	   (brush->sculpt_tool == SCULPT_TOOL_GRAB &&
		brush_use_size_pressure(ss->cache->vc->scene, brush)) ||
	   (brush->flag & BRUSH_RESTORE_MESH))
	{
		StrokeCache *cache = ss->cache;
		int i;

		PBVHNode **nodes;
		int n, totnode;

		BLI_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);

		#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
		for(n=0; n<totnode; n++) {
			SculptUndoNode *unode;
			
			unode= sculpt_undo_get_node(nodes[n]);
			if(unode) {
				PBVHVertexIter vd;

				BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
					copy_v3_v3(vd.co, unode->co[vd.i]);
					if(vd.no) copy_v3_v3_short(vd.no, unode->no[vd.i]);
					else normal_short_to_float_v3(vd.fno, unode->no[vd.i]);

					if(vd.mvert) vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
				}
				BLI_pbvh_vertex_iter_end;

				BLI_pbvh_node_mark_update(nodes[n]);
			}
		}

		if(ss->face_normals) {
			float *fn = ss->face_normals;
			for(i = 0; i < ss->totpoly; ++i, fn += 3)
				copy_v3_v3(fn, cache->face_norms[i]);
		}

		if(nodes)
			MEM_freeN(nodes);
	}
}

static void sculpt_flush_update(bContext *C)
{
	Object *ob = CTX_data_active_object(C);
	SculptSession *ss = ob->sculpt;
	ARegion *ar = CTX_wm_region(C);
	MultiresModifierData *mmd = ss->multires;

	if(mmd)
		multires_mark_as_modified(ob);
	if(ob->derivedFinal) /* VBO no longer valid */
		GPU_drawobject_free(ob->derivedFinal);

	if(ss->modifiers_active) {
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		ED_region_tag_redraw(ar);
	}
	else {
		rcti r;

		BLI_pbvh_update(ss->pbvh, PBVH_UpdateBB, NULL);
		if (sculpt_get_redraw_rect(ar, CTX_wm_region_view3d(C), ob, &r)) {
			if (ss->cache)
				ss->cache->previous_r= r;

			r.xmin += ar->winrct.xmin + 1;
			r.xmax += ar->winrct.xmin - 1;
			r.ymin += ar->winrct.ymin + 1;
			r.ymax += ar->winrct.ymin - 1;

			ss->partial_redraw = 1;
			ED_region_tag_redraw_partial(ar, &r);
		}
	}
}

/* Returns whether the mouse/stylus is over the mesh (1)
 * or over the background (0) */
static int over_mesh(bContext *C, struct wmOperator *UNUSED(op), float x, float y)
{
	float mouse[2], co[3];

	mouse[0] = x;
	mouse[1] = y;

	return sculpt_stroke_get_location(C, co, mouse);
}

static int sculpt_stroke_test_start(bContext *C, struct wmOperator *op,
					wmEvent *event)
{
	/* Don't start the stroke until mouse goes over the mesh.
	 * note: event will only be null when re-executing the saved stroke. */
	if(event==NULL || over_mesh(C, op, event->x, event->y)) {
		Object *ob = CTX_data_active_object(C);
		SculptSession *ss = ob->sculpt;
		Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

		ED_view3d_init_mats_rv3d(ob, CTX_wm_region_view3d(C));

		sculpt_update_cache_invariants(C, sd, ss, op, event);

		sculpt_undo_push_begin(sculpt_tool_name(sd));

#ifdef _OPENMP
		/* If using OpenMP then create a number of threads two times the
		 * number of processor cores.
		 * Justification: Empirically I've found that two threads per
		 * processor gives higher throughput. */
		if (sd->flags & SCULPT_USE_OPENMP) {
			int num_procs;

			num_procs = omp_get_num_procs();
			omp_set_num_threads(2*num_procs);
		}
#endif

		return 1;
	}
	else
		return 0;
}

static void sculpt_stroke_update_step(bContext *C, struct PaintStroke *stroke, PointerRNA *itemptr)
{
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
	Object *ob = CTX_data_active_object(C);
	SculptSession *ss = ob->sculpt;
	
	sculpt_stroke_modifiers_check(C, ob);
	sculpt_update_cache_variants(C, sd, ob, stroke, itemptr);
	sculpt_restore_mesh(sd, ss);
	do_symmetrical_brush_actions(sd, ob);

	/* Cleanup */
	sculpt_flush_update(C);
}

static void sculpt_brush_exit_tex(Sculpt *sd)
{
	Brush *brush= paint_brush(&sd->paint);
	MTex *mtex= &brush->mtex;

	if(mtex->tex && mtex->tex->nodetree)
		ntreeTexEndExecTree(mtex->tex->nodetree->execdata, 1);
}

static void sculpt_stroke_done(bContext *C, struct PaintStroke *UNUSED(stroke))
{
	Object *ob= CTX_data_active_object(C);
	SculptSession *ss = ob->sculpt;
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

	// reset values used to draw brush after completing the stroke
	sd->draw_anchored= 0;
	sd->draw_pressure= 0;
	sd->special_rotation= 0;

	/* Finished */
	if(ss->cache) {
		Brush *brush= paint_brush(&sd->paint);
		brush->flag &= ~BRUSH_INVERTED;

		sculpt_stroke_modifiers_check(C, ob);

		/* Alt-Smooth */
		if (ss->cache->alt_smooth) {
			Paint *p= &sd->paint;
			brush= (Brush *)find_id("BR", ss->cache->saved_active_brush_name);
			if(brush) {
				paint_brush_set(p, brush);
			}
		}

		sculpt_cache_free(ss->cache);
		ss->cache = NULL;

		sculpt_undo_push_end();

		BLI_pbvh_update(ss->pbvh, PBVH_UpdateOriginalBB, NULL);

		/* optimization: if there is locked key and active modifiers present in */
		/* the stack, keyblock is updating at each step. otherwise we could update */
		/* keyblock only when stroke is finished */
		if(ss->kb && !ss->modifiers_active) sculpt_update_keyblock(ob);

		ss->partial_redraw = 0;

		/* try to avoid calling this, only for e.g. linked duplicates now */
		if(((Mesh*)ob->data)->id.us > 1)
			DAG_id_tag_update(&ob->id, OB_RECALC_DATA);

		WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	}

	sculpt_brush_exit_tex(sd);
}

static int sculpt_brush_stroke_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	struct PaintStroke *stroke;
	int ignore_background_click;

	if(!sculpt_brush_stroke_init(C, op))
		return OPERATOR_CANCELLED;

	stroke = paint_stroke_new(C, sculpt_stroke_get_location,
	                          sculpt_stroke_test_start,
	                          sculpt_stroke_update_step,
	                          sculpt_stroke_done, event->type);

	op->customdata = stroke;

	/* For tablet rotation */
	ignore_background_click = RNA_boolean_get(op->ptr,
						  "ignore_background_click"); 

	if(ignore_background_click && !over_mesh(C, op, event->x, event->y)) {
		paint_stroke_free(stroke);
		return OPERATOR_PASS_THROUGH;
	}
	
	/* add modal handler */
	WM_event_add_modal_handler(C, op);

	op->type->modal(C, op, event);
	
	return OPERATOR_RUNNING_MODAL;
}

static int sculpt_brush_stroke_exec(bContext *C, wmOperator *op)
{
	if(!sculpt_brush_stroke_init(C, op))
		return OPERATOR_CANCELLED;

	op->customdata = paint_stroke_new(C, sculpt_stroke_get_location, sculpt_stroke_test_start,
					  sculpt_stroke_update_step, sculpt_stroke_done, 0);

	/* frees op->customdata */
	paint_stroke_exec(C, op);

	return OPERATOR_FINISHED;
}

static int sculpt_brush_stroke_cancel(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_active_object(C);
	SculptSession *ss = ob->sculpt;
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

	paint_stroke_cancel(C, op);

	if(ss->cache) {
		sculpt_cache_free(ss->cache);
		ss->cache = NULL;
	}

	sculpt_brush_exit_tex(sd);

	return OPERATOR_CANCELLED;
}

static void SCULPT_OT_brush_stroke(wmOperatorType *ot)
{
	static EnumPropertyItem stroke_mode_items[] = {
		{BRUSH_STROKE_NORMAL, "NORMAL", 0, "Normal", "Apply brush normally"},
		{BRUSH_STROKE_INVERT, "INVERT", 0, "Invert", "Invert action of brush for duration of stroke"},
		{BRUSH_STROKE_SMOOTH, "SMOOTH", 0, "Smooth", "Switch brush to smooth mode for duration of stroke"},
		{0}
	};

	/* identifiers */
	ot->name= "Sculpt Mode";
	ot->idname= "SCULPT_OT_brush_stroke";
	
	/* api callbacks */
	ot->invoke= sculpt_brush_stroke_invoke;
	ot->modal= paint_stroke_modal;
	ot->exec= sculpt_brush_stroke_exec;
	ot->poll= sculpt_poll;
	ot->cancel= sculpt_brush_stroke_cancel;

	/* flags (sculpt does own undo? (ton) */
	ot->flag= OPTYPE_BLOCKING;

	/* properties */

	RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement,
			"Stroke", "");

	RNA_def_enum(ot->srna, "mode", stroke_mode_items, BRUSH_STROKE_NORMAL, 
			"Sculpt Stroke Mode",
			"Action taken when a sculpt stroke is made");

	RNA_def_boolean(ot->srna, "ignore_background_click", 0,
			"Ignore Background Click",
			"Clicks on the background do not start the stroke");
}

/**** Reset the copy of the mesh that is being sculpted on (currently just for the layer brush) ****/

static int sculpt_set_persistent_base(bContext *C, wmOperator *UNUSED(op))
{
	SculptSession *ss = CTX_data_active_object(C)->sculpt;

	if(ss) {
		if(ss->layer_co)
			MEM_freeN(ss->layer_co);
		ss->layer_co = NULL;
	}

	return OPERATOR_FINISHED;
}

static void SCULPT_OT_set_persistent_base(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Persistent Base";
	ot->idname= "SCULPT_OT_set_persistent_base";
	
	/* api callbacks */
	ot->exec= sculpt_set_persistent_base;
	ot->poll= sculpt_mode_poll;
	
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/**** Toggle operator for turning sculpt mode on or off ****/

static void sculpt_init_session(Scene *scene, Object *ob)
{
	ob->sculpt = MEM_callocN(sizeof(SculptSession), "sculpt session");

	sculpt_update_mesh_elements(scene, scene->toolsettings->sculpt, ob, 0);
}

static int sculpt_toggle_mode(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = CTX_data_tool_settings(C);
	Object *ob = CTX_data_active_object(C);
	MultiresModifierData *mmd= sculpt_multires_active(scene, ob);
	int flush_recalc= 0;

	/* multires in sculpt mode could have different from object mode subdivision level */
	flush_recalc |= mmd && mmd->sculptlvl != mmd->lvl;
	/* if object has got active modifiers, it's dm could be different in sculpt mode  */
	flush_recalc |= sculpt_has_active_modifiers(scene, ob);

	if(ob->mode & OB_MODE_SCULPT) {
		if(mmd)
			multires_force_update(ob);

		if(flush_recalc)
			DAG_id_tag_update(&ob->id, OB_RECALC_DATA);

		/* Leave sculptmode */
		ob->mode &= ~OB_MODE_SCULPT;

		free_sculptsession(ob);
	}
	else {
		/* Enter sculptmode */
		ob->mode |= OB_MODE_SCULPT;

		if(flush_recalc)
			DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		
		/* Create persistent sculpt mode data */
		if(!ts->sculpt) {
			ts->sculpt = MEM_callocN(sizeof(Sculpt), "sculpt mode data");

			/* Turn on X plane mirror symmetry by default */
			ts->sculpt->flags |= SCULPT_SYMM_X;
		}

		/* Create sculpt mode session data */
		if(ob->sculpt)
			free_sculptsession(ob);

		sculpt_init_session(scene, ob);

		paint_init(&ts->sculpt->paint, PAINT_CURSOR_SCULPT);
		
		paint_cursor_start(C, sculpt_poll);
	}

	WM_event_add_notifier(C, NC_SCENE|ND_MODE, CTX_data_scene(C));

	return OPERATOR_FINISHED;
}

static void SCULPT_OT_sculptmode_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Sculpt Mode";
	ot->idname= "SCULPT_OT_sculptmode_toggle";
	
	/* api callbacks */
	ot->exec= sculpt_toggle_mode;
	ot->poll= ED_operator_object_active_editable_mesh;
	
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

void ED_operatortypes_sculpt(void)
{
	WM_operatortype_append(SCULPT_OT_brush_stroke);
	WM_operatortype_append(SCULPT_OT_sculptmode_toggle);
	WM_operatortype_append(SCULPT_OT_set_persistent_base);
}
