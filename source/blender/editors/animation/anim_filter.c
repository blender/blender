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
 * The Original Code is Copyright (C) 2008 Blender Foundation, Joshua Leung
 * All rights reserved.
 *
 * 
 * Contributor(s): Joshua Leung (original author)
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
 * the Graph Editor also uses this for its channel list and for determining which curves
 * are being edited. Likewise, the NLA Editor also uses this for its channel list and in
 * its operators.
 *
 * Note: much of the original system this was based on was built before the creation of the RNA
 * system. In future, it would be interesting to replace some parts of this code with RNA queries,
 * however, RNA does not eliminate some of the boiler-plate reduction benefits presented by this 
 * system, so if any such work does occur, it should only be used for the internals used here...
 *
 * -- Joshua Leung, Dec 2008 (Last revision July 2009)
 */

#include <string.h>
#include <stdio.h>

#include "DNA_listBase.h"
#include "DNA_ID.h"
#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
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
#include "DNA_meta_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_space_types.h"
#include "DNA_sequence_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_world_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_fcurve.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_object.h"
#include "BKE_material.h"
#include "BKE_node.h"
#include "BKE_sequencer.h"
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
Key *actedit_get_shapekeys (bAnimContext *ac, SpaceAction *saction) 
{
    Scene *scene= ac->scene;
    Object *ob;
    Key *key;
	
    ob = OBACT;
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
static short actedit_get_context (bAnimContext *ac, SpaceAction *saction)
{
	/* sync settings with current view status, then return appropriate data */
	switch (saction->mode) {
		case SACTCONT_ACTION: /* 'Action Editor' */
			/* if not pinned, sync with active object */
			if (saction->pin == 0) {
				if (ac->obact && ac->obact->adt)
					saction->action = ac->obact->adt->action;
				else
					saction->action= NULL;
			}
			
			ac->datatype= ANIMCONT_ACTION;
			ac->data= saction->action;
			
			ac->mode= saction->mode;
			return 1;
			
		case SACTCONT_SHAPEKEY: /* 'ShapeKey Editor' */
			ac->datatype= ANIMCONT_SHAPEKEY;
			ac->data= actedit_get_shapekeys(ac, saction);
			
			ac->mode= saction->mode;
			return 1;
			
		case SACTCONT_GPENCIL: /* Grease Pencil */ // XXX review how this mode is handled...
			ac->datatype=ANIMCONT_GPENCIL;
			//ac->data= CTX_wm_screen(C); // FIXME: add that dopesheet type thing here!
			ac->data= NULL; // !!!
			
			ac->mode= saction->mode;
			return 1;
			
		case SACTCONT_DOPESHEET: /* DopeSheet */
			/* update scene-pointer (no need to check for pinning yet, as not implemented) */
			saction->ads.source= (ID *)ac->scene;
			
			ac->datatype= ANIMCONT_DOPESHEET;
			ac->data= &saction->ads;
			
			ac->mode= saction->mode;
			return 1;
		
		default: /* unhandled yet */
			ac->datatype= ANIMCONT_NONE;
			ac->data= NULL;
			
			ac->mode= -1;
			return 0;
	}
}

/* ----------- Private Stuff - Graph Editor ------------- */

/* Get data being edited in Graph Editor (depending on current 'mode') */
static short graphedit_get_context (bAnimContext *ac, SpaceIpo *sipo)
{
	/* init dopesheet data if non-existant (i.e. for old files) */
	if (sipo->ads == NULL) {
		sipo->ads= MEM_callocN(sizeof(bDopeSheet), "GraphEdit DopeSheet");
		sipo->ads->source= (ID *)ac->scene;
	}
	
	/* set settings for Graph Editor - "Selected = Editable" */
	if (sipo->flag & SIPO_SELCUVERTSONLY)
		sipo->ads->filterflag |= ADS_FILTER_SELEDIT;
	else
		sipo->ads->filterflag &= ~ADS_FILTER_SELEDIT;
	
	/* sync settings with current view status, then return appropriate data */
	switch (sipo->mode) {
		case SIPO_MODE_ANIMATION:	/* Animation F-Curve Editor */
			/* update scene-pointer (no need to check for pinning yet, as not implemented) */
			sipo->ads->source= (ID *)ac->scene;
			sipo->ads->filterflag &= ~ADS_FILTER_ONLYDRIVERS;
			
			ac->datatype= ANIMCONT_FCURVES;
			ac->data= sipo->ads;
			
			ac->mode= sipo->mode;
			return 1;
		
		case SIPO_MODE_DRIVERS:		/* Driver F-Curve Editor */
			/* update scene-pointer (no need to check for pinning yet, as not implemented) */
			sipo->ads->source= (ID *)ac->scene;
			sipo->ads->filterflag |= ADS_FILTER_ONLYDRIVERS;
			
			ac->datatype= ANIMCONT_DRIVERS;
			ac->data= sipo->ads;
			
			ac->mode= sipo->mode;
			return 1;
		
		default: /* unhandled yet */
			ac->datatype= ANIMCONT_NONE;
			ac->data= NULL;
			
			ac->mode= -1;
			return 0;
	}
}

/* ----------- Private Stuff - NLA Editor ------------- */

/* Get data being edited in Graph Editor (depending on current 'mode') */
static short nlaedit_get_context (bAnimContext *ac, SpaceNla *snla)
{
	/* init dopesheet data if non-existant (i.e. for old files) */
	if (snla->ads == NULL)
		snla->ads= MEM_callocN(sizeof(bDopeSheet), "NlaEdit DopeSheet");
	
	/* sync settings with current view status, then return appropriate data */
	/* update scene-pointer (no need to check for pinning yet, as not implemented) */
	snla->ads->source= (ID *)ac->scene;
	snla->ads->filterflag |= ADS_FILTER_ONLYNLA;
	
	ac->datatype= ANIMCONT_NLA;
	ac->data= snla->ads;
	
	return 1;
}

/* ----------- Public API --------------- */

/* Obtain current anim-data context, given that context info from Blender context has already been set 
 *	- AnimContext to write to is provided as pointer to var on stack so that we don't have
 *	  allocation/freeing costs (which are not that avoidable with channels).
 */
short ANIM_animdata_context_getdata (bAnimContext *ac)
{
	ScrArea *sa= ac->sa;
	short ok= 0;
	
	/* context depends on editor we are currently in */
	if (sa) {
		switch (sa->spacetype) {
			case SPACE_ACTION:
			{
				SpaceAction *saction= (SpaceAction *)sa->spacedata.first;
				ok= actedit_get_context(ac, saction);
			}
				break;
				
			case SPACE_IPO:
			{
				SpaceIpo *sipo= (SpaceIpo *)sa->spacedata.first;
				ok= graphedit_get_context(ac, sipo);
			}
				break;
				
			case SPACE_NLA:
			{
				SpaceNla *snla= (SpaceNla *)sa->spacedata.first;
				ok= nlaedit_get_context(ac, snla);
			}
				break;
		}
	}
	
	/* check if there's any valid data */
	if (ok && ac->data)
		return 1;
	else
		return 0;
}

/* Obtain current anim-data context from Blender Context info 
 *	- AnimContext to write to is provided as pointer to var on stack so that we don't have
 *	  allocation/freeing costs (which are not that avoidable with channels).
 *	- Clears data and sets the information from Blender Context which is useful
 */
short ANIM_animdata_get_context (const bContext *C, bAnimContext *ac)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= CTX_wm_region(C);
	Scene *scene= CTX_data_scene(C);
	
	/* clear old context info */
	if (ac == NULL) return 0;
	memset(ac, 0, sizeof(bAnimContext));
	
	/* get useful default context settings from context */
	ac->scene= scene;
	if (scene) {
		ac->markers= &scene->markers;		
		ac->obact= (scene->basact)?  scene->basact->object : NULL;
	}
	ac->sa= sa;
	ac->ar= ar;
	ac->spacetype= (sa) ? sa->spacetype : 0;
	ac->regiontype= (ar) ? ar->regiontype : 0;
	
	/* get data context info */
	return ANIM_animdata_context_getdata(ac);
}

/* ************************************************************ */
/* Blender Data <-- Filter --> Channels to be operated on */

/* quick macro to test if AnimData is usable */
#define ANIMDATA_HAS_KEYS(id) ((id)->adt && (id)->adt->action)

/* quick macro to test if AnimData is usable for drivers */
#define ANIMDATA_HAS_DRIVERS(id) ((id)->adt && (id)->adt->drivers.first)

/* quick macro to test if AnimData is usable for NLA */
#define ANIMDATA_HAS_NLA(id) ((id)->adt && (id)->adt->nla_tracks.first)


/* Quick macro to test for all three avove usability tests, performing the appropriate provided 
 * action for each when the AnimData context is appropriate. 
 *
 * Priority order for this goes (most important, to least): AnimData blocks, NLA, Drivers, Keyframes.
 *
 * For this to work correctly, a standard set of data needs to be available within the scope that this
 * gets called in: 
 *	- ListBase anim_data;
 *	- bDopeSheet *ads;
 *	- bAnimListElem *ale;
 * 	- int items;
 *
 * 	- id: ID block which should have an AnimData pointer following it immediately, to use
 *	- adtOk: line or block of code to execute for AnimData-blocks case (usually ANIMDATA_ADD_ANIMDATA)
 *	- nlaOk: line or block of code to execute for NLA tracks+strips case
 *	- driversOk: line or block of code to execute for Drivers case
 *	- keysOk: line or block of code for Keyframes case
 *
 * The checks for the various cases are as follows:
 *	0) top level: checks for animdata and also that all the F-Curves for the block will be visible
 *	1) animdata check: for filtering animdata blocks only
 *	2A) nla tracks: include animdata block's data as there are NLA tracks+strips there
 *	2B) actions to convert to nla: include animdata block's data as there is an action that can be 
 *		converted to a new NLA strip, and the filtering options allow this
 *	3) drivers: include drivers from animdata block (for Drivers mode in Graph Editor)
 *	4) normal keyframes: only when there is an active action
 */
