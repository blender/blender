/**
 * $Id: editaction.c 17746 2008-12-08 11:19:44Z aligorith $
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
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_listBase.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_ipo_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_constraint_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_userdef_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BKE_action.h"
#include "BKE_depsgraph.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_material.h"
#include "BKE_object.h"
#include "BKE_context.h"
#include "BKE_utildefines.h"

#include "UI_view2d.h"

#include "ED_anim_api.h"
#include "ED_keyframing.h"
#include "ED_keyframes_draw.h"
#include "ED_keyframes_edit.h"
#include "ED_screen.h"
#include "ED_space_api.h"

#include "WM_api.h"
#include "WM_types.h"

/* ************************************************************************** */
/* GENERAL STUFF */

#if 0
/* this function finds the channel that mouse is floating over */
void *get_nearest_act_channel (short mval[], short *ret_type, void **owner)
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	void *data;
	short datatype;
	int filter;
	
	int clickmin, clickmax;
	float x,y;
	
	/* init 'owner' return val */
	*owner= NULL;
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	if (data == NULL) {
		*ret_type= ACTTYPE_NONE;
		return NULL;
	}
	
    areamouseco_to_ipoco(G.v2d, mval, &x, &y);
	clickmin = (int) (((CHANNELHEIGHT/2) - y) / (CHANNELHEIGHT+CHANNELSKIP));
	clickmax = clickmin;
	
	if (clickmax < 0) {
		*ret_type= ACTTYPE_NONE;
		return NULL;
	}
	
	/* filter data */
	filter= (ACTFILTER_FORDRAWING | ACTFILTER_VISIBLE | ACTFILTER_CHANNELS);
	actdata_filter(&act_data, filter, data, datatype);
	
	for (ale= act_data.first; ale; ale= ale->next) {
		if (clickmax < 0) 
			break;
		if (clickmin <= 0) {
			/* found match */
			*ret_type= ale->type;
			data= ale->data;
			
			/* if an 'ID' has been set, this takes presidence as owner (for dopesheet) */
			if (datatype == ACTCONT_DOPESHEET) {
				/* return pointer to ID as owner instead */
				if (ale->id) 
					*owner= ale->id;
				else
					*owner= ale->owner;
			}
			else {
				/* just use own owner */
				*owner= ale->owner;
			}
			
			BLI_freelistN(&act_data);
			
			return data;
		}
		--clickmin;
		--clickmax;
	}
	
	/* cleanup */
	BLI_freelistN(&act_data);
	
	*ret_type= ACTTYPE_NONE;
	return NULL;
}
#endif

/* used only by mouse_action. It is used to find the location of the nearest 
 * keyframe to where the mouse clicked, 
 */
