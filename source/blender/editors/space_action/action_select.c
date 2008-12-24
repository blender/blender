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
		*ret_type= ANIMTYPE_NONE;
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
    clickmin = (int) ((-y) / (ACHANNEL_STEP));
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

/* ******************** Deselect All Operator ***************************** */
/* This operator works in one of three ways:
 *	1) (de)select all (AKEY) - test if select all or deselect all
 *	2) invert all (CTRL-IKEY) - invert selection of all keyframes
 *	3) (de)select all - no testing is done; only for use internal tools as normal function...
 */

/* Deselects keyframes in the action editor
 *	- This is called by the deselect all operator, as well as other ones!
 *
 * 	- test: check if select or deselect all
 *	- sel: how to select keyframes 
 *		0 = deselect
 *		1 = select
 *		2 = invert
 */
static void deselect_action_keys (bAnimContext *ac, short test, short sel)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* determine type-based settings */
	if (ac->datatype == ANIMCONT_GPENCIL)
		filter= (ANIMFILTER_VISIBLE);
	else
		filter= (ANIMFILTER_VISIBLE | ANIMFILTER_IPOKEYS);
	
	/* filter data */
	ANIM_animdata_filter(&anim_data, filter, ac->data, ac->datatype);
	
	/* See if we should be selecting or deselecting */
	if (test) {
		for (ale= anim_data.first; ale; ale= ale->next) {
			if (ale->type == ANIMTYPE_GPLAYER) {
				//if (is_gplayer_frame_selected(ale->data)) {
				//	sel= 0;
				//	break;
				//}
			}
			else {
				if (is_ipo_key_selected(ale->key_data)) {
					sel= 0;
					break;
				}
			}
		}
	}
		
	/* Now set the flags */
	for (ale= anim_data.first; ale; ale= ale->next) {
		//if (ale->type == ACTTYPE_GPLAYER)
		//	set_gplayer_frame_selection(ale->data, sel);
		//else
			set_ipo_key_selection(ale->key_data, sel);
	}
	
	/* Cleanup */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int actkeys_deselectall_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	/* get editor data */
	if ((ANIM_animdata_get_context(C, &ac) == 0) || (ac.data == NULL))
		return OPERATOR_CANCELLED;
		
	/* 'standard' behaviour - check if selected, then apply relevant selection */
	if (RNA_boolean_get(op->ptr, "invert"))
		deselect_action_keys(&ac, 0, 2);
	else
		deselect_action_keys(&ac, 1, 1);
	
	/* set notifier tha things have changed */
	ED_area_tag_redraw(CTX_wm_area(C)); // FIXME... should be updating 'keyframes' data context or so instead!
	
	return OPERATOR_FINISHED;
}
 
void ED_ACT_OT_keyframes_deselectall (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select All";
	ot->idname= "ED_ACT_OT_keyframes_deselectall";
	
	/* api callbacks */
	ot->exec= actkeys_deselectall_exec;
	ot->poll= ED_operator_areaactive;
	
	/* props */
	RNA_def_property(ot->srna, "invert", PROP_BOOLEAN, PROP_NONE);
}

/* ******************** Border Select Operator **************************** */
/* This operator works in one of three ways:
 *	1) borderselect over keys (BKEY) - mouse over main area when initialised; will select keys in region
 *	2) borderselect over horizontal scroller - mouse over horizontal scroller when initialised; will select keys in frame range
 *	3) borderselect over vertical scroller - mouse over vertical scroller when initialised; will select keys in row range
 */

