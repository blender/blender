/**
 * $Id$
 *
 * ***** BEGIN GPL BLOCK *****
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
 * ***** END GPL *****
 *
 * This file is a horrible mess: An attmept to cram some
 * final functionality into blender before it is too late.
 *
 * Hopefully it can be tidied up at a later date...
*/

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "PIL_time.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "DNA_action_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_ipo_types.h"
#include "DNA_object_types.h"
#include "DNA_nla_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "BKE_action.h"
#include "BKE_blender.h"
#include "BKE_depsgraph.h"
#include "BKE_group.h"
#include "BKE_global.h"
#include "BKE_ipo.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_nla.h"
#include "BKE_utildefines.h"

#include "BIF_screen.h"
#include "BIF_interface.h"
#include "BIF_butspace.h"
#include "BIF_space.h"
#include "BIF_mywindow.h"
#include "BIF_editview.h"
#include "BIF_toolbox.h"
#include "BIF_editnla.h"
#include "BIF_editaction.h"
#include "BIF_transform.h"

#include "BSE_editipo.h"
#include "BSE_editnla_types.h"
#include "BSE_headerbuttons.h"
#include "BSE_drawipo.h"
#include "BSE_editaction_types.h"
#include "BSE_trans_types.h"
#include "BSE_edit.h"
#include "BSE_filesel.h"
#include "BDR_editobject.h"
#include "BSE_drawnla.h"
#include "BSE_time.h"

#include "blendef.h"
#include "mydevice.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* Note: A lot of these pretty much duplicate the behaviour of the
action windows.  The functions should be shared, not copy-pasted */

static void mouse_nla(int selectmode);
static Base *get_nearest_nlachannel_ob_key (float *index, short *sel);
static bAction *get_nearest_nlachannel_ac_key (float *index, short *sel);
static Base *get_nearest_nlastrip (bActionStrip **rstrip, short *sel);
static void mouse_nlachannels(short mval[2]);

/* ******************** SPACE: NLA ********************** */

void shift_nlastrips_up(void) {
	
	Base *base;
	bActionStrip *strip, *prevstrip;

	for (base=G.scene->base.first; base; base=base->next) {
		if (base->object->nlaflag & OB_NLA_COLLAPSED)
			continue;
		
		for (strip = base->object->nlastrips.first; 
			 strip; strip=strip->next){
			if (strip->flag & ACTSTRIP_SELECT) {
				if ( (prevstrip = strip->prev) ) {
					if (prevstrip->prev)
						prevstrip->prev->next = strip;
					if (strip->next)
						strip->next->prev = prevstrip;
					strip->prev = prevstrip->prev;
					prevstrip->next = strip->next;
					strip->next = prevstrip;
					prevstrip->prev = strip;

					if (prevstrip == base->object->nlastrips.first)
						base->object->nlastrips.first = strip;
					if (strip == base->object->nlastrips.last)
						base->object->nlastrips.last = prevstrip;

					strip = prevstrip;
				}
				else {
					break;
				}
			}
		}
	}
	BIF_undo_push("Shift NLA strip");
	allqueue (REDRAWNLA, 0);

}

void shift_nlastrips_down(void) {
	
	Base *base;
	bActionStrip *strip, *nextstrip;

	for (base=G.scene->base.first; base; base=base->next) {
		if (base->object->nlaflag & OB_NLA_COLLAPSED)
			continue;
		
		for (strip = base->object->nlastrips.last; 
			 strip; strip=strip->prev){
			if (strip->flag & ACTSTRIP_SELECT) {
				if ( (nextstrip = strip->next) ) {
					if (nextstrip->next)
						nextstrip->next->prev = strip;
					if (strip->prev)
						strip->prev->next = nextstrip;
					strip->next = nextstrip->next;
					nextstrip->prev = strip->prev;
					strip->prev = nextstrip;
					nextstrip->next = strip;

					if (nextstrip == base->object->nlastrips.last)
						base->object->nlastrips.last = strip;
					if (strip == base->object->nlastrips.first)
						base->object->nlastrips.first = nextstrip;

					strip = nextstrip;
				}
				else {
					break;
				}
			}
		}
	}
	
	BIF_undo_push("Shift NLA strips");
	allqueue (REDRAWNLA, 0);
}