static void *get_nearest_action_key (bAnimContext *ac, int mval[2], float *selx, short *sel, short *ret_type, bActionChannel **par)
{
	ListBase anim_data = {NULL, NULL};
	ListBase anim_keys = {NULL, NULL};
	bAnimListElem *ale;
	ActKeyColumn *ak;
	View2D *v2d= &ac->ar->v2d;
	int filter;
	
	rctf rectf;
	void *data = NULL;
	float xmin, xmax, x, y;
	int clickmin, clickmax;
	short found = 0;
	
	/* action-channel */
	*par= NULL;
	
	UI_view2d_region_to_view(v2d, mval[0], mval[1], &x, &y);
    clickmin = (int) (((ACHANNEL_HEIGHT_HALF) - y) / (ACHANNEL_STEP)); // xxx max y-co (first) is -ACHANNEL_HEIGHT
	clickmax = clickmin;
	
	/* x-range to check is +/- 7 on either side of mouse click (size of keyframe icon) */
	UI_view2d_region_to_view(v2d, mval[0]-7, mval[1], &rectf.xmin, &rectf.ymin);
	UI_view2d_region_to_view(v2d, mval[0]+7, mval[1], &rectf.xmax, &rectf.ymax);
	
	if (clickmax < 0) {
		*ret_type= ANIMTYPE_NONE;
		return NULL;
	}
	
	/* filter data */
	filter= (ANIMFILTER_FORDRAWING | ANIMFILTER_VISIBLE | ANIMFILTER_CHANNELS);
	ANIM_animdata_filter(&anim_data, filter, ac->data, ac->datatype);
	
	for (ale= anim_data.first; ale; ale= ale->next) {
		if (clickmax < 0) 
			break;
		if (clickmin <= 0) {
			/* found match - must return here... */
			Object *nob= ANIM_nla_mapping_get(ac, ale);
			
			/* apply NLA-scaling correction? */
			if (nob) {
				xmin= get_action_frame(nob, rectf.xmin);
				xmax= get_action_frame(nob, rectf.xmax);
			}
			else {
				xmin= rectf.xmin;
				xmax= rectf.xmax;
			}
			
			/* make list of keyframes */
			if (ale->key_data) {
				switch (ale->datatype) {
					case ALE_OB:
					{
						Object *ob= (Object *)ale->key_data;
						ob_to_keylist(ob, &anim_keys, NULL, NULL);
					}
						break;
					case ALE_ACT:
					{
						bAction *act= (bAction *)ale->key_data;
						action_to_keylist(act, &anim_keys, NULL, NULL);
					}
						break;
					case ALE_IPO:
					{
						Ipo *ipo= (Ipo *)ale->key_data;
						ipo_to_keylist(ipo, &anim_keys, NULL, NULL);
					}
						break;
					case ALE_ICU:
					{
						IpoCurve *icu= (IpoCurve *)ale->key_data;
						icu_to_keylist(icu, &anim_keys, NULL, NULL);
					}
						break;
				}
			}
			else if (ale->type == ANIMTYPE_GROUP) {
				bActionGroup *agrp= (bActionGroup *)ale->data;
				agroup_to_keylist(agrp, &anim_keys, NULL, NULL);
			}
			else if (ale->type == ANIMTYPE_GPDATABLOCK) {
				/* cleanup */
				BLI_freelistN(&anim_data);
				
				/* this channel currently doens't have any keyframes... must ignore! */
				*ret_type= ANIMTYPE_NONE;
				return NULL;
			}
			else if (ale->type == ANIMTYPE_GPLAYER) {
				bGPDlayer *gpl= (bGPDlayer *)ale->data;
				gpl_to_keylist(gpl, &anim_keys, NULL, NULL);
			}
			
			/* loop through keyframes, finding one that was clicked on */
			for (ak= anim_keys.first; ak; ak= ak->next) {
				if (IN_RANGE(ak->cfra, xmin, xmax)) {
					*selx= ak->cfra;
					found= 1;
					break;
				}
			}
			/* no matching keyframe found - set to mean frame value so it doesn't actually select anything */
			if (found == 0)
				*selx= ((xmax+xmin) / 2);
			
			/* figure out what to return */
			if (ac->datatype == ANIMCONT_ACTION) {
				*par= ale->owner; /* assume that this is an action channel */
				*ret_type= ale->type;
				data = ale->data;
			}
			else if (ac->datatype == ANIMCONT_SHAPEKEY) {
				data = ale->key_data;
				*ret_type= ANIMTYPE_ICU;
			}
			else if (ac->datatype == ANIMCONT_DOPESHEET) {
				data = ale->data;
				*ret_type= ale->type;
			}
			else if (ac->datatype == ANIMCONT_GPENCIL) {
				data = ale->data;
				*ret_type= ANIMTYPE_GPLAYER;
			}
			
			/* cleanup tempolary lists */
			BLI_freelistN(&anim_keys);
			anim_keys.first = anim_keys.last = NULL;
			
			BLI_freelistN(&anim_data);
			
			return data;
		}
		--clickmin;
		--clickmax;
	}
	
	/* cleanup */
	BLI_freelistN(&anim_data);
	
	*ret_type= ANIMTYPE_NONE;
	return NULL;
}

/* ************************************************************************** */
/* CHANNEL STUFF */

/* ************************************************************************** */
/* KEYFRAMES STUFF */

/* ******************** Column Select Operator **************************** */

