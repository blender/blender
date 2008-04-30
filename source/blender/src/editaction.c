/**
 * $Id$
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
 * Contributor(s): 2007, Joshua Leung (major rewrite of Action Editor)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stddef.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_ipo_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_constraint_types.h"
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_nla_types.h"
#include "DNA_lattice_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_utildefines.h"
#include "BKE_object.h" /* for where_is_object in obanim -> action baking */

#include "BIF_butspace.h"
#include "BIF_editaction.h"
#include "BIF_editarmature.h"
#include "BIF_editnla.h"
#include "BIF_editview.h"
#include "BIF_gl.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_poseobject.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"
#include "BIF_transform.h"

#include "BSE_edit.h"
#include "BSE_drawipo.h"
#include "BSE_headerbuttons.h"
#include "BSE_editaction_types.h"
#include "BSE_editipo.h"
#include "BSE_time.h"
#include "BSE_trans_types.h"

#include "BDR_drawaction.h"
#include "BDR_editobject.h"

#include "mydevice.h"
#include "blendef.h"
#include "nla.h"

/* **************************************************** */
/* ACTION API */

/* this function adds a new Action block */
bAction *add_empty_action (char *name)
{
	bAction *act;
	
	act= alloc_libblock(&G.main->action, ID_AC, name);
	act->id.flag |= LIB_FAKEUSER;
	act->id.us++;
	
	return act;
}

/* generic get current action call, for action window context */
bAction *ob_get_action (Object *ob)
{
	bActionStrip *strip;
	
	if(ob->action)
		return ob->action;
	
	for (strip=ob->nlastrips.first; strip; strip=strip->next) {
		if (strip->flag & ACTSTRIP_SELECT)
			return strip->act;
	}
	return NULL;
}

/* used by ipo, outliner, buttons to find the active channel */
bActionChannel *get_hilighted_action_channel (bAction *action)
{
	bActionChannel *achan;

	if (!action)
		return NULL;

	for (achan= action->chanbase.first; achan; achan= achan->next) {
		if (VISIBLE_ACHAN(achan)) {
			if (SEL_ACHAN(achan) && (achan->flag & ACHAN_HILIGHTED))
				return achan;
		}
	}

	return NULL;
}

/* ----------------------------------------- */

void remake_action_ipos (bAction *act)
{
	bActionChannel *achan;
	bConstraintChannel *conchan;
	IpoCurve *icu;

	for (achan= act->chanbase.first; achan; achan= achan->next) {
		if (achan->ipo) {
			for (icu = achan->ipo->curve.first; icu; icu=icu->next) {
				sort_time_ipocurve(icu);
				testhandles_ipocurve(icu);
			}
		}
		for (conchan=achan->constraintChannels.first; conchan; conchan=conchan->next) {
			if (conchan->ipo) {
				for (icu = conchan->ipo->curve.first; icu; icu=icu->next) {
					sort_time_ipocurve(icu);
					testhandles_ipocurve(icu);
				}
			}
		}
	}
	
	synchronize_action_strips();
}

/* **************************************************** */
/* FILTER->EDIT STRUCTURES */
/* 
 * This method involves generating a list of edit structures which enable
 * tools to naively perform the actions they require without all the boiler-plate
 * associated with loops within loops and checking for cases to ignore. 
 */

/* this function allocates memory for a new bActListElem struct for the 
 * provided action channel-data. 
 */
bActListElem *make_new_actlistelem (void *data, short datatype, void *owner, short ownertype)
{
	bActListElem *ale= NULL;
	
	/* only allocate memory if there is data to convert */
	if (data) {
		/* allocate and set generic data */
		ale= MEM_callocN(sizeof(bActListElem), "bActListElem");
		
		ale->data= data;
		ale->type= datatype;
		ale->owner= owner;
		ale->ownertype= ownertype;
		
		if ((owner) && (ownertype == ACTTYPE_ACHAN)) {
			bActionChannel *ochan= (bActionChannel *)owner;
			ale->grp= ochan->grp;
		}
		else 
			ale->grp= NULL;
		
		/* do specifics */
		switch (datatype) {
			case ACTTYPE_GROUP:
			{
				bActionGroup *agrp= (bActionGroup *)data;
				
				ale->flag= agrp->flag;
				
				ale->key_data= NULL;
				ale->datatype= ALE_GROUP;
			}
				break;
			case ACTTYPE_ACHAN:
			{
				bActionChannel *achan= (bActionChannel *)data;
				
				ale->flag= achan->flag;
				
				if (achan->ipo) {
					ale->key_data= achan->ipo;
					ale->datatype= ALE_IPO;
				}
				else {
					ale->key_data= NULL;
					ale->datatype= ALE_NONE;
				}
			}	
				break;
			case ACTTYPE_CONCHAN:
			case ACTTYPE_CONCHAN2:
			{
				bConstraintChannel *conchan= (bConstraintChannel *)data;
				
				ale->flag= conchan->flag;
				
				if (datatype == ACTTYPE_CONCHAN2) {
					/* CONCHAN2 is a hack so that constraint-channels keyframes can be edited */
					if (conchan->ipo) {
						ale->key_data= conchan->ipo;
						ale->datatype= ALE_IPO;
					}
					else {
						ale->key_data= NULL;
						ale->datatype= ALE_NONE;
					}
				}
				else {
					if (conchan->ipo && conchan->ipo->curve.first) {
						/* we assume that constraint ipo blocks only have 1 curve:
						 * INFLUENCE, so we pretend that a constraint channel is 
						 * really just a Ipo-Curve channel instead.
						 */
						ale->key_data= conchan->ipo->curve.first;
						ale->datatype= ALE_ICU;
					}
					else {
						ale->key_data= NULL;
						ale->datatype= ALE_NONE;
					}
				}
			}
				break;
			case ACTTYPE_ICU:
			{
				IpoCurve *icu= (IpoCurve *)data;
				
				ale->flag= icu->flag;
				ale->key_data= icu;
				ale->datatype= ALE_ICU;
			}
				break;
				
			case ACTTYPE_FILLIPO:
			case ACTTYPE_FILLCON:
			{
				bActionChannel *achan= (bActionChannel *)data;
				
				if (datatype == ACTTYPE_FILLIPO)
					ale->flag= FILTER_IPO_ACHAN(achan);
				else
					ale->flag= FILTER_CON_ACHAN(achan);
					
				ale->key_data= NULL;
				ale->datatype= ALE_NONE;
			}
				break;
			case ACTTYPE_IPO:
			{
				ale->flag= 0;
				ale->key_data= data;
				ale->datatype= ALE_IPO;
			}
				break;
		}
	}
	
	/* return created datatype */
	return ale;
}
 
/* ----------------------------------------- */

static void actdata_filter_actionchannel (ListBase *act_data, bActionChannel *achan, int filter_mode)
{
	bActListElem *ale;
	bConstraintChannel *conchan;
	IpoCurve *icu;
	
	/* only work with this channel and its subchannels if it is visible */
	if (!(filter_mode & ACTFILTER_VISIBLE) || VISIBLE_ACHAN(achan)) {
		/* only work with this channel and its subchannels if it is editable */
		if (!(filter_mode & ACTFILTER_FOREDIT) || EDITABLE_ACHAN(achan)) {
			/* check if this achan should only be included if it is selected */
			if (!(filter_mode & ACTFILTER_SEL) || SEL_ACHAN(achan)) {
				/* are we only interested in the ipo-curves? */
				if ((filter_mode & ACTFILTER_ONLYICU)==0) {
					ale= make_new_actlistelem(achan, ACTTYPE_ACHAN, achan, ACTTYPE_ACHAN);
					if (ale) BLI_addtail(act_data, ale);
				}
			}
			else {
				/* for insert key... this check could be improved */
				return;
			}
			
			/* check if expanded - if not, continue on to next action channel */
			if (EXPANDED_ACHAN(achan) == 0 && (filter_mode & ACTFILTER_ONLYICU)==0) {
				/* only exit if we don't need to include constraint channels for group-channel keyframes */
				if ( !(filter_mode & ACTFILTER_IPOKEYS) || (achan->grp == NULL) || (EXPANDED_AGRP(achan->grp)==0) )
					return;
			}
				
			/* ipo channels */
			if ((achan->ipo) && (filter_mode & ACTFILTER_IPOKEYS)==0) {
				/* include ipo-expand widget? */
				if ((filter_mode & ACTFILTER_CHANNELS) && (filter_mode & ACTFILTER_ONLYICU)==0) {
					ale= make_new_actlistelem(achan, ACTTYPE_FILLIPO, achan, ACTTYPE_ACHAN);
					if (ale) BLI_addtail(act_data, ale);
				}
				
				/* add ipo-curve channels? */
				if (FILTER_IPO_ACHAN(achan) || (filter_mode & ACTFILTER_ONLYICU)) {
					/* loop through ipo-curve channels, adding them */
					for (icu= achan->ipo->curve.first; icu; icu=icu->next) {
						ale= make_new_actlistelem(icu, ACTTYPE_ICU, achan, ACTTYPE_ACHAN);
						if (ale) BLI_addtail(act_data, ale); 
					}
				}
			}
			
			/* constraint channels */
			if (achan->constraintChannels.first) {
				/* include constraint-expand widget? */
				if ( (filter_mode & ACTFILTER_CHANNELS) && !(filter_mode & ACTFILTER_ONLYICU)
					 && !(filter_mode & ACTFILTER_IPOKEYS) ) 
				{
					ale= make_new_actlistelem(achan, ACTTYPE_FILLCON, achan, ACTTYPE_ACHAN);
					if (ale) BLI_addtail(act_data, ale);
				}
				
				/* add constraint channels? */
				if (FILTER_CON_ACHAN(achan) || (filter_mode & ACTFILTER_IPOKEYS) || (filter_mode & ACTFILTER_ONLYICU)) {
					/* loop through constraint channels, checking and adding them */
					for (conchan=achan->constraintChannels.first; conchan; conchan=conchan->next) {
						/* only work with this channel and its subchannels if it is editable */
						if (!(filter_mode & ACTFILTER_FOREDIT) || EDITABLE_CONCHAN(conchan)) {
							/* check if this conchan should only be included if it is selected */
							if (!(filter_mode & ACTFILTER_SEL) || SEL_CONCHAN(conchan)) {
								if (filter_mode & ACTFILTER_IPOKEYS) {
									ale= make_new_actlistelem(conchan, ACTTYPE_CONCHAN2, achan, ACTTYPE_ACHAN);
									if (ale) BLI_addtail(act_data, ale);
								}
								else {
									ale= make_new_actlistelem(conchan, ACTTYPE_CONCHAN, achan, ACTTYPE_ACHAN);
									if (ale) BLI_addtail(act_data, ale);
								}
							}
						}
					}
				}
			}
		}		
	}
}

static void actdata_filter_action (ListBase *act_data, bAction *act, int filter_mode)
{
	bActListElem *ale;
	bActionGroup *agrp;
	bActionChannel *achan, *lastchan=NULL;
	
	/* loop over groups */
	for (agrp= act->groups.first; agrp; agrp= agrp->next) {
		/* add this group as a channel first */
		if (!(filter_mode & ACTFILTER_ONLYICU) && !(filter_mode & ACTFILTER_IPOKEYS)) {
			/* check if filtering by selection */
			if ( !(filter_mode & ACTFILTER_SEL) || SEL_AGRP(agrp) ) {
				ale= make_new_actlistelem(agrp, ACTTYPE_GROUP, NULL, ACTTYPE_NONE);
				if (ale) BLI_addtail(act_data, ale);
			}
		}
		
		/* store reference to last channel of group */
		if (agrp->channels.last) 
			lastchan= agrp->channels.last;
		
		
		/* there are some situations, where only the channels of the active group should get considered */
		if (!(filter_mode & ACTFILTER_ACTGROUPED) || (agrp->flag & AGRP_ACTIVE)) {
			/* filters here are a bit convoulted...
			 *	- groups show a "summary" of keyframes beside their name which must accessable for tools which handle keyframes
			 *	- groups can be collapsed (and those tools which are only interested in channels rely on knowing that group is closed)
			 *
			 * cases when we should include action-channels and so-forth inside group:
			 *	- we don't care about visibility
			 *	- group is expanded
			 *	- we're interested in keyframes, but not if they appear in selected channels
			 */
			if ( (!(filter_mode & ACTFILTER_VISIBLE) || EXPANDED_AGRP(agrp)) || 
				 ( ((filter_mode & ACTFILTER_IPOKEYS) || (filter_mode & ACTFILTER_ONLYICU)) && 
				  !(filter_mode & ACTFILTER_SEL) ) ) 
			{
				if (!(filter_mode & ACTFILTER_FOREDIT) || EDITABLE_AGRP(agrp)) {					
					for (achan= agrp->channels.first; achan && achan->grp==agrp; achan= achan->next) {
						actdata_filter_actionchannel(act_data, achan, filter_mode);
					}
				}
			}
		}
	}
	
	/* loop over un-grouped action channels (only if we're not only considering those channels in the active group) */
	if (!(filter_mode & ACTFILTER_ACTGROUPED))  {
		for (achan=(lastchan)?lastchan->next:act->chanbase.first; achan; achan=achan->next) {
			actdata_filter_actionchannel(act_data, achan, filter_mode);
		}
	}
}

static void actdata_filter_shapekey (ListBase *act_data, Key *key, int filter_mode)
{
	bActListElem *ale;
	KeyBlock *kb;
	IpoCurve *icu;
	int i;
	
	/* are we filtering for display or editing */
	if (filter_mode & ACTFILTER_FORDRAWING) {
		/* for display - loop over shapekeys, adding ipo-curve references where needed */
		kb= key->block.first;
		
		/* loop through possible shapekeys, manually creating entries */
		for (i= 1; i < key->totkey; i++) {
			ale= MEM_callocN(sizeof(bActListElem), "bActListElem");
			kb = kb->next;
			
			ale->data= kb;
			ale->type= ACTTYPE_SHAPEKEY; /* 'abused' usage of this type */
			ale->owner= key;
			ale->ownertype= ACTTYPE_SHAPEKEY;
			ale->datatype= ALE_NONE;
			ale->index = i;
			
			if (key->ipo) {
				for (icu= key->ipo->curve.first; icu; icu=icu->next) {
					if (icu->adrcode == i) {
						ale->key_data= icu;
						ale->datatype= ALE_ICU;
						break;
					}
				}
			}
			
			BLI_addtail(act_data, ale);
		}
	}
	else {
		/* loop over ipo curves if present - for editing */
		if (key->ipo) {
			if (filter_mode & ACTFILTER_IPOKEYS) {
				ale= make_new_actlistelem(key->ipo, ACTTYPE_IPO, key, ACTTYPE_SHAPEKEY);
				if (ale) BLI_addtail(act_data, ale);
			}
			else {
				for (icu= key->ipo->curve.first; icu; icu=icu->next) {
					ale= make_new_actlistelem(icu, ACTTYPE_ICU, key, ACTTYPE_SHAPEKEY);
					if (ale) BLI_addtail(act_data, ale);
				}
			}
		}
	}
}
 
/* This function filters the active data source to leave only the desired
 * data types. 'Public' api call.
 * 	*act_data: is a pointer to a ListBase, to which the filtered action data 
 *		will be placed for use.
 *	filter_mode: how should the data be filtered - bitmapping accessed flags
 */
void actdata_filter (ListBase *act_data, int filter_mode, void *data, short datatype)
{
	/* only filter data if there's somewhere to put it */
	if (data && act_data) {
		bActListElem *ale, *next;
		
		/* firstly filter the data */
		switch (datatype) {
			case ACTCONT_ACTION:
				actdata_filter_action(act_data, data, filter_mode);
				break;
			case ACTCONT_SHAPEKEY:
				actdata_filter_shapekey(act_data, data, filter_mode);
				break;
		}
			
		/* remove any weedy entries */
		for (ale= act_data->first; ale; ale= next) {
			next= ale->next;
			
			if (ale->type == ACTTYPE_NONE)
				BLI_freelinkN(act_data, ale);
			
			if (filter_mode & ACTFILTER_IPOKEYS) {
				if (ale->datatype != ALE_IPO)
					BLI_freelinkN(act_data, ale);
				else if (ale->key_data == NULL)
					BLI_freelinkN(act_data, ale);
			}
		}
	}
}

/* **************************************************** */
/* GENERAL ACTION TOOLS */

/* gets the key data from the currently selected
 * mesh/lattice. If a mesh is not selected, or does not have
 * key data, then we return NULL (currently only
 * returns key data for RVK type meshes). If there
 * is an action that is pinned, return null
 */
/* Note: there's a similar function in key.c (ob_get_key) */
Key *get_action_mesh_key(void) 
{
    Object *ob;
    Key    *key;

    ob = OBACT;
    if (ob == NULL) 
		return NULL;

	if (G.saction->pin) return NULL;

    if (ob->type==OB_MESH) 
		key = ((Mesh *)ob->data)->key;
	else if (ob->type==OB_LATTICE) 
		key = ((Lattice *)ob->data)->key;
	else if (ELEM(ob->type, OB_CURVE, OB_SURF))
		key= ((Curve *)ob->data)->key;
	else 
		return NULL;

	if (key) {
		if (key->type == KEY_RELATIVE)
			return key;
	}

    return NULL;
}

/* TODO: kill this! */
int get_nearest_key_num (Key *key, short *mval, float *x) 
{
	/* returns the key num that cooresponds to the
	 * y value of the mouse click. Does not check
	 * if this is a valid keynum. Also gives the Ipo
	 * x coordinate.
	 */
    int num;
    float y;

    areamouseco_to_ipoco(G.v2d, mval, x, &y);
    num = (int) ((CHANNELHEIGHT/2 - y) / (CHANNELHEIGHT+CHANNELSKIP));

    return (num + 1);
}

