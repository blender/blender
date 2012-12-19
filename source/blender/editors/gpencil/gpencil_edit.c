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
 * The Original Code is Copyright (C) 2008, Blender Foundation, Joshua Leung
 * This is a new part of Blender
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/gpencil/gpencil_edit.c
 *  \ingroup edgpencil
 */

 

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#include "MEM_guardedalloc.h"


#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_gpencil_types.h"

#include "BKE_animsys.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_library.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_tracking.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_view2d.h"

#include "ED_gpencil.h"
#include "ED_view3d.h"
#include "ED_clip.h"
#include "ED_keyframing.h"

#include "gpencil_intern.h"

/* ************************************************ */
/* Context Wrangling... */

/* Get pointer to active Grease Pencil datablock, and an RNA-pointer to trace back to whatever owns it */
bGPdata **gpencil_data_get_pointers(const bContext *C, PointerRNA *ptr)
{
	ID *screen_id = (ID *)CTX_wm_screen(C);
	Scene *scene = CTX_data_scene(C);
	ScrArea *sa = CTX_wm_area(C);
	
	/* if there's an active area, check if the particular editor may
	 * have defined any special Grease Pencil context for editing...
	 */
	if (sa) {
		switch (sa->spacetype) {
			case SPACE_VIEW3D: /* 3D-View */
			{
				Object *ob = CTX_data_active_object(C);
				
				/* TODO: we can include other data-types such as bones later if need be... */

				/* just in case no active object */
				if (ob) {
					/* for now, as long as there's an object, default to using that in 3D-View */
					if (ptr) RNA_id_pointer_create(&ob->id, ptr);
					return &ob->gpd;
				}
			}
			break;
			
			case SPACE_NODE: /* Nodes Editor */
			{
				SpaceNode *snode = (SpaceNode *)CTX_wm_space_data(C);
				
				/* return the GP data for the active node block/node */
				if (snode && snode->nodetree) {
					/* for now, as long as there's an active node tree, default to using that in the Nodes Editor */
					if (ptr) RNA_id_pointer_create(&snode->nodetree->id, ptr);
					return &snode->nodetree->gpd;
				}
				else {
					/* even when there is no node-tree, don't allow this to flow to scene */
					return NULL;
				}
			}
			break;
				
			case SPACE_SEQ: /* Sequencer */
			{
				SpaceSeq *sseq = (SpaceSeq *)CTX_wm_space_data(C);
				
				/* for now, Grease Pencil data is associated with the space (actually preview region only) */
				/* XXX our convention for everything else is to link to data though... */
				if (ptr) RNA_pointer_create(screen_id, &RNA_SpaceSequenceEditor, sseq, ptr);
				return &sseq->gpd;
			}
			break;
			
			case SPACE_IMAGE: /* Image/UV Editor */
			{
				SpaceImage *sima = (SpaceImage *)CTX_wm_space_data(C);
				
				/* for now, Grease Pencil data is associated with the space... */
				/* XXX our convention for everything else is to link to data though... */
				if (ptr) RNA_pointer_create(screen_id, &RNA_SpaceImageEditor, sima, ptr);
				return &sima->gpd;
			}
			break;
				
			case SPACE_CLIP: /* Nodes Editor */
			{
				SpaceClip *sc = (SpaceClip *)CTX_wm_space_data(C);
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
			}
			break;
				
			default: /* unsupported space */
				return NULL;
		}
	}
	
	/* just fall back on the scene's GP data */
	if (ptr) RNA_id_pointer_create((ID *)scene, ptr);
	return (scene) ? &scene->gpd : NULL;
}

/* Get the active Grease Pencil datablock */
bGPdata *gpencil_data_get_active(const bContext *C)
{
	bGPdata **gpd_ptr = gpencil_data_get_pointers(C, NULL);
	return (gpd_ptr) ? *(gpd_ptr) : NULL;
}

/* needed for offscreen rendering */
bGPdata *gpencil_data_get_active_v3d(Scene *scene)
{
	bGPdata *gpd = scene->basact ? scene->basact->object->gpd : NULL;
	return gpd ? gpd : scene->gpd;
}

/* ************************************************ */
/* Panel Operators */

/* poll callback for adding data/layers - special */
static int gp_add_poll(bContext *C)
{
	/* the base line we have is that we have somewhere to add Grease Pencil data */
	return gpencil_data_get_pointers(C, NULL) != NULL;
}

/* ******************* Add New Data ************************ */

/* add new datablock - wrapper around API */
static int gp_data_add_exec(bContext *C, wmOperator *op)
{
	bGPdata **gpd_ptr = gpencil_data_get_pointers(C, NULL);
	
	if (gpd_ptr == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Nowhere for grease pencil data to go");
		return OPERATOR_CANCELLED;
	}
	else {
		/* decrement user count and add new datablock */
		bGPdata *gpd = (*gpd_ptr);
		
		id_us_min(&gpd->id);
		*gpd_ptr = gpencil_data_addnew("GPencil");
	}
	
	/* notifiers */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_data_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Grease Pencil Add New";
	ot->idname = "GPENCIL_OT_data_add";
	ot->description = "Add new Grease Pencil datablock";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* callbacks */
	ot->exec = gp_data_add_exec;
	ot->poll = gp_add_poll;
}

/* ******************* Unlink Data ************************ */

/* poll callback for adding data/layers - special */
static int gp_data_unlink_poll(bContext *C)
{
	bGPdata **gpd_ptr = gpencil_data_get_pointers(C, NULL);
	
	/* if we have access to some active data, make sure there's a datablock before enabling this */
	return (gpd_ptr && *gpd_ptr);
}


/* unlink datablock - wrapper around API */
static int gp_data_unlink_exec(bContext *C, wmOperator *op)
{
	bGPdata **gpd_ptr = gpencil_data_get_pointers(C, NULL);
	
	if (gpd_ptr == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Nowhere for grease pencil data to go");
		return OPERATOR_CANCELLED;
	}
	else {
		/* just unlink datablock now, decreasing its user count */
		bGPdata *gpd = (*gpd_ptr);
		
		id_us_min(&gpd->id);
		*gpd_ptr = NULL;
	}
	
	/* notifiers */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL); 
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_data_unlink(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Grease Pencil Unlink";
	ot->idname = "GPENCIL_OT_data_unlink";
	ot->description = "Unlink active Grease Pencil datablock";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* callbacks */
	ot->exec = gp_data_unlink_exec;
	ot->poll = gp_data_unlink_poll;
}

/* ******************* Add New Layer ************************ */

/* add new layer - wrapper around API */
static int gp_layer_add_exec(bContext *C, wmOperator *op)
{
	bGPdata **gpd_ptr = gpencil_data_get_pointers(C, NULL);
	
	/* if there's no existing Grease-Pencil data there, add some */
	if (gpd_ptr == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Nowhere for grease pencil data to go");
		return OPERATOR_CANCELLED;
	}
	if (*gpd_ptr == NULL)
		*gpd_ptr = gpencil_data_addnew("GPencil");
		
	/* add new layer now */
	gpencil_layer_addnew(*gpd_ptr, "GP_Layer", 1);
	
	/* notifiers */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_layer_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add New Layer";
	ot->idname = "GPENCIL_OT_layer_add";
	ot->description = "Add new Grease Pencil layer for the active Grease Pencil datablock";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* callbacks */
	ot->exec = gp_layer_add_exec;
	ot->poll = gp_add_poll;
}

