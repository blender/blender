/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joshua Leung (full recode)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/anim_sys.c
 *  \ingroup bke
 */


#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <float.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_dynstr.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_depsgraph.h"
#include "BKE_fcurve.h"
#include "BKE_nla.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_library.h"
#include "BKE_report.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"

#include "nla_private.h"

/* ***************************************** */
/* AnimData API */

/* Getter/Setter -------------------------------------------- */

/* Check if ID can have AnimData */
short id_type_can_have_animdata (ID *id)
{
	/* sanity check */
	if (id == NULL)
		return 0;
		
	/* Only some ID-blocks have this info for now */
	// TODO: finish adding this for the other blocktypes
	switch (GS(id->name)) {
			/* has AnimData */
		case ID_OB:
		case ID_ME: case ID_MB: case ID_CU: case ID_AR: case ID_LT:
		case ID_KE:
		case ID_PA:
		case ID_MA: case ID_TE: case ID_NT:
		case ID_LA: case ID_CA: case ID_WO:
		case ID_SPK:
		case ID_SCE:
		case ID_MC:
		{
			return 1;
		}
		
			/* no AnimData */
		default:
			return 0;
	}
}


/* Get AnimData from the given ID-block. In order for this to work, we assume that 
 * the AnimData pointer is stored immediately after the given ID-block in the struct,
 * as per IdAdtTemplate.
 */
AnimData *BKE_animdata_from_id (ID *id)
{
	/* only some ID-blocks have this info for now, so we cast the 
	 * types that do to be of type IdAdtTemplate, and extract the
	 * AnimData that way
	 */
	if (id_type_can_have_animdata(id)) {
		IdAdtTemplate *iat= (IdAdtTemplate *)id;
		return iat->adt;
	}
	else
		return NULL;
}

/* Add AnimData to the given ID-block. In order for this to work, we assume that 
 * the AnimData pointer is stored immediately after the given ID-block in the struct,
 * as per IdAdtTemplate. Also note that 
 */
AnimData *BKE_id_add_animdata (ID *id)
{
	/* Only some ID-blocks have this info for now, so we cast the 
	 * types that do to be of type IdAdtTemplate, and add the AnimData
	 * to it using the template
	 */
	if (id_type_can_have_animdata(id)) {
		IdAdtTemplate *iat= (IdAdtTemplate *)id;
		
		/* check if there's already AnimData, in which case, don't add */
		if (iat->adt == NULL) {
			AnimData *adt;
			
			/* add animdata */
			adt= iat->adt= MEM_callocN(sizeof(AnimData), "AnimData");
			
			/* set default settings */
			adt->act_influence= 1.0f;
		}
		
		return iat->adt;
	}
	else 
		return NULL;
}

/* Action Setter --------------------------------------- */

/* Called when user tries to change the active action of an AnimData block (via RNA, Outliner, etc.) */
short BKE_animdata_set_action (ReportList *reports, ID *id, bAction *act)
{
	AnimData *adt = BKE_animdata_from_id(id);
	short ok = 0;
	
	/* animdata validity check */
	if (adt == NULL) {
		BKE_report(reports, RPT_WARNING, "No AnimData to set action on");
		return ok;
	}
	
	/* active action is only editable when it is not a tweaking strip 
	 * see rna_AnimData_action_editable() in rna_animation.c
	 */
	if ((adt->flag & ADT_NLA_EDIT_ON) || (adt->actstrip) || (adt->tmpact)) {
		/* cannot remove, otherwise things turn to custard */
		BKE_report(reports, RPT_ERROR, "Cannot change action, as it is still being edited in NLA");
		return ok;
	}
	
	/* manage usercount for current action */
	if (adt->action)
		id_us_min((ID*)adt->action);
	
	/* assume that AnimData's action can in fact be edited... */
	if (act) {
		/* action must have same type as owner */
		if (ELEM(act->idroot, 0, GS(id->name))) {
			/* can set */
			adt->action = act;
			id_us_plus((ID*)adt->action);
			ok = 1;
		}
		else {
			/* cannot set */
			BKE_reportf(reports, RPT_ERROR,
					"Couldn't set Action '%s' onto ID '%s', as it doesn't have suitably rooted paths for this purpose", 
					act->id.name+2, id->name);
			//ok = 0;
		}
	}
	else {
		/* just clearing the action... */
		adt->action = NULL;
		ok = 1;
	}
	
	return ok;
}

/* Freeing -------------------------------------------- */

/* Free AnimData used by the nominated ID-block, and clear ID-block's AnimData pointer */
void BKE_free_animdata (ID *id)
{
	/* Only some ID-blocks have this info for now, so we cast the 
	 * types that do to be of type IdAdtTemplate
	 */
	if (id_type_can_have_animdata(id)) {
		IdAdtTemplate *iat= (IdAdtTemplate *)id;
		AnimData *adt= iat->adt;
		
		/* check if there's any AnimData to start with */
		if (adt) {
			/* unlink action (don't free, as it's in its own list) */
			if (adt->action)
				adt->action->id.us--;
			/* same goes for the temporarily displaced action */
			if (adt->tmpact)
				adt->tmpact->id.us--;
				
			/* free nla data */
			free_nladata(&adt->nla_tracks);
			
			/* free drivers - stored as a list of F-Curves */
			free_fcurves(&adt->drivers);
			
			/* free overrides */
			// TODO...
			
			/* free animdata now */
			MEM_freeN(adt);
			iat->adt= NULL;
		}
	}
}

/* Freeing -------------------------------------------- */

/* Make a copy of the given AnimData - to be used when copying datablocks */
AnimData *BKE_copy_animdata (AnimData *adt, const short do_action)
{
	AnimData *dadt;
	
	/* sanity check before duplicating struct */
	if (adt == NULL)
		return NULL;
	dadt= MEM_dupallocN(adt);
	
	/* make a copy of action - at worst, user has to delete copies... */
	if (do_action) {
		dadt->action= copy_action(adt->action);
		dadt->tmpact= copy_action(adt->tmpact);
	}
	else {
		id_us_plus((ID *)dadt->action);
		id_us_plus((ID *)dadt->tmpact);
	}

	/* duplicate NLA data */
	copy_nladata(&dadt->nla_tracks, &adt->nla_tracks);
	
	/* duplicate drivers (F-Curves) */
	copy_fcurves(&dadt->drivers, &adt->drivers);
	
	/* don't copy overrides */
	dadt->overrides.first= dadt->overrides.last= NULL;
	
	/* return */
	return dadt;
}

int BKE_copy_animdata_id (ID *id_to, ID *id_from, const short do_action)
{
	AnimData *adt;

	if ((id_to && id_from) && (GS(id_to->name) != GS(id_from->name)))
		return 0;

	BKE_free_animdata(id_to);

	adt = BKE_animdata_from_id(id_from);
	if (adt) {
		IdAdtTemplate *iat = (IdAdtTemplate *)id_to;
		iat->adt= BKE_copy_animdata(adt, do_action);
	}

	return 1;
}

void BKE_copy_animdata_id_action(ID *id)
{
	AnimData *adt= BKE_animdata_from_id(id);
	if (adt) {
		if (adt->action) {
			id_us_min((ID *)adt->action);
			adt->action= copy_action(adt->action);
		}
		if (adt->tmpact) {
			id_us_min((ID *)adt->tmpact);
			adt->tmpact= copy_action(adt->tmpact);
		}
	}
}

/* Make Local -------------------------------------------- */

static void make_local_strips(ListBase *strips)
{
	NlaStrip *strip;

	for (strip=strips->first; strip; strip=strip->next) {
		if (strip->act) make_local_action(strip->act);
		if (strip->remap && strip->remap->target) make_local_action(strip->remap->target);
		
		make_local_strips(&strip->strips);
	}
}

/* Use local copy instead of linked copy of various ID-blocks */
void BKE_animdata_make_local(AnimData *adt)
{
	NlaTrack *nlt;
	
	/* Actions - Active and Temp */
	if (adt->action) make_local_action(adt->action);
	if (adt->tmpact) make_local_action(adt->tmpact);
	/* Remaps */
	if (adt->remap && adt->remap->target) make_local_action(adt->remap->target);
	
	/* Drivers */
	// TODO: need to remap the ID-targets too?
	
	/* NLA Data */
	for (nlt=adt->nla_tracks.first; nlt; nlt=nlt->next) 
		make_local_strips(&nlt->strips);
}


/* When duplicating data (i.e. objects), drivers referring to the original data will 
 * get updated to point to the duplicated data (if drivers belong to the new data)
 */
void BKE_relink_animdata (AnimData *adt)
{
	/* sanity check */
	if (adt == NULL)
		return;
	
	/* drivers */
	if (adt->drivers.first) {
		FCurve *fcu;
		
		/* check each driver against all the base paths to see if any should go */
		for (fcu= adt->drivers.first; fcu; fcu=fcu->next) {
			ChannelDriver *driver= fcu->driver;
			DriverVar *dvar;
			
			/* driver variables */
			for (dvar= driver->variables.first; dvar; dvar=dvar->next) {
				/* only change the used targets, since the others will need fixing manually anyway */
				DRIVER_TARGETS_USED_LOOPER(dvar)
				{
					if (dtar->id && dtar->id->newid) {
						dtar->id= dtar->id->newid;
					}
				}
				DRIVER_TARGETS_LOOPER_END
			}
		}
	}
}

/* Sub-ID Regrouping ------------------------------------------- */

/* helper heuristic for determining if a path is compatible with the basepath 
 * < path: (str) full RNA-path from some data (usually an F-Curve) to compare
 * < basepath: (str) shorter path fragment to look for
 * > returns (bool) whether there is a match
 */
static short animpath_matches_basepath (const char path[], const char basepath[])
{
	/* we need start of path to be basepath */
	return (path && basepath) && (strstr(path, basepath) == path);
}

/* Move F-Curves in src action to dst action, setting up all the necessary groups 
 * for this to happen, but only if the F-Curves being moved have the appropriate 
 * "base path". 
 *	- This is used when data moves from one datablock to another, causing the
 *	  F-Curves to need to be moved over too
 */