#define ANIMDATA_FILTER_CASES(id, adtOk, nlaOk, driversOk, keysOk) \
	{\
		if ((id)->adt) {\
			if (!(filter_mode & ANIMFILTER_CURVEVISIBLE) || !((id)->adt->flag & ADT_CURVES_NOT_VISIBLE)) {\
				if (filter_mode & ANIMFILTER_ANIMDATA) {\
					adtOk\
				}\
				else if (ads->filterflag & ADS_FILTER_ONLYNLA) {\
					if (ANIMDATA_HAS_NLA(id)) {\
						nlaOk\
					}\
					else if (!(ads->filterflag & ADS_FILTER_NLA_NOACT) && ANIMDATA_HAS_KEYS(id)) {\
						nlaOk\
					}\
				}\
				else if (ads->filterflag & ADS_FILTER_ONLYDRIVERS) {\
					if (ANIMDATA_HAS_DRIVERS(id)) {\
						driversOk\
					}\
				}\
				else {\
					if (ANIMDATA_HAS_KEYS(id)) {\
						keysOk\
					}\
				}\
			}\
		}\
	}


/* quick macro to add a pointer to an AnimData block as a channel */
#define ANIMDATA_ADD_ANIMDATA(id) \
	{\
		ale= make_new_animlistelem((id)->adt, ANIMTYPE_ANIMDATA, NULL, ANIMTYPE_NONE, (ID *)id);\
		if (ale) {\
			BLI_addtail(anim_data, ale);\
			items++;\
		}\
	}
	
/* quick macro to test if an anim-channel representing an AnimData block is suitably active */
#define ANIMCHANNEL_ACTIVEOK(ale) \
	( !(filter_mode & ANIMFILTER_ACTIVE) || !(ale->adt) || (ale->adt->flag & ADT_UI_ACTIVE) )

/* quick macro to test if an anim-channel (F-Curve, Group, etc.) is selected in an acceptable way */
#define ANIMCHANNEL_SELOK(test_func) \
		( !(filter_mode & (ANIMFILTER_SEL|ANIMFILTER_UNSEL)) || \
		  ((filter_mode & ANIMFILTER_SEL) && test_func) || \
		  ((filter_mode & ANIMFILTER_UNSEL) && test_func==0) ) 
		  
/* quick macro to test if an anim-channel (F-Curve) is selected ok for editing purposes 
 *	- _SELEDIT means that only selected curves will have visible+editable keyframes
 */
// FIXME: this doesn't work cleanly yet...
#define ANIMCHANNEL_SELEDITOK(test_func) \
		( !(filter_mode & ANIMFILTER_SELEDIT) || \
		  (filter_mode & ANIMFILTER_CHANNELS) || \
		  (test_func) )

/* ----------- 'Private' Stuff --------------- */

/* this function allocates memory for a new bAnimListElem struct for the 
 * provided animation channel-data. 
 */
