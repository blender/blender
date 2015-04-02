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
 *
 * Operators for editing Grease Pencil strokes
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
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_gpencil_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_library.h"
#include "BKE_report.h"
#include "BKE_screen.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_view2d.h"

#include "ED_gpencil.h"
#include "ED_view3d.h"

#include "gpencil_intern.h"

/* ************************************************ */
/* Stroke Editing Operators */

/* poll callback for all stroke editing operators */
static int gp_stroke_edit_poll(bContext *C)
{
	/* NOTE: this is a bit slower, but is the most accurate... */
	return CTX_DATA_COUNT(C, editable_gpencil_strokes) != 0;
}

/* ************** Duplicate Selected Strokes **************** */

/* Make copies of selected point segments in a selected stroke */
static void gp_duplicate_points(const bGPDstroke *gps, ListBase *new_strokes)
{
	bGPDspoint *pt;
	int i;
	
	int start_idx = -1;
	
	
	/* Step through the original stroke's points:
	 * - We accumulate selected points (from start_idx to current index)
	 *   and then convert that to a new stroke
	 */
	for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
		/* searching for start, are waiting for end? */
		if (start_idx == -1) {
			/* is this the first selected point for a new island? */
			if (pt->flag & GP_SPOINT_SELECT) {
				start_idx = i;
			}
		}
		else {
			size_t len = 0;
			
			/* is this the end of current island yet?
			 * 1) Point i-1 was the last one that was selected
			 * 2) Point i is the last in the array
			 */
			if ((pt->flag & GP_SPOINT_SELECT) == 0) {
				len = i - start_idx;
			}
			else if (i == gps->totpoints - 1) {
				len = i - start_idx + 1;
			}
			//printf("copying from %d to %d = %d\n", start_idx, i, len);
		
			/* make copies of the relevant data */
			if (len) {
				bGPDstroke *gpsd;
				
				/* make a stupid copy first of the entire stroke (to get the flags too) */
				gpsd = MEM_dupallocN(gps);
				
				/* now, make a new points array, and copy of the relevant parts */
				gpsd->points = MEM_callocN(sizeof(bGPDspoint) * len, "gps stroke points copy");
				memcpy(gpsd->points, gps->points + start_idx, sizeof(bGPDspoint) * len);
				gpsd->totpoints = len;
				
				/* add to temp buffer */
				gpsd->next = gpsd->prev = NULL;
				BLI_addtail(new_strokes, gpsd);
				
				/* cleanup + reset for next */
				start_idx = -1;
			}
		}
	}
}

static int gp_duplicate_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	
	if (gpd == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data");
		return OPERATOR_CANCELLED;
	}
	
	/* for each visible (and editable) layer's selected strokes,
	 * copy the strokes into a temporary buffer, then append
	 * once all done
	 */
	CTX_DATA_BEGIN(C, bGPDlayer *, gpl, editable_gpencil_layers)
	{
		ListBase new_strokes = {NULL, NULL};
		bGPDframe *gpf = gpl->actframe;
		bGPDstroke *gps;
		
		if (gpf == NULL)
			continue;
		
		/* make copies of selected strokes, and deselect these once we're done */
		for (gps = gpf->strokes.first; gps; gps = gps->next) {
			/* skip strokes that are invalid for current view */
			if (ED_gpencil_stroke_can_use(C, gps) == false)
				continue;
			
			if (gps->flag & GP_STROKE_SELECT) {
				if (gps->totpoints == 1) {
					/* Special Case: If there's just a single point in this stroke... */
					bGPDstroke *gpsd;
					
					/* make direct copies of the stroke and its points */
					gpsd = MEM_dupallocN(gps);
					gpsd->points = MEM_dupallocN(gps->points);
					
					/* add to temp buffer */
					gpsd->next = gpsd->prev = NULL;
					BLI_addtail(&new_strokes, gpsd);
				}
				else {
					/* delegate to a helper, as there's too much to fit in here (for copying subsets)... */
					gp_duplicate_points(gps, &new_strokes);
				}
				
				/* deselect original stroke, or else the originals get moved too
				 * (when using the copy + move macro)
				 */
				gps->flag &= ~GP_STROKE_SELECT;
			}
		}
		
		/* add all new strokes in temp buffer to the frame (preventing double-copies) */
		BLI_movelisttolist(&gpf->strokes, &new_strokes);
		BLI_assert(new_strokes.first == NULL);
	}
	CTX_DATA_END;
	
	/* updates */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_duplicate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Duplicate Strokes";
	ot->idname = "GPENCIL_OT_duplicate";
	ot->description = "Duplicate the selected Grease Pencil strokes";
	
	/* callbacks */
	ot->exec = gp_duplicate_exec;
	ot->poll = gp_stroke_edit_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ******************* Copy/Paste Strokes ************************* */
