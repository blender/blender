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
 * Contributor(s): 2007, Joshua Leung (major rewrite of Action Editor)
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <string.h>
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
		if(VISIBLE_ACHAN(achan)) {
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
		for (conchan=achan->constraintChannels.first; conchan; conchan=conchan->next){
			if (conchan->ipo) {
				for (icu = conchan->ipo->curve.first; icu; icu=icu->next){
					sort_time_ipocurve(icu);
					testhandles_ipocurve(icu);
				}
			}
		}
	}
	
	synchronize_action_strips();
}

static void remake_meshaction_ipos (Ipo *ipo)
{
	/* this puts the bezier triples in proper
	 * order and makes sure the bezier handles
	 * aren't too strange.
	 */
	IpoCurve *icu;

	for (icu = ipo->curve.first; icu; icu=icu->next) {
		sort_time_ipocurve(icu);
		testhandles_ipocurve(icu);
	}
}

static void meshkey_do_redraw (Key *key)
{
	if(key->ipo)
		remake_meshaction_ipos(key->ipo);

	DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);
	
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWNLA, 0);

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
		
		/* do specifics */
		switch (datatype) {
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
			{
				bConstraintChannel *conchan= (bConstraintChannel *)data;
				
				ale->flag= conchan->flag;
				
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
 
static void actdata_filter_action (ListBase *act_data, bAction *act, int filter_mode)
{
	bActListElem *ale;
	bActionChannel *achan;
	bConstraintChannel *conchan;
	IpoCurve *icu;
	
	/* loop over action channels, performing the necessary checks */
	for (achan= act->chanbase.first; achan; achan= achan->next) {
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
				
				/* check if expanded - if not, continue on to next action channel */
				if (EXPANDED_ACHAN(achan) == 0 && (filter_mode & ACTFILTER_ONLYICU)==0) 
					continue;
					
				/* ipo channels */
				if (achan->ipo) {
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
					if ((filter_mode & ACTFILTER_CHANNELS) && (filter_mode & ACTFILTER_ONLYICU)==0) {
						ale= make_new_actlistelem(achan, ACTTYPE_FILLCON, achan, ACTTYPE_ACHAN);
						if (ale) BLI_addtail(act_data, ale);
					}
					
					/* add constaint channels? */
					if (FILTER_CON_ACHAN(achan)) {
						/* loop through constraint channels, checking and adding them */
						for (conchan=achan->constraintChannels.first; conchan; conchan=conchan->next) {
							/* only work with this channel and its subchannels if it is editable */
							if (!(filter_mode & ACTFILTER_FOREDIT) || EDITABLE_CONCHAN(conchan)) {
								/* check if this conchan should only be included if it is selected */
								if (!(filter_mode & ACTFILTER_SEL) || SEL_CONCHAN(conchan)) {
									if ((filter_mode & ACTFILTER_ONLYICU)==0) {
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
Key *get_action_mesh_key(void) 
{
    Object *ob;
    Key    *key;

    ob = OBACT;
    if (!ob) return NULL;

	if (G.saction->pin) return NULL;

    if (ob->type==OB_MESH ) {
		key = ((Mesh *)ob->data)->key;
    }
	else if (ob->type==OB_LATTICE ) {
		key = ((Lattice *)ob->data)->key;
	}
	else return NULL;

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
	filter= (ACTFILTER_VISIBLE | ACTFILTER_CHANNELS);
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
						ipo_to_keylist(ipo, &act_keys, NULL);
					}
						break;
					case ALE_ICU:
					{
						IpoCurve *icu= (IpoCurve *)ale->key_data;
						icu_to_keylist(icu, &act_keys, NULL);
					}
						break;
				}
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

/* **************************************************** */
/* TRANSFORM TOOLS */

/* initialise the transform data - create transverts */
static TransVert *transform_action_init (int *tvtot, float *minx, float *maxx)
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	TransVert *tv;
	void *data;
	short datatype;
	int filter;
	int count= 0;
	float min= 0, max= 0;
	int i;
	
	/* initialise the values being passed by reference */
	*tvtot = *minx = *maxx = 0;
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	if (data == NULL) return NULL;
	
	/* filter data */
	filter= (ACTFILTER_VISIBLE | ACTFILTER_FOREDIT | ACTFILTER_IPOKEYS);
	actdata_filter(&act_data, filter, data, datatype);
	
	/* loop 1: fully select ipo-keys and count how many BezTriples are selected */
	for (ale= act_data.first; ale; ale= ale->next)
		count += fullselect_ipo_keys(ale->key_data);
	
	/* stop if trying to build list if nothing selected */
	if (count == 0) {
		/* cleanup temp list */
		BLI_freelistN(&act_data);
		return NULL;
	}
		
	/* Build the transvert structure */
	tv = MEM_callocN (sizeof(TransVert) * count, "transVert");
	
	/* loop 2: build transvert structure */
	for (ale= act_data.first; ale; ale= ale->next)
		*tvtot = add_trans_ipo_keys(ale->key_data, tv, *tvtot);
		
	/* min max, only every other three */
	min= max= tv[1].loc[0];
	for (i=1; i<count; i+=3){
		if(min>tv[i].loc[0]) min= tv[i].loc[0];
		if(max<tv[i].loc[0]) max= tv[i].loc[0];
	}
	*minx= min;
	*maxx= max;
	
	/* cleanup temp list */
	BLI_freelistN(&act_data);
	
	/* return created transverts */
	return tv;
} 

/* main transform loop for action editor */
static short transform_action_loop (TransVert *tv, int tvtot, char mode, short context, float minx, float maxx)
{
	Object *ob= OBACT;
	float deltax, startx;
	float cenf[2];
	float sval[2], cval[2], lastcval[2]={0,0};
	float fac=0.0f;
	int	loop=1, invert=0;
	int	i;
	short cancel=0, firsttime=1;
	short mvals[2], mvalc[2], cent[2];
	char str[256];
	
	/* Do the event loop */
	cent[0] = curarea->winx + (G.saction->v2d.hor.xmax)/2;
	cent[1] = curarea->winy + (G.saction->v2d.hor.ymax)/2;
	areamouseco_to_ipoco(G.v2d, cent, &cenf[0], &cenf[1]);

	getmouseco_areawin (mvals);
	areamouseco_to_ipoco(G.v2d, mvals, &sval[0], &sval[1]);

	if (NLA_ACTION_SCALED)
		sval[0]= get_action_frame(OBACT, sval[0]);
	
	/* used for drawing */
	if(mode=='t') {
		G.saction->flag |= SACTION_MOVING;
		G.saction->timeslide= sval[0];
	}
	
	startx=sval[0];
	while (loop) {
		if(mode=='t' && minx==maxx)
			break;
		
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
				tv[i].loc[0]=tv[i].oldloc[0];
				tv[i].loc[1]=tv[i].oldloc[1];
			}
		} 
		else {
			getmouseco_areawin (mvalc);
			areamouseco_to_ipoco(G.v2d, mvalc, &cval[0], &cval[1]);
			
			if (NLA_ACTION_SCALED)
				cval[0]= get_action_frame(OBACT, cval[0]);

			if (mode=='t')
				G.saction->timeslide= cval[0];
			
			if (!firsttime && lastcval[0]==cval[0] && lastcval[1]==cval[1]) {
				PIL_sleep_ms(1);
			} 
			else {
				short autosnap= 0;
				
				/* determine mode of keyframe snapping/autosnap */
				if (mode != 't') {
					switch (G.saction->autosnap) {
					case SACTSNAP_OFF:
						if (G.qual == LR_CTRLKEY) 
							autosnap= SACTSNAP_STEP;
						else if (G.qual == LR_SHIFTKEY)
							autosnap= SACTSNAP_FRAME;
						else
							autosnap= SACTSNAP_OFF;
						break;
					case SACTSNAP_STEP:
						autosnap= (G.qual==LR_CTRLKEY)? SACTSNAP_OFF: SACTSNAP_STEP;
						break;
					case SACTSNAP_FRAME:
						autosnap= (G.qual==LR_SHIFTKEY)? SACTSNAP_OFF: SACTSNAP_FRAME;
						break;
					}
				}
				
				for (i=0; i<tvtot; i++) {
					tv[i].loc[0]=tv[i].oldloc[0];

					switch (mode) {
					case 't':
						if( sval[0] > minx && sval[0] < maxx) {
							float timefac, cvalc= CLAMPIS(cval[0], minx, maxx);
							
							/* left half */
							if(tv[i].oldloc[0] < sval[0]) {
								timefac= ( sval[0] - tv[i].oldloc[0])/(sval[0] - minx);
								tv[i].loc[0]= cvalc - timefac*( cvalc - minx);
							}
							else {
								timefac= (tv[i].oldloc[0] - sval[0])/(maxx - sval[0]);
								tv[i].loc[0]= cvalc + timefac*(maxx- cvalc);
							}
						}
						break;
					case 'g':
						if (NLA_ACTION_SCALED && context==ACTCONT_ACTION) {
							deltax = get_action_frame_inv(OBACT, cval[0]);
							deltax -= get_action_frame_inv(OBACT, sval[0]);
							
							if (autosnap == SACTSNAP_STEP) 
								deltax= 1.0f*floor(deltax/1.0f + 0.5f);
							
							fac = get_action_frame_inv(OBACT, tv[i].loc[0]);
							fac += deltax;
							tv[i].loc[0] = get_action_frame(OBACT, fac);
						}
						else {
							deltax = cval[0] - sval[0];
							fac= deltax;
							
							if (autosnap == SACTSNAP_STEP)
								fac= 1.0f*floor(fac/1.0f + 0.5f);
							
							tv[i].loc[0]+=fac;
						}
						break;
					case 's':
						startx=mvals[0]-(ACTWIDTH/2+(curarea->winrct.xmax-curarea->winrct.xmin)/2);
						deltax=mvalc[0]-(ACTWIDTH/2+(curarea->winrct.xmax-curarea->winrct.xmin)/2);
						fac= fabs(deltax/startx);
						
						if (autosnap == SACTSNAP_STEP) {
							fac= 1.0f*floor(fac/1.0f + 0.5f);
						}
						
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
						if(NLA_ACTION_SCALED && context==ACTCONT_ACTION)
							startx= get_action_frame(OBACT, startx);
							
						tv[i].loc[0]-= startx;
						tv[i].loc[0]*=fac;
						tv[i].loc[0]+= startx;
		
						break;
					}
					
					/* snap key to nearest frame? */
					if (autosnap == SACTSNAP_FRAME) {
						float snapval;
						
						/* convert frame to nla-action time (if needed) */
						if (NLA_ACTION_SCALED && context==ACTCONT_ACTION) 
							snapval= get_action_frame_inv(OBACT, tv[i].loc[0]);
						else
							snapval= tv[i].loc[0];
						
						/* snap to nearest frame */
						snapval= (float)(floor(snapval+0.5));
							
						/* convert frame out of nla-action time */
						if (NLA_ACTION_SCALED && context==ACTCONT_ACTION)
							tv[i].loc[0]= get_action_frame(OBACT, snapval);
						else
							tv[i].loc[0]= snapval;
					}
				}
	
				if (mode=='s') {
					sprintf(str, "scaleX: %.3f", fac);
					headerprint(str);
				}
				else if (mode=='g') {
					if (NLA_ACTION_SCALED && context==ACTCONT_ACTION) {
						/* recalculate the delta based on 'visual' times */
						fac = get_action_frame_inv(OBACT, cval[0]);
						fac -= get_action_frame_inv(OBACT, sval[0]);
						
						if (autosnap == SACTSNAP_STEP) 
							fac= 1.0f*floor(fac/1.0f + 0.5f);
					}
					sprintf(str, "deltaX: %.3f", fac);
					headerprint(str);
				}
				else if (mode=='t') {
					float fac= 2.0*(cval[0]-sval[0])/(maxx-minx);
					CLAMP(fac, -1.0f, 1.0f);
					sprintf(str, "TimeSlide: %.3f", fac);
					headerprint(str);
				}
		
				if (G.saction->lock) {
					if (context == ACTCONT_ACTION) {
						if(ob) {
							ob->ctime= -1234567.0f;
							if(ob->pose || ob_get_key(ob))
								DAG_object_flush_update(G.scene, ob, OB_RECALC);
							else
								DAG_object_flush_update(G.scene, ob, OB_RECALC_OB);
						}
						force_draw_plus(SPACE_VIEW3D, 0);
					}
					else if (context == ACTCONT_SHAPEKEY) {
						DAG_object_flush_update(G.scene, OBACT, OB_RECALC_OB|OB_RECALC_DATA);
		                allqueue (REDRAWVIEW3D, 0);
		                allqueue (REDRAWACTION, 0);
		                allqueue (REDRAWIPO, 0);
		                allqueue(REDRAWNLA, 0);
						allqueue(REDRAWTIME, 0);
		                force_draw_all(0);
					}
				}
				else {
					force_draw(0);
				}
			}
		}
		
		lastcval[0]= cval[0];
		lastcval[1]= cval[1];
		firsttime= 0;
	}
	
	return cancel;
}

/* main call to start transforming keyframes */
/* 	NOTE: someday, this should be integrated with the transform system
 * 		instead of relying on our own methods
 */
void transform_action_keys (int mode, int dummy)
{
	Object *ob= OBACT;
	TransVert *tv;
	int tvtot= 0;
	short cancel;
	float minx, maxx;
	
	void *data;
	short datatype;
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	if (data == NULL) return;
	
	/* initialise transform */
	tv= transform_action_init(&tvtot, &minx, &maxx);
	if (tv == NULL) return;
	
	/* do transform loop */
	cancel= transform_action_loop(tv, tvtot, mode, datatype, minx, maxx); 
	
	/* cleanup */
	if (datatype == ACTCONT_ACTION) {
		/* Update the curve */
		/* Depending on the lock status, draw necessary views */
		if(ob) {
			ob->ctime= -1234567.0f;

			if(ob->pose || ob_get_key(ob))
				DAG_object_flush_update(G.scene, ob, OB_RECALC);
			else
				DAG_object_flush_update(G.scene, ob, OB_RECALC_OB);
		}
		
		remake_action_ipos((bAction *)data);

		G.saction->flag &= ~SACTION_MOVING;
		
		if (cancel==0) BIF_undo_push("Transform Action"); // does it have to be here?
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWNLA, 0);
		allqueue(REDRAWIPO, 0);
		allqueue(REDRAWTIME, 0);
	}
	else if (datatype == ACTCONT_SHAPEKEY) {
		/* fix up the Ipocurves and redraw stuff */
	    meshkey_do_redraw((Key *)data);
		if (cancel==0) BIF_undo_push("Transform Action");
	}
	
	MEM_freeN(tv);
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
	transform_action_keys('g', 0);
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
			strcpy(str, "Snap Keys To Current Frame");
			break;
		case 3:
			strcpy(str, "Snap Keys To Nearest Marker");
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
		mode = pupmenu("Insert Key%t|All Channels%x1|Only Selected Channels%x2");
		if (mode == 0) return;
		
		/* filter data */
		filter= (ACTFILTER_VISIBLE | ACTFILTER_FOREDIT | ACTFILTER_ONLYICU );
		if (mode == 2) filter |= ACTFILTER_SEL;
		
		actdata_filter(&act_data, filter, data, datatype);
		
		/* loop through ipo curves retrieved */
		for (ale= act_data.first; ale; ale= ale->next) {
			/* verify that this is indeed an ipo curve */
			if (ale->key_data && ale->owner) {
				bActionChannel *achan= (bActionChannel *)ale->owner;
				IpoCurve *icu= (IpoCurve *)ale->key_data;
				
				if (ob)
					insertkey((ID *)ob, icu->blocktype, achan->name, NULL, icu->adrcode);
				else
					insert_vert_ipo(icu, cfra, icu->curval);
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
		if (mode == 0) return;
		
		if (key->ipo) {
			for (icu= key->ipo->curve.first; icu; icu=icu->next) {
				insert_vert_ipo(icu, cfra, icu->curval);
			}
		}
	}
	
	BIF_undo_push("Insert Key");
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWNLA, 0);
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

/* delete selected keyframes */
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
		
		/* release reference to ipo users */
		if (achan->ipo)
			achan->ipo->id.us--;
			
		for (conchan= achan->constraintChannels.first; conchan; conchan=cnext) {
			cnext= conchan->next;
			
			if (conchan->ipo)
				conchan->ipo->id.us--;
		}
		
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

/* **************************************************** */
/* VARIOUS SETTINGS */

/* this function combines several features related to setting 
 * various ipo extrapolation/interpolation
 */
void action_set_ipo_flags (int mode)
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	void *data;
	short datatype;
	int filter, event;
	
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
			if(event < 1) return;
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
			if(event < 1) return;
		}
			break;
			
		default: /* weird, unhandled case */
			return;
	}
	
	/* filter data */
	filter= (ACTFILTER_VISIBLE | ACTFILTER_FOREDIT | ACTFILTER_IPOKEYS);
	actdata_filter(&act_data, filter, data, datatype);
	
	/* loop through setting flags */
	for (ale= act_data.first; ale; ale= ale->next) {
		Ipo *ipo= (Ipo *)ale->key_data; 
	
		/* depending on the mode */
		switch (mode) {
			case SET_EXTEND_POPUP: /* extrapolation */
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

static void clever_keyblock_names (Key *key, short *mval)
{
    KeyBlock   *kb;
	int        but=0, i, keynum;
    char       str[64];
	float      x;
	
	/* get the keynum cooresponding to the y value
	 * of the mouse pointer, return if this is
	 * an invalid key number (and we don't deal
	 * with the speed ipo).
	 */

    keynum = get_nearest_key_num(key, mval, &x);
    if ( (keynum < 1) || (keynum >= key->totkey) )
        return;

	kb= key->block.first;
	for (i=0; i<keynum; ++i) kb = kb->next; 

	if (kb->name[0] == '\0') {
		sprintf(str, "Key %d", keynum);
	}
	else {
		strcpy(str, kb->name);
	}

	if ( (kb->slidermin >= kb->slidermax) ) {
		kb->slidermin = 0.0;
		kb->slidermax = 1.0;
	}

    add_numbut(but++, TEX, "KB: ", 0, 24, str, 
               "Does this really need a tool tip?");
	add_numbut(but++, NUM|FLO, "Slider Min:", 
			   -10000, kb->slidermax, &kb->slidermin, 0);
	add_numbut(but++, NUM|FLO, "Slider Max:", 
			   kb->slidermin, 10000, &kb->slidermax, 0);

    if (do_clever_numbuts(str, but, REDRAW)) {
		strcpy(kb->name, str);
        allqueue (REDRAWACTION, 0);
		allspace(REMAKEIPO, 0);
        allqueue (REDRAWIPO, 0);
	}
}

static void clever_achannel_names (short *mval)
{
	void *act_channel;
	bActionChannel *achan= NULL;
	bConstraintChannel *conchan= NULL;
	IpoCurve *icu= NULL;
	
	int but=0;
    char str[64];
	short chantype;
	short expand, protect, mute;
	float slidermin, slidermax;
	
	/* figure out what is under cursor */
	act_channel= get_nearest_act_channel(mval, &chantype);
	
	/* create items for clever-numbut */
	if (chantype == ACTTYPE_ACHAN) {
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
		conchan= (bConstraintChannel *)act_channel;
		
		strcpy(str, conchan->name);
		protect= (conchan->flag & CONSTRAINT_CHANNEL_PROTECTED);
		mute = (conchan->ipo)? (conchan->ipo->muteipo): 0;
		
		add_numbut(but++, TEX, "ConChan: ", 0, 29, str, "Name of Constraint Channel");
		add_numbut(but++, TOG|SHO, "Muted", 0, 24, &mute, "Channel is Muted");
		add_numbut(but++, TOG|SHO, "Protected", 0, 24, &protect, "Channel is Protected");
	}
	else if (chantype == ACTTYPE_ICU) {
		icu= (IpoCurve *)act_channel;
		
		strcpy(str, getname_ipocurve(icu));
		
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
		
        allqueue (REDRAWACTION, 0);
		allqueue (REDRAWVIEW3D, 0);
	}
}

/* this gets called when nkey is pressed (no Transform Properties panel yet) */
static void numbuts_action (void)
{
	/* now called from action window event loop, plus reacts on mouseclick */
	/* removed Hos grunts for that reason! :) (ton) */
	void *data;
	short datatype;
    short mval[2];
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	getmouseco_areawin(mval);
	
	if (mval[0] < NAMEWIDTH) {
		switch (datatype) {
			case ACTCONT_ACTION:
				clever_achannel_names(mval);
				break;
			case ACTCONT_SHAPEKEY:
				clever_keyblock_names(data, mval);
				break;
		}
	}
}


/* **************************************************** */
/* CHANNEL SELECTION */

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

/* messy call... */
static void select_poseelement_by_name(char *name, int select)
{
	/* Syncs selection of channels with selection of object elements in posemode */
	Object *ob= OBACT;
	bPoseChannel *pchan;
	
	if (!ob || ob->type!=OB_ARMATURE)
		return;
	
	if(select==2) {
		for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next)
			pchan->bone->flag &= ~(BONE_ACTIVE);
	}
	
	pchan= get_pose_channel(ob->pose, name);
	if(pchan) {
		if(select)
			pchan->bone->flag |= (BONE_SELECTED);
		else 
			pchan->bone->flag &= ~(BONE_SELECTED);
		if(select==2)
			pchan->bone->flag |= (BONE_ACTIVE);
	}
}

/* apparently within active object context */
/* called extern, like on bone selection */
void select_actionchannel_by_name (bAction *act, char *name, int select)
{
	bActionChannel *achan;

	if (!act)
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

/* deselects action channels in given action */
void deselect_actionchannels (bAction *act, short test)
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	int filter, sel=1;
	
	/* filter data */
	filter= ACTFILTER_VISIBLE;
	actdata_filter(&act_data, filter, act, ACTCONT_ACTION);
	
	/* See if we should be selecting or deselecting */
	if (test) {
		for (ale= act_data.first; ale; ale= ale->next) {
			if (sel == 0) 
				break;
			
			switch (ale->type) {
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
			case ACTTYPE_ACHAN:
			{
				bActionChannel *achan= (bActionChannel *)ale->data;
				
				if (sel)
					achan->flag |= ACHAN_SELECTED;
				else
					achan->flag &= ~ACHAN_SELECTED;
				select_poseelement_by_name(achan->name, sel);
			}
				break;
			case ACTTYPE_CONCHAN:
			{
				bConstraintChannel *conchan= (bConstraintChannel *)ale->data;
				
				if (sel)
					conchan->flag |= CONSTRAINT_CHANNEL_SELECT;
				else
					conchan->flag &= ~CONSTRAINT_CHANNEL_SELECT;
			}
				break;
			case ACTTYPE_ICU:
			{
				IpoCurve *icu= (IpoCurve *)ale->data;
				
				if (sel)
					icu->flag |= IPO_SELECT;
				else
					icu->flag &= ~IPO_SELECT;
			}
				break;
		}
	}
	
	/* Cleanup */
	BLI_freelistN(&act_data);
}

/* deselects channels in the action editor */
void deselect_action_channels (short test)
{
	void *data;
	short datatype;
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	if (data == NULL) return;
	
	/* based on type */
	if (datatype == ACTCONT_ACTION)
		deselect_actionchannels(data, test);
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

/* ----------------------------------------- */

/* This function makes a list of the selected keyframes
 * in the ipo curves it has been passed
 */
static void make_sel_cfra_list(Ipo *ipo, ListBase *elems)
{
	IpoCurve *icu;
	
	if (ipo == NULL) return;
	
	for(icu= ipo->curve.first; icu; icu= icu->next) {
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
void column_select_action_keys(int mode)
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
	}
	
	/* loop through all of the keys and select additional keyframes
	 * based on the keys found to be selected above
	 */
	filter= (ACTFILTER_VISIBLE | ACTFILTER_ONLYICU);
	actdata_filter(&act_data, filter, data, datatype);
	
	for (ale= act_data.first; ale; ale= ale->next) {
		for(ce= elems.first; ce; ce= ce->next) {
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
	int val, selectmode;
	int (*select_function)(BezTriple *);
	short	mval[2];
	float	ymin, ymax;
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	if (data == NULL) return;
	
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
			if (!((ymax < rectf.ymin) || (ymin > rectf.ymax))) {
				if (ale->key_data) {
					if (ale->datatype == ALE_IPO)
						borderselect_ipo_key(ale->key_data, rectf.xmin, rectf.xmax, selectmode);
					else if (ale->datatype == ALE_ICU)
						borderselect_icu_key(ale->key_data, rectf.xmin, rectf.xmax, select_function);
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
	bActionChannel *achan= NULL;
	bConstraintChannel *conchan= NULL;
	IpoCurve *icu= NULL;
	TimeMarker *marker;
	void *act_channel;
	short sel, act_type;
	float selx;
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	if (data == NULL) return;
	if (datatype == ACTCONT_ACTION) act= (bAction *)data;

	act_channel= get_nearest_action_key(&selx, &sel, &act_type, &achan);
	marker=find_nearest_marker(1);
		
	if (act_channel) {
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
			default:
				return;
		}
		
		if (selectmode == SELECT_REPLACE) {
			selectmode = SELECT_ADD;
			
			deselect_action_keys(0, 0);
			
			if (datatype == ACTCONT_ACTION) {
				deselect_action_channels(0);
				
				achan->flag |= ACHAN_SELECTED;
				hilight_channel(act, achan, 1);
				select_poseelement_by_name(achan->name, 2);	/* 2 is activate */
			}
		}
		
		if (icu)
			select_icu_key(icu, selx, selectmode);
		else if (conchan)
			select_ipo_key(conchan->ipo, selx, selectmode);
		else
			select_ipo_key(achan->ipo, selx, selectmode);
		
		std_rmouse_transform(transform_action_keys);
		
		allqueue(REDRAWIPO, 0);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWNLA, 0);
		allqueue(REDRAWOOPS, 0);
		allqueue(REDRAWBUTSALL, 0);
	}
	else if (marker) {
		/* not channel, so maybe marker */		
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
		
		allqueue(REDRAWTIME, 0);
		allqueue(REDRAWIPO, 0);
		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWNLA, 0);
		allqueue(REDRAWSOUND, 0);
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
	
	allqueue (REDRAWIPO, 0);
	allqueue (REDRAWVIEW3D, 0);
	allqueue (REDRAWACTION, 0);
	allqueue (REDRAWNLA, 0);
	allqueue (REDRAWOOPS, 0);
	allqueue (REDRAWBUTSALL, 0);
}

/* **************************************************** */
/* ACTION CHANNEL RE-ORDERING */

void top_sel_action ()
{
	bAction *act;
	bActionChannel *achan;
	
	/* Get the selected action, exit if none are selected */
	act = G.saction->action;
	if (!act) return;
	
	for (achan= act->chanbase.first; achan; achan= achan->next){
		if (VISIBLE_ACHAN(achan)) {
			if (SEL_ACHAN(achan) && !(achan->flag & ACHAN_MOVED)){
				/* take it out off the chain keep data */
				BLI_remlink (&act->chanbase, achan);
				/* make it first element */
				BLI_insertlinkbefore(&act->chanbase, act->chanbase.first, achan);
				achan->flag |= ACHAN_MOVED;
				/* restart with rest of list */
				achan= achan->next;
			}
		}
	}
    /* clear temp flags */
	for (achan= act->chanbase.first; achan; achan= achan->next){
		achan->flag = achan->flag & ~ACHAN_MOVED;
	}
	
	/* Clean up and redraw stuff */
	remake_action_ipos (act);
	BIF_undo_push("Top Action channel");
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWNLA, 0);
}

void up_sel_action ()
{
	bAction *act;
	bActionChannel *achan, *prev;
	
	/* Get the selected action, exit if none are selected */
	act = G.saction->action;
	if (!act) return;
	
	for (achan=act->chanbase.first; achan; achan= achan->next) {
		if (VISIBLE_ACHAN(achan)) {
			if (SEL_ACHAN(achan) && !(achan->flag & ACHAN_MOVED)){
				prev = achan->prev;
				if (prev) {
					/* take it out off the chain keep data */
					BLI_remlink (&act->chanbase, achan);
					/* push it up */
					BLI_insertlinkbefore(&act->chanbase, prev, achan);
					achan->flag |= ACHAN_MOVED;
					/* restart with rest of list */
					achan= achan->next;
				}
			}
		}
	}
	/* clear temp flags */
	for (achan=act->chanbase.first; achan; achan= achan->next){
		achan->flag = achan->flag & ~ACHAN_MOVED;
	}
	
	/* Clean up and redraw stuff */
	remake_action_ipos (act);
	BIF_undo_push("Up Action channel");
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWNLA, 0);
}

void down_sel_action ()
{
	bAction *act;
	bActionChannel *achan, *next;
	
	/* Get the selected action, exit if none are selected */
	act = G.saction->action;
	if (!act) return;
	
	for (achan= act->chanbase.last; achan; achan= achan->prev) {
		if (VISIBLE_ACHAN(achan)) {
			if (SEL_ACHAN(achan) && !(achan->flag & ACHAN_MOVED)){
				next = achan->next;
				if (next) next = next->next;
				if (next) {
					/* take it out off the chain keep data */
					BLI_remlink (&act->chanbase, achan);
					/* move it down */
					BLI_insertlinkbefore(&act->chanbase, next, achan);
					achan->flag |= ACHAN_MOVED;
				}
				else {
					/* take it out off the chain keep data */
					BLI_remlink (&act->chanbase, achan);
					/* add at end */
					BLI_addtail(&act->chanbase, achan);
					achan->flag |= ACHAN_MOVED;
				}
			}
		}
	}
	/* clear temp flags */
	for (achan= act->chanbase.first; achan; achan= achan->next){
		achan->flag = achan->flag & ~ACHAN_MOVED;
	}
	
	/* Clean up and redraw stuff */
	remake_action_ipos (act);
	BIF_undo_push("Down Action channel");
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWNLA, 0);
}

void bottom_sel_action ()
{
	bAction *act;
	bActionChannel *achan;
	
	/* Get the selected action, exit if none are selected */
	act = G.saction->action;
	if (!act) return;
	
	for (achan=act->chanbase.last; achan; achan= achan->prev) {
		if (VISIBLE_ACHAN(achan)) {
			if (SEL_ACHAN(achan) && !(achan->flag & ACHAN_MOVED)) {
				/* take it out off the chain keep data */
				BLI_remlink (&act->chanbase, achan);
				/* add at end */
				BLI_addtail(&act->chanbase, achan);
				achan->flag |= ACHAN_MOVED;
			}
		}		
	}
	/* clear temp flags */
	for (achan=act->chanbase.first; achan; achan= achan->next) {
		achan->flag = achan->flag & ~ACHAN_MOVED;
	}
	
	/* Clean up and redraw stuff */
	remake_action_ipos (act);
	BIF_undo_push("Bottom Action channel");
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWNLA, 0);
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

	if(curarea->win==0) return;

	saction= curarea->spacedata.first;
	if (!saction)
		return;

	data= get_action_context(&datatype);
	
	if (val) {
		if ( uiDoBlocks(&curarea->uiblocks, event)!=UI_NOTHING ) event= 0;
		
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
			do_actionbuts(val); 	// window itself
			break;
		
		case HOMEKEY:
			do_action_buttons(B_ACTHOME);	// header
			break;

		case AKEY:
			if (mval[0]<NAMEWIDTH) {
				deselect_action_channels (1);
				allqueue (REDRAWVIEW3D, 0);
				allqueue (REDRAWACTION, 0);
				allqueue(REDRAWNLA, 0);
				allqueue (REDRAWIPO, 0);
			}
			else if (mval[0]>ACTWIDTH) {
				if (G.qual == LR_CTRLKEY) {
					deselect_markers (1, 0);
					allqueue(REDRAWTIME, 0);
					allqueue(REDRAWIPO, 0);
					allqueue(REDRAWACTION, 0);
					allqueue(REDRAWNLA, 0);
					allqueue(REDRAWSOUND, 0);
				}
				else {
					deselect_action_keys (1, 1);
					allqueue (REDRAWACTION, 0);
					allqueue(REDRAWNLA, 0);
					allqueue (REDRAWIPO, 0);
				}
			}
			break;

		case BKEY:
			if (G.qual & LR_CTRLKEY) {
				borderselect_markers();
			}
			else {
				if (mval[0]>ACTWIDTH)
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
			if (mval[0]>ACTWIDTH) {
				if (G.qual == (LR_CTRLKEY|LR_SHIFTKEY))
					duplicate_marker();
				else if (G.qual == LR_SHIFTKEY)
					duplicate_action_keys();
			}
			break;

		case GKEY:
			if (G.qual & LR_CTRLKEY) {
				transform_markers('g', 0);
			}
			else {
				if (mval[0]>=ACTWIDTH)
					transform_action_keys('g', 0);
			}
			break;
		
		case HKEY:
			if(G.qual & LR_SHIFTKEY) {
				if(okee("Set Keys to Auto Handle"))
					sethandles_action_keys(HD_AUTO);
			}
			else {
				if(okee("Toggle Keys Aligned Handle"))
					sethandles_action_keys(HD_ALIGN);
			}
			break;
		
		case KKEY:
			if (G.qual & LR_CTRLKEY) {
				markers_selectkeys_between();
			}
			else {
				val= (G.qual & LR_SHIFTKEY) ? 2 : 1;
				column_select_action_keys(val);
			}
			
			allqueue(REDRAWTIME, 0);
			allqueue(REDRAWIPO, 0);
			allqueue(REDRAWACTION, 0);
			allqueue(REDRAWNLA, 0);
			break;
			
		case MKEY:
			if (G.qual & LR_SHIFTKEY) {
				/* mirror keyframes */
				if (data) {
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
				allqueue(REDRAWTIME, 0);
				allqueue(REDRAWIPO, 0);
				allqueue(REDRAWACTION, 0);
				allqueue(REDRAWNLA, 0);
				allqueue(REDRAWSOUND, 0);
			}
			break;
			
		case NKEY:
			if(G.qual==0) {
				numbuts_action();
				
				/* no panel (yet). current numbuts are not easy to put in panel... */
				//add_blockhandler(curarea, ACTION_HANDLER_PROPERTIES, UI_PNL_TO_MOUSE);
				//scrarea_queue_winredraw(curarea);
			}
			break;
			
		case OKEY:
			clean_action();
			break;
			
		case PKEY:
			if (G.qual & LR_CTRLKEY) /* set preview range */
				anim_previewrange_set();
			else if (G.qual & LR_ALTKEY) /* clear preview range */
				anim_previewrange_clear();
			allqueue(REDRAWTIME, 0);
			allqueue(REDRAWBUTSALL, 0);
			allqueue(REDRAWACTION, 0);
			allqueue(REDRAWNLA, 0);
			allqueue(REDRAWIPO, 0);
			break;
			
		case SKEY: 
			if (mval[0]>=ACTWIDTH) {
				if(G.qual & LR_SHIFTKEY) {
					if (data) {
						val = pupmenu("Snap Keys To%t|Nearest Frame%x1|Current Frame%x2|Nearest Marker %x3");
						snap_action_keys(val);
					}
				}
				else
					transform_action_keys ('s', 0);	
			}
			break;
		
		case TKEY:
			if(G.qual & LR_SHIFTKEY)
				action_set_ipo_flags(SET_IPO_POPUP);
			else
				transform_action_keys ('t', 0);
			break;

		case VKEY:
			if(okee("Set Keys to Vector Handle"))
				sethandles_action_keys(HD_VECT);
			break;

		case PAGEUPKEY:
			if (datatype == ACTCONT_ACTION) {
				if(G.qual & LR_SHIFTKEY)
					top_sel_action();
				else if (G.qual & LR_CTRLKEY)
					up_sel_action();
				else
					nextprev_marker(1);
			}
			else if (datatype == ACTCONT_SHAPEKEY) {
				/* only jump to markers possible (key channels can't be moved yet) */
				nextprev_marker(1);
			}
			break;
		case PAGEDOWNKEY:
			if (datatype == ACTCONT_ACTION) {
				if(G.qual & LR_SHIFTKEY)
					bottom_sel_action();
				else if (G.qual & LR_CTRLKEY) 
					down_sel_action();
				else
					nextprev_marker(-1);
			}
			else if (datatype == ACTCONT_SHAPEKEY) {
				/* only jump to markers possible (key channels can't be moved yet) */
				nextprev_marker(-1);
			}
			break;
			
		case DELKEY:
		case XKEY:
			if (okee("Erase selected")) {
				if (mval[0]<NAMEWIDTH)
					delete_action_channels();
				else
					delete_action_keys();
				
				if (mval[0] >= NAMEWIDTH)
					remove_marker();
				
				allqueue(REDRAWTIME, 0);
				allqueue(REDRAWIPO, 0);
				allqueue(REDRAWACTION, 0);
				allqueue(REDRAWNLA, 0);
				allqueue(REDRAWSOUND, 0);
			}
			break;
		
		/* LEFTMOUSE and RIGHTMOUSE event codes can be swapped above,
		 * based on user preference USER_LMOUSESELECT
		 */
		case LEFTMOUSE:
			if(view2dmove(LEFTMOUSE)) // only checks for sliders
				break;
			else if (mval[0]>ACTWIDTH) {
				do {
					getmouseco_areawin(mval);
					
					areamouseco_to_ipoco(G.v2d, mval, &dx, &dy);
					
					cfra= (int)dx;
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
			/* passed on as selection */
		case RIGHTMOUSE:
			/* Clicking in the channel area */
			if (mval[0]<NAMEWIDTH) {
				if (datatype == ACTCONT_ACTION) {
					/* mouse is over action channels */
					if (G.qual & LR_CTRLKEY)
						clever_achannel_names(mval);
					else 
						mouse_actionchannels(mval);
				}
				else numbuts_action();
			}
			else if (mval[0]>ACTWIDTH) {
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
				else
					mouse_action(select_mode);
			}
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

	if(doredraw) addqueue(curarea->win, REDRAW, 1);
	
}

/* **************************************************** */