void action_move_fcurves_by_basepath (bAction *srcAct, bAction *dstAct, const char basepath[])
{
	FCurve *fcu, *fcn=NULL;
	
	/* sanity checks */
	if ELEM3(NULL, srcAct, dstAct, basepath) {
		if (G.f & G_DEBUG) {
			printf("ERROR: action_partition_fcurves_by_basepath(%p, %p, %p) has insufficient info to work with\n",
					(void *)srcAct, (void *)dstAct, (void *)basepath);
		}
		return;
	}
		
	/* clear 'temp' flags on all groups in src, as we'll be needing them later 
	 * to identify groups that we've managed to empty out here
	 */
	action_groups_clear_tempflags(srcAct);
	
	/* iterate over all src F-Curves, moving over the ones that need to be moved */
	for (fcu = srcAct->curves.first; fcu; fcu = fcn) {
		/* store next pointer in case we move stuff */
		fcn = fcu->next;
		
		/* should F-Curve be moved over?
		 *	- we only need the start of the path to match basepath
		 */
		if (animpath_matches_basepath(fcu->rna_path, basepath)) {			
			bActionGroup *agrp = NULL;
			
			/* if grouped... */
			if (fcu->grp) {
				/* make sure there will be a matching group on the other side for the migrants */
				agrp = action_groups_find_named(dstAct, fcu->grp->name);
				
				if (agrp == NULL) {
					/* add a new one with a similar name (usually will be the same though) */
					agrp = action_groups_add_new(dstAct, fcu->grp->name);
				}
				
				/* old groups should be tagged with 'temp' flags so they can be removed later
				 * if we remove everything from them
				 */
				fcu->grp->flag |= AGRP_TEMP;
			}
			
			/* perform the migration now */
			action_groups_remove_channel(srcAct, fcu);
			
			if (agrp)
				action_groups_add_channel(dstAct, agrp, fcu);
			else
				BLI_addtail(&dstAct->curves, fcu);
		}
	}
	
	/* cleanup groups (if present) */
	if (srcAct->groups.first) {
		bActionGroup *agrp, *grp=NULL;
		
		for (agrp = srcAct->groups.first; agrp; agrp = grp) {
			grp = agrp->next;
			
			/* only tagged groups need to be considered - clearing these tags or removing them */
			if (agrp->flag & AGRP_TEMP) {
				/* if group is empty and tagged, then we can remove as this operation
				 * moved out all the channels that were formerly here
				 */
				if (agrp->channels.first == NULL)
					BLI_freelinkN(&srcAct->groups, agrp);
				else
					agrp->flag &= ~AGRP_TEMP;
			}
		}
	}
}

/* Transfer the animation data from srcID to dstID where the srcID
 * animation data is based off "basepath", creating new AnimData and
 * associated data as necessary
 */
void BKE_animdata_separate_by_basepath (ID *srcID, ID *dstID, ListBase *basepaths)
{
	AnimData *srcAdt=NULL, *dstAdt=NULL;
	LinkData *ld;
	
	/* sanity checks */
	if ELEM(NULL, srcID, dstID) {
		if (G.f & G_DEBUG)
			printf("ERROR: no source or destination ID to separate AnimData with\n");
		return;
	}
	
	/* get animdata from src, and create for destination (if needed) */
	srcAdt = BKE_animdata_from_id(srcID);
	dstAdt = BKE_id_add_animdata(dstID);
	
	if ELEM(NULL, srcAdt, dstAdt) {
		if (G.f & G_DEBUG)
			printf("ERROR: no AnimData for this pair of ID's\n");
		return;
	}
	
	/* active action */
	if (srcAdt->action) {
		/* set up an action if necessary, and name it in a similar way so that it can be easily found again */
		if (dstAdt->action == NULL) {
			dstAdt->action = add_empty_action(srcAdt->action->id.name+2);
		}
		else if (dstAdt->action == srcAdt->action) {
			printf("Argh! Source and Destination share animation! ('%s' and '%s' both use '%s') Making new empty action\n",
				srcID->name, dstID->name, srcAdt->action->id.name);
			
			// TODO: review this...
			id_us_min(&dstAdt->action->id);
			dstAdt->action = add_empty_action(dstAdt->action->id.name+2);
		}
			
		/* loop over base paths, trying to fix for each one... */
		for (ld = basepaths->first; ld; ld = ld->next) {
			const char *basepath = (const char *)ld->data;
			action_move_fcurves_by_basepath(srcAdt->action, dstAdt->action, basepath);
		}
	}
	
	/* drivers */
	if (srcAdt->drivers.first) {
		FCurve *fcu, *fcn=NULL;
		
		/* check each driver against all the base paths to see if any should go */
		for (fcu = srcAdt->drivers.first; fcu; fcu = fcn) {
			fcn = fcu->next;
			
			/* try each basepath in turn, but stop on the first one which works */
			for (ld = basepaths->first; ld; ld = ld->next) {
				const char *basepath = (const char *)ld->data;
				
				if (animpath_matches_basepath(fcu->rna_path, basepath)) {
					/* just need to change lists */
					BLI_remlink(&srcAdt->drivers, fcu);
					BLI_addtail(&dstAdt->drivers, fcu);
					
					// TODO: add depsgraph flushing calls?
					
					/* can stop now, as moved already */
					break;
				}
			}
		}
	}
}

/* Path Validation -------------------------------------------- */

/* Check if a given RNA Path is valid, by tracing it from the given ID, and seeing if we can resolve it */
static short check_rna_path_is_valid (ID *owner_id, const char *path)
{
	PointerRNA id_ptr, ptr;
	PropertyRNA *prop=NULL;
	
	/* make initial RNA pointer to start resolving from */
	RNA_id_pointer_create(owner_id, &id_ptr);
	
	/* try to resolve */
	return RNA_path_resolve(&id_ptr, path, &ptr, &prop); 
}

/* Check if some given RNA Path needs fixing - free the given path and set a new one as appropriate 
 * NOTE: we assume that oldName and newName have [" "] padding around them
 */
static char *rna_path_rename_fix (ID *owner_id, const char *prefix, const char *oldName, const char *newName, char *oldpath, int verify_paths)
{
	char *prefixPtr= strstr(oldpath, prefix);
	char *oldNamePtr= strstr(oldpath, oldName);
	int prefixLen= strlen(prefix);
	int oldNameLen= strlen(oldName);
	
	/* only start fixing the path if the prefix and oldName feature in the path,
	 * and prefix occurs immediately before oldName
	 */
	if ( (prefixPtr && oldNamePtr) && (prefixPtr+prefixLen == oldNamePtr) ) {
		/* if we haven't aren't able to resolve the path now, try again after fixing it */
		if (!verify_paths || check_rna_path_is_valid(owner_id, oldpath) == 0) {		
			DynStr *ds= BLI_dynstr_new();
			char *postfixPtr= oldNamePtr+oldNameLen;
			char *newPath = NULL;
			char oldChar;
			
			/* add the part of the string that goes up to the start of the prefix */
			if (prefixPtr > oldpath) {
				oldChar= prefixPtr[0]; 
				prefixPtr[0]= 0;
				BLI_dynstr_append(ds, oldpath);
				prefixPtr[0]= oldChar;
			}
			
			/* add the prefix */
			BLI_dynstr_append(ds, prefix);
			
			/* add the new name (complete with brackets) */
			BLI_dynstr_append(ds, newName);
			
			/* add the postfix */
			BLI_dynstr_append(ds, postfixPtr);
			
			/* create new path, and cleanup old data */
			newPath= BLI_dynstr_get_cstring(ds);
			BLI_dynstr_free(ds);
			
			/* check if the new path will solve our problems */
			// TODO: will need to check whether this step really helps in practice
			if (!verify_paths || check_rna_path_is_valid(owner_id, newPath)) {
				/* free the old path, and return the new one, since we've solved the issues */
				MEM_freeN(oldpath);
				return newPath;
			}
			else {
				/* still couldn't resolve the path... so, might as well just leave it alone */
				MEM_freeN(newPath);
			}
		}
	}
	
	/* the old path doesn't need to be changed */
	return oldpath;
}

/* Check RNA-Paths for a list of F-Curves */
static void fcurves_path_rename_fix (ID *owner_id, const char *prefix, char *oldName, char *newName, ListBase *curves, int verify_paths)
{
	FCurve *fcu;
	
	/* we need to check every curve... */
	for (fcu= curves->first; fcu; fcu= fcu->next) {
		/* firstly, handle the F-Curve's own path */
		if (fcu->rna_path)
			fcu->rna_path= rna_path_rename_fix(owner_id, prefix, oldName, newName, fcu->rna_path, verify_paths);
	}
}

/* Check RNA-Paths for a list of Drivers */
static void drivers_path_rename_fix (ID *owner_id, const char *prefix, const char *oldName, const char *newName, const char *oldKey, const char *newKey, ListBase *curves, int verify_paths)
{
	FCurve *fcu;
	
	/* we need to check every curve - drivers are F-Curves too! */
	for (fcu= curves->first; fcu; fcu= fcu->next) {
		/* firstly, handle the F-Curve's own path */
		if (fcu->rna_path)
			fcu->rna_path= rna_path_rename_fix(owner_id, prefix, oldKey, newKey, fcu->rna_path, verify_paths);
		
		/* driver? */
		if (fcu->driver) {
			ChannelDriver *driver= fcu->driver;
			DriverVar *dvar;
			
			/* driver variables */
			for (dvar= driver->variables.first; dvar; dvar=dvar->next) {
				/* only change the used targets, since the others will need fixing manually anyway */
				DRIVER_TARGETS_USED_LOOPER(dvar) 
				{
					/* rename RNA path */
					if (dtar->rna_path && dtar->id)
						dtar->rna_path= rna_path_rename_fix(dtar->id, prefix, oldKey, newKey, dtar->rna_path, verify_paths);
					
					/* also fix the bone-name (if applicable) */
					if (strstr(prefix, "bones")) {
						if ( ((dtar->id) && (GS(dtar->id->name) == ID_OB)) &&
							 (dtar->pchan_name[0]) && (strcmp(oldName, dtar->pchan_name)==0) )
						{
							BLI_strncpy(dtar->pchan_name, newName, sizeof(dtar->pchan_name));
						}
					}
				}
				DRIVER_TARGETS_LOOPER_END
			}
		}
	}
}

/* Fix all RNA-Paths for Actions linked to NLA Strips */
static void nlastrips_path_rename_fix (ID *owner_id, const char *prefix, char *oldName, char *newName, ListBase *strips, int verify_paths)
{
	NlaStrip *strip;
	
	/* recursively check strips, fixing only actions... */
	for (strip= strips->first; strip; strip= strip->next) {
		/* fix strip's action */
		if (strip->act)
			fcurves_path_rename_fix(owner_id, prefix, oldName, newName, &strip->act->curves, verify_paths);
		/* ignore own F-Curves, since those are local...  */
		
		/* check sub-strips (if metas) */
		nlastrips_path_rename_fix(owner_id, prefix, oldName, newName, &strip->strips, verify_paths);
	}
}