/* Grease Pencil stroke data copy/paste buffer:
 * - The copy operation collects all segments of selected strokes,
 *   dumping "ready to be copied" copies of the strokes into the buffer.
 * - The paste operation makes a copy of those elements, and adds them
 *   to the active layer. This effectively flattens down the strokes
 *   from several different layers into a single layer.
 */

/* list of bGPDstroke instances */
static ListBase gp_strokes_copypastebuf = {NULL, NULL};

/* Free copy/paste buffer data */
void ED_gpencil_strokes_copybuf_free(void)
{
	bGPDstroke *gps, *gpsn;
	
	for (gps = gp_strokes_copypastebuf.first; gps; gps = gpsn) {
		gpsn = gps->next;
		
		MEM_freeN(gps->points);
		BLI_freelinkN(&gp_strokes_copypastebuf, gps);
	}
	
	gp_strokes_copypastebuf.first = gp_strokes_copypastebuf.last = NULL;
}

/* --------------------- */
/* Copy selected strokes */

static int gp_strokes_copy_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	
	if (gpd == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data");
		return OPERATOR_CANCELLED;
	}
	
	/* clear the buffer first */
	ED_gpencil_strokes_copybuf_free();
	
	/* for each visible (and editable) layer's selected strokes,
	 * copy the strokes into a temporary buffer, then append
	 * once all done
	 */
	CTX_DATA_BEGIN(C, bGPDlayer *, gpl, editable_gpencil_layers)
	{
		bGPDframe *gpf = gpl->actframe;
		bGPDstroke *gps;
		
		if (gpf == NULL)
			continue;
		
		/* make copies of selected strokes, and deselect these once we're done */
		for (gps = gpf->strokes.first; gps; gps = gps->next) {
			/* skip strokes that are invalid for current view */
			if (ED_gpencil_stroke_can_use(C, gps) == false)
				continue;
			
			if (gps->flag & GP_STROKE_SELECT) {
				if (gps->totpoints == 1) {
					/* Special Case: If there's just a single point in this stroke... */
					bGPDstroke *gpsd;
					
					/* make direct copies of the stroke and its points */
					gpsd = MEM_dupallocN(gps);
					gpsd->points = MEM_dupallocN(gps->points);
					
					/* add to temp buffer */
					gpsd->next = gpsd->prev = NULL;
					BLI_addtail(&gp_strokes_copypastebuf, gpsd);
				}
				else {
					/* delegate to a helper, as there's too much to fit in here (for copying subsets)... */
					gp_duplicate_points(gps, &gp_strokes_copypastebuf);
				}
			}
		}
	}
	CTX_DATA_END;
	
	/* done - no updates needed */
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_copy(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Copy Strokes";
	ot->idname = "GPENCIL_OT_copy";
	ot->description = "Copy selected Grease Pencil points and strokes";
	
	/* callbacks */
	ot->exec = gp_strokes_copy_exec;
	ot->poll = gp_stroke_edit_poll;
	
	/* flags */
	//ot->flag = OPTYPE_REGISTER;
}

/* --------------------- */
/* Paste selected strokes */

