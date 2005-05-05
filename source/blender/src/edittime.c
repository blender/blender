/**
 * $Id:
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
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
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "BKE_ipo.h"
#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_library.h"

#include "BIF_space.h"
#include "BIF_screen.h"
#include "BIF_interface.h"
#include "BIF_toolbox.h"
#include "BIF_mywindow.h"

#include "BSE_drawipo.h"
#include "BSE_headerbuttons.h"
#include "BSE_time.h"

#include "blendef.h"

#include "mydevice.h"

#include "PIL_time.h"

/* ************* Timeline marker code **************** */

/* add TimeMarker at curent frame */
void add_timeline_marker(int frame)
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
}

/* remove TimeMarker */
void remove_timeline_marker(void)
{
	TimeMarker *marker;
	
	for(marker= G.scene->markers.first; marker; marker= marker->next) {
		if(marker->flag & SELECT){
			BLI_freelinkN(&G.scene->markers, marker);
		}
	}
}

/* rename first selected TimeMarker */
void rename_timeline_marker(void)
{
	TimeMarker *marker;
	char name[64];
	
	for(marker= G.scene->markers.first; marker; marker= marker->next) {
		if(marker->flag & SELECT) {
			sprintf(name, marker->name);
			if (sbutton(name, 0, sizeof(name)-1, "Name: "))
				BLI_strncpy(marker->name, name, sizeof(marker->name));
			break;
		}
	}
}

static int find_nearest_marker(float dx)
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

/* select/deselect TimeMarker at current frame */
static void select_timeline_marker_frame(int frame, unsigned char shift)
{
	TimeMarker *marker;
	
	for(marker= G.scene->markers.first; marker; marker= marker->next) {
		/* if Shift is not set, then deselect Markers */
		if(!shift) marker->flag &= ~SELECT;
		/* this way a not-shift select will allways give 1 selected marker */
		if(marker->frame == frame) {
			if(marker->flag & SELECT) 
				marker->flag &= ~SELECT;
			else
				marker->flag |= SELECT;
		}
	}
}

/* select/deselect all TimeMarkers */
void select_timeline_markers(void)
{
	TimeMarker *marker;
	char any_selected= 0;
	
	for(marker= G.scene->markers.first; marker; marker= marker->next) {
		if(marker->flag & SELECT) any_selected= 1;
		marker->flag &= ~SELECT;
	}
	
	/* no TimeMarker selected, then select all TimeMarkers */
	if(!any_selected){
		for(marker= G.scene->markers.first; marker; marker= marker->next) {
			marker->flag |= SELECT;
		}
	}
}

void nextprev_timeline_marker(short dir)
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

/* *********** end Markers *************** */

static int float_to_frame(float frame) 
{
	int to= (int) floor(0.5 + frame/G.scene->r.framelen );
	
	return to;	
}

void nextprev_timeline_key(short dir)
{
	/*mostly copied from drawobject.c, draw_object() AND editipo.c, movekey_obipo() */
	Object *ob;
	bActionChannel *achan;
	bAction *act;
	CfraElem *ce;
	int a;
	float toframe= CFRA;
	
	if (OBACT) {
		ob = OBACT;
		
		if(ob) {
			if(ob!=G.obedit) {
				if(ob->ipo) {
					/* convert the ipo to a list of 'current frame elements' */
					ListBase elems;
					
					elems.first= elems.last= 0;
					make_cfra_list(ob->ipo, &elems);
					
					/* disable time offset for the purposes of drawing the frame ticks */
					/* ipoflag= ob->ipoflag;
					ob->ipoflag &= ~OB_OFFS_OB;
					
					set_no_parent_ipo(1);
					disable_speed_curve(1); */
					
					/* go through the list and decide if we can find a new keyframe to visit */
					if(elems.first) {
						ce= elems.first;
						if (dir==-1) {
							while (ce && float_to_frame(ce->cfra)<CFRA) {
								toframe= ce->cfra;
								ce= ce->next;
							}
						} else {
							while (ce && float_to_frame(ce->cfra)<=CFRA) {
								ce= ce->next;
							}
							if (ce) toframe= ce->cfra;
						}
					}
					/*set_no_parent_ipo(0);
					disable_speed_curve(0);
					
					ob->ipoflag= ipoflag; */
					BLI_freelistN(&elems);
				}
				
				if(ob->action) {
					act = ob->action;
					/* go through each channel in the action */
					for (achan=act->chanbase.first; achan; achan=achan->next){
						/* convert the ipo to a list of 'current frame elements' */
						ListBase elems;
						
						elems.first= elems.last= 0;
						make_cfra_list(achan->ipo, &elems);
						
						/* go through the list and decide if we can find a new keyframe to visit */
						if(elems.first) {
							ce= elems.first;
							if (dir==-1) {
								while (ce && float_to_frame(ce->cfra)<CFRA) {
									if ((ce->cfra > toframe) || (toframe == CFRA)) {
										toframe= ce->cfra;
									}
									ce= ce->next;
								}
							} else {
								while (ce && float_to_frame(ce->cfra)<=CFRA) {
									ce= ce->next;
								}
								if (ce) {
									if ((ce->cfra < toframe) || (toframe == CFRA))
										toframe= ce->cfra;
								}
							}
						}
						BLI_freelistN(&elems);
					}
				}
			}
		}
		
		a= float_to_frame(toframe);
		
		if (a!=CFRA) {
			CFRA= a;
			update_for_newframe();
		}	
		
		BIF_undo_push("Next/Prev Key");
		allqueue(REDRAWALL, 0);
	}
}