/* this function is used to get a pointer to an action or shapekey 
 * datablock, thus simplying that process.
 */
/* this function is intended for use */
void *get_nearest_act_channel (short mval[], short *ret_type)
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	void *data;
	short datatype;
	int filter;
	
	int clickmin, clickmax;
	float x,y;
	
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

/* used only by mouse_action. It is used to find the location of the nearest 
 * keyframe to where the mouse clicked, 
 */
static void *get_nearest_action_key (float *selx, short *sel, short *ret_type, bActionChannel **par)
{
	ListBase act_data = {NULL, NULL};
	ListBase act_keys = {NULL, NULL};
	bActListElem *ale;
	ActKeyColumn *ak;
	void *data;
	short datatype;
	int filter;
	
	rctf rectf;
	float xmin, xmax, x, y;
	int clickmin, clickmax;
	short mval[2];
	short found = 0;
		
	getmouseco_areawin (mval);

	/* action-channel */
	*par= NULL;
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	if (data == NULL) {
		*ret_type= ACTTYPE_NONE;
		return NULL;
	}

    areamouseco_to_ipoco(G.v2d, mval, &x, &y);
    clickmin = (int) (((CHANNELHEIGHT/2) - y) / (CHANNELHEIGHT+CHANNELSKIP));
	clickmax = clickmin;
	
	mval[0]-=7;
	areamouseco_to_ipoco(G.v2d, mval, &rectf.xmin, &rectf.ymin);
	mval[0]+=14;
	areamouseco_to_ipoco(G.v2d, mval, &rectf.xmax, &rectf.ymax);

	/* if action is mapped in NLA, it returns a correction */
	if (NLA_ACTION_SCALED && datatype==ACTCONT_ACTION) {
		xmin= get_action_frame(OBACT, rectf.xmin);
		xmax= get_action_frame(OBACT, rectf.xmax);
	}
	else {
		xmin= rectf.xmin;
		xmax= rectf.xmax;
	}
	
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
			
			/* make list of keyframes */
			if (ale->key_data) {
				switch (ale->datatype) {
					case ALE_IPO:
					{
						Ipo *ipo= (Ipo *)ale->key_data;
						ipo_to_keylist(ipo, &act_keys, NULL, NULL);
					}
						break;
					case ALE_ICU:
					{
						IpoCurve *icu= (IpoCurve *)ale->key_data;
						icu_to_keylist(icu, &act_keys, NULL, NULL);
					}
						break;
				}
			}
			else if (ale->type == ACTTYPE_GROUP) {
				bActionGroup *agrp= (bActionGroup *)ale->data;
				agroup_to_keylist(agrp, &act_keys, NULL, NULL);
			}
			
			/* loop through keyframes, finding one that was clicked on */
			for (ak= act_keys.first; ak; ak= ak->next) {
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
			if (datatype == ACTCONT_ACTION) {
				*par= ale->owner; /* assume that this is an action channel */
				*ret_type= ale->type;
				data = ale->data;
			}
			else if (datatype == ACTCONT_SHAPEKEY) {
				data = ale->key_data;
				*ret_type= ACTTYPE_ICU;
			}
			
			/* cleanup tempolary lists */
			BLI_freelistN(&act_keys);
			act_keys.first = act_keys.last = NULL;
			
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

void *get_action_context (short *datatype)
{
	bAction *act;
	Key *key;
	
	/* get pointers to active action/shapekey blocks */
	act = (G.saction)? G.saction->action: NULL;
	key = get_action_mesh_key();
	
	if (act) {
		*datatype= ACTCONT_ACTION;
		return act;
	}
	else if (key) {
		*datatype= ACTCONT_SHAPEKEY;
		return key;
	}
	else {
		*datatype= ACTCONT_NONE;
		return NULL;
	}
}

/* Quick-tool for preview-range functionality in Action Editor for setting Preview-Range  
 * bounds to extents of Action, when Ctrl-Alt-P is used. Only available for actions.
 */
void action_previewrange_set (bAction *act)
{
	float start, end;
	
	/* sanity check */
	if (act == NULL)
		return;
		
	/* calculate range + make sure it is adjusted for nla-scaling */
	calc_action_range(act, &start, &end, 0);
	if (NLA_ACTION_SCALED) {
		start= get_action_frame_inv(OBACT, start);
		end= get_action_frame_inv(OBACT, end);
	}
	
	/* set preview range */
	G.scene->r.psfra= start;
	G.scene->r.pefra= end;
	
	BIF_undo_push("Set anim-preview range");
	allqueue(REDRAWTIME, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWNLA, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWBUTSALL, 0);
}

/* **************************************************** */
/* ACTION CHANNEL GROUPS */

/* Get the active action-group for an Action */
bActionGroup *get_active_actiongroup (bAction *act)
{
	bActionGroup *agrp= NULL;
	
	if (act && act->groups.first) {	
		for (agrp= act->groups.first; agrp; agrp= agrp->next) {
			if (agrp->flag & AGRP_ACTIVE)
				break;
		}
	}
	
	return agrp;
}

/* Make the given Action-Group the active one */
void set_active_actiongroup (bAction *act, bActionGroup *agrp, short select)
{
	bActionGroup *grp;
	
	/* sanity checks */
	if (act == NULL)
		return;
	
	/* Deactive all others */
	for (grp= act->groups.first; grp; grp= grp->next) {
		if ((grp==agrp) && (select))
			grp->flag |= AGRP_ACTIVE;
		else	
			grp->flag &= ~AGRP_ACTIVE;
	}
}

/* Add given channel into (active) group 
 *	- assumes that channel is not linked to anything anymore
 *	- always adds at the end of the group 
 */
static void action_groups_addachan (bAction *act, bActionGroup *agrp, bActionChannel *achan)
{
	bActionChannel *chan;
	short done=0;
	
	/* sanity checks */
	if (ELEM3(NULL, act, agrp, achan))
		return;
	
	/* if no channels, just add to two lists at the same time */
	if (act->chanbase.first == NULL) {
		achan->next = achan->prev = NULL;
		
		agrp->channels.first = agrp->channels.last = achan;
		act->chanbase.first = act->chanbase.last = achan;
		
		achan->grp= agrp;
		return;
	}
	
	/* try to find a channel to slot this in before/after */
	for (chan= act->chanbase.first; chan; chan= chan->next) {
		/* if channel has no group, then we have ungrouped channels, which should always occur after groups */
		if (chan->grp == NULL) {
			BLI_insertlinkbefore(&act->chanbase, chan, achan);
			
			if (agrp->channels.first == NULL)
				agrp->channels.first= achan;
			agrp->channels.last= achan;
			
			done= 1;
			break;
		}
		
		/* if channel has group after current, we can now insert (otherwise we have gone too far) */
		else if (chan->grp == agrp->next) {
			BLI_insertlinkbefore(&act->chanbase, chan, achan);
			
			if (agrp->channels.first == NULL)
				agrp->channels.first= achan;
			agrp->channels.last= achan;
			
			done= 1;
			break;
		}
		
		/* if channel has group we're targeting, check whether it is the last one of these */
		else if (chan->grp == agrp) {
			if ((chan->next) && (chan->next->grp != agrp)) {
				BLI_insertlinkafter(&act->chanbase, chan, achan);
				agrp->channels.last= achan;
				done= 1;
				break;
			}
			else if (chan->next == NULL) {
				BLI_addtail(&act->chanbase, achan);
				agrp->channels.last= achan;
				done= 1;
				break;
			}
		}
		
		/* if channel has group before target, check whether the next one is something after target */
		else if (chan->grp == agrp->prev) {
			if (chan->next) {
				if ((chan->next->grp != chan->grp) && (chan->next->grp != agrp)) {
					BLI_insertlinkafter(&act->chanbase, chan, achan);
					
					agrp->channels.first= achan;
					agrp->channels.last= achan;
					
					done= 1;
					break;
				}
			}
			else {
				BLI_insertlinkafter(&act->chanbase, chan, achan);
				
				agrp->channels.first= achan;
				agrp->channels.last= achan;
				
				done= 1;
				break;
			}
		}
	}
	
	/* only if added, set channel as belonging to this group */
	if (done) {
		achan->grp= agrp;
	}
	else 
		printf("Error: ActionChannel: '%s' couldn't be added to Group: '%s' \n", achan->name, agrp->name);
}	

/* Remove the given channel from all groups */
static void action_groups_removeachan (bAction *act, bActionChannel *achan)
{
	/* sanity checks */
	if (ELEM(NULL, act, achan))	
		return;
	
	/* check if any group used this directly */
	if (achan->grp) {
		bActionGroup *agrp= achan->grp;
		
		if (agrp->channels.first == agrp->channels.last) {
			if (agrp->channels.first == achan) {
				agrp->channels.first= NULL;
				agrp->channels.last= NULL;
			}
		}
		else if (agrp->channels.first == achan) {
			if ((achan->next) && (achan->next->grp==agrp))
				agrp->channels.first= achan->next;
			else
				agrp->channels.first= NULL;
		}
		else if (agrp->channels.last == achan) {
			if ((achan->prev) && (achan->prev->grp==agrp))
				agrp->channels.last= achan->prev;
			else
				agrp->channels.last= NULL;
		}
		
		achan->grp= NULL;
	}
	
	/* now just remove from list */
	BLI_remlink(&act->chanbase, achan);
}

/* Add a new Action-Group or add channels to active one */
void action_groups_group (short add_group)
{
	bAction *act;
	bActionChannel *achan, *anext;
	bActionGroup *agrp;
	void *data;
	short datatype;
	
	/* validate type of data we are working on */
	data = get_action_context(&datatype);
	if (data == NULL) return;
	if (datatype != ACTCONT_ACTION) return;
	act= (bAction *)data;
	
	/* get active group */
	if ((act->groups.first==NULL) || (add_group)) {		
		/* Add a new group, and make it active */
		agrp= MEM_callocN(sizeof(bActionGroup), "bActionGroup");
		
		agrp->flag |= (AGRP_ACTIVE|AGRP_SELECTED|AGRP_EXPANDED);
		sprintf(agrp->name, "Group");
		
		BLI_addtail(&act->groups, agrp);
		BLI_uniquename(&act->groups, agrp, "Group", offsetof(bActionGroup, name), 32);
		
		set_active_actiongroup(act, agrp, 1);
		
		add_group= 1;
	}
	else {
		agrp= get_active_actiongroup(act);
		
		if (agrp == NULL) {
			error("No Active Action Group");
			return;
		}
	}
	
	/* loop through action-channels, finding those that are selected + visible to move */
	// FIXME: this should be done with action api instead 
	for (achan= act->chanbase.first; achan; achan= anext) {
		anext= achan->next;
		
		/* make sure not already in new-group */
		if (achan->grp != agrp) {
			if ((achan->grp==NULL) || (EXPANDED_AGRP(achan->grp))) { 
				if (VISIBLE_ACHAN(achan) && SEL_ACHAN(achan)) {
					/* unlink from everything else */
					action_groups_removeachan(act, achan);
					
					/* add to end of group's channels */
					action_groups_addachan(act, agrp, achan);
				}
			}
		}
	}
	
	/* updates and undo */
	if (add_group)
		BIF_undo_push("Add Action Group");
	else
		BIF_undo_push("Add to Action Group");
	
	allqueue(REDRAWACTION, 0);
}

/* Remove selected channels from their groups */
void action_groups_ungroup (void)
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	bAction *act;
	void *data;
	short datatype;
	short filter;
	
	/* validate type of data we are working on */
	data = get_action_context(&datatype);
	if (data == NULL) return;
	if (datatype != ACTCONT_ACTION) return;
	act= (bAction *)data;
	
	/* filter data */
	filter= (ACTFILTER_VISIBLE|ACTFILTER_SEL);
	actdata_filter(&act_data, filter, act, ACTCONT_ACTION);
	
	/* Only ungroup selected action-channels */
	for (ale= act_data.first; ale; ale= ale->next) {
		if (ale->type == ACTTYPE_ACHAN) {
			action_groups_removeachan(act, ale->data);
			BLI_addtail(&act->chanbase, ale->data);
		}
	}
	
	BLI_freelistN(&act_data);
		
	/* updates and undo */
	BIF_undo_push("Remove From Action Groups");
	
	allqueue(REDRAWACTION, 0);
}

/* This function is used when inserting keyframes for pose-channels. It assigns the
 * action-channel with the nominated name to a group with the same name as that of 
 * the pose-channel with the nominated name.
 *
 * Note: this function calls validate_action_channel if action channel doesn't exist 
 */
void verify_pchan2achan_grouping (bAction *act, bPose *pose, char name[])
{
	bActionChannel *achan;
	bPoseChannel *pchan;
	
	/* sanity checks */
	if (ELEM3(NULL, act, pose, name))
		return;
	if (name[0] == 0)
		return;
	
	/* try to get the channels */
	pchan= get_pose_channel(pose, name);
	if (pchan == NULL) return;
	achan= verify_action_channel(act, name);
	
	/* check if pchan has a group */
	if ((pchan->agrp_index > 0) && (achan->grp == NULL)) {
		bActionGroup *agrp, *grp=NULL;
		
		/* get group to try to be like */
		agrp= (bActionGroup *)BLI_findlink(&pose->agroups, (pchan->agrp_index - 1));
		if (agrp == NULL) {
			error("PoseChannel has invalid group!");
			return;
		}
		
		/* try to find a group which is similar to the one we want (or add one) */
		for (grp= act->groups.first; grp; grp= grp->next) {
			if (!strcmp(grp->name, agrp->name))
				break;
		}
		if (grp == NULL) {
			grp= MEM_callocN(sizeof(bActionGroup), "bActionGroup");
			
			grp->flag |= (AGRP_ACTIVE|AGRP_SELECTED|AGRP_EXPANDED);
			
			/* copy name */
			sprintf(grp->name, agrp->name);
			
			/* deal with group-color copying */
			if (agrp->customCol) {
				if (agrp->customCol > 0) {
					/* copy theme colors on-to group's custom color in case user tries to edit color */
					bTheme *btheme= U.themes.first;
					ThemeWireColor *col_set= &btheme->tarm[(agrp->customCol - 1)];
					
					memcpy(&grp->cs, col_set, sizeof(ThemeWireColor));
				}
				else {
					/* init custom colours with a generic multi-colour rgb set, if not initialised already */
					if (agrp->cs.solid[0] == 0) {
						/* define for setting colors in theme below */
						#define SETCOL(col, r, g, b, a)  col[0]=r; col[1]=g; col[2]= b; col[3]= a;
						
						SETCOL(grp->cs.solid, 0xff, 0x00, 0x00, 255);
						SETCOL(grp->cs.select, 0x81, 0xe6, 0x14, 255);
						SETCOL(grp->cs.active, 0x18, 0xb6, 0xe0, 255);
						
						#undef SETCOL
					}
					else {
						/* just copy color set specified */
						memcpy(&grp->cs, &agrp->cs, sizeof(ThemeWireColor));
					}
				}
			}
			grp->customCol= agrp->customCol;
			
			BLI_addtail(&act->groups, grp);
		}
		
		/* make sure this channel is definitely not connected to anything before adding to group */
		action_groups_removeachan(act, achan);
		action_groups_addachan(act, grp, achan);
	}
}

/* This function is used when the user specifically requests to sync changes of pchans + bone groups
 * to achans + action groups. All achans are detached from their groups, and all groups are destroyed.
 * They are then recreated when the achans are reassigned to groups. 
 *
 * Note: This doesn't preserve hand-created groups, and will operate on ALL action-channels regardless of
 *		whether they were selected or active. More specific filtering can be added later. 
 */
void sync_pchan2achan_grouping ()
{
	void *data;
	short datatype;
	bAction *act;
	bActionChannel *achan, *next, *last;
	bPose *pose;
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	if ((datatype != ACTCONT_ACTION) || (data==NULL)) return;
	if ((G.saction->pin) || (OBACT==NULL) || (OBACT->type != OB_ARMATURE)) {
		error("Action doesn't belong to active armature");
		return;
	}
	
	/* get data */
	act= (bAction *)data;
	pose= OBACT->pose;
	
	/* remove achan->group links, then delete all groups */
	for (achan= act->chanbase.first; achan; achan= achan->next)
		achan->grp = NULL;
	BLI_freelistN(&act->groups);
	
	/* loop through all achans, reassigning them to groups (colours are resyncronised) */
	last= act->chanbase.last;
	for (achan= act->chanbase.first; achan && achan!=last; achan= next) {
		next= achan->next;
		verify_pchan2achan_grouping(act, pose, achan->name);
	}
	
	/* undo and redraw */
	BIF_undo_push("Sync Armature-Data and Action");
	allqueue(REDRAWACTION, 0);
}

/* **************************************************** */
/* TRANSFORM TOOLS */

/* main call to start transforming keyframes */
void transform_action_keys (int mode, int dummy)
{
	void *data;
	short datatype;
	short context = (U.flag & USER_DRAGIMMEDIATE)?CTX_TWEAK:CTX_NONE;
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	if (data == NULL) return;
	
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
		case 't':
		{
			initTransform(TFM_TIME_SLIDE, context);
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

/* ----------------------------------------- */

/* duplicate keyframes */
void duplicate_action_keys (void)
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	void *data;
	short datatype;
	int filter;
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	if (data == NULL) return;
	
	/* filter data */
	filter= (ACTFILTER_VISIBLE | ACTFILTER_FOREDIT | ACTFILTER_IPOKEYS);
	actdata_filter(&act_data, filter, data, datatype);
	
	/* loop through filtered data and duplicate selected keys */
	for (ale= act_data.first; ale; ale= ale->next) {
		duplicate_ipo_keys((Ipo *)ale->key_data);
	}
	
	/* free filtered list */
	BLI_freelistN(&act_data);
	
	/* now, go into transform-grab mode, to move keys */
	BIF_TransformSetUndo("Add Duplicate");
	transform_action_keys('g', 0);
}

/* this function is responsible for snapping the current frame to selected data  */
void snap_cfra_action() 
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	int filter;
	void *data;
	short datatype;
		
	/* get data */
	data= get_action_context(&datatype);
	if (data == NULL) return;
	
	/* filter data */
	filter= (ACTFILTER_VISIBLE | ACTFILTER_IPOKEYS);
	actdata_filter(&act_data, filter, data, datatype);
	
	/* snap current frame to selected data */
	snap_cfra_ipo_keys(NULL, -1);
	
	for (ale= act_data.first; ale; ale= ale->next) {
		if (NLA_ACTION_SCALED && datatype==ACTCONT_ACTION) {
			actstrip_map_ipo_keys(OBACT, ale->key_data, 0, 1); 
			snap_cfra_ipo_keys(ale->key_data, 0);
			actstrip_map_ipo_keys(OBACT, ale->key_data, 1, 1);
		}
		else 
			snap_cfra_ipo_keys(ale->key_data, 0);
	}
	BLI_freelistN(&act_data);
	
	snap_cfra_ipo_keys(NULL, 1);
	
	BIF_undo_push("Snap Current Frame to Keys");
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWNLA, 0);
}