static void borderselect_action (bAnimContext *ac, rcti rect, short in_scroller, short selectmode)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	View2D *v2d= &ac->ar->v2d;
	BeztEditFunc select_cb;
	rctf rectf;
	float ymin=0, ymax=ACHANNEL_HEIGHT;
	
	UI_view2d_region_to_view(v2d, rect.xmin, rect.ymin+2, &rectf.xmin, &rectf.ymin);
	UI_view2d_region_to_view(v2d, rect.xmax, rect.ymax-2, &rectf.xmax, &rectf.ymax);
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CHANNELS);
	ANIM_animdata_filter(&anim_data, filter, ac->data, ac->datatype);
	
	/* get selection editing func */
	select_cb= ANIM_editkeyframes_select(selectmode);
	
	/* loop over data, doing border select */
	for (ale= anim_data.first; ale; ale= ale->next) {
		Object *nob= ANIM_nla_mapping_get(ac, ale);
		
		ymin= ymax - ACHANNEL_STEP;
		
		/* if action is mapped in NLA, it returns a correction */
		if (nob) {
			rectf.xmin= get_action_frame(nob, rectf.xmin);
			rectf.xmax= get_action_frame(nob, rectf.xmax);
		}
		
		/* what gets selected depends on the mode (based on initial position of cursor) */
		switch (in_scroller) {
		case 'h': /* all in frame(s) (option 3) */
			if (ale->key_data) {
				if (ale->datatype == ALE_IPO)
					borderselect_ipo_key(ale->key_data, rectf.xmin, rectf.xmax, selectmode);
				else if (ale->datatype == ALE_ICU)
					borderselect_icu_key(ale->key_data, rectf.xmin, rectf.xmax, select_cb);
			}
			else if (ale->type == ANIMTYPE_GROUP) {
				bActionGroup *agrp= ale->data;
				bActionChannel *achan;
				bConstraintChannel *conchan;
				
				for (achan= agrp->channels.first; achan && achan->grp==agrp; achan= achan->next) {
					borderselect_ipo_key(achan->ipo, rectf.xmin, rectf.xmax, selectmode);
					
					for (conchan=achan->constraintChannels.first; conchan; conchan=conchan->next)
						borderselect_ipo_key(conchan->ipo, rectf.xmin, rectf.xmax, selectmode);
				}
			}
			//else if (ale->type == ANIMTYPE_GPLAYER) {
			//	borderselect_gplayer_frames(ale->data, rectf.xmin, rectf.xmax, selectmode);
			//}
			break;
		case 'v': /* all in channel(s) (option 2) */
			if (!((ymax < rectf.ymin) || (ymin > rectf.ymax))) {
				if (ale->key_data) {
					if (ale->datatype == ALE_IPO)
						ipo_keys_bezier_loop(ac->scene, ale->key_data, select_cb, NULL);
					else if (ale->datatype == ALE_ICU)
						icu_keys_bezier_loop(ac->scene, ale->key_data, select_cb, NULL);
				}
				else if (ale->type == ANIMTYPE_GROUP) {
					bActionGroup *agrp= ale->data;
					bActionChannel *achan;
					bConstraintChannel *conchan;
					
					for (achan= agrp->channels.first; achan && achan->grp==agrp; achan= achan->next) {
						ipo_keys_bezier_loop(ac->scene, achan->ipo, select_cb, NULL);
						
						for (conchan=achan->constraintChannels.first; conchan; conchan=conchan->next)
							ipo_keys_bezier_loop(ac->scene, conchan->ipo, select_cb, NULL);
					}
				}
				//else if (ale->type == ANIMTYPE_GPLAYER) {
				//	select_gpencil_frames(ale->data, selectmode);
				//}
			}
			break;
		default: /* any keyframe inside region defined by region (option 1) */
			if (!((ymax < rectf.ymin) || (ymin > rectf.ymax))) {
				if (ale->key_data) {
					if (ale->datatype == ALE_IPO)
						borderselect_ipo_key(ale->key_data, rectf.xmin, rectf.xmax, selectmode);
					else if (ale->datatype == ALE_ICU)
						borderselect_icu_key(ale->key_data, rectf.xmin, rectf.xmax, select_cb);
				}
				else if (ale->type == ANIMTYPE_GROUP) {
					// fixme: need a nicer way of dealing with summaries!
					bActionGroup *agrp= ale->data;
					bActionChannel *achan;
					bConstraintChannel *conchan;
					
					for (achan= agrp->channels.first; achan && achan->grp==agrp; achan= achan->next) {
						borderselect_ipo_key(achan->ipo, rectf.xmin, rectf.xmax, selectmode);
						
						for (conchan=achan->constraintChannels.first; conchan; conchan=conchan->next)
							borderselect_ipo_key(conchan->ipo, rectf.xmin, rectf.xmax, selectmode);
					}
				}
				//else if (ale->type == ANIMTYPE_GPLAYER) {
				////	borderselect_gplayer_frames(ale->data, rectf.xmin, rectf.xmax, selectmode);
				//}
			}
		}
		
		/* if action is mapped in NLA, unapply correction */
		if (nob) {
			rectf.xmin= get_action_frame_inv(nob, rectf.xmin);
			rectf.xmax= get_action_frame_inv(nob, rectf.xmax);
		}
		
		ymax=ymin;
	}
	
	/* cleanup */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int actkeys_borderselect_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	rcti rect;
	short in_scroller, selectmode;
	int event;
	
	/* get editor data */
	if ((ANIM_animdata_get_context(C, &ac) == 0) || (ac.data == NULL))
		return OPERATOR_CANCELLED;
	
	/* get settings from operator */
	rect.xmin= RNA_int_get(op->ptr, "xmin");
	rect.ymin= RNA_int_get(op->ptr, "ymin");
	rect.xmax= RNA_int_get(op->ptr, "xmax");
	rect.ymax= RNA_int_get(op->ptr, "ymax");
	
	in_scroller= RNA_int_get(op->ptr, "in_scroller");
	
	event= RNA_int_get(op->ptr, "event_type");
	if (event == LEFTMOUSE) // FIXME... hardcoded
		selectmode = SELECT_ADD;
	else
		selectmode = SELECT_SUBTRACT;
		
	borderselect_action(&ac, rect, in_scroller, selectmode);
	
	return OPERATOR_FINISHED;
} 