void synchronize_action_strips(void)
{
	Base *base;
	Object *ob;
	bActionStrip *strip;
	
	for (base=G.scene->base.first; base; base=base->next) {
		/* get object first */
		ob= base->object;
		
		/* step 1: adjust strip-lengths */
		//	FIXME: this seems very buggy
		for (strip = ob->nlastrips.last; strip; strip=strip->prev) {
			if (strip->flag & ACTSTRIP_LOCK_ACTION) {
				float actstart, actend;
				
				calc_action_range(strip->act, &actstart, &actend, 1);
				
				if ((strip->actstart!=actstart) || (strip->actend!=actend)) {		
					float offset = strip->scale * (actstart - strip->actstart);
					float actlen = actend - actstart;
					
					strip->start += offset;
					strip->end = (strip->scale * strip->repeat * actlen) + strip->start;
					
					strip->actstart= actstart;
					strip->actend= actend;
				}
			}
		}
		
		/* step 2: adjust blendin/out values for each strip if option is turned on */
		for (strip= ob->nlastrips.first; strip; strip=strip->next) {
			if (strip->flag & ACTSTRIP_AUTO_BLENDS) {
				bActionStrip *prev= strip->prev;
				bActionStrip *next= strip->next;
				float pr[2], nr[2];
				
				strip->blendin = 0.0f;
				strip->blendout = 0.0f;
				
				/* setup test ranges */
				if (prev && next) {
					/* init range for previous strip */
					pr[0]= prev->start;
					pr[1]= prev->end;
					
					/* init range for next strip */
					nr[0]= next->start;
					nr[1]= next->end;
				}
				else if (prev) {
					/* next strip's range is same as previous strip's range  */
					pr[0] = nr[0] = prev->start;
					pr[1] = nr[1] = prev->end;
				}
				else if (next) {
					/* previous strip's range is same as next strip's range */
					pr[0] = nr[0] = next->start;
					pr[1] = nr[1] = next->end;
				}
				else {
					/* there shouldn't be any more strips to loop through for this operation */
					break;
				}
				
				/* test various cases */
				if ( IN_RANGE(pr[1], strip->start, strip->end) && 
					(IN_RANGE(pr[0], strip->start, strip->end)==0) )
				{
					/* previous strip intersects start of current */
					
					if ( IN_RANGE(nr[1], strip->start, strip->end) && 
						(IN_RANGE(nr[0], strip->start, strip->end)==0) )
					{
						/* next strip also intersects start of current */
						if (nr[1] < pr[1])
							strip->blendin= nr[1] - strip->start;
						else
							strip->blendin= pr[1] - strip->start;
					}
					else if (IN_RANGE(nr[0], strip->start, strip->end) && 
							(IN_RANGE(nr[1], strip->start, strip->end)==0))
					{
						/* next strip intersects end of current */
						strip->blendout= strip->end - nr[0];
						strip->blendin= pr[1] - strip->start;
					}
					else {
						/* only previous strip intersects current */
						strip->blendin= pr[1] - strip->start;
					}
				}
				else if (IN_RANGE(pr[0], strip->start, strip->end) && 
						(IN_RANGE(pr[1], strip->start, strip->end)==0) )
				{
					/* previous strip intersects end of current */
					
					if ( IN_RANGE(nr[0], strip->start, strip->end) && 
						(IN_RANGE(nr[1], strip->start, strip->end)==0) )
					{
						/* next strip also intersects end of current */
						if (nr[1] > pr[1])
							strip->blendout= strip->end - nr[0];
						else
							strip->blendout= strip->end - pr[0];
					}
					else if (IN_RANGE(nr[1], strip->start, strip->end) && 
							(IN_RANGE(nr[0], strip->start, strip->end)==0))
					{
						/* next strip intersects start of current */
						strip->blendin= nr[1] - strip->start;
						strip->blendout= strip->end - pr[0];
					}
					else {
						/* only previous strip intersects current */
						strip->blendout= strip->end - pr[0];
					}
				}
				else if (IN_RANGE(nr[1], strip->start, strip->end) && 
						(IN_RANGE(nr[0], strip->start, strip->end)==0) )
				{
					/* next strip intersects start of current */
					
					if ( IN_RANGE(pr[1], strip->start, strip->end) && 
						(IN_RANGE(pr[0], strip->start, strip->end)==0) )
					{
						/* previous strip also intersects start of current */
						if (pr[1] < nr[1])
							strip->blendin= pr[1] - strip->start;
						else
							strip->blendin= nr[1] - strip->start;
					}
					else if (IN_RANGE(pr[0], strip->start, strip->end) && 
							(IN_RANGE(pr[1], strip->start, strip->end)==0))
					{
						/* previous strip intersects end of current */
						strip->blendout= strip->end - pr[0];
						strip->blendin= nr[1] - strip->start;
					}
					else {
						/* only next strip intersects current */
						strip->blendin= nr[1] - strip->start;
					}
				}
				else if (IN_RANGE(nr[0], strip->start, strip->end) && 
						(IN_RANGE(nr[1], strip->start, strip->end)==0) )
				{
					/* next strip intersects end of current */
					
					if ( IN_RANGE(pr[0], strip->start, strip->end) && 
						(IN_RANGE(pr[1], strip->start, strip->end)==0) )
					{
						/* previous strip also intersects end of current */
						if (pr[1] > nr[1])
							strip->blendout= strip->end - pr[0];
						else
							strip->blendout= strip->end - nr[0];
					}
					else if (IN_RANGE(pr[1], strip->start, strip->end) && 
							(IN_RANGE(pr[0], strip->start, strip->end)==0))
					{
						/* previous strip intersects start of current */
						strip->blendin= pr[1] - strip->start;
						strip->blendout= strip->end - nr[0];
					}
					else {
						/* only next strip intersects current */
						strip->blendout= strip->end - nr[0];
					}
				}
				
				/* make sure blending stays in ranges */
				CLAMP(strip->blendin, 0, (strip->end-strip->start));
				CLAMP(strip->blendout, 0, (strip->end-strip->start));
			}
		}
	}
	
}

void reset_action_strips(int val)
{
	Base *base;
	bActionStrip *strip;
	
	for (base=G.scene->base.first; base; base=base->next) {
		if (base->object->nlaflag & OB_NLA_COLLAPSED)
			continue;
		
		for (strip = base->object->nlastrips.last; strip; strip=strip->prev) {
			if (strip->flag & ACTSTRIP_SELECT) {
				switch (val) {
					case 1:
					{
						/* clear scaling - reset to 1.0 without touching keys */
						float actlen= (strip->actend - strip->actstart);
						
						strip->scale= 1.0f;
						strip->end= (strip->repeat * actlen) + strip->start;
					}
						break;
					case 2:
					{
						/* reset action-range */
						calc_action_range(strip->act, &strip->actstart, &strip->actend, 1);
					}
						break;
					case 3:
					{
						/* apply scale to keys - scale is reset to 1.0f, but keys stay at the same times */
						bActionChannel *achan;
						
						if (strip->act) {
							for (achan= strip->act->chanbase.first; achan; achan= achan->next) {
								actstrip_map_ipo_keys(base->object, achan->ipo, 0, 0);
							}
							
							/* now we can reset scale */
							calc_action_range(strip->act, &strip->actstart, &strip->actend, 1);
							strip->scale= 1.0f;
							strip->end = (strip->repeat * (strip->actend - strip->actstart)) + strip->start;
						}
					}
						break;
				}
				base->object->ctime= -1234567.0f;	// evil! 
				DAG_object_flush_update(G.scene, base->object, OB_RECALC_OB|OB_RECALC_DATA);
			}
		}
	}
	BIF_undo_push("Reset NLA strips");
	allqueue (REDRAWVIEW3D, 0);
	allqueue (REDRAWACTION, 0);
	allqueue (REDRAWNLA, 0);
}

