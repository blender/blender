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
 * The Original Code is Copyright (C) 2014, Blender Foundation
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/gpencil/gpencil_utils.c
 *  \ingroup edgpencil
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_gpencil_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_context.h"
#include "BKE_gpencil.h"
#include "BKE_tracking.h"

#include "WM_api.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "ED_gpencil.h"
#include "ED_clip.h"
#include "ED_view3d.h"

#include "gpencil_intern.h"

/* ******************************************************** */
/* Context Wrangling... */

/* Get pointer to active Grease Pencil datablock, and an RNA-pointer to trace back to whatever owns it,
 * when context info is not available.
 */
bGPdata **ED_gpencil_data_get_pointers_direct(ID *screen_id, Scene *scene, ScrArea *sa, Object *ob, PointerRNA *ptr)
{
	/* if there's an active area, check if the particular editor may
	 * have defined any special Grease Pencil context for editing...
	 */
	if (sa) {
		SpaceLink *sl = sa->spacedata.first;
		
		switch (sa->spacetype) {
			case SPACE_VIEW3D: /* 3D-View */
			{
				BLI_assert(scene && ELEM(scene->toolsettings->gpencil_src,
				                         GP_TOOL_SOURCE_SCENE, GP_TOOL_SOURCE_OBJECT));
				
				if (scene->toolsettings->gpencil_src == GP_TOOL_SOURCE_OBJECT) {
					/* legacy behaviour for usage with old addons requiring object-linked to objects */
					
					/* just in case no active/selected object... */
					if (ob && (ob->flag & SELECT)) {
						/* for now, as long as there's an object, default to using that in 3D-View */
						if (ptr) RNA_id_pointer_create(&ob->id, ptr);
						return &ob->gpd;
					}
					/* else: defaults to scene... */
				}
				else {
					if (ptr) RNA_id_pointer_create(&scene->id, ptr);
					return &scene->gpd;
				}
				break;
			}
			case SPACE_NODE: /* Nodes Editor */
			{
				SpaceNode *snode = (SpaceNode *)sl;
				
				/* return the GP data for the active node block/node */
				if (snode && snode->nodetree) {
					/* for now, as long as there's an active node tree, default to using that in the Nodes Editor */
					if (ptr) RNA_id_pointer_create(&snode->nodetree->id, ptr);
					return &snode->nodetree->gpd;
				}
				
				/* even when there is no node-tree, don't allow this to flow to scene */
				return NULL;
			}
			case SPACE_SEQ: /* Sequencer */
			{
				SpaceSeq *sseq = (SpaceSeq *)sl;
			
				/* for now, Grease Pencil data is associated with the space (actually preview region only) */
				/* XXX our convention for everything else is to link to data though... */
				if (ptr) RNA_pointer_create(screen_id, &RNA_SpaceSequenceEditor, sseq, ptr);
				return &sseq->gpd;
			}
			case SPACE_IMAGE: /* Image/UV Editor */
			{
				SpaceImage *sima = (SpaceImage *)sl;
				
				/* for now, Grease Pencil data is associated with the space... */
				/* XXX our convention for everything else is to link to data though... */
				if (ptr) RNA_pointer_create(screen_id, &RNA_SpaceImageEditor, sima, ptr);
				return &sima->gpd;
			}
			case SPACE_CLIP: /* Nodes Editor */
			{
				SpaceClip *sc = (SpaceClip *)sl;
				MovieClip *clip = ED_space_clip_get_clip(sc);
				
				if (clip) {
					if (sc->gpencil_src == SC_GPENCIL_SRC_TRACK) {
						MovieTrackingTrack *track = BKE_tracking_track_get_active(&clip->tracking);
						
						if (!track)
							return NULL;
						
						if (ptr)
							RNA_pointer_create(&clip->id, &RNA_MovieTrackingTrack, track, ptr);
						
						return &track->gpd;
					}
					else {
						if (ptr)
							RNA_id_pointer_create(&clip->id, ptr);
						
						return &clip->gpd;
					}
				}
				break;
			}
			default: /* unsupported space */
				return NULL;
		}
	}
	
	/* just fall back on the scene's GP data */
	if (ptr) RNA_id_pointer_create((ID *)scene, ptr);
	return (scene) ? &scene->gpd : NULL;
}

