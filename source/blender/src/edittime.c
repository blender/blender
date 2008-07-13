/**
 * $Id: BIF_edittime.c
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_action_types.h"
#include "DNA_ipo_types.h"
#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "BKE_ipo.h"
#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_library.h"

#include "BIF_space.h"
#include "BIF_screen.h"
#include "BIF_interface.h"
#include "BIF_toolbox.h"
#include "BIF_mywindow.h"
#include "BIF_editaction.h"

#include "BSE_drawipo.h"
#include "BSE_edit.h"
#include "BSE_headerbuttons.h"
#include "BSE_time.h"

#include "BDR_editobject.h"

#include "blendef.h"

#include "mydevice.h"

#include "PIL_time.h"

/* declarations */
void winqreadtimespace(ScrArea *, void *, BWinEvent *);

/* ************* Marker API **************** */

/* add TimeMarker at curent frame */
void add_marker(int frame)
{
	TimeMarker *marker;
	
	/* two markers can't be at the same place */
	for(marker= G.scene->markers.first; marker; marker= marker->next)
		if(marker->frame == frame) return;
	/* deselect all */
	for(marker= G.scene->markers.first; marker; marker= marker->next)
		marker->flag &= ~SELECT;
		
	marker = MEM_callocN(sizeof(TimeMarker), "TimeMarker");
	marker->flag= SELECT;
	marker->frame= frame;
	BLI_addtail(&(G.scene->markers), marker);
	
	BIF_undo_push("Add Marker");
}



/* remove selected TimeMarkers */
void remove_marker(void)
{
	TimeMarker *marker, *nmarker;
	short changed= 0;
		
	for(marker= G.scene->markers.first; marker; marker= nmarker) {
		nmarker= marker->next;
		if(marker->flag & SELECT) {
			BLI_freelinkN(&(G.scene->markers), marker);
			changed= 1;
		}
	}
	
	if (changed)
		BIF_undo_push("Remove Marker");
}

/* rename first selected TimeMarker */
void rename_marker(void)
{
	TimeMarker *marker;
	char name[64];
			
	for(marker= G.scene->markers.first; marker; marker= marker->next) {
		if(marker->flag & SELECT) {
			strcpy(name, marker->name);
			if (sbutton(name, 0, sizeof(name)-1, "Name: "))
				BLI_strncpy(marker->name, name, sizeof(marker->name));
			break;
		}
	}
	
//	BIF_undo_push("Rename Marker");
}

/* duplicate selected TimeMarkers */
void duplicate_marker(void)
{
	TimeMarker *marker, *newmarker;
	
	/* go through the list of markers, duplicate selected markers and add duplicated copies
	 * to the begining of the list (unselect original markers) */
	for(marker= G.scene->markers.first; marker; marker= marker->next) {
		if(marker->flag & SELECT){
			/* unselect selected marker */
			marker->flag &= ~SELECT;
			/* create and set up new marker */
			newmarker = MEM_callocN(sizeof(TimeMarker), "TimeMarker");
			newmarker->flag= SELECT;
			newmarker->frame= marker->frame;
			BLI_strncpy(newmarker->name, marker->name, sizeof(marker->name));
			/* new marker is added to the begining of list */
			BLI_addhead(&(G.scene->markers), newmarker);
		}
	}
	
	transform_markers('g', 0);
}