/* this function is responsible for snapping keyframes to frame-times */
void snap_action_keys(short mode) 
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	int filter;
	void *data;
	short datatype;
	char str[32];
	
	/* get data */
	data= get_action_context(&datatype);
	if (data == NULL) return;
	
	/* determine mode */
	switch (mode) {
		case 1:
			strcpy(str, "Snap Keys To Nearest Frame");
			break;
		case 2:
			if (G.saction->flag & SACTION_DRAWTIME)
				strcpy(str, "Snap Keys To Current Time");
			else
				strcpy(str, "Snap Keys To Current Frame");
			break;
		case 3:
			strcpy(str, "Snap Keys To Nearest Marker");
			break;
		case 4:
			strcpy(str, "Snap Keys To Nearest Second");
			break;
		default:
			return;
	}
	
	/* filter data */
	filter= (ACTFILTER_VISIBLE | ACTFILTER_FOREDIT | ACTFILTER_IPOKEYS);
	actdata_filter(&act_data, filter, data, datatype);
	
	/* snap to frame */
	for (ale= act_data.first; ale; ale= ale->next) {
		if (NLA_ACTION_SCALED && datatype==ACTCONT_ACTION) {
			actstrip_map_ipo_keys(OBACT, ale->key_data, 0, 1); 
			snap_ipo_keys(ale->key_data, mode);
			actstrip_map_ipo_keys(OBACT, ale->key_data, 1, 1);
		}
		else 
			snap_ipo_keys(ale->key_data, mode);
	}
	BLI_freelistN(&act_data);
	
	if (datatype == ACTCONT_ACTION)
		remake_action_ipos(data);
	
	BIF_undo_push(str);
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWNLA, 0);
}

/* this function is responsible for snapping keyframes to frame-times */
void mirror_action_keys(short mode) 
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	int filter;
	void *data;
	short datatype;
	char str[32];
		
	/* get data */
	data= get_action_context(&datatype);
	if (data == NULL) return;
	
	/* determine mode */
	switch (mode) {
		case 1:
			strcpy(str, "Mirror Keys Over Current Frame");
			break;
		case 2:
			strcpy(str, "Mirror Keys Over Y-Axis");
			break;
		case 3:
			strcpy(str, "Mirror Keys Over X-Axis");
			break;
		case 4:
			strcpy(str, "Mirror Keys Over Marker");
			break;
		default:
			return;
	}
	
	/* filter data */
	filter= (ACTFILTER_VISIBLE | ACTFILTER_FOREDIT | ACTFILTER_IPOKEYS);
	actdata_filter(&act_data, filter, data, datatype);
	
	/* mirror */
	for (ale= act_data.first; ale; ale= ale->next) {
		if (NLA_ACTION_SCALED && datatype==ACTCONT_ACTION) {
			actstrip_map_ipo_keys(OBACT, ale->key_data, 0, 1); 
			mirror_ipo_keys(ale->key_data, mode);
			actstrip_map_ipo_keys(OBACT, ale->key_data, 1, 1);
		}
		else 
			mirror_ipo_keys(ale->key_data, mode);
	}
	BLI_freelistN(&act_data);
	
	if (datatype == ACTCONT_ACTION)
		remake_action_ipos(data);
	
	BIF_undo_push(str);
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWNLA, 0);
}

/* **************************************************** */
/* ADD/REMOVE KEYFRAMES */

/* This function allows the user to insert keyframes on the current
 * frame from the Action Editor, using the current values of the channels
 * to be keyframed.  
 */
void insertkey_action(void)
{
	void *data;
	short datatype;
	
	Object *ob= OBACT;
	short mode;
	float cfra;
	
	/* get data */
	data= get_action_context(&datatype);
	if (data == NULL) return;
	cfra = frame_to_float(CFRA);
		
	if (datatype == ACTCONT_ACTION) {
		ListBase act_data = {NULL, NULL};
		bActListElem *ale;
		int filter;
		
		/* ask user what to keyframe */
		mode = pupmenu("Insert Key%t|All Channels%x1|Only Selected Channels%x2|In Active Group%x3");
		if (mode <= 0) return;
		
		/* filter data */
		filter= (ACTFILTER_VISIBLE | ACTFILTER_FOREDIT | ACTFILTER_ONLYICU );
		if (mode == 2) 			filter |= ACTFILTER_SEL;
		else if (mode == 3) 	filter |= ACTFILTER_ACTGROUPED;
		
		actdata_filter(&act_data, filter, data, datatype);
		
		/* loop through ipo curves retrieved */
		for (ale= act_data.first; ale; ale= ale->next) {
			/* verify that this is indeed an ipo curve */
			if (ale->key_data && ale->owner) {
				bActionChannel *achan= (bActionChannel *)ale->owner;
				bConstraintChannel *conchan= (ale->type==ACTTYPE_CONCHAN) ? ale->data : NULL;
				IpoCurve *icu= (IpoCurve *)ale->key_data;
				
				if (ob)
					insertkey((ID *)ob, icu->blocktype, achan->name, ((conchan)?(conchan->name):(NULL)), icu->adrcode, 0);
				else
					insert_vert_icu(icu, cfra, icu->curval, 0);
			}
		}
		
		/* cleanup */
		BLI_freelistN(&act_data);
	}
	else if (datatype == ACTCONT_SHAPEKEY) {
		Key *key= (Key *)data;
		IpoCurve *icu;
		
		/* ask user if they want to insert a keyframe */
		mode = okee("Insert Keyframe?");
		if (mode <= 0) return;
		
		if (key->ipo) {
			for (icu= key->ipo->curve.first; icu; icu=icu->next) {
				insert_vert_icu(icu, cfra, icu->curval, 0);
			}
		}
	}
	
	BIF_undo_push("Insert Key");
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWNLA, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
}

/* delete selected keyframes */
void delete_action_keys (void)
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	void *data;
	short datatype;
	int filter;
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	if (data == NULL) return;
	
	/* filter data */
	filter= (ACTFILTER_VISIBLE | ACTFILTER_FOREDIT | ACTFILTER_IPOKEYS);
	actdata_filter(&act_data, filter, data, datatype);
	
	/* loop through filtered data and delete selected keys */
	for (ale= act_data.first; ale; ale= ale->next) {
		delete_ipo_keys((Ipo *)ale->key_data);
	}
	
	/* free filtered list */
	BLI_freelistN(&act_data);
	
	if (datatype == ACTCONT_ACTION)
		remake_action_ipos(data);
	
	BIF_undo_push("Delete Action Keys");
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWNLA, 0);
}

/* delete selected action-channels (only achans and conchans are considered) */
void delete_action_channels (void)
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale, *next;
	bAction *act;
	void *data;
	short datatype;
	int filter;
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	if (data == NULL) return;
	if (datatype != ACTCONT_ACTION) return;
	act= (bAction *)data;
	
	/* deal with groups first */
	if (act->groups.first) {
		bActionGroup *agrp, *grp;
		bActionChannel *chan, *nchan;
		
		/* unlink achan's that belonged to this group (and make sure they're not selected if they weren't visible) */
		for (agrp= act->groups.first; agrp; agrp= grp) {
			grp= agrp->next;
			
			/* remove if group is selected */
			if (SEL_AGRP(agrp)) {
				for (chan= agrp->channels.first; chan && chan->grp==agrp; chan= nchan) {
					nchan= chan->next;
					
					action_groups_removeachan(act, chan);
					BLI_addtail(&act->chanbase, chan);
					
					if (EXPANDED_AGRP(agrp) == 0)
						chan->flag &= ~(ACHAN_SELECTED|ACHAN_HILIGHTED);
				}
				
				BLI_freelinkN(&act->groups, agrp);
			}
		}
	}
	
	/* filter data */
	filter= (ACTFILTER_VISIBLE | ACTFILTER_FOREDIT | ACTFILTER_CHANNELS | ACTFILTER_SEL);
	actdata_filter(&act_data, filter, data, datatype);
	
	/* remove irrelevant entries */
	for (ale= act_data.first; ale; ale= next) {
		next= ale->next;
		
		if (ale->type != ACTTYPE_ACHAN)
			BLI_freelinkN(&act_data, ale);
	}
	
	/* clean up action channels */
	for (ale= act_data.first; ale; ale= next) {
		bActionChannel *achan= (bActionChannel *)ale->data;
		bConstraintChannel *conchan, *cnext;
		next= ale->next;
		
		/* release references to ipo users */
		if (achan->ipo)
			achan->ipo->id.us--;
			
		for (conchan= achan->constraintChannels.first; conchan; conchan=cnext) {
			cnext= conchan->next;
			
			if (conchan->ipo)
				conchan->ipo->id.us--;
		}
		
		/* remove action-channel from group(s) */
		if (achan->grp)
			action_groups_removeachan(act, achan);
		
		/* free memory */
		BLI_freelistN(&achan->constraintChannels);
		BLI_freelinkN(&act->chanbase, achan);
		BLI_freelinkN(&act_data, ale);
	}
	
	remake_action_ipos(data);
	
	BIF_undo_push("Delete Action Channels");
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWNLA, 0);
}

/* 'Clean' IPO curves - remove any unnecessary keyframes */
void clean_action (void)
{	
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	int filter;
	void *data;
	short datatype, ok;
	
	/* don't proceed any further if nothing to work on or user refuses */
	data= get_action_context(&datatype);
	ok= fbutton(&G.scene->toolsettings->clean_thresh, 
				0.0000001f, 1.0, 0.001, 0.1,
				"Clean Threshold");
	if (!ok) return;
	
	/* filter data */
	filter= (ACTFILTER_VISIBLE | ACTFILTER_FOREDIT | ACTFILTER_SEL | ACTFILTER_ONLYICU);
	actdata_filter(&act_data, filter, data, datatype);
	
	/* loop through filtered data and clean curves */
	for (ale= act_data.first; ale; ale= ale->next) {
		clean_ipo_curve((IpoCurve *)ale->key_data);
	}
	
	/* admin and redraws */
	BLI_freelistN(&act_data);
	
	BIF_undo_push("Clean Action");
	allqueue(REMAKEIPO, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWNLA, 0);
}


/* little cache for values... */
typedef struct tempFrameValCache {
	float frame, val;
} tempFrameValCache;

/* Evaluates the curves between each selected keyframe on each frame, and keys the value  */
void sample_action_keys (void)
{	
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	int filter;
	void *data;
	short datatype;
	
	/* sanity checks */
	data= get_action_context(&datatype);
	if (data == NULL) return;
	
	/* filter data */
	filter= (ACTFILTER_VISIBLE | ACTFILTER_FOREDIT | ACTFILTER_ONLYICU);
	actdata_filter(&act_data, filter, data, datatype);
	
	/* loop through filtered data and add keys between selected keyframes on every frame  */
	for (ale= act_data.first; ale; ale= ale->next) {
		IpoCurve *icu= (IpoCurve *)ale->key_data;
		BezTriple *bezt, *start=NULL, *end=NULL;
		tempFrameValCache *value_cache, *fp;
		int sfra, range;
		int i, n;
		
		/* find selected keyframes... once pair has been found, add keyframes  */
		for (i=0, bezt=icu->bezt; i < icu->totvert; i++, bezt++) {
			/* check if selected, and which end this is */
			if (BEZSELECTED(bezt)) {
				if (start) {
					/* set end */
					end= bezt;
					
					/* cache values then add keyframes using these values, as adding
					 * keyframes while sampling will affect the outcome...
					 */
					range= (int)( ceil(end->vec[1][0] - start->vec[1][0]) );
					sfra= (int)( floor(start->vec[1][0]) );
					
					if (range) {
						value_cache= MEM_callocN(sizeof(tempFrameValCache)*range, "IcuFrameValCache");
						
						/* 	sample values 	*/
						for (n=0, fp=value_cache; n<range && fp; n++, fp++) {
							fp->frame= (float)(sfra + n);
							fp->val= eval_icu(icu, fp->frame);
						}
						
						/* 	add keyframes with these 	*/
						for (n=0, fp=value_cache; n<range && fp; n++, fp++) {
							insert_vert_icu(icu, fp->frame, fp->val, 1);
						}
						
						/* free temp cache */
						MEM_freeN(value_cache);

						/* as we added keyframes, we need to compensate so that bezt is at the right place */
						bezt = icu->bezt + i + range - 1;
						i += (range - 1);
					}
					
					/* bezt was selected, so it now marks the start of a whole new chain to search */
					start= bezt;
					end= NULL;
				}
				else {
					/* just set start keyframe */
					start= bezt;
					end= NULL;
				}
			}
		}
		
		/* recalculate channel's handles? */
		calchandles_ipocurve(icu);
	}
	
	/* admin and redraws */
	BLI_freelistN(&act_data);
	
	BIF_undo_push("Sample Action Keys");
	allqueue(REMAKEIPO, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWNLA, 0);
}

/* **************************************************** */
/* COPY/PASTE FOR ACTIONS */
/* - The copy/paste buffer currently stores a set of Action Channels, with temporary
 *	IPO-blocks, and also temporary IpoCurves which only contain the selected keyframes.
 * - Only pastes between compatable data is possible (i.e. same achan->name, ipo-curve type, etc.)
 *	Unless there is only one element in the buffer, names are also tested to check for compatability.
 * - All pasted frames are offset by the same amount. This is calculated as the difference in the times of
 *	the current frame and the 'first keyframe' (i.e. the earliest one in all channels).
 * - The earliest frame is calculated per copy operation.
 */

/* globals for copy/paste data (like for other copy/paste buffers) */
ListBase actcopybuf = {NULL, NULL};
static float actcopy_firstframe= 999999999.0f;

/* This function frees any MEM_calloc'ed copy/paste buffer data */
void free_actcopybuf ()
{
	bActionChannel *achan, *anext;
	bConstraintChannel *conchan, *cnext;
	
	for (achan= actcopybuf.first; achan; achan= anext) {
		anext= achan->next;
		
		if (achan->ipo) {
			free_ipo(achan->ipo);
			MEM_freeN(achan->ipo);
		}
		
		for (conchan=achan->constraintChannels.first; conchan; conchan=cnext) {
			cnext= conchan->next;
			
			if (conchan->ipo) {
				free_ipo(conchan->ipo);
				MEM_freeN(conchan->ipo);
			}
			
			BLI_freelinkN(&achan->constraintChannels, conchan);
		}
		
		BLI_freelinkN(&actcopybuf, achan);
	}
	
	actcopybuf.first= actcopybuf.last= NULL;
	actcopy_firstframe= 999999999.0f;
}

/* This function adds data to the copy/paste buffer, freeing existing data first
 * Only the selected action channels gets their selected keyframes copied.
 */
void copy_actdata ()
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	int filter;
	void *data;
	short datatype;
	
	/* clear buffer first */
	free_actcopybuf();
	
	/* get data */
	data= get_action_context(&datatype);
	if (data == NULL) return;
	
	/* filter data */
	filter= (ACTFILTER_VISIBLE | ACTFILTER_SEL | ACTFILTER_IPOKEYS);
	actdata_filter(&act_data, filter, data, datatype);
	
	/* assume that each of these is an ipo-block */
	for (ale= act_data.first; ale; ale= ale->next) {
		bActionChannel *achan;
		Ipo *ipo= ale->key_data;
		Ipo *ipn;
		IpoCurve *icu, *icn;
		BezTriple *bezt;
		int i;
		
		/* coerce an action-channel out of owner */
		if (ale->ownertype == ACTTYPE_ACHAN) {
			bActionChannel *achanO= ale->owner;
			achan= MEM_callocN(sizeof(bActionChannel), "ActCopyPasteAchan");
			strcpy(achan->name, achanO->name);
		}
		else if (ale->ownertype == ACTTYPE_SHAPEKEY) {
			achan= MEM_callocN(sizeof(bActionChannel), "ActCopyPasteAchan");
			strcpy(achan->name, "#ACP_ShapeKey");
		}
		else
			continue;
		BLI_addtail(&actcopybuf, achan);
		
		/* add constraint channel if needed, then add new ipo-block */
		if (ale->type == ACTTYPE_CONCHAN) {
			bConstraintChannel *conchanO= ale->data;
			bConstraintChannel *conchan;
			
			conchan= MEM_callocN(sizeof(bConstraintChannel), "ActCopyPasteConchan");
			strcpy(conchan->name, conchanO->name);
			BLI_addtail(&achan->constraintChannels, conchan);
			
			conchan->ipo= ipn= MEM_callocN(sizeof(Ipo), "ActCopyPasteIpo");
		}
		else {
			achan->ipo= ipn= MEM_callocN(sizeof(Ipo), "ActCopyPasteIpo");
		}
		ipn->blocktype = ipo->blocktype;
		
		/* now loop through curves, and only copy selected keyframes */
		for (icu= ipo->curve.first; icu; icu= icu->next) {
			/* allocate a new curve */
			icn= MEM_callocN(sizeof(IpoCurve), "ActCopyPasteIcu");
			icn->blocktype = icu->blocktype;
			icn->adrcode = icu->adrcode;
			BLI_addtail(&ipn->curve, icn);
			
			/* find selected BezTriples to add to the buffer (and set first frame) */
			for (i=0, bezt=icu->bezt; i < icu->totvert; i++, bezt++) {
				if (BEZSELECTED(bezt)) {
					/* add to buffer ipo-curve */
					insert_bezt_icu(icn, bezt);
					
					/* check if this is the earliest frame encountered so far */
					if (bezt->vec[1][0] < actcopy_firstframe)
						actcopy_firstframe= bezt->vec[1][0];
				}
			}
		}
	}
	
	/* check if anything ended up in the buffer */
	if (ELEM(NULL, actcopybuf.first, actcopybuf.last))
		error("Nothing copied to buffer");
	
	/* free temp memory */
	BLI_freelistN(&act_data);
}

