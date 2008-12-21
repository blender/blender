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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* This file contains a system used to provide a layer of abstraction between sources
 * of animation data and tools in Animation Editors. The method used here involves 
 * generating a list of edit structures which enable tools to naively perform the actions 
 * they require without all the boiler-plate associated with loops within loops and checking 
 * for cases to ignore. 
 *
 * While this is primarily used for the Action/Dopesheet Editor (and its accessory modes),
 * the IPO Editor also uses this for it's channel list and for determining which curves
 * are being edited.
 *
 * Note: much of the original system this was based on was built before the creation of the RNA
 * system. In future, it would be interesting to replace some parts of this code with RNA queries,
 * however, RNA does not eliminate some of the boiler-plate reduction benefits presented by this 
 * system, so if any such work does occur, it should only be used for the internals used here...
 *
 * -- Joshua Leung, Dec 2008
 */

#include <string.h>
#include <stdio.h>

#include "DNA_listBase.h"
#include "DNA_ID.h"
#include "DNA_action_types.h"
#include "DNA_constraint_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_ipo_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_object.h"
#include "BKE_material.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "ED_anim_api.h"
#include "ED_types.h"
#include "ED_util.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

/* ************************************************************ */
/* Blender Context <-> Animation Context mapping */

/* ----------- Private Stuff - Action Editor ------------- */

/* Get shapekey data being edited (for Action Editor -> ShapeKey mode) */
/* Note: there's a similar function in key.c (ob_get_key) */
Key *actedit_get_shapekeys (const bContext *C, SpaceAction *saction) 
{
    Scene *scene= CTX_data_scene(C);
    Object *ob;
    Key *key;
	
    ob = OBACT;  // XXX er...
    if (ob == NULL) 
		return NULL;
	
	/* XXX pinning is not available in 'ShapeKey' mode... */
	//if (saction->pin) return NULL;
	
	/* shapekey data is stored with geometry data */
	switch (ob->type) {
		case OB_MESH:
			key= ((Mesh *)ob->data)->key;
			break;
			
		case OB_LATTICE:
			key= ((Lattice *)ob->data)->key;
			break;
			
		case OB_CURVE:
		case OB_SURF:
			key= ((Curve *)ob->data)->key;
			break;
			
		default:
			return NULL;
	}
	
	if (key) {
		if (key->type == KEY_RELATIVE)
			return key;
	}
	
    return NULL;
}

/* Get data being edited in Action Editor (depending on current 'mode') */
static void *actedit_get_context (const bContext *C, SpaceAction *saction, short *datatype)
{
	Scene *scene= CTX_data_scene(C);
	
	/* sync settings with current view status, then return appropriate data */
	switch (saction->mode) {
		case SACTCONT_ACTION: /* 'Action Editor' */
			/* if not pinned, sync with active object */
			if (saction->pin == 0) {
				if (OBACT)
					saction->action = OBACT->action;
				else
					saction->action= NULL;
			}
				
			*datatype= ANIMCONT_ACTION;
			return saction->action;
			
		case SACTCONT_SHAPEKEY: /* 'ShapeKey Editor' */
			*datatype= ANIMCONT_SHAPEKEY;
			return actedit_get_shapekeys(C, saction);
			
		case SACTCONT_GPENCIL: /* Grease Pencil */ // XXX review how this mode is handled...
			*datatype=ANIMCONT_GPENCIL;
			return CTX_wm_screen(C); // FIXME: add that dopesheet type thing here!
			break;
			
		case SACTCONT_DOPESHEET: /* DopeSheet */
			/* update scene-pointer (no need to check for pinning yet, as not implemented) */
			saction->ads.source= (ID *)scene;
			
			*datatype= ANIMCONT_DOPESHEET;
			return &saction->ads;
		
		default: /* unhandled yet */
			*datatype= ANIMCONT_NONE;
			return NULL;
	}
}

/* ----------- Private Stuff - IPO Editor ------------- */

/* Get data being edited in IPO Editor (depending on current 'mode') */
static void *ipoedit_get_context (const bContext *C, SpaceIpo *sipo, short *datatype)
{
	// XXX FIXME...
	return NULL;
}

/* ----------- Public API --------------- */

/* Obtain current anim-data context from Blender Context info */
void *ANIM_animdata_get_context (const bContext *C, short *datatype)
{
	ScrArea *sa= CTX_wm_area(C);
	
	/* set datatype to 'None' for convenience */
	if (datatype == NULL) return NULL;
	*datatype= ANIMCONT_NONE;
	if (sa == NULL) return NULL; /* highly unlikely to happen, but still! */
	
	/* context depends on editor we are currently in */
	switch (sa->spacetype) {
		case SPACE_ACTION:
		{
			SpaceAction *saction= (SpaceAction *)CTX_wm_space_data(C);
			return actedit_get_context(C, saction, datatype);
		}
			break;
			
		case SPACE_IPO:
		{
			SpaceIpo *sipo= (SpaceIpo *)CTX_wm_space_data(C);
			return ipoedit_get_context(C, sipo, datatype);
		}
			break;
	}
	
	/* nothing appropriate */
	return NULL;
}

/* ************************************************************ */
/* Blender Data <-- Filter --> Channels to be operated on */

/* ----------- 'Private' Stuff --------------- */

/* this function allocates memory for a new bAnimListElem struct for the 
 * provided animation channel-data. 
 */