void transform_markers(int mode, int smode)	// mode and smode unused here, for callback
{
	SpaceLink *slink= curarea->spacedata.first;
	SpaceTime *stime= curarea->spacedata.first;
	SpaceAction *saction = curarea->spacedata.first;
	ListBase *markers;
	TimeMarker *marker, *selmarker=NULL;
	float dx, fac;
	int a, ret_val= 0, totmark=0, *oldframe, offs, firsttime=1;
	unsigned short event;
	short val, pmval[2], mval[2], mvalo[2];
	char str[32];
	
	/* hack for pose-markers in action editor */
	if ((slink->spacetype == SPACE_ACTION) && (saction->flag & SACTION_POSEMARKERS_MOVE)) {
		if (saction->action)
			markers= &saction->action->markers;
		else
			markers= NULL;
	}
	else
		markers= &G.scene->markers;
	
	for (marker= markers->first; marker; marker= marker->next) {
		if (marker->flag & SELECT) totmark++;
	}
	if (totmark==0) return;
	
	oldframe= MEM_mallocN(totmark*sizeof(int), "marker array");
	for (a=0, marker= markers->first; marker; marker= marker->next) {
		if (marker->flag & SELECT) {
			oldframe[a]= marker->frame;
			selmarker= marker;	// used for headerprint
			a++;
		}
	}
	
	dx= G.v2d->mask.xmax-G.v2d->mask.xmin;
	dx= (G.v2d->cur.xmax-G.v2d->cur.xmin)/dx;
	
	getmouseco_areawin(pmval);
	mvalo[0]= pmval[0];
	
	while (ret_val == 0) {
		getmouseco_areawin(mval);
		
		if (mval[0] != mvalo[0] || firsttime) {
			mvalo[0]= mval[0];
			firsttime= 0;
			
			fac= (((float)(mval[0] - pmval[0]))*dx);
			
			if (ELEM(slink->spacetype, SPACE_TIME, SPACE_SOUND)) 
				apply_keyb_grid(&fac, 0.0, FPS, 0.1*FPS, 0);
			else
				apply_keyb_grid(&fac, 0.0, 1.0, 0.1, U.flag & USER_AUTOGRABGRID);
			offs= (int)fac;
			
			for (a=0, marker= markers->first; marker; marker= marker->next) {
				if (marker->flag & SELECT) {
					marker->frame= oldframe[a] + offs;
					a++;
				}
			}
			
			if (totmark==1) {	
				/* we print current marker value */
				if (ELEM(slink->spacetype, SPACE_TIME, SPACE_SOUND)) {
					if (stime->flag & TIME_DRAWFRAMES) 
						sprintf(str, "Marker %d offset %d", selmarker->frame, offs);
					else 
						sprintf(str, "Marker %.2f offset %.2f", FRA2TIME(selmarker->frame), FRA2TIME(offs));
				}
				else if (slink->spacetype == SPACE_ACTION) {
					if (saction->flag & SACTION_DRAWTIME)
						sprintf(str, "Marker %.2f offset %.2f", FRA2TIME(selmarker->frame), FRA2TIME(offs));
					else
						sprintf(str, "Marker %.2f offset %.2f", (double)(selmarker->frame), (double)(offs));
				}
				else {
					sprintf(str, "Marker %.2f offset %.2f", (double)(selmarker->frame), (double)(offs));
				}
			}
			else {
				/* we only print the offset */
				if (ELEM(slink->spacetype, SPACE_TIME, SPACE_SOUND)) { 
					if (stime->flag & TIME_DRAWFRAMES) 
						sprintf(str, "Marker offset %d ", offs);
					else 
						sprintf(str, "Marker offset %.2f ", FRA2TIME(offs));
				}
				else if (slink->spacetype == SPACE_ACTION) {
					if (saction->flag & SACTION_DRAWTIME)
						sprintf(str, "Marker offset %.2f ", FRA2TIME(offs));
					else
						sprintf(str, "Marker offset %.2f ", (double)(offs));
				}
				else {
					sprintf(str, "Marker offset %.2f ", (double)(offs));
				}
			}
			headerprint(str);
			
			force_draw(0);	// areas identical to this, 0 = no header
		}
		else PIL_sleep_ms(10);	// idle
		
		/* emptying queue and reading events */
		while ( qtest() ) {
			event= extern_qread(&val);
			
			if (val) {
				if (ELEM(event, ESCKEY, RIGHTMOUSE)) ret_val= 2;
				else if (ELEM3(event, LEFTMOUSE, RETKEY, SPACEKEY)) ret_val= 1;
			}
		}
	}
	
	/* restore? */
	if (ret_val==2) {
		for (a=0, marker= markers->first; marker; marker= marker->next) {
			if (marker->flag & SELECT) {
				marker->frame= oldframe[a];
				a++;
			}
		}
	}
	else {
		BIF_undo_push("Move Markers");
	}
	MEM_freeN(oldframe);
	allqueue(REDRAWMARKER, 0);
}

/* select/deselect all TimeMarkers
 * 	test - based on current selections?
 *	sel - selection status to set all markers to if blanket apply status
 */