void paste_actdata ()
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	int filter;
	void *data;
	short datatype;
	
	short no_name= 0;
	float offset = CFRA - actcopy_firstframe;
	char *actname = NULL, *conname = NULL;
	
	/* check if buffer is empty */
	if (ELEM(NULL, actcopybuf.first, actcopybuf.last)) {
		error("No data in buffer to paste");
		return;
	}
	/* check if single channel in buffer (disregard names if so)  */
	if (actcopybuf.first == actcopybuf.last)
		no_name= 1;
	
	/* get data */
	data= get_action_context(&datatype);
	if (data == NULL) return;
	
	/* filter data */
	filter= (ACTFILTER_VISIBLE | ACTFILTER_SEL | ACTFILTER_FOREDIT | ACTFILTER_IPOKEYS);
	actdata_filter(&act_data, filter, data, datatype);
	
	/* from selected channels */
	for (ale= act_data.first; ale; ale= ale->next) {
		Ipo *ipo_src=NULL, *ipo_dst=ale->key_data;
		bActionChannel *achan;
		IpoCurve *ico, *icu;
		BezTriple *bezt;
		int i;
		
		/* find matching ipo-block */
		for (achan= actcopybuf.first; achan; achan= achan->next) {
			/* try to match data */
			if (ale->ownertype == ACTTYPE_ACHAN) {
				bActionChannel *achant= ale->owner;
				
				/* check if we have a corresponding action channel */
				if ((no_name) || (strcmp(achan->name, achant->name)==0)) {
					actname= achan->name;
					
					/* check if this is a constraint channel */
					if (ale->type == ACTTYPE_CONCHAN) {
						bConstraintChannel *conchant= ale->data;
						bConstraintChannel *conchan;
						
						for (conchan=achan->constraintChannels.first; conchan; conchan=conchan->next) {
							if (strcmp(conchan->name, conchant->name)==0) {
								conname= conchan->name;
								ipo_src= conchan->ipo;
								break;
							}
						}
						if (ipo_src) break;
					}
					else {
						ipo_src= achan->ipo;
						break;
					}
				}
			}
			else if (ale->ownertype == ACTTYPE_SHAPEKEY) {
				/* check if this action channel is "#ACP_ShapeKey" */
				if ((no_name) || (strcmp(achan->name, "#ACP_ShapeKey")==0)) {
					actname= achan->name;
					ipo_src= achan->ipo;
					break;
				}
			}	
		}
		
		/* this shouldn't happen, but it might */
		if (ELEM(NULL, ipo_src, ipo_dst))
			continue;
		
		/* loop over curves, pasting keyframes */
		for (ico= ipo_src->curve.first; ico; ico= ico->next) {
			icu= verify_ipocurve((ID*)OBACT, ico->blocktype, actname, conname, "", ico->adrcode);
			
			if (icu) {
				/* just start pasting, with the the first keyframe on the current frame, and so on */
				for (i=0, bezt=ico->bezt; i < ico->totvert; i++, bezt++) {						
					/* temporarily apply offset to src beztriple while copying */
					bezt->vec[0][0] += offset;
					bezt->vec[1][0] += offset;
					bezt->vec[2][0] += offset;
					
					/* insert the keyframe */
					insert_bezt_icu(icu, bezt);
					
					/* un-apply offset from src beztriple after copying */
					bezt->vec[0][0] -= offset;
					bezt->vec[1][0] -= offset;
					bezt->vec[2][0] -= offset;
				}
				
				/* recalculate channel's handles? */
				calchandles_ipocurve(icu);
			}
		}
	}
	
	/* free temp memory */
	BLI_freelistN(&act_data);
	
	/* undo and redraw stuff */
	allqueue(REDRAWVIEW3D, 0);
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWNLA, 0);
	BIF_undo_push("Paste Action Keyframes");
}

/* **************************************************** */
/* VARIOUS SETTINGS */

/* This function combines several features related to setting 
 * various ipo extrapolation/interpolation
 */
void action_set_ipo_flags (short mode, short event)
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	void *data;
	short datatype;
	int filter;
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	if (data == NULL) return;
	
	/* determine which set of processing we are doing */
	switch (mode) {
		case SET_EXTEND_POPUP:
		{
			/* present popup menu for ipo extrapolation type */
			event
				=  pupmenu("Channel Extending Type %t|"
						   "Constant %x11|"
						   "Extrapolation %x12|"
						   "Cyclic %x13|"
						   "Cyclic extrapolation %x14");
			if (event < 1) return;
		}
			break;
		case SET_IPO_POPUP:
		{
			/* present popup menu for ipo interpolation type */
			event
				=  pupmenu("Channel Ipo Type %t|"
						   "Constant %x1|"
						   "Linear %x2|"
						   "Bezier %x3");
			if (event < 1) return;
		}
			break;
			
		case SET_IPO_MENU:	/* called from menus */
		case SET_EXTEND_MENU:
			break;
			
		default: /* weird, unhandled case */
			return;
	}
	
	/* filter data */
	filter= (ACTFILTER_VISIBLE | ACTFILTER_SEL | ACTFILTER_FOREDIT | ACTFILTER_IPOKEYS);
	actdata_filter(&act_data, filter, data, datatype);
	
	/* loop through setting flags */
	for (ale= act_data.first; ale; ale= ale->next) {
		Ipo *ipo= (Ipo *)ale->key_data; 
	
		/* depending on the mode */
		switch (mode) {
			case SET_EXTEND_POPUP: /* extrapolation */
			case SET_EXTEND_MENU:
			{
				switch (event) {
					case SET_EXTEND_CONSTANT:
						setexprap_ipoloop(ipo, IPO_HORIZ);
						break;
					case SET_EXTEND_EXTRAPOLATION:
						setexprap_ipoloop(ipo, IPO_DIR);
						break;
					case SET_EXTEND_CYCLIC:
						setexprap_ipoloop(ipo, IPO_CYCL);
						break;
					case SET_EXTEND_CYCLICEXTRAPOLATION:
						setexprap_ipoloop(ipo, IPO_CYCLX);
						break;
				}
			}
				break;
			case SET_IPO_POPUP: /* interpolation */
			case SET_IPO_MENU:
			{
				setipotype_ipo(ipo, event);
			}
				break;
		}
	}
	
	/* cleanup */
	BLI_freelistN(&act_data);
	
	if (datatype == ACTCONT_ACTION)
		remake_action_ipos(data);
	
	BIF_undo_push("Set Ipo Type");
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWNLA, 0);
}

/* this function sets the handles on keyframes */
void sethandles_action_keys (int code)
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	void *data;
	short datatype;
	int filter;
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	if (data == NULL) return;
	
	/* filter data */
	filter= (ACTFILTER_VISIBLE | ACTFILTER_FOREDIT | ACTFILTER_IPOKEYS);
	actdata_filter(&act_data, filter, data, datatype);
	
	/* loop through setting flags */
	for (ale= act_data.first; ale; ale= ale->next) {
		sethandles_ipo_keys((Ipo *)ale->key_data, code);
	}
	
	/* cleanup */
	BLI_freelistN(&act_data);
	if (datatype == ACTCONT_ACTION)
		remake_action_ipos(data);
	
	BIF_undo_push("Set Handle Type");
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWNLA, 0);
}

/* ----------------------------------------- */

/* this gets called when nkey is pressed (no Transform Properties panel yet) */
static void numbuts_action ()
{
	void *data;
	short datatype;
	
	void *act_channel;
	short chantype;
	
	bActionGroup *agrp= NULL;
	bActionChannel *achan= NULL;
	bConstraintChannel *conchan= NULL;
	IpoCurve *icu= NULL;
	KeyBlock *kb= NULL;
	
	short mval[2];
	
	int but=0;
    char str[64];
	short expand, protect, mute;
	float slidermin, slidermax;
	
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	if (data == NULL) return;
	
	/* figure out what is under cursor */
	getmouseco_areawin(mval);
	if (mval[0] > NAMEWIDTH) 
		return;
	act_channel= get_nearest_act_channel(mval, &chantype);
	
	/* create items for clever-numbut */
	if (chantype == ACTTYPE_ACHAN) {
		/* Action Channel */
		achan= (bActionChannel *)act_channel;
		
		strcpy(str, achan->name);
		protect= (achan->flag & ACHAN_PROTECTED);
		expand = (achan->flag & ACHAN_EXPANDED);
		mute = (achan->ipo)? (achan->ipo->muteipo): 0;
		
		add_numbut(but++, TEX, "ActChan: ", 0, 31, str, "Name of Action Channel");
		add_numbut(but++, TOG|SHO, "Expanded", 0, 24, &expand, "Action Channel is Expanded");
		add_numbut(but++, TOG|SHO, "Muted", 0, 24, &mute, "Channel is Muted");
		add_numbut(but++, TOG|SHO, "Protected", 0, 24, &protect, "Channel is Protected");
	}
	else if (chantype == ACTTYPE_CONCHAN) {
		/* Constraint Channel */
		conchan= (bConstraintChannel *)act_channel;
		
		strcpy(str, conchan->name);
		protect= (conchan->flag & CONSTRAINT_CHANNEL_PROTECTED);
		mute = (conchan->ipo)? (conchan->ipo->muteipo): 0;
		
		add_numbut(but++, TEX, "ConChan: ", 0, 29, str, "Name of Constraint Channel");
		add_numbut(but++, TOG|SHO, "Muted", 0, 24, &mute, "Channel is Muted");
		add_numbut(but++, TOG|SHO, "Protected", 0, 24, &protect, "Channel is Protected");
	}
	else if (chantype == ACTTYPE_ICU) {
		/* IPO Curve */
		icu= (IpoCurve *)act_channel;
		
		if (G.saction->pin)
			sprintf(str, getname_ipocurve(icu, NULL));
		else
			sprintf(str, getname_ipocurve(icu, OBACT));
		
		if (IS_EQ(icu->slide_max, icu->slide_min)) {
			if (IS_EQ(icu->ymax, icu->ymin)) {
				icu->slide_min= -100.0;
				icu->slide_max= 100.0;
			}
			else {
				icu->slide_min= icu->ymin;
				icu->slide_max= icu->ymax;
			}
		}
		slidermin= icu->slide_min;
		slidermax= icu->slide_max;
		
		//protect= (icu->flag & IPO_PROTECT);
		mute = (icu->flag & IPO_MUTE);
		
		add_numbut(but++, NUM|FLO, "Slider Min:", -10000, slidermax, &slidermin, 0);
		add_numbut(but++, NUM|FLO, "Slider Max:", slidermin, 10000, &slidermax, 0);
		add_numbut(but++, TOG|SHO, "Muted", 0, 24, &mute, "Channel is Muted");
		//add_numbut(but++, TOG|SHO, "Protected", 0, 24, &protect, "Channel is Protected");
	}
	else if (chantype == ACTTYPE_SHAPEKEY) {
		/* Shape Key */
		kb= (KeyBlock *)act_channel;
		
		if (kb->name[0] == '\0') {
			Key *key= (Key *)data;
			int keynum= BLI_findindex(&key->block, kb);
			
			sprintf(str, "Key %d", keynum);
		}
		else
			strcpy(str, kb->name);
		
		if (kb->slidermin >= kb->slidermax) {
			kb->slidermin = 0.0;
			kb->slidermax = 1.0;
		}
		
	    add_numbut(but++, TEX, "KB: ", 0, 24, str, 
	               "Does this really need a tool tip?");
		add_numbut(but++, NUM|FLO, "Slider Min:", 
				   -10000, kb->slidermax, &kb->slidermin, 0);
		add_numbut(but++, NUM|FLO, "Slider Max:", 
				   kb->slidermin, 10000, &kb->slidermax, 0);
	}
	else if (chantype == ACTTYPE_GROUP) {
		/* Action Group */
		agrp= (bActionGroup *)act_channel;
		
		strcpy(str, agrp->name);
		protect= (agrp->flag & AGRP_PROTECTED);
		expand = (agrp->flag & AGRP_EXPANDED);
		
		add_numbut(but++, TEX, "ActGroup: ", 0, 31, str, "Name of Action Group");
		add_numbut(but++, TOG|SHO, "Expanded", 0, 24, &expand, "Action Group is Expanded");
		add_numbut(but++, TOG|SHO, "Protected", 0, 24, &protect, "Group is Protected");
	}
	else {
		/* nothing under-cursor */
		return;
	}
	
	/* draw clever-numbut */
    if (do_clever_numbuts(str, but, REDRAW)) {
		/* restore settings based on type */
		if (icu) {
			icu->slide_min= slidermin;
			icu->slide_max= slidermax;
			
			//if (protect) icu->flag |= IPO_PROTECT;
			//else icu->flag &= ~IPO_PROTECT;
			if (mute) icu->flag |= IPO_MUTE;
			else icu->flag &= ~IPO_MUTE;
		}
		else if (conchan) {
			strcpy(conchan->name, str);
			
			if (protect) conchan->flag |= CONSTRAINT_CHANNEL_PROTECTED;
			else conchan->flag &= ~CONSTRAINT_CHANNEL_PROTECTED;
			
			if (conchan->ipo)
				conchan->ipo->muteipo = mute;
		}
		else if (achan) {
			strcpy(achan->name, str);
			
			if (expand) achan->flag |= ACHAN_EXPANDED;
			else achan->flag &= ~ACHAN_EXPANDED;
			
			if (protect) achan->flag |= ACHAN_PROTECTED;
			else achan->flag &= ~ACHAN_PROTECTED;
			
			if (achan->ipo)
				achan->ipo->muteipo = mute;
		}
		else if (agrp) {
			strcpy(agrp->name, str);
			BLI_uniquename(&( ((bAction *)data)->groups ), agrp, "Group", offsetof(bActionGroup, name), 32);
			
			if (expand) agrp->flag |= AGRP_EXPANDED;
			else agrp->flag &= ~AGRP_EXPANDED;
			
			if (protect) agrp->flag |= AGRP_PROTECTED;
			else agrp->flag &= ~AGRP_PROTECTED;
		}
		
        allqueue(REDRAWACTION, 0);
		allspace(REMAKEIPO, 0);
		allqueue(REDRAWIPO, 0);
		allqueue(REDRAWVIEW3D, 0);
	}
}

/* Set/clear a particular flag (setting) for all selected + visible channels 
 *	mode: 0 = toggle, 1 = turn on, 2 = turn off
 */
void setflag_action_channels (short mode)
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	int filter;
	void *data;
	short datatype;
	char str[32];
	short val;
	
	/* get data */
	data= get_action_context(&datatype);
	if (data == NULL) return;
	
	/* get setting to affect */
	if (mode == 2) {
		val= pupmenu("Disable Setting%t|Protect %x1|Mute%x2");
		sprintf(str, "Disable Action Setting");
	}
	else if (mode == 1) {
		val= pupmenu("Enable Setting%t|Protect %x1|Mute%x2");
		sprintf(str, "Enable Action Setting");
	}
	else {
		val= pupmenu("Toggle Setting%t|Protect %x1|Mute%x2");
		sprintf(str, "Toggle Action Setting");
	}
	if (val <= 0) return;
	
	/* filter data */
	filter= (ACTFILTER_VISIBLE | ACTFILTER_CHANNELS | ACTFILTER_SEL);
	actdata_filter(&act_data, filter, data, datatype);
	
	/* affect selected channels */
	for (ale= act_data.first; ale; ale= ale->next) {
		switch (ale->type) {
			case ACTTYPE_GROUP:
			{
				bActionGroup *agrp= (bActionGroup *)ale->data;
				
				/* only 'protect' is available */
				if (val == 1) {
					if (mode == 2)
						agrp->flag &= ~AGRP_PROTECTED;
					else if (mode == 1)
						agrp->flag |= AGRP_PROTECTED;
					else
						agrp->flag ^= AGRP_PROTECTED;
				}
			}
				break;
			case ACTTYPE_ACHAN:
			{
				bActionChannel *achan= (bActionChannel *)ale->data;
				
				/* 'protect' and 'mute' */
				if ((val == 2) && (achan->ipo)) {
					Ipo *ipo= achan->ipo;
					
					/* mute */
					if (mode == 2)
						ipo->muteipo= 0;
					else if (mode == 1)
						ipo->muteipo= 1;
					else
						ipo->muteipo= (ipo->muteipo) ? 0 : 1;
				}
				else if (val == 1) {
					/* protected */
					if (mode == 2)
						achan->flag &= ~ACHAN_PROTECTED;
					else if (mode == 1)
						achan->flag |= ACHAN_PROTECTED;
					else
						achan->flag ^= ACHAN_PROTECTED;
				}
			}
				break;
			case ACTTYPE_CONCHAN:
			{
				bConstraintChannel *conchan= (bConstraintChannel *)ale->data;
				
				/* 'protect' and 'mute' */
				if ((val == 2) && (conchan->ipo)) {
					Ipo *ipo= conchan->ipo;
					
					/* mute */
					if (mode == 2)
						ipo->muteipo= 0;
					else if (mode == 1)
						ipo->muteipo= 1;
					else
						ipo->muteipo= (ipo->muteipo) ? 0 : 1;
				}
				else if (val == 1) {
					/* protect */
					if (mode == 2)
						conchan->flag &= ~CONSTRAINT_CHANNEL_PROTECTED;
					else if (mode == 1)
						conchan->flag |= CONSTRAINT_CHANNEL_PROTECTED;
					else
						conchan->flag ^= CONSTRAINT_CHANNEL_PROTECTED;
				}
			}
				break;
			case ACTTYPE_ICU:
			{
				IpoCurve *icu= (IpoCurve *)ale->data;
				
				/* mute */
				if (val == 2) {
					if (mode == 2)
						icu->flag &= ~IPO_MUTE;
					else if (mode == 1)
						icu->flag |= IPO_MUTE;
					else
						icu->flag ^= IPO_MUTE;
				}
			}
				break;
		}
	}
	BLI_freelistN(&act_data);
	
	BIF_undo_push(str);
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWNLA, 0);
}