void snap_action_strips(int snap_mode)
{
	Base *base;
	bActionStrip *strip;
	
	for (base=G.scene->base.first; base; base=base->next) {
		/* object has ipo - these keyframes should be able to be snapped, even if strips are collapsed */
		if (base->object->ipo) {
			snap_ipo_keys(base->object->ipo, snap_mode);
		}
		
		/* object is collapsed - action and nla strips not shown/editable */
		if (base->object->nlaflag & OB_NLA_COLLAPSED)
			continue;
		
		/* snap action strips */
		for (strip = base->object->nlastrips.last; strip; strip=strip->prev) {
			if (strip->flag & ACTSTRIP_SELECT) {
				if (snap_mode==1) {
					/* nearest frame */
					strip->start= floor(strip->start+0.5);
					strip->end= floor(strip->end+0.5);
				}
				else if (snap_mode==2) {
					/* current frame */
					float diff;
					if (CFRA < strip->start) {
						diff = (strip->start - CFRA);
						strip->start -= diff;
						strip->end -= diff;
					}
					else {
						diff = (CFRA - strip->start);
						strip->start += diff;
						strip->end += diff;
					}
				}
				else if (snap_mode==3) {
					/* nearest second */
					float secf = FPS;
					strip->start= (float)(floor(strip->start/secf + 0.5f) * secf);
					strip->end= (float)(floor(strip->end/secf + 0.5f) * secf);
				}
			}
		}
		
		/* object has action */
		if (base->object->action) {
			ListBase act_data = {NULL, NULL};
			bActListElem *ale;
			int filter;
			
			/* filter action data */
			filter= (ACTFILTER_VISIBLE | ACTFILTER_FOREDIT | ACTFILTER_IPOKEYS);
			actdata_filter(&act_data, filter, base->object->action, ACTCONT_ACTION);
			
			/* snap to frame */
			for (ale= act_data.first; ale; ale= ale->next) {
				actstrip_map_ipo_keys(base->object, ale->key_data, 0, 1); 
				snap_ipo_keys(ale->key_data, snap_mode);
				actstrip_map_ipo_keys(base->object, ale->key_data, 1, 1);
			}
			BLI_freelistN(&act_data);
			
			remake_action_ipos(base->object->action);
		}
	}
	BIF_undo_push("Snap NLA strips");
	allqueue (REDRAWVIEW3D, 0);
	allqueue (REMAKEIPO, 0);
	allqueue (REDRAWIPO, 0);
	allqueue (REDRAWACTION, 0);
	allqueue (REDRAWNLA, 0);
}

static void set_active_strip(Object *ob, bActionStrip *act)
{
	bActionStrip *strip;
	
	for (strip = ob->nlastrips.first; strip; strip=strip->next)
		strip->flag &= ~ACTSTRIP_ACTIVE;
	
	if(act) {
		act->flag |= ACTSTRIP_ACTIVE;
	
		if(ob->action!=act->act) {
			if(ob->action) ob->action->id.us--;
			if(act->act->id.lib) {
				ob->action= NULL;
			}
			else {
				ob->action= act->act;
				id_us_plus(&ob->action->id);
			}			
			allqueue(REDRAWIPO, 0);
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWACTION, 0);
			allqueue(REDRAWNLA, 0);
			ob->ctime= -1234567.0f;	// eveil! 
			DAG_object_flush_update(G.scene, ob, OB_RECALC_OB|OB_RECALC_DATA);
		}
	}	
}

void convert_nla(void)
{
	bActionStrip *strip;
	Object *ob= OBACT;
	char str[128];
	short event;
	
	if ((ob==NULL)||(ob->action==NULL)) {
		error("Need active Object to convert Action to NLA Strip");
		return;
	}
	
	sprintf(str, "Convert Action%%t|%s to NLA Strip%%x1", ob->action->id.name+2);
	event = pupmenu(str);
	
	if (event==1) {
		if (ob->action) {
			deselect_nlachannel_keys(0);
			strip = convert_action_to_strip(ob); //creates a new NLA strip from the action in given object
			set_active_strip(ob, strip);
			BIF_undo_push("Convert NLA");
			allqueue (REDRAWNLA, 0);
		}
	}
}

static void add_nla_block(short event)
{
	Object *ob= OBACT;
	bAction *act=NULL;
	bActionStrip *strip;
	int		cur;

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
	id_us_plus(&act->id);
	calc_action_range(strip->act, &strip->actstart, &strip->actend, 1);
	strip->start = G.scene->r.cfra;		/* could be mval[0] another time... */
	strip->end = strip->start + (strip->actend-strip->actstart);
		/* simple prevention of zero strips */
	if(strip->start>strip->end-2) 
		strip->end= strip->start+100;
	strip->repeat = strip->scale= 1.0f;
	
	strip->flag = ACTSTRIP_SELECT|ACTSTRIP_LOCK_ACTION;
	
	find_stridechannel(ob, strip);
	set_active_strip(ob, strip);
	strip->object= group_get_member_with_action(ob->dup_group, act);
	if(strip->object)
		id_lib_extern(&strip->object->id);	/* checks lib data, sets correct flag for saving then */

	if(ob->nlastrips.first == NULL)
		ob->nlaflag |= OB_NLA_OVERRIDE;
	
	BLI_addtail(&ob->nlastrips, strip);

	BIF_undo_push("Add NLA strip");
}