void timeline_frame_to_center(void)
{
	float dtime;
	
	dtime= CFRA*(G.scene->r.framelen) - (G.v2d->cur.xmin + G.v2d->cur.xmax)/2.0; 
	G.v2d->cur.xmin += dtime;
	G.v2d->cur.xmax += dtime;
	scrarea_queue_winredraw(curarea);
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
		
		if( uiDoBlocks(&sa->uiblocks, event)!=UI_NOTHING ) event= 0;

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
				
				cfra = (int)dx;
				if(cfra< MINFRAME) cfra= MINFRAME;
				
				if( cfra!=CFRA || first )
				{
					first= 0;
					CFRA= cfra;
					update_for_newframe();
					force_draw_plus(SPACE_VIEW3D, 1);
				}
				else PIL_sleep_ms(30);
			
			} while(get_mbut() & mousebut);
			
			stime->flag &= ~TIME_CFRA_NUM;
			
			doredraw= 1;
			break;
			
		case RIGHTMOUSE: /* select/deselect marker */
			getmouseco_areawin(mval);
			areamouseco_to_ipoco(G.v2d, mval, &dx, &dy);

			cfra= find_nearest_marker(dx);

			if(cfra < MINFRAME) cfra= MINFRAME;

			if (G.qual && LR_SHIFTKEY)
				select_timeline_marker_frame(cfra, 1);
			else
				select_timeline_marker_frame(cfra, 0);

			doredraw= 1;

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

			doredraw= 1;
			break;
		case PADMINUS:
			dx= (float)(0.15*(G.v2d->cur.xmax-G.v2d->cur.xmin));
			G.v2d->cur.xmin-= dx;
			G.v2d->cur.xmax+= dx;
			test_view2d(G.v2d, sa->winx, sa->winy);

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
				nextprev_timeline_marker(1);
			break;
		case PAGEDOWNKEY: /* prev keyframe */
			if(G.qual==LR_CTRLKEY)
				nextprev_timeline_key(-1);
			else
				nextprev_timeline_marker(-1);
			break;
			
		case AKEY:
			/* deselect all TimeMarkers */
			select_timeline_markers();
			doredraw= 1;
			break;
		case CKEY:
			timeline_frame_to_center();
			break;
		case GKEY: /* move marker ... not yet implemented */
			break;
		case EKEY: /* set end frame */
			G.scene->r.efra = CFRA;
			allqueue(REDRAWBUTSALL, 0);
			allqueue(REDRAWTIME, 1);
			break;
		case MKEY: /* add, rename marker */
			if (G.qual & LR_CTRLKEY)
				rename_timeline_marker();
			else
				add_timeline_marker(CFRA);
			allqueue(REDRAWTIME, 0);
			break;
		case SKEY: /* set start frame */
			G.scene->r.sfra = CFRA;
			allqueue(REDRAWBUTSALL, 0);
			allqueue(REDRAWTIME, 1);
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

			remove_timeline_marker();
			allqueue(REDRAWTIME, 0);
			break;
		}
	}

	if(doredraw)
		scrarea_queue_winredraw(sa);
}