/* Get pointer to active Grease Pencil datablock, and an RNA-pointer to trace back to whatever owns it */
bGPdata **ED_gpencil_data_get_pointers(const bContext *C, PointerRNA *ptr)
{
	ID *screen_id = (ID *)CTX_wm_screen(C);
	Scene *scene = CTX_data_scene(C);
	ScrArea *sa = CTX_wm_area(C);
	Object *ob = CTX_data_active_object(C);
	
	return ED_gpencil_data_get_pointers_direct(screen_id, scene, sa, ob, ptr);
}

/* -------------------------------------------------------- */

/* Get the active Grease Pencil datablock, when context is not available */
bGPdata *ED_gpencil_data_get_active_direct(ID *screen_id, Scene *scene, ScrArea *sa, Object *ob)
{
	bGPdata **gpd_ptr = ED_gpencil_data_get_pointers_direct(screen_id, scene, sa, ob, NULL);
	return (gpd_ptr) ? *(gpd_ptr) : NULL;
}

/* Get the active Grease Pencil datablock */
bGPdata *ED_gpencil_data_get_active(const bContext *C)
{
	bGPdata **gpd_ptr = ED_gpencil_data_get_pointers(C, NULL);
	return (gpd_ptr) ? *(gpd_ptr) : NULL;
}

/* -------------------------------------------------------- */

// XXX: this should be removed... We really shouldn't duplicate logic like this!
bGPdata *ED_gpencil_data_get_active_v3d(Scene *scene, View3D *v3d)
{
	Base *base = scene->basact;
	bGPdata *gpd = NULL;
	/* We have to make sure active object is actually visible and selected, else we must use default scene gpd,
	 * to be consistent with ED_gpencil_data_get_active's behavior.
	 */
	
	if (base && TESTBASE(v3d, base)) {
		gpd = base->object->gpd;
	}
	return gpd ? gpd : scene->gpd;
}

/* ******************************************************** */
/* Keyframe Indicator Checks */

/* Check whether there's an active GP keyframe on the current frame */
bool ED_gpencil_has_keyframe_v3d(Scene *scene, Object *ob, int cfra)
{
	/* just check both for now... */
	// XXX: this could get confusing (e.g. if only on the object, but other places don't show this)
	if (scene->gpd) {
		bGPDlayer *gpl = gpencil_layer_getactive(scene->gpd);
		if (gpl) {
			if (gpl->actframe) {
				// XXX: assumes that frame has been fetched already
				return (gpl->actframe->framenum == cfra);
			}
			else {
				/* XXX: disabled as could be too much of a penalty */
				/* return BKE_gpencil_layer_find_frame(gpl, cfra); */
			}
		}
	}
	
	if (ob && ob->gpd) {
		bGPDlayer *gpl = gpencil_layer_getactive(ob->gpd);
		if (gpl) {
			if (gpl->actframe) {
				// XXX: assumes that frame has been fetched already
				return (gpl->actframe->framenum == cfra);
			}
			else {
				/* XXX: disabled as could be too much of a penalty */
				/* return BKE_gpencil_layer_find_frame(gpl, cfra); */
			}
		}
	}
	
	return false;
}

/* ******************************************************** */
/* Poll Callbacks */

/* poll callback for adding data/layers - special */
int gp_add_poll(bContext *C)
{
	/* the base line we have is that we have somewhere to add Grease Pencil data */
	return ED_gpencil_data_get_pointers(C, NULL) != NULL;
}

/* poll callback for checking if there is an active layer */
int gp_active_layer_poll(bContext *C)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *gpl = gpencil_layer_getactive(gpd);
	
	return (gpl != NULL);
}

/* ******************************************************** */
/* Dynamic Enums of GP Layers */
/* NOTE: These include an option to create a new layer and use that... */