/* Fix all RNA-Paths in the AnimData block used by the given ID block
 * NOTE: it is assumed that the structure we're replacing is <prefix><["><name><"]>
 * 		i.e. pose.bones["Bone"]
 */
void BKE_animdata_fix_paths_rename (ID *owner_id, AnimData *adt, const char *prefix, const char *oldName, const char *newName, int oldSubscript, int newSubscript, int verify_paths)
{
	NlaTrack *nlt;
	char *oldN, *newN;
	
	/* if no AnimData, no need to proceed */
	if (ELEM(NULL, owner_id, adt))
		return;
	
	if ((oldName != NULL) && (newName != NULL)) {
		/* pad the names with [" "] so that only exact matches are made */
		oldN= BLI_sprintfN("[\"%s\"]", oldName);
		newN= BLI_sprintfN("[\"%s\"]", newName);
	} 
	else {
		oldN= BLI_sprintfN("[%d]", oldSubscript);
		newN= BLI_sprintfN("[%d]", newSubscript);
	}
	
	/* Active action and temp action */
	if (adt->action)
		fcurves_path_rename_fix(owner_id, prefix, oldN, newN, &adt->action->curves, verify_paths);
	if (adt->tmpact)
		fcurves_path_rename_fix(owner_id, prefix, oldN, newN, &adt->tmpact->curves, verify_paths);
		
	/* Drivers - Drivers are really F-Curves */
	drivers_path_rename_fix(owner_id, prefix, oldName, newName, oldN, newN, &adt->drivers, verify_paths);
	
	/* NLA Data - Animation Data for Strips */
	for (nlt= adt->nla_tracks.first; nlt; nlt= nlt->next)
		nlastrips_path_rename_fix(owner_id, prefix, oldN, newN, &nlt->strips, verify_paths);
		
	/* free the temp names */
	MEM_freeN(oldN);
	MEM_freeN(newN);
}

/* Whole Database Ops -------------------------------------------- */

/* apply the given callback function on all data in main database */
void BKE_animdata_main_cb (Main *mainptr, ID_AnimData_Edit_Callback func, void *user_data)
{
	ID *id;

	/* standard data version */
#define ANIMDATA_IDS_CB(first) \
	for (id= first; id; id= id->next) { \
		AnimData *adt= BKE_animdata_from_id(id); \
		if (adt) func(id, adt, user_data); \
	}
	
	/* "embedded" nodetree cases (i.e. scene/material/texture->nodetree) */
#define ANIMDATA_NODETREE_IDS_CB(first, NtId_Type) \
	for (id= first; id; id= id->next) { \
		AnimData *adt= BKE_animdata_from_id(id); \
		NtId_Type *ntp= (NtId_Type *)id; \
		if (ntp->nodetree) { \
			AnimData *adt2= BKE_animdata_from_id((ID *)ntp); \
			if (adt2) func(id, adt2, user_data); \
		} \
		if (adt) func(id, adt, user_data); \
	}
	
	/* nodes */
	ANIMDATA_IDS_CB(mainptr->nodetree.first);
	
	/* textures */
	ANIMDATA_NODETREE_IDS_CB(mainptr->tex.first, Tex);
	
	/* lamps */
	ANIMDATA_IDS_CB(mainptr->lamp.first);
	
	/* materials */
	ANIMDATA_NODETREE_IDS_CB(mainptr->mat.first, Material);
	
	/* cameras */
	ANIMDATA_IDS_CB(mainptr->camera.first);
	
	/* shapekeys */
	ANIMDATA_IDS_CB(mainptr->key.first);
	
	/* metaballs */
	ANIMDATA_IDS_CB(mainptr->mball.first);
	
	/* curves */
	ANIMDATA_IDS_CB(mainptr->curve.first);
	
	/* armatures */
	ANIMDATA_IDS_CB(mainptr->armature.first);
	
	/* lattices */
	ANIMDATA_IDS_CB(mainptr->latt.first);
	
	/* meshes */
	ANIMDATA_IDS_CB(mainptr->mesh.first);
	
	/* particles */
	ANIMDATA_IDS_CB(mainptr->particle.first);

	/* speakers */
	ANIMDATA_IDS_CB(mainptr->speaker.first);

	/* movie clips */
	ANIMDATA_IDS_CB(mainptr->movieclip.first);

	/* objects */
	ANIMDATA_IDS_CB(mainptr->object.first);
	
	/* worlds */
	ANIMDATA_IDS_CB(mainptr->world.first);
	
	/* scenes */
	ANIMDATA_NODETREE_IDS_CB(mainptr->scene.first, Scene);
}

/* Fix all RNA-Paths throughout the database (directly access the Global.main version)
 * NOTE: it is assumed that the structure we're replacing is <prefix><["><name><"]>
 * 		i.e. pose.bones["Bone"]
 */
/* TODO: use BKE_animdata_main_cb for looping over all data  */
void BKE_all_animdata_fix_paths_rename (const char *prefix, const char *oldName, const char *newName)
{
	Main *mainptr= G.main;
	ID *id;
	
	/* macro for less typing 
	 *	- whether animdata exists is checked for by the main renaming callback, though taking 
	 *	  this outside of the function may make things slightly faster?
	 */
#define RENAMEFIX_ANIM_IDS(first) \
	for (id= first; id; id= id->next) { \
		AnimData *adt= BKE_animdata_from_id(id); \
		BKE_animdata_fix_paths_rename(id, adt, prefix, oldName, newName, 0, 0, 1);\
	}
	
	/* another version of this macro for nodetrees */
#define RENAMEFIX_ANIM_NODETREE_IDS(first, NtId_Type) \
	for (id= first; id; id= id->next) { \
		AnimData *adt= BKE_animdata_from_id(id); \
		NtId_Type *ntp= (NtId_Type *)id; \
		if (ntp->nodetree) { \
			AnimData *adt2= BKE_animdata_from_id((ID *)ntp); \
			BKE_animdata_fix_paths_rename((ID *)ntp, adt2, prefix, oldName, newName, 0, 0, 1);\
		} \
		BKE_animdata_fix_paths_rename(id, adt, prefix, oldName, newName, 0, 0, 1);\
	}
	
	/* nodes */
	RENAMEFIX_ANIM_IDS(mainptr->nodetree.first);
	
	/* textures */
	RENAMEFIX_ANIM_NODETREE_IDS(mainptr->tex.first, Tex);
	
	/* lamps */
	RENAMEFIX_ANIM_IDS(mainptr->lamp.first);
	
	/* materials */
	RENAMEFIX_ANIM_NODETREE_IDS(mainptr->mat.first, Material);
	
	/* cameras */
	RENAMEFIX_ANIM_IDS(mainptr->camera.first);
	
	/* shapekeys */
	RENAMEFIX_ANIM_IDS(mainptr->key.first);
	
	/* metaballs */
	RENAMEFIX_ANIM_IDS(mainptr->mball.first);
	
	/* curves */
	RENAMEFIX_ANIM_IDS(mainptr->curve.first);
	
	/* armatures */
	RENAMEFIX_ANIM_IDS(mainptr->armature.first);
	
	/* lattices */
	RENAMEFIX_ANIM_IDS(mainptr->latt.first);
	
	/* meshes */
	RENAMEFIX_ANIM_IDS(mainptr->mesh.first);
	
	/* particles */
	RENAMEFIX_ANIM_IDS(mainptr->particle.first);

	/* speakers */
	RENAMEFIX_ANIM_IDS(mainptr->speaker.first);

	/* movie clips */
	RENAMEFIX_ANIM_IDS(mainptr->movieclip.first);

	/* objects */
	RENAMEFIX_ANIM_IDS(mainptr->object.first); 
	
	/* worlds */
	RENAMEFIX_ANIM_IDS(mainptr->world.first);
	
	/* scenes */
	RENAMEFIX_ANIM_NODETREE_IDS(mainptr->scene.first, Scene);
}

/* *********************************** */ 
/* KeyingSet API */

/* Finding Tools --------------------------- */

/* Find the first path that matches the given criteria */
// TODO: do we want some method to perform partial matches too?
KS_Path *BKE_keyingset_find_path (KeyingSet *ks, ID *id, const char group_name[], const char rna_path[], int array_index, int UNUSED(group_mode))
{
	KS_Path *ksp;
	
	/* sanity checks */
	if ELEM3(NULL, ks, rna_path, id)
		return NULL;
	
	/* loop over paths in the current KeyingSet, finding the first one where all settings match 
	 * (i.e. the first one where none of the checks fail and equal 0)
	 */
	for (ksp= ks->paths.first; ksp; ksp= ksp->next) {
		short eq_id=1, eq_path=1, eq_index=1, eq_group=1;
		
		/* id */
		if (id != ksp->id)
			eq_id= 0;
		
		/* path */
		if ((ksp->rna_path==NULL) || strcmp(rna_path, ksp->rna_path))
			eq_path= 0;
			
		/* index - need to compare whole-array setting too... */
		if (ksp->array_index != array_index)
			eq_index= 0;
			
		/* group */
		if (group_name) {
			// FIXME: these checks need to be coded... for now, it's not too important though
		}
			
		/* if all aspects are ok, return */
		if (eq_id && eq_path && eq_index && eq_group)
			return ksp;
	}
	
	/* none found */
	return NULL;
}
 
/* Defining Tools --------------------------- */

/* Used to create a new 'custom' KeyingSet for the user, that will be automatically added to the stack */
KeyingSet *BKE_keyingset_add (ListBase *list, const char name[], short flag, short keyingflag)
{
	KeyingSet *ks;
	
	/* allocate new KeyingSet */
	ks= MEM_callocN(sizeof(KeyingSet), "KeyingSet");

	BLI_strncpy(ks->name, name ? name : "KeyingSet", sizeof(ks->name));

	ks->flag= flag;
	ks->keyingflag= keyingflag;
	
	/* add KeyingSet to list */
	BLI_addtail(list, ks);
	
	/* make sure KeyingSet has a unique name (this helps with identification) */
	BLI_uniquename(list, ks, "KeyingSet", '.', offsetof(KeyingSet, name), sizeof(ks->name));
	
	/* return new KeyingSet for further editing */
	return ks;
}

/* Add a path to a KeyingSet. Nothing is returned for now...
 * Checks are performed to ensure that destination is appropriate for the KeyingSet in question
 */
