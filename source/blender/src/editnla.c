/**
* $Id$
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
* This file is a horrible mess: An attmept to cram some
* final functionality into blender before it is too late.
*
* Hopefully it can be tidied up at a later date...
*/

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "PIL_time.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_library.h"
#include "BKE_nla.h"
#include "BKE_action.h"

#include "BIF_screen.h"
#include "BIF_interface.h"
#include "BIF_buttons.h"
#include "BIF_space.h"
#include "BIF_mywindow.h"
#include "BIF_editview.h"
#include "BIF_toolbox.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_ipo_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_userdef_types.h"
#include "DNA_action_types.h"
#include "DNA_nla_types.h"
#include "DNA_constraint_types.h"

#include "BSE_editipo.h"
#include "BSE_editnla_types.h"
#include "BSE_headerbuttons.h"
#include "BSE_drawipo.h"
#include "BSE_trans_types.h"
#include "BSE_edit.h"
#include "BSE_filesel.h"
#include "BDR_editobject.h"

#include "interface.h"
#include "blendef.h"
#include "mydevice.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* Note: A lot of these pretty much duplicate the behaviour of the
action windows.  The functions should be shared, not copy-pasted */

static void deselect_nlachannel_keys (int test);
static void deselect_nlachannels(int test);
static void transform_nlachannel_keys(char mode);	
static void delete_nlachannel_keys(void);
static void delete_nlachannels(void);
static void duplicate_nlachannel_keys(void);
static void borderselect_nla(void);
static void mouse_nla(int selectmode);
static Base *get_nearest_nlachannel_ob_key (float *index, short *sel);
static bAction *get_nearest_nlachannel_ac_key (float *index, short *sel);
static Base *get_nearest_nlastrip (bActionStrip **rstrip, short *sel);

static void mouse_nlachannels(short mval[2]);
static void add_nlablock(short mval[2]);
static bActionStrip *get_active_nlastrip(void);
static void convert_nla(short mval[2]);

extern int count_nla_levels(void);	/* From drawnla.c */
extern int nla_filter (Base* base, int flags);	/* From drawnla.c */

/* ******************** SPACE: NLA ********************** */

void winqreadnlaspace(ScrArea *sa, void *spacedata, BWinEvent *evt)
{
	unsigned short event= evt->event;
	short val= evt->val;
	char ascii= evt->ascii;
	SpaceNla *snla = curarea->spacedata.first;
	int doredraw= 0;
	short	mval[2];
	float dx,dy;
	int	cfra;
	
	if (curarea->win==0) return;
	if (!snla) return;
	
	if(val) {
		if( uiDoBlocks(&curarea->uiblocks, event)!=UI_NOTHING ) event= 0;
		
		getmouseco_areawin(mval);
		
		switch(event) {
		case UI_BUT_EVENT:
			do_blenderbuttons(val);
			break;
		case HOMEKEY:
			do_nla_buttons(B_NLAHOME);
			break;
		case DKEY:
			if (G.qual & LR_SHIFTKEY && mval[0]>=NLAWIDTH){
				duplicate_nlachannel_keys();
				update_for_newframe();
			}
			break;
		case DELKEY:
		case XKEY:
			if (mval[0]>=NLAWIDTH)
				delete_nlachannel_keys ();
			else
				delete_nlachannels();
			update_for_newframe();
			break;
		case GKEY:
			if (mval[0]>=NLAWIDTH)
				transform_nlachannel_keys ('g');
			update_for_newframe();
			break;
		case SKEY:
			if (mval[0]>=NLAWIDTH)
				transform_nlachannel_keys ('s');
			update_for_newframe();
			break;
		case BKEY:
			borderselect_nla();
			break;
		case CKEY:
			convert_nla(mval);
			break;
			
		case AKEY:
			if (G.qual & LR_SHIFTKEY){
				add_nlablock(mval);
				allqueue (REDRAWNLA, 0);
				allqueue (REDRAWVIEW3D, 0);
			}
			else{
				if (mval[0]>=NLAWIDTH)
					deselect_nlachannel_keys(1);
				else{
					deselect_nlachannels(1);
					allqueue (REDRAWVIEW3D, 0);
				}
				allqueue (REDRAWNLA, 0);
				allqueue (REDRAWIPO, 0);
			}
			break;
		case RIGHTMOUSE:
			if (mval[0]>=NLAWIDTH) {
				if(G.qual & LR_SHIFTKEY)
					mouse_nla(SELECT_INVERT);
				else
					mouse_nla(SELECT_REPLACE);
			}
			else
				mouse_nlachannels(mval);
			break;
		case LEFTMOUSE:
			if (mval[0]>NLAWIDTH){
				do {
					getmouseco_areawin(mval);
					
					areamouseco_to_ipoco(G.v2d, mval, &dx, &dy);
					
					cfra= (int)dx;
					if(cfra< 1) cfra= 1;
					
					if( cfra!=CFRA ) {
						CFRA= cfra;
						update_for_newframe();
						force_draw_plus(SPACE_VIEW3D);
						force_draw_plus(SPACE_IPO);
					}
					
				} while(get_mbut()&L_MOUSE);
			}
			
			break;
		case MIDDLEMOUSE:
		case WHEELUPMOUSE:
		case WHEELDOWNMOUSE:
			view2dmove(event);	/* in drawipo.c */
			break;
		}
	}
	
	if(doredraw) scrarea_queue_winredraw(curarea);
}