/* Just existing layers */
EnumPropertyItem *ED_gpencil_layers_enum_itemf(bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop), bool *r_free)
{
	bGPdata *gpd = CTX_data_gpencil_data(C);
	bGPDlayer *gpl;
	EnumPropertyItem *item = NULL, item_tmp = {0};
	int totitem = 0;
	int i = 0;
	
	if (ELEM(NULL, C, gpd)) {
		return DummyRNA_DEFAULT_items;
	}
	
	/* Existing layers */
	for (gpl = gpd->layers.first; gpl; gpl = gpl->next, i++) {
		item_tmp.identifier = gpl->info;
		item_tmp.name = gpl->info;
		item_tmp.value = i;
		
		if (gpl->flag & GP_LAYER_ACTIVE)
			item_tmp.icon = ICON_GREASEPENCIL;
		else 
			item_tmp.icon = ICON_NONE;
		
		RNA_enum_item_add(&item, &totitem, &item_tmp);
	}
	
	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

/* Existing + Option to add/use new layer */
EnumPropertyItem *ED_gpencil_layers_with_new_enum_itemf(bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop), bool *r_free)
{
	bGPdata *gpd = CTX_data_gpencil_data(C);
	bGPDlayer *gpl;
	EnumPropertyItem *item = NULL, item_tmp = {0};
	int totitem = 0;
	int i = 0;
	
	if (ELEM(NULL, C, gpd)) {
		return DummyRNA_DEFAULT_items;
	}
	
	/* Create new layer */
	/* TODO: have some way of specifying that we don't want this? */
	{
		/* active Keying Set */
		item_tmp.identifier = "__CREATE__";
		item_tmp.name = "New Layer";
		item_tmp.value = -1;
		item_tmp.icon = ICON_ZOOMIN;
		RNA_enum_item_add(&item, &totitem, &item_tmp);
		
		/* separator */
		RNA_enum_item_add_separator(&item, &totitem);
	}
	
	/* Existing layers */
	for (gpl = gpd->layers.first, i = 0; gpl; gpl = gpl->next, i++) {
		item_tmp.identifier = gpl->info;
		item_tmp.name = gpl->info;
		item_tmp.value = i;
		
		if (gpl->flag & GP_LAYER_ACTIVE)
			item_tmp.icon = ICON_GREASEPENCIL;
		else 
			item_tmp.icon = ICON_NONE;
		
		RNA_enum_item_add(&item, &totitem, &item_tmp);
	}
	
	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}



/* ******************************************************** */
/* Brush Tool Core */

/* Check if part of stroke occurs within last segment drawn by eraser */
bool gp_stroke_inside_circle(const int mval[2], const int UNUSED(mvalo[2]),
                             int rad, int x0, int y0, int x1, int y1)
{
	/* simple within-radius check for now */
	const float mval_fl[2]     = {mval[0], mval[1]};
	const float screen_co_a[2] = {x0, y0};
	const float screen_co_b[2] = {x1, y1};
	
	if (edge_inside_circle(mval_fl, rad, screen_co_a, screen_co_b)) {
		return true;
	}
	
	/* not inside */
	return false;
}

/* ******************************************************** */
/* Stroke Validity Testing */

/* Check whether given stroke can be edited given the supplied context */
// XXX: do we need additional flags for screenspace vs dataspace?
bool ED_gpencil_stroke_can_use_direct(const ScrArea *sa, const bGPDstroke *gps)
{
	/* sanity check */
	if (ELEM(NULL, sa, gps))
		return false;

	/* filter stroke types by flags + spacetype */
	if (gps->flag & GP_STROKE_3DSPACE) {
		/* 3D strokes - only in 3D view */
		return (sa->spacetype == SPACE_VIEW3D);
	}
	else if (gps->flag & GP_STROKE_2DIMAGE) {
		/* Special "image" strokes - only in Image Editor */
		return (sa->spacetype == SPACE_IMAGE);
	}
	else if (gps->flag & GP_STROKE_2DSPACE) {
		/* 2D strokes (dataspace) - for any 2D view (i.e. everything other than 3D view) */
		return (sa->spacetype != SPACE_VIEW3D);
	}
	else {
		/* view aligned - anything goes */
		return true;
	}
}

/* Check whether given stroke can be edited in the current context */
bool ED_gpencil_stroke_can_use(const bContext *C, const bGPDstroke *gps)
{
	ScrArea *sa = CTX_wm_area(C);
	return ED_gpencil_stroke_can_use_direct(sa, gps);
}

/* ******************************************************** */
/* Space Conversion */