/* ******************* Delete Active Frame ************************ */

static int gp_actframe_delete_poll(bContext *C)
{
	bGPdata *gpd = gpencil_data_get_active(C);
	bGPDlayer *gpl = gpencil_layer_getactive(gpd);
	
	/* only if there's an active layer with an active frame */
	return (gpl && gpl->actframe);
}

/* delete active frame - wrapper around API calls */
static int gp_actframe_delete_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	bGPdata *gpd = gpencil_data_get_active(C);
	bGPDlayer *gpl = gpencil_layer_getactive(gpd);
	bGPDframe *gpf = gpencil_layer_getframe(gpl, CFRA, 0);
	
	/* if there's no existing Grease-Pencil data there, add some */
	if (gpd == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No grease pencil data");
		return OPERATOR_CANCELLED;
	}
	if (ELEM(NULL, gpl, gpf)) {
		BKE_report(op->reports, RPT_ERROR, "No active frame to delete");
		return OPERATOR_CANCELLED;
	}
	
	/* delete it... */
	gpencil_layer_delframe(gpl, gpf);
	
	/* notifiers */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_active_frame_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete Active Frame";
	ot->idname = "GPENCIL_OT_active_frame_delete";
	ot->description = "Delete the active frame for the active Grease Pencil datablock";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* callbacks */
	ot->exec = gp_actframe_delete_exec;
	ot->poll = gp_actframe_delete_poll;
}

/* ************************************************ */
/* Grease Pencil to Data Operator */

/* defines for possible modes */
enum {
	GP_STROKECONVERT_PATH = 1,
	GP_STROKECONVERT_CURVE,
};

/* Defines for possible timing modes */
enum {
	GP_STROKECONVERT_TIMING_NONE = 1,
	GP_STROKECONVERT_TIMING_LINEAR = 2,
	GP_STROKECONVERT_TIMING_FULL = 3,
	GP_STROKECONVERT_TIMING_CUSTOMGAP = 4,
};