KS_Path *BKE_keyingset_add_path (KeyingSet *ks, ID *id, const char group_name[], const char rna_path[], int array_index, short flag, short groupmode)
{
	KS_Path *ksp;
	
	/* sanity checks */
	if ELEM(NULL, ks, rna_path) {
		printf("ERROR: no Keying Set and/or RNA Path to add path with \n");
		return NULL;
	}
	
	/* ID is required for all types of KeyingSets */
	if (id == NULL) {
		printf("ERROR: No ID provided for Keying Set Path. \n");
		return NULL;
	}
	
	/* don't add if there is already a matching KS_Path in the KeyingSet */
	if (BKE_keyingset_find_path(ks, id, group_name, rna_path, array_index, groupmode)) {
		if (G.f & G_DEBUG)
			printf("ERROR: destination already exists in Keying Set \n");
		return NULL;
	}
	
	/* allocate a new KeyingSet Path */
	ksp= MEM_callocN(sizeof(KS_Path), "KeyingSet Path");
	
	/* just store absolute info */
	ksp->id= id;
	if (group_name)
		BLI_strncpy(ksp->group, group_name, sizeof(ksp->group));
	else
		ksp->group[0]= '\0';
	
	/* store additional info for relative paths (just in case user makes the set relative) */
	if (id)
		ksp->idtype= GS(id->name);
	
	/* just copy path info */
	// TODO: should array index be checked too?
	ksp->rna_path= BLI_strdupn(rna_path, strlen(rna_path));
	ksp->array_index= array_index;
	
	/* store flags */
	ksp->flag= flag;
	ksp->groupmode= groupmode;
	
	/* add KeyingSet path to KeyingSet */
	BLI_addtail(&ks->paths, ksp);
	
	/* return this path */
	return ksp;
}	

/* Free the given Keying Set path */
void BKE_keyingset_free_path (KeyingSet *ks, KS_Path *ksp)
{
	/* sanity check */
	if ELEM(NULL, ks, ksp)
		return;

	/* free RNA-path info */
	if(ksp->rna_path)
		MEM_freeN(ksp->rna_path);

	/* free path itself */
	BLI_freelinkN(&ks->paths, ksp);
}

/* Copy all KeyingSets in the given list */
void BKE_keyingsets_copy (ListBase *newlist, ListBase *list)
{
	KeyingSet *ksn;
	KS_Path *kspn;
	
	BLI_duplicatelist(newlist, list);

	for (ksn=newlist->first; ksn; ksn=ksn->next) {
		BLI_duplicatelist(&ksn->paths, &ksn->paths);
		
		for (kspn=ksn->paths.first; kspn; kspn=kspn->next)
			kspn->rna_path= MEM_dupallocN(kspn->rna_path);
	}
}

/* Freeing Tools --------------------------- */

/* Free data for KeyingSet but not set itself */
void BKE_keyingset_free (KeyingSet *ks)
{
	KS_Path *ksp, *kspn;
	
	/* sanity check */
	if (ks == NULL)
		return;
	
	/* free each path as we go to avoid looping twice */
	for (ksp= ks->paths.first; ksp; ksp= kspn) {
		kspn= ksp->next;
		BKE_keyingset_free_path(ks, ksp);
	}
}

/* Free all the KeyingSets in the given list */
void BKE_keyingsets_free (ListBase *list)
{
	KeyingSet *ks, *ksn;
	
	/* sanity check */
	if (list == NULL)
		return;
	
	/* loop over KeyingSets freeing them 
	 * 	- BKE_keyingset_free() doesn't free the set itself, but it frees its sub-data
	 */
	for (ks= list->first; ks; ks= ksn) {
		ksn= ks->next;
		BKE_keyingset_free(ks);
		BLI_freelinkN(list, ks);
	}
}

/* ***************************************** */
/* Evaluation Data-Setting Backend */

/* Retrieve string to act as RNA-path, adjusted using mapping-table if provided 
 * It returns whether the string needs to be freed (i.e. if it was a temp remapped one)
 * // FIXME: maybe it would be faster if we didn't have to alloc/free strings like this all the time, but for now it's safer
 *
 *	- remap: remapping table to use
 *	- path: original path string (as stored in F-Curve data)
 *	- dst: destination string to write data to
 */
static short animsys_remap_path (AnimMapper *UNUSED(remap), char *path, char **dst)
{
	/* is there a valid remapping table to use? */
	//if (remap) {
		/* find a matching entry... to use to remap */
		// ...TODO...
	//}
	
	/* nothing suitable found, so just set dst to look at path (i.e. no alloc/free needed) */
	*dst= path;
	return 0;
}


/* less then 1.0 evaluates to false, use epsilon to avoid float error */
#define ANIMSYS_FLOAT_AS_BOOL(value) ((value) > ((1.0f-FLT_EPSILON)))

/* Write the given value to a setting using RNA, and return success */
static short animsys_write_rna_setting (PointerRNA *ptr, char *path, int array_index, float value)
{
	PropertyRNA *prop;
	PointerRNA new_ptr;
	
	//printf("%p %s %i %f\n", ptr, path, array_index, value);
	
	/* get property to write to */
	if (RNA_path_resolve(ptr, path, &new_ptr, &prop)) 
	{
		/* set value - only for animatable numerical values */
		if (RNA_property_animateable(&new_ptr, prop)) 
		{
			int array_len= RNA_property_array_length(&new_ptr, prop);
			
			if (array_len && array_index >= array_len)
			{
				if (G.f & G_DEBUG) {
					printf("Animato: Invalid array index. ID = '%s',  '%s[%d]', array length is %d \n",
						(ptr && ptr->id.data) ? (((ID *)ptr->id.data)->name+2) : "<No ID>",
						path, array_index, array_len-1);
				}
				
				return 0;
			}
			
			switch (RNA_property_type(prop)) 
			{
				case PROP_BOOLEAN:
					if (array_len)
						RNA_property_boolean_set_index(&new_ptr, prop, array_index, ANIMSYS_FLOAT_AS_BOOL(value));
					else
						RNA_property_boolean_set(&new_ptr, prop, ANIMSYS_FLOAT_AS_BOOL(value));
					break;
				case PROP_INT:
					if (array_len)
						RNA_property_int_set_index(&new_ptr, prop, array_index, (int)value);
					else
						RNA_property_int_set(&new_ptr, prop, (int)value);
					break;
				case PROP_FLOAT:
					if (array_len)
						RNA_property_float_set_index(&new_ptr, prop, array_index, value);
					else
						RNA_property_float_set(&new_ptr, prop, value);
					break;
				case PROP_ENUM:
					RNA_property_enum_set(&new_ptr, prop, (int)value);
					break;
				default:
					/* nothing can be done here... so it is unsuccessful? */
					return 0;
			}
			
			/* RNA property update disabled for now - [#28525] [#28690] [#28774] [#28777] */
#if 0
			/* buffer property update for later flushing */
			if (RNA_property_update_check(prop)) {
				short skip_updates_hack = 0;
				
				/* optimisation hacks: skip property updates for those properties
				 * for we know that which the updates in RNA were really just for
				 * flushing property editing via UI/Py
				 */
				if (new_ptr.type == &RNA_PoseBone) {
					/* bone transforms - update pose (i.e. tag depsgraph) */
					skip_updates_hack = 1;
				}				
				
				if (skip_updates_hack == 0)
					RNA_property_update_cache_add(&new_ptr, prop);
			}
#endif

			/* as long as we don't do property update, we still tag datablock
			   as having been updated. this flag does not cause any updates to
			   be run, it's for e.g. render engines to synchronize data */
			if(new_ptr.id.data) {
				ID *id= new_ptr.id.data;
				id->flag |= LIB_ID_RECALC;
				DAG_id_type_tag(G.main, GS(id->name));
			}
		}
		
		/* successful */
		return 1;
	}
	else {
		/* failed to get path */
		// XXX don't tag as failed yet though, as there are some legit situations (Action Constraint) 
		// where some channels will not exist, but shouldn't lock up Action
		if (G.f & G_DEBUG) {
			printf("Animato: Invalid path. ID = '%s',  '%s[%d]' \n",
				(ptr && ptr->id.data) ? (((ID *)ptr->id.data)->name+2) : "<No ID>", 
				path, array_index);
		}
		return 0;
	}
}

/* Simple replacement based data-setting of the FCurve using RNA */
static short animsys_execute_fcurve (PointerRNA *ptr, AnimMapper *remap, FCurve *fcu)
{
	char *path = NULL;
	short free_path=0;
	short ok= 0;
	
	/* get path, remapped as appropriate to work in its new environment */
	free_path= animsys_remap_path(remap, fcu->rna_path, &path);
	
	/* write value to setting */
	if (path)
		ok= animsys_write_rna_setting(ptr, path, fcu->array_index, fcu->curval);
	
	/* free temp path-info */
	if (free_path)
		MEM_freeN(path);
		
	/* return whether we were successful */
	return ok;
}

/* Evaluate all the F-Curves in the given list 
 * This performs a set of standard checks. If extra checks are required, separate code should be used
 */
static void animsys_evaluate_fcurves (PointerRNA *ptr, ListBase *list, AnimMapper *remap, float ctime)
{
	FCurve *fcu;
	
	/* calculate then execute each curve */
	for (fcu= list->first; fcu; fcu= fcu->next) 
	{
		/* check if this F-Curve doesn't belong to a muted group */
		if ((fcu->grp == NULL) || (fcu->grp->flag & AGRP_MUTED)==0) {
			/* check if this curve should be skipped */
			if ((fcu->flag & (FCURVE_MUTED|FCURVE_DISABLED)) == 0) 
			{
				calculate_fcurve(fcu, ctime);
				animsys_execute_fcurve(ptr, remap, fcu); 
			}
		}
	}
}

/* ***************************************** */
/* Driver Evaluation */

/* Evaluate Drivers */
static void animsys_evaluate_drivers (PointerRNA *ptr, AnimData *adt, float ctime)
{
	FCurve *fcu;
	
	/* drivers are stored as F-Curves, but we cannot use the standard code, as we need to check if
	 * the depsgraph requested that this driver be evaluated...
	 */
	for (fcu= adt->drivers.first; fcu; fcu= fcu->next) 
	{
		ChannelDriver *driver= fcu->driver;
		short ok= 0;
		
		/* check if this driver's curve should be skipped */
		if ((fcu->flag & (FCURVE_MUTED|FCURVE_DISABLED)) == 0) 
		{
			/* check if driver itself is tagged for recalculation */
			if ((driver) && !(driver->flag & DRIVER_FLAG_INVALID)/*&& (driver->flag & DRIVER_FLAG_RECALC)*/) {	// XXX driver recalc flag is not set yet by depsgraph!
				/* evaluate this using values set already in other places */
				// NOTE: for 'layering' option later on, we should check if we should remove old value before adding new to only be done when drivers only changed
				calculate_fcurve(fcu, ctime);
				ok= animsys_execute_fcurve(ptr, NULL, fcu);
				
				/* clear recalc flag */
				driver->flag &= ~DRIVER_FLAG_RECALC;
				
				/* set error-flag if evaluation failed */
				if (ok == 0)
					driver->flag |= DRIVER_FLAG_INVALID; 
			}
		}
	}
}