/* ******************** Mouse-Click Select Operator *********************** */
/* This operator works in one of four ways:
 *	- main area
 *		-> 1) without alt-key - selects keyframe that was under mouse position
 *		-> 2) with alt-key - only those keyframes on same side of current frame
 *	- 3) horizontal scroller (*) - select all keyframes in frame (err... maybe integrate this with column select only)?
 *	- 4) vertical scroller (*) - select all keyframes in channel
 *
 *	(*) - these are not obviously presented in UI. We need to find a new way to showcase them.
 */

/* option 1) select keyframe directly under mouse */
static void mouse_action_keys (bAnimContext *ac, int mval[2], short selectmode)
{
	Object *ob= NULL;
	bDopeSheet *ads= NULL;
	bAction	*act= NULL;
	bActionGroup *agrp= NULL;
	bActionChannel *achan= NULL;
	bConstraintChannel *conchan= NULL;
	Ipo *ipo= NULL;
	IpoCurve *icu= NULL;
	bGPdata *gpd = NULL;
	bGPDlayer *gpl = NULL;
	
	void *act_channel;
	short sel, act_type = 0;
	float selx = 0.0f, selxa;
	
	/* determine what type of data we are operating on */
	if (ac->datatype == ANIMCONT_ACTION) 
		act= (bAction *)ac->data;
	else if (ac->datatype == ANIMCONT_DOPESHEET) 
		ads= (bDopeSheet *)ac->data;
	else if (ac->datatype == ANIMCONT_GPENCIL) 
		gpd= (bGPdata *)ac->data;

	act_channel= get_nearest_action_key(ac, mval, &selx, &sel, &act_type, &achan);
	if (act_channel) {
		/* must have been a channel */
		switch (act_type) {
			case ANIMTYPE_ICU:
				icu= (IpoCurve *)act_channel;
				break;
			case ANIMTYPE_CONCHAN:
				conchan= (bConstraintChannel *)act_channel;
				break;
			case ANIMTYPE_ACHAN:
				achan= (bActionChannel *)act_channel;
				break;
			case ANIMTYPE_GROUP:
				agrp= (bActionGroup *)act_channel;
				break;
			case ANIMTYPE_DSMAT:
				ipo= ((Material *)act_channel)->ipo;
				break;
			case ANIMTYPE_DSLAM:
				ipo= ((Lamp *)act_channel)->ipo;
				break;
			case ANIMTYPE_DSCAM:
				ipo= ((Camera *)act_channel)->ipo;
				break;
			case ANIMTYPE_DSCUR:
				ipo= ((Curve *)act_channel)->ipo;
				break;
			case ANIMTYPE_DSSKEY:
				ipo= ((Key *)act_channel)->ipo;
				break;
			case ANIMTYPE_FILLACTD:
				act= (bAction *)act_channel;
				break;
			case ANIMTYPE_FILLIPOD:
				ipo= ((Object *)act_channel)->ipo;
				break;
			case ANIMTYPE_OBJECT:
				ob= ((Base *)act_channel)->object;
				break;
			case ANIMTYPE_GPLAYER:
				gpl= (bGPDlayer *)act_channel;
				break;
			default:
				return;
		}
		
		if (selectmode == SELECT_REPLACE) {
			selectmode = SELECT_ADD;
			
			//deselect_action_keys(0, 0); // XXX fixme
			
			if (ELEM(ac->datatype, ANIMCONT_ACTION, ANIMCONT_DOPESHEET)) {
				//deselect_action_channels(0);
				
				/* Highlight either an Action-Channel or Action-Group */
				if (achan) {
					achan->flag |= ACHAN_SELECTED;
					//hilight_channel(act, achan, 1);
					//select_poseelement_by_name(achan->name, 2);	/* 2 is activate */
				}
				else if (agrp) {
					agrp->flag |= AGRP_SELECTED;
					//set_active_actiongroup(act, agrp, 1);
				}
			}
			else if (ac->datatype == ANIMCONT_GPENCIL) {
				//deselect_action_channels(0);
				
				/* Highlight gpencil layer */
				gpl->flag |= GP_LAYER_SELECT;
				//gpencil_layer_setactive(gpd, gpl);
			}
		}
		
		if (icu)
			select_icu_key(ac->scene, icu, selx, selectmode);
		else if (ipo)
			select_ipo_key(ac->scene, ipo, selx, selectmode);
		else if (conchan)
			select_ipo_key(ac->scene, conchan->ipo, selx, selectmode);
		else if (achan)
			select_ipo_key(ac->scene, achan->ipo, selx, selectmode);
		else if (agrp) {
			for (achan= agrp->channels.first; achan && achan->grp==agrp; achan= achan->next) {
				select_ipo_key(ac->scene, achan->ipo, selx, selectmode);
				
				for (conchan=achan->constraintChannels.first; conchan; conchan=conchan->next)
					select_ipo_key(ac->scene, conchan->ipo, selx, selectmode);
			}
		}
		else if (act) {
			for (achan= act->chanbase.first; achan; achan= achan->next) {
				select_ipo_key(ac->scene, achan->ipo, selx, selectmode);
				
				for (conchan=achan->constraintChannels.first; conchan; conchan=conchan->next)
					select_ipo_key(ac->scene, conchan->ipo, selx, selectmode);
			}
		}
		else if (ob) {
			if (ob->ipo) 
				select_ipo_key(ac->scene, ob->ipo, selx, selectmode);
			
			if (ob->action) {
				selxa= get_action_frame(ob, selx);
				
				for (achan= ob->action->chanbase.first; achan; achan= achan->next) {
					select_ipo_key(ac->scene, achan->ipo, selxa, selectmode);
					
					for (conchan=achan->constraintChannels.first; conchan; conchan=conchan->next)
						select_ipo_key(ac->scene, conchan->ipo, selxa, selectmode);
				}
			}
			
			for (conchan=ob->constraintChannels.first; conchan; conchan=conchan->next)
				select_ipo_key(ac->scene, conchan->ipo, selx, selectmode);
		}
		//else if (gpl)
		//	select_gpencil_frame(gpl, (int)selx, selectmode);
	}
}
 