static void convert_nla(short mval[2])
{
	short event;
	float	ymax, ymin;
	Base *base;
	float x,y;
	int sel=0;
	bActionStrip *strip, *nstrip;
	/* Find out what strip we're over */
	ymax = count_nla_levels() * (NLACHANNELSKIP+NLACHANNELHEIGHT);
	areamouseco_to_ipoco(G.v2d, mval, &x, &y);
	
	for (base=G.scene->base.first; base; base=base->next){
		if (nla_filter(base, 0)){
			/* Check object ipo */
			ymin=ymax-(NLACHANNELSKIP+NLACHANNELHEIGHT);
			if (y>=ymin && y<=ymax)
				break;
			ymax=ymin;
			
			if (base->object->type==OB_ARMATURE){
				/* Check action ipo */
				ymin=ymax-(NLACHANNELSKIP+NLACHANNELHEIGHT);
				if (y>=ymin && y<=ymax)
					break;
				ymax=ymin;
				
				/* Check nlastrips */
				for (strip=base->object->nlastrips.first; strip; strip=strip->next){
					ymin=ymax-(NLACHANNELSKIP+NLACHANNELHEIGHT);
					if (y>=ymin && y<=ymax){
						sel = 1;
						break;
					}
					ymax=ymin;
				}
				if (sel)
					break;
			}
		}
	}
	
	if (!base)
		return;
	
	if (base->object->type==OB_ARMATURE){
		event = pupmenu("Convert%t|Action to NLAstrip%x1");
		switch (event){
		case 1:
			if (base->object->action){
				/* Make new actionstrip */
				nstrip = MEM_callocN(sizeof(bActionStrip), "bActionStrip");
				
				deselect_nlachannel_keys(0);
				
				/* Link the action to the nstrip */
				nstrip->act = base->object->action;
				nstrip->actstart = calc_action_start(base->object->action);	/* MAKE THIS THE FIRST FRAME OF THE ACTION */
				nstrip->actend = calc_action_end(base->object->action);
				nstrip->start = nstrip->actstart;
				nstrip->end = nstrip->actend;
				nstrip->flag = ACTSTRIP_SELECT;
				nstrip->repeat = 1.0;
								
				BLI_addtail(&base->object->nlastrips, nstrip);
				
				/* Unlink action */
				base->object->action = NULL;
				
				allqueue (REDRAWNLA, 0);
			}
			
			
			break;
		default:
			break;
		}
	}
}

static Base *nla_base=NULL;	/* global, bad, bad! put it in nla space later, or recode the 2 functions below (ton) */

static void add_nla_block(int val)
{
	/* val is not used, databrowse needs it to optional pass an event */
	bAction *act=NULL;
	bActionStrip *strip;
	int		cur;
	short event;
	
	if(nla_base==NULL) return;
	
	event= G.snla->menunr;	/* set by databrowse or pupmenu */
	
	if (event!=-1){
		for (cur = 1, act=G.main->action.first; act; act=act->id.next, cur++){
			if (cur==event){
				break;
			}
		}
	}
	
	/* Bail out if no action was chosen */
	if (!act){
		return;
	}
	
	/* Initialize the new action block */
	strip = MEM_callocN(sizeof(bActionStrip), "bActionStrip");
	
	deselect_nlachannel_keys(0);
	
	/* Link the action to the strip */
	strip->act = act;
	strip->actstart = 1.0;
	strip->actend = calc_action_end(act);
	strip->start = G.scene->r.cfra; /* Should be mval[0] */
	strip->end = strip->start + (strip->actend-strip->actstart);
	strip->flag = ACTSTRIP_SELECT;
	strip->repeat = 1.0;
	
	act->id.us++;
	
	BLI_addtail(&nla_base->object->nlastrips, strip);

}

static void add_nlablock(short mval[2])
{
	/* Make sure we are over an armature */
	Base *base;
	float ymin, ymax;
	float x, y;
	rctf	rectf;
	short event;
	char *str;
	short nr;
	
	areamouseco_to_ipoco(G.v2d, mval, &x, &y);
	
	mval[0]-=7;
	areamouseco_to_ipoco(G.v2d, mval, &rectf.xmin, &rectf.ymin);
	
	mval[0]+=14;
	areamouseco_to_ipoco(G.v2d, mval, &rectf.xmax, &rectf.ymax);
	
	ymax = count_nla_levels();	
	ymax*=(NLACHANNELHEIGHT + NLACHANNELSKIP);
	
	for (base=G.scene->base.first; base; base=base->next){
		/* Handle object ipo selection */
		if (nla_filter(base, 0)){
			
			/* STUPID STUPID STUPID */
			ymin=ymax-(NLACHANNELHEIGHT+NLACHANNELSKIP);
			
			/* Handle object ipos */
			if (base->object->type==OB_ARMATURE){
				if (!((ymax < rectf.ymin) || (ymin > rectf.ymax)))
					break;		
			}
			
			ymax=ymin;
			
			/* Handle action ipos & Action strips */
			if (base->object->type==OB_ARMATURE){
				ymin=ymax-(NLACHANNELHEIGHT+NLACHANNELSKIP)*(BLI_countlist(&base->object->nlastrips) + 1);
				if (!((ymax < rectf.ymin) || (ymin > rectf.ymax)))
					break;
				ymax=ymin;
				
				
			}			
		}
	}	
	
	/* global... for the call above, because the NLA system seems not to have an 'active strip' stored */
	nla_base= base;
	
	/* Make sure we have an armature */
	if (!base){
		error ("Not an armature!");
		return;
	}
	
	/* Popup action menu */
	IDnames_to_pupstring(&str, "Add action", NULL, &G.main->action, (ID *)G.scene, &nr);
	
	if(strncmp(str+13, "DataBrow", 8)==0) {
		MEM_freeN(str);

		activate_databrowse((ID *)NULL, ID_AC, 0, 0, &G.snla->menunr, add_nla_block );
		
		return;			
	}
	else {
		event = pupmenu(str);
		MEM_freeN(str);
	}
	
	/* this is a callback for databrowse too */
	add_nla_block(0);
	
}