static void add_nla_block_by_name(char name[32], Object *ob, short hold, short add, float repeat)
{
	bAction *act=NULL;
	bActionStrip *strip;
	int		cur;

	if (name){
		for (cur = 1, act=G.main->action.first; act; act=act->id.next, cur++){
			if (strcmp(name,act->id.name)==0) {
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
	strip->scale= 1.0f;
	
	deselect_nlachannel_keys(0);
	
	/* Link the action to the strip */
	strip->act = act;
	calc_action_range(strip->act, &strip->actstart, &strip->actend, 1);
	strip->start = G.scene->r.cfra;		/* could be mval[0] another time... */
	strip->end = strip->start + (strip->actend-strip->actstart);
		/* simple prevention of zero strips */
	if(strip->start>strip->end-2) 
		strip->end= strip->start+100;
	
	strip->flag = ACTSTRIP_SELECT|ACTSTRIP_LOCK_ACTION; //|ACTSTRIP_USEMATCH;
	
	if (hold==1)
		strip->flag = strip->flag|ACTSTRIP_HOLDLASTFRAME;
		
	if (add==1)
		strip->mode = ACTSTRIPMODE_ADD;
	
	find_stridechannel(ob, strip);
	
	set_active_strip(ob, strip);
	
	strip->repeat = repeat;
	
	act->id.us++;
	
	if(ob->nlastrips.first == NULL)
		ob->nlaflag |= OB_NLA_OVERRIDE;
		
	BLI_addtail(&ob->nlastrips, strip);
}

static void add_nla_databrowse_callback(unsigned short val)
{
	/* val is not used, databrowse needs it to optional pass an event */
	short event;
	
	if(OBACT==NULL) return;
	
	event= G.snla->menunr;	/* set by databrowse or pupmenu */
	
	add_nla_block(event);
}

/* Adds strip to to active Object */
void add_nlablock(void)
{
	Object *ob= OBACT;
	short event;
	short nr=0;
	char *str, title[64];
	
	if(ob==NULL) {
		error("Need active Object to add NLA strips");
		return;
	}
	
	sprintf(title, "Add Action strip to: %s", ob->id.name+2);
	
	/* Popup action menu */
	IDnames_to_pupstring(&str, title, NULL, &G.main->action, (ID *)G.scene, &nr);
	
	if(nr==-2) {
		MEM_freeN(str);

		activate_databrowse((ID *)NULL, ID_AC, 0, 0, &G.snla->menunr, 
							add_nla_databrowse_callback );
		
		return;			
	}
	else {
		event = pupmenu_col(str, 20);
		MEM_freeN(str);
		add_nla_block(event);
	}
}

/* Creates a new action, and makes a new actionstrip of that */
void add_empty_nlablock(void)
{
	Object *ob= OBACT;
	bAction *act= NULL;
	bActionStrip *strip;
	
	/* check for active object first - will add strip to active object */
	if (ob == NULL) 
		return;
		
	/* make new action */
	if ((ob->type == OB_ARMATURE) && (ob->flag & OB_POSEMODE))
		act= add_empty_action("ObAction");
	else
		act= add_empty_action("Action");
		
	/* make a new strip for it */
	add_nla_block_by_name(act->id.name, ob, 0, 1, 1.0f);
	strip= ob->nlastrips.last; 
	
	/* change some settings of the strip - try to avoid bad scaling */
	if ((EFRA-CFRA) < 100) {
		strip->flag |= ACTSTRIP_AUTO_BLENDS;
		strip->flag &= ~ACTSTRIP_LOCK_ACTION;
		strip->actstart = CFRA;
		strip->actend = CFRA + 100;
		
		strip->start = CFRA;
		strip->end = CFRA + 100;
	}
	else {
		strip->flag |= ACTSTRIP_AUTO_BLENDS;
		strip->flag &= ~ACTSTRIP_LOCK_ACTION;
		strip->actstart = CFRA;
		strip->actend = EFRA;
		
		strip->start = CFRA;
		strip->end = EFRA;
	}
	
	BIF_undo_push("Add NLA strip");
}

/* Adds strip to to active Object */
static void relink_active_strip(void)
{
	Object *ob= OBACT;
	bActionStrip *strip;
	bAction *act;
	short event;
	short cur;
	char *str;
	
	if(ob==NULL) return;
	if(ob->nlaflag & OB_NLA_COLLAPSED) return;
	
	for (strip = ob->nlastrips.first; strip; strip=strip->next)
		if(strip->flag & ACTSTRIP_ACTIVE)
			break;
	
	if(strip==NULL) return;
	
	/* Popup action menu */
	IDnames_to_pupstring(&str, "Relink Action strip", NULL, &G.main->action, (ID *)G.scene, NULL);
	if(str) {
		event = pupmenu_col(str, 20);
		MEM_freeN(str);
		
		for (cur = 1, act=G.main->action.first; act; act=act->id.next, cur++){
			if (cur==event){
				break;
			}
		}
		
		if(act) {
			if(strip->act) strip->act->id.us--;
			strip->act = act;
			id_us_plus(&act->id);
			
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWACTION, 0);
			allqueue(REDRAWNLA, 0);
		}
	}
}



/* Left hand side of channels display, selects objects */
static void mouse_nlachannels(short mval[2])
{
	bActionStrip *strip= NULL;
	Base	*base;
	Object *ob=NULL;
	float	x,y;
	int		click, obclick=0, actclick=0;
	int		wsize;
	
	wsize = (count_nla_levels ()*(NLACHANNELHEIGHT+NLACHANNELSKIP));
	wsize+= NLACHANNELHEIGHT/2;

	areamouseco_to_ipoco(G.v2d, mval, &x, &y);
	click = (int)floor( ((float)wsize - y) / (NLACHANNELHEIGHT+NLACHANNELSKIP));
	
	if (click<0)
		return;

	for (base = G.scene->base.first; base; base=base->next){
		if (nla_filter(base)) {
			ob= base->object;
			
			/* See if this is a base selected */
			if (click==0) {
				obclick= 1;
				break;
			}
			click--;
			
			/* see if any strips under object */
			if ((ob->nlaflag & OB_NLA_COLLAPSED)==0) {
				/* See if this is an action */
				if (ob->action){
					if (click==0) {
						actclick= 1;
						break;
					}
					click--;
				}

				/* See if this is an nla strip */
				if(ob->nlastrips.first) {
					for (strip = ob->nlastrips.first; strip; strip=strip->next){
						if (click==0) break;
						click--;				
					}
					if (strip && click==0) break;
				}
			}
		}
	}

	if (!base)
		return;

	/* Handle object strip selection */
	if (G.qual & LR_SHIFTKEY) {
		if (base->flag & SELECT) base->flag &= ~SELECT;
		else base->flag |= SELECT;
	}
	else {
		deselect_nlachannels (0);	// Auto clear
		base->flag |= SELECT;
	}
	ob->flag= base->flag;
	
	if(base!=BASACT) set_active_base(base);
	
	if(actclick) /* de-activate all strips */
		set_active_strip(ob, NULL);
	else if(strip) {
		if(mval[0] >= (NLAWIDTH-16)) /* toggle strip muting */
			strip->flag ^= ACTSTRIP_MUTE;
		else /* set action */
			set_active_strip(ob, strip);
	}

	/* icon toggles beside strip */
	if (obclick && mval[0]<20) {
		/* collapse option for NLA object strip */
		ob->nlaflag ^= OB_NLA_COLLAPSED;
	}
	else if(obclick && mval[0]<36) {
		/* override option for NLA */
		ob->nlaflag ^= OB_NLA_OVERRIDE;
	}
	else if((obclick) && (ob->ipo) && (mval[0] >= (NLAWIDTH-16))) {
		/* mute Object IPO-block */
		ob->ipo->muteipo = (ob->ipo->muteipo)? 0: 1;
	}
	
	ob->ctime= -1234567.0f;	// eveil! 
	DAG_object_flush_update(G.scene, ob, OB_RECALC_OB|OB_RECALC_DATA);

	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWNLA, 0);
	
}

void deselect_nlachannel_keys (int test)
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
			
			/* check if collapsed */
			if (base->object->nlaflag & OB_NLA_COLLAPSED)
				continue;
			
			/* Test action ipos */
			if (sel){
				if (base->object->action){
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
				for (strip=base->object->nlastrips.first; strip; strip=strip->next){
					if (strip->flag & ACTSTRIP_SELECT){
						sel = 0;
						break;
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

		/* check if collapsed */
		if (base->object->nlaflag & OB_NLA_COLLAPSED)
			continue;
		
		/* Set the action ipos */
		if (base->object->action){
			for (chan=base->object->action->chanbase.first; chan; chan=chan->next){
				set_ipo_key_selection(chan->ipo, sel);
				/* Set the action constraint ipos */
				for (conchan=chan->constraintChannels.first; conchan; conchan=conchan->next)
					set_ipo_key_selection(conchan->ipo, sel);
			}
		}
		
		/* Set the nlastrips */
		for (strip=base->object->nlastrips.first; strip; strip=strip->next){
			if (sel)
				strip->flag |= ACTSTRIP_SELECT;
			else
				strip->flag &= ~ACTSTRIP_SELECT;
		}
	}
}

/* very bad call! */
static void recalc_all_ipos(void)
{
	Ipo *ipo;
	IpoCurve *icu;
	
	/* Go to each ipo */
	for (ipo=G.main->ipo.first; ipo; ipo=ipo->id.next){
		for (icu = ipo->curve.first; icu; icu=icu->next){
			sort_time_ipocurve(icu);
			testhandles_ipocurve(icu);
		}
	}
}

void transform_nlachannel_keys(int mode, int dummy)
{
	short context = (U.flag & USER_DRAGIMMEDIATE)?CTX_TWEAK:CTX_NONE;

	switch (mode) {
		case 'g':
		{
			initTransform(TFM_TIME_TRANSLATE, context);
			Transform();
		}
			break;
		case 's':
		{
			initTransform(TFM_TIME_SCALE, context);
			Transform();
		}
			break;
		case 'e':
		{
			initTransform(TFM_TIME_EXTEND, context);
			Transform();
		}
			break;
	}
}

void delete_nlachannel_keys(void)
{
	Base *base;
	bActionChannel *chan;
	bConstraintChannel *conchan;
	bActionStrip *strip, *nextstrip;
		
	for (base = G.scene->base.first; base; base=base->next){
		/* Delete object ipos */
		delete_ipo_keys(base->object->ipo);
		
		/* Delete object constraint keys */
		for(conchan=base->object->constraintChannels.first; conchan; conchan=conchan->next)
			delete_ipo_keys(conchan->ipo);

		/* skip actions and nlastrips if object collapsed */
		if (base->object->nlaflag & OB_NLA_COLLAPSED)
			continue;
			
		/* Delete NLA strips */
		for (strip = base->object->nlastrips.first; strip; strip=nextstrip){
			nextstrip=strip->next;
			if (strip->flag & ACTSTRIP_SELECT){
				free_actionstrip(strip);
				BLI_remlink(&base->object->nlastrips, strip);
				MEM_freeN(strip);
			}
		}
		
		/* Delete action ipos */
		if (base->object->action){
			for (chan=base->object->action->chanbase.first; chan; chan=chan->next){
				if (EDITABLE_ACHAN(chan))
					delete_ipo_keys(chan->ipo);
					
				/* Delete action constraint keys */
				for(conchan=chan->constraintChannels.first; conchan; conchan=conchan->next){
					if (EDITABLE_CONCHAN(conchan))
						delete_ipo_keys(conchan->ipo);
				}
			}
		}
	}
	
	recalc_all_ipos();	// bad
	synchronize_action_strips();
	
	BIF_undo_push("Delete NLA keys");
	allspace(REMAKEIPO,0);
	allqueue (REDRAWVIEW3D, 0);
	allqueue(REDRAWNLA, 0);
	allqueue(REDRAWIPO, 0);
}

void duplicate_nlachannel_keys(void)
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
		
		/* skip actions and nlastrips if object collapsed */
		if (base->object->nlaflag & OB_NLA_COLLAPSED)
			continue;
			
		/* Duplicate nla strips */
		laststrip = base->object->nlastrips.last;
		for (strip=base->object->nlastrips.first; strip; strip=strip->next){
			if (strip->flag & ACTSTRIP_SELECT){
				bActionStrip *newstrip;
				
				copy_actionstrip(&newstrip, &strip);
				
				BLI_addtail(&base->object->nlastrips, newstrip);
				
				strip->flag &= ~ACTSTRIP_SELECT;
				newstrip->flag |= ACTSTRIP_SELECT;
				set_active_strip(base->object, newstrip);

			}
			if (strip==laststrip)
				break;
		}
		
		/* Duplicate actionchannel keys */
		if (base->object->action){
			for (chan=base->object->action->chanbase.first; chan; chan=chan->next){
				if (EDITABLE_ACHAN(chan))
					duplicate_ipo_keys(chan->ipo);
					
				/* Duplicate action constraint keys */
				for(conchan=chan->constraintChannels.first; conchan; conchan=conchan->next) {
					if (EDITABLE_CONCHAN(conchan))
						duplicate_ipo_keys(conchan->ipo);
				}
			}
		}
	}
	
	BIF_undo_push("Duplicate NLA");
	transform_nlachannel_keys ('g', 0);
}

void borderselect_nla(void)
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
		ymax+= (NLACHANNELHEIGHT+NLACHANNELSKIP)/2;
	
		for (base=G.scene->base.first; base; base=base->next){
			if (nla_filter(base)) {
				
				ymin=ymax-(NLACHANNELHEIGHT+NLACHANNELSKIP);
				
				/* Check object ipos */
				if (base->object->ipo){
					if (!((ymax < rectf.ymin) || (ymin > rectf.ymax)))
						borderselect_ipo_key(base->object->ipo, rectf.xmin, rectf.xmax,
                                 selectmode);
				}
				/* Check object constraint ipos */
				for (conchan=base->object->constraintChannels.first; conchan; conchan=conchan->next){
					if (!((ymax < rectf.ymin) || (ymin > rectf.ymax)))
						borderselect_ipo_key(conchan->ipo, rectf.xmin, rectf.xmax,
                                 selectmode);
				}
				
				ymax=ymin;
				
				/* skip actions and nlastrips if object collapsed */
				if (base->object->nlaflag & OB_NLA_COLLAPSED)
					continue;
				
				/* Check action ipos */
				if (base->object->action){
					bActionChannel *chan;
					float xmin, xmax;
					
					ymin=ymax-(NLACHANNELHEIGHT+NLACHANNELSKIP);
					
					/* if action is mapped in NLA, it returns a correction */
					xmin= get_action_frame(base->object, rectf.xmin);
					xmax= get_action_frame(base->object, rectf.xmax);
					
					if (!((ymax < rectf.ymin) || (ymin > rectf.ymax))){
						for (chan=base->object->action->chanbase.first; chan; chan=chan->next){
							borderselect_ipo_key(chan->ipo, xmin, xmax, selectmode);
							/* Check action constraint ipos */
							for (conchan=chan->constraintChannels.first; conchan; conchan=conchan->next)
								borderselect_ipo_key(conchan->ipo, xmin, xmax, selectmode);
						}
					}

					ymax=ymin;
				}	/* End of if action */
				
				/* Skip nlastrips */
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
		}	
		BIF_undo_push("Border select NLA");
		allqueue(REDRAWMARKER, 0);
	}
}