bAnimListElem *make_new_animlistelem (void *data, short datatype, void *owner, short ownertype, ID *owner_id)
{
	bAnimListElem *ale= NULL;
	
	/* only allocate memory if there is data to convert */
	if (data) {
		/* allocate and set generic data */
		ale= MEM_callocN(sizeof(bAnimListElem), "bAnimListElem");
		
		ale->data= data;
		ale->type= datatype;
			// XXX what is the point of the owner data?
		ale->owner= owner;
		ale->ownertype= ownertype;
		
		ale->id= owner_id;
		ale->adt= BKE_animdata_from_id(owner_id);
		
		/* do specifics */
		switch (datatype) {
			case ANIMTYPE_SUMMARY:
			{
				/* nothing to include for now... this is just a dummy wrappy around all the other channels 
				 * in the DopeSheet, and gets included at the start of the list
				 */
				ale->key_data= NULL;
				ale->datatype= ALE_ALL;
			}
				break;
			
			case ANIMTYPE_SCENE:
			{
				Scene *sce= (Scene *)data;
				
				ale->flag= sce->flag;
				
				ale->key_data= sce;
				ale->datatype= ALE_SCE;
				
				ale->adt= BKE_animdata_from_id(data);
			}
				break;
			case ANIMTYPE_OBJECT:
			{
				Base *base= (Base *)data;
				Object *ob= base->object;
				
				ale->flag= ob->flag;
				
				ale->key_data= ob;
				ale->datatype= ALE_OB;
				
				ale->adt= BKE_animdata_from_id(&ob->id);
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
			case ANIMTYPE_FILLDRIVERS:
			{
				AnimData *adt= (AnimData *)data;
				
				ale->flag= adt->flag;
				
					// XXX... drivers don't show summary for now
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
			case ANIMTYPE_FILLPARTD:
			{
				Object *ob= (Object *)data;
				
				ale->flag= FILTER_PART_OBJC(ob);
				
				ale->key_data= NULL;
				ale->datatype= ALE_NONE;
			}
				break;
			
			case ANIMTYPE_DSMAT:
			{
				Material *ma= (Material *)data;
				AnimData *adt= ma->adt;
				
				ale->flag= FILTER_MAT_OBJD(ma);
				
				ale->key_data= (adt) ? adt->action : NULL;
				ale->datatype= ALE_ACT;
				
				ale->adt= BKE_animdata_from_id(data);
			}
				break;
			case ANIMTYPE_DSLAM:
			{
				Lamp *la= (Lamp *)data;
				AnimData *adt= la->adt;
				
				ale->flag= FILTER_LAM_OBJD(la);
				
				ale->key_data= (adt) ? adt->action : NULL;
				ale->datatype= ALE_ACT;
				
				ale->adt= BKE_animdata_from_id(data);
			}
				break;
			case ANIMTYPE_DSCAM:
			{
				Camera *ca= (Camera *)data;
				AnimData *adt= ca->adt;
				
				ale->flag= FILTER_CAM_OBJD(ca);
				
				ale->key_data= (adt) ? adt->action : NULL;
				ale->datatype= ALE_ACT;
				
				ale->adt= BKE_animdata_from_id(data);
			}
				break;
			case ANIMTYPE_DSCUR:
			{
				Curve *cu= (Curve *)data;
				AnimData *adt= cu->adt;
				
				ale->flag= FILTER_CUR_OBJD(cu);
				
				ale->key_data= (adt) ? adt->action : NULL;
				ale->datatype= ALE_ACT;
				
				ale->adt= BKE_animdata_from_id(data);
			}
				break;
			case ANIMTYPE_DSARM:
			{
				bArmature *arm= (bArmature *)data;
				AnimData *adt= arm->adt;
				
				ale->flag= FILTER_ARM_OBJD(arm);
				
				ale->key_data= (adt) ? adt->action : NULL;
				ale->datatype= ALE_ACT;
				
				ale->adt= BKE_animdata_from_id(data);
			}
				break;
			case ANIMTYPE_DSSKEY:
			{
				Key *key= (Key *)data;
				AnimData *adt= key->adt;
				
				ale->flag= FILTER_SKE_OBJD(key); 
				
				ale->key_data= (adt) ? adt->action : NULL;
				ale->datatype= ALE_ACT;
				
				ale->adt= BKE_animdata_from_id(data);
			}
				break;
			case ANIMTYPE_DSWOR:
			{
				World *wo= (World *)data;
				AnimData *adt= wo->adt;
				
				ale->flag= FILTER_WOR_SCED(wo); 
				
				ale->key_data= (adt) ? adt->action : NULL;
				ale->datatype= ALE_ACT;
				
				ale->adt= BKE_animdata_from_id(data);
			}
				break;
			case ANIMTYPE_DSNTREE:
			{
				bNodeTree *ntree= (bNodeTree *)data;
				AnimData *adt= ntree->adt;
				
				ale->flag= FILTER_NTREE_SCED(ntree); 
				
				ale->key_data= (adt) ? adt->action : NULL;
				ale->datatype= ALE_ACT;
				
				ale->adt= BKE_animdata_from_id(data);
			}
				break;
			case ANIMTYPE_DSPART:
			{
				ParticleSettings *part= (ParticleSettings*)ale->data;
				AnimData *adt= part->adt;
				
				ale->flag= FILTER_PART_OBJD(part); 
				
				ale->key_data= (adt) ? adt->action : NULL;
				ale->datatype= ALE_ACT;
				
				ale->adt= BKE_animdata_from_id(data);
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
			case ANIMTYPE_FCURVE:
			{
				FCurve *fcu= (FCurve *)data;
				
				ale->flag= fcu->flag;
				
				ale->key_data= fcu;
				ale->datatype= ALE_FCURVE;
			}
				break;
				
			case ANIMTYPE_SHAPEKEY:
			{
				KeyBlock *kb= (KeyBlock *)data;
				Key *key= (Key *)ale->id;
				
				ale->flag= kb->flag;
				
				/* whether we have keyframes depends on whether there is a Key block to find it from */
				if (key) {
					/* index of shapekey is defined by place in key's list */
					ale->index= BLI_findindex(&key->block, kb);
					
					/* the corresponding keyframes are from the animdata */
					if (ale->adt && ale->adt->action) {
						bAction *act= ale->adt->action;
						char *rna_path = key_get_curValue_rnaPath(key, kb);
						
						/* try to find the F-Curve which corresponds to this exactly,
						 * then free the MEM_alloc'd string
						 */
						if (rna_path) {
							ale->key_data= (void *)list_find_fcurve(&act->curves, rna_path, 0);
							MEM_freeN(rna_path);
						}
					}
					ale->datatype= (ale->key_data)? ALE_FCURVE : ALE_NONE;
				}
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
				
			case ANIMTYPE_NLATRACK:
			{
				NlaTrack *nlt= (NlaTrack *)data;
				
				ale->flag= nlt->flag;
				
				ale->key_data= &nlt->strips;
				ale->datatype= ALE_NLASTRIP;
			}
				break;
			case ANIMTYPE_NLAACTION:
			{
				/* nothing to include for now... nothing editable from NLA-perspective here */
				ale->key_data= NULL;
				ale->datatype= ALE_NONE;
			}
				break;
		}
	}
	
	/* return created datatype */
	return ale;
}
 
/* ----------------------------------------- */

static int skip_fcurve_selected_data(FCurve *fcu, ID *owner_id)
{
	if (GS(owner_id->name) == ID_OB) {
		Object *ob= (Object *)owner_id;
		
		/* only consider if F-Curve involves pose.bones */
		if ((fcu->rna_path) && strstr(fcu->rna_path, "bones")) {
			bPoseChannel *pchan;
			char *bone_name;
			
			/* get bone-name, and check if this bone is selected */
			bone_name= BLI_getQuotedStr(fcu->rna_path, "bones[");
			pchan= get_pose_channel(ob->pose, bone_name);
			if (bone_name) MEM_freeN(bone_name);
			
			/* can only add this F-Curve if it is selected */
			if ((pchan) && (pchan->bone) && (pchan->bone->flag & BONE_SELECTED)==0)
				return 1;
		}
	}
	else if (GS(owner_id->name) == ID_SCE) {
		Scene *scene = (Scene *)owner_id;
		
		/* only consider if F-Curve involves sequence_editor.sequences */
		if ((fcu->rna_path) && strstr(fcu->rna_path, "sequences_all")) {
			Editing *ed= seq_give_editing(scene, FALSE);
			Sequence *seq;
			char *seq_name;
			
			/* get strip name, and check if this strip is selected */
			seq_name= BLI_getQuotedStr(fcu->rna_path, "sequences_all[");
			seq = get_seq_by_name(ed->seqbasep, seq_name, FALSE);
			if (seq_name) MEM_freeN(seq_name);
			
			/* can only add this F-Curve if it is selected */
			if (seq==NULL || (seq->flag & SELECT)==0)
				return 1;
		}
	}
	else if (GS(owner_id->name) == ID_NT) {
		bNodeTree *ntree = (bNodeTree *)owner_id;
		
		/* check for selected  nodes */
		if ((fcu->rna_path) && strstr(fcu->rna_path, "nodes")) {
			bNode *node;
			char *node_name;
			
			/* get strip name, and check if this strip is selected */
			node_name= BLI_getQuotedStr(fcu->rna_path, "nodes[");
			node = nodeFindNodebyName(ntree, node_name);
			if (node_name) MEM_freeN(node_name);
			
			/* can only add this F-Curve if it is selected */
			if ((node) && (node->flag & NODE_SELECT)==0)
				return 1;
		}
	}
	return 0;
}

/* find the next F-Curve that is usable for inclusion */
static FCurve *animdata_filter_fcurve_next (bDopeSheet *ads, FCurve *first, bActionGroup *grp, int filter_mode, ID *owner_id)
{
	FCurve *fcu = NULL;
	
	/* loop over F-Curves - assume that the caller of this has already checked that these should be included 
	 * NOTE: we need to check if the F-Curves belong to the same group, as this gets called for groups too...
	 */
	for (fcu= first; ((fcu) && (fcu->grp==grp)); fcu= fcu->next) {
		/* special exception for Pose-Channel Based F-Curves:
		 *	- the 'Only Selected' data filter should be applied to Pose-Channel data too, but those are
		 *	  represented as F-Curves. The way the filter for objects worked was to be the first check
		 *	  after 'normal' visibility, so this is done first here too...
		 *	- we currently use an 'approximate' method for getting these F-Curves that doesn't require
		 *	  carefully checking the entire path
		 *	- this will also affect things like Drivers, and also works for Bone Constraints
		 */
		if ( ((ads) && (ads->filterflag & ADS_FILTER_ONLYSEL)) && (owner_id) ) {
			if (skip_fcurve_selected_data(fcu, owner_id))
				continue;
		}
				
		/* only include if visible (Graph Editor check, not channels check) */
		if (!(filter_mode & ANIMFILTER_CURVEVISIBLE) || (fcu->flag & FCURVE_VISIBLE)) {
			/* only work with this channel and its subchannels if it is editable */
			if (!(filter_mode & ANIMFILTER_FOREDIT) || EDITABLE_FCU(fcu)) {
				/* only include this curve if selected in a way consistent with the filtering requirements */
				if ( ANIMCHANNEL_SELOK(SEL_FCU(fcu)) && ANIMCHANNEL_SELEDITOK(SEL_FCU(fcu)) ) {
					/* only include if this curve is active */
					if (!(filter_mode & ANIMFILTER_ACTIVE) || (fcu->flag & FCURVE_ACTIVE)) {
						/* this F-Curve can be used, so return it */
						return fcu;
					}
				}
			}
		}
	}
	
	/* no (more) F-Curves from the list are suitable... */
	return NULL;
}

static int animdata_filter_fcurves (ListBase *anim_data, bDopeSheet *ads, FCurve *first, bActionGroup *grp, void *owner, short ownertype, int filter_mode, ID *owner_id)
{
	FCurve *fcu;
	int items = 0;
	
	/* loop over every F-Curve able to be included 
	 *	- this for-loop works like this: 
	 *		1) the starting F-Curve is assigned to the fcu pointer so that we have a starting point to search from
	 *		2) the first valid F-Curve to start from (which may include the one given as 'first') in the remaining 
	 *		   list of F-Curves is found, and verified to be non-null
	 *		3) the F-Curve referenced by fcu pointer is added to the list
	 *		4) the fcu pointer is set to the F-Curve after the one we just added, so that we can keep going through 
	 *		   the rest of the F-Curve list without an eternal loop. Back to step 2 :)
	 */
	for (fcu=first; ( (fcu = animdata_filter_fcurve_next(ads, fcu, grp, filter_mode, owner_id)) ); fcu=fcu->next)
	{
		bAnimListElem *ale = make_new_animlistelem(fcu, ANIMTYPE_FCURVE, owner, ownertype, owner_id);
		
		if (ale) {
			BLI_addtail(anim_data, ale);
			items++;
		}
	}
	
	/* return the number of items added to the list */
	return items;
}

static int animdata_filter_action (ListBase *anim_data, bDopeSheet *ads, bAction *act, int filter_mode, void *owner, short ownertype, ID *owner_id)
{
	bAnimListElem *ale=NULL;
	bActionGroup *agrp;
	FCurve *lastchan=NULL;
	int items = 0;
	
	/* loop over groups */
	// TODO: in future, should we expect to need nested groups?
	for (agrp= act->groups.first; agrp; agrp= agrp->next) {
		FCurve *first_fcu;
		
		/* store reference to last channel of group */
		if (agrp->channels.last) 
			lastchan= agrp->channels.last;
		
		/* get the first F-Curve in this group we can start to use, 
		 * and if there isn't any F-Curve to start from, then don't 
		 * this group at all...
		 *
		 * exceptions for when we might not care whether there's anything inside this group or not
		 *	- if we're interested in channels and their selections, in which case group channel should get considered too
		 *	  even if all its sub channels are hidden...
		 */
		first_fcu = animdata_filter_fcurve_next(ads, agrp->channels.first, agrp, filter_mode, owner_id);
		
		if ( (filter_mode & (ANIMFILTER_SEL|ANIMFILTER_UNSEL)) ||
			 (first_fcu) ) 
		{
			/* add this group as a channel first */
			if ((filter_mode & ANIMFILTER_CHANNELS) || !(filter_mode & ANIMFILTER_CURVESONLY)) {
				/* check if filtering by selection */
				if ( ANIMCHANNEL_SELOK(SEL_AGRP(agrp)) ) {
					ale= make_new_animlistelem(agrp, ANIMTYPE_GROUP, NULL, ANIMTYPE_NONE, owner_id);
					if (ale) {
						BLI_addtail(anim_data, ale);
						items++;
					}
				}
			}
			
			/* there are some situations, where only the channels of the action group should get considered */
			if (!(filter_mode & ANIMFILTER_ACTGROUPED) || (agrp->flag & AGRP_ACTIVE)) {
				/* filters here are a bit convoulted...
				 *	- groups show a "summary" of keyframes beside their name which must accessable for tools which handle keyframes
				 *	- groups can be collapsed (and those tools which are only interested in channels rely on knowing that group is closed)
				 *
				 * cases when we should include F-Curves inside group:
				 *	- we don't care about visibility
				 *	- group is expanded
				 *	- we just need the F-Curves present
				 */
				if ( (!(filter_mode & ANIMFILTER_VISIBLE) || EXPANDED_AGRP(agrp)) || (filter_mode & ANIMFILTER_CURVESONLY) ) 
				{
					/* for the Graph Editor, curves may be set to not be visible in the view to lessen clutter,
					 * but to do this, we need to check that the group doesn't have it's not-visible flag set preventing 
					 * all its sub-curves to be shown
					 */
					if ( !(filter_mode & ANIMFILTER_CURVEVISIBLE) || !(agrp->flag & AGRP_NOTVISIBLE) )
					{
						if (!(filter_mode & ANIMFILTER_FOREDIT) || EDITABLE_AGRP(agrp)) {
							items += animdata_filter_fcurves(anim_data, ads, first_fcu, agrp, owner, ownertype, filter_mode, owner_id);
						}
					}
				}
			}
		}
	}
	
	/* loop over un-grouped F-Curves (only if we're not only considering those channels in the animive group) */
	if (!(filter_mode & ANIMFILTER_ACTGROUPED))  {
		// XXX the 'owner' info here needs review...
		items += animdata_filter_fcurves(anim_data, ads, (lastchan)?(lastchan->next):(act->curves.first), NULL, owner, ownertype, filter_mode, owner_id);
	}
	
	/* return the number of items added to the list */
	return items;
}

/* Include NLA-Data for NLA-Editor:
 *	- when ANIMFILTER_CHANNELS is used, that means we should be filtering the list for display
 *	  Although the evaluation order is from the first track to the last and then apply the Action on top,
 *	  we present this in the UI as the Active Action followed by the last track to the first so that we 
 *	  get the evaluation order presented as per a stack.
 *	- for normal filtering (i.e. for editing), we only need the NLA-tracks but they can be in 'normal' evaluation
 *	  order, i.e. first to last. Otherwise, some tools may get screwed up.
 */
static int animdata_filter_nla (ListBase *anim_data, bDopeSheet *ads, AnimData *adt, int filter_mode, void *owner, short ownertype, ID *owner_id)
{
	bAnimListElem *ale;
	NlaTrack *nlt;
	NlaTrack *first=NULL, *next=NULL;
	int items = 0;
	
	/* if showing channels, include active action */
	if (filter_mode & ANIMFILTER_CHANNELS) {
		/* there isn't really anything editable here, so skip if need editable */
		// TODO: currently, selection isn't checked since it doesn't matter
		if ((filter_mode & ANIMFILTER_FOREDIT) == 0) { 
			/* just add the action track now (this MUST appear for drawing)
			 *	- as AnimData may not have an action, we pass a dummy pointer just to get the list elem created, then
			 *	  overwrite this with the real value - REVIEW THIS...
			 */
			ale= make_new_animlistelem((void *)(&adt->action), ANIMTYPE_NLAACTION, owner, ownertype, owner_id);
			ale->data= (adt->action) ? adt->action : NULL;
				
			if (ale) {
				BLI_addtail(anim_data, ale);
				items++;
			}
		}
		
		/* first track to include will be the last one if we're filtering by channels */
		first= adt->nla_tracks.last;
	}
	else {
		/* first track to include will the the first one (as per normal) */
		first= adt->nla_tracks.first;
	}
	
	/* loop over NLA Tracks - assume that the caller of this has already checked that these should be included */
	for (nlt= first; nlt; nlt= next) {
		/* 'next' NLA-Track to use depends on whether we're filtering for drawing or not */
		if (filter_mode & ANIMFILTER_CHANNELS) 
			next= nlt->prev;
		else
			next= nlt->next;
		
		/* if we're in NLA-tweakmode, don't show this track if it was disabled (due to tweaking) for now 
		 *	- active track should still get shown though (even though it has disabled flag set)
		 */
		// FIXME: the channels after should still get drawn, just 'differently', and after an active-action channel
		if ((adt->flag & ADT_NLA_EDIT_ON) && (nlt->flag & NLATRACK_DISABLED) && !(nlt->flag & NLATRACK_ACTIVE))
			continue;
		
		/* only work with this channel and its subchannels if it is editable */
		if (!(filter_mode & ANIMFILTER_FOREDIT) || EDITABLE_NLT(nlt)) {
			/* only include this track if selected in a way consistent with the filtering requirements */
			if ( ANIMCHANNEL_SELOK(SEL_NLT(nlt)) ) {
				/* only include if this track is active */
				if (!(filter_mode & ANIMFILTER_ACTIVE) || (nlt->flag & NLATRACK_ACTIVE)) {
					ale= make_new_animlistelem(nlt, ANIMTYPE_NLATRACK, owner, ownertype, owner_id);
						
					if (ale) {
						BLI_addtail(anim_data, ale);
						items++;
					}
				}
			}
		}
	}
	
	/* return the number of items added to the list */
	return items;
}

/* Include ShapeKey Data for ShapeKey Editor */
static int animdata_filter_shapekey (ListBase *anim_data, Key *key, int filter_mode)
{
	bAnimListElem *ale;
	int items = 0;
	
	/* check if channels or only F-Curves */
	if ((filter_mode & ANIMFILTER_CURVESONLY) == 0) {
		KeyBlock *kb;
		
		/* loop through the channels adding ShapeKeys as appropriate */
		for (kb= key->block.first; kb; kb= kb->next) {
			/* skip the first one, since that's the non-animateable basis */
			// XXX maybe in future this may become handy?
			if (kb == key->block.first) continue;
			
			/* only work with this channel and its subchannels if it is editable */
			if (!(filter_mode & ANIMFILTER_FOREDIT) || EDITABLE_SHAPEKEY(kb)) {
				/* only include this track if selected in a way consistent with the filtering requirements */
				if ( ANIMCHANNEL_SELOK(SEL_SHAPEKEY(kb)) ) {
					// TODO: consider 'active' too?
					
					/* owner-id here must be key so that the F-Curve can be resolved... */
					ale= make_new_animlistelem(kb, ANIMTYPE_SHAPEKEY, NULL, ANIMTYPE_NONE, (ID *)key);
					
					if (ale) {
						BLI_addtail(anim_data, ale);
						items++;
					}
				}
			}
		}
	}
	else {
		/* just use the action associated with the shapekey */
		// FIXME: is owner-id and having no owner/dopesheet really fine?
		if (key->adt) {
			if (filter_mode & ANIMFILTER_ANIMDATA)
				ANIMDATA_ADD_ANIMDATA(key)
			else if (key->adt->action)
				items= animdata_filter_action(anim_data, NULL, key->adt->action, filter_mode, NULL, ANIMTYPE_NONE, (ID *)key);
		}
	}
	
	/* return the number of items added to the list */
	return items;
}

#if 0
// FIXME: switch this to use the bDopeSheet...
static int animdata_filter_gpencil (ListBase *anim_data, bScreen *sc, int filter_mode)
{
	bAnimListElem *ale;
	ScrArea *sa, *curarea;
	bGPdata *gpd;
	bGPDlayer *gpl;
	int items = 0;
	
	/* check if filtering types are appropriate */
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
			gpd= gpencil_data_get_active(sa);
			if (gpd == NULL) continue;
			
			/* add gpd as channel too (if for drawing, and it has layers) */
			if ((filter_mode & ANIMFILTER_CHANNELS) && (gpd->layers.first)) {
				/* add to list */
				ale= make_new_animlistelem(gpd, ANIMTYPE_GPDATABLOCK, sa, ANIMTYPE_SPECIALDATA);
				if (ale) {
					BLI_addtail(anim_data, ale);
					items++;
				}
			}
			
			/* only add layers if they will be visible (if drawing channels) */
			if ( !(filter_mode & ANIMFILTER_VISIBLE) || (EXPANDED_GPD(gpd)) ) {
				/* loop over layers as the conditions are acceptable */
				for (gpl= gpd->layers.first; gpl; gpl= gpl->next) {
					/* only if selected */
					if ( ANIMCHANNEL_SELOK(SEL_GPL(gpl)) ) {
						/* only if editable */
						if (!(filter_mode & ANIMFILTER_FOREDIT) || EDITABLE_GPL(gpl)) {
							/* add to list */
							ale= make_new_animlistelem(gpl, ANIMTYPE_GPLAYER, gpd, ANIMTYPE_GPDATABLOCK);
							if (ale) {
								BLI_addtail(anim_data, ale);
								items++;
							}
						}
					}
				}
			}
		}
	}
	
	/* return the number of items added to the list */
	return items;
}
#endif 


static int animdata_filter_dopesheet_mats (ListBase *anim_data, bDopeSheet *ads, Base *base, int filter_mode)
{
	ListBase mats = {NULL, NULL};
	LinkData *ld;
	
	bAnimListElem *ale=NULL;
	Object *ob= base->object;
	int items=0, a=0;
	
	/* firstly check that we actuallly have some materials, by gathering all materials in a temp list */
	for (a=0; a < ob->totcol; a++) {
		Material *ma= give_current_material(ob, a);
		short ok = 0;
		
		/* for now, if no material returned, skip (this shouldn't confuse the user I hope) */
		if (ELEM(NULL, ma, ma->adt)) 
			continue;
		
		/* check if ok */
		ANIMDATA_FILTER_CASES(ma, 
			{ /* AnimData blocks - do nothing... */ },
			ok=1;, 
			ok=1;, 
			ok=1;)
		if (ok == 0) continue;
		
		/* make a temp list elem for this */
		ld= MEM_callocN(sizeof(LinkData), "DopeSheet-MaterialCache");
		ld->data= ma;
		BLI_addtail(&mats, ld);
	}
	
	/* if there were no channels found, no need to carry on */
	if (mats.first == NULL)
		return 0;
	
	/* include materials-expand widget? */
	if ((filter_mode & ANIMFILTER_CHANNELS) && !(filter_mode & ANIMFILTER_CURVESONLY)) {
		ale= make_new_animlistelem(ob, ANIMTYPE_FILLMATD, base, ANIMTYPE_OBJECT, (ID *)ob);
		if (ale) {
			BLI_addtail(anim_data, ale);
			items++;
		}
	}
	
	/* add materials? */
	if (FILTER_MAT_OBJC(ob) || (filter_mode & ANIMFILTER_CURVESONLY)) {
		/* for each material in cache, add channels  */
		for (ld= mats.first; ld; ld= ld->next) {
			Material *ma= (Material *)ld->data;
			
			/* include material-expand widget? */
			// hmm... do we need to store the index of this material in the array anywhere?
			if (filter_mode & ANIMFILTER_CHANNELS) {
				/* check if filtering by active status */
				if ANIMCHANNEL_ACTIVEOK(ma) {
					ale= make_new_animlistelem(ma, ANIMTYPE_DSMAT, base, ANIMTYPE_OBJECT, (ID *)ma);
					if (ale) {
						BLI_addtail(anim_data, ale);
						items++;
					}
				}
			}
			
			/* add material's animation data */
			if (!(filter_mode & ANIMFILTER_VISIBLE) || FILTER_MAT_OBJD(ma) || (filter_mode & ANIMFILTER_CURVESONLY)) {
				ANIMDATA_FILTER_CASES(ma, 
					{ /* AnimData blocks - do nothing... */ },
					items += animdata_filter_nla(anim_data, ads, ma->adt, filter_mode, ma, ANIMTYPE_DSMAT, (ID *)ma);, 
					items += animdata_filter_fcurves(anim_data, ads, ma->adt->drivers.first, NULL, ma, ANIMTYPE_DSMAT, filter_mode, (ID *)ma);, 
					items += animdata_filter_action(anim_data, ads, ma->adt->action, filter_mode, ma, ANIMTYPE_DSMAT, (ID *)ma);)
			}
		}
	}
	
	/* free cache */
	BLI_freelistN(&mats);
	
	/* return the number of items added to the list */
	return items;
}

static int animdata_filter_dopesheet_particles (ListBase *anim_data, bDopeSheet *ads, Base *base, int filter_mode)
{
	bAnimListElem *ale=NULL;
	Object *ob= base->object;
	ParticleSystem *psys = ob->particlesystem.first;
	int items= 0, first = 1;

	for(; psys; psys=psys->next) {
		short ok = 0;

		if(ELEM(NULL, psys->part, psys->part->adt))
			continue;

		ANIMDATA_FILTER_CASES(psys->part,
			{ /* AnimData blocks - do nothing... */ },
			ok=1;, 
			ok=1;, 
			ok=1;)
		if (ok == 0) continue;

		/* include particles-expand widget? */
		if (first && (filter_mode & ANIMFILTER_CHANNELS) && !(filter_mode & ANIMFILTER_CURVESONLY)) {
			ale= make_new_animlistelem(ob, ANIMTYPE_FILLPARTD, base, ANIMTYPE_OBJECT, (ID *)ob);
			if (ale) {
				BLI_addtail(anim_data, ale);
				items++;
			}
			first = 0;
		}
		
		/* add particle settings? */
		if (FILTER_PART_OBJC(ob) || (filter_mode & ANIMFILTER_CURVESONLY)) {
			if ((filter_mode & ANIMFILTER_CHANNELS)) {
				/* check if filtering by active status */
				if ANIMCHANNEL_ACTIVEOK(psys->part) {
					ale = make_new_animlistelem(psys->part, ANIMTYPE_DSPART, base, ANIMTYPE_OBJECT, (ID *)psys->part);
					if (ale) {
						BLI_addtail(anim_data, ale);
						items++;
					}
				}
			}
			
			if (!(filter_mode & ANIMFILTER_VISIBLE) || FILTER_PART_OBJD(psys->part) || (filter_mode & ANIMFILTER_CURVESONLY)) {
				ANIMDATA_FILTER_CASES(psys->part,
					{ /* AnimData blocks - do nothing... */ },
					items += animdata_filter_nla(anim_data, ads, psys->part->adt, filter_mode, psys->part, ANIMTYPE_DSPART, (ID *)psys->part);, 
					items += animdata_filter_fcurves(anim_data, ads, psys->part->adt->drivers.first, NULL, psys->part, ANIMTYPE_DSPART, filter_mode, (ID *)psys->part);, 
					items += animdata_filter_action(anim_data, ads, psys->part->adt->action, filter_mode, psys->part, ANIMTYPE_DSPART, (ID *)psys->part);)
			}
		}
	}
	
	/* return the number of items added to the list */
	return items;
}

static int animdata_filter_dopesheet_obdata (ListBase *anim_data, bDopeSheet *ads, Base *base, int filter_mode)
{
	bAnimListElem *ale=NULL;
	Object *ob= base->object;
	IdAdtTemplate *iat= ob->data;
	AnimData *adt= iat->adt;
	short type=0, expanded=0;
	int items= 0;
	
	/* get settings based on data type */
	switch (ob->type) {
		case OB_CAMERA: /* ------- Camera ------------ */
		{
			Camera *ca= (Camera *)ob->data;
			
			type= ANIMTYPE_DSCAM;
			expanded= FILTER_CAM_OBJD(ca);
		}
			break;
		case OB_LAMP: /* ---------- Lamp ----------- */
		{
			Lamp *la= (Lamp *)ob->data;
			
			type= ANIMTYPE_DSLAM;
			expanded= FILTER_LAM_OBJD(la);
		}
			break;
		case OB_CURVE: /* ------- Curve ---------- */
		{
			Curve *cu= (Curve *)ob->data;
			
			type= ANIMTYPE_DSCUR;
			expanded= FILTER_CUR_OBJD(cu);
		}
			break;
		case OB_MBALL: /* ------- MetaBall ---------- */
		{
			MetaBall *mb= (MetaBall *)ob->data;
			
			type= ANIMTYPE_DSMBALL;
			expanded= FILTER_MBALL_OBJD(mb);
		}
			break;
		case OB_ARMATURE: /* ------- Armature ---------- */
		{
			bArmature *arm= (bArmature *)ob->data;
			
			type= ANIMTYPE_DSARM;
			expanded= FILTER_ARM_OBJD(arm);
		}
			break;
	}
	
	/* special exception for drivers instead of action */
	if (ads->filterflag & ADS_FILTER_ONLYDRIVERS)
		expanded= EXPANDED_DRVD(adt);
	
	/* include data-expand widget? */
	if ((filter_mode & ANIMFILTER_CURVESONLY) == 0) {	
		/* check if filtering by active status */
		if ANIMCHANNEL_ACTIVEOK(iat) {
			ale= make_new_animlistelem(iat, type, base, ANIMTYPE_OBJECT, (ID *)iat);
			if (ale) BLI_addtail(anim_data, ale);
		}
	}
	
	/* add object-data animation channels? */
	if (!(filter_mode & ANIMFILTER_VISIBLE) || (expanded) || (filter_mode & ANIMFILTER_CURVESONLY)) {
		/* filtering for channels - nla, drivers, keyframes */
		ANIMDATA_FILTER_CASES(iat, 
			{ /* AnimData blocks - do nothing... */ },
			items+= animdata_filter_nla(anim_data, ads, iat->adt, filter_mode, iat, type, (ID *)iat);,
			items+= animdata_filter_fcurves(anim_data, ads, adt->drivers.first, NULL, iat, type, filter_mode, (ID *)iat);, 
			items += animdata_filter_action(anim_data, ads, iat->adt->action, filter_mode, iat, type, (ID *)iat);)
	}
	
	/* return the number of items added to the list */
	return items;
}

static int animdata_filter_dopesheet_ob (ListBase *anim_data, bDopeSheet *ads, Base *base, int filter_mode)
{
	bAnimListElem *ale=NULL;
	AnimData *adt = NULL;
	Object *ob= base->object;
	Key *key= ob_get_key(ob);
	short obdata_ok = 0;
	int items = 0;
	
	/* add this object as a channel first */
	if ((filter_mode & (ANIMFILTER_CURVESONLY|ANIMFILTER_NLATRACKS)) == 0) {
		/* check if filtering by selection */
		if ANIMCHANNEL_SELOK((base->flag & SELECT)) {
			/* check if filtering by active status */
			if ANIMCHANNEL_ACTIVEOK(ob) {
				ale= make_new_animlistelem(base, ANIMTYPE_OBJECT, NULL, ANIMTYPE_NONE, (ID *)ob);
				if (ale) {
					BLI_addtail(anim_data, ale);
					items++;
				}
			}
		}
	}
	
	/* if collapsed, don't go any further (unless adding keyframes only) */
	if ( ((filter_mode & ANIMFILTER_VISIBLE) && EXPANDED_OBJC(ob) == 0) &&
		 !(filter_mode & (ANIMFILTER_CURVESONLY|ANIMFILTER_NLATRACKS)) )
		return items;
	
	/* Action, Drivers, or NLA */
	if (ob->adt) {
		adt= ob->adt;
		ANIMDATA_FILTER_CASES(ob,
			{ /* AnimData blocks - do nothing... */ },
			{ /* nla */
				/* add NLA tracks */
				items += animdata_filter_nla(anim_data, ads, adt, filter_mode, ob, ANIMTYPE_OBJECT, (ID *)ob);
			},
			{ /* drivers */
				/* include drivers-expand widget? */
				if ((filter_mode & ANIMFILTER_CHANNELS) && !(filter_mode & ANIMFILTER_CURVESONLY)) {
					ale= make_new_animlistelem(adt->action, ANIMTYPE_FILLDRIVERS, base, ANIMTYPE_OBJECT, (ID *)ob);
					if (ale) {
						BLI_addtail(anim_data, ale);
						items++;
					}
				}
				
				/* add F-Curve channels (drivers are F-Curves) */
				if (!(filter_mode & ANIMFILTER_VISIBLE) || EXPANDED_DRVD(adt) || !(filter_mode & ANIMFILTER_CHANNELS)) {
					// need to make the ownertype normal object here... (maybe type should be a separate one for clarity?)
					items += animdata_filter_fcurves(anim_data, ads, adt->drivers.first, NULL, ob, ANIMTYPE_OBJECT, filter_mode, (ID *)ob);
				}
			},
			{ /* action (keyframes) */
				/* include action-expand widget? */
				if ((filter_mode & ANIMFILTER_CHANNELS) && !(filter_mode & ANIMFILTER_CURVESONLY)) {
					ale= make_new_animlistelem(adt->action, ANIMTYPE_FILLACTD, base, ANIMTYPE_OBJECT, (ID *)ob);
					if (ale) {
						BLI_addtail(anim_data, ale);
						items++;
					}
				}
				
				/* add F-Curve channels? */
				if (!(filter_mode & ANIMFILTER_VISIBLE) || EXPANDED_ACTC(adt->action) || !(filter_mode & ANIMFILTER_CHANNELS)) {
					// need to make the ownertype normal object here... (maybe type should be a separate one for clarity?)
					items += animdata_filter_action(anim_data, ads, adt->action, filter_mode, ob, ANIMTYPE_OBJECT, (ID *)ob); 
				}
			}
		);
	}
	
	
	/* ShapeKeys? */
	if ((key) && !(ads->filterflag & ADS_FILTER_NOSHAPEKEYS)) {
		adt= key->adt;
		ANIMDATA_FILTER_CASES(key,
			{ /* AnimData blocks - do nothing... */ },
			{ /* nla */
				/* include shapekey-expand widget? */
				if ((filter_mode & ANIMFILTER_CHANNELS) && !(filter_mode & ANIMFILTER_CURVESONLY)) {
					/* check if filtering by active status */
					if ANIMCHANNEL_ACTIVEOK(key) {
						ale= make_new_animlistelem(key, ANIMTYPE_DSSKEY, base, ANIMTYPE_OBJECT, (ID *)ob);
						if (ale) {
							BLI_addtail(anim_data, ale);
							items++;
						}
					}
				}
				
				/* add NLA tracks - only if expanded or so */
				if (!(filter_mode & ANIMFILTER_VISIBLE) || FILTER_SKE_OBJD(key) || (filter_mode & ANIMFILTER_CURVESONLY))
					items += animdata_filter_nla(anim_data, ads, adt, filter_mode, ob, ANIMTYPE_OBJECT, (ID *)ob);
			},
			{ /* drivers */
				/* include shapekey-expand widget? */
				if ((filter_mode & ANIMFILTER_CHANNELS) && !(filter_mode & ANIMFILTER_CURVESONLY)) {
					ale= make_new_animlistelem(key, ANIMTYPE_DSSKEY, base, ANIMTYPE_OBJECT, (ID *)ob);
					if (ale) {
						BLI_addtail(anim_data, ale);
						items++;
					}
				}
				
				/* add channels */
				if (!(filter_mode & ANIMFILTER_VISIBLE) || FILTER_SKE_OBJD(key) || (filter_mode & ANIMFILTER_CURVESONLY)) {
					items += animdata_filter_fcurves(anim_data, ads, adt->drivers.first, NULL, key, ANIMTYPE_DSSKEY, filter_mode, (ID *)key);
				}
			},
			{ /* action (keyframes) */
				/* include shapekey-expand widget? */
				if ((filter_mode & ANIMFILTER_CHANNELS) && !(filter_mode & ANIMFILTER_CURVESONLY)) {
					/* check if filtering by active status */
					if ANIMCHANNEL_ACTIVEOK(key) {
						ale= make_new_animlistelem(key, ANIMTYPE_DSSKEY, base, ANIMTYPE_OBJECT, (ID *)ob);
						if (ale) {
							BLI_addtail(anim_data, ale);
							items++;
						}
					}
				}
				
				/* add channels */
				if (!(filter_mode & ANIMFILTER_VISIBLE) || FILTER_SKE_OBJD(key) || (filter_mode & ANIMFILTER_CURVESONLY)) {
					items += animdata_filter_action(anim_data, ads, adt->action, filter_mode, key, ANIMTYPE_DSSKEY, (ID *)key); 
				}
			}
		);
	}

	/* Materials? */
	if ((ob->totcol) && !(ads->filterflag & ADS_FILTER_NOMAT))
		items += animdata_filter_dopesheet_mats(anim_data, ads, base, filter_mode);
	
	/* Object Data */
	switch (ob->type) {
		case OB_CAMERA: /* ------- Camera ------------ */
		{
			Camera *ca= (Camera *)ob->data;
			
			if ((ads->filterflag & ADS_FILTER_NOCAM) == 0) {
				ANIMDATA_FILTER_CASES(ca,
					{ /* AnimData blocks - do nothing... */ },
					obdata_ok= 1;,
					obdata_ok= 1;,
					obdata_ok= 1;)
			}
		}
			break;
		case OB_LAMP: /* ---------- Lamp ----------- */
		{
			Lamp *la= (Lamp *)ob->data;
			
			if ((ads->filterflag & ADS_FILTER_NOLAM) == 0) {
				ANIMDATA_FILTER_CASES(la,
					{ /* AnimData blocks - do nothing... */ },
					obdata_ok= 1;,
					obdata_ok= 1;,
					obdata_ok= 1;)
			}
		}
			break;
		case OB_CURVE: /* ------- Curve ---------- */
		{
			Curve *cu= (Curve *)ob->data;
			
			if ((ads->filterflag & ADS_FILTER_NOCUR) == 0) {
				ANIMDATA_FILTER_CASES(cu,
					{ /* AnimData blocks - do nothing... */ },
					obdata_ok= 1;,
					obdata_ok= 1;,
					obdata_ok= 1;)
			}
		}
			break;
		case OB_MBALL: /* ------- MetaBall ---------- */
		{
			MetaBall *mb= (MetaBall *)ob->data;
			
			if ((ads->filterflag & ADS_FILTER_NOMBA) == 0) {
				ANIMDATA_FILTER_CASES(mb,
					{ /* AnimData blocks - do nothing... */ },
					obdata_ok= 1;,
					obdata_ok= 1;,
					obdata_ok= 1;)
			}
		}
			break;
		case OB_ARMATURE: /* ------- Armature ---------- */
		{
			bArmature *arm= (bArmature *)ob->data;
			
			if ((ads->filterflag & ADS_FILTER_NOARM) == 0) {
				ANIMDATA_FILTER_CASES(arm,
					{ /* AnimData blocks - do nothing... */ },
					obdata_ok= 1;,
					obdata_ok= 1;,
					obdata_ok= 1;)
			}
		}
			break;
	}
	if (obdata_ok) 
		items += animdata_filter_dopesheet_obdata(anim_data, ads, base, filter_mode);

	/* particles */
	if (ob->particlesystem.first && !(ads->filterflag & ADS_FILTER_NOPART))
		items += animdata_filter_dopesheet_particles(anim_data, ads, base, filter_mode);
	
	/* return the number of items added to the list */
	return items;
}	

static int animdata_filter_dopesheet_scene (ListBase *anim_data, bDopeSheet *ads, Scene *sce, int filter_mode)
{
	World *wo= sce->world;
	bNodeTree *ntree= sce->nodetree;
	AnimData *adt= NULL;
	bAnimListElem *ale;
	int items = 0;
	
	/* add scene as a channel first (even if we aren't showing scenes we still need to show the scene's sub-data */
	if ((filter_mode & (ANIMFILTER_CURVESONLY|ANIMFILTER_NLATRACKS)) == 0) {
		/* check if filtering by selection */
		if (ANIMCHANNEL_SELOK( (sce->flag & SCE_DS_SELECTED) )) {
			ale= make_new_animlistelem(sce, ANIMTYPE_SCENE, NULL, ANIMTYPE_NONE, NULL);
			if (ale) {
				BLI_addtail(anim_data, ale);
				items++;
			}
		}
	}
	
	/* if collapsed, don't go any further (unless adding keyframes only) */
	if ( (EXPANDED_SCEC(sce) == 0) && !(filter_mode & (ANIMFILTER_CURVESONLY|ANIMFILTER_NLATRACKS)) )
		return items;
		
	/* Action, Drivers, or NLA for Scene */
	if ((ads->filterflag & ADS_FILTER_NOSCE) == 0) {
		adt= sce->adt;
		ANIMDATA_FILTER_CASES(sce,
			{ /* AnimData blocks - do nothing... */ },
			{ /* nla */
				/* add NLA tracks */
				items += animdata_filter_nla(anim_data, ads, adt, filter_mode, sce, ANIMTYPE_SCENE, (ID *)sce);
			},
			{ /* drivers */
				/* include drivers-expand widget? */
				if ((filter_mode & ANIMFILTER_CHANNELS) && !(filter_mode & ANIMFILTER_CURVESONLY)) {
					ale= make_new_animlistelem(adt->action, ANIMTYPE_FILLDRIVERS, sce, ANIMTYPE_SCENE, (ID *)sce);
					if (ale) {
						BLI_addtail(anim_data, ale);
						items++;
					}
				}
				
				/* add F-Curve channels (drivers are F-Curves) */
				if (EXPANDED_DRVD(adt) || !(filter_mode & ANIMFILTER_CHANNELS)) {
					items += animdata_filter_fcurves(anim_data, ads, adt->drivers.first, NULL, sce, ANIMTYPE_SCENE, filter_mode, (ID *)sce);
				}
			},
			{ /* action */
				/* include action-expand widget? */
				if ((filter_mode & ANIMFILTER_CHANNELS) && !(filter_mode & ANIMFILTER_CURVESONLY)) {
					ale= make_new_animlistelem(adt->action, ANIMTYPE_FILLACTD, sce, ANIMTYPE_SCENE, (ID *)sce);
					if (ale) {
						BLI_addtail(anim_data, ale);
						items++;
					}
				}
				
				/* add F-Curve channels? */
				if (EXPANDED_ACTC(adt->action) || !(filter_mode & ANIMFILTER_CHANNELS)) {
					items += animdata_filter_action(anim_data, ads, adt->action, filter_mode, sce, ANIMTYPE_SCENE, (ID *)sce); 
				}
			}
		)
	}
	
	/* world */
	if ((wo && wo->adt) && !(ads->filterflag & ADS_FILTER_NOWOR)) {
		/* Action, Drivers, or NLA for World */
		adt= wo->adt;
		ANIMDATA_FILTER_CASES(wo,
			{ /* AnimData blocks - do nothing... */ },
			{ /* nla */
				/* add NLA tracks */
				items += animdata_filter_nla(anim_data, ads, adt, filter_mode, wo, ANIMTYPE_DSWOR, (ID *)wo);
			},
			{ /* drivers */
				/* include world-expand widget? */
				if ((filter_mode & ANIMFILTER_CHANNELS) && !(filter_mode & ANIMFILTER_CURVESONLY)) {
					ale= make_new_animlistelem(wo, ANIMTYPE_DSWOR, sce, ANIMTYPE_SCENE, (ID *)wo);
					if (ale) {
						BLI_addtail(anim_data, ale);
						items++;
					}
				}
				
				/* add F-Curve channels (drivers are F-Curves) */
				if (FILTER_WOR_SCED(wo)/*EXPANDED_DRVD(adt)*/ || !(filter_mode & ANIMFILTER_CHANNELS)) {
					// XXX owner info is messed up now...
					items += animdata_filter_fcurves(anim_data, ads, adt->drivers.first, NULL, wo, ANIMTYPE_DSWOR, filter_mode, (ID *)wo);
				}
			},
			{ /* action */
				/* include world-expand widget? */
				if ((filter_mode & ANIMFILTER_CHANNELS) && !(filter_mode & ANIMFILTER_CURVESONLY)) {
					ale= make_new_animlistelem(wo, ANIMTYPE_DSWOR, sce, ANIMTYPE_SCENE, (ID *)sce);
					if (ale) {
						BLI_addtail(anim_data, ale);
						items++;
					}
				}
				
				/* add channels */
				if (FILTER_WOR_SCED(wo) || (filter_mode & ANIMFILTER_CURVESONLY)) {
					items += animdata_filter_action(anim_data, ads, adt->action, filter_mode, wo, ANIMTYPE_DSWOR, (ID *)wo); 
				}
			}
		)
	}
	/* nodetree */
	if ((ntree && ntree->adt) && !(ads->filterflag & ADS_FILTER_NONTREE)) {
		/* Action, Drivers, or NLA for Nodetree */
		adt= ntree->adt;
		ANIMDATA_FILTER_CASES(ntree,
			{ /* AnimData blocks - do nothing... */ },
			{ /* nla */
				/* add NLA tracks */
				items += animdata_filter_nla(anim_data, ads, adt, filter_mode, ntree, ANIMTYPE_DSNTREE, (ID *)ntree);
			},
			{ /* drivers */
				/* include nodetree-expand widget? */
				if ((filter_mode & ANIMFILTER_CHANNELS) && !(filter_mode & ANIMFILTER_CURVESONLY)) {
					ale= make_new_animlistelem(ntree, ANIMTYPE_DSNTREE, sce, ANIMTYPE_SCENE, (ID *)ntree);
					if (ale) {
						BLI_addtail(anim_data, ale);
						items++;
					}
				}
				
				/* add F-Curve channels (drivers are F-Curves) */
				if (FILTER_NTREE_SCED(ntree)/*EXPANDED_DRVD(adt)*/ || !(filter_mode & ANIMFILTER_CHANNELS)) {
					// XXX owner info is messed up now...
					items += animdata_filter_fcurves(anim_data, ads, adt->drivers.first, NULL, ntree, ANIMTYPE_DSNTREE, filter_mode, (ID *)ntree);
				}
			},
			{ /* action */
				/* include nodetree-expand widget? */
				if ((filter_mode & ANIMFILTER_CHANNELS) && !(filter_mode & ANIMFILTER_CURVESONLY)) {
					ale= make_new_animlistelem(ntree, ANIMTYPE_DSNTREE, sce, ANIMTYPE_SCENE, (ID *)sce);
					if (ale) {
						BLI_addtail(anim_data, ale);
						items++;
					}
				}
				
				/* add channels */
				if (FILTER_NTREE_SCED(ntree) || (filter_mode & ANIMFILTER_CURVESONLY)) {
					items += animdata_filter_action(anim_data, ads, adt->action, filter_mode, ntree, ANIMTYPE_DSNTREE, (ID *)ntree); 
				}
			}
		)
	}

	
	// TODO: scene compositing nodes (these aren't standard node-trees)
	
	/* return the number of items added to the list */
	return items;
}

// TODO: implement pinning... (if and when pinning is done, what we need to do is to provide freeing mechanisms - to protect against data that was deleted)
static int animdata_filter_dopesheet (ListBase *anim_data, bAnimContext *ac, bDopeSheet *ads, int filter_mode)
{
	Scene *sce= (Scene *)ads->source;
	Base *base;
	bAnimListElem *ale;
	int items = 0;
	
	/* check that we do indeed have a scene */
	if ((ads->source == NULL) || (GS(ads->source->name)!=ID_SCE)) {
		printf("DopeSheet Error: Not scene!\n");
		if (G.f & G_DEBUG)
			printf("\tPointer = %p, Name = '%s' \n", ads->source, (ads->source)?ads->source->name:NULL);
		return 0;
	}
	
	/* scene-linked animation */
	// TODO: sequencer, composite nodes - are we to include those here too?
	{
		short sceOk= 0, worOk= 0, nodeOk=0;
		
		/* check filtering-flags if ok */
		ANIMDATA_FILTER_CASES(sce, 
			{
				/* for the special AnimData blocks only case, we only need to add
				 * the block if it is valid... then other cases just get skipped (hence ok=0)
				 */
				ANIMDATA_ADD_ANIMDATA(sce);
				sceOk=0;
			},
			sceOk= !(ads->filterflag & ADS_FILTER_NOSCE);, 
			sceOk= !(ads->filterflag & ADS_FILTER_NOSCE);, 
			sceOk= !(ads->filterflag & ADS_FILTER_NOSCE);)
		if (sce->world) {
			ANIMDATA_FILTER_CASES(sce->world, 
				{
					/* for the special AnimData blocks only case, we only need to add
					 * the block if it is valid... then other cases just get skipped (hence ok=0)
					 */
					ANIMDATA_ADD_ANIMDATA(sce->world);
					worOk=0;
				},
				worOk= !(ads->filterflag & ADS_FILTER_NOWOR);, 
				worOk= !(ads->filterflag & ADS_FILTER_NOWOR);, 
				worOk= !(ads->filterflag & ADS_FILTER_NOWOR);)
		}
		if (sce->nodetree) {
			ANIMDATA_FILTER_CASES(sce->nodetree, 
				{
					/* for the special AnimData blocks only case, we only need to add
					 * the block if it is valid... then other cases just get skipped (hence ok=0)
					 */
					ANIMDATA_ADD_ANIMDATA(sce->nodetree);
					nodeOk=0;
				},
				nodeOk= !(ads->filterflag & ADS_FILTER_NONTREE);, 
				nodeOk= !(ads->filterflag & ADS_FILTER_NONTREE);, 
				nodeOk= !(ads->filterflag & ADS_FILTER_NONTREE);)
		}
		
		/* if only F-Curves with visible flags set can be shown, check that 
		 * datablocks haven't been set to invisible 
		 */
		if (filter_mode & ANIMFILTER_CURVEVISIBLE) {
			if ((sce->adt) && (sce->adt->flag & ADT_CURVES_NOT_VISIBLE))
				sceOk= worOk= nodeOk= 0;
		}
		
		/* check if not all bad (i.e. so there is something to show) */
		if ( !(!sceOk && !worOk && !nodeOk) ) {
			/* add scene data to the list of filtered channels */
			items += animdata_filter_dopesheet_scene(anim_data, ads, sce, filter_mode);
		}
	}
	
	
	/* loop over all bases in the scene */
	for (base= sce->base.first; base; base= base->next) {
		/* check if there's an object (all the relevant checks are done in the ob-function) */
		if (base->object) {
			Object *ob= base->object;
			Key *key= ob_get_key(ob);
			short actOk=1, keyOk=1, dataOk=1, matOk=1, partOk=1;
			
			/* firstly, check if object can be included, by the following factors:
			 *	- if only visible, must check for layer and also viewport visibility
			 *	- if only selected, must check if object is selected 
			 *	- there must be animation data to edit
			 */
			// TODO: if cache is implemented, just check name here, and then 
			if (filter_mode & ANIMFILTER_VISIBLE) {
				/* layer visibility - we check both object and base, since these may not be in sync yet */
				if ((sce->lay & (ob->lay|base->lay))==0) continue;
				
				/* outliner restrict-flag */
				if (ob->restrictflag & OB_RESTRICT_VIEW) continue;
			}
			
			/* if only F-Curves with visible flags set can be shown, check that 
			 * datablock hasn't been set to invisible 
			 */
			if (filter_mode & ANIMFILTER_CURVEVISIBLE) {
				if ((ob->adt) && (ob->adt->flag & ADT_CURVES_NOT_VISIBLE))
					continue;
			}
			
			/* additionally, dopesheet filtering also affects what objects to consider */
			if (ads->filterflag) {
				/* check selection and object type filters */
				if ( (ads->filterflag & ADS_FILTER_ONLYSEL) && !((base->flag & SELECT) /*|| (base == sce->basact)*/) )  {
					/* only selected should be shown */
					continue;
				}
				
				/* check filters for datatypes */
					/* object */
				actOk= 0;
				keyOk= 0;
				ANIMDATA_FILTER_CASES(ob, 
					{
						/* for the special AnimData blocks only case, we only need to add
						 * the block if it is valid... then other cases just get skipped (hence ok=0)
						 */
						ANIMDATA_ADD_ANIMDATA(ob);
						actOk=0;
					},
					actOk= 1;, 
					actOk= 1;, 
					actOk= 1;)
				if (key) {
					/* shapekeys */
					ANIMDATA_FILTER_CASES(key, 
						{
							/* for the special AnimData blocks only case, we only need to add
							 * the block if it is valid... then other cases just get skipped (hence ok=0)
							 */
							ANIMDATA_ADD_ANIMDATA(key);
							keyOk=0;
						},
						keyOk= 1;, 
						keyOk= 1;, 
						keyOk= 1;)
				}
				
				/* materials - only for geometric types */
				matOk= 0; /* by default, not ok... */
				if ( !(ads->filterflag & ADS_FILTER_NOMAT) && (ob->totcol) && 
					 ELEM5(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_MBALL) ) 
				{
					int a;
					
					/* firstly check that we actuallly have some materials */
					for (a=0; a < ob->totcol; a++) {
						Material *ma= give_current_material(ob, a);
						
						if (ma) {
							/* if material has relevant animation data, break */
							ANIMDATA_FILTER_CASES(ma, 
								{
									/* for the special AnimData blocks only case, we only need to add
									 * the block if it is valid... then other cases just get skipped (hence ok=0)
									 */
									ANIMDATA_ADD_ANIMDATA(ma);
									matOk=0;
								},
								matOk= 1;, 
								matOk= 1;, 
								matOk= 1;)
						}
							
						if (matOk) 
							break;
					}
				}
				
				/* data */
				switch (ob->type) {
					case OB_CAMERA: /* ------- Camera ------------ */
					{
						Camera *ca= (Camera *)ob->data;
						dataOk= 0;
						ANIMDATA_FILTER_CASES(ca, 
							if ((ads->filterflag & ADS_FILTER_NOCAM)==0) {
								/* for the special AnimData blocks only case, we only need to add
								 * the block if it is valid... then other cases just get skipped (hence ok=0)
								 */
								ANIMDATA_ADD_ANIMDATA(ca);
								dataOk=0;
							},
							dataOk= !(ads->filterflag & ADS_FILTER_NOCAM);, 
							dataOk= !(ads->filterflag & ADS_FILTER_NOCAM);, 
							dataOk= !(ads->filterflag & ADS_FILTER_NOCAM);)
					}
						break;
					case OB_LAMP: /* ---------- Lamp ----------- */
					{
						Lamp *la= (Lamp *)ob->data;
						dataOk= 0;
						ANIMDATA_FILTER_CASES(la, 
							if ((ads->filterflag & ADS_FILTER_NOLAM)==0) {
								/* for the special AnimData blocks only case, we only need to add
								 * the block if it is valid... then other cases just get skipped (hence ok=0)
								 */
								ANIMDATA_ADD_ANIMDATA(la);
								dataOk=0;
							},
							dataOk= !(ads->filterflag & ADS_FILTER_NOLAM);, 
							dataOk= !(ads->filterflag & ADS_FILTER_NOLAM);, 
							dataOk= !(ads->filterflag & ADS_FILTER_NOLAM);)
					}
						break;
					case OB_CURVE: /* ------- Curve ---------- */
					{
						Curve *cu= (Curve *)ob->data;
						dataOk= 0;
						ANIMDATA_FILTER_CASES(cu, 
							if ((ads->filterflag & ADS_FILTER_NOCUR)==0) {
								/* for the special AnimData blocks only case, we only need to add
								 * the block if it is valid... then other cases just get skipped (hence ok=0)
								 */
								ANIMDATA_ADD_ANIMDATA(cu);
								dataOk=0;
							},
							dataOk= !(ads->filterflag & ADS_FILTER_NOCUR);, 
							dataOk= !(ads->filterflag & ADS_FILTER_NOCUR);, 
							dataOk= !(ads->filterflag & ADS_FILTER_NOCUR);)
					}
						break;
					case OB_MBALL: /* ------- MetaBall ---------- */
					{
						MetaBall *mb= (MetaBall *)ob->data;
						dataOk= 0;
						ANIMDATA_FILTER_CASES(mb, 
							if ((ads->filterflag & ADS_FILTER_NOMBA)==0) {
								/* for the special AnimData blocks only case, we only need to add
								 * the block if it is valid... then other cases just get skipped (hence ok=0)
								 */
								ANIMDATA_ADD_ANIMDATA(mb);
								dataOk=0;
							},
							dataOk= !(ads->filterflag & ADS_FILTER_NOMBA);, 
							dataOk= !(ads->filterflag & ADS_FILTER_NOMBA);, 
							dataOk= !(ads->filterflag & ADS_FILTER_NOMBA);)
					}
						break;
					case OB_ARMATURE: /* ------- Armature ---------- */
					{
						bArmature *arm= (bArmature *)ob->data;
						dataOk= 0;
						ANIMDATA_FILTER_CASES(arm, 
							if ((ads->filterflag & ADS_FILTER_NOARM)==0) {
								/* for the special AnimData blocks only case, we only need to add
								 * the block if it is valid... then other cases just get skipped (hence ok=0)
								 */
								ANIMDATA_ADD_ANIMDATA(arm);
								dataOk=0;
							},
							dataOk= !(ads->filterflag & ADS_FILTER_NOARM);, 
							dataOk= !(ads->filterflag & ADS_FILTER_NOARM);, 
							dataOk= !(ads->filterflag & ADS_FILTER_NOARM);)
					}
						break;
					default: /* --- other --- */
						dataOk= 0;
						break;
				}
				
				/* particles */
				partOk = 0;
				if (!(ads->filterflag & ADS_FILTER_NOPART) && ob->particlesystem.first) {
					ParticleSystem *psys = ob->particlesystem.first;
					for(; psys; psys=psys->next) {
						if (psys->part) {
							/* if particlesettings has relevant animation data, break */
							ANIMDATA_FILTER_CASES(psys->part, 
								{
									/* for the special AnimData blocks only case, we only need to add
									 * the block if it is valid... then other cases just get skipped (hence ok=0)
									 */
									ANIMDATA_ADD_ANIMDATA(psys->part);
									partOk=0;
								},
								partOk= 1;, 
								partOk= 1;, 
								partOk= 1;)
						}
							
						if (partOk) 
							break;
					}
				}
				
				/* check if all bad (i.e. nothing to show) */
				if (!actOk && !keyOk && !dataOk && !matOk && !partOk)
					continue;
			}
			else {
				/* check data-types */
				actOk= ANIMDATA_HAS_KEYS(ob);
				keyOk= (key != NULL);
				
				/* materials - only for geometric types */
				matOk= 0; /* by default, not ok... */
				if (ELEM5(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_MBALL) && (ob->totcol)) 
				{
					int a;
					
					/* firstly check that we actuallly have some materials */
					for (a=0; a < ob->totcol; a++) {
						Material *ma= give_current_material(ob, a);
						
						if ((ma) && ANIMDATA_HAS_KEYS(ma)) {
							matOk= 1;
							break;
						}
					}
				}
				
				/* data */
				switch (ob->type) {
					case OB_CAMERA: /* ------- Camera ------------ */
					{
						Camera *ca= (Camera *)ob->data;
						dataOk= ANIMDATA_HAS_KEYS(ca);						
					}
						break;
					case OB_LAMP: /* ---------- Lamp ----------- */
					{
						Lamp *la= (Lamp *)ob->data;
						dataOk= ANIMDATA_HAS_KEYS(la);	
					}
						break;
					case OB_CURVE: /* -------- Curve ---------- */
					{
						Curve *cu= (Curve *)ob->data;
						dataOk= ANIMDATA_HAS_KEYS(cu);	
					}
						break;
					case OB_MBALL: /* -------- Metas ---------- */
					{
						MetaBall *mb= (MetaBall *)ob->data;
						dataOk= ANIMDATA_HAS_KEYS(mb);	
					}
						break;
					case OB_ARMATURE: /* -------- Armature ---------- */
					{
						bArmature *arm= (bArmature *)ob->data;
						dataOk= ANIMDATA_HAS_KEYS(arm);	
					}
						break;
					default: /* --- other --- */
						dataOk= 0;
						break;
				}
				
				/* particles */
				partOk = 0;
				if (ob->particlesystem.first) {
					ParticleSystem *psys = ob->particlesystem.first;
					for(; psys; psys=psys->next) {
						if(psys->part && ANIMDATA_HAS_KEYS(psys->part)) {
							partOk = 1;
							break;
						}
					}
				}
				
				/* check if all bad (i.e. nothing to show) */
				if (!actOk && !keyOk && !dataOk && !matOk && !partOk)
					continue;
			}
			
			/* since we're still here, this object should be usable */
			items += animdata_filter_dopesheet_ob(anim_data, ads, base, filter_mode);
		}
	}
	
	/* return the number of items in the list */
	return items;
}

/* Summary track for DopeSheet/Action Editor 
 * 	- return code is whether the summary lets the other channels get drawn
 */
static short animdata_filter_dopesheet_summary (bAnimContext *ac, ListBase *anim_data, int filter_mode, int *items)
{
	bDopeSheet *ads = NULL;
	
	/* get the DopeSheet information to use 
	 *	- we should only need to deal with the DopeSheet/Action Editor, 
	 *	  since all the other Animation Editors won't have this concept
	 *	  being applicable.
	 */
	if ((ac && ac->sa) && (ac->sa->spacetype == SPACE_ACTION)) {
		SpaceAction *saction= (SpaceAction *)ac->sa->spacedata.first;
		ads= &saction->ads;
	}
	else {
		/* invalid space type - skip this summary channels */
		return 1;
	}
	
	/* dopesheet summary 
	 *	- only for drawing and/or selecting keyframes in channels, but not for real editing 
	 *	- only useful for DopeSheet Editor, where the summary is useful
	 */
	// TODO: we should really check if some other prohibited filters are also active, but that can be for later
	if ((filter_mode & ANIMFILTER_CHANNELS) && (ads->filterflag & ADS_FILTER_SUMMARY)) {
		bAnimListElem *ale= make_new_animlistelem(ac, ANIMTYPE_SUMMARY, NULL, ANIMTYPE_NONE, NULL);
		if (ale) {
			BLI_addtail(anim_data, ale);
			(*items)++;
		}
		
		/* if summary is collapsed, don't show other channels beneath this 
		 *	- this check is put inside the summary check so that it doesn't interfere with normal operation
		 */ 
		if (ads->flag & ADS_FLAG_SUMMARY_COLLAPSED)
			return 0;
	}
	
	/* the other channels beneath this can be shown */
	return 1;
}  

/* ----------- Public API --------------- */

/* This function filters the active data source to leave only animation channels suitable for
 * usage by the caller. It will return the length of the list 
 * 
 * 	*anim_data: is a pointer to a ListBase, to which the filtered animation channels
 *		will be placed for use.
 *	filter_mode: how should the data be filtered - bitmapping accessed flags
 */
int ANIM_animdata_filter (bAnimContext *ac, ListBase *anim_data, int filter_mode, void *data, short datatype)
{
	int items = 0;
	
	/* only filter data if there's somewhere to put it */
	if (data && anim_data) {
		bAnimListElem *ale, *next;
		Object *obact= (ac) ? ac->obact : NULL;
		
		/* firstly filter the data */
		switch (datatype) {
			case ANIMCONT_ACTION:	/* 'Action Editor' */
			{
				/* the check for the DopeSheet summary is included here since the summary works here too */
				if (animdata_filter_dopesheet_summary(ac, anim_data, filter_mode, &items))
					items += animdata_filter_action(anim_data, NULL, data, filter_mode, NULL, ANIMTYPE_NONE, (ID *)obact);
			}
				break;
				
			case ANIMCONT_SHAPEKEY: /* 'ShapeKey Editor' */
			{
				/* the check for the DopeSheet summary is included here since the summary works here too */
				if (animdata_filter_dopesheet_summary(ac, anim_data, filter_mode, &items))
					items= animdata_filter_shapekey(anim_data, data, filter_mode);
			}
				break;
				
			case ANIMCONT_GPENCIL:
			{
				//items= animdata_filter_gpencil(anim_data, data, filter_mode);
			}
				break;
				
			case ANIMCONT_DOPESHEET: /* 'DopeSheet Editor' */
			{
				/* the DopeSheet editor is the primary place where the DopeSheet summaries are useful */
				if (animdata_filter_dopesheet_summary(ac, anim_data, filter_mode, &items))
					items += animdata_filter_dopesheet(anim_data, ac, data, filter_mode);
			}
				break;
				
			case ANIMCONT_FCURVES: /* Graph Editor -> FCurves/Animation Editing */
			case ANIMCONT_DRIVERS: /* Graph Editor -> Drivers Editing */
			case ANIMCONT_NLA: /* NLA Editor */
			{
				/* all of these editors use the basic DopeSheet data for filtering options, but don't have all the same features */
				items = animdata_filter_dopesheet(anim_data, ac, data, filter_mode);
			}
				break;
		}
			
		/* remove any weedy entries */
		// XXX this is weedy code!
		for (ale= anim_data->first; ale; ale= next) {
			next= ale->next;
			
			if (ale->type == ANIMTYPE_NONE) {
				items--;
				BLI_freelinkN(anim_data, ale);
			}
		}
	}
	
	/* return the number of items in the list */
	return items;
}

/* ************************************************************ */