static void mouse_nlachannels(short mval[2])
{
	/* Find which strip has been clicked */
//	bActionChannel *chan;
	bConstraintChannel *conchan=NULL;
	bActionStrip *strip;
	float	click;
	int		wsize;
	int		sel;
	Base	*base;
	
	wsize = (count_nla_levels ()*(NLACHANNELHEIGHT+NLACHANNELSKIP));


	click = (wsize-(mval[1]+G.v2d->cur.ymin));
	click += NLACHANNELHEIGHT/2;
	click /= (NLACHANNELHEIGHT+NLACHANNELSKIP);

	if (click<0)
		return;

	for (base = G.scene->base.first; base; base=base->next){
		if (nla_filter(base, 0)){
			/* See if this is a base selected */
			if ((int)click==0)
				break;
			
			click--;
			
			/* Check for click in a constraint */
			for (conchan=base->object->constraintChannels.first; conchan; conchan=conchan->next){
				if ((int)click==0){
					base=G.scene->base.last;
					break;
				}
				click--;
			}

			/* See if this is an action */
			if (base->object->type==OB_ARMATURE && base->object->action){
				if ((int)click==0){
					break;
				}
				click--;
			}

			/* See if this is an nla strip */
			for (strip = base->object->nlastrips.first; strip; strip=strip->next){
				if ((int)click==0){
					base=G.scene->base.last;
					break;
				}
				click--;				
			}
		}
	}

	if (!base && !conchan)
		return;

	/* Handle constraint strip selection */
	if (conchan){
		if (conchan->flag & CONSTRAINT_CHANNEL_SELECT)
			sel = 0;
		else
			sel =1;
		
		/* Channel names clicking */
		if (G.qual & LR_SHIFTKEY){
			//		select_poseelement_by_name(chan->name, !(chan->flag & ACHAN_SELECTED));
			if (conchan->flag & CONSTRAINT_CHANNEL_SELECT){
				conchan->flag &= ~CONSTRAINT_CHANNEL_SELECT;
				//	hilight_channel(act, chan, 0);
			}
			else{
				conchan->flag |= CONSTRAINT_CHANNEL_SELECT;
				//	hilight_channel(act, chan, 1);
			}
		}
		else{
			deselect_nlachannels (0);	// Auto clear
			conchan->flag |= CONSTRAINT_CHANNEL_SELECT;
			//	hilight_channel(act, chan, 1);
			//	act->achan = chan;
			//	select_poseelement_by_name(chan->name, 1);
		}
		
	}
	
	/* Handle object strip selection */
	else if (base)
	{
		/* Choose the mode */
		if (base->flag & SELECT)
			sel = 0;
		else
			sel =1;
		
		/* Channel names clicking */
		if (G.qual & LR_SHIFTKEY){
	//		select_poseelement_by_name(chan->name, !(chan->flag & ACHAN_SELECTED));
			if (base->flag & SELECT){
				base->flag &= ~SELECT;
		//		hilight_channel(act, chan, 0);
			}
			else{
				base->flag |= SELECT;
		//		hilight_channel(act, chan, 1);
			}
		}
		else{
			deselect_nlachannels (0);	// Auto clear
			base->flag |= SELECT;
		//	hilight_channel(act, chan, 1);
		//	act->achan = chan;
		//	select_poseelement_by_name(chan->name, 1);
		}
		
	}
	allqueue (REDRAWIPO, 0);
	allqueue (REDRAWVIEW3D, 0);
	allqueue (REDRAWACTION, 0);
	allqueue(REDRAWNLA, 0);
	
}

void init_nlaspace(ScrArea *sa)
{
	SpaceNla *snla;
	
	snla= MEM_callocN(sizeof(SpaceNla), "initnlaspace");
	BLI_addhead(&sa->spacedata, snla);
	
	snla->spacetype= SPACE_NLA;
	
	snla->v2d.tot.xmin= 1.0;
	snla->v2d.tot.ymin=	0.0;
	snla->v2d.tot.xmax= 1000.0;
	snla->v2d.tot.ymax= 1000.0;
	
	snla->v2d.cur.xmin= -5.0;
	snla->v2d.cur.ymin= 0.0;
	snla->v2d.cur.xmax= 65.0;
	snla->v2d.cur.ymax= 1000.0;
	
	snla->v2d.min[0]= 0.0;
	snla->v2d.min[1]= 0.0;
	
	snla->v2d.max[0]= 1000.0;
	snla->v2d.max[1]= 1000.0;
	
	snla->v2d.minzoom= 0.1F;
	snla->v2d.maxzoom= 10;
	
	snla->v2d.scroll= R_SCROLL+B_SCROLL;
	snla->v2d.keepaspect= 0;
	snla->v2d.keepzoom= V2D_LOCKZOOM_Y;
	snla->v2d.keeptot= 0;
	
	snla->lock = 0;
};

static void deselect_nlachannel_keys (int test)
{
	Base			*base;
	int				sel=1;
	bActionChannel	*chan;
	bActionStrip	*strip;
	bConstraintChannel *conchan;
	
	/* Determine if this is selection or deselection */
	if (test){
		for (base=G.scene->base.first; base && sel; base=base->next){
			
			/* Test object ipos */
			if (is_ipo_key_selected(base->object->ipo)){
				sel = 0;
				break;
			}
			
			/* Test object constraint ipos */
			if (sel){
				for (conchan=base->object->constraintChannels.first; conchan; conchan=conchan->next){
					if (is_ipo_key_selected(conchan->ipo)){
						sel=0;
						break;
					}
				}
			}
			
			/* Test action ipos */
			if (sel){
				if (base->object->type==OB_ARMATURE && base->object->action){
					for (chan=base->object->action->chanbase.first; chan; chan=chan->next){
						if (is_ipo_key_selected(chan->ipo)){
							sel=0;
							break;
						}

						/* Test action constraints */
						if (sel){
							for (conchan=chan->constraintChannels.first; conchan; conchan=conchan->next){
								if (is_ipo_key_selected(conchan->ipo)){
									sel=0;
									break;
								}
							}
						}
					}
				}
			}
			
			/* Test NLA strips */
			if (sel){
				if (base->object->type==OB_ARMATURE){
					for (strip=base->object->nlastrips.first; strip; strip=strip->next){
						if (strip->flag & ACTSTRIP_SELECT){
							sel = 0;
							break;
						}
					}
				}
			}
		}
	}
	else
		sel=0;
	
	
	/* Set the flags */
	for (base=G.scene->base.first; base; base=base->next){
		/* Set the object ipos */
		set_ipo_key_selection(base->object->ipo, sel);

		
		/* Set the object constraint ipos */
		for (conchan=base->object->constraintChannels.first; conchan; conchan=conchan->next){
			set_ipo_key_selection(conchan->ipo, sel);			
		}

		/* Set the action ipos */
		if (base->object->type==OB_ARMATURE && base->object->action){
			for (chan=base->object->action->chanbase.first; chan; chan=chan->next){
				set_ipo_key_selection(chan->ipo, sel);
				/* Set the action constraint ipos */
				for (conchan=chan->constraintChannels.first; conchan; conchan=conchan->next)
					set_ipo_key_selection(conchan->ipo, sel);
			}
		}
		
		/* Set the nlastrips */
		if (base->object->type==OB_ARMATURE){
			for (strip=base->object->nlastrips.first; strip; strip=strip->next){
				if (sel)
					strip->flag |= ACTSTRIP_SELECT;
				else
					strip->flag &= ~ACTSTRIP_SELECT;
			}
		}
	}
}