/* right hand side of window, does ipokeys, actionkeys or strips */
static void mouse_nla(int selectmode)
{
	Base *base;
	bAction *act;
	bActionChannel *chan;
	bActionStrip *rstrip;
	bConstraintChannel *conchan;
	TimeMarker *marker;
	float	selx;
	short	mval[2];
	short sel, isdone=0;
	
	getmouseco_areawin (mval);
	
	/* Try object ipo or ob-constraint ipo selection */
	base= get_nearest_nlachannel_ob_key(&selx, &sel);
	marker=find_nearest_marker(SCE_MARKERS, 1);
	if (base) {
		isdone= 1;
		
		if (selectmode == SELECT_REPLACE){
			deselect_nlachannel_keys(0);
			selectmode = SELECT_ADD;
		}
		
		select_ipo_key(base->object->ipo, selx, selectmode);
		
		/* Try object constraint selection */
		for (conchan=base->object->constraintChannels.first; conchan; conchan=conchan->next)
			select_ipo_key(conchan->ipo, selx, selectmode);
	}
	else if (marker) {
		/* marker */		
		if (selectmode == SELECT_REPLACE) {			
			deselect_markers(0, 0);
			marker->flag |= SELECT;
		}
		else if (selectmode == SELECT_INVERT) {
			if (marker->flag & SELECT)
				marker->flag &= ~SELECT;
			else
				marker->flag |= SELECT;
		}
		else if (selectmode == SELECT_ADD) 
			marker->flag |= SELECT;
		else if (selectmode == SELECT_SUBTRACT)
			marker->flag &= ~SELECT;
		
		std_rmouse_transform(transform_markers);
		
		allqueue(REDRAWMARKER, 0);
	}
	else {
		/* Try action ipo selection */
		act= get_nearest_nlachannel_ac_key(&selx, &sel);
		if (act) {
			isdone= 1;
			
			if (selectmode == SELECT_REPLACE){
				deselect_nlachannel_keys(0);
				selectmode = SELECT_ADD;
			}
			
			for (chan=act->chanbase.first; chan; chan=chan->next) {
				select_ipo_key(chan->ipo, selx, selectmode);
				/* Try action constraint selection */
				for (conchan=chan->constraintChannels.first; conchan; conchan=conchan->next)
					select_ipo_key(conchan->ipo, selx, selectmode);
			}
		}
		else {
	
			/* Try nla strip selection */
			base= get_nearest_nlastrip(&rstrip, &sel);
			if (base){
				isdone= 1;
				
				if (!(G.qual & LR_SHIFTKEY)){
					deselect_nlachannel_keys(0);
					sel = 0;
				}
				
				if (sel)
					rstrip->flag &= ~ACTSTRIP_SELECT;
				else
					rstrip->flag |= ACTSTRIP_SELECT;
				
				set_active_strip(base->object, rstrip);
				
				if(base!=BASACT) set_active_base(base);
			}
		}
	}
	
	if(isdone) {
		std_rmouse_transform(transform_nlachannel_keys);
		
		allqueue(REDRAWIPO, 0);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWNLA, 0);
	}
}