void deselect_markers(short test, short sel)
{
	TimeMarker *marker;
		
	/* check if need to find out whether to how to select markers */
	if (test) {
		/* dependant on existing selection */
		/* determine if select all or deselect all */
		sel = 1;
		for (marker= G.scene->markers.first; marker; marker= marker->next) {
			if (marker->flag & SELECT) {
				sel = 0;
				break;
			}
		}
		
		/* do selection */
		for (marker= G.scene->markers.first; marker; marker= marker->next) {
			if (sel == 2) {
				marker->flag ^= SELECT;
			}
			else if (sel == 1) {
				if ((marker->flag & SELECT)==0) 
					marker->flag |= SELECT;
			}
			else {
				if (marker->flag & SELECT)
					marker->flag &= ~SELECT;
			}
		}
	}
	else {
		/* not dependant on existing selection */
		for (marker= G.scene->markers.first; marker; marker= marker->next) {
				if (sel==2) {
					marker->flag ^= SELECT;
				}
				else if (sel==1) {
					if ((marker->flag & SELECT)==0)
						marker->flag |= SELECT;
				}
				else {
					if (marker->flag & SELECT)
						marker->flag &= ~SELECT;
				}
		}
	}
}

static void borderselect_markers_func(float xmin, float xmax, int selectmode)
{
	TimeMarker *marker;
		
	for(marker= G.scene->markers.first; marker; marker= marker->next) {
		if ((marker->frame > xmin) && (marker->frame <= xmax)) {
			switch (selectmode) {
				case SELECT_ADD:
					if ((marker->flag & SELECT) == 0) 
						marker->flag |= SELECT;
					break;
				case SELECT_SUBTRACT:
					if (marker->flag & SELECT) 
						marker->flag &= ~SELECT;
					break;
			}
		}
	}
}

/* border-select markers */
void borderselect_markers(void) 
{
	rcti rect;
	rctf rectf;
	int val, selectmode;		
	short	mval[2];

	if ( (val = get_border(&rect, 3)) ){
		if (val == LEFTMOUSE)
			selectmode = SELECT_ADD;
		else
			selectmode = SELECT_SUBTRACT;

		mval[0]= rect.xmin;
		mval[1]= rect.ymin+2;
		areamouseco_to_ipoco(G.v2d, mval, &rectf.xmin, &rectf.ymin);
		mval[0]= rect.xmax;
		mval[1]= rect.ymax-2;
		areamouseco_to_ipoco(G.v2d, mval, &rectf.xmax, &rectf.ymax);
			
		/* do markers */
		borderselect_markers_func(rectf.xmin, rectf.xmax, selectmode);
		
		BIF_undo_push("Border Select Markers");
		allqueue(REDRAWMARKER, 0);
	}
}

void nextprev_marker(short dir)
{
	TimeMarker *marker, *cur=NULL, *first, *last;
	int mindist= MAXFRAME, dist;
		
	first= last= G.scene->markers.first; 
	for(marker= G.scene->markers.first; marker; marker= marker->next) {
		/* find closest to current frame first */
		dist= (marker->frame/G.scene->r.framelen) - CFRA;
		if(dir==1 && dist>0 && dist<mindist) {
			mindist= dist;
			cur= marker;
		}
		else if(dir==-1 && dist<0 && -dist<mindist) {
			mindist= -dist;
			cur= marker;
		}
		/* find first/last */
		if(marker->frame > last->frame) last= marker;
		if(marker->frame < first->frame) first= marker;
	}
	
	if(cur==NULL) {
		if(dir==1) cur= first;
		else cur= last;
	}
	if(cur) {
		CFRA= cur->frame/G.scene->r.framelen;
		update_for_newframe();
		allqueue(REDRAWALL, 0);
	}
}

void get_minmax_markers(short sel, float *first, float *last)
{
	TimeMarker *marker;
	ListBase *markers;
	float min, max;
	int selcount = 0;
	
	markers= &(G.scene->markers);
	
	if (sel)
		for (marker= markers->first; marker; marker= marker->next) {
			if (marker->flag & SELECT)
				selcount++;
		}
	else {
		selcount= BLI_countlist(markers);
	}
	
	if (markers->first && markers->last) {
		min= ((TimeMarker *)markers->first)->frame;
		max= ((TimeMarker *)markers->last)->frame;
	}
	else {
		*first = 0.0f;
		*last = 0.0f;
		return;
	}
	
	if (selcount > 1) {
		for (marker= markers->first; marker; marker= marker->next) {
			if (sel) {
				if (marker->flag & SELECT) {
					if (marker->frame < min)
						min= marker->frame;
					else if (marker->frame > max)
						max= marker->frame;
				}
			}
			else {
				if (marker->frame < min)
					min= marker->frame;
				else if (marker->frame > max)
					max= marker->frame;
			}	
		}
	}
	
	*first= min;
	*last= max;
}