static void transform_nlachannel_keys(char mode)
{
	Base *base;
	TransVert *tv;
	int /*sel=0,*/  i;
	short	mvals[2], mvalc[2];
	//	short	 cent[2];
	float	sval[2], cval[2], lastcval[2];
	short	cancel=0;
	float	fac=0.0F;
	int		loop=1;
	int		tvtot=0;
	float	deltax, startx;
	//	float	cenf[2];
	int		invert=0, firsttime=1;
	char	str[256];
	bActionChannel *chan;
	bActionStrip *strip;
	bConstraintChannel *conchan;

	/* Ensure that partial selections result in beztriple selections */
	for (base=G.scene->base.first; base; base=base->next){

		/* Check object ipos */
		tvtot+=fullselect_ipo_keys(base->object->ipo);
		
		/* Check object constraint ipos */
		for(conchan=base->object->constraintChannels.first; conchan; conchan=conchan->next)
			tvtot+=fullselect_ipo_keys(conchan->ipo);			
		
		/* Check action ipos */
		if (base->object->type == OB_ARMATURE && base->object->action){
			for (chan=base->object->action->chanbase.first; chan; chan=chan->next){
				tvtot+=fullselect_ipo_keys(chan->ipo);
				
				/* Check action constraint ipos */
				for (conchan=chan->constraintChannels.first; conchan; conchan=conchan->next)
					tvtot+=fullselect_ipo_keys(conchan->ipo);
			}
		
		}

		/* Check nlastrips */
		if (base->object->type==OB_ARMATURE){
			for (strip=base->object->nlastrips.first; strip; strip=strip->next){
				if (strip->flag & ACTSTRIP_SELECT)
					tvtot+=2;
			}
		}
	}
	
	/* If nothing is selected, bail out */
	if (!tvtot)
		return;
	
	
	/* Build the transvert structure */
	tv = MEM_callocN (sizeof(TransVert) * tvtot, "transVert");
	tvtot=0;
	for (base=G.scene->base.first; base; base=base->next){
		/* Manipulate object ipos */
		tvtot=add_trans_ipo_keys(base->object->ipo, tv, tvtot);

		/* Manipulate object constraint ipos */
		for (conchan=base->object->constraintChannels.first; conchan; conchan=conchan->next)
			tvtot=add_trans_ipo_keys(conchan->ipo, tv, tvtot);

		/* Manipulate action ipos */
		if (base->object->type==OB_ARMATURE && base->object->action){
			for (chan=base->object->action->chanbase.first; chan; chan=chan->next){
				tvtot=add_trans_ipo_keys(chan->ipo, tv, tvtot);

				/* Manipulate action constraint ipos */
				for (conchan=chan->constraintChannels.first; conchan; conchan=conchan->next)
					tvtot=add_trans_ipo_keys(conchan->ipo, tv, tvtot);
			}
		}

		/* Manipulate nlastrips */
		for (strip=base->object->nlastrips.first; strip; strip=strip->next){
			if (strip->flag & ACTSTRIP_SELECT){
				tv[tvtot+0].val=&strip->start;
				tv[tvtot+1].val=&strip->end;
				
				tv[tvtot+0].oldval = strip->start;
				tv[tvtot+1].oldval = strip->end;
				
				tvtot+=2;
			}
		}
	}
	
	/* Do the event loop */
	//	cent[0] = curarea->winx + (G.snla->v2d.hor.xmax)/2;
	//	cent[1] = curarea->winy + (G.snla->v2d.hor.ymax)/2;
	
	//	areamouseco_to_ipoco(cent, &cenf[0], &cenf[1]);
	
	getmouseco_areawin (mvals);
	areamouseco_to_ipoco(G.v2d, mvals, &sval[0], &sval[1]);
	
	startx=sval[0];
	while (loop) {
		/*		Get the input */
		/*		If we're cancelling, reset transformations */
		/*			Else calc new transformation */
		/*		Perform the transformations */
		while (qtest()) {
			short val;
			unsigned short event= extern_qread(&val);
			
			if (val) {
				switch (event) {
				case LEFTMOUSE:
				case SPACEKEY:
				case RETKEY:
					loop=0;
					break;
				case XKEY:
					break;
				case ESCKEY:
				case RIGHTMOUSE:
					cancel=1;
					loop=0;
					break;
				default:
					arrows_move_cursor(event);
					break;
				};
			}
		}
		
		if (cancel) {
			for (i=0; i<tvtot; i++) {
				if (tv[i].loc){
					tv[i].loc[0]=tv[i].oldloc[0];
					tv[i].loc[1]=tv[i].oldloc[1];
				}
				if (tv[i].val)
					tv[i].val[0]=tv[i].oldval;
			}
		}
		else {
			getmouseco_areawin (mvalc);
			areamouseco_to_ipoco(G.v2d, mvalc, &cval[0], &cval[1]);
			
			if (!firsttime && lastcval[0]==cval[0] && lastcval[1]==cval[1]) {
				PIL_sleep_ms(1);
			}
			else {
				for (i=0; i<tvtot; i++){
					if (tv[i].loc)
						tv[i].loc[0]=tv[i].oldloc[0];
					if (tv[i].val)
						tv[i].val[0]=tv[i].oldval;
					
					switch (mode){
					case 'g':
						deltax = cval[0]-sval[0];
						fac= deltax;
						
						apply_keyb_grid(&fac, 0.0F, 1.0F, 0.1F, U.flag & AUTOGRABGRID);
						
						if (tv[i].loc)
							tv[i].loc[0]+=fac;
						if (tv[i].val)
							tv[i].val[0]+=fac;
						break;
					case 's': 
						startx=mvals[0]-(NLAWIDTH/2+(curarea->winrct.xmax-curarea->winrct.xmin)/2);
						deltax=mvalc[0]-(NLAWIDTH/2+(curarea->winrct.xmax-curarea->winrct.xmin)/2);
						fac= (float)fabs(deltax/startx);
						
						apply_keyb_grid(&fac, 0.0F, 0.2F, 0.1F, U.flag & AUTOSIZEGRID);
						
						if (invert){
							if (i % 03 == 0){
								memcpy (tv[i].loc, tv[i].oldloc, sizeof(tv[i+2].oldloc));
							}
							if (i % 03 == 2){
								memcpy (tv[i].loc, tv[i].oldloc, sizeof(tv[i-2].oldloc));
							}
							
							fac*=-1;
						}
						startx= (G.scene->r.cfra);
						
						if (tv[i].loc){
							tv[i].loc[0]-= startx;
							tv[i].loc[0]*=fac;
							tv[i].loc[0]+= startx;
						}
						if (tv[i].val){
							tv[i].val[0]-= startx;
							tv[i].val[0]*=fac;
							tv[i].val[0]+= startx;
						}
						
						break;
					}
				}
			}
			
			if (mode=='s'){
				sprintf(str, "sizeX: %.3f", fac);
				headerprint(str);
			}
			else if (mode=='g'){
				sprintf(str, "deltaX: %.3f", fac);
				headerprint(str);
			}
			
			if (G.snla->lock){
				allqueue (REDRAWVIEW3D, 0);
				allqueue (REDRAWNLA, 0);
				allqueue (REDRAWIPO, 0);
				force_draw_all();
			}
			else {
				addqueue (curarea->win, REDRAWALL, 0);
				force_draw ();
			}
		}
		
		lastcval[0]= cval[0];
		lastcval[1]= cval[1];
		firsttime= 0;
	}
	
	allspace(REMAKEALLIPO, 0);
	allqueue (REDRAWVIEW3D, 0);
	allqueue (REDRAWNLA, 0);
	allqueue (REDRAWIPO, 0);
	MEM_freeN (tv);
}