/* RNA enum define */
static EnumPropertyItem prop_gpencil_convertmodes[] = {
	{GP_STROKECONVERT_PATH, "PATH", 0, "Path", ""},
	{GP_STROKECONVERT_CURVE, "CURVE", 0, "Bezier Curve", ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem prop_gpencil_convert_timingmodes_restricted[] = {
	{GP_STROKECONVERT_TIMING_NONE, "NONE", 0, "No Timing", "Ignore timing"},
	{GP_STROKECONVERT_TIMING_LINEAR, "LINEAR", 0, "Linear", "Simple linear timing"},
	{0, NULL, 0, NULL, NULL},
};

static EnumPropertyItem prop_gpencil_convert_timingmodes[] = {
	{GP_STROKECONVERT_TIMING_NONE, "NONE", 0, "No Timing", "Ignore timing"},
	{GP_STROKECONVERT_TIMING_LINEAR, "LINEAR", 0, "Linear", "Simple linear timing"},
	{GP_STROKECONVERT_TIMING_FULL, "FULL", 0, "Original", "Use the original timing, gaps included"},
	{GP_STROKECONVERT_TIMING_CUSTOMGAP, "CUSTOMGAP", 0, "Custom Gaps",
	                                    "Use the original timing, but with custom gap lengths (in frames)"},
	{0, NULL, 0, NULL, NULL},
};

static EnumPropertyItem *rna_GPConvert_mode_items(bContext *UNUSED(C), PointerRNA *ptr, PropertyRNA *UNUSED(prop),
                                                  int *free)
{
	*free = FALSE;
	if (RNA_boolean_get(ptr, "use_timing_data")) {
		return prop_gpencil_convert_timingmodes;
	}
	return prop_gpencil_convert_timingmodes_restricted;
}

/* --- */

/* convert the coordinates from the given stroke point into 3d-coordinates 
 *	- assumes that the active space is the 3D-View
 */
static void gp_strokepoint_convertcoords(bContext *C, bGPDstroke *gps, bGPDspoint *pt, float p3d[3], rctf *subrect)
{
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	ARegion *ar = CTX_wm_region(C);
	
	if (gps->flag & GP_STROKE_3DSPACE) {
		/* directly use 3d-coordinates */
		copy_v3_v3(p3d, &pt->x);
	}
	else {
		const float *fp = give_cursor(scene, v3d);
		float mvalf[2];
		
		/* get screen coordinate */
		if (gps->flag & GP_STROKE_2DSPACE) {
			int mvali[2];
			View2D *v2d = &ar->v2d;
			UI_view2d_view_to_region(v2d, pt->x, pt->y, mvali, mvali + 1);
			VECCOPY2D(mvalf, mvali);
		}
		else {
			if (subrect) {
				mvalf[0] = (((float)pt->x / 100.0f) * BLI_rctf_size_x(subrect)) + subrect->xmin;
				mvalf[1] = (((float)pt->y / 100.0f) * BLI_rctf_size_y(subrect)) + subrect->ymin;
			}
			else {
				mvalf[0] = (float)pt->x / 100.0f * ar->winx;
				mvalf[1] = (float)pt->y / 100.0f * ar->winy;
			}
		}
		
		/* convert screen coordinate to 3d coordinates 
		 *	- method taken from editview.c - mouse_cursor() 
		 */
		ED_view3d_win_to_3d(ar, fp, mvalf, p3d);
	}
}

/* --- */

/* temp struct for gp_stroke_path_animation() */
typedef struct tGpTimingData {
	/* Data set from operator settings */
	int mode;
	int frame_range; /* Number of frames evaluated for path animation */
	int start_frame, end_frame;
	int realtime; /* A bool, actually, will overwrite end_frame in case of Original or CustomGap timing... */
	float gap_duration, gap_randomness; /* To be used with CustomGap mode*/
	int seed;

	/* Data set from points, used to compute final timing FCurve */
	int num_points, cur_point;

	/* Distances */
	float *dists;
	float tot_dist;

	/* Times */
	float *times; /* Note: Gap times will be negative! */
	float tot_time, gap_tot_time;
	double inittime;
} tGpTimingData;

/* init point buffers for timing data */
static void _gp_timing_data_set_nbr(tGpTimingData *gtd, int nbr)
{
	float *tmp;

	BLI_assert(nbr > gtd->num_points);
	
	/* distances */
	tmp = gtd->dists;
	gtd->dists = MEM_callocN(sizeof(float) * nbr, __func__);
	if (tmp) {
		memcpy(gtd->dists, tmp, sizeof(float) * gtd->num_points);
		MEM_freeN(tmp);
	}
	
	/* times */
	tmp = gtd->times;
	gtd->times = MEM_callocN(sizeof(float) * nbr, __func__);
	if (tmp) {
		memcpy(gtd->times, tmp, sizeof(float) * gtd->num_points);
		MEM_freeN(tmp);
	}

	gtd->num_points = nbr;
}

/* add stroke point to timing buffers */
static void gp_timing_data_add_point(tGpTimingData *gtd, double stroke_inittime, float time, float delta_dist)
{
	if (time < 0.0f) {
		/* This is a gap, negative value! */
		gtd->times[gtd->cur_point] = -(((float)(stroke_inittime - gtd->inittime)) + time);
		gtd->tot_time = -gtd->times[gtd->cur_point];
		
		gtd->gap_tot_time += gtd->times[gtd->cur_point] - gtd->times[gtd->cur_point - 1];
	}
	else {
		gtd->times[gtd->cur_point] = (((float)(stroke_inittime - gtd->inittime)) + time);
		gtd->tot_time = (gtd->times[gtd->cur_point]);
	}
	
	gtd->tot_dist += delta_dist;
	gtd->dists[gtd->cur_point] = gtd->tot_dist;
	
	gtd->cur_point++;
}

/* In frames! Binary search for FCurve keys have a threshold of 0.01, so we can't set
 * arbitrarily close points - this is esp. important with NoGaps mode!
 */
#define MIN_TIME_DELTA 0.02f

/* Loop over next points to find the end of the stroke, and compute */
static int gp_find_end_of_stroke_idx(tGpTimingData *gtd, int idx, int nbr_gaps, int *nbr_done_gaps,
                                     float tot_gaps_time, float delta_time, float *next_delta_time)
{
	int j;
	
	for (j = idx + 1; j < gtd->num_points; j++) {
		if (gtd->times[j] < 0) {
			gtd->times[j] = -gtd->times[j];
			if (gtd->mode == GP_STROKECONVERT_TIMING_CUSTOMGAP) {
				/* In this mode, gap time between this stroke and the next should be 0 currently...
				 * So we have to compute its final duration!
				 */
				if (gtd->gap_randomness > 0.0f) {
					/* We want gaps that are in gtd->gap_duration +/- gtd->gap_randomness range,
					 * and which sum to exactly tot_gaps_time...
					 */
					int rem_gaps = nbr_gaps - (*nbr_done_gaps);
					if (rem_gaps < 2) {
						/* Last gap, just give remaining time! */
						*next_delta_time = tot_gaps_time;
					}
					else {
						float delta, min, max;
						
						/* This code ensures that if the first gaps have been shorter than average gap_duration,
						 * next gaps will tend to be longer (i.e. try to recover the lateness), and vice-versa!
						 */
						delta = delta_time - (gtd->gap_duration * (*nbr_done_gaps));
						
						/* Clamp min between [-gap_randomness, 0.0], with lower delta giving higher min */
						min = -gtd->gap_randomness - delta;
						CLAMP(min, -gtd->gap_randomness, 0.0f);
						
						/* Clamp max between [0.0, gap_randomness], with lower delta giving higher max */
						max = gtd->gap_randomness - delta;
						CLAMP(max, 0.0f, gtd->gap_randomness);
						*next_delta_time += gtd->gap_duration + (BLI_frand() * (max - min)) + min;
					}
				}
				else {
					*next_delta_time += gtd->gap_duration;
				}
			}
			(*nbr_done_gaps)++;
			break;
		}
	}

	return j - 1;
}

static void gp_stroke_path_animation_preprocess_gaps(tGpTimingData *gtd, int *nbr_gaps, float *tot_gaps_time)
{
	int i;
	float delta_time = 0.0f;

	for (i = 0; i < gtd->num_points; i++) {
		if (gtd->times[i] < 0 && i) {
			(*nbr_gaps)++;
			gtd->times[i] = -gtd->times[i] - delta_time;
			delta_time += gtd->times[i] - gtd->times[i - 1];
			gtd->times[i] = -gtd->times[i - 1]; /* Temp marker, values *have* to be different! */
		}
		else {
			gtd->times[i] -= delta_time;
		}
	}
	gtd->tot_time -= delta_time;

	*tot_gaps_time = (float)(*nbr_gaps) * gtd->gap_duration;
	gtd->tot_time += *tot_gaps_time;
	if (G.debug & G_DEBUG) {
		printf("%f, %f, %f, %d\n", gtd->tot_time, delta_time, *tot_gaps_time, *nbr_gaps);
	}
	if (gtd->gap_randomness > 0.0f) {
		BLI_srandom(gtd->seed);
	}
}

static void gp_stroke_path_animation_add_keyframes(ReportList *reports, PointerRNA ptr, PropertyRNA *prop, FCurve *fcu,
                                                   Curve *cu, tGpTimingData *gtd, float time_range,
                                                   int nbr_gaps, float tot_gaps_time)
{
	/* Use actual recorded timing! */
	float time_start = (float)gtd->start_frame;

	float last_valid_time = 0.0f;
	int end_stroke_idx = -1, start_stroke_idx = 0;
	float end_stroke_time = 0.0f;

	/* CustomGaps specific */
	float delta_time = 0.0f, next_delta_time = 0.0f;
	int nbr_done_gaps = 0;

	int i;
	float cfra;

	/* This is a bit tricky, as:
	 * - We can't add arbitrarily close points on FCurve (in time).
	 * - We *must* have all "caps" points of all strokes in FCurve, as much as possible!
	 */
	for (i = 0; i < gtd->num_points; i++) {
		/* If new stroke... */
		if (i > end_stroke_idx) {
			start_stroke_idx = i;
			delta_time = next_delta_time;
			/* find end of that new stroke */
			end_stroke_idx = gp_find_end_of_stroke_idx(gtd, i, nbr_gaps, &nbr_done_gaps,
			                                           tot_gaps_time, delta_time, &next_delta_time);
			/* This one should *never* be negative! */
			end_stroke_time = time_start + ((gtd->times[end_stroke_idx] + delta_time) / gtd->tot_time * time_range);
		}
		
		/* Simple proportional stuff... */
		cu->ctime = gtd->dists[i] / gtd->tot_dist * cu->pathlen;
		cfra = time_start + ((gtd->times[i] + delta_time) / gtd->tot_time * time_range);
		
		/* And now, the checks about timing... */
		if (i == start_stroke_idx) {
			/* If first point of a stroke, be sure it's enough ahead of last valid keyframe, and
			 * that the end point of the stroke is far enough!
			 * In case it is not, we keep the end point...
			 * Note that with CustomGaps mode, this is here we set the actual gap timing!
			 */
			if ((end_stroke_time - last_valid_time) > MIN_TIME_DELTA * 2) {
				if ((cfra - last_valid_time) < MIN_TIME_DELTA) {
					cfra = last_valid_time + MIN_TIME_DELTA;
				}
				insert_keyframe_direct(reports, ptr, prop, fcu, cfra, INSERTKEY_FAST);
				last_valid_time = cfra;
			}
			else if (G.debug & G_DEBUG) {
				printf("\t Skipping start point %d, too close from end point %d\n", i, end_stroke_idx);
			}
		}
		else if (i == end_stroke_idx) {
			/* Always try to insert end point of a curve (should be safe enough, anyway...) */
			if ((cfra - last_valid_time) < MIN_TIME_DELTA) {
				cfra = last_valid_time + MIN_TIME_DELTA;
			}
			insert_keyframe_direct(reports, ptr, prop, fcu, cfra, INSERTKEY_FAST);
			last_valid_time = cfra;
		}
		else {
			/* Else ("middle" point), we only insert it if it's far enough from last keyframe,
			 * and also far enough from (not yet added!) end_stroke keyframe!
			 */
			if ((cfra - last_valid_time) > MIN_TIME_DELTA && (end_stroke_time - cfra) > MIN_TIME_DELTA) {
				insert_keyframe_direct(reports, ptr, prop, fcu, cfra, INSERTKEY_FAST);
				last_valid_time = cfra;
			}
			else if (G.debug & G_DEBUG) {
				printf("\t Skipping \"middle\" point %d, too close from last added point or end point %d\n",
				       i, end_stroke_idx);
			}
		}
	}
}

static void gp_stroke_path_animation(bContext *C, ReportList *reports, Curve *cu, tGpTimingData *gtd)
{
	Scene *scene = CTX_data_scene(C);
	bAction *act;
	FCurve *fcu;
	PointerRNA ptr;
	PropertyRNA *prop = NULL;
	int nbr_gaps = 0, i;
	
	if (gtd->mode == GP_STROKECONVERT_TIMING_NONE)
		return;
	
	/* gap_duration and gap_randomness are in frames, but we need seconds!!! */
	gtd->gap_duration = FRA2TIME(gtd->gap_duration);
	gtd->gap_randomness = FRA2TIME(gtd->gap_randomness);
	
	/* Enable path! */
	cu->flag |= CU_PATH;
	cu->pathlen = gtd->frame_range;
	
	/* Get RNA pointer to read/write path time values */
	RNA_id_pointer_create((ID *)cu, &ptr);
	prop = RNA_struct_find_property(&ptr, "eval_time");
	
	/* Ensure we have an F-Curve to add keyframes to */
	act = verify_adt_action((ID *)cu, TRUE);
	fcu = verify_fcurve(act, NULL, &ptr, "eval_time", 0, TRUE);
	
	if (G.debug & G_DEBUG) {
		printf("%s: tot len: %f\t\ttot time: %f\n", __func__, gtd->tot_dist, gtd->tot_time);
		for (i = 0; i < gtd->num_points; i++) {
			printf("\tpoint %d:\t\tlen: %f\t\ttime: %f\n", i, gtd->dists[i], gtd->times[i]);
		}
	}
	
	if (gtd->mode == GP_STROKECONVERT_TIMING_LINEAR) {
		float cfra;
		
		/* Linear extrapolation! */
		fcu->extend = FCURVE_EXTRAPOLATE_LINEAR;
		
		cu->ctime = 0.0f;
		cfra = (float)gtd->start_frame;
		insert_keyframe_direct(reports, ptr, prop, fcu, cfra, INSERTKEY_FAST);
		
		cu->ctime = cu->pathlen;
		if (gtd->realtime) {
			cfra += (float)TIME2FRA(gtd->tot_time); /* Seconds to frames */
		}
		else {
			cfra = (float)gtd->end_frame;
		}
		insert_keyframe_direct(reports, ptr, prop, fcu, cfra, INSERTKEY_FAST);
	}
	else {
		/* Use actual recorded timing! */
		float time_range;
		
		/* CustomGaps specific */
		float tot_gaps_time = 0.0f;
		
		/* Pre-process gaps, in case we don't want to keep their original timing */
		if (gtd->mode == GP_STROKECONVERT_TIMING_CUSTOMGAP) {
			gp_stroke_path_animation_preprocess_gaps(gtd, &nbr_gaps, &tot_gaps_time);
		}
		
		if (gtd->realtime) {
			time_range = (float)TIME2FRA(gtd->tot_time); /* Seconds to frames */
		}
		else {
			time_range = (float)(gtd->end_frame - gtd->start_frame);
		}
		
		if (G.debug & G_DEBUG) {
			printf("GP Stroke Path Conversion: Starting keying!\n");
		}
		
		gp_stroke_path_animation_add_keyframes(reports, ptr, prop, fcu, cu, gtd, time_range,
		                                       nbr_gaps, tot_gaps_time);
	}
	
	/* As we used INSERTKEY_FAST mode, we need to recompute all curve's handles now */
	calchandles_fcurve(fcu);
	
	if (G.debug & G_DEBUG) {
		printf("%s: \ntot len: %f\t\ttot time: %f\n", __func__, gtd->tot_dist, gtd->tot_time);
		for (i = 0; i < gtd->num_points; i++) {
			printf("\tpoint %d:\t\tlen: %f\t\ttime: %f\n", i, gtd->dists[i], gtd->times[i]);
		}
		printf("\n\n");
	}
	
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	/* send updates */
	DAG_id_tag_update(&cu->id, 0);
}

#undef MIN_TIME_DELTA

#define GAP_DFAC 0.05f
#define WIDTH_CORR_FAC 0.1f
#define BEZT_HANDLE_FAC 0.3f

/* convert stroke to 3d path */
static void gp_stroke_to_path(bContext *C, bGPDlayer *gpl, bGPDstroke *gps, Curve *cu, rctf *subrect, Nurb **curnu,
                              float minmax_weights[2], float rad_fac, int stitch, tGpTimingData *gtd)
{
	bGPDspoint *pt;
	Nurb *nu = (curnu) ? *curnu : NULL;
	BPoint *bp, *prev_bp = NULL;
	const int do_gtd = (gtd->mode != GP_STROKECONVERT_TIMING_NONE);
	int i, old_nbp = 0;

	/* create new 'nurb' or extend current one within the curve */
	if (nu) {
		old_nbp = nu->pntsu;
		
		/* If stitch, the first point of this stroke is already present in current nu.
		 * Else, we have to add to additional points to make the zero-radius link between strokes.
		 */
		BKE_nurb_points_add(nu, gps->totpoints + (stitch ? -1 : 2));
	}
	else {
		nu = (Nurb *)MEM_callocN(sizeof(Nurb), "gpstroke_to_path(nurb)");
		
		nu->pntsu = gps->totpoints;
		nu->pntsv = 1;
		nu->orderu = 2; /* point-to-point! */
		nu->type = CU_NURBS;
		nu->flagu = CU_NURB_ENDPOINT;
		nu->resolu = cu->resolu;
		nu->resolv = cu->resolv;
		nu->knotsu = NULL;
		
		nu->bp = (BPoint *)MEM_callocN(sizeof(BPoint) * nu->pntsu, "bpoints");
		
		stitch = FALSE; /* Security! */
	}

	if (do_gtd) {
		_gp_timing_data_set_nbr(gtd, nu->pntsu);
	}

	/* If needed, make the link between both strokes with two zero-radius additional points */
	/* About "zero-radius" point interpolations:
	 * - If we have at least two points in current curve (most common case), we linearly extrapolate
	 *   the last segment to get the first point (p1) position and timing.
	 * - If we do not have those (quite odd, but may happen), we linearly interpolate the last point
	 *   with the first point of the current stroke.
	 * The same goes for the second point, first segment of the current stroke is "negatively" extrapolated
	 * if it exists, else (if the stroke is a single point), linear interpolation with last curve point...
	 */
	if (curnu && !stitch && old_nbp) {
		float p1[3], p2[3], p[3], next_p[3];
		float delta_time;

		prev_bp = NULL;
		if ((old_nbp > 1) && gps->prev && (gps->prev->totpoints > 1)) {
			/* Only use last curve segment if previous stroke was not a single-point one! */
			prev_bp = nu->bp + old_nbp - 2;
		}
		bp = nu->bp + old_nbp - 1;
		
		/* XXX We do this twice... Not sure it's worth to bother about this! */
		gp_strokepoint_convertcoords(C, gps, gps->points, p, subrect);
		if (prev_bp) {
			interp_v3_v3v3(p1, prev_bp->vec, bp->vec, 1.0f + GAP_DFAC);
		}
		else {
			interp_v3_v3v3(p1, bp->vec, p, GAP_DFAC);
		}
		
		if (gps->totpoints > 1) {
			/* XXX We do this twice... Not sure it's worth to bother about this! */
			gp_strokepoint_convertcoords(C, gps, gps->points + 1, next_p, subrect);
			interp_v3_v3v3(p2, p, next_p, -GAP_DFAC);
		}
		else {
			interp_v3_v3v3(p2, p, bp->vec, GAP_DFAC);
		}
		
		/* First point */
		bp++;
		copy_v3_v3(bp->vec, p1);
		bp->vec[3] = 1.0f;
		bp->f1 = SELECT;
		minmax_weights[0] = bp->radius = bp->weight = 0.0f;
		if (do_gtd) {
			if (prev_bp) {
				delta_time = gtd->tot_time + (gtd->tot_time - gtd->times[gtd->cur_point - 1]) * GAP_DFAC;
			}
			else {
				delta_time = gtd->tot_time + (((float)(gps->inittime - gtd->inittime)) - gtd->tot_time) * GAP_DFAC;
			}
			gp_timing_data_add_point(gtd, gtd->inittime, delta_time, len_v3v3((bp - 1)->vec, p1));
		}
		
		/* Second point */
		bp++;
		copy_v3_v3(bp->vec, p2);
		bp->vec[3] = 1.0f;
		bp->f1 = SELECT;
		minmax_weights[0] = bp->radius = bp->weight = 0.0f;
		if (do_gtd) {
			/* This negative delta_time marks the gap! */
			if (gps->totpoints > 1) {
				delta_time = ((gps->points + 1)->time - gps->points->time) * -GAP_DFAC;
			}
			else {
				delta_time = -(((float)(gps->inittime - gtd->inittime)) - gtd->tot_time) * GAP_DFAC;
			}
			gp_timing_data_add_point(gtd, gps->inittime, delta_time, len_v3v3(p1, p2));
		}
		
		old_nbp += 2;
	}
	if (old_nbp && do_gtd) {
		prev_bp = nu->bp + old_nbp - 1;
	}
	
	/* add points */
	for (i = (stitch) ? 1 : 0, pt = gps->points + ((stitch) ? 1 : 0), bp = nu->bp + old_nbp;
	     i < gps->totpoints;
	     i++, pt++, bp++)
	{
		float p3d[3];
		float width = pt->pressure * gpl->thickness * WIDTH_CORR_FAC;
		
		/* get coordinates to add at */
		gp_strokepoint_convertcoords(C, gps, pt, p3d, subrect);
		copy_v3_v3(bp->vec, p3d);
		bp->vec[3] = 1.0f;
		
		/* set settings */
		bp->f1 = SELECT;
		bp->radius = width * rad_fac;
		bp->weight = width;
		CLAMP(bp->weight, 0.0f, 1.0f);
		if (bp->weight < minmax_weights[0]) {
			minmax_weights[0] = bp->weight;
		}
		else if (bp->weight > minmax_weights[1]) {
			minmax_weights[1] = bp->weight;
		}
		
		/* Update timing data */
		if (do_gtd) {
			gp_timing_data_add_point(gtd, gps->inittime, pt->time, (prev_bp) ? len_v3v3(prev_bp->vec, p3d) : 0.0f);
		}
		prev_bp = bp;
	}
	
	/* add nurb to curve */
	if (!curnu || !*curnu) {
		BLI_addtail(&cu->nurb, nu);
	}
	if (curnu) {
		*curnu = nu;
	}
	
	BKE_nurb_knot_calc_u(nu);
}

static int gp_camera_view_subrect(bContext *C, rctf *subrect)
{
	View3D *v3d = CTX_wm_view3d(C);
	ARegion *ar = CTX_wm_region(C);
	
	if (v3d) {
		RegionView3D *rv3d = ar->regiondata;
		
		/* for camera view set the subrect */
		if (rv3d->persp == RV3D_CAMOB) {
			Scene *scene = CTX_data_scene(C);
			ED_view3d_calc_camera_border(scene, ar, v3d, rv3d, subrect, TRUE); /* no shift */
			return 1;
		}
	}
	
	return 0;
}

/* convert stroke to 3d bezier */
static void gp_stroke_to_bezier(bContext *C, bGPDlayer *gpl, bGPDstroke *gps, Curve *cu, rctf *subrect, Nurb **curnu,
                                float minmax_weights[2], float rad_fac, int stitch, tGpTimingData *gtd)
{
	bGPDspoint *pt;
	Nurb *nu = (curnu) ? *curnu : NULL;
	BezTriple *bezt, *prev_bezt = NULL;
	int i, tot, old_nbezt = 0;
	float p3d_cur[3], p3d_prev[3], p3d_next[3];
	const int do_gtd = (gtd->mode != GP_STROKECONVERT_TIMING_NONE);
	
	/* create new 'nurb' or extend current one within the curve */
	if (nu) {
		old_nbezt = nu->pntsu;
		/* If we do stitch, first point of current stroke is assumed the same as last point of previous stroke,
		 * so no need to add it.
		 * If no stitch, we want to add two additional points to make a "zero-radius" link between both strokes.
		 */
		BKE_nurb_bezierPoints_add(nu, gps->totpoints + ((stitch) ? -1 : 2));
	}
	else {
		nu = (Nurb *)MEM_callocN(sizeof(Nurb), "gpstroke_to_bezier(nurb)");
		
		nu->pntsu = gps->totpoints;
		nu->resolu = 12;
		nu->resolv = 12;
		nu->type = CU_BEZIER;
		nu->bezt = (BezTriple *)MEM_callocN(gps->totpoints * sizeof(BezTriple), "bezts");
		
		stitch = FALSE; /* Security! */
	}

	if (do_gtd) {
		_gp_timing_data_set_nbr(gtd, nu->pntsu);
	}

	tot = gps->totpoints;

	/* get initial coordinates */
	pt = gps->points;
	if (tot) {
		gp_strokepoint_convertcoords(C, gps, pt, (stitch) ? p3d_prev : p3d_cur, subrect);
		if (tot > 1) {
			gp_strokepoint_convertcoords(C, gps, pt + 1, (stitch) ? p3d_cur : p3d_next, subrect);
		}
		if (stitch && tot > 2) {
			gp_strokepoint_convertcoords(C, gps, pt + 2, p3d_next, subrect);
		}
	}

	/* If needed, make the link between both strokes with two zero-radius additional points */
	if (curnu && old_nbezt) {
		/* Update last point's second handle */
		if (stitch) {
			float h2[3];
			bezt = nu->bezt + old_nbezt - 1;
			interp_v3_v3v3(h2, bezt->vec[1], p3d_cur, BEZT_HANDLE_FAC);
			copy_v3_v3(bezt->vec[2], h2);
			pt++;
		}
		
		/* Create "link points" */
		/* About "zero-radius" point interpolations:
		 * - If we have at least two points in current curve (most common case), we linearly extrapolate
		 *   the last segment to get the first point (p1) position and timing.
		 * - If we do not have those (quite odd, but may happen), we linearly interpolate the last point
		 *   with the first point of the current stroke.
		 * The same goes for the second point, first segment of the current stroke is "negatively" extrapolated
		 * if it exists, else (if the stroke is a single point), linear interpolation with last curve point...
		 */
		else {
			float h1[3], h2[3], p1[3], p2[3];
			float delta_time;
			
			prev_bezt = NULL;
			if (old_nbezt > 1 && gps->prev && gps->prev->totpoints > 1) {
				/* Only use last curve segment if previous stroke was not a single-point one! */
				prev_bezt = nu->bezt + old_nbezt - 2;
			}
			bezt = nu->bezt + old_nbezt - 1;
			if (prev_bezt) {
				interp_v3_v3v3(p1, prev_bezt->vec[1], bezt->vec[1], 1.0f + GAP_DFAC);
			}
			else {
				interp_v3_v3v3(p1, bezt->vec[1], p3d_cur, GAP_DFAC);
			}
			if (tot > 1) {
				interp_v3_v3v3(p2, p3d_cur, p3d_next, -GAP_DFAC);
			}
			else {
				interp_v3_v3v3(p2, p3d_cur, bezt->vec[1], GAP_DFAC);
			}
			
			/* Second handle of last point */
			interp_v3_v3v3(h2, bezt->vec[1], p1, BEZT_HANDLE_FAC);
			copy_v3_v3(bezt->vec[2], h2);
			
			/* First point */
			interp_v3_v3v3(h1, p1, bezt->vec[1], BEZT_HANDLE_FAC);
			interp_v3_v3v3(h2, p1, p2, BEZT_HANDLE_FAC);
			
			bezt++;
			copy_v3_v3(bezt->vec[0], h1);
			copy_v3_v3(bezt->vec[1], p1);
			copy_v3_v3(bezt->vec[2], h2);
			bezt->h1 = bezt->h2 = HD_FREE;
			bezt->f1 = bezt->f2 = bezt->f3 = SELECT;
			minmax_weights[0] = bezt->radius = bezt->weight = 0.0f;
			
			if (do_gtd) {
				if (prev_bezt) {
					delta_time = gtd->tot_time + (gtd->tot_time - gtd->times[gtd->cur_point - 1]) * GAP_DFAC;
				}
				else {
					delta_time = gtd->tot_time + (((float)(gps->inittime - gtd->inittime)) - gtd->tot_time) * GAP_DFAC;
				}
				gp_timing_data_add_point(gtd, gtd->inittime, delta_time, len_v3v3((bezt - 1)->vec[1], p1));
			}
			
			/* Second point */
			interp_v3_v3v3(h1, p2, p1, BEZT_HANDLE_FAC);
			interp_v3_v3v3(h2, p2, p3d_cur, BEZT_HANDLE_FAC);
			
			bezt++;
			copy_v3_v3(bezt->vec[0], h1);
			copy_v3_v3(bezt->vec[1], p2);
			copy_v3_v3(bezt->vec[2], h2);
			bezt->h1 = bezt->h2 = HD_FREE;
			bezt->f1 = bezt->f2 = bezt->f3 = SELECT;
			minmax_weights[0] = bezt->radius = bezt->weight = 0.0f;
			
			if (do_gtd) {
				/* This negative delta_time marks the gap! */
				if (tot > 1) {
					delta_time = ((gps->points + 1)->time - gps->points->time) * -GAP_DFAC;
				}
				else {
					delta_time = -(((float)(gps->inittime - gtd->inittime)) - gtd->tot_time) * GAP_DFAC;
				}
				gp_timing_data_add_point(gtd, gps->inittime, delta_time, len_v3v3(p1, p2));
			}
			
			old_nbezt += 2;
			copy_v3_v3(p3d_prev, p2);
		}
	}
	if (old_nbezt && do_gtd) {
		prev_bezt = nu->bezt + old_nbezt - 1;
	}
	
	/* add points */
	for (i = stitch ? 1 : 0, bezt = nu->bezt + old_nbezt; i < tot; i++, pt++, bezt++) {
		float h1[3], h2[3];
		float width = pt->pressure * gpl->thickness * WIDTH_CORR_FAC;
		
		if (i || old_nbezt) {
			interp_v3_v3v3(h1, p3d_cur, p3d_prev, BEZT_HANDLE_FAC);
		}
		else {
			interp_v3_v3v3(h1, p3d_cur, p3d_next, -BEZT_HANDLE_FAC);
		}
		
		if (i < tot - 1) {
			interp_v3_v3v3(h2, p3d_cur, p3d_next, BEZT_HANDLE_FAC);
		}
		else {
			interp_v3_v3v3(h2, p3d_cur, p3d_prev, -BEZT_HANDLE_FAC);
		}
		
		copy_v3_v3(bezt->vec[0], h1);
		copy_v3_v3(bezt->vec[1], p3d_cur);
		copy_v3_v3(bezt->vec[2], h2);
		
		/* set settings */
		bezt->h1 = bezt->h2 = HD_FREE;
		bezt->f1 = bezt->f2 = bezt->f3 = SELECT;
		bezt->radius = width * rad_fac;
		bezt->weight = width;
		CLAMP(bezt->weight, 0.0f, 1.0f);
		if (bezt->weight < minmax_weights[0]) {
			minmax_weights[0] = bezt->weight;
		}
		else if (bezt->weight > minmax_weights[1]) {
			minmax_weights[1] = bezt->weight;
		}
		
		/* Update timing data */
		if (do_gtd) {
			gp_timing_data_add_point(gtd, gps->inittime, pt->time, prev_bezt ? len_v3v3(prev_bezt->vec[1], p3d_cur) : 0.0f);
		}
		
		/* shift coord vects */
		copy_v3_v3(p3d_prev, p3d_cur);
		copy_v3_v3(p3d_cur, p3d_next);
		
		if (i + 2 < tot) {
			gp_strokepoint_convertcoords(C, gps, pt + 2, p3d_next, subrect);
		}
		
		prev_bezt = bezt;
	}
	
	/* must calculate handles or else we crash */
	BKE_nurb_handles_calc(nu);

	if (!curnu || !*curnu) {
		BLI_addtail(&cu->nurb, nu);
	}
	if (curnu) {
		*curnu = nu;
	}
}

#undef GAP_DFAC
#undef WIDTH_CORR_FAC
#undef BEZT_HANDLE_FAC

static void gp_stroke_finalize_curve_endpoints(Curve *cu)
{
	/* start */
	Nurb *nu = cu->nurb.first;
	int i = 0;
	if (nu->bezt) {
		BezTriple *bezt = nu->bezt;
		if (bezt) {
			bezt[i].weight = bezt[i].radius = 0.0f;
		}
	}
	else if (nu->bp) {
		BPoint *bp = nu->bp;
		if (bp) {
			bp[i].weight = bp[i].radius = 0.0f;
		}
	}
	
	/* end */
	nu = cu->nurb.last;
	i = nu->pntsu - 1;
	if (nu->bezt) {
		BezTriple *bezt = nu->bezt;
		if (bezt) {
			bezt[i].weight = bezt[i].radius = 0.0f;
		}
	}
	else if (nu->bp) {
		BPoint *bp = nu->bp;
		if (bp) {
			bp[i].weight = bp[i].radius = 0.0f;
		}
	}
}

static void gp_stroke_norm_curve_weights(Curve *cu, float minmax_weights[2])
{
	Nurb *nu;
	const float delta = minmax_weights[0];
	const float fac = 1.0f / (minmax_weights[1] - delta);
	int i;
	
	for (nu = cu->nurb.first; nu; nu = nu->next) {
		if (nu->bezt) {
			BezTriple *bezt = nu->bezt;
			for (i = 0; i < nu->pntsu; i++, bezt++) {
				bezt->weight = (bezt->weight - delta) * fac;
			}
		}
		else if (nu->bp) {
			BPoint *bp = nu->bp;
			for (i = 0; i < nu->pntsu; i++, bp++) {
				bp->weight = (bp->weight - delta) * fac;
			}
		}
	}
}

/* convert a given grease-pencil layer to a 3d-curve representation (using current view if appropriate) */
static void gp_layer_to_curve(bContext *C, ReportList *reports, bGPdata *gpd, bGPDlayer *gpl, int mode,
                              int norm_weights, float rad_fac, int link_strokes, tGpTimingData *gtd)
{
	Scene *scene = CTX_data_scene(C);
	bGPDframe *gpf = gpencil_layer_getframe(gpl, CFRA, 0);
	bGPDstroke *gps, *prev_gps = NULL;
	Object *ob;
	Curve *cu;
	Nurb *nu = NULL;
	Base *base = BASACT, *newbase = NULL;
	float minmax_weights[2] = {1.0f, 0.0f};

	/* camera framing */
	rctf subrect, *subrect_ptr = NULL;
	
	/* error checking */
	if (ELEM3(NULL, gpd, gpl, gpf))
		return;
	
	/* only convert if there are any strokes on this layer's frame to convert */
	if (gpf->strokes.first == NULL)
		return;

	/* initialize camera framing */
	if (gp_camera_view_subrect(C, &subrect)) {
		subrect_ptr = &subrect;
	}
	
	/* init the curve object (remove rotation and get curve data from it)
	 *	- must clear transforms set on object, as those skew our results
	 */
	ob = BKE_object_add(scene, OB_CURVE);
	zero_v3(ob->loc);
	zero_v3(ob->rot);
	cu = ob->data;
	cu->flag |= CU_3D;
	
	/* rename object and curve to layer name */
	rename_id((ID *)ob, gpl->info);
	rename_id((ID *)cu, gpl->info);
	
	gtd->inittime = ((bGPDstroke *)gpf->strokes.first)->inittime;
	
	/* add points to curve */
	for (gps = gpf->strokes.first; gps; gps = gps->next) {
		/* Detect new strokes created because of GP_STROKE_BUFFER_MAX reached,
		 * and stitch them to previous one.
		 */
		int stitch = FALSE;
		
		if (prev_gps) {
			bGPDspoint *pt1 = prev_gps->points + prev_gps->totpoints - 1;
			bGPDspoint *pt2 = gps->points;
			
			if ((pt1->x == pt2->x) && (pt1->y == pt2->y)) {
				stitch = TRUE;
			}
		}
		
		/* Decide whether we connect this stroke to previous one */
		if (!(stitch || link_strokes)) {
			nu = NULL;
		}
		
		switch (mode) {
			case GP_STROKECONVERT_PATH: 
				gp_stroke_to_path(C, gpl, gps, cu, subrect_ptr, &nu, minmax_weights, rad_fac, stitch, gtd);
				break;
			case GP_STROKECONVERT_CURVE:
				gp_stroke_to_bezier(C, gpl, gps, cu, subrect_ptr, &nu, minmax_weights, rad_fac, stitch, gtd);
				break;
			default:
				BLI_assert(!"invalid mode");
				break;
		}
		prev_gps = gps;
	}

	/* If link_strokes, be sure first and last points have a zero weight/size! */
	if (link_strokes)
		gp_stroke_finalize_curve_endpoints(cu);

	/* Update curve's weights, if needed */
	if (norm_weights && ((minmax_weights[0] > 0.0f) || (minmax_weights[1] < 1.0f)))
		gp_stroke_norm_curve_weights(cu, minmax_weights);

	/* Create the path animation, if needed */
	gp_stroke_path_animation(C, reports, cu, gtd);

	/* Reset original object as active, else we can't edit operator's settings!!! */
	/* set layers OK */
	newbase = BASACT;
	if (base) {
		newbase->lay = base->lay;
		ob->lay = newbase->lay;
	}
	
	/* restore, BKE_object_add sets active */
	BASACT = base;
	if (base) {
		base->flag |= SELECT;
	}
}

/* --- */

/* Check a GP layer has valid timing data! Else, most timing options are hidden in the operator.
 * op may be NULL.
 */
static int gp_convert_check_has_valid_timing(bContext *C, bGPDlayer *gpl, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	bGPDframe *gpf = gpencil_layer_getframe(gpl, CFRA, 0);
	bGPDstroke *gps = gpf->strokes.first;
	bGPDspoint *pt;
	double base_time, cur_time, prev_time = -1.0;
	int i, valid = TRUE;
	
	do {
		base_time = cur_time = gps->inittime;
		if (cur_time <= prev_time) {
			valid = FALSE;
			break;
		}
		
		prev_time = cur_time;
		for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
			cur_time = base_time + (double)pt->time;
			/* First point of a stroke should have the same time as stroke's inittime,
			 * so it's the only case where equality is allowed!
			 */
			if ((i && cur_time <= prev_time) || (cur_time < prev_time)) {
				valid = FALSE;
				break;
			}
			prev_time = cur_time;
		}
		
		if (!valid) {
			break;
		}
	} while ((gps = gps->next));
	
	if (op) {
		RNA_boolean_set(op->ptr, "use_timing_data", valid);
	}
	return valid;
}

/* Check end_frame is always > start frame! */
static void gp_convert_set_end_frame(struct Main *UNUSED(main), struct Scene *UNUSED(scene), struct PointerRNA *ptr)
{
	int start_frame = RNA_int_get(ptr, "start_frame");
	int end_frame = RNA_int_get(ptr, "end_frame");
	
	if (end_frame <= start_frame) {
		RNA_int_set(ptr, "end_frame", start_frame + 1);
	}
}

static int gp_convert_poll(bContext *C)
{
	bGPdata *gpd = gpencil_data_get_active(C);
	ScrArea *sa = CTX_wm_area(C);
	Scene *scene = CTX_data_scene(C);

	/* only if there's valid data, and the current view is 3D View */
	return ((sa && sa->spacetype == SPACE_VIEW3D) && gpencil_layer_getactive(gpd) && (scene->obedit == NULL));
}

static int gp_convert_layer_exec(bContext *C, wmOperator *op)
{
	PropertyRNA *prop = RNA_struct_find_property(op->ptr, "use_timing_data");
	bGPdata *gpd = gpencil_data_get_active(C);
	bGPDlayer *gpl = gpencil_layer_getactive(gpd);
	Scene *scene = CTX_data_scene(C);
	int mode = RNA_enum_get(op->ptr, "type");
	int norm_weights = RNA_boolean_get(op->ptr, "use_normalize_weights");
	float rad_fac = RNA_float_get(op->ptr, "radius_multiplier");
	int link_strokes = RNA_boolean_get(op->ptr, "use_link_strokes");
	int valid_timing;
	tGpTimingData gtd;
	
	/* check if there's data to work with */
	if (gpd == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data to work on");
		return OPERATOR_CANCELLED;
	}
	
	if (!RNA_property_is_set(op->ptr, prop) && !gp_convert_check_has_valid_timing(C, gpl, op)) {
		BKE_report(op->reports, RPT_WARNING,
		           "Current Grease Pencil strokes have no valid timing data, most timing options will be hidden!");
	}
	valid_timing = RNA_property_boolean_get(op->ptr, prop);
	
	gtd.mode = RNA_enum_get(op->ptr, "timing_mode");
	/* Check for illegal timing mode! */
	if (!valid_timing && !ELEM(gtd.mode, GP_STROKECONVERT_TIMING_NONE, GP_STROKECONVERT_TIMING_LINEAR)) {
		gtd.mode = GP_STROKECONVERT_TIMING_LINEAR;
		RNA_enum_set(op->ptr, "timing_mode", gtd.mode);
	}
	if (!link_strokes) {
		gtd.mode = GP_STROKECONVERT_TIMING_NONE;
	}
	
	/* grab all relevant settings */
	gtd.frame_range = RNA_int_get(op->ptr, "frame_range");
	gtd.start_frame = RNA_int_get(op->ptr, "start_frame");
	gtd.realtime = valid_timing ? RNA_boolean_get(op->ptr, "use_realtime") : FALSE;
	gtd.end_frame = RNA_int_get(op->ptr, "end_frame");
	gtd.gap_duration = RNA_float_get(op->ptr, "gap_duration");
	gtd.gap_randomness = RNA_float_get(op->ptr, "gap_randomness");
	gtd.gap_randomness = min_ff(gtd.gap_randomness, gtd.gap_duration);
	gtd.seed = RNA_int_get(op->ptr, "seed");
	gtd.num_points = gtd.cur_point = 0;
	gtd.dists = gtd.times = NULL;
	gtd.tot_dist = gtd.tot_time = gtd.gap_tot_time = 0.0f;
	gtd.inittime = 0.0;
	
	/* perform conversion */
	gp_layer_to_curve(C, op->reports, gpd, gpl, mode, norm_weights, rad_fac, link_strokes, &gtd);
	
	/* free temp memory */
	if (gtd.dists) {
		MEM_freeN(gtd.dists);
		gtd.dists = NULL;
	}
	if (gtd.times) {
		MEM_freeN(gtd.times);
		gtd.times = NULL;
	}
	
	/* notifiers */
	WM_event_add_notifier(C, NC_OBJECT | NA_ADDED, NULL);
	WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
	
	/* done */
	return OPERATOR_FINISHED;
}

static int gp_convert_draw_check_prop(PointerRNA *ptr, PropertyRNA *prop)
{
	const char *prop_id = RNA_property_identifier(prop);
	int link_strokes = RNA_boolean_get(ptr, "use_link_strokes");
	int timing_mode = RNA_enum_get(ptr, "timing_mode");
	int realtime = RNA_boolean_get(ptr, "use_realtime");
	float gap_duration = RNA_float_get(ptr, "gap_duration");
	float gap_randomness = RNA_float_get(ptr, "gap_randomness");
	int valid_timing = RNA_boolean_get(ptr, "use_timing_data");
	
	/* Always show those props */
	if (strcmp(prop_id, "type") == 0 ||
	    strcmp(prop_id, "use_normalize_weights") == 0 ||
	    strcmp(prop_id, "radius_multiplier") == 0 ||
	    strcmp(prop_id, "use_link_strokes") == 0)
	{
		return TRUE;
	}
	
	/* Never show this prop */
	if (strcmp(prop_id, "use_timing_data") == 0)
		return FALSE;

	if (link_strokes) {
		/* Only show when link_stroke is TRUE */
		if (strcmp(prop_id, "timing_mode") == 0)
			return TRUE;
		
		if (timing_mode != GP_STROKECONVERT_TIMING_NONE) {
			/* Only show when link_stroke is TRUE and stroke timing is enabled */
			if (strcmp(prop_id, "frame_range") == 0 ||
			    strcmp(prop_id, "start_frame") == 0)
			{
				return TRUE;
			}
			
			/* Only show if we have valid timing data! */
			if (valid_timing && strcmp(prop_id, "use_realtime") == 0)
				return TRUE;
			
			/* Only show if realtime or valid_timing is FALSE! */
			if ((!realtime || !valid_timing) && strcmp(prop_id, "end_frame") == 0)
				return TRUE;
			
			if (valid_timing && timing_mode == GP_STROKECONVERT_TIMING_CUSTOMGAP) {
				/* Only show for custom gaps! */
				if (strcmp(prop_id, "gap_duration") == 0)
					return TRUE;
				
				/* Only show randomness for non-null custom gaps! */
				if (strcmp(prop_id, "gap_randomness") == 0 && (gap_duration > 0.0f))
					return TRUE;
				
				/* Only show seed for randomize action! */
				if (strcmp(prop_id, "seed") == 0 && (gap_duration > 0.0f) && (gap_randomness > 0.0f))
					return TRUE;
			}
		}
	}

	/* Else, hidden! */
	return FALSE;
}

static void gp_convert_ui(bContext *C, wmOperator *op)
{
	uiLayout *layout = op->layout;
	wmWindowManager *wm = CTX_wm_manager(C);
	PointerRNA ptr;

	RNA_pointer_create(&wm->id, op->type->srna, op->properties, &ptr);

	/* Main auto-draw call */
	uiDefAutoButsRNA(layout, &ptr, gp_convert_draw_check_prop, '\0');
}

void GPENCIL_OT_convert(wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	/* identifiers */
	ot->name = "Convert Grease Pencil";
	ot->idname = "GPENCIL_OT_convert";
	ot->description = "Convert the active Grease Pencil layer to a new Curve Object";
	
	/* callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = gp_convert_layer_exec;
	ot->poll = gp_convert_poll;
	ot->ui = gp_convert_ui;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", prop_gpencil_convertmodes, 0, "Type", "Which type of curve to convert to");
	
	RNA_def_boolean(ot->srna, "use_normalize_weights", TRUE, "Normalize Weight",
	                "Normalize weight (set from stroke width)");
	RNA_def_float(ot->srna, "radius_multiplier", 1.0f, 0.0f, 1000.0f, "Radius Fac",
	              "Multiplier for the points' radii (set from stroke width)", 0.0f, 10.0f);
	RNA_def_boolean(ot->srna, "use_link_strokes", TRUE, "Link Strokes",
	                "Whether to link strokes with zero-radius sections of curves");
	
	prop = RNA_def_enum(ot->srna, "timing_mode", prop_gpencil_convert_timingmodes, GP_STROKECONVERT_TIMING_FULL,
	                    "Timing Mode", "How to use timing data stored in strokes");
	RNA_def_enum_funcs(prop, rna_GPConvert_mode_items);
	
	RNA_def_int(ot->srna, "frame_range", 100, 1, 10000, "Frame Range",
	            "The duration of evaluation of the path control curve", 1, 1000);
	RNA_def_int(ot->srna, "start_frame", 1, 1, 100000, "Start Frame",
	            "The start frame of the path control curve", 1, 100000);
	RNA_def_boolean(ot->srna, "use_realtime", FALSE, "Realtime",
	                "Whether the path control curve reproduces the drawing in realtime, starting from Start Frame");
	prop = RNA_def_int(ot->srna, "end_frame", 250, 1, 100000, "End Frame",
	                   "The end frame of the path control curve (if Realtime is not set)", 1, 100000);
	RNA_def_property_update_runtime(prop, gp_convert_set_end_frame);
	
	RNA_def_float(ot->srna, "gap_duration", 0.0f, 0.0f, 10000.0f, "Gap Duration",
	              "Custom Gap mode: (Average) length of gaps, in frames "
	              "(Note: Realtime value, will be scaled if Realtime is not set)", 0.0f, 1000.0f);
	RNA_def_float(ot->srna, "gap_randomness", 0.0f, 0.0f, 10000.0f, "Gap Randomness",
	              "Custom Gap mode: Number of frames that gap lengths can vary", 0.0f, 1000.0f);
	RNA_def_int(ot->srna, "seed", 0, 0, 1000, "Random Seed",
	            "Custom Gap mode: Random generator seed", 0, 100);
				
	/* Note: Internal use, this one will always be hidden by UI code... */
	prop = RNA_def_boolean(ot->srna, "use_timing_data", FALSE, "Has Valid Timing",
	                       "Whether the converted Grease Pencil layer has valid timing data (internal use)");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* ************************************************ */