TimeMarker *find_nearest_marker(ListBase *markers, int clip_y)
{
	TimeMarker *marker;
	float xmin, xmax;
	rctf	rectf;
	short mval[2];
	
	getmouseco_areawin (mval);
	
	/* first clip selection in Y */
	if ((clip_y) && (mval[1] > 30))
		return NULL;
	
	mval[0]-=7;
	areamouseco_to_ipoco(G.v2d, mval, &rectf.xmin, &rectf.ymin);
	mval[0]+=14;
	areamouseco_to_ipoco(G.v2d, mval, &rectf.xmax, &rectf.ymax);
	
	xmin= rectf.xmin;
	xmax= rectf.xmax;
	
	for (marker= markers->first; marker; marker= marker->next) {
		if ((marker->frame > xmin) && (marker->frame <= xmax)) {
			return marker;
		}
	}
	
	return NULL;
}

/* Adds a marker to list of cfra elems */
void add_marker_to_cfra_elem(ListBase *lb, TimeMarker *marker, short only_sel)
{
	CfraElem *ce, *cen;
	
	/* should this one only be considered if it is selected? */
	if ((only_sel) && ((marker->flag & SELECT)==0))
		return;
	
	/* try to find a previous cfra elem */
	ce= lb->first;
	while(ce) {
		
		if( ce->cfra==marker->frame ) {
			/* do because of double keys */
			if(marker->flag & SELECT) ce->sel= marker->flag;
			return;
		}
		else if(ce->cfra > marker->frame) break;
		
		ce= ce->next;
	}	
	
	cen= MEM_callocN(sizeof(CfraElem), "add_to_cfra_elem");	
	if(ce) BLI_insertlinkbefore(lb, ce, cen);
	else BLI_addtail(lb, cen);

	cen->cfra= marker->frame;
	cen->sel= marker->flag;
}

/* This function makes a list of all the markers. The only_sel
 * argument is used to specify whether only the selected markers
 * are added.
 */
void make_marker_cfra_list(ListBase *lb, short only_sel)
{
	TimeMarker *marker;
	
	for (marker= G.scene->markers.first; marker; marker= marker->next) {
		add_marker_to_cfra_elem(lb, marker, only_sel);
	}
}

int find_nearest_marker_time(float dx)
{
	TimeMarker *marker, *nearest= NULL;
	float dist, min_dist= 1000000;
	
	for(marker= G.scene->markers.first; marker; marker= marker->next) {
		dist = ABS((float)marker->frame - dx);
		if(dist < min_dist){
			min_dist= dist;
			nearest= marker;
		}
	}
	
	if(nearest) return nearest->frame;
	else return (int)floor(dx);
}

/* *********** End Markers - Markers API *************** */
/* select/deselect TimeMarker at current frame */
static void select_timeline_marker_frame(int frame, unsigned char shift)
{
	TimeMarker *marker;
	int select=0;
	
	for(marker= G.scene->markers.first; marker; marker= marker->next) {
		/* if Shift is not set, then deselect Markers */
		if(!shift) marker->flag &= ~SELECT;
		/* this way a not-shift select will allways give 1 selected marker */
		if((marker->frame == frame) && (!select)) {
			if(marker->flag & SELECT) 
				marker->flag &= ~SELECT;
			else
				marker->flag |= SELECT;
			select = 1;
		}
	}
}

/* *********** end Markers - TimeLine *************** */