/* ***************************************** */
/* Actions Evaluation */

/* strictly not necessary for actual "evaluation", but it is a useful safety check
 * to reduce the amount of times that users end up having to "revive" wrongly-assigned
 * actions
 */
static void action_idcode_patch_check (ID *id, bAction *act)
{
	int idcode = 0;
	
	/* just in case */
	if (ELEM(NULL, id, act))
		return;
	else
		idcode = GS(id->name);
	
	/* the actual checks... hopefully not too much of a performance hit in the long run... */
	if (act->idroot == 0) {
		/* use the current root if not set already (i.e. newly created actions and actions from 2.50-2.57 builds)
		 * 	- this has problems if there are 2 users, and the first one encountered is the invalid one
		 *	  in which case, the user will need to manually fix this (?)
		 */
		act->idroot = idcode;
	}
	else if (act->idroot != idcode) {
		/* only report this error if debug mode is enabled (to save performance everywhere else) */
		if (G.f & G_DEBUG) {
			printf("AnimSys Safety Check Failed: Action '%s' is not meant to be used from ID-Blocks of type %d such as '%s'\n",
				act->id.name+2, idcode, id->name);
		}
	}
}

/* ----------------------------------------- */

/* Evaluate Action Group */
void animsys_evaluate_action_group (PointerRNA *ptr, bAction *act, bActionGroup *agrp, AnimMapper *remap, float ctime)
{
	FCurve *fcu;
	
	/* check if mapper is appropriate for use here (we set to NULL if it's inappropriate) */
	if ELEM(NULL, act, agrp) return;
	if ((remap) && (remap->target != act)) remap= NULL;
	
	action_idcode_patch_check(ptr->id.data, act);
	
	/* if group is muted, don't evaluated any of the F-Curve */
	if (agrp->flag & AGRP_MUTED)
		return;
	
	/* calculate then execute each curve */
	for (fcu= agrp->channels.first; (fcu) && (fcu->grp == agrp); fcu= fcu->next) 
	{
		/* check if this curve should be skipped */
		if ((fcu->flag & (FCURVE_MUTED|FCURVE_DISABLED)) == 0) 
		{
			calculate_fcurve(fcu, ctime);
			animsys_execute_fcurve(ptr, remap, fcu); 
		}
	}
}

/* Evaluate Action (F-Curve Bag) */
void animsys_evaluate_action (PointerRNA *ptr, bAction *act, AnimMapper *remap, float ctime)
{
	/* check if mapper is appropriate for use here (we set to NULL if it's inappropriate) */
	if (act == NULL) return;
	if ((remap) && (remap->target != act)) remap= NULL;
	
	action_idcode_patch_check(ptr->id.data, act);
	
	/* calculate then execute each curve */
	animsys_evaluate_fcurves(ptr, &act->curves, remap, ctime);
}

/* ***************************************** */
/* NLA System - Evaluation */

/* calculate influence of strip based for given frame based on blendin/out values */
static float nlastrip_get_influence (NlaStrip *strip, float cframe)
{
	/* sanity checks - normalise the blendin/out values? */
	strip->blendin= fabsf(strip->blendin);
	strip->blendout= fabsf(strip->blendout);
	
	/* result depends on where frame is in respect to blendin/out values */
	if (IS_EQ(strip->blendin, 0)==0 && (cframe <= (strip->start + strip->blendin))) {
		/* there is some blend-in */
		return fabsf(cframe - strip->start) / (strip->blendin);
	}
	else if (IS_EQ(strip->blendout, 0)==0 && (cframe >= (strip->end - strip->blendout))) {
		/* there is some blend-out */
		return fabsf(strip->end - cframe) / (strip->blendout);
	}
	else {
		/* in the middle of the strip, we should be full strength */
		return 1.0f;
	}
}

/* evaluate the evaluation time and influence for the strip, storing the results in the strip */
static void nlastrip_evaluate_controls (NlaStrip *strip, float ctime)
{
	/* firstly, analytically generate values for influence and time (if applicable) */
	if ((strip->flag & NLASTRIP_FLAG_USR_TIME) == 0)
		strip->strip_time= nlastrip_get_frame(strip, ctime, NLATIME_CONVERT_EVAL);
	if ((strip->flag & NLASTRIP_FLAG_USR_INFLUENCE) == 0)
		strip->influence= nlastrip_get_influence(strip, ctime);
	
	/* now strip's evaluate F-Curves for these settings (if applicable) */
	if (strip->fcurves.first) {
		PointerRNA strip_ptr;
		
		/* create RNA-pointer needed to set values */
		RNA_pointer_create(NULL, &RNA_NlaStrip, strip, &strip_ptr);
		
		/* execute these settings as per normal */
		animsys_evaluate_fcurves(&strip_ptr, &strip->fcurves, NULL, ctime);
	}

	/* if user can control the evaluation time (using F-Curves), consider the option which allows this time to be clamped 
	 * to lie within extents of the action-clip, so that a steady changing rate of progress through several cycles of the clip
	 * can be achieved easily
	 */
	// NOTE: if we add any more of these special cases, we better group them up nicely...
	if ((strip->flag & NLASTRIP_FLAG_USR_TIME) && (strip->flag & NLASTRIP_FLAG_USR_TIME_CYCLIC))
		strip->strip_time= fmod(strip->strip_time - strip->actstart, strip->actend - strip->actstart);
}

/* gets the strip active at the current time for a list of strips for evaluation purposes */
NlaEvalStrip *nlastrips_ctime_get_strip (ListBase *list, ListBase *strips, short index, float ctime)
{
	NlaStrip *strip, *estrip=NULL;
	NlaEvalStrip *nes;
	short side= 0;
	
	/* loop over strips, checking if they fall within the range */
	for (strip= strips->first; strip; strip= strip->next) {
		/* check if current time occurs within this strip  */
		if (IN_RANGE_INCL(ctime, strip->start, strip->end)) {
			/* this strip is active, so try to use it */
			estrip= strip;
			side= NES_TIME_WITHIN;
			break;
		}
		
		/* if time occurred before current strip... */
		if (ctime < strip->start) {
			if (strip == strips->first) {
				/* before first strip - only try to use it if it extends backwards in time too */
				if (strip->extendmode == NLASTRIP_EXTEND_HOLD)
					estrip= strip;
					
				/* side is 'before' regardless of whether there's a useful strip */
				side= NES_TIME_BEFORE;
			}
			else {
				/* before next strip - previous strip has ended, but next hasn't begun, 
				 * so blending mode depends on whether strip is being held or not...
				 * 	- only occurs when no transition strip added, otherwise the transition would have
				 * 	  been picked up above...
				 */
				strip= strip->prev;
				
				if (strip->extendmode != NLASTRIP_EXTEND_NOTHING)
					estrip= strip;
				side= NES_TIME_AFTER;
			}
			break;
		}
		
		/* if time occurred after current strip... */
		if (ctime > strip->end) {
			/* only if this is the last strip should we do anything, and only if that is being held */
			if (strip == strips->last) {
				if (strip->extendmode != NLASTRIP_EXTEND_NOTHING)
					estrip= strip;
					
				side= NES_TIME_AFTER;
				break;
			}
			
			/* otherwise, skip... as the 'before' case will catch it more elegantly! */
		}
	}
	
	/* check if a valid strip was found
	 *	- must not be muted (i.e. will have contribution
	 */
	if ((estrip == NULL) || (estrip->flag & NLASTRIP_FLAG_MUTED)) 
		return NULL;
		
	/* if ctime was not within the boundaries of the strip, clamp! */
	switch (side) {
		case NES_TIME_BEFORE: /* extend first frame only */
			ctime= estrip->start;
			break;
		case NES_TIME_AFTER: /* extend last frame only */
			ctime= estrip->end;
			break;
	}
	
	/* evaluate strip's evaluation controls  
	 * 	- skip if no influence (i.e. same effect as muting the strip)
	 *	- negative influence is not supported yet... how would that be defined?
	 */
	// TODO: this sounds a bit hacky having a few isolated F-Curves stuck on some data it operates on...
	nlastrip_evaluate_controls(estrip, ctime);
	if (estrip->influence <= 0.0f)
		return NULL;
		
	/* check if strip has valid data to evaluate,
	 * and/or perform any additional type-specific actions
	 */
	switch (estrip->type) {
		case NLASTRIP_TYPE_CLIP: 
			/* clip must have some action to evaluate */
			if (estrip->act == NULL)
				return NULL;
			break;
		case NLASTRIP_TYPE_TRANSITION:
			/* there must be strips to transition from and to (i.e. prev and next required) */
			if (ELEM(NULL, estrip->prev, estrip->next))
				return NULL;
				
			/* evaluate controls for the relevant extents of the bordering strips... */
			nlastrip_evaluate_controls(estrip->prev, estrip->start);
			nlastrip_evaluate_controls(estrip->next, estrip->end);
			break;
	}
	
	/* add to list of strips we need to evaluate */
	nes= MEM_callocN(sizeof(NlaEvalStrip), "NlaEvalStrip");
	
	nes->strip= estrip;
	nes->strip_mode= side;
	nes->track_index= index;
	nes->strip_time= estrip->strip_time;
	
	if (list)
		BLI_addtail(list, nes);
	
	return nes;
}

/* ---------------------- */

/* find an NlaEvalChannel that matches the given criteria 
 *	- ptr and prop are the RNA data to find a match for
 */
static NlaEvalChannel *nlaevalchan_find_match (ListBase *channels, PointerRNA *ptr, PropertyRNA *prop, int array_index)
{
	NlaEvalChannel *nec;
	
	/* sanity check */
	if (channels == NULL)
		return NULL;
	
	/* loop through existing channels, checking for a channel which affects the same property */
	for (nec= channels->first; nec; nec= nec->next) {
		/* - comparing the PointerRNA's is done by comparing the pointers
		 *   to the actual struct the property resides in, since that all the
		 *   other data stored in PointerRNA cannot allow us to definitively 
		 *	identify the data 
		 */
		if ((nec->ptr.data == ptr->data) && (nec->prop == prop) && (nec->index == array_index))
			return nec;
	}
	
	/* not found */
	return NULL;
}