/* **************************************************** */
/* CHANNEL SELECTION */

/* select_mode = SELECT_REPLACE
 *             = SELECT_ADD
 *             = SELECT_SUBTRACT
 *             = SELECT_INVERT
 */
static void select_action_group (bAction *act, bActionGroup *agrp, int selectmode) 
{
	/* Select the channel based on the selection mode */
	short select;

	switch (selectmode) {
	case SELECT_ADD:
		agrp->flag |= AGRP_SELECTED;
		break;
	case SELECT_SUBTRACT:
		agrp->flag &= ~AGRP_SELECTED;
		break;
	case SELECT_INVERT:
		agrp->flag ^= AGRP_SELECTED;
		break;
	}
	select = (agrp->flag & AGRP_SELECTED) ? 1 : 0;

	set_active_actiongroup(act, agrp, select);
}

static void hilight_channel(bAction *act, bActionChannel *achan, short select)
{
	bActionChannel *curchan;

	if (!act)
		return;

	for (curchan=act->chanbase.first; curchan; curchan=curchan->next) {
		if (curchan==achan && select)
			curchan->flag |= ACHAN_HILIGHTED;
		else
			curchan->flag &= ~ACHAN_HILIGHTED;
	}
}

/* Syncs selection of channels with selection of object elements in posemode */
/* messy call... */
static void select_poseelement_by_name (char *name, int select)
{
	Object *ob= OBACT;
	bPoseChannel *pchan;
	
	if ((ob==NULL) || (ob->type!=OB_ARMATURE))
		return;
	
	if (select == 2) {
		for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next)
			pchan->bone->flag &= ~(BONE_ACTIVE);
	}
	
	pchan= get_pose_channel(ob->pose, name);
	if (pchan) {
		if (select)
			pchan->bone->flag |= (BONE_SELECTED);
		else 
			pchan->bone->flag &= ~(BONE_SELECTED);
		if (select == 2)
			pchan->bone->flag |= (BONE_ACTIVE);
	}
}

/* apparently within active object context */
/* called extern, like on bone selection */
void select_actionchannel_by_name (bAction *act, char *name, int select)
{
	bActionChannel *achan;

	if (act == NULL)
		return;
	
	for (achan = act->chanbase.first; achan; achan= achan->next) {
		if (!strcmp(achan->name, name)) {
			if (select) {
				achan->flag |= ACHAN_SELECTED;
				hilight_channel(act, achan, 1);
			}
			else {
				achan->flag &= ~ACHAN_SELECTED;
				hilight_channel(act, achan, 0);
			}
			return;
		}
	}
}

/* select_mode = SELECT_REPLACE
 *             = SELECT_ADD
 *             = SELECT_SUBTRACT
 *             = SELECT_INVERT
 */

/* exported for outliner (ton) */
/* apparently within active object context */
int select_channel(bAction *act, bActionChannel *achan, int selectmode) 
{
	/* Select the channel based on the selection mode */
	int flag;

	switch (selectmode) {
	case SELECT_ADD:
		achan->flag |= ACHAN_SELECTED;
		break;
	case SELECT_SUBTRACT:
		achan->flag &= ~ACHAN_SELECTED;
		break;
	case SELECT_INVERT:
		achan->flag ^= ACHAN_SELECTED;
		break;
	}
	flag = (achan->flag & ACHAN_SELECTED) ? 1 : 0;

	hilight_channel(act, achan, flag);
	select_poseelement_by_name(achan->name, flag);

	return flag;
}

static int select_constraint_channel(bAction *act, 
                                     bConstraintChannel *conchan, 
                                     int selectmode) 
{
	/* Select the constraint channel based on the selection mode */
	int flag;

	switch (selectmode) {
	case SELECT_ADD:
		conchan->flag |= CONSTRAINT_CHANNEL_SELECT;
		break;
	case SELECT_SUBTRACT:
		conchan->flag &= ~CONSTRAINT_CHANNEL_SELECT;
		break;
	case SELECT_INVERT:
		conchan->flag ^= CONSTRAINT_CHANNEL_SELECT;
		break;
	}
	flag = (conchan->flag & CONSTRAINT_CHANNEL_SELECT) ? 1 : 0;

	return flag;
}

int select_icu_channel(bAction *act, IpoCurve *icu, int selectmode) 
{
	/* Select the channel based on the selection mode */
	int flag;

	switch (selectmode) {
	case SELECT_ADD:
		icu->flag |= IPO_SELECT;
		break;
	case SELECT_SUBTRACT:
		icu->flag &= ~IPO_SELECT;
		break;
	case SELECT_INVERT:
		icu->flag ^= IPO_SELECT;
		break;
	}
	flag = (icu->flag & IPO_SELECT) ? 1 : 0;
	return flag;
}

/* ----------------------------------------- */

/* De-selects or inverts the selection of Channels in a given Action 
 *	mode: 0 = default behaviour (select all), 1 = test if (de)select all, 2 = invert all 
 */
void deselect_actionchannels (bAction *act, short mode)
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	int filter, sel=1;
	
	/* filter data */
	filter= ACTFILTER_VISIBLE;
	actdata_filter(&act_data, filter, act, ACTCONT_ACTION);
	
	/* See if we should be selecting or deselecting */
	if (mode == 1) {
		for (ale= act_data.first; ale; ale= ale->next) {
			if (sel == 0) 
				break;
			
			switch (ale->type) {
				case ACTTYPE_GROUP:
					if (ale->flag & AGRP_SELECTED)
						sel= 0;
					break;
				case ACTTYPE_ACHAN:
					if (ale->flag & ACHAN_SELECTED) 
						sel= 0;
					break;
				case ACTTYPE_CONCHAN:
					if (ale->flag & CONSTRAINT_CHANNEL_SELECT) 
						sel=0;
					break;
				case ACTTYPE_ICU:
					if (ale->flag & IPO_SELECT)
						sel=0;
					break;
			}
		}
	}
	else
		sel= 0;
		
	/* Now set the flags */
	for (ale= act_data.first; ale; ale= ale->next) {
		switch (ale->type) {
			case ACTTYPE_GROUP:
			{
				bActionGroup *agrp= (bActionGroup *)ale->data;
				
				if (mode == 2)
					agrp->flag ^= AGRP_SELECTED;
				else if (sel)
					agrp->flag |= AGRP_SELECTED;
				else
					agrp->flag &= ~AGRP_SELECTED;
					
				agrp->flag &= ~AGRP_ACTIVE;
			}
				break;
			case ACTTYPE_ACHAN:
			{
				bActionChannel *achan= (bActionChannel *)ale->data;
				
				if (mode == 2)
					achan->flag ^= AGRP_SELECTED;
				else if (sel)
					achan->flag |= ACHAN_SELECTED;
				else
					achan->flag &= ~ACHAN_SELECTED;
					
				select_poseelement_by_name(achan->name, sel);
				achan->flag &= ~ACHAN_HILIGHTED;
			}
				break;
			case ACTTYPE_CONCHAN:
			{
				bConstraintChannel *conchan= (bConstraintChannel *)ale->data;
				
				if (mode == 2)
					conchan->flag ^= CONSTRAINT_CHANNEL_SELECT;
				else if (sel)
					conchan->flag |= CONSTRAINT_CHANNEL_SELECT;
				else
					conchan->flag &= ~CONSTRAINT_CHANNEL_SELECT;
			}
				break;
			case ACTTYPE_ICU:
			{
				IpoCurve *icu= (IpoCurve *)ale->data;
				
				if (mode == 2)
					icu->flag ^= IPO_SELECT;
				else if (sel)
					icu->flag |= IPO_SELECT;
				else
					icu->flag &= ~IPO_SELECT;
					
				icu->flag &= ~IPO_ACTIVE;
			}
				break;
		}
	}
	
	/* Cleanup */
	BLI_freelistN(&act_data);
}

/* deselects channels in the action editor */
void deselect_action_channels (short mode)
{
	void *data;
	short datatype;
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	if (data == NULL) return;
	
	/* based on type */
	if (datatype == ACTCONT_ACTION)
		deselect_actionchannels(data, mode);
	// should shapekey channels be allowed to do this? 
}

/* deselects keyframes in the action editor */
void deselect_action_keys (short test, short sel)
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	int filter;
	void *data;
	short datatype;
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	if (data == NULL) return;
		
	/* filter data */
	filter= (ACTFILTER_VISIBLE | ACTFILTER_IPOKEYS);
	actdata_filter(&act_data, filter, data, datatype);
	
	/* See if we should be selecting or deselecting */
	if (test) {
		for (ale= act_data.first; ale; ale= ale->next) {
			if (is_ipo_key_selected(ale->key_data)) {
				sel= 0;
				break;
			}
		}
	}
		
	/* Now set the flags */
	for (ale= act_data.first; ale; ale= ale->next) {
		set_ipo_key_selection(ale->key_data, sel);
	}
	
	/* Cleanup */
	BLI_freelistN(&act_data);
}

/* selects all keyframes in the action editor - per channel or time 
 *	mode = 0: all in channel; mode = 1: all in frame
 */
void selectall_action_keys (short mval[], short mode, short select_mode)
{
	void *data;
	short datatype;
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	if (data == NULL) return;
		
	if (select_mode == SELECT_REPLACE) {
		deselect_action_keys(0, 0);
		select_mode = SELECT_ADD;
	}
		
	/* depending on mode */
	switch (mode) {
		case 0: /* all in channel*/
		{
			void *act_channel;
			short chantype;
			
			/* get channel, and act according to type */
			act_channel= get_nearest_act_channel(mval, &chantype);
			switch (chantype) {
				case ACTTYPE_GROUP:	
				{
					bActionGroup *agrp= (bActionGroup *)act_channel;
					bActionChannel *achan;
					bConstraintChannel *conchan;
					
					for (achan= agrp->channels.first; achan && achan->grp==agrp; achan= achan->next) {
						select_ipo_bezier_keys(achan->ipo, select_mode);
						
						for (conchan=achan->constraintChannels.first; conchan; conchan=conchan->next) 
							select_ipo_bezier_keys(conchan->ipo, select_mode);
					}
				}
					break;
				case ACTTYPE_ACHAN:
				{
					bActionChannel *achan= (bActionChannel *)act_channel;
					select_ipo_bezier_keys(achan->ipo, select_mode);
				}
					break;
				case ACTTYPE_CONCHAN:
				{
					bConstraintChannel *conchan= (bConstraintChannel *)act_channel;
					select_ipo_bezier_keys(conchan->ipo, select_mode);
				}
					break;
				case ACTTYPE_ICU:
				{
					IpoCurve *icu= (IpoCurve *)act_channel;
					select_icu_bezier_keys(icu, select_mode);
				}
					break;
			}
		}
			break;
		case 1: /* all in frame */
		{
			ListBase act_data = {NULL, NULL};
			bActListElem *ale;
			int filter;
			rcti rect;
			rctf rectf;
			
			/* use bounding box to find kframe */
			rect.xmin = rect.xmax = mval[0];
			rect.ymin = rect.ymax = mval[1];
			
			mval[0]= rect.xmin;
			mval[1]= rect.ymin+2;
			areamouseco_to_ipoco(G.v2d, mval, &rectf.xmin, &rectf.ymin);
			rectf.xmax= rectf.xmin;
			rectf.ymax= rectf.ymin;
			
			rectf.xmin = rectf.xmin - 0.5;
			rectf.xmax = rectf.xmax + 0.5;
			
			/* filter data */
			filter= (ACTFILTER_VISIBLE | ACTFILTER_IPOKEYS);
			actdata_filter(&act_data, filter, data, datatype);
				
			/* Now set the flags */
			for (ale= act_data.first; ale; ale= ale->next)
				borderselect_ipo_key(ale->key_data, rectf.xmin, rectf.xmax, select_mode);
			
			/* Cleanup */
			BLI_freelistN(&act_data);
		}
		break;
	}
	
	allqueue(REDRAWNLA, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
}

/* Selects all visible keyframes between the specified markers */
void markers_selectkeys_between (void)
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	int filter;
	void *data;
	short datatype;
	float min, max;
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	if (data == NULL) return;
	
	/* get extreme markers */
	get_minmax_markers(1, &min, &max);
	if (min==max) return;
	min -= 0.5f;
	max += 0.5f;
	
	/* filter data */
	filter= (ACTFILTER_VISIBLE | ACTFILTER_IPOKEYS);
	actdata_filter(&act_data, filter, data, datatype);
		
	/* select keys in-between */
	for (ale= act_data.first; ale; ale= ale->next) {
		if(NLA_ACTION_SCALED && datatype==ACTCONT_ACTION) {
			actstrip_map_ipo_keys(OBACT, ale->key_data, 0, 1);
			borderselect_ipo_key(ale->key_data, min, max, SELECT_ADD);
			actstrip_map_ipo_keys(OBACT, ale->key_data, 1, 1);
		}
		else {
			borderselect_ipo_key(ale->key_data, min, max, SELECT_ADD);
		}
	}
	
	/* Cleanup */
	BLI_freelistN(&act_data);
}

/* Selects all the keyframes on either side of the current frame (depends on which side the mouse is on) */
void selectkeys_leftright (short leftright, short select_mode)
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	int filter;
	void *data;
	short datatype;
	float min, max;
	
	if (select_mode==SELECT_REPLACE) {
		select_mode=SELECT_ADD;
		deselect_action_keys(0, 0);
	}
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	if (data == NULL) return;
	
	if (leftright == 1) {
		min = -MAXFRAMEF;
		max = (float)(CFRA + 0.1f);
	} 
	else {
		min = (float)(CFRA - 0.1f);
		max = MAXFRAMEF;
	}
	
	/* filter data */
	filter= (ACTFILTER_VISIBLE | ACTFILTER_IPOKEYS);
	actdata_filter(&act_data, filter, data, datatype);
		
	/* select keys on the side where most data occurs */
	for (ale= act_data.first; ale; ale= ale->next) {
		if(NLA_ACTION_SCALED && datatype==ACTCONT_ACTION) {
			actstrip_map_ipo_keys(OBACT, ale->key_data, 0, 1);
			borderselect_ipo_key(ale->key_data, min, max, SELECT_ADD);
			actstrip_map_ipo_keys(OBACT, ale->key_data, 1, 1);
		}
		else {
			borderselect_ipo_key(ale->key_data, min, max, SELECT_ADD);
		}
	}
	
	/* Cleanup */
	BLI_freelistN(&act_data);
	
	allqueue(REDRAWNLA, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
}

/* ----------------------------------------- */

/* Jumps to the frame where the next/previous keyframe (that is visible) occurs 
 *	dir: indicates direction
 */
void nextprev_action_keyframe (short dir)
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	int filter;
	void *data;
	short datatype;
	
	ListBase elems= {NULL, NULL};
	CfraElem *ce, *nearest=NULL;
	float dist, min_dist= 1000000;
	
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	if (data == NULL) return;
	
	/* abort if no direction */
	if (dir == 0)
		return;
	
	/* get list of keyframes that can be used (in global-time) */
	filter= (ACTFILTER_VISIBLE | ACTFILTER_IPOKEYS);
	actdata_filter(&act_data, filter, data, datatype);
	
	for (ale= act_data.first; ale; ale= ale->next) {
		if (NLA_ACTION_SCALED && datatype==ACTCONT_ACTION) {
			actstrip_map_ipo_keys(OBACT, ale->key_data, 0, 1); 
			make_cfra_list(ale->key_data, &elems);
			actstrip_map_ipo_keys(OBACT, ale->key_data, 1, 1);
		}
		else 
			make_cfra_list(ale->key_data, &elems);
	}
	
	BLI_freelistN(&act_data);
	
	/* find nearest keyframe to current frame */
	for (ce= elems.first; ce; ce= ce->next) {
		dist= ABS(ce->cfra - CFRA);
		
		if (dist < min_dist) {
			min_dist= dist;
			nearest= ce;
		}
	}
	
	/* if a nearest keyframe was found, use the one either side */
	if (nearest) {
		short changed= 0;
		
		if ((dir > 0) && (nearest->next)) {
			CFRA= nearest->next->cfra;
			changed= 1;
		}
		else if ((dir < 0) && (nearest->prev)) {
			CFRA= nearest->prev->cfra;
			changed= 1;
		}
			
		if (changed) {	
			update_for_newframe();	
			allqueue(REDRAWALL, 0);
		}
	}
	
	/* free temp data */
	BLI_freelistN(&elems);
}