static void delete_nlachannel_keys(void)
{
	Base *base;
	bActionChannel *chan;
	bConstraintChannel *conchan;
	bActionStrip *strip, *nextstrip;
	
	if (!okee("Erase selected keys"))
		return;
	
	for (base = G.scene->base.first; base; base=base->next){

		/* Delete object ipos */
		delete_ipo_keys(base->object->ipo);
		
		/* Delete object constraint keys */
		for(conchan=base->object->constraintChannels.first; conchan; conchan=conchan->next)
			delete_ipo_keys(conchan->ipo);

		/* Delete NLA strips */
		if (base->object->type==OB_ARMATURE){
			for (strip = base->object->nlastrips.first; strip; strip=nextstrip){
				nextstrip=strip->next;
				if (strip->flag & ACTSTRIP_SELECT){
					free_actionstrip(strip);
					BLI_remlink(&base->object->nlastrips, strip);
					MEM_freeN(strip);
				}
			}
		}
		
		/* Delete action ipos */
		if (base->object->type==OB_ARMATURE && base->object->action){
			for (chan=base->object->action->chanbase.first; chan; chan=chan->next){
				delete_ipo_keys(chan->ipo);
				/* Delete action constraint keys */
				for(conchan=chan->constraintChannels.first; conchan; conchan=conchan->next)
					delete_ipo_keys(conchan->ipo);
			}
		}
	}
	
	allspace(REMAKEALLIPO, 0);
        allspace(REMAKEIPO,0);
	allqueue (REDRAWVIEW3D, 0);
	allqueue(REDRAWNLA, 0);
	allqueue(REDRAWIPO, 0);
}

static void duplicate_nlachannel_keys(void)
{
	Base *base;
	bActionChannel *chan;
	bConstraintChannel *conchan;
	bActionStrip *strip, *laststrip;
	
	/* Find selected items */
	for (base = G.scene->base.first; base; base=base->next){
		/* Duplicate object keys */
		duplicate_ipo_keys(base->object->ipo);
		
		/* Duplicate object constraint keys */
		for(conchan=base->object->constraintChannels.first; conchan; conchan=conchan->next)
			duplicate_ipo_keys(conchan->ipo);

		/* Duplicate nla strips */
		if (base->object->type == OB_ARMATURE){
			laststrip = base->object->nlastrips.last;
			for (strip=base->object->nlastrips.first; strip; strip=strip->next){
				if (strip->flag & ACTSTRIP_SELECT){
					bActionStrip *newstrip;
					
					copy_actionstrip(&newstrip, &strip);
					
					BLI_addtail(&base->object->nlastrips, newstrip);
					
					strip->flag &= ~ACTSTRIP_SELECT;
					newstrip->flag |= ACTSTRIP_SELECT;
					
				}
				if (strip==laststrip)
					break;
			}
		}
		
		/* Duplicate actionchannel keys */
		if (base->object->type == OB_ARMATURE && base->object->action){
			for (chan=base->object->action->chanbase.first; chan; chan=chan->next){
				duplicate_ipo_keys(chan->ipo);
				/* Duplicate action constraint keys */
				for(conchan=chan->constraintChannels.first; conchan; conchan=conchan->next)
					duplicate_ipo_keys(conchan->ipo);
			}
		}
	}
	
	transform_nlachannel_keys ('g');
}

static void borderselect_nla(void)
{ 
	Base *base;
	rcti rect;
	rctf rectf;
	int  val, selectmode;
	short	mval[2];
	float	ymin, ymax;
	bActionStrip *strip;
	bConstraintChannel *conchan;
	
	if ( (val = get_border (&rect, 3)) ){
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
		
		ymax = count_nla_levels();
		ymax*= (NLACHANNELHEIGHT+NLACHANNELSKIP);
		
		for (base=G.scene->base.first; base; base=base->next){
			/* Check object ipos */
			if (nla_filter(base, 0)){
				ymin=ymax-(NLACHANNELHEIGHT+NLACHANNELSKIP);
				if (base->object->ipo){
					if (!((ymax < rectf.ymin) || (ymin > rectf.ymax)))
						borderselect_ipo_key(base->object->ipo, rectf.xmin, rectf.xmax,
                                 selectmode);
				}
				ymax=ymin;

				/* Check object constraint ipos */
				for (conchan=base->object->constraintChannels.first; conchan; conchan=conchan->next){
					ymin=ymax-(NLACHANNELHEIGHT+NLACHANNELSKIP);
					if (!((ymax < rectf.ymin) || (ymin > rectf.ymax)))
						borderselect_ipo_key(conchan->ipo, rectf.xmin, rectf.xmax,
                                 selectmode);
					ymax=ymin;
				}

				/* Check action ipos */
				if (ACTIVE_ARMATURE(base)){
					ymin=ymax-(NLACHANNELHEIGHT+NLACHANNELSKIP);
					if (base->object->action){
						bActionChannel *chan;
						
						if (!((ymax < rectf.ymin) || (ymin > rectf.ymax))){
							for (chan=base->object->action->chanbase.first; chan; chan=chan->next){
								borderselect_ipo_key(chan->ipo, rectf.xmin, rectf.xmax,
                                     selectmode);
								/* Check action constraint ipos */
								for (conchan=chan->constraintChannels.first; conchan; conchan=conchan->next)
									borderselect_ipo_key(conchan->ipo, rectf.xmin, rectf.xmax,
                                       selectmode);
							}
						}
					}
					ymax=ymin;
				}	/* End of if armature */
				
				/* Skip nlastrips */
				if (base->object->type==OB_ARMATURE){
					for (strip=base->object->nlastrips.first; strip; strip=strip->next){
						ymin=ymax-(NLACHANNELHEIGHT+NLACHANNELSKIP);
						//
						if (!((ymax < rectf.ymin) || (ymin > rectf.ymax))){
							if (!((rectf.xmax<strip->start) || (rectf.xmin>strip->end))){
								if (val==1)
									strip->flag |= ACTSTRIP_SELECT;
								else
									strip->flag &= ~ACTSTRIP_SELECT;
							}
						}
						
						ymax=ymin;
					}
				}
				
			}	/* End of object filter */
		}	
		allqueue(REDRAWNLA, 0);
		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWIPO, 0);
	}
}