/* verify that an appropriate NlaEvalChannel for this F-Curve exists */
static NlaEvalChannel *nlaevalchan_verify (PointerRNA *ptr, ListBase *channels, NlaEvalStrip *nes, FCurve *fcu, short *newChan)
{
	NlaEvalChannel *nec;
	NlaStrip *strip= nes->strip;
	PropertyRNA *prop;
	PointerRNA new_ptr;
	char *path = NULL;
	/* short free_path=0; */
	
	/* sanity checks */
	if (channels == NULL)
		return NULL;
	
	/* get RNA pointer+property info from F-Curve for more convenient handling */
		/* get path, remapped as appropriate to work in its new environment */
	/* free_path= */ /* UNUSED */ animsys_remap_path(strip->remap, fcu->rna_path, &path);
	
		/* a valid property must be available, and it must be animateable */
	if (RNA_path_resolve(ptr, path, &new_ptr, &prop) == 0) {
		if (G.f & G_DEBUG) printf("NLA Strip Eval: Cannot resolve path \n");
		return NULL;
	}
		/* only ok if animateable */
	else if (RNA_property_animateable(&new_ptr, prop) == 0) {
		if (G.f & G_DEBUG) printf("NLA Strip Eval: Property not animateable \n");
		return NULL;
	}
	
	/* try to find a match */
	nec= nlaevalchan_find_match(channels, &new_ptr, prop, fcu->array_index);
	
	/* allocate a new struct for this if none found */
	if (nec == NULL) {
		nec= MEM_callocN(sizeof(NlaEvalChannel), "NlaEvalChannel");
		*newChan= 1;
		BLI_addtail(channels, nec);
		
		nec->ptr= new_ptr; 
		nec->prop= prop;
		nec->index= fcu->array_index;
	}
	else
		*newChan= 0;
	
	/* we can now return */
	return nec;
}

/* accumulate (i.e. blend) the given value on to the channel it affects */
static void nlaevalchan_accumulate (NlaEvalChannel *nec, NlaEvalStrip *nes, short newChan, float value)
{
	NlaStrip *strip= nes->strip;
	short blendmode= strip->blendmode;
	float inf= strip->influence;
	
	/* if channel is new, just store value regardless of blending factors, etc. */
	if (newChan) {
		nec->value= value;
		return;
	}
		
	/* if this is being performed as part of transition evaluation, incorporate
	 * an additional weighting factor for the influence
	 */
	if (nes->strip_mode == NES_TIME_TRANSITION_END) 
		inf *= nes->strip_time;
	
	/* premultiply the value by the weighting factor */
	if (IS_EQ(inf, 0)) return;
	value *= inf;
	
	/* perform blending */
	switch (blendmode) {
		case NLASTRIP_MODE_ADD:
			/* simply add the scaled value on to the stack */
			nec->value += value;
			break;
			
		case NLASTRIP_MODE_SUBTRACT:
			/* simply subtract the scaled value from the stack */
			nec->value -= value;
			break;
			
		case NLASTRIP_MODE_MULTIPLY:
			/* multiply the scaled value with the stack */
			nec->value *= value;
			break;
		
		case NLASTRIP_MODE_REPLACE:
		default: // TODO: do we really want to blend by default? it seems more uses might prefer add...
			/* do linear interpolation 
			 *	- the influence of the accumulated data (elsewhere, that is called dstweight) 
			 *	  is 1 - influence, since the strip's influence is srcweight
			 */
			nec->value= nec->value * (1.0f - inf)   +   value;
			break;
	}
}

/* accumulate the results of a temporary buffer with the results of the full-buffer */
static void nlaevalchan_buffers_accumulate (ListBase *channels, ListBase *tmp_buffer, NlaEvalStrip *nes)
{
	NlaEvalChannel *nec, *necn, *necd;
	
	/* optimise - abort if no channels */
	if (tmp_buffer->first == NULL)
		return;
	
	/* accumulate results in tmp_channels buffer to the accumulation buffer */
	for (nec= tmp_buffer->first; nec; nec= necn) {
		/* get pointer to next channel in case we remove the current channel from the temp-buffer */
		necn= nec->next;
		
		/* try to find an existing matching channel for this setting in the accumulation buffer */
		necd= nlaevalchan_find_match(channels, &nec->ptr, nec->prop, nec->index);
		
		/* if there was a matching channel already in the buffer, accumulate to it,
		 * otherwise, add the current channel to the buffer for efficiency
		 */
		if (necd)
			nlaevalchan_accumulate(necd, nes, 0, nec->value);
		else {
			BLI_remlink(tmp_buffer, nec);
			BLI_addtail(channels, nec);
		}
	}
	
	/* free temp-channels that haven't been assimilated into the buffer */
	BLI_freelistN(tmp_buffer);
}

/* ---------------------- */
/* F-Modifier stack joining/separation utilities - should we generalise these for BLI_listbase.h interface? */

/* Temporarily join two lists of modifiers together, storing the result in a third list */
static void nlaeval_fmodifiers_join_stacks (ListBase *result, ListBase *list1, ListBase *list2)
{
	FModifier *fcm1, *fcm2;
	
	/* if list1 is invalid...  */
	if ELEM(NULL, list1, list1->first) {
		if (list2 && list2->first) {
			result->first= list2->first;
			result->last= list2->last;
		}
	}
	/* if list 2 is invalid... */
	else if ELEM(NULL, list2, list2->first) {
		result->first= list1->first;
		result->last= list1->last;
	}
	else {
		/* list1 should be added first, and list2 second, with the endpoints of these being the endpoints for result 
		 * 	- the original lists must be left unchanged though, as we need that fact for restoring
		 */
		result->first= list1->first;
		result->last= list2->last;
		
		fcm1= list1->last;
		fcm2= list2->first;
		
		fcm1->next= fcm2;
		fcm2->prev= fcm1;
	}
}

/* Split two temporary lists of modifiers */
static void nlaeval_fmodifiers_split_stacks (ListBase *list1, ListBase *list2)
{
	FModifier *fcm1, *fcm2;
	
	/* if list1/2 is invalid... just skip */
	if ELEM(NULL, list1, list2)
		return;
	if ELEM(NULL, list1->first, list2->first)
		return;
		
	/* get endpoints */
	fcm1= list1->last;
	fcm2= list2->first;
	
	/* clear their links */
	fcm1->next= NULL;
	fcm2->prev= NULL;
}

/* ---------------------- */

/* evaluate action-clip strip */
static void nlastrip_evaluate_actionclip (PointerRNA *ptr, ListBase *channels, ListBase *modifiers, NlaEvalStrip *nes)
{
	ListBase tmp_modifiers = {NULL, NULL};
	NlaStrip *strip= nes->strip;
	FCurve *fcu;
	float evaltime;
	
	/* sanity checks for action */
	if (strip == NULL)
		return;
		
	if (strip->act == NULL) {
		printf("NLA-Strip Eval Error: Strip '%s' has no Action\n", strip->name);
		return;
	}
	
	action_idcode_patch_check(ptr->id.data, strip->act);
	
	/* join this strip's modifiers to the parent's modifiers (own modifiers first) */
	nlaeval_fmodifiers_join_stacks(&tmp_modifiers, &strip->modifiers, modifiers);
	
	/* evaluate strip's modifiers which modify time to evaluate the base curves at */
	evaltime= evaluate_time_fmodifiers(&tmp_modifiers, NULL, 0.0f, strip->strip_time);
	
	/* evaluate all the F-Curves in the action, saving the relevant pointers to data that will need to be used */
	for (fcu= strip->act->curves.first; fcu; fcu= fcu->next) {
		NlaEvalChannel *nec;
		float value = 0.0f;
		short newChan = -1;
		
		/* check if this curve should be skipped */
		if (fcu->flag & (FCURVE_MUTED|FCURVE_DISABLED)) 
			continue;
		if ((fcu->grp) && (fcu->grp->flag & AGRP_MUTED))
			continue;
			
		/* evaluate the F-Curve's value for the time given in the strip 
		 * NOTE: we use the modified time here, since strip's F-Curve Modifiers are applied on top of this 
		 */
		value= evaluate_fcurve(fcu, evaltime);
		
		/* apply strip's F-Curve Modifiers on this value 
		 * NOTE: we apply the strip's original evaluation time not the modified one (as per standard F-Curve eval)
		 */
		evaluate_value_fmodifiers(&tmp_modifiers, fcu, &value, strip->strip_time);
		
		
		/* get an NLA evaluation channel to work with, and accumulate the evaluated value with the value(s)
		 * stored in this channel if it has been used already
		 */
		nec= nlaevalchan_verify(ptr, channels, nes, fcu, &newChan);
		if (nec)
			nlaevalchan_accumulate(nec, nes, newChan, value);
	}
	
	/* unlink this strip's modifiers from the parent's modifiers again */
	nlaeval_fmodifiers_split_stacks(&strip->modifiers, modifiers);
}

/* evaluate transition strip */
static void nlastrip_evaluate_transition (PointerRNA *ptr, ListBase *channels, ListBase *modifiers, NlaEvalStrip *nes)
{
	ListBase tmp_channels = {NULL, NULL};
	ListBase tmp_modifiers = {NULL, NULL};
	NlaEvalStrip tmp_nes;
	NlaStrip *s1, *s2;
	
	/* join this strip's modifiers to the parent's modifiers (own modifiers first) */
	nlaeval_fmodifiers_join_stacks(&tmp_modifiers, &nes->strip->modifiers, modifiers);
	
	/* get the two strips to operate on 
	 *	- we use the endpoints of the strips directly flanking our strip
	 *	  using these as the endpoints of the transition (destination and source)
	 *	- these should have already been determined to be valid...
	 *	- if this strip is being played in reverse, we need to swap these endpoints
	 *	  otherwise they will be interpolated wrong
	 */
	if (nes->strip->flag & NLASTRIP_FLAG_REVERSE) {
		s1= nes->strip->next;
		s2= nes->strip->prev;
	}
	else {
		s1= nes->strip->prev;
		s2= nes->strip->next;
	}
	
	/* prepare template for 'evaluation strip' 
	 *	- based on the transition strip's evaluation strip data
	 *	- strip_mode is NES_TIME_TRANSITION_* based on which endpoint
	 *	- strip_time is the 'normalised' (i.e. in-strip) time for evaluation,
	 *	  which doubles up as an additional weighting factor for the strip influences
	 *	  which allows us to appear to be 'interpolating' between the two extremes
	 */
	tmp_nes= *nes;
	
	/* evaluate these strips into a temp-buffer (tmp_channels) */
	// FIXME: modifier evalation here needs some work...
		/* first strip */
	tmp_nes.strip_mode= NES_TIME_TRANSITION_START;
	tmp_nes.strip= s1;
	nlastrip_evaluate(ptr, &tmp_channels, &tmp_modifiers, &tmp_nes);
	
		/* second strip */
	tmp_nes.strip_mode= NES_TIME_TRANSITION_END;
	tmp_nes.strip= s2;
	nlastrip_evaluate(ptr, &tmp_channels, &tmp_modifiers, &tmp_nes);
	
	
	/* assumulate temp-buffer and full-buffer, using the 'real' strip */
	nlaevalchan_buffers_accumulate(channels, &tmp_channels, nes);
	
	/* unlink this strip's modifiers from the parent's modifiers again */
	nlaeval_fmodifiers_split_stacks(&nes->strip->modifiers, modifiers);
}