/* Init handling for space-conversion function (from passed-in parameters) */
void gp_point_conversion_init(bContext *C, GP_SpaceConversion *r_gsc)
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	
	/* zero out the storage (just in case) */
	memset(r_gsc, 0, sizeof(GP_SpaceConversion));
	unit_m4(r_gsc->mat);
	
	/* store settings */
	r_gsc->sa = sa;
	r_gsc->ar = ar;
	r_gsc->v2d = &ar->v2d;
	
	/* init region-specific stuff */
	if (sa->spacetype == SPACE_VIEW3D) {
		wmWindow *win = CTX_wm_window(C);
		Scene *scene = CTX_data_scene(C);
		View3D *v3d = (View3D *)CTX_wm_space_data(C);
		RegionView3D *rv3d = ar->regiondata;
		
		/* init 3d depth buffers */
		view3d_operator_needs_opengl(C);
		
		view3d_region_operator_needs_opengl(win, ar);
		ED_view3d_autodist_init(scene, ar, v3d, 0);
		
		/* for camera view set the subrect */
		if (rv3d->persp == RV3D_CAMOB) {
			ED_view3d_calc_camera_border(scene, ar, v3d, rv3d, &r_gsc->subrect_data, true); /* no shift */
			r_gsc->subrect = &r_gsc->subrect_data;
		}
	}
}


/* Convert Grease Pencil points to screen-space values
 * WARNING: This assumes that the caller has already checked whether the stroke in question can be drawn
 */