static void mouse_nla(int selectmode)
{
	short sel;
	float	selx;
	short	mval[2];
	Base *base;
	bAction *act;
	bActionChannel *chan;
	bActionStrip *rstrip;
	bConstraintChannel *conchan;
	
	getmouseco_areawin (mval);
	
	/* Try object ipo selection */
	base=get_nearest_nlachannel_ob_key(&selx, &sel);
	if (base){
		if (selectmode == SELECT_REPLACE){
			deselect_nlachannel_keys(0);
			selectmode = SELECT_ADD;
		}
		
		select_ipo_key(base->object->ipo, selx, selectmode);
		
		/* Try object constraint selection */
		for (conchan=base->object->constraintChannels.first; conchan; conchan=conchan->next)
			select_ipo_key(conchan->ipo, selx, selectmode);
		
		
		allqueue(REDRAWIPO, 0);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWNLA, 0);
		return;
	}

	/* Try action ipo selection */
	act=get_nearest_nlachannel_ac_key(&selx, &sel);
	if (act){
		if (selectmode == SELECT_REPLACE){
			deselect_nlachannel_keys(0);
			selectmode = SELECT_ADD;
		}
		
		for (chan=act->chanbase.first; chan; chan=chan->next){
			select_ipo_key(chan->ipo, selx, selectmode);
			/* Try action constraint selection */
			for (conchan=chan->constraintChannels.first; conchan; conchan=conchan->next)
				select_ipo_key(conchan->ipo, selx, selectmode);
		}
		
		allqueue(REDRAWIPO, 0);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWNLA, 0);
		return;
	}
	
	/* Try nla strip selection */
	base=get_nearest_nlastrip(&rstrip, &sel);
	if (base){
		if (!(G.qual & LR_SHIFTKEY)){
			deselect_nlachannel_keys(0);
			sel = 0;
		}
		
		if (sel)
			rstrip->flag &= ~ACTSTRIP_SELECT;
		else
			rstrip->flag |= ACTSTRIP_SELECT;
		
		allqueue(REDRAWIPO, 0);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWNLA, 0);
		return;
		
	}
	
}

static Base *get_nearest_nlastrip (bActionStrip **rstrip, short *sel)
/* This function is currently more complicated than it seems like it should be.
* However, this will be needed once the nla strip timeline is more complex */
{
	Base *base, *firstbase=NULL;
	short mval[2];
	short foundsel = 0;
	rctf	rectf;
	float ymin, ymax;
	bActionStrip *strip, *firststrip, *foundstrip;
	
	getmouseco_areawin (mval);
	
	mval[0]-=7;
	areamouseco_to_ipoco(G.v2d, mval, &rectf.xmin, &rectf.ymin);
	
	mval[0]+=14;
	areamouseco_to_ipoco(G.v2d, mval, &rectf.xmax, &rectf.ymax);
	
	ymax = count_nla_levels();
	ymax*=(NLACHANNELHEIGHT + NLACHANNELSKIP);
	
	for (base = G.scene->base.first; base; base=base->next){
		if (nla_filter(base, 0)){
			/* Skip object ipos */
			//	if (base->object->ipo)
			ymax-=(NLACHANNELHEIGHT+NLACHANNELSKIP);
			
			if (base->object->type==OB_ARMATURE){
				/* Skip action ipos */
				if (base->object->action)
					ymax-=(NLACHANNELHEIGHT+NLACHANNELSKIP);
				
				for (strip=base->object->nlastrips.first; strip; strip=strip->next){
					ymin=ymax-(NLACHANNELHEIGHT+NLACHANNELSKIP);
					/* Do Ytest */
					if (!((ymax < rectf.ymin) || (ymin > rectf.ymax))){
						/* Do XTest */
						if (!((rectf.xmax<strip->start) || (rectf.xmin>strip->end))){
							if (!firstbase){
								firstbase=base;
								firststrip=strip;
								*sel = strip->flag & ACTSTRIP_SELECT;
							}
							
							if (strip->flag & ACTSTRIP_SELECT){ 
								if (!foundsel){
									foundsel=1;
									foundstrip = strip;
								}
							}
							else if (foundsel && strip != foundstrip){
								*rstrip=strip;
								*sel = 0;
								return base;
							}
						}
					}
					ymax=ymin;
				}
			}
		}
	}
	*rstrip=firststrip;
	return firstbase;
}