/* This function is currently more complicated than it seems like it should be.
* However, this will be needed once the nla strip timeline is more complex */
static Base *get_nearest_nlastrip (bActionStrip **rstrip, short *sel)
{
	Base *base, *firstbase=NULL;
	bActionStrip *strip, *firststrip=NULL, *foundstrip=NULL;
	rctf	rectf;
	float ymin, ymax;
	short mval[2];
	short foundsel = 0;

	getmouseco_areawin (mval);
	
	mval[0]-=7;
	areamouseco_to_ipoco(G.v2d, mval, &rectf.xmin, &rectf.ymin);
	
	mval[0]+=14;
	areamouseco_to_ipoco(G.v2d, mval, &rectf.xmax, &rectf.ymax);
	
	ymax = count_nla_levels();
	ymax*=(NLACHANNELHEIGHT + NLACHANNELSKIP);
	ymax+= NLACHANNELHEIGHT/2;
	
	for (base = G.scene->base.first; base; base=base->next){
		if (nla_filter(base)) {
			
			/* Skip object ipos */
			ymax-=(NLACHANNELHEIGHT+NLACHANNELSKIP);
			
			/* check if skip strips if collapsed */
			if (base->object->nlaflag & OB_NLA_COLLAPSED)
				continue;

			/* Skip action ipos */
			if (base->object->action)
				ymax-=(NLACHANNELHEIGHT+NLACHANNELSKIP);
			
			/* the strips */
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
	float firstvertx=-1, foundx=-1;
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
	
	ymax*= (NLACHANNELHEIGHT + NLACHANNELSKIP);
	ymax+= NLACHANNELHEIGHT/2;
	
	*sel=0;
	
	for (base=G.scene->base.first; base; base=base->next){
		if (nla_filter(base)) {
			
			ymin=ymax-(NLACHANNELHEIGHT+NLACHANNELSKIP);
			
			/* Handle object ipo selection */
			if (base->object->ipo){
				if (!((ymax < rectf.ymin) || (ymin > rectf.ymax))){
					for (icu=base->object->ipo->curve.first; icu; icu=icu->next){
						for (i=0; i<icu->totvert; i++){
							if (icu->bezt[i].vec[1][0] > rectf.xmin && icu->bezt[i].vec[1][0] <= rectf.xmax ){
								if (!firstbase){
									firstbase=base;
									firstvertx=icu->bezt[i].vec[1][0];
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
			/* Handle object constraint ipos */
			for (conchan=base->object->constraintChannels.first; conchan; conchan=conchan->next){
				if ( !((ymax < rectf.ymin) || (ymin > rectf.ymax)) && conchan->ipo){
					for (icu=conchan->ipo->curve.first; icu; icu=icu->next){
						for (i=0; i<icu->totvert; i++){
							if (icu->bezt[i].vec[1][0] > rectf.xmin && icu->bezt[i].vec[1][0] <= rectf.xmax ){
								if (!firstbase){
									firstbase=base;
									firstvertx=icu->bezt[i].vec[1][0];
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
			
			/* Skip actions and nlastrips if object is collapsed */
			if (base->object->nlaflag & OB_NLA_COLLAPSED)
				continue;
			
			/* Skip action ipos */
			if (base->object->action){
				ymax-=(NLACHANNELHEIGHT+NLACHANNELSKIP);
			}
			/* Skip nlastrips */
			ymax-=(NLACHANNELHEIGHT+NLACHANNELSKIP)*BLI_countlist(&base->object->nlastrips);
		}
	}	
	
	*index=firstvertx;
	return firstbase;
}

static bAction *get_nearest_nlachannel_ac_key (float *index, short *sel)
{
	Base *base;
	IpoCurve *icu;
	bAction *firstact=NULL;
	bActionChannel *chan;
	bConstraintChannel *conchan;
	rctf	rectf;
	float firstvert=-1, foundx=-1;
	float ymin, ymax, xmin, xmax;
	int i;
	int	foundsel=0;
	short mval[2];
	
	*index=0;
	
	getmouseco_areawin (mval);
	
	mval[0]-=7;
	areamouseco_to_ipoco(G.v2d, mval, &rectf.xmin, &rectf.ymin);
	
	mval[0]+=14;
	areamouseco_to_ipoco(G.v2d, mval, &rectf.xmax, &rectf.ymax);
	
	ymax = count_nla_levels();
	
	ymax*= (NLACHANNELHEIGHT + NLACHANNELSKIP);
	ymax+= NLACHANNELHEIGHT/2;
	
	*sel=0;
	
	for (base=G.scene->base.first; base; base=base->next){
		/* Handle object ipo selection */
		if (nla_filter(base)) {
			
			/* Skip object ipo and ob-constraint ipo */
			ymin=ymax-(NLACHANNELHEIGHT+NLACHANNELSKIP);
			
			/* skip this object if it is collapsed */
			if (base->object->nlaflag & OB_NLA_COLLAPSED)
				continue;
			
			ymax=ymin;
			
			/* Handle action ipos */
			if (base->object->action){
				bAction *act= base->object->action;
				
				/* if action is mapped in NLA, it returns a correction */
				xmin= get_action_frame(base->object, rectf.xmin);
				xmax= get_action_frame(base->object, rectf.xmax);
				
				ymin=ymax-(NLACHANNELHEIGHT+NLACHANNELSKIP);
				if (!((ymax < rectf.ymin) || (ymin > rectf.ymax))){
					for (chan=act->chanbase.first; chan; chan=chan->next){
						if(chan->ipo) {
							for (icu=chan->ipo->curve.first; icu; icu=icu->next){
								for (i=0; i<icu->totvert; i++){
									if (icu->bezt[i].vec[1][0] > xmin && icu->bezt[i].vec[1][0] <= xmax ){
										if (!firstact){
											firstact= act;
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
											return act;
										}
									}
								}
							}
						}
						
						for (conchan=chan->constraintChannels.first; conchan; conchan=conchan->next){
							ymin=ymax-(NLACHANNELHEIGHT+NLACHANNELSKIP);
							if ( !((ymax < rectf.ymin) || (ymin > rectf.ymax)) && conchan->ipo){
								for (icu=conchan->ipo->curve.first; icu; icu=icu->next){
									for (i=0; i<icu->totvert; i++){
										if (icu->bezt[i].vec[1][0] > xmin && icu->bezt[i].vec[1][0] <= xmax ){
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
			ymax-=(NLACHANNELHEIGHT+NLACHANNELSKIP)*BLI_countlist(&base->object->nlastrips);
		}
	}	
	
	*index=firstvert;
	return firstact;
}

void deselect_nlachannels(int test)
{
	Base *base;
	int sel = 1;

	if (test){
		for (base=G.scene->base.first; base; base=base->next){
			/* Check base flags for previous selection */
			if (base->flag & SELECT){
				sel=0;
				break;
			}
		}
	}
	else
		sel = 0;

	/* Select objects */
	for (base=G.scene->base.first; base; base=base->next){
		if (sel){
			if (nla_filter(base))
				base->flag |= SELECT;
		}
		else
			base->flag &= ~SELECT;
		
		base->object->flag= base->flag;
	}	
}

static Object *get_object_from_active_strip(void) {

	Base *base;
	bActionStrip *strip;
	
	for (base=G.scene->base.first; base; base=base->next) {
		if ((base->object->nlaflag & OB_NLA_COLLAPSED)==0) {
			for (strip = base->object->nlastrips.first; 
				 strip; strip=strip->next){
				if (strip->flag & ACTSTRIP_SELECT) {
					return base->object;
				}
			}
		}
	}
	return NULL;
}


void winqreadnlaspace(ScrArea *sa, void *spacedata, BWinEvent *evt)
{
	unsigned short event= evt->event;
	short val= evt->val;
	SpaceNla *snla = curarea->spacedata.first;
	int doredraw= 0;
	short	mval[2];
	float dx,dy;
	int	cfra;
	short mousebut = L_MOUSE;
	Object		*ob; //in shift-B / bake
	
	if (curarea->win==0) return;
	if (!snla) return;
	
	if(val) {
		if( uiDoBlocks(&curarea->uiblocks, event, 1)!=UI_NOTHING ) event= 0;
		
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
		
		getmouseco_areawin(mval);
		
		switch(event) {
			case UI_BUT_EVENT:
				do_nlabuts(val); // in drawnla.c
				break;
				
			case HOMEKEY:
				do_nla_buttons(B_NLAHOME);
				break;
				
			case EQUALKEY:
				shift_nlastrips_up();
				break;
			
			case PAGEUPKEY:
				if (G.qual & LR_CTRLKEY)
					shift_nlastrips_up();
				else {
					nextprev_marker(1);
					allqueue(REDRAWMARKER, 0);
				}				
				break;
				
			case MINUSKEY:
				shift_nlastrips_down();
				break;
				
			case PAGEDOWNKEY:
				if (G.qual & LR_CTRLKEY)
					shift_nlastrips_down();
				else {
					nextprev_marker(-1);
					allqueue(REDRAWMARKER, 0);
				}
				break;
				
			case AKEY:
				if (G.qual & LR_SHIFTKEY){
					add_nlablock();
					allqueue (REDRAWNLA, 0);
					allqueue (REDRAWVIEW3D, 0);
				}
				else if (G.qual & LR_CTRLKEY) {
					deselect_markers(1, 0);
					allqueue(REDRAWMARKER, 0);
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
					BIF_undo_push("(De)select all NLA");
				}
				break;
				
			case BKEY:
				if (G.qual & LR_SHIFTKEY){
					bake_all_to_action();
					allqueue (REDRAWNLA, 0);
				    allqueue (REDRAWVIEW3D, 0);
				    BIF_undo_push("Bake All To Action");
				    ob = get_object_from_active_strip();
				    //build_match_caches(ob);
				}
				else if (G.qual & LR_CTRLKEY) 
					borderselect_markers();
				else
					borderselect_nla();
				break;
				
			case CKEY:
				if(G.qual==LR_CTRLKEY) {
					if(okee("Copy Modifiers"))
						copy_action_modifiers();
				}
				else convert_nla();
				break;
				
			case DKEY:
				if (G.qual == (LR_CTRLKEY|LR_SHIFTKEY) && mval[0]>=NLAWIDTH) {
					duplicate_marker();
				}
				else if (G.qual & LR_SHIFTKEY && mval[0]>=NLAWIDTH){
					duplicate_nlachannel_keys();
					update_for_newframe_muted();
				}
				
				break;
				
			case EKEY:
				if (mval[0] >= NLAWIDTH) {
					transform_nlachannel_keys ('e', 0);
					update_for_newframe_muted();
				}
				break;
				
			case GKEY:
				if (mval[0]>=NLAWIDTH) {
					if (G.qual & LR_CTRLKEY) {
						transform_markers('g', 0);
					}
					else {
						transform_nlachannel_keys ('g', 0);
						update_for_newframe_muted();
					}
				}
				break;
			
			case MKEY:
				/* marker operations */
				if (G.qual == 0)
					add_marker(CFRA);
				else if (G.qual == LR_CTRLKEY)
					rename_marker();
				else 
					break;
				allqueue(REDRAWMARKER, 0);
				break;				
			
			case NKEY:
				if(G.qual==0) {
					toggle_blockhandler(curarea, NLA_HANDLER_PROPERTIES, UI_PNL_TO_MOUSE);
					scrarea_queue_winredraw(curarea);
				}
				else if (G.qual & LR_SHIFTKEY) {
					add_empty_nlablock();
				}
				break;
			case LKEY:
				relink_active_strip();
				break;
				
			case PKEY:
				if (G.qual & LR_CTRLKEY) /* set preview range */
					anim_previewrange_set();
				else if (G.qual & LR_ALTKEY) /* clear preview range */
					anim_previewrange_clear();
				allqueue(REDRAWMARKER, 0);
				break;
				
			case SKEY:
				if (G.qual==LR_ALTKEY) {
					val= pupmenu("Action Strip Scale%t|Reset Strip Scale%x1|Remap Action Start/End%x2|Apply Scale%x3");
					if (val > 0)
						reset_action_strips(val);
				}
				else if (G.qual & LR_SHIFTKEY) {
					if (snla->flag & SNLA_DRAWTIME)
						val= pupmenu("Snap To%t|Nearest Second%x3|Current Time%x2");
					else
						val= pupmenu("Snap To%t|Nearest Frame%x1|Current Frame%x2");
					if (ELEM3(val, 1, 2, 3))
						snap_action_strips(val);
				}
				else {
					if (mval[0]>=NLAWIDTH)
						transform_nlachannel_keys ('s', 0);
					update_for_newframe_muted();
				}
				break;
				
			case TKEY:
				if (G.qual & LR_CTRLKEY) {
					val= pupmenu("Time value%t|Frames %x1|Seconds%x2");
					
					if (val > 0) {
						if (val == 2) snla->flag |= SNLA_DRAWTIME;
						else snla->flag &= ~SNLA_DRAWTIME;
						
						doredraw= 1;
					}
				}				
				break;
				
			case DELKEY:
			case XKEY:
				if (mval[0]>=NLAWIDTH) {
					if (okee("Erase selected?")) {
						delete_nlachannel_keys();
						update_for_newframe_muted();
						
						remove_marker();
						
						allqueue(REDRAWMARKER, 0);
					}
				}
				break;
				
				/* LEFTMOUSE and RIGHTMOUSE event codes can be swapped above,
				* based on user preference USER_LMOUSESELECT
				*/
			case LEFTMOUSE:
				if(view2dmove(LEFTMOUSE))
					break; // only checks for sliders
				else if (mval[0]>=snla->v2d.mask.xmin) {
					do {
						getmouseco_areawin(mval);
						
						areamouseco_to_ipoco(G.v2d, mval, &dx, &dy);
						
						cfra= (int)(dx+0.5f);
						if(cfra< 1) cfra= 1;
						
						if( cfra!=CFRA ) {
							CFRA= cfra;
							update_for_newframe();
							force_draw_all(0);
						}
						else PIL_sleep_ms(30);
						
					} while(get_mbut() & mousebut);
					break;
				}
					/* else pass on! */
				case RIGHTMOUSE:
					if (mval[0]>=snla->v2d.mask.xmin) {
						if(G.qual & LR_SHIFTKEY)
							mouse_nla(SELECT_INVERT);
						else
							mouse_nla(SELECT_REPLACE);
					}
					else
						mouse_nlachannels(mval);
					break;
					
				case PADPLUSKEY:
					view2d_zoom(G.v2d, 0.1154, sa->winx, sa->winy);
					test_view2d(G.v2d, sa->winx, sa->winy);
					view2d_do_locks(curarea, V2D_LOCK_COPY);
					doredraw= 1;
					break;
				case PADMINUS:
					view2d_zoom(G.v2d, -0.15, sa->winx, sa->winy);
					test_view2d(G.v2d, sa->winx, sa->winy);
					view2d_do_locks(curarea, V2D_LOCK_COPY);
					doredraw= 1;
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

void bake_all_to_action(void)
{
	Object		*ob;
	bAction		*newAction;
	Ipo		*ipo;
	ID		*id;
	short		hold, add;
	float		repeat;

	/* burn object-level motion into a new action */
	ob = get_object_from_active_strip();
	if (ob) {
		if (ob->flag&OB_ARMATURE) {
			//newAction = bake_obIPO_to_action(ob);
			newAction = NULL;
			if (newAction) {
				/* unlink the object's IPO */
				ipo=ob->ipo;
				if (ipo) {
					id = &ipo->id;
					if (id->us > 0)
						id->us--;
					ob->ipo = NULL;
				}
				
				/* add the new Action to NLA as a strip */
				hold=1;
				add=1;
				repeat=1.0;
				printf("about to add nla block...\n");
				add_nla_block_by_name(newAction->id.name, ob, hold, add, repeat);
				BIF_undo_push("Add NLA strip");
			}
		}
	}
}

void copy_action_modifiers(void)
{
	bActionStrip *strip, *actstrip;
	Object *ob= OBACT;
	
	if(ob==NULL)
		return;
	
	/* active strip */
	for (actstrip=ob->nlastrips.first; actstrip; actstrip=actstrip->next)
		if(actstrip->flag & ACTSTRIP_ACTIVE)
			break;
	if(actstrip==NULL)
		return;
	
	/* copy to selected items */
	for (strip=ob->nlastrips.first; strip; strip=strip->next){
		if (strip->flag & ACTSTRIP_SELECT) {
			if(strip!=actstrip) {
				if (strip->modifiers.first)
					BLI_freelistN(&strip->modifiers);
				if (actstrip->modifiers.first)
					duplicatelist (&strip->modifiers, &actstrip->modifiers);
			}
		}
	}
	
	BIF_undo_push("Copy Action Modifiers");
	allqueue(REDRAWNLA, 0);
	DAG_scene_flush_update(G.scene, screen_view3d_layers(), 0);
}

