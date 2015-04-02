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
			case SPACE_TIME:   /* Timeline - XXX: this is a hack to get it to show GP keyframes for 3D view */
			case SPACE_ACTION: /* DepeSheet - XXX: this is a hack to get the keyframe jump operator to take GP Keyframes into account */
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

/* ******************************************************** */