/* ----------------------------------------- */

/* This function makes a list of the selected keyframes
 * in the ipo curves it has been passed
 */
static void make_sel_cfra_list (Ipo *ipo, ListBase *elems)
{
	IpoCurve *icu;
	
	if (ipo == NULL) return;
	
	for (icu= ipo->curve.first; icu; icu= icu->next) {
		BezTriple *bezt;
		int a= 0;
		
		for (bezt=icu->bezt; a<icu->totvert; a++, bezt++) {
			if (bezt && BEZSELECTED(bezt))
				add_to_cfra_elem(elems, bezt);
		}
	}
}

/* This function selects all key frames in the same column(s) as a already selected key(s)
 * or marker(s), or all the keyframes on a particular frame (triggered by a RMB on x-scrollbar)
 */
void column_select_action_keys (int mode)
{
	ListBase elems= {NULL, NULL};
	CfraElem *ce;
	IpoCurve *icu;
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	int filter;
	void *data;
	short datatype;
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	if (data == NULL) return;
	
	/* build list of columns */
	switch (mode) {
		case 1: /* list of selected keys */
			filter= (ACTFILTER_VISIBLE | ACTFILTER_IPOKEYS);
			actdata_filter(&act_data, filter, data, datatype);
			
			for (ale= act_data.first; ale; ale= ale->next)
				make_sel_cfra_list(ale->key_data, &elems);
			
			BLI_freelistN(&act_data);
			break;
		case 2: /* list of selected markers */
			make_marker_cfra_list(&elems, 1);
			
			/* apply scaled action correction if needed */
			if (NLA_ACTION_SCALED && datatype==ACTCONT_ACTION) {
				for (ce= elems.first; ce; ce= ce->next) 
					ce->cfra= get_action_frame(OBACT, ce->cfra);
			}
			break;
		case 3: /* current frame */
			/* make a single CfraElem */
			ce= MEM_callocN(sizeof(CfraElem), "cfraElem");
			BLI_addtail(&elems, ce);
			
			/* apply scaled action correction if needed */
			if (NLA_ACTION_SCALED && datatype==ACTCONT_ACTION)
				ce->cfra= get_action_frame(OBACT, CFRA);
			else
				ce->cfra= CFRA;
	}
	
	/* loop through all of the keys and select additional keyframes
	 * based on the keys found to be selected above
	 */
	filter= (ACTFILTER_VISIBLE | ACTFILTER_ONLYICU);
	actdata_filter(&act_data, filter, data, datatype);
	
	for (ale= act_data.first; ale; ale= ale->next) {
		for (ce= elems.first; ce; ce= ce->next) {
			for (icu= ale->key_data; icu; icu= icu->next) {
				BezTriple *bezt;
				int verts = 0;
				
				for (bezt=icu->bezt; verts<icu->totvert; bezt++, verts++) {
					if (bezt) {
						if( (int)(ce->cfra) == (int)(bezt->vec[1][0]) )
							bezt->f2 |= 1;
					}
				}
			}
		}
	}
	
	BLI_freelistN(&act_data);
	BLI_freelistN(&elems);
}

/* borderselect: for action-channels */
void borderselect_actionchannels (void)
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	int filter;
	void *data;
	short datatype;
	
	rcti rect;
	rctf rectf;
	int val, selectmode;
	short mval[2];
	float ymin, ymax;
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	if (data == NULL) return;
	if (datatype != ACTCONT_ACTION) return;
	
	/* draw and handle the borderselect stuff (ui) and get the select rect */
	if ( (val = get_border(&rect, 3)) ) {
		selectmode= ((val==LEFTMOUSE) ? SELECT_ADD : SELECT_SUBTRACT);
		
		mval[0]= rect.xmin;
		mval[1]= rect.ymin+2;
		areamouseco_to_ipoco(G.v2d, mval, &rectf.xmin, &rectf.ymin);
		mval[0]= rect.xmax;
		mval[1]= rect.ymax-2;
		areamouseco_to_ipoco(G.v2d, mval, &rectf.xmax, &rectf.ymax);
		
		ymax = CHANNELHEIGHT/2;
		
		/* filter data */
		filter= (ACTFILTER_VISIBLE | ACTFILTER_CHANNELS);
		actdata_filter(&act_data, filter, data, datatype);
		
		/* loop over data, doing border select */
		for (ale= act_data.first; ale; ale= ale->next) {
			ymin=ymax-(CHANNELHEIGHT+CHANNELSKIP);
			
			/* if channel is within border-select region, alter it */
			if (!((ymax < rectf.ymin) || (ymin > rectf.ymax))) {
				/* only the following types can be selected */
				switch (ale->type) {
					case ACTTYPE_GROUP: /* action group */
					{
						bActionGroup *agrp= (bActionGroup *)ale->data;
						
						if (selectmode == SELECT_ADD)
							agrp->flag |= AGRP_SELECTED;
						else
							agrp->flag &= ~AGRP_SELECTED;
					}
						break;
					case ACTTYPE_ACHAN: /* action channel */
					{
						bActionChannel *achan= (bActionChannel *)ale->data;
						
						if (selectmode == SELECT_ADD)
							achan->flag |= ACHAN_SELECTED;
						else
							achan->flag &= ~ACHAN_SELECTED;
					}
						break;
					case ACTTYPE_CONCHAN: /* constraint channel */
					{
						bConstraintChannel *conchan = (bConstraintChannel *)ale->data;
						
						if (selectmode == SELECT_ADD)
							conchan->flag |= CONSTRAINT_CHANNEL_SELECT;
						else
							conchan->flag &= ~CONSTRAINT_CHANNEL_SELECT;
					}
						break;
					case ACTTYPE_ICU: /* ipo-curve channel */
					{
						IpoCurve *icu = (IpoCurve *)ale->data;
						
						if (selectmode == SELECT_ADD)
							icu->flag |= IPO_SELECT;
						else
							icu->flag &= ~IPO_SELECT;
					}
						break;
				}
			}
			
			ymax=ymin;
		}
		
		/* cleanup */
		BLI_freelistN(&act_data);
		
		BIF_undo_push("Border Select Action");
		allqueue(REDRAWIPO, 0);
		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWNLA, 0);
	}
}

/* some quick defines for borderselect modes */
enum {
	ACTEDIT_BORDERSEL_ALL = 0,
	ACTEDIT_BORDERSEL_FRA,
	ACTEDIT_BORDERSEL_CHA
};

/* borderselect: for keyframes only */
void borderselect_action (void)
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	int filter;
	void *data;
	short datatype;
	
	rcti rect;
	rctf rectf;
	int val, selectmode, mode;
	int (*select_function)(BezTriple *);
	short mval[2];
	float ymin, ymax;
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	if (data == NULL) return;
	
	/* what should be selected (based on the starting location of cursor) */
	getmouseco_areawin(mval);
	if (IN_2D_VERT_SCROLL(mval)) 
		mode = ACTEDIT_BORDERSEL_CHA;
	else if (IN_2D_HORIZ_SCROLL(mval))
		mode = ACTEDIT_BORDERSEL_FRA;
	else
		mode = ACTEDIT_BORDERSEL_ALL;
	
	/* draw and handle the borderselect stuff (ui) and get the select rect */
	if ( (val = get_border(&rect, 3)) ) {
		if (val == LEFTMOUSE) {
			selectmode = SELECT_ADD;
			select_function = select_bezier_add;
		}
		else {
			selectmode = SELECT_SUBTRACT;
			select_function = select_bezier_subtract;
		}
		
		mval[0]= rect.xmin;
		mval[1]= rect.ymin+2;
		areamouseco_to_ipoco(G.v2d, mval, &rectf.xmin, &rectf.ymin);
		mval[0]= rect.xmax;
		mval[1]= rect.ymax-2;
		areamouseco_to_ipoco(G.v2d, mval, &rectf.xmax, &rectf.ymax);
		
		/* if action is mapped in NLA, it returns a correction */
		if (NLA_ACTION_SCALED && datatype==ACTCONT_ACTION) {
			rectf.xmin= get_action_frame(OBACT, rectf.xmin);
			rectf.xmax= get_action_frame(OBACT, rectf.xmax);
		}
		
		ymax = CHANNELHEIGHT/2;
		
		/* filter data */
		filter= (ACTFILTER_VISIBLE | ACTFILTER_CHANNELS);
		actdata_filter(&act_data, filter, data, datatype);
		
		/* loop over data, doing border select */
		for (ale= act_data.first; ale; ale= ale->next) {
			ymin=ymax-(CHANNELHEIGHT+CHANNELSKIP);
			
			/* what gets selected depends on the mode (based on initial position of cursor) */
			switch (mode) {
			case ACTEDIT_BORDERSEL_FRA: /* all in frame(s) */
				if (ale->key_data) {
					if (ale->datatype == ALE_IPO)
						borderselect_ipo_key(ale->key_data, rectf.xmin, rectf.xmax, selectmode);
					else if (ale->datatype == ALE_ICU)
						borderselect_icu_key(ale->key_data, rectf.xmin, rectf.xmax, select_function);
				}
				else if (ale->type == ACTTYPE_GROUP) {
					bActionGroup *agrp= ale->data;
					bActionChannel *achan;
					bConstraintChannel *conchan;
					
					for (achan= agrp->channels.first; achan && achan->grp==agrp; achan= achan->next) {
						borderselect_ipo_key(achan->ipo, rectf.xmin, rectf.xmax, selectmode);
						
						for (conchan=achan->constraintChannels.first; conchan; conchan=conchan->next)
							borderselect_ipo_key(conchan->ipo, rectf.xmin, rectf.xmax, selectmode);
					}
				}
				break;
			case ACTEDIT_BORDERSEL_CHA: /* all in channel(s) */
				if (!((ymax < rectf.ymin) || (ymin > rectf.ymax))) {
					if (ale->key_data) {
						if (ale->datatype == ALE_IPO)
							select_ipo_bezier_keys(ale->key_data, selectmode);
						else if (ale->datatype == ALE_ICU)
							select_icu_bezier_keys(ale->key_data, selectmode);
					}
					else if (ale->type == ACTTYPE_GROUP) {
						bActionGroup *agrp= ale->data;
						bActionChannel *achan;
						bConstraintChannel *conchan;
						
						for (achan= agrp->channels.first; achan && achan->grp==agrp; achan= achan->next) {
							select_ipo_bezier_keys(achan->ipo, selectmode);
							
							for (conchan=achan->constraintChannels.first; conchan; conchan=conchan->next)
								select_ipo_bezier_keys(conchan->ipo, selectmode);
						}
					}
				}
				break;
			default: /* any keyframe inside region defined by region */
				if (!((ymax < rectf.ymin) || (ymin > rectf.ymax))) {
					if (ale->key_data) {
						if (ale->datatype == ALE_IPO)
							borderselect_ipo_key(ale->key_data, rectf.xmin, rectf.xmax, selectmode);
						else if (ale->datatype == ALE_ICU)
							borderselect_icu_key(ale->key_data, rectf.xmin, rectf.xmax, select_function);
					}
					else if (ale->type == ACTTYPE_GROUP) {
						bActionGroup *agrp= ale->data;
						bActionChannel *achan;
						bConstraintChannel *conchan;
						
						for (achan= agrp->channels.first; achan && achan->grp==agrp; achan= achan->next) {
							borderselect_ipo_key(achan->ipo, rectf.xmin, rectf.xmax, selectmode);
							
							for (conchan=achan->constraintChannels.first; conchan; conchan=conchan->next)
								borderselect_ipo_key(conchan->ipo, rectf.xmin, rectf.xmax, selectmode);
						}
					}
				}
			}
			
			ymax=ymin;
		}
		
		/* cleanup */
		BLI_freelistN(&act_data);
		
		BIF_undo_push("Border Select Action");
		allqueue(REDRAWIPO, 0);
		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWNLA, 0);
	}
}

/* **************************************************** */
/* MOUSE-HANDLING */

/* right-hand side - mouse click */
static void mouse_action (int selectmode)
{
	void *data;
	short datatype;
	
	bAction	*act= NULL;
	bActionGroup *agrp= NULL;
	bActionChannel *achan= NULL;
	bConstraintChannel *conchan= NULL;
	IpoCurve *icu= NULL;
	TimeMarker *marker, *pmarker;
	
	void *act_channel;
	short sel, act_type = 0;
	float selx = 0.0;
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	if (data == NULL) return;
	if (datatype == ACTCONT_ACTION) act= (bAction *)data;

	act_channel= get_nearest_action_key(&selx, &sel, &act_type, &achan);
	marker= find_nearest_marker(SCE_MARKERS, 1);
	pmarker= (act) ? find_nearest_marker(&act->markers, 1) : NULL;
	
	if (marker) {
		/* what about scene's markers? */		
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
	else if (pmarker) {
		/* action's markers are drawn behind scene markers */		
		if (selectmode == SELECT_REPLACE) {
			action_set_activemarker(act, pmarker, 1);
			pmarker->flag |= SELECT;
		}
		else if (selectmode == SELECT_INVERT) {
			if (pmarker->flag & SELECT) {
				pmarker->flag &= ~SELECT;
				action_set_activemarker(act, NULL, 0);
			}
			else {
				pmarker->flag |= SELECT;
				action_set_activemarker(act, pmarker, 0);
			}
		}
		else if (selectmode == SELECT_ADD)  {
			pmarker->flag |= SELECT;
			action_set_activemarker(act, pmarker, 0);
		}
		else if (selectmode == SELECT_SUBTRACT) {
			pmarker->flag &= ~SELECT;
			action_set_activemarker(act, NULL, 0);
		}
		
		// TODO: local-markers cannot be moved atm...
		//std_rmouse_transform(transform_markers);
		
		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWBUTSEDIT, 0);
	}
	else if (act_channel) {
		/* must have been a channel */
		switch (act_type) {
			case ACTTYPE_ICU:
				icu= (IpoCurve *)act_channel;
				break;
			case ACTTYPE_CONCHAN:
				conchan= (bConstraintChannel *)act_channel;
				break;
			case ACTTYPE_ACHAN:
				achan= (bActionChannel *)act_channel;
				break;
			case ACTTYPE_GROUP:
				agrp= (bActionGroup *)act_channel;
				break;
			default:
				return;
		}
		
		if (selectmode == SELECT_REPLACE) {
			selectmode = SELECT_ADD;
			
			deselect_action_keys(0, 0);
			
			if (datatype == ACTCONT_ACTION) {
				deselect_action_channels(0);
				
				/* Highlight either an Action-Channel or Action-Group */
				if (achan) {
					achan->flag |= ACHAN_SELECTED;
					hilight_channel(act, achan, 1);
					select_poseelement_by_name(achan->name, 2);	/* 2 is activate */
				}
				else if (agrp) {
					agrp->flag |= AGRP_SELECTED;
					set_active_actiongroup(act, agrp, 1);
				}
			}
		}
		
		if (icu)
			select_icu_key(icu, selx, selectmode);
		else if (conchan)
			select_ipo_key(conchan->ipo, selx, selectmode);
		else if (achan)
			select_ipo_key(achan->ipo, selx, selectmode);
		else if (agrp) {
			for (achan= agrp->channels.first; achan && achan->grp==agrp; achan= achan->next) {
				select_ipo_key(achan->ipo, selx, selectmode);
				
				for (conchan=achan->constraintChannels.first; conchan; conchan=conchan->next)
					select_ipo_key(conchan->ipo, selx, selectmode);
			}
		}
		
		std_rmouse_transform(transform_action_keys);
		
		allqueue(REDRAWIPO, 0);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWNLA, 0);
		allqueue(REDRAWOOPS, 0);
		allqueue(REDRAWBUTSALL, 0);
	}
}