/* evaluate meta-strip */
static void nlastrip_evaluate_meta (PointerRNA *ptr, ListBase *channels, ListBase *modifiers, NlaEvalStrip *nes)
{
	ListBase tmp_channels = {NULL, NULL};
	ListBase tmp_modifiers = {NULL, NULL};
	NlaStrip *strip= nes->strip;
	NlaEvalStrip *tmp_nes;
	float evaltime;
	
	/* meta-strip was calculated normally to have some time to be evaluated at
	 * and here we 'look inside' the meta strip, treating it as a decorated window to
	 * it's child strips, which get evaluated as if they were some tracks on a strip 
	 * (but with some extra modifiers to apply).
	 *
	 * NOTE: keep this in sync with animsys_evaluate_nla()
	 */
	 
	/* join this strip's modifiers to the parent's modifiers (own modifiers first) */
	nlaeval_fmodifiers_join_stacks(&tmp_modifiers, &strip->modifiers, modifiers); 
	
	/* find the child-strip to evaluate */
	evaltime= (nes->strip_time * (strip->end - strip->start)) + strip->start;
	tmp_nes= nlastrips_ctime_get_strip(NULL, &strip->strips, -1, evaltime);
	if (tmp_nes == NULL)
		return;
		
	/* evaluate child-strip into tmp_channels buffer before accumulating 
	 * in the accumulation buffer
	 */
	nlastrip_evaluate(ptr, &tmp_channels, &tmp_modifiers, tmp_nes);
	
	/* assumulate temp-buffer and full-buffer, using the 'real' strip */
	nlaevalchan_buffers_accumulate(channels, &tmp_channels, nes);
	
	/* free temp eval-strip */
	MEM_freeN(tmp_nes);
	
	/* unlink this strip's modifiers from the parent's modifiers again */
	nlaeval_fmodifiers_split_stacks(&strip->modifiers, modifiers);
}

/* evaluates the given evaluation strip */
void nlastrip_evaluate (PointerRNA *ptr, ListBase *channels, ListBase *modifiers, NlaEvalStrip *nes)
{
	NlaStrip *strip= nes->strip;
	
	/* to prevent potential infinite recursion problems (i.e. transition strip, beside meta strip containing a transition
	 * several levels deep inside it), we tag the current strip as being evaluated, and clear this when we leave
	 */
	// TODO: be careful with this flag, since some edit tools may be running and have set this while animplayback was running
	if (strip->flag & NLASTRIP_FLAG_EDIT_TOUCHED)
		return;
	strip->flag |= NLASTRIP_FLAG_EDIT_TOUCHED;
	
	/* actions to take depend on the type of strip */
	switch (strip->type) {
		case NLASTRIP_TYPE_CLIP: /* action-clip */
			nlastrip_evaluate_actionclip(ptr, channels, modifiers, nes);
			break;
		case NLASTRIP_TYPE_TRANSITION: /* transition */
			nlastrip_evaluate_transition(ptr, channels, modifiers, nes);
			break;
		case NLASTRIP_TYPE_META: /* meta */
			nlastrip_evaluate_meta(ptr, channels, modifiers, nes);
			break;
			
		default: /* do nothing */
			break;
	}
	
	/* clear temp recursion safe-check */
	strip->flag &= ~NLASTRIP_FLAG_EDIT_TOUCHED;
}

/* write the accumulated settings to */
void nladata_flush_channels (ListBase *channels)
{
	NlaEvalChannel *nec;
	
	/* sanity checks */
	if (channels == NULL)
		return;
	
	/* for each channel with accumulated values, write its value on the property it affects */
	for (nec= channels->first; nec; nec= nec->next) {
		PointerRNA *ptr= &nec->ptr;
		PropertyRNA *prop= nec->prop;
		int array_index= nec->index;
		float value= nec->value;
		
		/* write values - see animsys_write_rna_setting() to sync the code */
		switch (RNA_property_type(prop)) 
		{
			case PROP_BOOLEAN:
				if (RNA_property_array_length(ptr, prop))
					RNA_property_boolean_set_index(ptr, prop, array_index, ANIMSYS_FLOAT_AS_BOOL(value));
				else
					RNA_property_boolean_set(ptr, prop, ANIMSYS_FLOAT_AS_BOOL(value));
				break;
			case PROP_INT:
				if (RNA_property_array_length(ptr, prop))
					RNA_property_int_set_index(ptr, prop, array_index, (int)value);
				else
					RNA_property_int_set(ptr, prop, (int)value);
				break;
			case PROP_FLOAT:
				if (RNA_property_array_length(ptr, prop))
					RNA_property_float_set_index(ptr, prop, array_index, value);
				else
					RNA_property_float_set(ptr, prop, value);
				break;
			case PROP_ENUM:
				RNA_property_enum_set(ptr, prop, (int)value);
				break;
			default:
				// can't do anything with other types of property....
				break;
		}
	}
}

/* ---------------------- */

/* NLA Evaluation function - values are calculated and stored in temporary "NlaEvalChannels" 
 * ! This is exported so that keyframing code can use this for make use of it for anim layers support
 * > echannels: (list<NlaEvalChannels>) evaluation channels with calculated values
 */
static void animsys_evaluate_nla (ListBase *echannels, PointerRNA *ptr, AnimData *adt, float ctime)
{
	NlaTrack *nlt;
	short track_index=0;
	short has_strips = 0;
	
	ListBase estrips= {NULL, NULL};
	NlaEvalStrip *nes;
	
	/* 1. get the stack of strips to evaluate at current time (influence calculated here) */
	for (nlt=adt->nla_tracks.first; nlt; nlt=nlt->next, track_index++) { 
		/* stop here if tweaking is on and this strip is the tweaking track (it will be the first one that's 'disabled')... */
		if ((adt->flag & ADT_NLA_EDIT_ON) && (nlt->flag & NLATRACK_DISABLED))
			break;
			
		/* skip if we're only considering a track tagged 'solo' */
		if ((adt->flag & ADT_NLA_SOLO_TRACK) && (nlt->flag & NLATRACK_SOLO)==0)
			continue;
		/* skip if track is muted */
		if (nlt->flag & NLATRACK_MUTED) 
			continue;
			
		/* if this track has strips (but maybe they won't be suitable), set has_strips 
		 *	- used for mainly for still allowing normal action evaluation...
		 */
		if (nlt->strips.first)
			has_strips= 1;
			
		/* otherwise, get strip to evaluate for this channel */
		nes= nlastrips_ctime_get_strip(&estrips, &nlt->strips, track_index, ctime);
		if (nes) nes->track= nlt;
	}
	
	/* add 'active' Action (may be tweaking track) as last strip to evaluate in NLA stack
	 *	- only do this if we're not exclusively evaluating the 'solo' NLA-track
	 */
	if ((adt->action) && !(adt->flag & ADT_NLA_SOLO_TRACK)) {
		/* if there are strips, evaluate action as per NLA rules */
		if ((has_strips) || (adt->actstrip)) {
			/* make dummy NLA strip, and add that to the stack */
			NlaStrip dummy_strip= {NULL};
			ListBase dummy_trackslist;
			
			dummy_trackslist.first= dummy_trackslist.last= &dummy_strip;
			
			if ((nlt) && !(adt->flag & ADT_NLA_EDIT_NOMAP)) {
				/* edit active action in-place according to its active strip, so copy the data  */
				memcpy(&dummy_strip, adt->actstrip, sizeof(NlaStrip));
				dummy_strip.next = dummy_strip.prev = NULL;
			}
			else {
				/* set settings of dummy NLA strip from AnimData settings */
				dummy_strip.act= adt->action;
				dummy_strip.remap= adt->remap;
				
				/* action range is calculated taking F-Modifiers into account (which making new strips doesn't do due to the troublesome nature of that) */
				calc_action_range(dummy_strip.act, &dummy_strip.actstart, &dummy_strip.actend, 1);
				dummy_strip.start = dummy_strip.actstart;
				dummy_strip.end = (IS_EQF(dummy_strip.actstart, dummy_strip.actend)) ?  (dummy_strip.actstart + 1.0f): (dummy_strip.actend);
				
				dummy_strip.blendmode= adt->act_blendmode;
				dummy_strip.extendmode= adt->act_extendmode;
				dummy_strip.influence= adt->act_influence;
			}
			
			/* add this to our list of evaluation strips */
			nlastrips_ctime_get_strip(&estrips, &dummy_trackslist, -1, ctime);
		}
		else {
			/* special case - evaluate as if there isn't any NLA data */
			// TODO: this is really just a stop-gap measure...
			animsys_evaluate_action(ptr, adt->action, adt->remap, ctime);
			return;
		}
	}
	
	/* only continue if there are strips to evaluate */
	if (estrips.first == NULL)
		return;
	
	
	/* 2. for each strip, evaluate then accumulate on top of existing channels, but don't set values yet */
	for (nes= estrips.first; nes; nes= nes->next) 
		nlastrip_evaluate(ptr, echannels, NULL, nes);
		
	/* 3. free temporary evaluation data that's not used elsewhere */
	BLI_freelistN(&estrips);
}

/* NLA Evaluation function (mostly for use through do_animdata) 
 *	- All channels that will be affected are not cleared anymore. Instead, we just evaluate into 
 *		some temp channels, where values can be accumulated in one go.
 */