static Base *get_nearest_nlachannel_ob_key (float *index, short *sel)
{
	Base *base;
	IpoCurve *icu;
	Base *firstbase=NULL;
	bConstraintChannel *conchan;
	int	foundsel=0;
	float firstvert=-1, foundx=-1;
	int i;
	short mval[2];
	float ymin, ymax;
	rctf	rectf;
	
	*index=0;
	
	getmouseco_areawin (mval);
	
	mval[0]-=7;
	areamouseco_to_ipoco(G.v2d, mval, &rectf.xmin, &rectf.ymin);
	
	mval[0]+=14;
	areamouseco_to_ipoco(G.v2d, mval, &rectf.xmax, &rectf.ymax);
	
	ymax = count_nla_levels();
	
	ymax*=(NLACHANNELHEIGHT + NLACHANNELSKIP);
	
	*sel=0;
	
	for (base=G.scene->base.first; base; base=base->next){
		/* Handle object ipo selection */
		if (nla_filter(base, 0)){
			ymin=ymax-(NLACHANNELHEIGHT+NLACHANNELSKIP);
			if (base->object->ipo){
				if (!((ymax < rectf.ymin) || (ymin > rectf.ymax))){
					for (icu=base->object->ipo->curve.first; icu; icu=icu->next){
						for (i=0; i<icu->totvert; i++){
							if (icu->bezt[i].vec[1][0] > rectf.xmin && icu->bezt[i].vec[1][0] <= rectf.xmax ){
								if (!firstbase){
									firstbase=base;
									firstvert=icu->bezt[i].vec[1][0];
									*sel = icu->bezt[i].f2 & 1;	
								}
								
								if (icu->bezt[i].f2 & 1){ 
									if (!foundsel){
										foundsel=1;
										foundx = icu->bezt[i].vec[1][0];
									}
								}
								else if (foundsel && icu->bezt[i].vec[1][0] != foundx){
									*index=icu->bezt[i].vec[1][0];
									*sel = 0;
									return base;
								}
							}
						}
					}
				}
			}
		
			ymax=ymin;

			/* Handle object constraint ipos */
			for (conchan=base->object->constraintChannels.first; conchan; conchan=conchan->next){
				ymin=ymax-(NLACHANNELHEIGHT+NLACHANNELSKIP);
				if (!((ymax < rectf.ymin) || (ymin > rectf.ymax))){
					for (icu=conchan->ipo->curve.first; icu; icu=icu->next){
						for (i=0; i<icu->totvert; i++){
							if (icu->bezt[i].vec[1][0] > rectf.xmin && icu->bezt[i].vec[1][0] <= rectf.xmax ){
								if (!firstbase){
									firstbase=base;
									firstvert=icu->bezt[i].vec[1][0];
									*sel = icu->bezt[i].f2 & 1;	
								}
								
								if (icu->bezt[i].f2 & 1){ 
									if (!foundsel){
										foundsel=1;
										foundx = icu->bezt[i].vec[1][0];
									}
								}
								else if (foundsel && icu->bezt[i].vec[1][0] != foundx){
									*index=icu->bezt[i].vec[1][0];
									*sel = 0;
									return base;
								}
							}
						}
					}
				}
				ymax=ymin;
			}

			/* Skip action ipos */
			if (ACTIVE_ARMATURE(base)){
				ymax-=(NLACHANNELHEIGHT+NLACHANNELSKIP);
			}
			/* Skip nlastrips */
			if (base->object->type==OB_ARMATURE){
				ymax-=(NLACHANNELHEIGHT+NLACHANNELSKIP)*BLI_countlist(&base->object->nlastrips);
			}
		}
	}	
	
	*index=firstvert;
	return firstbase;
}

static bAction *get_nearest_nlachannel_ac_key (float *index, short *sel)
{
	Base *base;
	IpoCurve *icu;
	bAction *firstact=NULL;
	int	foundsel=0;
	float firstvert=-1, foundx=-1;
	int i;
	short mval[2];
	float ymin, ymax;
	rctf	rectf;
	bActionChannel *chan;
	bConstraintChannel *conchan;
	
	*index=0;
	
	getmouseco_areawin (mval);
	
	mval[0]-=7;
	areamouseco_to_ipoco(G.v2d, mval, &rectf.xmin, &rectf.ymin);
	
	mval[0]+=14;
	areamouseco_to_ipoco(G.v2d, mval, &rectf.xmax, &rectf.ymax);
	
	ymax = count_nla_levels();
	
	ymax*=(NLACHANNELHEIGHT + NLACHANNELSKIP);
	
	*sel=0;
	
	for (base=G.scene->base.first; base; base=base->next){
		/* Handle object ipo selection */
		if (nla_filter(base, 0)){
			/* Skip object ipo */
			ymin=ymax-(NLACHANNELHEIGHT+NLACHANNELSKIP);
			ymax=ymin;
			
			/* Handle action ipos */
			if (ACTIVE_ARMATURE(base)){
				ymin=ymax-(NLACHANNELHEIGHT+NLACHANNELSKIP);
				if (!((ymax < rectf.ymin) || (ymin > rectf.ymax))){
					for (chan=base->object->action->chanbase.first; chan; chan=chan->next){
						for (icu=chan->ipo->curve.first; icu; icu=icu->next){
							for (i=0; i<icu->totvert; i++){
								if (icu->bezt[i].vec[1][0] > rectf.xmin && icu->bezt[i].vec[1][0] <= rectf.xmax ){
									if (!firstact){
										firstact=base->object->action;
										firstvert=icu->bezt[i].vec[1][0];
										*sel = icu->bezt[i].f2 & 1;	
									}
									
									if (icu->bezt[i].f2 & 1){ 
										if (!foundsel){
											foundsel=1;
											foundx = icu->bezt[i].vec[1][0];
										}
									}
									else if (foundsel && icu->bezt[i].vec[1][0] != foundx){
										*index=icu->bezt[i].vec[1][0];
										*sel = 0;
										return base->object->action;
									}
								}
							}
						}
						
						
						for (conchan=chan->constraintChannels.first; conchan; conchan=conchan->next){
							ymin=ymax-(NLACHANNELHEIGHT+NLACHANNELSKIP);
							if (!((ymax < rectf.ymin) || (ymin > rectf.ymax))){
								for (icu=conchan->ipo->curve.first; icu; icu=icu->next){
									for (i=0; i<icu->totvert; i++){
										if (icu->bezt[i].vec[1][0] > rectf.xmin && icu->bezt[i].vec[1][0] <= rectf.xmax ){
											if (!firstact){
												firstact=base->object->action;
												firstvert=icu->bezt[i].vec[1][0];
												*sel = icu->bezt[i].f2 & 1;	
											}
											
											if (icu->bezt[i].f2 & 1){ 
												if (!foundsel){
													foundsel=1;
													foundx = icu->bezt[i].vec[1][0];
												}
											}
											else if (foundsel && icu->bezt[i].vec[1][0] != foundx){
												*index=icu->bezt[i].vec[1][0];
												*sel = 0;
												return base->object->action;
											}
										}
									}
								}
							}
							ymax=ymin;
						}
					
					
					}
				}			
				ymax=ymin;
			}
			
			/* Skip nlastrips */
			if (base->object->type==OB_ARMATURE){
				ymax-=(NLACHANNELHEIGHT+NLACHANNELSKIP)*BLI_countlist(&base->object->nlastrips);
			}
		}
	}	
	
	*index=firstvert;
	return firstact;
}