/* lefthand side - mouse-click  */
static void mouse_actionchannels (short mval[])
{
	bAction *act= G.saction->action;
	void *data, *act_channel;
	short datatype, chantype;
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	if (data == NULL) return;
	
	/* get channel to work on */
	act_channel= get_nearest_act_channel(mval, &chantype);
	
	/* action to take depends on what channel we've got */
	switch (chantype) {
		case ACTTYPE_GROUP: 
			{
				bActionGroup *agrp= (bActionGroup *)act_channel;
				
				if (mval[0] < 16) {
					/* toggle expand */
					agrp->flag ^= AGRP_EXPANDED;
				}
				else if (mval[0] >= (NAMEWIDTH-16)) {
					/* toggle protection/locking */
					agrp->flag ^= AGRP_PROTECTED;
 				}
 				else {
					/* select/deselect group */
					if (G.qual == LR_SHIFTKEY) {
						/* inverse selection status of group */
						select_action_group(act, agrp, SELECT_INVERT);
					}
					else if (G.qual == (LR_CTRLKEY|LR_SHIFTKEY)) {
						bActionChannel *achan;
						
						/* select all in group (and deselect everthing else) */	
						deselect_actionchannels(act, 0);
						
						for (achan= agrp->channels.first; achan && achan->grp==agrp; achan= achan->next) {
							select_channel(act, achan, SELECT_ADD);
							
							/* messy... set active bone */
							select_poseelement_by_name(achan->name, 1);
						}
						select_action_group(act, agrp, SELECT_ADD);
					}
					else {
						/* select group by itself */
						deselect_actionchannels(act, 0);
						select_action_group(act, agrp, SELECT_ADD);
					}
 				}
			}
			break;
		case ACTTYPE_ACHAN:
			{
				bActionChannel *achan= (bActionChannel *)act_channel;
				
				if (mval[0] >= (NAMEWIDTH-16)) {
					/* toggle protect */
					achan->flag ^= ACHAN_PROTECTED;
				}
				else if ((mval[0] >= (NAMEWIDTH-32)) && (achan->ipo)) {
					/* toggle mute */
					achan->ipo->muteipo = (achan->ipo->muteipo)? 0: 1;
				}
				else if (mval[0] <= 17) {
					/* toggle expand */
					achan->flag ^= ACHAN_EXPANDED;
				}				
				else {
					/* select/deselect achan */		
					if (G.qual & LR_SHIFTKEY) {
						select_channel(act, achan, SELECT_INVERT);
					}
					else {
						deselect_actionchannels(act, 0);
						select_channel(act, achan, SELECT_ADD);
					}
					
					/* messy... set active bone */
					select_poseelement_by_name(achan->name, 2);
				}
			}
				break;
		case ACTTYPE_FILLIPO:
			{
				bActionChannel *achan= (bActionChannel *)act_channel;
				
				achan->flag ^= ACHAN_SHOWIPO;
				
				if ((mval[0] > 24) && (achan->flag & ACHAN_SHOWIPO)) {
					/* select+make active achan */		
					deselect_actionchannels(act, 0);
					select_channel(act, achan, SELECT_ADD);
					
					/* messy... set active bone */
					select_poseelement_by_name(achan->name, 2);
				}	
			}
			break;
		case ACTTYPE_FILLCON:
			{
				bActionChannel *achan= (bActionChannel *)act_channel;
				
				achan->flag ^= ACHAN_SHOWCONS;
				
				if ((mval[0] > 24) && (achan->flag & ACHAN_SHOWCONS)) {
					/* select+make active achan */		
					deselect_actionchannels(act, 0);
					select_channel(act, achan, SELECT_ADD);
					
					/* messy... set active bone */
					select_poseelement_by_name(achan->name, 2);
				}	
			}
			break;
		case ACTTYPE_ICU: 
			{
				IpoCurve *icu= (IpoCurve *)act_channel;
				
#if 0 /* disabled until all ipo tools support this ------->  */
				if (mval[0] >= (NAMEWIDTH-16)) {
					/* toggle protection */
					icu->flag ^= IPO_PROTECT;
				}
#endif /* <------- end of disabled code */
				if (mval[0] >= (NAMEWIDTH-16)) {
					/* toggle mute */
					icu->flag ^= IPO_MUTE;
				}
				else {
					/* select/deselect */
					select_icu_channel(act, icu, SELECT_INVERT);
				}

				allspace(REMAKEIPO, 0);
			}
			break;
		case ACTTYPE_CONCHAN:
			{
				bConstraintChannel *conchan= (bConstraintChannel *)act_channel;
				
				if (mval[0] >= (NAMEWIDTH-16)) {
					/* toggle protection */
					conchan->flag ^= CONSTRAINT_CHANNEL_PROTECTED;
				}
				else if ((mval[0] >= (NAMEWIDTH-32)) && (conchan->ipo)) {
					/* toggle mute */
					conchan->ipo->muteipo = (conchan->ipo->muteipo)? 0: 1;
				}
				else {
					/* select/deselect */
					select_constraint_channel(act, conchan, SELECT_INVERT);
				}
			}
				break;
		default:
			return;
	}
	
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWNLA, 0);
	allqueue(REDRAWTIME, 0);
	allqueue(REDRAWOOPS, 0);
	allqueue(REDRAWBUTSALL, 0);
}

/* **************************************************** */
/* ACTION CHANNEL RE-ORDERING */

/* make sure all action-channels belong to a group (and clear action's list) */
static void split_groups_action_temp (bAction *act, bActionGroup *tgrp)
{
	bActionChannel *achan;
	bActionGroup *agrp;
	
	/* Separate action-channels into lists per group */
	for (agrp= act->groups.first; agrp; agrp= agrp->next) {
		if (agrp->channels.first) {
			achan= agrp->channels.last;
			act->chanbase.first= achan->next;
			
			achan= agrp->channels.first;
			achan->prev= NULL;
			
			achan= agrp->channels.last;
			achan->next= NULL;
		}
	}
	
	/* Initialise memory for temp-group */
	memset(tgrp, 0, sizeof(bActionGroup));
	tgrp->flag |= (AGRP_EXPANDED|AGRP_TEMP);
	strcpy(tgrp->name, "#TempGroup");
		
	/* Move any action-channels not already moved, to the temp group */
	if (act->chanbase.first) {
		/* start of list */
		achan= act->chanbase.first;
		achan->prev= NULL;
		tgrp->channels.first= achan;
		act->chanbase.first= NULL;
		
		/* end of list */
		achan= act->chanbase.last;
		achan->next= NULL;
		tgrp->channels.last= achan;
		act->chanbase.last= NULL;
	}
	
	/* Add temp-group to list */
	BLI_addtail(&act->groups, tgrp);
}

/* link lists of channels that groups have */
static void join_groups_action_temp (bAction *act)
{
	bActionGroup *agrp;
	bActionChannel *achan;
	
	for (agrp= act->groups.first; agrp; agrp= agrp->next) {
		ListBase tempGroup;
		
		/* add list of channels to action's channels */
		tempGroup= agrp->channels;
		addlisttolist(&act->chanbase, &agrp->channels);
		agrp->channels= tempGroup;
		
		/* clear moved flag */
		agrp->flag &= ~AGRP_MOVED;
		
		/* if temp-group... remove from list (but don't free as it's on the stack!) */
		if (agrp->flag & AGRP_TEMP) {
			BLI_remlink(&act->groups, agrp);
			break;
		}
	}
	
	/* clear "moved" flag from all achans */
	for (achan= act->chanbase.first; achan; achan= achan->next) 
		achan->flag &= ~ACHAN_MOVED;
}


static short rearrange_actchannel_is_ok (Link *channel, short type)
{
	if (type == ACTTYPE_GROUP) {
		bActionGroup *agrp= (bActionGroup *)channel;
		
		if (SEL_AGRP(agrp) && !(agrp->flag & AGRP_MOVED))
			return 1;
	}
	else if (type == ACTTYPE_ACHAN) {
		bActionChannel *achan= (bActionChannel *)channel;
		
		if (VISIBLE_ACHAN(achan) && SEL_ACHAN(achan) && !(achan->flag & ACHAN_MOVED))
			return 1;
	}
	
	return 0;
}

static short rearrange_actchannel_after_ok (Link *channel, short type)
{
	if (type == ACTTYPE_GROUP) {
		bActionGroup *agrp= (bActionGroup *)channel;
		
		if (agrp->flag & AGRP_TEMP)
			return 0;
	}
	
	return 1;
}


static short rearrange_actchannel_top (ListBase *list, Link *channel, short type)
{
	if (rearrange_actchannel_is_ok(channel, type)) {
		/* take it out off the chain keep data */
		BLI_remlink(list, channel);
		
		/* make it first element */
		BLI_insertlinkbefore(list, list->first, channel);
		
		return 1;
	}
	
	return 0;
}

static short rearrange_actchannel_up (ListBase *list, Link *channel, short type)
{
	if (rearrange_actchannel_is_ok(channel, type)) {
		Link *prev= channel->prev;
		
		if (prev) {
			/* take it out off the chain keep data */
			BLI_remlink(list, channel);
			
			/* push it up */
			BLI_insertlinkbefore(list, prev, channel);
			
			return 1;
		}
	}
	
	return 0;
}

static short rearrange_actchannel_down (ListBase *list, Link *channel, short type)
{
	if (rearrange_actchannel_is_ok(channel, type)) {
		Link *next = (channel->next) ? channel->next->next : NULL;
		
		if (next) {
			/* take it out off the chain keep data */
			BLI_remlink(list, channel);
			
			/* move it down */
			BLI_insertlinkbefore(list, next, channel);
			
			return 1;
		}
		else if (rearrange_actchannel_after_ok(list->last, type)) {
			/* take it out off the chain keep data */
			BLI_remlink(list, channel);
			
			/* add at end */
			BLI_addtail(list, channel);
			
			return 1;
		}
		else {
			/* take it out off the chain keep data */
			BLI_remlink(list, channel);
			
			/* add just before end */
			BLI_insertlinkbefore(list, list->last, channel);
			
			return 1;
		}
	}
	
	return 0;
}

static short rearrange_actchannel_bottom (ListBase *list, Link *channel, short type)
{
	if (rearrange_actchannel_is_ok(channel, type)) {
		if (rearrange_actchannel_after_ok(list->last, type)) {
			/* take it out off the chain keep data */
			BLI_remlink(list, channel);
			
			/* add at end */
			BLI_addtail(list, channel);
			
			return 1;
		}
	}
	
	return 0;
}


/* Change the order of action-channels 
 *	mode: REARRANGE_ACTCHAN_*  
 */
void rearrange_action_channels (short mode)
{
	bAction *act;
	bActionChannel *achan, *chan;
	bActionGroup *agrp, *grp;
	bActionGroup tgrp;
	
	void *data;
	short datatype;
	
	short (*rearrange_func)(ListBase *, Link *, short);
	short do_channels = 1;
	char undostr[60];
	
	/* Get the active action, exit if none are selected */
	data = get_action_context(&datatype);
	if (data == NULL) return;
	if (datatype != ACTCONT_ACTION) return;
	act= (bAction *)data;
	
	/* exit if invalid mode */
	switch (mode) {
		case REARRANGE_ACTCHAN_TOP:
			strcpy(undostr, "Channel(s) to Top");
			rearrange_func= rearrange_actchannel_top;
			break;
		case REARRANGE_ACTCHAN_UP:
			strcpy(undostr, "Channel(s) Move Up");
			rearrange_func= rearrange_actchannel_up;
			break;
		case REARRANGE_ACTCHAN_DOWN:
			strcpy(undostr, "Channel(s) Move Down");
			rearrange_func= rearrange_actchannel_down;
			break;
		case REARRANGE_ACTCHAN_BOTTOM:
			strcpy(undostr, "Channel(s) to Bottom");
			rearrange_func= rearrange_actchannel_bottom;
			break;
		default:
			return;
	}
	
	/* make sure we're only operating with groups */
	split_groups_action_temp(act, &tgrp);
	
	/* rearrange groups first (and then, only consider channels if the groups weren't moved) */
	#define GET_FIRST(list) ((mode > 0) ? (list.first) : (list.last))
	#define GET_NEXT(item) ((mode > 0) ? (item->next) : (item->prev))
	
	for (agrp= GET_FIRST(act->groups); agrp; agrp= grp) {
		/* Get next group to consider */
		grp= GET_NEXT(agrp);
		
		/* try to do group first */
		if (rearrange_func(&act->groups, (Link *)agrp, ACTTYPE_GROUP)) {
			do_channels= 0;
			agrp->flag |= AGRP_MOVED;
		}
	}
	
	if (do_channels) {
		for (agrp= GET_FIRST(act->groups); agrp; agrp= grp) {
			/* Get next group to consider */
			grp= GET_NEXT(agrp);
			
			/* only consider action-channels if they're visible (group expanded) */
			if (EXPANDED_AGRP(agrp)) {
				for (achan= GET_FIRST(agrp->channels); achan; achan= chan) {
					/* Get next channel to consider */
					chan= GET_NEXT(achan);
					
					/* Try to do channel */
					if (rearrange_func(&agrp->channels, (Link *)achan, ACTTYPE_ACHAN))
						achan->flag |= ACHAN_MOVED;
				}
			}
		}
	}
	#undef GET_FIRST
	#undef GET_NEXT
	
	/* assemble lists into one list (and clear moved tags) */
	join_groups_action_temp(act);
	
	/* Undo + redraw */
	BIF_undo_push(undostr);
	allqueue(REDRAWACTION, 0);
}

/* ******************************************************************* */
/* CHANNEL VISIBILITY/FOLDING */

/* Expand all channels to show full hierachy */
void expand_all_action (void)
{
	void *data;
	short datatype;
	
	bAction *act;
	bActionChannel *achan;
	bActionGroup *agrp;
	short mode= 1;
	
	/* Get the selected action, exit if none are selected */
	data = get_action_context(&datatype);
	if (data == NULL) return;
	if (datatype != ACTCONT_ACTION) return;
	act= (bAction *)data;
	
	/* check if expand all, or close all */
	for (agrp= act->groups.first; agrp; agrp= agrp->next) {
		if (EXPANDED_AGRP(agrp)) {
			mode= 0;
			break;
		}
	}
	
	if (mode == 0) {
		for (achan= act->chanbase.first; achan; achan= achan->next) {
			if (VISIBLE_ACHAN(achan)) {
				if (EXPANDED_ACHAN(achan)) {
					mode= 0;
					break;
				}
			}
		}
	}
	
	/* expand/collapse depending on mode */
	for (agrp= act->groups.first; agrp; agrp= agrp->next) {
		if (mode == 1)
			agrp->flag |= AGRP_EXPANDED;
		else
			agrp->flag &= ~AGRP_EXPANDED;
	}
	
	for (achan=act->chanbase.first; achan; achan= achan->next) {
		if (VISIBLE_ACHAN(achan)) {
			if (mode == 1)
				achan->flag |= (ACHAN_EXPANDED|ACHAN_SHOWIPO|ACHAN_SHOWCONS);
			else
				achan->flag &= ~(ACHAN_EXPANDED|ACHAN_SHOWIPO|ACHAN_SHOWCONS);
		}
	}
	
	/* Cleanup and do redraws */
	BIF_undo_push("Expand Action Hierachy");
	allqueue(REDRAWACTION, 0);
}

/* Expands those groups which are hiding a selected actionchannel */
void expand_obscuregroups_action (void)
{
	void *data;
	short datatype;
	
	bAction *act;
	bActionChannel *achan;
	
	/* Get the selected action, exit if none are selected */
	data = get_action_context(&datatype);
	if (data == NULL) return;
	if (datatype != ACTCONT_ACTION) return;
	act= (bAction *)data;
	
	/* check if expand all, or close all */
	for (achan= act->chanbase.first; achan; achan= achan->next) {
		if (VISIBLE_ACHAN(achan) && SEL_ACHAN(achan)) {
			if (achan->grp)
				achan->grp->flag |= AGRP_EXPANDED;
		}
	}
	
	/* Cleanup and do redraws */
	BIF_undo_push("Show Group-Hidden Channels");
	allqueue(REDRAWACTION, 0);
}

/* For visible channels, expand/collapse one level */
void openclose_level_action (short mode)
{
	void *data;
	short datatype;
	
	bAction *act;
	bActionChannel *achan;
	bActionGroup *agrp;
	
	/* Get the selected action, exit if none are selected */
	data = get_action_context(&datatype);
	if (data == NULL) return;
	if (datatype != ACTCONT_ACTION) return;
	act= (bAction *)data;
	
	/* Abort if no operation required */
	if (mode == 0) return;
	
	/* Only affect selected channels */
	for (achan= act->chanbase.first; achan; achan= achan->next) {
		/* make sure if there is a group, it isn't about to be collapsed and is open */
		if ( (achan->grp==NULL) || (EXPANDED_AGRP(achan->grp) && SEL_AGRP(achan->grp)==0) ) {
			if (VISIBLE_ACHAN(achan) && SEL_ACHAN(achan)) {
				if (EXPANDED_ACHAN(achan)) {
					if (FILTER_IPO_ACHAN(achan) || FILTER_CON_ACHAN(achan)) {
						if (mode < 0)
							achan->flag &= ~(ACHAN_SHOWIPO|ACHAN_SHOWCONS);
					}
					else {
						if (mode > 0)
							achan->flag |= (ACHAN_SHOWIPO|ACHAN_SHOWCONS);
						else
							achan->flag &= ~ACHAN_EXPANDED;
					}					
				}
				else {
					if (mode > 0)
						achan->flag |= ACHAN_EXPANDED;
				}
			}
		}
	}
	
	/* Expand/collapse selected groups */
	for (agrp= act->groups.first; agrp; agrp= agrp->next) {
		if (SEL_AGRP(agrp)) {
			if (mode < 0)
				agrp->flag &= ~AGRP_EXPANDED;
			else
				agrp->flag |= AGRP_EXPANDED;
		}
	}
	
	/* Cleanup and do redraws */
	BIF_undo_push("Expand/Collapse Action Level");
	allqueue(REDRAWACTION, 0);
}

/* **************************************************** */
/* ACTION MARKERS (PoseLib features) */
/* NOTE: yes, these duplicate code from edittime.c a bit, but these do a bit more...
 * These could get merged with those someday if need be...  (Aligorith, 20071230)
 */

/* Makes the given marker the active one
 *	- deselect indicates whether unactive ones should be deselected too
 */
void action_set_activemarker (bAction *act, TimeMarker *active, short deselect)
{
	TimeMarker *marker;
	int index= 0;
	
	/* sanity checks */
	if (act == NULL)
		return;
	act->active_marker= 0;
	
	/* set appropriate flags for all markers */
	for (marker=act->markers.first; marker; marker=marker->next, index++) {
		/* only active may be active */
		if (marker == active) {
			act->active_marker= index + 1;
			marker->flag |= (SELECT|ACTIVE);
		}
		else {
			if (deselect)
				marker->flag &= ~(SELECT|ACTIVE);
			else
				marker->flag &= ~ACTIVE;
		}
	}	
}