static void animsys_calculate_nla (PointerRNA *ptr, AnimData *adt, float ctime)
{
	ListBase echannels= {NULL, NULL};
	
	// TODO: need to zero out all channels used, otherwise we have problems with threadsafety
	// and also when the user jumps between different times instead of moving sequentially...
	
	/* evaluate the NLA stack, obtaining a set of values to flush */
	animsys_evaluate_nla(&echannels, ptr, adt, ctime);
	
	/* flush effects of accumulating channels in NLA to the actual data they affect */
	nladata_flush_channels(&echannels);
	
	/* free temp data */
	BLI_freelistN(&echannels);
}

/* ***************************************** */ 
/* Overrides System - Public API */

/* Clear all overides */

/* Add or get existing Override for given setting */
#if 0
AnimOverride *BKE_animsys_validate_override (PointerRNA *UNUSED(ptr), char *UNUSED(path), int UNUSED(array_index))
{
	// FIXME: need to define how to get overrides
	return NULL;
} 
#endif

/* -------------------- */

/* Evaluate Overrides */
static void animsys_evaluate_overrides (PointerRNA *ptr, AnimData *adt)
{
	AnimOverride *aor;
	
	/* for each override, simply execute... */
	for (aor= adt->overrides.first; aor; aor= aor->next)
		animsys_write_rna_setting(ptr, aor->rna_path, aor->array_index, aor->value);
}

/* ***************************************** */
/* Evaluation System - Public API */

/* Overview of how this system works:
 *	1) Depsgraph sorts data as necessary, so that data is in an order that means 
 *		that all dependencies are resolved before dependants.
 *	2) All normal animation is evaluated, so that drivers have some basis values to
 *		work with
 *		a.	NLA stacks are done first, as the Active Actions act as 'tweaking' tracks
 *			which modify the effects of the NLA-stacks
 *		b.	Active Action is evaluated as per normal, on top of the results of the NLA tracks
 *
 * --------------< often in a separate phase... >------------------ 
 *
 *	3) Drivers/expressions are evaluated on top of this, in an order where dependencies are
 *		resolved nicely. 
 *	   Note: it may be necessary to have some tools to handle the cases where some higher-level
 *		drivers are added and cause some problematic dependencies that didn't exist in the local levels...
 *
 * --------------< always executed >------------------ 
 *
 * Maintenance of editability of settings (XXX):
 *	In order to ensure that settings that are animated can still be manipulated in the UI without requiring
 *	that keyframes are added to prevent these values from being overwritten, we use 'overrides'. 
 *
 * Unresolved things:
 *	- Handling of multi-user settings (i.e. time-offset, group-instancing) -> big cache grids or nodal system? but stored where?
 *	- Multiple-block dependencies (i.e. drivers for settings are in both local and higher levels) -> split into separate lists? 
 *
 * Current Status:
 *	- Currently (as of September 2009), overrides we haven't needed to (fully) implement overrides. 
 * 	  However, the code fo this is relatively harmless, so is left in the code for now.
 */

/* Evaluation loop for evaluation animation data 
 *
 * This assumes that the animation-data provided belongs to the ID block in question,
 * and that the flags for which parts of the anim-data settings need to be recalculated 
 * have been set already by the depsgraph. Now, we use the recalc 
 */
void BKE_animsys_evaluate_animdata (Scene *scene, ID *id, AnimData *adt, float ctime, short recalc)
{
	PointerRNA id_ptr;
	
	/* sanity checks */
	if ELEM(NULL, id, adt)
		return;
	
	/* get pointer to ID-block for RNA to use */
	RNA_id_pointer_create(id, &id_ptr);
	
	/* recalculate keyframe data:
	 *	- NLA before Active Action, as Active Action behaves as 'tweaking track'
	 *	  that overrides 'rough' work in NLA
	 */
	// TODO: need to double check that this all works correctly
	if ((recalc & ADT_RECALC_ANIM) || (adt->recalc & ADT_RECALC_ANIM))
	{
		/* evaluate NLA data */
		if ((adt->nla_tracks.first) && !(adt->flag & ADT_NLA_EVAL_OFF))
		{
			/* evaluate NLA-stack 
			 *	- active action is evaluated as part of the NLA stack as the last item
			 */
			animsys_calculate_nla(&id_ptr, adt, ctime);
		}
		/* evaluate Active Action only */
		else if (adt->action)
			animsys_evaluate_action(&id_ptr, adt->action, adt->remap, ctime);
		
		/* reset tag */
		adt->recalc &= ~ADT_RECALC_ANIM;
	}
	
	/* recalculate drivers 
	 *	- Drivers need to be evaluated afterwards, as they can either override 
	 *	  or be layered on top of existing animation data.
	 *	- Drivers should be in the appropriate order to be evaluated without problems...
	 */
	if ((recalc & ADT_RECALC_DRIVERS) /*&& (adt->recalc & ADT_RECALC_DRIVERS)*/) // XXX for now, don't check yet, as depsgraph hasn't been updated
	{
		animsys_evaluate_drivers(&id_ptr, adt, ctime);
	}
	
	/* always execute 'overrides' 
	 *	- Overrides allow editing, by overwriting the value(s) set from animation-data, with the
	 *	  value last set by the user (and not keyframed yet). 
	 *	- Overrides are cleared upon frame change and/or keyframing
	 *	- It is best that we execute this everytime, so that no errors are likely to occur.
	 */
	animsys_evaluate_overrides(&id_ptr, adt);
	
	/* execute and clear all cached property update functions */
	if (scene)
	{
		Main *bmain = G.main; // xxx - to get passed in!
		RNA_property_update_cache_flush(bmain, scene);
		RNA_property_update_cache_free();
	}
	
	/* clear recalc flag now */
	adt->recalc= 0;
}

/* Evaluation of all ID-blocks with Animation Data blocks - Animation Data Only
 *
 * This will evaluate only the animation info available in the animation data-blocks
 * encountered. In order to enforce the system by which some settings controlled by a
 * 'local' (i.e. belonging in the nearest ID-block that setting is related to, not a
 * standard 'root') block are overridden by a larger 'user'
 */
void BKE_animsys_evaluate_all_animation (Main *main, Scene *scene, float ctime)
{
	ID *id;

	if (G.f & G_DEBUG)
		printf("Evaluate all animation - %f \n", ctime);
	
	/* macros for less typing 
	 *	- only evaluate animation data for id if it has users (and not just fake ones)
	 *	- whether animdata exists is checked for by the evaluation function, though taking 
	 *	  this outside of the function may make things slightly faster?
	 */
#define EVAL_ANIM_IDS(first, aflag) \
	for (id= first; id; id= id->next) { \
		if (ID_REAL_USERS(id) > 0) { \
			AnimData *adt= BKE_animdata_from_id(id); \
			BKE_animsys_evaluate_animdata(scene, id, adt, ctime, aflag); \
		} \
	}
	/* another macro for the "embedded" nodetree cases 
	 *	- this is like EVAL_ANIM_IDS, but this handles the case "embedded nodetrees" 
	 *	  (i.e. scene/material/texture->nodetree) which we need a special exception
	 * 	  for, otherwise they'd get skipped
	 *	- ntp = "node tree parent" = datablock where node tree stuff resides
	 */
#define EVAL_ANIM_NODETREE_IDS(first, NtId_Type, aflag) \
	for (id= first; id; id= id->next) { \
		if (ID_REAL_USERS(id) > 0) { \
			AnimData *adt= BKE_animdata_from_id(id); \
			NtId_Type *ntp= (NtId_Type *)id; \
			if (ntp->nodetree) { \
				AnimData *adt2= BKE_animdata_from_id((ID *)ntp->nodetree); \
				BKE_animsys_evaluate_animdata(scene, (ID *)ntp->nodetree, adt2, ctime, ADT_RECALC_ANIM); \
			} \
			BKE_animsys_evaluate_animdata(scene, id, adt, ctime, aflag); \
		} \
	}
	
	/* optimisation: 
	 * when there are no actions, don't go over database and loop over heaps of datablocks, 
	 * which should ultimately be empty, since it is not possible for now to have any animation 
	 * without some actions, and drivers wouldn't get affected by any state changes
	 *
	 * however, if there are some curves, we will need to make sure that their 'ctime' property gets
	 * set correctly, so this optimisation must be skipped in that case...
	 */
	if ((main->action.first == NULL) && (main->curve.first == NULL)) {
		if (G.f & G_DEBUG)
			printf("\tNo Actions, so no animation needs to be evaluated...\n");
			
		return;
	}
	
	/* nodes */
	EVAL_ANIM_IDS(main->nodetree.first, ADT_RECALC_ANIM);
	
	/* textures */
	EVAL_ANIM_NODETREE_IDS(main->tex.first, Tex, ADT_RECALC_ANIM);
	
	/* lamps */
	EVAL_ANIM_NODETREE_IDS(main->lamp.first, Lamp, ADT_RECALC_ANIM);
	
	/* materials */
	EVAL_ANIM_NODETREE_IDS(main->mat.first, Material, ADT_RECALC_ANIM);
	
	/* cameras */
	EVAL_ANIM_IDS(main->camera.first, ADT_RECALC_ANIM);
	
	/* shapekeys */
	EVAL_ANIM_IDS(main->key.first, ADT_RECALC_ANIM);
	
	/* metaballs */
	EVAL_ANIM_IDS(main->mball.first, ADT_RECALC_ANIM);
	
	/* curves */
	EVAL_ANIM_IDS(main->curve.first, ADT_RECALC_ANIM);
	
	/* armatures */
	EVAL_ANIM_IDS(main->armature.first, ADT_RECALC_ANIM);
	
	/* lattices */
	EVAL_ANIM_IDS(main->latt.first, ADT_RECALC_ANIM);
	
	/* meshes */
	EVAL_ANIM_IDS(main->mesh.first, ADT_RECALC_ANIM);
	
	/* particles */
	EVAL_ANIM_IDS(main->particle.first, ADT_RECALC_ANIM);
	
	/* speakers */
	EVAL_ANIM_IDS(main->speaker.first, ADT_RECALC_ANIM);

	/* movie clips */
	EVAL_ANIM_IDS(main->movieclip.first, ADT_RECALC_ANIM);

	/* objects */
		/* ADT_RECALC_ANIM doesn't need to be supplied here, since object AnimData gets 
		 * this tagged by Depsgraph on framechange. This optimisation means that objects
		 * linked from other (not-visible) scenes will not need their data calculated.
		 */
	EVAL_ANIM_IDS(main->object.first, 0); 
	
	/* worlds */
	EVAL_ANIM_NODETREE_IDS(main->world.first, World, ADT_RECALC_ANIM);
	
	/* scenes */
	EVAL_ANIM_NODETREE_IDS(main->scene.first, Scene, ADT_RECALC_ANIM);
}

/* ***************************************** */ 