static int actkeys_borderselect_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	bAnimContext ac;
	ARegion *ar;
	
	/* get editor data */
	if ((ANIM_animdata_get_context(C, &ac) == 0) || (ac.data == NULL))
		return OPERATOR_CANCELLED;
	ar= ac.ar;
		
	/* check if mouse is in a scroller */
	// XXX move this to keymap level thing (boundbox checking)?
	RNA_enum_set(op->ptr, "in_scroller", UI_view2d_mouse_in_scrollers(C, &ar->v2d, event->x, event->y));
	
	/* now init borderselect operator to handle borderselect as per normal */
	return WM_border_select_invoke(C, op, event);
}

void ED_ACT_OT_keyframes_borderselect(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Border Select";
	ot->idname= "ED_ACT_OT_keyframes_borderselect";
	
	/* api callbacks */
	ot->invoke= actkeys_borderselect_invoke;//WM_border_select_invoke;
	ot->exec= actkeys_borderselect_exec;
	ot->modal= WM_border_select_modal;
	
	ot->poll= ED_operator_areaactive;
	
	/* rna */
	RNA_def_property(ot->srna, "in_scroller", PROP_INT, PROP_NONE); // as enum instead?
	RNA_def_property(ot->srna, "event_type", PROP_INT, PROP_NONE);
	RNA_def_property(ot->srna, "xmin", PROP_INT, PROP_NONE);
	RNA_def_property(ot->srna, "xmax", PROP_INT, PROP_NONE);
	RNA_def_property(ot->srna, "ymin", PROP_INT, PROP_NONE);
	RNA_def_property(ot->srna, "ymax", PROP_INT, PROP_NONE);
}

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
	
	void *anim_channel;
	short sel, chan_type = 0;
	float selx = 0.0f, selxa;
	
	/* determine what type of data we are operating on */
	if (ac->datatype == ANIMCONT_ACTION) 
		act= (bAction *)ac->data;
	else if (ac->datatype == ANIMCONT_DOPESHEET) 
		ads= (bDopeSheet *)ac->data;
	else if (ac->datatype == ANIMCONT_GPENCIL) 
		gpd= (bGPdata *)ac->data;

	anim_channel= get_nearest_action_key(ac, mval, &selx, &sel, &chan_type, &achan);
	if (anim_channel) {
		/* must have been a channel */
		switch (chan_type) {
			case ANIMTYPE_ICU:
				icu= (IpoCurve *)anim_channel;
				break;
			case ANIMTYPE_CONCHAN:
				conchan= (bConstraintChannel *)anim_channel;
				break;
			case ANIMTYPE_ACHAN:
				achan= (bActionChannel *)anim_channel;
				break;
			case ANIMTYPE_GROUP:
				agrp= (bActionGroup *)anim_channel;
				break;
			case ANIMTYPE_DSMAT:
				ipo= ((Material *)anim_channel)->ipo;
				break;
			case ANIMTYPE_DSLAM:
				ipo= ((Lamp *)anim_channel)->ipo;
				break;
			case ANIMTYPE_DSCAM:
				ipo= ((Camera *)anim_channel)->ipo;
				break;
			case ANIMTYPE_DSCUR:
				ipo= ((Curve *)anim_channel)->ipo;
				break;
			case ANIMTYPE_DSSKEY:
				ipo= ((Key *)anim_channel)->ipo;
				break;
			case ANIMTYPE_FILLACTD:
				act= (bAction *)anim_channel;
				break;
			case ANIMTYPE_FILLIPOD:
				ipo= ((Object *)anim_channel)->ipo;
				break;
			case ANIMTYPE_OBJECT:
				ob= ((Base *)anim_channel)->object;
				break;
			case ANIMTYPE_GPLAYER:
				gpl= (bGPDlayer *)anim_channel;
				break;
			default:
				return;
		}
		
		if (selectmode == SELECT_REPLACE) {
			selectmode = SELECT_ADD;
			
			deselect_action_keys(ac, 0, 0);
			
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

/* Option 2) Selects all the keyframes on either side of the current frame (depends on which side the mouse is on) */
static void selectkeys_leftright (bAnimContext *ac, short leftright, short select_mode)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	Scene *scene= ac->scene;
	float min, max;
	
	if (select_mode==SELECT_REPLACE) {
		select_mode=SELECT_ADD;
		deselect_action_keys(ac, 0, 0);
	}
	
	if (leftright == 1) {
		min = -MAXFRAMEF;
		max = (float)(CFRA + 0.1f);
	} 
	else {
		min = (float)(CFRA - 0.1f);
		max = MAXFRAMEF;
	}
	
	/* filter data */
	if (ac->datatype == ANIMCONT_GPENCIL)
		filter= (ANIMFILTER_VISIBLE);
	else
		filter= (ANIMFILTER_VISIBLE | ANIMFILTER_IPOKEYS);
	ANIM_animdata_filter(&anim_data, filter, ac->data, ac->datatype);
		
	/* select keys on the side where most data occurs */
	for (ale= anim_data.first; ale; ale= ale->next) {
		Object *nob= ANIM_nla_mapping_get(ac, ale);
		
		if (nob) {
			ANIM_nla_mapping_apply(nob, ale->key_data, 0, 1);
			borderselect_ipo_key(ale->key_data, min, max, SELECT_ADD);
			ANIM_nla_mapping_apply(nob, ale->key_data, 1, 1);
		}
		//else if (ale->type == ANIMTYPE_GPLAYER)
		//	borderselect_gplayer_frames(ale->data, min, max, SELECT_ADD);
		else
			borderselect_ipo_key(ale->key_data, min, max, SELECT_ADD);
	}
	
	/* Cleanup */
	BLI_freelistN(&anim_data);
}
 
/* ------------------- */

static int actkeys_clickselect_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	bAnimContext ac;
	Scene *scene;
	ARegion *ar;
	short in_scroller, selectmode;
	int mval[2];
	
	/* get editor data */
	if ((ANIM_animdata_get_context(C, &ac) == 0) || (ac.data == NULL))
		return OPERATOR_CANCELLED;
		
	/* get useful pointers from animation context data */
	scene= ac.scene;
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
		selectkeys_leftright(&ac, (mval[0] < CFRA), selectmode);
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