void gp_point_to_xy(GP_SpaceConversion *gsc, bGPDstroke *gps, bGPDspoint *pt,
                    int *r_x, int *r_y)
{
	ARegion *ar = gsc->ar;
	View2D *v2d = gsc->v2d;
	rctf *subrect = gsc->subrect;
	int xyval[2];
	
	/* sanity checks */
	BLI_assert(!(gps->flag & GP_STROKE_3DSPACE) || (gsc->sa->spacetype == SPACE_VIEW3D));
	BLI_assert(!(gps->flag & GP_STROKE_2DSPACE) || (gsc->sa->spacetype != SPACE_VIEW3D));
	
	
	if (gps->flag & GP_STROKE_3DSPACE) {
		if (ED_view3d_project_int_global(ar, &pt->x, xyval, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) {
			*r_x = xyval[0];
			*r_y = xyval[1];
		}
		else {
			*r_x = V2D_IS_CLIPPED;
			*r_y = V2D_IS_CLIPPED;
		}
	}
	else if (gps->flag & GP_STROKE_2DSPACE) {
		float vec[3] = {pt->x, pt->y, 0.0f};
		mul_m4_v3(gsc->mat, vec);
		UI_view2d_view_to_region_clip(v2d, vec[0], vec[1], r_x, r_y);
	}
	else {
		if (subrect == NULL) {
			/* normal 3D view (or view space) */
			*r_x = (int)(pt->x / 100 * ar->winx);
			*r_y = (int)(pt->y / 100 * ar->winy);
		}
		else {
			/* camera view, use subrect */
			*r_x = (int)((pt->x / 100) * BLI_rctf_size_x(subrect)) + subrect->xmin;
			*r_y = (int)((pt->y / 100) * BLI_rctf_size_y(subrect)) + subrect->ymin;
		}
	}
}

/**
 * Project screenspace coordinates to 3D-space
 *
 * \note We include this as a utility function, since the standard method
 * involves quite a few steps, which are invariably always the same
 * for all GPencil operations. So, it's nicer to just centralize these.
 *
 * \warning Assumes that it is getting called in a 3D view only.
 */
bool gp_point_xy_to_3d(GP_SpaceConversion *gsc, Scene *scene, const float screen_co[2], float r_out[3])
{
	View3D *v3d = gsc->sa->spacedata.first;
	RegionView3D *rv3d = gsc->ar->regiondata;
	float *rvec = ED_view3d_cursor3d_get(scene, v3d);
	float ref[3] = {rvec[0], rvec[1], rvec[2]};
	float zfac = ED_view3d_calc_zfac(rv3d, rvec, NULL);
	
	float mval_f[2], mval_prj[2];
	float dvec[3];
	
	copy_v2_v2(mval_f, screen_co);
	
	if (ED_view3d_project_float_global(gsc->ar, ref, mval_prj, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) {
		sub_v2_v2v2(mval_f, mval_prj, mval_f);
		ED_view3d_win_to_delta(gsc->ar, mval_f, dvec, zfac);
		sub_v3_v3v3(r_out, rvec, dvec);
		
		return true;
	}
	else {
		zero_v3(r_out);
		
		return false;
	}
}

/**
 * Apply smooth to stroke point 
 * \param gps              Stroke to smooth
 * \param i                Point index
 * \param inf              Amount of smoothing to apply
 * \param affect_pressure  Apply smoothing to pressure values too?
 */
bool gp_smooth_stroke(bGPDstroke *gps, int i, float inf, bool affect_pressure)
{
	bGPDspoint *pt = &gps->points[i];
	float pressure = 0.0f;
	float sco[3] = {0.0f};
	
	/* Do nothing if not enough points to smooth out */
	if (gps->totpoints <= 2) {
		return false;
	}
	
	/* Only affect endpoints by a fraction of the normal strength,
	 * to prevent the stroke from shrinking too much
	 */
	if ((i == 0) || (i == gps->totpoints - 1)) {
		inf *= 0.1f;
	}
	
	/* Compute smoothed coordinate by taking the ones nearby */
	/* XXX: This is potentially slow, and suffers from accumulation error as earlier points are handled before later ones */
	{
		// XXX: this is hardcoded to look at 2 points on either side of the current one (i.e. 5 items total)
		const int   steps = 2;
		const float average_fac = 1.0f / (float)(steps * 2 + 1);
		int step;
		
		/* add the point itself */
		madd_v3_v3fl(sco, &pt->x, average_fac);
		
		if (affect_pressure) {
			pressure += pt->pressure * average_fac;
		}
		
		/* n-steps before/after current point */
		// XXX: review how the endpoints are treated by this algorithm
		// XXX: falloff measures should also introduce some weighting variations, so that further-out points get less weight
		for (step = 1; step <= steps; step++) {
			bGPDspoint *pt1, *pt2;
			int before = i - step;
			int after = i + step;
			
			CLAMP_MIN(before, 0);
			CLAMP_MAX(after, gps->totpoints - 1);
			
			pt1 = &gps->points[before];
			pt2 = &gps->points[after];
			
			/* add both these points to the average-sum (s += p[i]/n) */
			madd_v3_v3fl(sco, &pt1->x, average_fac);
			madd_v3_v3fl(sco, &pt2->x, average_fac);
			
			/* do pressure too? */
			if (affect_pressure) {
				pressure += pt1->pressure * average_fac;
				pressure += pt2->pressure * average_fac;
			}
		}
	}
	
	/* Based on influence factor, blend between original and optimal smoothed coordinate */
	interp_v3_v3v3(&pt->x, &pt->x, sco, inf);
	
	if (affect_pressure) {
		pt->pressure = pressure;
	}
	
	return true;
}

/**
 * Subdivide a stroke once, by adding a point half way between each pair of existing points
 * \param gps           Stroke data
 * \param new_totpoints Total number of points (after subdividing)
 */
void gp_subdivide_stroke(bGPDstroke *gps, const int new_totpoints)
{
	/* Move points towards end of enlarged points array to leave space for new points */
	int y = 1;
	for (int i = gps->totpoints - 1; i > 0; i--) {
		gps->points[new_totpoints - y] = gps->points[i];
		y += 2;
	}
	
	/* Create interpolated points */
	for (int i = 0; i < new_totpoints - 1; i += 2) {
		bGPDspoint *prev  = &gps->points[i];
		bGPDspoint *pt    = &gps->points[i + 1];
		bGPDspoint *next  = &gps->points[i + 2];
		
		/* Interpolate all values */
		interp_v3_v3v3(&pt->x, &prev->x, &next->x, 0.5f);
		
		pt->pressure = interpf(prev->pressure, next->pressure, 0.5f);
		pt->time     = interpf(prev->time, next->time, 0.5f);
	}
	
	/* Update to new total number of points */
	gps->totpoints = new_totpoints;
}

/* ******************************************************** */


bool ED_gpencil_stroke_minmax(
        const bGPDstroke *gps, const bool use_select,
        float r_min[3], float r_max[3])
{
	const bGPDspoint *pt;
	int i;
	bool changed = false;

	for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
		if ((use_select == false) || (pt->flag & GP_SPOINT_SELECT)) {;
			minmax_v3v3_v3(r_min, r_max, &pt->x);
			changed = true;
		}
	}
	return changed;
}