/* Adds a local marker to the active action */
void action_add_localmarker (bAction *act, int frame)
{
	TimeMarker *marker;
	char name[64];
	
	/* sanity checks */
	if (act == NULL) 
		return;
	
	/* get name of marker */
	sprintf(name, "Pose");
	if (sbutton(name, 0, sizeof(name)-1, "Name: ") == 0)
		return;
	
	/* add marker to action - replaces any existing marker there */
	for (marker= act->markers.first; marker; marker= marker->next) {
		if (marker->frame == frame) {
			BLI_strncpy(marker->name, name, sizeof(marker->name));
			break;
		}
	}
	if (marker == NULL) {
		marker= MEM_callocN(sizeof(TimeMarker), "ActionMarker");
		
		BLI_strncpy(marker->name, name, sizeof(marker->name));
		marker->frame= frame;
		
		BLI_addtail(&act->markers, marker);
	}
	
	/* validate the name */
	BLI_uniquename(&act->markers, marker, "Pose", offsetof(TimeMarker, name), 64);
	
	/* sets the newly added marker as the active one */
	action_set_activemarker(act, marker, 1);
	
	BIF_undo_push("Action Add Marker");
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWBUTSEDIT, 0);
}

/* Renames the active local marker to the active action */
void action_rename_localmarker (bAction *act)
{
	TimeMarker *marker;
	char name[64];
	int val;
	
	/* sanity checks */
	if (act == NULL) 
		return;
	
	/* get active marker to rename */
	if (act->active_marker == 0)
		return;
	else
		val= act->active_marker;
	
	if (val <= 0) return;
	marker= BLI_findlink(&act->markers, val-1);
	if (marker == NULL) return;
	
	/* get name of marker */
	sprintf(name, marker->name);
	if (sbutton(name, 0, sizeof(name)-1, "Name: ") == 0)
		return;
	
	/* copy then validate name */
	BLI_strncpy(marker->name, name, sizeof(marker->name));
	BLI_uniquename(&act->markers, marker, "Pose", offsetof(TimeMarker, name), 64);
	
	/* undo and update */
	BIF_undo_push("Action Rename Marker");
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWACTION, 0);
}

/* Deletes all selected markers, and adjusts things as appropriate */
void action_remove_localmarkers (bAction *act)
{
	TimeMarker *marker, *next;
	
	/* sanity checks */
	if (act == NULL)
		return;
		
	/* remove selected markers */
	for (marker= act->markers.first; marker; marker= next) {
		next= marker->next;
		
		if (marker->flag & SELECT)
			BLI_freelinkN(&act->markers, marker);
	}
	
	/* clear active just in case */
	act->active_marker= 0;
	
	/* undo and update */
	BIF_undo_push("Action Remove Marker(s)");
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWACTION, 0);
}

/* **************************************************** */
/* EVENT HANDLING */

void winqreadactionspace(ScrArea *sa, void *spacedata, BWinEvent *evt)
{
	extern void do_actionbuts(unsigned short event); // drawaction.c
	SpaceAction *saction;
	void *data;
	short datatype;
	float dx, dy;
	int doredraw= 0;
	int	cfra;
	short mval[2];
	unsigned short event= evt->event;
	short val= evt->val;
	short mousebut = L_MOUSE;

	if (curarea->win==0) return;

	saction= curarea->spacedata.first;
	if (!saction)
		return;

	data= get_action_context(&datatype);
	
	if (val) {
		if ( uiDoBlocks(&curarea->uiblocks, event, 1)!=UI_NOTHING ) event= 0;
		
		/* swap mouse buttons based on user preference */
		if (U.flag & USER_LMOUSESELECT) {
			if (event == LEFTMOUSE) {
				event = RIGHTMOUSE;
				mousebut = L_MOUSE;
			} 
			else if (event == RIGHTMOUSE) {
				event = LEFTMOUSE;
				mousebut = R_MOUSE;
			}
		}
		
		getmouseco_areawin(mval);
		
		switch(event) {
		case UI_BUT_EVENT:
			do_actionbuts(val); 	/* window itself */
			break;
			
		/* LEFTMOUSE and RIGHTMOUSE event codes can be swapped above,
		 * based on user preference USER_LMOUSESELECT
		 */
		case LEFTMOUSE:
			if (view2dmove(LEFTMOUSE)) /* only checks for sliders */
				break;
			else if ((G.v2d->mask.xmin==0) || (mval[0] > ACTWIDTH)) {
				/* moving time-marker / current frame */
				do {
					getmouseco_areawin(mval);
					areamouseco_to_ipoco(G.v2d, mval, &dx, &dy);
					
					cfra= (int)(dx+0.5f);
					if (cfra < 1) cfra= 1;
					
					if (cfra != CFRA) {
						CFRA= cfra;
						update_for_newframe();
						force_draw_all(0);					
					}
					else 
						PIL_sleep_ms(30);
				} 
				while(get_mbut() & mousebut);
				break;
			}
			/* passed on as selection */
		case RIGHTMOUSE:
			/* Clicking in the channel area */
			if ((G.v2d->mask.xmin) && (mval[0] < NAMEWIDTH)) {
				if (datatype == ACTCONT_ACTION) {
					/* mouse is over action channels */
					if (G.qual == LR_CTRLKEY)
						numbuts_action();
					else 
						mouse_actionchannels(mval);
				}
				else 
					numbuts_action();
			}
			else {
				short select_mode= (G.qual & LR_SHIFTKEY)? SELECT_INVERT: SELECT_REPLACE;
				
				/* Clicking in the vertical scrollbar selects
				 * all of the keys for that channel at that height
				 */
				if (IN_2D_VERT_SCROLL(mval))
					selectall_action_keys(mval, 0, select_mode);
				
				/* Clicking in the horizontal scrollbar selects
				 * all of the keys within 0.5 of the nearest integer
				 * frame
				 */
				else if (IN_2D_HORIZ_SCROLL(mval))
					selectall_action_keys(mval, 1, select_mode);
				
				/* Clicking in the main area of the action window
				 * selects keys and markers
				 */
				else if (G.qual & LR_ALTKEY) {
					areamouseco_to_ipoco(G.v2d, mval, &dx, &dy);
					
					/* sends a 1 for left and 0 for right */
					selectkeys_leftright((dx < (float)CFRA), select_mode);
				}
				else
					mouse_action(select_mode);
			}
			break;
			
		case MIDDLEMOUSE:
		case WHEELUPMOUSE:
		case WHEELDOWNMOUSE:
			view2dmove(event);	/* in drawipo.c */
			break;
		
		case AKEY:
			if (mval[0] < NAMEWIDTH) {
				deselect_action_channels(1);
				BIF_undo_push("(De)Select Action Channels");
				allqueue(REDRAWVIEW3D, 0);
				allqueue(REDRAWACTION, 0);
				allqueue(REDRAWNLA, 0);
				allqueue(REDRAWIPO, 0);
			}
			else if (mval[0] > ACTWIDTH) {
				if (G.qual == LR_CTRLKEY) {
					deselect_markers(1, 0);
					BIF_undo_push("(De)Select Markers");
					allqueue(REDRAWTIME, 0);
					allqueue(REDRAWIPO, 0);
					allqueue(REDRAWACTION, 0);
					allqueue(REDRAWNLA, 0);
					allqueue(REDRAWSOUND, 0);
				}
				else {
					deselect_action_keys(1, 1);
					BIF_undo_push("(De)Select Keys");
					allqueue(REDRAWACTION, 0);
					allqueue(REDRAWNLA, 0);
					allqueue(REDRAWIPO, 0);
				}
			}
			break;
		
		case BKEY:
			if (G.qual & LR_CTRLKEY) {
				borderselect_markers();
			}
			else {
				if (mval[0] <= ACTWIDTH)
					borderselect_actionchannels();
				else
					borderselect_action();
			}
			break;
		
		case CKEY:
			/* scroll the window so the current
			 * frame is in the center.
			 */
			center_currframe();
			break;
		
		case DKEY:
			if (mval[0] > ACTWIDTH) {
				if (G.qual == (LR_CTRLKEY|LR_SHIFTKEY))
					duplicate_marker();
				else if (G.qual == LR_SHIFTKEY)
					duplicate_action_keys();
			}
			break;
			
		case EKEY:
			if (mval[0] >= ACTWIDTH) 
				transform_action_keys('e', 0);
			break;
		
		case GKEY:
			/* Action Channel Groups */
			if (G.qual == LR_SHIFTKEY)
				action_groups_group(0);
			else if (G.qual == (LR_CTRLKEY|LR_SHIFTKEY))
				action_groups_group(1);
			else if (G.qual == LR_ALTKEY)
				action_groups_ungroup();
			
			/* Transforms */
			else {
				if (mval[0] >= ACTWIDTH) {
					if (G.qual == LR_CTRLKEY)
						transform_markers('g', 0);
					else
						transform_action_keys('g', 0);
				}
			}
			break;
		
		case HKEY:
			if (G.qual & LR_SHIFTKEY) {
				if (okee("Set Keys to Auto Handle"))
					sethandles_action_keys(HD_AUTO);
			}
			else {
				if (okee("Toggle Keys Aligned Handle"))
					sethandles_action_keys(HD_ALIGN);
			}
			break;
			
		case IKEY: 	
			if (G.qual & LR_CTRLKEY) {
				if (mval[0] < ACTWIDTH) {
					deselect_action_channels(2);
					BIF_undo_push("Inverse Action Channels");
					allqueue(REDRAWVIEW3D, 0);
					allqueue(REDRAWACTION, 0);
					allqueue(REDRAWNLA, 0);
					allqueue(REDRAWIPO, 0);
				}
				else if (G.qual & LR_SHIFTKEY) {
					deselect_markers(0, 2);
					BIF_undo_push("Inverse Markers");
					allqueue(REDRAWMARKER, 0);
				}
				else {
					deselect_action_keys(0, 2);
					BIF_undo_push("Inverse Keys");
					allqueue(REDRAWACTION, 0);
					allqueue(REDRAWNLA, 0);
					allqueue(REDRAWIPO, 0);
				}
			}
			break;
		
		case KKEY:
			if (G.qual == LR_ALTKEY)
				markers_selectkeys_between();
			else if (G.qual == LR_SHIFTKEY)
				column_select_action_keys(2);
			else if (G.qual == LR_CTRLKEY)
				column_select_action_keys(3);
			else
				column_select_action_keys(1);
			
			allqueue(REDRAWMARKER, 0);
			break;
			
		case LKEY:
			/* poselib manipulation - only for actions */
			if (datatype == ACTCONT_ACTION) {
				if (G.qual == LR_SHIFTKEY) 
					action_add_localmarker(data, CFRA);
				else if (G.qual == (LR_CTRLKEY|LR_SHIFTKEY))
					action_rename_localmarker(data);
				else if (G.qual == LR_ALTKEY)
					action_remove_localmarkers(data);
				else if (G.qual == LR_CTRLKEY) {
					G.saction->flag |= SACTION_POSEMARKERS_MOVE;
					transform_markers('g', 0);
					G.saction->flag &= ~SACTION_POSEMARKERS_MOVE;
				}
			}
			break;
			
		case MKEY:
			if (G.qual & LR_SHIFTKEY) {
				/* mirror keyframes */
				if (data) {
					if (G.saction->flag & SACTION_DRAWTIME)
						val = pupmenu("Mirror Keys Over%t|Current Time%x1|Vertical Axis%x2|Horizontal Axis %x3|Selected Marker %x4");
					else
						val = pupmenu("Mirror Keys Over%t|Current Frame%x1|Vertical Axis%x2|Horizontal Axis %x3|Selected Marker %x4");
					
					mirror_action_keys(val);
				}
			}
			else {
				/* marker operations */
				if (G.qual == 0)
					add_marker(CFRA);
				else if (G.qual == LR_CTRLKEY)
					rename_marker();
				else 
					break;
				allqueue(REDRAWMARKER, 0);
			}
			break;
			
		case NKEY:
			if (G.qual==0) {
				numbuts_action();
				
				/* no panel (yet). current numbuts are not easy to put in panel... */
				//add_blockhandler(curarea, ACTION_HANDLER_PROPERTIES, UI_PNL_TO_MOUSE);
				//scrarea_queue_winredraw(curarea);
			}
			break;
			
		case OKEY:
			if (G.qual & LR_ALTKEY)
				sample_action_keys();
			else
				clean_action();
			break;
			
		case PKEY:
			if (G.qual == (LR_CTRLKEY|LR_ALTKEY)) /* set preview range to range of action */
				action_previewrange_set(G.saction->action);
			else if (G.qual & LR_CTRLKEY) /* set preview range */
				anim_previewrange_set();
			else if (G.qual & LR_ALTKEY) /* clear preview range */
				anim_previewrange_clear();
				
			allqueue(REDRAWMARKER, 0);
			allqueue(REDRAWBUTSALL, 0);
			break;
			
		case SKEY: 
			if (mval[0]>=ACTWIDTH) {
				if (G.qual == (LR_SHIFTKEY|LR_CTRLKEY)) {
					if (data) {
						snap_cfra_action();
					}
				}
				else if (G.qual & LR_SHIFTKEY) {
					if (data) {
						if (G.saction->flag & SACTION_DRAWTIME)
							val = pupmenu("Snap Keys To%t|Nearest Second%x4|Current Time%x2|Nearest Marker %x3");
						else
							val = pupmenu("Snap Keys To%t|Nearest Frame%x1|Current Frame%x2|Nearest Marker %x3");
						
						snap_action_keys(val);
					}
				}
				else {
					transform_action_keys('s', 0);	
				}
			}
			break;
		
		case TKEY:
			if (G.qual & LR_SHIFTKEY)
				action_set_ipo_flags(SET_IPO_POPUP, 0);
			else if (G.qual & LR_CTRLKEY) {
				val= pupmenu("Time value%t|Frames %x1|Seconds%x2");
				
				if (val > 0) {
					if (val == 2) saction->flag |= SACTION_DRAWTIME;
					else saction->flag &= ~SACTION_DRAWTIME;
					
					doredraw= 1;
				}
			}				
			else
				transform_action_keys ('t', 0);
			break;
		
		case VKEY:
			if (okee("Set Keys to Vector Handle"))
				sethandles_action_keys(HD_VECT);
			break;
			
		case WKEY:
			/* toggle/turn-on\off-based-on-setting */
			if (G.qual) {
				if (G.qual == (LR_CTRLKEY|LR_SHIFTKEY))
					val= 1;
				else if (G.qual == LR_ALTKEY)
					val= 2;
				else
					val= 0;	
				
				setflag_action_channels(val);
			}
			break;
		
		case PAGEUPKEY:
			if (datatype == ACTCONT_ACTION) {
				if (G.qual == (LR_CTRLKEY|LR_SHIFTKEY))
					rearrange_action_channels(REARRANGE_ACTCHAN_TOP);
				else if (G.qual == LR_SHIFTKEY) 
					rearrange_action_channels(REARRANGE_ACTCHAN_UP);
				else if (G.qual == LR_CTRLKEY) 
					nextprev_action_keyframe(1);
				else
					nextprev_marker(1);
			}
			else if (datatype == ACTCONT_SHAPEKEY) {
				/* only jump to markers possible (key channels can't be moved yet) */
				if (G.qual == LR_CTRLKEY) 
					nextprev_action_keyframe(1);
				else
					nextprev_marker(1);
			}
			break;
		case PAGEDOWNKEY:
			if (datatype == ACTCONT_ACTION) {
				if (G.qual == (LR_CTRLKEY|LR_SHIFTKEY))
					rearrange_action_channels(REARRANGE_ACTCHAN_BOTTOM);
				else if (G.qual == LR_SHIFTKEY) 
					rearrange_action_channels(REARRANGE_ACTCHAN_DOWN);
				else if (G.qual == LR_CTRLKEY) 
					nextprev_action_keyframe(-1);
				else
					nextprev_marker(-1);
			}
			else if (datatype == ACTCONT_SHAPEKEY) {
				/* only jump to markers possible (key channels can't be moved yet) */
				if (G.qual == LR_CTRLKEY) 
					nextprev_action_keyframe(-1);
				else
					nextprev_marker(-1);
			}
			break;
			
		case DELKEY:
		case XKEY:
			if (okee("Erase selected")) {
				if (mval[0] < NAMEWIDTH)
					delete_action_channels();
				else
					delete_action_keys();
				
				if (mval[0] >= NAMEWIDTH)
					remove_marker();
				
				allqueue(REDRAWMARKER, 0);
			}
			break;
		
		case ACCENTGRAVEKEY:
			if (datatype == ACTCONT_ACTION) {
				if (G.qual == LR_SHIFTKEY)
					expand_obscuregroups_action();
				else
					expand_all_action();
			}
			break;
		
		case PADPLUSKEY:
			if (G.qual == LR_CTRLKEY) {
				if (datatype == ACTCONT_ACTION)
					openclose_level_action(1);
			}
			else {
				view2d_zoom(G.v2d, 0.1154, sa->winx, sa->winy);
				test_view2d(G.v2d, sa->winx, sa->winy);
				view2d_do_locks(curarea, V2D_LOCK_COPY);
				
				doredraw= 1;
			}
			break;
		case PADMINUS:
			if (G.qual == LR_CTRLKEY) {
				if (datatype == ACTCONT_ACTION)
					openclose_level_action(-1);
			}
			else {
				view2d_zoom(G.v2d, -0.15, sa->winx, sa->winy);
				test_view2d(G.v2d, sa->winx, sa->winy);
				view2d_do_locks(curarea, V2D_LOCK_COPY);
				
				doredraw= 1;
			}
			break;	
		
		case HOMEKEY:
			do_action_buttons(B_ACTHOME);	/* header */
			break;	
		}
	}

	if (doredraw) addqueue(curarea->win, REDRAW, 1);
}

/* **************************************************** */