/* set the animation preview range of scene */
void anim_previewrange_set()
{
	rcti rect;
	rctf rectf;
	short val, mval[2];
	
	/* set range by drawing border-select rectangle */
	if ( (val = get_border(&rect, 5)) ) {	
		/* get frame numbers */
		mval[0]= rect.xmin;
		mval[1]= rect.ymin+2;
		areamouseco_to_ipoco(G.v2d, mval, &rectf.xmin, &rectf.ymin);
		mval[0]= rect.xmax;
		mval[1]= rect.ymax-2;
		areamouseco_to_ipoco(G.v2d, mval, &rectf.xmax, &rectf.ymax);
			
		/* set preview-range */
		if (rectf.xmin < 1) rectf.xmin = 1.0f;
		if (rectf.xmax < 1) rectf.xmax = 1.0f;
		G.scene->r.psfra= rectf.xmin;
		G.scene->r.pefra= rectf.xmax;
		
		BIF_undo_push("Set anim-preview range");
		allqueue(REDRAWTIME, 0);
		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWNLA, 0);
		allqueue(REDRAWIPO, 0);
		allqueue(REDRAWBUTSALL, 0);
	}
}

/* clear the animation preview range for scene */
void anim_previewrange_clear()
{
	G.scene->r.psfra = 0;
	G.scene->r.pefra = 0;
	
	BIF_undo_push("Clear anim-preview range");
	allqueue(REDRAWTIME, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWNLA, 0);
	allqueue(REDRAWBUTSALL, 0);
}

/* ************ end Animation Preview Range ********** */


static int float_to_frame(float frame) 
{
	int to= (int) floor(0.5 + frame/G.scene->r.framelen );
	
	return to;	
}

static float find_closest_cfra_elem(ListBase elems, int dir, float closest)
{
	CfraElem *ce;
	
	for(ce= elems.first; ce; ce= ce->next) {
		if (dir==-1) {
			if( float_to_frame(ce->cfra)<CFRA) {
				if ((ce->cfra > closest) || (closest == CFRA)) {
					closest= ce->cfra;
				}
			}
		} 
		else {
			if(float_to_frame(ce->cfra)>CFRA) {
				if ((ce->cfra < closest) || (closest == CFRA)) {
					closest= ce->cfra;
				}
			}
		}
	}
	return closest;
}

void nextprev_timeline_key(short dir)
{
	/*mostly copied from drawobject.c, draw_object() AND editipo.c, movekey_obipo() */
	Object *ob;
	bActionChannel *achan;
	bAction *act;
	ListBase elems;
	float closest= CFRA;
	int a;
	
	if (OBACT) {
		ob = OBACT;
		
		if(ob) {
			if(ob!=G.obedit) {
				if(ob->ipo) {
					/* convert the ipo to a list of 'current frame elements' */
					
					elems.first= elems.last= NULL;
					make_cfra_list(ob->ipo, &elems);
					
					closest= find_closest_cfra_elem(elems, dir, closest);
					
					BLI_freelistN(&elems);
				}
				
				if(ob->action) {
					act = ob->action;
					/* go through each channel in the action */
					for (achan=act->chanbase.first; achan; achan=achan->next){
						/* convert the ipo to a list of 'current frame elements' */
						
						if(achan->ipo) {
							elems.first= elems.last= NULL;
							make_cfra_list(achan->ipo, &elems);
							
							closest= find_closest_cfra_elem(elems, dir, closest);
							
							BLI_freelistN(&elems);
						}
					}
				}
				
				for(a=0; a<ob->totcol; a++) {
					Material *ma= give_current_material(ob, a+1);
					if(ma && ma->ipo) {
						elems.first= elems.last= NULL;
						make_cfra_list(ma->ipo, &elems);
						
						closest= find_closest_cfra_elem(elems, dir, closest);
						
						BLI_freelistN(&elems);
					}
				}
			}
		}
		
		a= float_to_frame(closest);
		
		if (a!=CFRA) {
			CFRA= a;
			update_for_newframe();
		}	
		
		BIF_undo_push("Next/Prev Key");
		allqueue(REDRAWALL, 0);
	}
}

/* return the current marker for this frame,
we can have more then 1 marker per frame, this just returns the first :/ */
TimeMarker *get_frame_marker(int frame)
{
	TimeMarker *marker, *best_marker = NULL;
	int best_frame = -MAXFRAME*2; 
	for (marker= G.scene->markers.first; marker; marker= marker->next) {
		if (marker->frame==frame) {
			return marker;
		}
		
		if ( marker->frame > best_frame && marker->frame < frame) {
			best_marker = marker;
			best_frame = marker->frame;
		}
	}
	
	return best_marker;
}