static bActionStrip *get_active_nlastrip(void)
/* For now just returns the first selected strip */
{
	Base *base;
	bActionStrip *strip;
	
	for (base=G.scene->base.first; base; base=base->next){
		if (nla_filter(base, 0) && base->object->type==OB_ARMATURE){
			for (strip=base->object->nlastrips.first; strip; strip=strip->next){
				if (strip->flag & ACTSTRIP_SELECT)
					return strip;
			}
		}
	}
	
	return NULL;
}

void clever_numbuts_nla(void){
	bActionStrip *strip;
	int but=0;
		
	/* Determine if an nla strip has been selected */
	strip = get_active_nlastrip();
	if (!strip)
		return;
	
	add_numbut(but++, LABEL, "Timeline Range:", 1.0, 18000.0, 0, 0);
	add_numbut(but++, NUM|FLO, "Strip Start:", 1.0, 18000.0, &strip->start, "First frame in the timeline");
	add_numbut(but++, NUM|FLO, "Strip End:", 1.0, 18000.0, &strip->end, "Last frame in the timeline");
	add_numbut(but++, LABEL, "Action Range:", 1.0, 18000.0, 0, 0);
	add_numbut(but++, NUM|FLO, "Action Start:", 1.0, 18000.0, &strip->actstart, "First frame of the action to map to the playrange");
	add_numbut(but++, NUM|FLO, "Action End:", 1.0, 18000.0, &strip->actend, "Last frame of the action to map to the playrange");
	add_numbut(but++, LABEL, "Blending:", 1.0, 18000.0, 0, 0);
	add_numbut(but++, NUM|FLO, "Blendin:", 0.0, 18000.0, &strip->blendin, "Number of frames of ease-in");
	add_numbut(but++, NUM|FLO, "Blendout:", 0.0, 18000.0, &strip->blendout, "Number of frames of ease-out");
	add_numbut(but++, LABEL, "Options:", 1.0, 18000.0, 0, 0);
	add_numbut(but++, NUM|FLO, "Repeat:", 0.0001, 18000.0, &strip->repeat, "Number of times the action should repeat");
	add_numbut(but++, NUM|FLO, "Stride:", 0.0001, 1000.0, &strip->stridelen, "Distance covered by one complete cycle of the action specified in the Action Range");
	{
		/* STUPID HACK BECAUSE NUMBUTS ARE BROKEN WITH MULTIPLE TOGGLES */
		short hold= (strip->flag & ACTSTRIP_HOLDLASTFRAME) ? 1 : 0;
		short frompath=(strip->flag & ACTSTRIP_USESTRIDE) ? 1 : 0;

		add_numbut(but++, TOG|SHO, "Use Path", 0, 0, &frompath, "Plays action based on position on path & stride length.  Only valid for armatures that are parented to a path");
		add_numbut(but++, TOG|SHO, "Hold", 0, 0, &hold, "Toggles whether or not to continue displaying the last frame past the end of the strip");
		add_numbut(but++, TOG|SHO, "Add", 0, 0, &strip->mode, "Toggles additive blending mode");
		
		do_clever_numbuts("Action", but, REDRAW);
		
		/* STUPID HACK BECAUSE NUMBUTS ARE BROKEN WITH MULTIPLE TOGGLES */
		if (hold) strip->flag |= ACTSTRIP_HOLDLASTFRAME;
		else strip->flag &= ~ACTSTRIP_HOLDLASTFRAME;

		if (frompath) strip->flag |= ACTSTRIP_USESTRIDE;
		else strip->flag &= ~ACTSTRIP_USESTRIDE;
		
	}

	if (strip->end<strip->start)
		strip->end=strip->start;


	if (strip->blendin>(strip->end-strip->start))
		strip->blendin = strip->end-strip->start;

	if (strip->blendout>(strip->end-strip->start))
		strip->blendout = strip->end-strip->start;

	if (strip->blendin > (strip->end-strip->start-strip->blendout))
		strip->blendin = (strip->end-strip->start-strip->blendout);

	if (strip->blendout > (strip->end-strip->start-strip->blendin))
		strip->blendout = (strip->end-strip->start-strip->blendin);
	
	
	update_for_newframe();
	allqueue (REDRAWNLA, 0);
	allqueue (REDRAWVIEW3D, 0);
}	

static void deselect_nlachannels(int test){
	int sel = 1;
	Base *base;
	bConstraintChannel *conchan;

	if (test){
		for (base=G.scene->base.first; base; base=base->next){
			/* Check base flags for previous selection */
			if (base->flag & SELECT){
				sel=0;
				break;
			}

			/* Check constraint flags for previous selection */
			for (conchan=base->object->constraintChannels.first; conchan; conchan=conchan->next){
				if (conchan->flag & CONSTRAINT_CHANNEL_SELECT){
					sel=0;
					base = G.scene->base.last;
					break;
				}
			}
		}
	}
	else
		sel = 0;

	/* Select objects */
	for (base=G.scene->base.first; base; base=base->next){
		if (sel){
			if (nla_filter(base, 0))
				base->flag |= SELECT;
		}
		else
			base->flag &= ~SELECT;

		/* Select constraint channels */
		for (conchan=base->object->constraintChannels.first; conchan; conchan=conchan->next){
			if (sel){
				if (nla_filter(base, 0))
					conchan->flag |= CONSTRAINT_CHANNEL_SELECT;
			}
			else
				conchan->flag &= ~CONSTRAINT_CHANNEL_SELECT;
		}
	}	
}

static void delete_nlachannels(void){
	Base *base;
	bConstraintChannel *conchan, *nextchan;
	int sel=0;

	/* See if there is anything selected */
	for (base = G.scene->base.first; base && (!sel); base=base->next){
		/* Check constraints */
		for (conchan=base->object->constraintChannels.first; conchan; conchan=conchan->next){
			if (conchan->flag & CONSTRAINT_CHANNEL_SELECT){
				sel = 1;
				break;
			}
		}
	}

	if (!sel)
		return;

	if (okee ("Delete selected channels")){
		for (base=G.scene->base.first; base; base=base->next){
			for (conchan=base->object->constraintChannels.first; conchan; conchan=nextchan){
				nextchan = conchan->next;
				
				if (conchan->flag & CONSTRAINT_CHANNEL_SELECT){
					/* If we're the active constraint, unlink us */
					if (conchan==base->object->activecon)
						base->object->activecon = NULL;
					
					if (conchan->ipo)
						conchan->ipo->id.us--;
					BLI_freelinkN(&base->object->constraintChannels, conchan);
				}
			}
		}
	}
}