bAnimListElem *make_new_animlistelem (void *data, short datatype, void *owner, short ownertype)
{
	bAnimListElem *ale= NULL;
	
	/* only allocate memory if there is data to convert */
	if (data) {
		/* allocate and set generic data */
		ale= MEM_callocN(sizeof(bAnimListElem), "bAnimListElem");
		
		ale->data= data;
		ale->type= datatype;
		ale->owner= owner;
		ale->ownertype= ownertype;
		
		if ((owner) && (ownertype == ANIMTYPE_ACHAN)) {
			bActionChannel *ochan= (bActionChannel *)owner;
			ale->grp= ochan->grp;
		}
		else 
			ale->grp= NULL;
		
		/* do specifics */
		switch (datatype) {
			case ANIMTYPE_OBJECT:
			{
				Base *base= (Base *)data;
				Object *ob= base->object;
				
				ale->flag= ob->flag;
				
				ale->key_data= ob;
				ale->datatype= ALE_OB;
			}
				break;
			case ANIMTYPE_FILLACTD:
			{
				bAction *act= (bAction *)data;
				
				ale->flag= act->flag;
				
				ale->key_data= act;
				ale->datatype= ALE_ACT;
			}
				break;
			case ANIMTYPE_FILLIPOD:
			{
				Object *ob= (Object *)data;
				
				ale->flag= FILTER_IPO_OBJC(ob);
				
				ale->key_data= ob->ipo;
				ale->datatype= ALE_IPO;
			}
				break;
			case ANIMTYPE_FILLCOND:
			{
				Object *ob= (Object *)data;
				
				ale->flag= FILTER_CON_OBJC(ob);
				
				ale->key_data= NULL;
				ale->datatype= ALE_NONE;
			}
				break;
			case ANIMTYPE_FILLMATD:
			{
				Object *ob= (Object *)data;
				
				ale->flag= FILTER_MAT_OBJC(ob);
				
				ale->key_data= NULL;
				ale->datatype= ALE_NONE;
			}
				break;
			
			case ANIMTYPE_DSMAT:
			{
				Material *ma= (Material *)data;
				
				ale->flag= FILTER_MAT_OBJD(ma);
				
				ale->key_data= ma->ipo;
				ale->datatype= ALE_IPO;
			}
				break;
			case ANIMTYPE_DSLAM:
			{
				Lamp *la= (Lamp *)data;
				
				ale->flag= FILTER_LAM_OBJD(la);
				
				ale->key_data= la->ipo;
				ale->datatype= ALE_IPO;
			}
				break;
			case ANIMTYPE_DSCAM:
			{
				Camera *ca= (Camera *)data;
				
				ale->flag= FILTER_CAM_OBJD(ca);
				
				ale->key_data= ca->ipo;
				ale->datatype= ALE_IPO;
			}
				break;
			case ANIMTYPE_DSCUR:
			{
				Curve *cu= (Curve *)data;
				
				ale->flag= FILTER_CUR_OBJD(cu);
				
				ale->key_data= cu->ipo;
				ale->datatype= ALE_IPO;
			}
				break;
			case ANIMTYPE_DSSKEY:
			{
				Key *key= (Key *)data;
				
				ale->flag= FILTER_SKE_OBJD(key);
				
				ale->key_data= key->ipo;
				ale->datatype= ALE_IPO;
			}
				break;
				
			case ANIMTYPE_GROUP:
			{
				bActionGroup *agrp= (bActionGroup *)data;
				
				ale->flag= agrp->flag;
				
				ale->key_data= NULL;
				ale->datatype= ALE_GROUP;
			}
				break;
			case ANIMTYPE_ACHAN:
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
			case ANIMTYPE_CONCHAN:
			case ANIMTYPE_CONCHAN2:
			{
				bConstraintChannel *conchan= (bConstraintChannel *)data;
				
				ale->flag= conchan->flag;
				
				if (datatype == ANIMTYPE_CONCHAN2) {
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
					if ((conchan->ipo) && (conchan->ipo->curve.first)) {
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
			case ANIMTYPE_ICU:
			{
				IpoCurve *icu= (IpoCurve *)data;
				
				ale->flag= icu->flag;
				ale->key_data= icu;
				ale->datatype= ALE_ICU;
			}
				break;
			case ANIMTYPE_FILLIPO:
			case ANIMTYPE_FILLCON:
			{
				bActionChannel *achan= (bActionChannel *)data;
				
				if (datatype == ANIMTYPE_FILLIPO)
					ale->flag= FILTER_IPO_ACHAN(achan);
				else
					ale->flag= FILTER_CON_ACHAN(achan);
					
				ale->key_data= NULL;
				ale->datatype= ALE_NONE;
			}
				break;
			case ANIMTYPE_IPO:
			{
				ale->flag= 0;
				ale->key_data= data;
				ale->datatype= ALE_IPO;
			}
				break;
			
			case ANIMTYPE_GPLAYER:
			{
				bGPDlayer *gpl= (bGPDlayer *)data;
				
				ale->flag= gpl->flag;
				
				ale->key_data= NULL;
				ale->datatype= ALE_GPFRAME;
			}
				break;
		}
	}
	
	/* return created datatype */
	return ale;
}
 
/* ----------------------------------------- */

static void animdata_filter_animionchannel (ListBase *anim_data, bActionChannel *achan, int filter_mode, void *owner, short ownertype)
{
	bAnimListElem *ale = NULL;
	bConstraintChannel *conchan;
	IpoCurve *icu;
	short owned= (owner && ownertype)? 1 : 0;
	
	/* only work with this channel and its subchannels if it is visible */
	if (!(filter_mode & ANIMFILTER_VISIBLE) || VISIBLE_ACHAN(achan)) {
		/* only work with this channel and its subchannels if it is editable */
		if (!(filter_mode & ANIMFILTER_FOREDIT) || EDITABLE_ACHAN(achan)) {
			/* check if this achan should only be included if it is selected */
			if (!(filter_mode & ANIMFILTER_SEL) || SEL_ACHAN(achan)) {
				/* are we only interested in the ipo-curves? */
				if ((filter_mode & ANIMFILTER_ONLYICU)==0) {
					ale= make_new_animlistelem(achan, ANIMTYPE_ACHAN, achan, ANIMTYPE_ACHAN);
					
					if (ale) {
						if (owned) ale->id= owner;
						BLI_addtail(anim_data, ale);
					}
				}
			}
			else {
				/* for insert key... this check could be improved */
				return;
			}
			
			/* check if expanded - if not, continue on to next animion channel */
			if (EXPANDED_ACHAN(achan) == 0 && (filter_mode & ANIMFILTER_ONLYICU)==0) {
				/* only exit if we don't need to include constraint channels for group-channel keyframes */
				if ( !(filter_mode & ANIMFILTER_IPOKEYS) || (achan->grp == NULL) || (EXPANDED_AGRP(achan->grp)==0) )
					return;
			}
				
			/* ipo channels */
			if ((achan->ipo) && (filter_mode & ANIMFILTER_IPOKEYS)==0) {
				/* include ipo-expand widget? */
				if ((filter_mode & ANIMFILTER_CHANNELS) && (filter_mode & ANIMFILTER_ONLYICU)==0) {
					ale= make_new_animlistelem(achan, ANIMTYPE_FILLIPO, achan, ANIMTYPE_ACHAN);
					
					if (ale) {
						if (owned) ale->id= owner;
						BLI_addtail(anim_data, ale);
					}
				}
				
				/* add ipo-curve channels? */
				if (FILTER_IPO_ACHAN(achan) || (filter_mode & ANIMFILTER_ONLYICU)) {
					/* loop through ipo-curve channels, adding them */
					for (icu= achan->ipo->curve.first; icu; icu=icu->next) {
						ale= make_new_animlistelem(icu, ANIMTYPE_ICU, achan, ANIMTYPE_ACHAN);
						
						if (ale) {
							if (owned) ale->id= owner;
							BLI_addtail(anim_data, ale); 
						}
					}
				}
			}
			
			/* constraint channels */
			if (achan->constraintChannels.first) {
				/* include constraint-expand widget? */
				if ( (filter_mode & ANIMFILTER_CHANNELS) && !(filter_mode & ANIMFILTER_ONLYICU)
					 && !(filter_mode & ANIMFILTER_IPOKEYS) ) 
				{
					ale= make_new_animlistelem(achan, ANIMTYPE_FILLCON, achan, ANIMTYPE_ACHAN);
					
					if (ale) {
						if (owned) ale->id= owner;
						BLI_addtail(anim_data, ale);
					}
				}
				
				/* add constraint channels? */
				if (FILTER_CON_ACHAN(achan) || (filter_mode & ANIMFILTER_IPOKEYS) || (filter_mode & ANIMFILTER_ONLYICU)) {
					/* loop through constraint channels, checking and adding them */
					for (conchan=achan->constraintChannels.first; conchan; conchan=conchan->next) {
						/* only work with this channel and its subchannels if it is editable */
						if (!(filter_mode & ANIMFILTER_FOREDIT) || EDITABLE_CONCHAN(conchan)) {
							/* check if this conchan should only be included if it is selected */
							if (!(filter_mode & ANIMFILTER_SEL) || SEL_CONCHAN(conchan)) {
								if (filter_mode & ANIMFILTER_IPOKEYS) {
									if (ale) BLI_addtail(anim_data, ale);
									ale= make_new_animlistelem(conchan, ANIMTYPE_CONCHAN2, achan, ANIMTYPE_ACHAN);
									
									if (ale) {
										if (owned) ale->id= owner;
										BLI_addtail(anim_data, ale);
									}
								}
								else {
									ale= make_new_animlistelem(conchan, ANIMTYPE_CONCHAN, achan, ANIMTYPE_ACHAN);
									
									if (ale) {
										if (owned) ale->id= owner;
										BLI_addtail(anim_data, ale);
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

static void animdata_filter_action (ListBase *anim_data, bAction *act, int filter_mode, void *owner, short ownertype)
{
	bAnimListElem *ale=NULL;
	bActionGroup *agrp;
	bActionChannel *achan, *lastchan=NULL;
	short owned= (owner && ownertype) ? 1 : 0;
	
	/* loop over groups */
	for (agrp= act->groups.first; agrp; agrp= agrp->next) {
		/* add this group as a channel first */
		if (!(filter_mode & ANIMFILTER_ONLYICU) && !(filter_mode & ANIMFILTER_IPOKEYS)) {
			/* check if filtering by selection */
			if ( !(filter_mode & ANIMFILTER_SEL) || SEL_AGRP(agrp) ) {
				ale= make_new_animlistelem(agrp, ANIMTYPE_GROUP, NULL, ANIMTYPE_NONE);
				if (ale) {
					if (owned) ale->id= owner;
					BLI_addtail(anim_data, ale);
				}
			}
		}
		
		/* store reference to last channel of group */
		if (agrp->channels.last) 
			lastchan= agrp->channels.last;
		
		
		/* there are some situations, where only the channels of the animive group should get considered */
		if (!(filter_mode & ANIMFILTER_ACTGROUPED) || (agrp->flag & AGRP_ACTIVE)) {
			/* filters here are a bit convoulted...
			 *	- groups show a "summary" of keyframes beside their name which must accessable for tools which handle keyframes
			 *	- groups can be collapsed (and those tools which are only interested in channels rely on knowing that group is closed)
			 *
			 * cases when we should include animion-channels and so-forth inside group:
			 *	- we don't care about visibility
			 *	- group is expanded
			 *	- we're interested in keyframes, but not if they appear in selected channels
			 */
			if ( (!(filter_mode & ANIMFILTER_VISIBLE) || EXPANDED_AGRP(agrp)) || 
				 ( ((filter_mode & ANIMFILTER_IPOKEYS) || (filter_mode & ANIMFILTER_ONLYICU)) && 
				   (!(filter_mode & ANIMFILTER_SEL) || (SEL_AGRP(agrp))) ) ) 
			{
				if (!(filter_mode & ANIMFILTER_FOREDIT) || EDITABLE_AGRP(agrp)) {					
					for (achan= agrp->channels.first; achan && achan->grp==agrp; achan= achan->next) {
						animdata_filter_animionchannel(anim_data, achan, filter_mode, owner, ownertype);
					}
					
					/* remove group from filtered list if last element is group 
					 * (i.e. only if group had channels, which were all hidden)
					 */
					// XXX this is really hacky... it should be fixed in a much more elegant way!
					if ( (ale) && (anim_data->last == ale) && 
						 (ale->data == agrp) && (agrp->channels.first) ) 
					{
						BLI_freelinkN(anim_data, ale);
					}
				}
			}
		}
	}
	
	/* loop over un-grouped animion channels (only if we're not only considering those channels in the animive group) */
	if (!(filter_mode & ANIMFILTER_ACTGROUPED))  {
		for (achan=(lastchan)?lastchan->next:act->chanbase.first; achan; achan=achan->next) {
			animdata_filter_animionchannel(anim_data, achan, filter_mode, owner, ownertype);
		}
	}
}

static void animdata_filter_shapekey (ListBase *anim_data, Key *key, int filter_mode, void *owner, short ownertype)
{
	bAnimListElem *ale;
	KeyBlock *kb;
	IpoCurve *icu;
	short owned= (owner && ownertype)? 1 : 0;
	int i;
	
	/* are we filtering for display or editing */
	if (filter_mode & ANIMFILTER_FORDRAWING) {
		/* for display - loop over shapekeys, adding ipo-curve references where needed */
		kb= key->block.first;
		
		/* loop through possible shapekeys, manually creating entries */
		for (i= 1; i < key->totkey; i++) {
			ale= MEM_callocN(sizeof(bAnimListElem), "bAnimListElem");
			kb = kb->next; /* do this even on the first try, as the first is 'Basis' (which doesn't get included) */
			
			ale->data= kb;
			ale->type= ANIMTYPE_SHAPEKEY; /* 'abused' usage of this type */
			ale->owner= key;
			ale->ownertype= ANIMTYPE_SHAPEKEY;
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
			
			if (owned) ale->id= owner;
			
			BLI_addtail(anim_data, ale);
		}
	}
	else {
		/* loop over ipo curves if present - for editing */
		if (key->ipo) {
			if (filter_mode & ANIMFILTER_IPOKEYS) {
				ale= make_new_animlistelem(key->ipo, ANIMTYPE_IPO, key, ANIMTYPE_SHAPEKEY);
				if (ale) {
					if (owned) ale->id= owner;
					BLI_addtail(anim_data, ale);
				}
			}
			else {
				for (icu= key->ipo->curve.first; icu; icu=icu->next) {
					ale= make_new_animlistelem(icu, ANIMTYPE_ICU, key, ANIMTYPE_SHAPEKEY);
					if (ale) {
						if (owned) ale->id= owner;
						BLI_addtail(anim_data, ale);
					}
				}
			}
		}
	}
}
 
#if 0
// FIXME: switch this to use the bDopeSheet...
static void animdata_filter_gpencil (ListBase *anim_data, bScreen *sc, int filter_mode)
{
	bAnimListElem *ale;
	ScrArea *sa, *curarea;
	bGPdata *gpd;
	bGPDlayer *gpl;
	
	/* check if filtering types are appropriate */
	if ( !(filter_mode & (ANIMFILTER_IPOKEYS|ANIMFILTER_ONLYICU|ANIMFILTER_ACTGROUPED)) ) 
	{
		/* special hack for fullscreen area (which must be this one then):
		 * 	- we use the curarea->full as screen to get spaces from, since the
		 * 	  old (pre-fullscreen) screen was stored there...
		 *	- this is needed as all data would otherwise disappear
		 */
		// XXX need to get new alternative for curarea
		if ((curarea->full) && (curarea->spacetype==SPACE_ACTION))
			sc= curarea->full;
		
		/* loop over spaces in current screen, finding gpd blocks (could be slow!) */
		for (sa= sc->areabase.first; sa; sa= sa->next) {
			/* try to get gp data */
			// XXX need to put back grease pencil api...
			gpd= gpencil_data_getactive(sa);
			if (gpd == NULL) continue;
			
			/* add gpd as channel too (if for drawing, and it has layers) */
			if ((filter_mode & ANIMFILTER_FORDRAWING) && (gpd->layers.first)) {
				/* add to list */
				ale= make_new_animlistelem(gpd, ANIMTYPE_GPDATABLOCK, sa, ANIMTYPE_SPECIALDATA);
				if (ale) BLI_addtail(anim_data, ale);
			}
			
			/* only add layers if they will be visible (if drawing channels) */
			if ( !(filter_mode & ANIMFILTER_VISIBLE) || (EXPANDED_GPD(gpd)) ) {
				/* loop over layers as the conditions are acceptable */
				for (gpl= gpd->layers.first; gpl; gpl= gpl->next) {
					/* only if selected */
					if (!(filter_mode & ANIMFILTER_SEL) || SEL_GPL(gpl)) {
						/* only if editable */
						if (!(filter_mode & ANIMFILTER_FOREDIT) || EDITABLE_GPL(gpl)) {
							/* add to list */
							ale= make_new_animlistelem(gpl, ANIMTYPE_GPLAYER, gpd, ANIMTYPE_GPDATABLOCK);
							if (ale) BLI_addtail(anim_data, ale);
						}
					}
				}
			}
		}
	}
}
#endif 

static void animdata_filter_dopesheet_mats (ListBase *anim_data, bDopeSheet *ads, Base *base, int filter_mode)
{
	bAnimListElem *ale=NULL;
	Object *ob= base->object;
	IpoCurve *icu;
	
	/* include materials-expand widget? */
	if ((filter_mode & ANIMFILTER_CHANNELS) && !(filter_mode & (ANIMFILTER_IPOKEYS|ANIMFILTER_ONLYICU))) {
		ale= make_new_animlistelem(ob, ANIMTYPE_FILLMATD, base, ANIMTYPE_OBJECT);
		if (ale) BLI_addtail(anim_data, ale);
	}
	
	/* add materials? */
	if (FILTER_MAT_OBJC(ob) || (filter_mode & ANIMFILTER_IPOKEYS) || (filter_mode & ANIMFILTER_ONLYICU)) {
		short a;
		
		/* for each material, either add channels separately, or as ipo-block */
		for (a=0; a<ob->totcol; a++) {
			Material *ma= give_current_material(ob, a);
			
			/* for now, if no material returned, skip (this shouldn't confuse the user I hope) */
			if (ELEM(NULL, ma, ma->ipo)) continue;
			
			/* include material-expand widget? */
			// hmm... do we need to store the index of this material in the array anywhere?
			if (filter_mode & (ANIMFILTER_CHANNELS|ANIMFILTER_IPOKEYS)) {
				ale= make_new_animlistelem(ma, ANIMTYPE_DSMAT, base, ANIMTYPE_OBJECT);
				if (ale) BLI_addtail(anim_data, ale);
			}
			
			/* add material's ipo-curve channels? */
			if ( (FILTER_MAT_OBJD(ma) || (filter_mode & ANIMFILTER_ONLYICU)) && 
				  !(filter_mode & ANIMFILTER_IPOKEYS) ) 
			{
				/* loop through ipo-curve channels, adding them */
				for (icu= ma->ipo->curve.first; icu; icu=icu->next) {
					/* only if selected (if checking for selection) */
					if ( !(filter_mode & ANIMFILTER_SEL) || (SEL_ICU(icu)) ) {
						ale= make_new_animlistelem(icu, ANIMTYPE_ICU, base, ANIMTYPE_OBJECT);
						if (ale) {
							/* make owner the material not object, so that indent is not just object level */
							ale->id= (ID *)ma;
							BLI_addtail(anim_data, ale);
						}
					}
				}
			}
		}
	}
}

static void animdata_filter_dopesheet_cam (ListBase *anim_data, bDopeSheet *ads, Base *base, int filter_mode)
{
	bAnimListElem *ale=NULL;
	Object *ob= base->object;
	Camera *ca= (Camera *)ob->data;
	IpoCurve *icu;
	
	/* include camera-expand widget? */
	if (filter_mode & (ANIMFILTER_CHANNELS|ANIMFILTER_IPOKEYS)) {
		ale= make_new_animlistelem(ca, ANIMTYPE_DSCAM, base, ANIMTYPE_OBJECT);
		if (ale) BLI_addtail(anim_data, ale);
	}
	
	/* add camera ipo-curve channels? */
	if ( (FILTER_CAM_OBJD(ca) || (filter_mode & ANIMFILTER_ONLYICU)) && 
		  !(filter_mode & ANIMFILTER_IPOKEYS) ) 
	{
		/* loop through ipo-curve channels, adding them */
		for (icu= ca->ipo->curve.first; icu; icu=icu->next) {
			/* only if selected (if checking for selection) */
			if ( !(filter_mode & ANIMFILTER_SEL) || (SEL_ICU(icu)) ) {
				ale= make_new_animlistelem(icu, ANIMTYPE_ICU, base, ANIMTYPE_OBJECT);
				if (ale) {
					/* make owner the material not object, so that indent is not just object level */
					ale->id= (ID *)ca;
					BLI_addtail(anim_data, ale); 
				}
			}
		}
	}
}

static void animdata_filter_dopesheet_lamp (ListBase *anim_data, bDopeSheet *ads, Base *base, int filter_mode)
{
	bAnimListElem *ale=NULL;
	Object *ob= base->object;
	Lamp *la= (Lamp *)ob->data;
	IpoCurve *icu;
	
	/* include lamp-expand widget? */
	if (filter_mode & (ANIMFILTER_CHANNELS|ANIMFILTER_IPOKEYS)) {
		ale= make_new_animlistelem(la, ANIMTYPE_DSLAM, base, ANIMTYPE_OBJECT);
		if (ale) BLI_addtail(anim_data, ale);
	}
	
	/* add lamp ipo-curve channels? */
	if ( (FILTER_LAM_OBJD(la) || (filter_mode & ANIMFILTER_ONLYICU)) && 
		  !(filter_mode & ANIMFILTER_IPOKEYS) ) 
	{
		/* loop through ipo-curve channels, adding them */
		for (icu= la->ipo->curve.first; icu; icu=icu->next) {
			/* only if selected (if checking for selection) */
			if ( !(filter_mode & ANIMFILTER_SEL) || (SEL_ICU(icu)) ) {
				ale= make_new_animlistelem(icu, ANIMTYPE_ICU, base, ANIMTYPE_OBJECT);
				if (ale) {
					/* make owner the material not object, so that indent is not just object level */
					ale->id= (ID *)la;
					BLI_addtail(anim_data, ale); 
				}
			}
		}
	}
}

static void animdata_filter_dopesheet_curve (ListBase *anim_data, bDopeSheet *ads, Base *base, int filter_mode)
{
	bAnimListElem *ale=NULL;
	Object *ob= base->object;
	Curve *cu= (Curve *)ob->data;
	IpoCurve *icu;
	
	/* include curve-expand widget? */
	if (filter_mode & (ANIMFILTER_CHANNELS|ANIMFILTER_IPOKEYS)) {
		ale= make_new_animlistelem(cu, ANIMTYPE_DSCUR, base, ANIMTYPE_OBJECT);
		if (ale) BLI_addtail(anim_data, ale);
	}
	
	/* add curve ipo-curve channels? */
	if ( (FILTER_CUR_OBJD(cu) || (filter_mode & ANIMFILTER_ONLYICU)) && 
		  !(filter_mode & ANIMFILTER_IPOKEYS) ) 
	{
		/* loop through ipo-curve channels, adding them */
		for (icu= cu->ipo->curve.first; icu; icu=icu->next) {
			/* only if selected (if checking for selection) */
			if ( !(filter_mode & ANIMFILTER_SEL) || (SEL_ICU(icu)) ) {
				ale= make_new_animlistelem(icu, ANIMTYPE_ICU, base, ANIMTYPE_OBJECT);
				if (ale) {
					/* make owner the material not object, so that indent is not just object level */
					ale->id= (ID *)cu;
					BLI_addtail(anim_data, ale); 
				}
			}
		}
	}
}

static void animdata_filter_dopesheet_ob (ListBase *anim_data, bDopeSheet *ads, Base *base, int filter_mode)
{
	bAnimListElem *ale=NULL;
	Scene *sce= (Scene *)ads->source;
	Object *ob= base->object;
	Key *key= ob_get_key(ob);
	IpoCurve *icu;
	
	/* add this object as a channel first */
	if (!(filter_mode & ANIMFILTER_ONLYICU) && !(filter_mode & ANIMFILTER_IPOKEYS)) {
		/* check if filtering by selection */
		if ( !(filter_mode & ANIMFILTER_SEL) || ((base->flag & SELECT) || (base == sce->basact)) ) {
			ale= make_new_animlistelem(base, ANIMTYPE_OBJECT, NULL, ANIMTYPE_NONE);
			if (ale) BLI_addtail(anim_data, ale);
		}
	}
	
	/* if collapsed, don't go any further (unless adding keyframes only) */
	if ( (EXPANDED_OBJC(ob) == 0) && !(filter_mode & (ANIMFILTER_IPOKEYS|ANIMFILTER_ONLYICU)) )
		return;
	
	/* IPO? */
	if ((ob->ipo) && !(ads->filterflag & ADS_FILTER_NOIPOS)) {		
		/* include ipo-expand widget? */
		if (filter_mode & (ANIMFILTER_CHANNELS|ANIMFILTER_IPOKEYS)) {
			ale= make_new_animlistelem(ob, ANIMTYPE_FILLIPOD, base, ANIMTYPE_OBJECT);
			if (ale) BLI_addtail(anim_data, ale);
		}
		
		/* add ipo-curve channels? */
		if ( (FILTER_IPO_OBJC(ob) || (filter_mode & ANIMFILTER_ONLYICU)) && 
			  !(filter_mode & ANIMFILTER_IPOKEYS) ) 
		{
			/* loop through ipo-curve channels, adding them */
			for (icu= ob->ipo->curve.first; icu; icu=icu->next) {
				/* only if selected (if checking for selection) */
				if ( !(filter_mode & ANIMFILTER_SEL) || (SEL_ICU(icu)) ) {
					ale= make_new_animlistelem(icu, ANIMTYPE_ICU, base, ANIMTYPE_OBJECT);
					if (ale) BLI_addtail(anim_data, ale); 
				}
			}
		}
	}
	
	/* Action? */
	if ((ob->action) && !(ads->filterflag & ADS_FILTER_NOACTS)) {
		/* include animion-expand widget? */
		if ((filter_mode & ANIMFILTER_CHANNELS) && !(filter_mode & (ANIMFILTER_IPOKEYS|ANIMFILTER_ONLYICU))) {
			ale= make_new_animlistelem(ob->action, ANIMTYPE_FILLACTD, base, ANIMTYPE_OBJECT);
			if (ale) {
				ale->id= (ID *)ob; // err.... is this a good idea?
				BLI_addtail(anim_data, ale);
			}
		}
		
		/* add ipo-curve channels? */
		if (EXPANDED_ACTC(ob->action) || !(filter_mode & (ANIMFILTER_CHANNELS|ANIMFILTER_FORDRAWING))) {
			// need to make the ownertype normal object here... (maybe type should be a separate one for clarity?)
			animdata_filter_action(anim_data, ob->action, filter_mode, ob, ANIMTYPE_OBJECT); 
		}
	}
	
	/* ShapeKeys? */
	if ((key) && !(ads->filterflag & ADS_FILTER_NOSHAPEKEYS)) {
		/* include shapekey-expand widget? */
		if ((filter_mode & ANIMFILTER_CHANNELS) && !(filter_mode & (ANIMFILTER_IPOKEYS|ANIMFILTER_ONLYICU))) {
			ale= make_new_animlistelem(key, ANIMTYPE_DSSKEY, base, ANIMTYPE_OBJECT);
			if (ale) BLI_addtail(anim_data, ale);
		}
		
		/* add channels */
		if (FILTER_SKE_OBJD(key) || (filter_mode & ANIMFILTER_IPOKEYS) || (filter_mode & ANIMFILTER_ONLYICU)) {
			animdata_filter_shapekey (anim_data, key, filter_mode, ob, ANIMTYPE_OBJECT);
		}
	}
	
	/* Materials? */
	if ((ob->totcol) && !(ads->filterflag & ADS_FILTER_NOMAT))
		animdata_filter_dopesheet_mats(anim_data, ads, base, filter_mode);
	
	/* Object Data */
	switch (ob->type) {
		case OB_CAMERA: /* ------- Camera ------------ */
		{
			Camera *ca= (Camera *)ob->data;
			if ((ca->ipo) && !(ads->filterflag & ADS_FILTER_NOCAM))
				animdata_filter_dopesheet_cam(anim_data, ads, base, filter_mode);
		}
			break;
		case OB_LAMP: /* ---------- Lamp ----------- */
		{
			Lamp *la= (Lamp *)ob->data;
			if ((la->ipo) && !(ads->filterflag & ADS_FILTER_NOLAM))
				animdata_filter_dopesheet_lamp(anim_data, ads, base, filter_mode);
		}
			break;
		case OB_CURVE: /* ------- Curve ---------- */
		{
			Curve *cu= (Curve *)ob->data;
			if ((cu->ipo) && !(ads->filterflag & ADS_FILTER_NOCUR))
				animdata_filter_dopesheet_curve(anim_data, ads, base, filter_mode);
		}
			break;
	}
	
	/* Constraint Channels? */
	if ((ob->constraintChannels.first) && !(ads->filterflag & ADS_FILTER_NOCONSTRAINTS)) {
		bConstraintChannel *conchan;
		
		/* include constraint-expand widget? */
		if ( (filter_mode & ANIMFILTER_CHANNELS) && !(filter_mode & ANIMFILTER_ONLYICU)
			 && !(filter_mode & ANIMFILTER_IPOKEYS) ) 
		{
			ale= make_new_animlistelem(ob, ANIMTYPE_FILLCOND, base, ANIMTYPE_OBJECT);
			if (ale) BLI_addtail(anim_data, ale);
		}
		
		/* add constraint channels? */
		if (FILTER_CON_OBJC(ob) || (filter_mode & ANIMFILTER_IPOKEYS) || (filter_mode & ANIMFILTER_ONLYICU)) {
			/* loop through constraint channels, checking and adding them */
			for (conchan=ob->constraintChannels.first; conchan; conchan=conchan->next) {
				/* only work with this channel and its subchannels if it is editable */
				if (!(filter_mode & ANIMFILTER_FOREDIT) || EDITABLE_CONCHAN(conchan)) {
					/* check if this conchan should only be included if it is selected */
					if (!(filter_mode & ANIMFILTER_SEL) || SEL_CONCHAN(conchan)) {
						if (filter_mode & ANIMFILTER_IPOKEYS) {
							ale= make_new_animlistelem(conchan, ANIMTYPE_CONCHAN2, base, ANIMTYPE_OBJECT);
							if (ale) {
								ale->id= (ID *)ob;
								BLI_addtail(anim_data, ale);
							}
						}
						else {
							ale= make_new_animlistelem(conchan, ANIMTYPE_CONCHAN, base, ANIMTYPE_OBJECT);
							if (ale) {
								ale->id= (ID *)ob;
								BLI_addtail(anim_data, ale);
							}
						}
					}
				}
			}
		}
	}
}	

// TODO: implement pinning... (if and when pinning is done, what we need to do is to provide freeing mechanisms - to protect against data that was deleted)
static void animdata_filter_dopesheet (ListBase *anim_data, bDopeSheet *ads, int filter_mode)
{
	Scene *sce= (Scene *)ads->source;
	Base *base;
	
	/* check that we do indeed have a scene */
	if ((ads->source == NULL) || (GS(ads->source->name)!=ID_SCE)) {
		printf("DopeSheet Error: Not scene! \n");
		return;
	}
	
	/* loop over all bases in the scene */
	for (base= sce->base.first; base; base= base->next) {
		/* check if there's an object (all the relevant checks are done in the ob-function) */
		if (base->object) {
			Object *ob= base->object;
			Key *key= ob_get_key(ob);
			short ipoOk, actOk, constsOk, keyOk, dataOk;
			
			/* firstly, check if object can be included, by the following fanimors:
			 *	- if only visible, must check for layer and also viewport visibility
			 *	- if only selected, must check if object is selected 
			 *	- there must be animation data to edit
			 */
			// TODO: if cache is implemented, just check name here, and then 
			if (filter_mode & ANIMFILTER_VISIBLE) {
				/* layer visibility */
				if ((ob->lay & sce->lay)==0) continue;
				
				/* outliner restrict-flag */
				if (ob->restrictflag & OB_RESTRICT_VIEW) continue;
			}
			
			/* additionally, dopesheet filtering also affects what objects to consider */
			if (ads->filterflag) {
				/* check selection and object type filters */
				if ( (ads->filterflag & ADS_FILTER_ONLYSEL) && !((base->flag & SELECT) || (base == sce->basact)) )  {
					/* only selected should be shown */
					continue;
				}
				if ((ads->filterflag & ADS_FILTER_NOARM) && (ob->type == OB_ARMATURE)) {
					/* not showing armatures  */
					continue;
				}
				if ((ads->filterflag & ADS_FILTER_NOOBJ) && (ob->type != OB_ARMATURE)) {
					/* not showing objects that aren't armatures */
					continue;
				}
				
				/* check filters for datatypes */
				ipoOk= ((ob->ipo) && !(ads->filterflag & ADS_FILTER_NOIPOS));
				actOk= ((ob->action) && !(ads->filterflag & ADS_FILTER_NOACTS));
				constsOk= ((ob->constraintChannels.first) && !(ads->filterflag & ADS_FILTER_NOCONSTRAINTS));
				keyOk= ((key) && !(ads->filterflag & ADS_FILTER_NOSHAPEKEYS));
				
				switch (ob->type) {
					case OB_CAMERA: /* ------- Camera ------------ */
					{
						Camera *ca= (Camera *)ob->data;
						dataOk= ((ca->ipo) && !(ads->filterflag & ADS_FILTER_NOCAM));						
					}
						break;
					case OB_LAMP: /* ---------- Lamp ----------- */
					{
						Lamp *la= (Lamp *)ob->data;
						dataOk= ((la->ipo) && !(ads->filterflag & ADS_FILTER_NOLAM));
					}
						break;
					case OB_CURVE: /* -------- Curve ---------- */
					{
						Curve *cu= (Curve *)ob->data;
						dataOk= ((cu->ipo) && !(ads->filterflag & ADS_FILTER_NOCUR));
					}
						break;
					default: /* --- other --- */
						dataOk= 0;
						break;
				}
				
				/* check if all bad (i.e. nothing to show) */
				if (!ipoOk && !actOk && !constsOk && !keyOk && !dataOk)
					continue;
			}
			else {
				/* check data-types */
				ipoOk= (ob->ipo != NULL);
				actOk= (ob->action != NULL);
				constsOk= (ob->constraintChannels.first != NULL);
				keyOk= (key != NULL);
				
				switch (ob->type) {
					case OB_CAMERA: /* ------- Camera ------------ */
					{
						Camera *ca= (Camera *)ob->data;
						dataOk= (ca->ipo != NULL);						
					}
						break;
					case OB_LAMP: /* ---------- Lamp ----------- */
					{
						Lamp *la= (Lamp *)ob->data;
						dataOk= (la->ipo != NULL);
					}
						break;
					case OB_CURVE: /* -------- Curve ---------- */
					{
						Curve *cu= (Curve *)ob->data;
						dataOk= (cu->ipo != NULL);
					}
						break;
					default: /* --- other --- */
						dataOk= 0;
						break;
				}
				
				/* check if all bad (i.e. nothing to show) */
				if (!ipoOk && !actOk && !constsOk && !keyOk && !dataOk)
					continue;
			}
			
			/* since we're still here, this object should be usable */
			animdata_filter_dopesheet_ob(anim_data, ads, base, filter_mode);
		}
	}
}

/* ----------- Public API --------------- */

/* This function filters the active data source to leave only the desired
 * data types. 'Public' api call.
 * 	*act_data: is a pointer to a ListBase, to which the filtered animation channels
 *		will be placed for use.
 *	filter_mode: how should the data be filtered - bitmapping accessed flags
 */
void ANIM_animdata_filter (ListBase *anim_data, int filter_mode, void *data, short datatype)
{
	/* only filter data if there's somewhere to put it */
	if (data && anim_data) {
		bAnimListElem *ale, *next;
		
		/* firstly filter the data */
		switch (datatype) {
			case ANIMCONT_ACTION:
				animdata_filter_action(anim_data, data, filter_mode, NULL, ANIMTYPE_NONE);
				break;
			case ANIMCONT_SHAPEKEY:
				animdata_filter_shapekey(anim_data, data, filter_mode, NULL, ANIMTYPE_NONE);
				break;
			case ANIMCONT_GPENCIL:
				//animdata_filter_gpencil(anim_data, data, filter_mode);
				break;
			case ANIMCONT_DOPESHEET:
				animdata_filter_dopesheet(anim_data, data, filter_mode);
				break;
		}
			
		/* remove any weedy entries */
		// XXX this is weedy code!
		for (ale= anim_data->first; ale; ale= next) {
			next= ale->next;
			
			if (ale->type == ANIMTYPE_NONE)
				BLI_freelinkN(anim_data, ale);
			
			if (filter_mode & ANIMFILTER_IPOKEYS) {
				if (ale->datatype != ALE_IPO)
					BLI_freelinkN(anim_data, ale);
				else if (ale->key_data == NULL)
					BLI_freelinkN(anim_data, ale);
			}
		}
	}
}

/* ************************************************************ */