static int gp_strokes_paste_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *gpl = CTX_data_active_gpencil_layer(C);
	bGPDframe *gpf;
	
	/* check for various error conditions */
	if (gpd == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data");
		return OPERATOR_CANCELLED;
	}
	else if (gp_strokes_copypastebuf.first == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No strokes to paste, select and copy some points before trying again");
		return OPERATOR_CANCELLED;
	}
	else if (gpl == NULL) {
		/* no active layer - let's just create one */
		gpl = gpencil_layer_addnew(gpd, DATA_("GP_Layer"), 1);
	}
	else if (gpl->flag & (GP_LAYER_HIDE | GP_LAYER_LOCKED)) {
		BKE_report(op->reports, RPT_ERROR, "Can not paste strokes when active layer is hidden or locked");
		return OPERATOR_CANCELLED;
	}
	else {
		/* Check that some of the strokes in the buffer can be used */
		bGPDstroke *gps;
		bool ok = false;
		
		for (gps = gp_strokes_copypastebuf.first; gps; gps = gps->next) {
			if (ED_gpencil_stroke_can_use(C, gps)) {
				ok = true;
				break;
			}
		}
		
		if (ok == false) {
			/* XXX: this check is not 100% accurate (i.e. image editor is incompatible with normal 2D strokes),
			 * but should be enough to give users a good idea of what's going on
			 */
			if (CTX_wm_area(C)->spacetype == SPACE_VIEW3D)
				BKE_report(op->reports, RPT_ERROR, "Cannot paste 2D strokes in 3D View");
			else
				BKE_report(op->reports, RPT_ERROR, "Cannot paste 3D strokes in 2D editors");
				
			return OPERATOR_CANCELLED;
		}
	}
	
	/* Deselect all strokes first */
	CTX_DATA_BEGIN(C, bGPDstroke *, gps, editable_gpencil_strokes)
	{
		bGPDspoint *pt;
		int i;
		
		for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
			pt->flag &= ~GP_SPOINT_SELECT;
		}
		
		gps->flag &= ~GP_STROKE_SELECT;
	}
	CTX_DATA_END;
	
	/* Ensure we have a frame to draw into
	 * NOTE: Since this is an op which creates strokes,
	 *       we are obliged to add a new frame if one
	 *       doesn't exist already
	 */
	gpf = gpencil_layer_getframe(gpl, CFRA, true);
	
	if (gpf) {
		bGPDstroke *gps;
		
		/* Copy each stroke into the layer */
		for (gps = gp_strokes_copypastebuf.first; gps; gps = gps->next) {
			if (ED_gpencil_stroke_can_use(C, gps)) {
				bGPDstroke *new_stroke = MEM_dupallocN(gps);
				
				new_stroke->points = MEM_dupallocN(gps->points);
				new_stroke->next = new_stroke->prev = NULL;
				
				BLI_addtail(&gpf->strokes, new_stroke);
			}
		}
	}
	
	/* updates */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_paste(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Paste Strokes";
	ot->idname = "GPENCIL_OT_paste";
	ot->description = "Paste previously copied strokes into active layer";
	
	/* callbacks */
	ot->exec = gp_strokes_paste_exec;
	ot->poll = gp_stroke_edit_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ******************* Delete Active Frame ************************ */

static int gp_actframe_delete_poll(bContext *C)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *gpl = gpencil_layer_getactive(gpd);
	
	/* only if there's an active layer with an active frame */
	return (gpl && gpl->actframe);
}

/* delete active frame - wrapper around API calls */
static int gp_actframe_delete_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	bGPdata *gpd = ED_gpencil_data_get_active(C);
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

/* ******************* Delete Operator ************************ */

typedef enum eGP_DeleteMode {
	/* delete selected stroke points */
	GP_DELETEOP_POINTS          = 0,
	/* delete selected strokes */
	GP_DELETEOP_STROKES         = 1,
	/* delete active frame */
	GP_DELETEOP_FRAME           = 2,
	/* delete selected stroke points (without splitting stroke) */
	GP_DELETEOP_POINTS_DISSOLVE = 3,
} eGP_DeleteMode;