void timeline_frame_to_center(void)
{
	float dtime;
	
	dtime= CFRA*(G.scene->r.framelen) - (G.v2d->cur.xmin + G.v2d->cur.xmax)/2.0; 
	G.v2d->cur.xmin += dtime;
	G.v2d->cur.xmax += dtime;
	scrarea_queue_winredraw(curarea);
}

/* copy of this is actually in editscreen.c, but event based */
static void timeline_force_draw(short val)
{
	ScrArea *sa, *tempsa, *samin= NULL;
	int dodraw;
	
	if(val & TIME_LEFTMOST_3D_WIN) {
		ScrArea *sa= G.curscreen->areabase.first;
		int min= 10000;
		for(; sa; sa= sa->next) {
			if(sa->spacetype==SPACE_VIEW3D) {
				if(sa->winrct.xmin - sa->winrct.ymin < min) {
					samin= sa;
					min= sa->winrct.xmin - sa->winrct.ymin;
				}
			}
		}
	}
	
	tempsa= curarea;
	sa= G.curscreen->areabase.first;
	while(sa) {
		dodraw= 0;
		if(sa->spacetype==SPACE_VIEW3D) {
			if(sa==samin || (val & TIME_ALL_3D_WIN)) dodraw= 1;
		}
		else if(ELEM5(sa->spacetype, SPACE_NLA, SPACE_IPO, SPACE_SEQ, SPACE_ACTION, SPACE_SOUND)) {
			if(val & TIME_ALL_ANIM_WIN) dodraw= 1;
		}
		else if(sa->spacetype==SPACE_BUTS) {
			if(val & TIME_ALL_BUTS_WIN) dodraw= 2;
		}
		else if(sa->spacetype==SPACE_IMAGE) {
			if (val & TIME_ALL_IMAGE_WIN) dodraw = 1;
		}
		else if(sa->spacetype==SPACE_SEQ) {
			if (val & TIME_SEQ) dodraw = 1;
		}
		else if(sa->spacetype==SPACE_TIME) dodraw= 2;
		
		if(dodraw) {
			areawinset(sa->win);
			scrarea_do_windraw(sa);
			if(dodraw==2) scrarea_do_headdraw(sa);
		}
		sa= sa->next;
	}
	areawinset(tempsa->win);
	
	screen_swapbuffers();

}

/* ***************************** */