/* ------------------- */

static int actkeys_clickselect_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	bAnimContext ac;
	ARegion *ar;
	short in_scroller, selectmode;
	int mval[2];
	
	puts("Action click select invoke");
	
	/* get editor data */
	if ((ANIM_animdata_get_context(C, &ac) == 0) || (ac.data == NULL))
		return OPERATOR_CANCELLED;
		
	/* get useful pointers from animation context data */
	ar= ac.ar;
	
	/* get mouse coordinates (in region coordinates) */
	mval[0]= (event->x - ar->winrct.xmin);
	mval[1]= (event->y - ar->winrct.ymin);
	
	/* check where in view mouse is */
	in_scroller = UI_view2d_mouse_in_scrollers(C, &ar->v2d, event->x, event->y);
	
	/* select mode is either replace (deselect all, then add) or add/extend */
	if (RNA_boolean_get(op->ptr, "extend_select"))
		selectmode= SELECT_ADD;
	else
		selectmode= SELECT_REPLACE;
	
	/* check which scroller mouse is in, and figure out how to handle this */
	if (in_scroller == 'h') {
		/* horizontal - column select in current frame */
		// FIXME.... todo
	}
	else if (in_scroller == 'v') {
		/* vertical - row select in current channel */
		// FIXME... 
	}
	else if (RNA_boolean_get(op->ptr, "left_right")) {
		/* select all keys on same side of current frame as mouse */
		
	}
	else {
		/* select keyframe under mouse */
		mouse_action_keys(&ac, mval, selectmode);
		// XXX activate transform...
	}
	
	/* set notifier tha things have changed */
	ED_area_tag_redraw(CTX_wm_area(C)); // FIXME... should be updating 'keyframes' data context or so instead!
	
	return OPERATOR_FINISHED;
}
 
void ED_ACT_OT_keyframes_clickselect (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Mouse Select Keys";
	ot->idname= "ED_ACT_OT_keyframes_clickselect";
	
	/* api callbacks */
	ot->invoke= actkeys_clickselect_invoke;
	//ot->poll= ED_operator_areaactive;
	
	/* id-props */
	RNA_def_property(ot->srna, "left_right", PROP_BOOLEAN, PROP_NONE); // ALTKEY
	RNA_def_property(ot->srna, "extend_select", PROP_BOOLEAN, PROP_NONE); // SHIFTKEY
}

/* ************************************************************************** */