/* Delete selected strokes */
static int gp_delete_selected_strokes(bContext *C)
{
	bool changed = false;
	
	CTX_DATA_BEGIN(C, bGPDlayer *, gpl, editable_gpencil_layers)
	{
		bGPDframe *gpf = gpl->actframe;
		bGPDstroke *gps, *gpsn;
		
		if (gpf == NULL)
			continue;
		
		/* simply delete strokes which are selected */
		for (gps = gpf->strokes.first; gps; gps = gpsn) {
			gpsn = gps->next;
			
			/* skip strokes that are invalid for current view */
			if (ED_gpencil_stroke_can_use(C, gps) == false)
				continue;
			
			/* free stroke if selected */
			if (gps->flag & GP_STROKE_SELECT) {
				/* free stroke memory arrays, then stroke itself */
				if (gps->points) MEM_freeN(gps->points);
				BLI_freelinkN(&gpf->strokes, gps);
				
				changed = true;
			}
		}
	}
	CTX_DATA_END;
	
	if (changed) {
		WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

/* Delete selected points but keep the stroke */
static int gp_dissolve_selected_points(bContext *C)
{
	bool changed = false;
	
	CTX_DATA_BEGIN(C, bGPDlayer *, gpl, editable_gpencil_layers)
	{
		bGPDframe *gpf = gpl->actframe;
		bGPDstroke *gps, *gpsn;
		
		if (gpf == NULL)
			continue;
		
		/* simply delete points from selected strokes
		 * NOTE: we may still have to remove the stroke if it ends up having no points!
		 */
		for (gps = gpf->strokes.first; gps; gps = gpsn) {
			gpsn = gps->next;
			
			/* skip strokes that are invalid for current view */
			if (ED_gpencil_stroke_can_use(C, gps) == false)
				continue;
			
			if (gps->flag & GP_STROKE_SELECT) {
				bGPDspoint *pt;
				int i;
				
				int tot = gps->totpoints; /* number of points in new buffer */
				
				/* First Pass: Count how many points are selected (i.e. how many to remove) */
				for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
					if (pt->flag & GP_SPOINT_SELECT) {
						/* selected point - one of the points to remove */
						tot--;
					}
				}
				
				/* if no points are left, we simply delete the entire stroke */
				if (tot <= 0) {
					/* remove the entire stroke */
					MEM_freeN(gps->points);
					BLI_freelinkN(&gpf->strokes, gps);
				}
				else {
					/* just copy all unselected into a smaller buffer */
					bGPDspoint *new_points = MEM_callocN(sizeof(bGPDspoint) * tot, "new gp stroke points copy");
					bGPDspoint *npt        = new_points;
					
					for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
						if ((pt->flag & GP_SPOINT_SELECT) == 0) {
							*npt = *pt;
							npt++;
						}
					}
					
					/* free the old buffer */
					MEM_freeN(gps->points);
					
					/* save the new buffer */
					gps->points = new_points;
					gps->totpoints = tot;
					
					/* deselect the stroke, since none of its selected points will still be selected */
					gps->flag &= ~GP_STROKE_SELECT;
				}
				
				changed = true;
			}
		}
	}
	CTX_DATA_END;
	
	if (changed) {
		WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

/* Split selected strokes into segments, splitting on selected points */
static int gp_delete_selected_points(bContext *C)
{
	bool changed = false;
	
	CTX_DATA_BEGIN(C, bGPDlayer *, gpl, editable_gpencil_layers)
	{
		bGPDframe *gpf = gpl->actframe;
		bGPDstroke *gps, *gpsn;
		
		if (gpf == NULL)
			continue;
		
		/* simply delete strokes which are selected */
		for (gps = gpf->strokes.first; gps; gps = gpsn) {
			gpsn = gps->next;
			
			/* skip strokes that are invalid for current view */
			if (ED_gpencil_stroke_can_use(C, gps) == false)
				continue;
			
			
			if (gps->flag & GP_STROKE_SELECT) {
				bGPDspoint *pt;
				int i;
				
				/* The algorithm used here is as follows:
				 * 1) We firstly identify the number of "islands" of non-selected points
				 *    which will all end up being in new strokes.
				 *    - In the most extreme case (i.e. every other vert is a 1-vert island),
				 *      we have at most n / 2 islands
				 *    - Once we start having larger islands than that, the number required
				 *      becomes much less
				 * 2) Each island gets converted to a new stroke
				 */
				typedef struct tGPDeleteIsland {
					int start_idx;
					int end_idx;
				} tGPDeleteIsland;
				
				tGPDeleteIsland *islands = MEM_callocN(sizeof(tGPDeleteIsland) * (gps->totpoints + 1) / 2, "gp_point_islands");
				bool in_island  = false;
				int num_islands = 0;
				
				/* First Pass: Identify start/end of islands */
				for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
					if (pt->flag & GP_SPOINT_SELECT) {
						/* selected - stop accumulating to island */
						in_island = false;
					}
					else {
						/* unselected - start of a new island? */
						int idx;
						
						if (in_island) {
							/* extend existing island */
							idx = num_islands - 1;
							islands[idx].end_idx = i;
						}
						else {
							/* start of new island */
							in_island = true;
							num_islands++;
							
							idx = num_islands - 1;
							islands[idx].start_idx = islands[idx].end_idx = i;
						}
					}
				}
				
				/* Watch out for special case where No islands = All points selected = Delete Stroke only */
				if (num_islands) {
					/* there are islands, so create a series of new strokes, adding them before the "next" stroke */
					int idx;
					
					/* deselect old stroke, since it will be used as template for the new strokes */
					gps->flag &= ~GP_STROKE_SELECT;
					
					/* create each new stroke... */
					for (idx = 0; idx < num_islands; idx++) {
						tGPDeleteIsland *island = &islands[idx];
						bGPDstroke *new_stroke  = MEM_dupallocN(gps);
						
						/* compute new buffer size (+ 1 needed as the endpoint index is "inclusive") */
						new_stroke->totpoints = island->end_idx - island->start_idx + 1;
						new_stroke->points    = MEM_callocN(sizeof(bGPDspoint) * new_stroke->totpoints, "gp delete stroke fragment");
						
						/* copy over the relevant points */
						memcpy(new_stroke->points, gps->points + island->start_idx, sizeof(bGPDspoint) * new_stroke->totpoints);
						
						/* add new stroke to the frame */
						if (gpsn) {
							BLI_insertlinkbefore(&gpf->strokes, gpsn, new_stroke);
						}
						else {
							BLI_addtail(&gpf->strokes, new_stroke);
						}
					}
				}
				
				/* free islands */
				MEM_freeN(islands);
				
				/* Delete the old stroke */
				MEM_freeN(gps->points);
				BLI_freelinkN(&gpf->strokes, gps);
				
				changed = true;
			}
		}
	}
	CTX_DATA_END;
	
	if (changed) {
		WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}


static int gp_delete_exec(bContext *C, wmOperator *op)
{
	eGP_DeleteMode mode = RNA_enum_get(op->ptr, "type");
	int result = OPERATOR_CANCELLED;
	
	switch (mode) {
		case GP_DELETEOP_STROKES:	/* selected strokes */
			result = gp_delete_selected_strokes(C);
			break;
		
		case GP_DELETEOP_POINTS:	/* selected points (breaks the stroke into segments) */
			result = gp_delete_selected_points(C);
			break;
		
		case GP_DELETEOP_POINTS_DISSOLVE: /* selected points (without splitting the stroke) */
			result = gp_dissolve_selected_points(C);
			break;
		
		case GP_DELETEOP_FRAME:		/* active frame */
			result = gp_actframe_delete_exec(C, op);
			break;
	}
	
	return result;
}

void GPENCIL_OT_delete(wmOperatorType *ot)
{
	static EnumPropertyItem prop_gpencil_delete_types[] = {
		{GP_DELETEOP_POINTS, "POINTS", 0, "Points", "Delete selected points and split strokes into segments"},
		{GP_DELETEOP_STROKES, "STROKES", 0, "Strokes", "Delete selected strokes"},
		{GP_DELETEOP_FRAME, "FRAME", 0, "Frame", "Delete active frame"},
		{0, "", 0, NULL, NULL},
		{GP_DELETEOP_POINTS_DISSOLVE, "DISSOLVE_POINTS", 0, "Dissolve Points",
		                              "Delete selected points without splitting strokes"},
		{0, NULL, 0, NULL, NULL}
	};
	
	/* identifiers */
	ot->name = "Delete...";
	ot->idname = "GPENCIL_OT_delete";
	ot->description = "Delete selected Grease Pencil strokes, vertices, or frames";
	
	/* callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = gp_delete_exec;
	ot->poll = gp_stroke_edit_poll;
	
	/* flags */
	ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;
	
	/* props */
	ot->prop = RNA_def_enum(ot->srna, "type", prop_gpencil_delete_types, 0, "Type", "Method used for deleting Grease Pencil data");
}

/* ************************************************ */
