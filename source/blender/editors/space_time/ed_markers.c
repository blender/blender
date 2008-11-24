/**
 * $Id:
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view2d_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BLI_blenlib.h"

#include "BKE_global.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_interface.h"
#include "UI_view2d.h"
#include "UI_resources.h"

#include "ED_markers.h"
#include "ED_types.h"

/* ************* Marker Drawing ************ */

/* XXX */
extern void ui_rasterpos_safe(float x, float y, float aspect);

/* function to draw markers */
static void draw_marker(View2D *v2d, TimeMarker *marker, int cfra, int flag)
{
	float xpos, ypixels, xscale, yscale;
	int icon_id= 0;
	
	xpos = marker->frame;
	/* no time correction for framelen! space is drawn with old values */
	
	ypixels= v2d->mask.ymax-v2d->mask.ymin;
	UI_view2d_getscale(v2d, &xscale, &yscale);
	
	glScalef( 1.0/xscale, 1.0/yscale, 1.0);
	
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);			
	
	/* verticle line */
	if (flag & DRAW_MARKERS_LINES) {
		setlinestyle(3);
		if(marker->flag & SELECT)
			glColor4ub(255,255,255, 96);
		else
			glColor4ub(0,0,0, 96);
		
		glBegin(GL_LINES);
		glVertex2f((xpos*xscale)+0.5, 12);
		glVertex2f((xpos*xscale)+0.5, 34*yscale); /* a bit lazy but we know it cant be greater then 34 strips high*/
		glEnd();
		setlinestyle(0);
	}
	
	/* 5 px to offset icon to align properly, space / pixels corrects for zoom */
	if (flag & DRAW_MARKERS_LOCAL) {
		icon_id= (marker->flag & ACTIVE) ? ICON_PMARKER_ACT : 
		(marker->flag & SELECT) ? ICON_PMARKER_SEL : 
		ICON_PMARKER;
	}
	else {
		icon_id= (marker->flag & SELECT) ? ICON_MARKER_HLT : 
		ICON_MARKER;
	}
	
	//BIF_icon_draw(xpos*xscale-5.0, 12.0, icon_id);
	glColor3ub(0, 100, 0);
	glRectf(xpos*xscale-5.0f, 12.0f, xpos*xscale, 17.0f);
	
	glBlendFunc(GL_ONE, GL_ZERO);
	glDisable(GL_BLEND);
	
	/* and the marker name too, shifted slightly to the top-right */
	if(marker->name && marker->name[0]) {
		if(marker->flag & SELECT) {
			//BIF_ThemeColor(TH_TEXT_HI);
			ui_rasterpos_safe(xpos*xscale+4.0, (ypixels<=39.0)?(ypixels-10.0):29.0, 1.0);
		}
		else {
			// BIF_ThemeColor(TH_TEXT);
			if((marker->frame <= cfra) && (marker->frame+5 > cfra))
				ui_rasterpos_safe(xpos*xscale+4.0, (ypixels<=39.0)?(ypixels-10.0):29.0, 1.0);
			else
				ui_rasterpos_safe(xpos*xscale+4.0, 17.0, 1.0);
		}
//		BIF_DrawString(G.font, marker->name, 0);
	}
	glScalef(xscale, yscale, 1.0);
}

/* Draw Scene-Markers in time window (XXX make generic) */
void draw_markers_time(const bContext *C, int flag)
{
	TimeMarker *marker;
	SpaceTime *stime= C->area->spacedata.first;
	View2D *v2d= &stime->v2d;
	
	/* unselected markers are drawn at the first time */
	for (marker= C->scene->markers.first; marker; marker= marker->next) {
		if (!(marker->flag & SELECT)) draw_marker(v2d, marker, C->scene->r.cfra, flag);
	}
	
	/* selected markers are drawn later */
	for (marker= C->scene->markers.first; marker; marker= marker->next) {
		if (marker->flag & SELECT) draw_marker(v2d, marker, C->scene->r.cfra, flag);
	}
}



/* ************* Marker API **************** */

/* add TimeMarker at curent frame */
static int ed_marker_add(bContext *C, wmOperator *op)
{
	TimeMarker *marker;
	int frame= C->scene->r.cfra;
	
	/* two markers can't be at the same place */
	for(marker= C->scene->markers.first; marker; marker= marker->next)
		if(marker->frame == frame) 
			return OPERATOR_CANCELLED;
	
	/* deselect all */
	for(marker= C->scene->markers.first; marker; marker= marker->next)
		marker->flag &= ~SELECT;
	
	marker = MEM_callocN(sizeof(TimeMarker), "TimeMarker");
	marker->flag= SELECT;
	marker->frame= frame;
	BLI_addtail(&(C->scene->markers), marker);
	
	//BIF_undo_push("Add Marker");
	
	return OPERATOR_FINISHED;
}


/* remove selected TimeMarkers */
void ed_marker_remove(bContext *C)
{
	TimeMarker *marker, *nmarker;
	short changed= 0;
	
	for(marker= C->scene->markers.first; marker; marker= nmarker) {
		nmarker= marker->next;
		if(marker->flag & SELECT) {
			BLI_freelinkN(&(C->scene->markers), marker);
			changed= 1;
		}
	}
	
//	if (changed)
//		BIF_undo_push("Remove Marker");
}


/* duplicate selected TimeMarkers */
void ed_marker_duplicate(bContext *C)
{
	TimeMarker *marker, *newmarker;
	
	/* go through the list of markers, duplicate selected markers and add duplicated copies
	* to the begining of the list (unselect original markers) */
	for(marker= C->scene->markers.first; marker; marker= marker->next) {
		if(marker->flag & SELECT){
			/* unselect selected marker */
			marker->flag &= ~SELECT;
			/* create and set up new marker */
			newmarker = MEM_callocN(sizeof(TimeMarker), "TimeMarker");
			newmarker->flag= SELECT;
			newmarker->frame= marker->frame;
			BLI_strncpy(newmarker->name, marker->name, sizeof(marker->name));
			/* new marker is added to the begining of list */
			BLI_addhead(&(C->scene->markers), newmarker);
		}
	}
	
//	transform_markers('g', 0);
}

void ED_MARKER_OT_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Time Marker";
	ot->idname= "ED_MARKER_OT_add";
	
	/* api callbacks */
	ot->exec= ed_marker_add;
	
}


/* ************************** registration **********************************/

void marker_operatortypes(void)
{
	WM_operatortype_append(ED_MARKER_OT_add);
}

	