/* Right. Now for some implementation: */
void winqreadtimespace(ScrArea *sa, void *spacedata, BWinEvent *evt)
{
	SpaceTime *stime= spacedata;
	unsigned short event= evt->event;
	short val= evt->val;
	float dx, dy;
	int doredraw= 0, cfra, first = 0;
	short mval[2], nr;
	short mousebut = L_MOUSE;
	
	if(sa->win==0) return;

	if(val) {
		
		if( uiDoBlocks(&sa->uiblocks, event, 1)!=UI_NOTHING ) event= 0;

		/* swap mouse buttons based on user preference */
		if (U.flag & USER_LMOUSESELECT) {
			if (event == LEFTMOUSE) {
				event = RIGHTMOUSE;
				mousebut = L_MOUSE;
			} else if (event == RIGHTMOUSE) {
				event = LEFTMOUSE;
				mousebut = R_MOUSE;
			}
		}

		switch(event) {
		case LEFTMOUSE:
			stime->flag |= TIME_CFRA_NUM;
			do {
				getmouseco_areawin(mval);
				areamouseco_to_ipoco(G.v2d, mval, &dx, &dy);
				
				cfra = (int)(dx+0.5f);
				if(cfra< MINFRAME) cfra= MINFRAME;
				
				if( cfra!=CFRA || first )
				{
					first= 0;
					CFRA= cfra;
					update_for_newframe_nodraw(0);	// 1= nosound
					timeline_force_draw(stime->redraws);
				}
				else PIL_sleep_ms(30);
			
			} while(get_mbut() & mousebut);
			
			stime->flag &= ~TIME_CFRA_NUM;
			allqueue(REDRAWALL, 0);
			break;
			
		case RIGHTMOUSE: /* select/deselect marker */
			getmouseco_areawin(mval);
			areamouseco_to_ipoco(G.v2d, mval, &dx, &dy);
			
			cfra= find_nearest_marker_time(dx);
			
			if (G.qual && LR_SHIFTKEY)
				select_timeline_marker_frame(cfra, 1);
			else
				select_timeline_marker_frame(cfra, 0);
			
			force_draw(0);
			std_rmouse_transform(transform_markers);

			break;
		case MIDDLEMOUSE:
		case WHEELUPMOUSE:
		case WHEELDOWNMOUSE:
			view2dmove(event);	/* in drawipo.c */
			break;
		case PADPLUSKEY:
			dx= (float)(0.1154*(G.v2d->cur.xmax-G.v2d->cur.xmin));
			G.v2d->cur.xmin+= dx;
			G.v2d->cur.xmax-= dx;
			test_view2d(G.v2d, sa->winx, sa->winy);
			view2d_do_locks(curarea, V2D_LOCK_COPY);
			doredraw= 1;
			break;
		case PADMINUS:
			dx= (float)(0.15*(G.v2d->cur.xmax-G.v2d->cur.xmin));
			G.v2d->cur.xmin-= dx;
			G.v2d->cur.xmax+= dx;
			test_view2d(G.v2d, sa->winx, sa->winy);
			view2d_do_locks(curarea, V2D_LOCK_COPY);
			doredraw= 1;
			break;
		case HOMEKEY:
			first= G.scene->r.sfra;
			if(first >= G.scene->r.efra) first= G.scene->r.efra;
			G.v2d->cur.xmin=G.v2d->tot.xmin= (float)first-2;
			G.v2d->cur.xmax=G.v2d->tot.xmax= (float)G.scene->r.efra+2;
			doredraw= 1;
			break;
			
		case PAGEUPKEY: /* next keyframe */
			if(G.qual==LR_CTRLKEY)
				nextprev_timeline_key(1);
			else
				nextprev_marker(1);
			break;
		case PAGEDOWNKEY: /* prev keyframe */
			if(G.qual==LR_CTRLKEY)
				nextprev_timeline_key(-1);
			else
				nextprev_marker(-1);
			break;
			
		case AKEY:
			/* deselect all TimeMarkers */
			deselect_markers(1, 0);
			allqueue(REDRAWMARKER, 0);
			break;
		case BKEY:
			/* borderselect markers */
			borderselect_markers();
			break;
		case DKEY:
			if(G.qual==LR_SHIFTKEY)
				duplicate_marker();
			break;
		case CKEY:
			timeline_frame_to_center();
			break;
		case GKEY: /* move marker */
			transform_markers('g', 0);
			break;
		case EKEY: /* set end frame */
			if (G.scene->r.psfra) {
				if (CFRA < G.scene->r.psfra)
					G.scene->r.psfra= CFRA;
				G.scene->r.pefra= CFRA;
			}				
			else
				G.scene->r.efra = CFRA;
			allqueue(REDRAWALL, 1);
			break;
		case MKEY: /* add, rename marker */
			if (G.qual & LR_CTRLKEY)
				rename_marker();
			else
				add_marker(CFRA);
			allqueue(REDRAWMARKER, 0);
			break;
		case PKEY:	/* preview-range stuff */
			if (G.qual & LR_CTRLKEY) /* set preview range */
				anim_previewrange_set();
			else if (G.qual & LR_ALTKEY) /* clear preview range */
				anim_previewrange_clear();
			break;
		case SKEY: /* set start frame */
			if (G.scene->r.psfra) {
				if (G.scene->r.pefra < CFRA)
					G.scene->r.pefra= CFRA;
				G.scene->r.psfra= CFRA;
			}				
			else
				G.scene->r.sfra = CFRA;
			allqueue(REDRAWALL, 1);
			break;
		case TKEY: /* popup menu */
			nr= pupmenu("Time value%t|Frames %x1|Seconds%x2");
			if (nr>0) {
				if(nr==1) stime->flag |= TIME_DRAWFRAMES;
				else stime->flag &= ~TIME_DRAWFRAMES;
				doredraw= 1;
			}
			break;
		case DELKEY:
		case XKEY:
			if( okee("Erase selected")==0 ) break;

			remove_marker();
			allqueue(REDRAWMARKER, 0);
			break;
		}
	}

	if(doredraw)
		scrarea_queue_winredraw(sa);
}


