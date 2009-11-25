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
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joshua Leung (full recode)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <float.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_dynstr.h"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_fcurve.h"
#include "BKE_nla.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "nla_private.h"

/* ***************************************** */
/* AnimData API */

/* Getter/Setter -------------------------------------------- */

/* Internal utility to check if ID can have AnimData */
static short id_has_animdata (ID *id)
{
	/* sanity check */
	if (id == NULL)
		return 0;
		
	/* Only some ID-blocks have this info for now */
	// TODO: finish adding this for the other blocktypes
	switch (GS(id->name)) {
			/* has AnimData */
		case ID_OB:
		case ID_MB: case ID_CU: case ID_AR:
		case ID_KE:
		case ID_PA:
		case ID_MA: case ID_TE: case ID_NT:
		case ID_LA: case ID_CA: case ID_WO:
		case ID_SCE:
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
	if (id_has_animdata(id)) {
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
	if (id_has_animdata(id)) {
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

/* Freeing -------------------------------------------- */

/* Free AnimData used by the nominated ID-block, and clear ID-block's AnimData pointer */
void BKE_free_animdata (ID *id)
{
	/* Only some ID-blocks have this info for now, so we cast the 
	 * types that do to be of type IdAdtTemplate
	 */
	if (id_has_animdata(id)) {
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
AnimData *BKE_copy_animdata (AnimData *adt)
{
	AnimData *dadt;
	
	/* sanity check before duplicating struct */
	if (adt == NULL)
		return NULL;
	dadt= MEM_dupallocN(adt);
	
	/* make a copy of action - at worst, user has to delete copies... */
	dadt->action= copy_action(adt->action);
	dadt->tmpact= copy_action(adt->tmpact);
	
	/* duplicate NLA data */
	copy_nladata(&dadt->nla_tracks, &adt->nla_tracks);
	
	/* duplicate drivers (F-Curves) */
	copy_fcurves(&dadt->drivers, &adt->drivers);
	
	/* don't copy overrides */
	dadt->overrides.first= dadt->overrides.last= NULL;
	
	/* return */
	return dadt;
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

/* Path Validation -------------------------------------------- */

/* Check if a given RNA Path is valid, by tracing it from the given ID, and seeing if we can resolve it */
static short check_rna_path_is_valid (ID *owner_id, char *path)
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
static char *rna_path_rename_fix (ID *owner_id, char *prefix, char *oldName, char *newName, char *oldpath)
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
		if (check_rna_path_is_valid(owner_id, oldpath) == 0) {		
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
			if (check_rna_path_is_valid(owner_id, newPath)) {
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
static void fcurves_path_rename_fix (ID *owner_id, char *prefix, char *oldName, char *newName, ListBase *curves)
{
	FCurve *fcu;
	
	/* we need to check every curve... */
	for (fcu= curves->first; fcu; fcu= fcu->next) {
		/* firstly, handle the F-Curve's own path */
		fcu->rna_path= rna_path_rename_fix(owner_id, prefix, oldName, newName, fcu->rna_path);
		
		/* driver? */
		if (fcu->driver) {
			ChannelDriver *driver= fcu->driver;
			DriverTarget *dtar;
			
			/* driver targets */
			for (dtar= driver->targets.first; dtar; dtar=dtar->next) {
				dtar->rna_path= rna_path_rename_fix(dtar->id, prefix, oldName, newName, dtar->rna_path);
			}
		}
	}
}

/* Fix all RNA-Paths for Actions linked to NLA Strips */
static void nlastrips_path_rename_fix (ID *owner_id, char *prefix, char *oldName, char *newName, ListBase *strips)
{
	NlaStrip *strip;
	
	/* recursively check strips, fixing only actions... */
	for (strip= strips->first; strip; strip= strip->next) {
		/* fix strip's action */
		if (strip->act)
			fcurves_path_rename_fix(owner_id, prefix, oldName, newName, &strip->act->curves);
		/* ignore own F-Curves, since those are local...  */
		
		/* check sub-strips (if metas) */
		nlastrips_path_rename_fix(owner_id, prefix, oldName, newName, &strip->strips);
	}
}

/* Fix all RNA-Paths in the AnimData block used by the given ID block
 * NOTE: it is assumed that the structure we're replacing is <prefix><["><name><"]>
 * 		i.e. pose.bones["Bone"]
 */
void BKE_animdata_fix_paths_rename (ID *owner_id, AnimData *adt, char *prefix, char *oldName, char *newName)
{
	NlaTrack *nlt;
	char *oldN, *newN;
	
	/* if no AnimData, no need to proceed */
	if (ELEM4(NULL, owner_id, adt, oldName, newName))
		return;
	
	/* pad the names with [" "] so that only exact matches are made */
	oldN= BLI_sprintfN("[\"%s\"]", oldName);
	newN= BLI_sprintfN("[\"%s\"]", newName);
	
	/* Active action and temp action */
	if (adt->action)
		fcurves_path_rename_fix(owner_id, prefix, oldN, newN, &adt->action->curves);
	if (adt->tmpact)
		fcurves_path_rename_fix(owner_id, prefix, oldN, newN, &adt->tmpact->curves);
		
	/* Drivers - Drivers are really F-Curves */
	fcurves_path_rename_fix(owner_id, prefix, oldN, newN, &adt->drivers);
	
	/* NLA Data - Animation Data for Strips */
	for (nlt= adt->nla_tracks.first; nlt; nlt= nlt->next)
		nlastrips_path_rename_fix(owner_id, prefix, oldN, newN, &nlt->strips);
		
	/* free the temp names */
	MEM_freeN(oldN);
	MEM_freeN(newN);
}

/* Fix all RNA-Paths throughout the database (directly access the Global.main version)
 * NOTE: it is assumed that the structure we're replacing is <prefix><["><name><"]>
 * 		i.e. pose.bones["Bone"]
 */
void BKE_all_animdata_fix_paths_rename (char *prefix, char *oldName, char *newName)
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
		BKE_animdata_fix_paths_rename(id, adt, prefix, oldName, newName);\
	}
	
	/* nodes */
	RENAMEFIX_ANIM_IDS(mainptr->nodetree.first);
	
	/* textures */
	RENAMEFIX_ANIM_IDS(mainptr->tex.first);
	
	/* lamps */
	RENAMEFIX_ANIM_IDS(mainptr->lamp.first);
	
	/* materials */
	RENAMEFIX_ANIM_IDS(mainptr->mat.first);
	
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
	
	/* meshes */
	// TODO...
	
	/* particles */
	RENAMEFIX_ANIM_IDS(mainptr->particle.first);
	
	/* objects */
	RENAMEFIX_ANIM_IDS(mainptr->object.first); 
	
	/* worlds */
	RENAMEFIX_ANIM_IDS(mainptr->world.first);
	
	/* scenes */
	for (id= mainptr->scene.first; id; id= id->next) {
		AnimData *adt= BKE_animdata_from_id(id);
		Scene *scene= (Scene *)id;
		
		/* do compositing nodes first (since these aren't included in main tree) */
		if (scene->nodetree) {
			AnimData *adt2= BKE_animdata_from_id((ID *)scene->nodetree);
			BKE_animdata_fix_paths_rename((ID *)scene->nodetree, adt2, prefix, oldName, newName);
		}
		
		/* now fix scene animation data as per normal */
		BKE_animdata_fix_paths_rename((ID *)id, adt, prefix, oldName, newName);
	}
}

/* *********************************** */ 
/* KeyingSet API */

/* Finding Tools --------------------------- */

/* Find the first path that matches the given criteria */
// TODO: do we want some method to perform partial matches too?
KS_Path *BKE_keyingset_find_destination (KeyingSet *ks, ID *id, const char group_name[], const char rna_path[], int array_index, int group_mode)
{
	KS_Path *ksp;
	
	/* sanity checks */
	if ELEM(NULL, ks, rna_path)
		return NULL;
	
	/* ID is optional for relative KeyingSets, but is necessary for absolute KeyingSets */
	if (id == NULL) {
		if (ks->flag & KEYINGSET_ABSOLUTE)
			return NULL;
	}
	
	/* loop over paths in the current KeyingSet, finding the first one where all settings match 
	 * (i.e. the first one where none of the checks fail and equal 0)
	 */
	for (ksp= ks->paths.first; ksp; ksp= ksp->next) {
		short eq_id=1, eq_path=1, eq_index=1, eq_group=1;
		
		/* id */
		if ((ks->flag & KEYINGSET_ABSOLUTE) && (id != ksp->id))
			eq_id= 0;
		
		/* path */
		if ((ksp->rna_path==0) || strcmp(rna_path, ksp->rna_path))
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
	
	if (name)
		BLI_snprintf(ks->name, 64, name);
	else
		strcpy(ks->name, "KeyingSet");
	
	ks->flag= flag;
	ks->keyingflag= keyingflag;
	
	/* add KeyingSet to list */
	BLI_addtail(list, ks);
	
	/* make sure KeyingSet has a unique name (this helps with identification) */
	BLI_uniquename(list, ks, "KeyingSet", '.', offsetof(KeyingSet, name), 64);
	
	/* return new KeyingSet for further editing */
	return ks;
}

/* Add a destination to a KeyingSet. Nothing is returned for now...
 * Checks are performed to ensure that destination is appropriate for the KeyingSet in question
 */
void BKE_keyingset_add_destination (KeyingSet *ks, ID *id, const char group_name[], const char rna_path[], int array_index, short flag, short groupmode)
{
	KS_Path *ksp;
	
	/* sanity checks */
	if ELEM(NULL, ks, rna_path) {
		printf("ERROR: no Keying Set and/or RNA Path to add destination with \n");
		return;
	}
	
	/* ID is optional for relative KeyingSets, but is necessary for absolute KeyingSets */
	if (id == NULL) {
		if (ks->flag & KEYINGSET_ABSOLUTE) {
			printf("ERROR: No ID provided for absolute destination. \n");
			return;
		}
	}
	
	/* don't add if there is already a matching KS_Path in the KeyingSet */
	if (BKE_keyingset_find_destination(ks, id, group_name, rna_path, array_index, groupmode)) {
		if (G.f & G_DEBUG)
			printf("ERROR: destination already exists in Keying Set \n");
		return;
	}
	
	/* allocate a new KeyingSet Path */
	ksp= MEM_callocN(sizeof(KS_Path), "KeyingSet Path");
	
	/* just store absolute info */
	if (ks->flag & KEYINGSET_ABSOLUTE) {
		ksp->id= id;
		if (group_name)
			BLI_snprintf(ksp->group, 64, group_name);
		else
			strcpy(ksp->group, "");
	}
	
	/* store additional info for relative paths (just in case user makes the set relative) */
	if (id)
		ksp->idtype= GS(id->name);
	
	/* just copy path info */
	// XXX no checks are performed for templates yet
	// should array index be checked too?
	ksp->rna_path= BLI_strdupn(rna_path, strlen(rna_path));
	ksp->array_index= array_index;
	
	/* store flags */
	ksp->flag= flag;
	ksp->groupmode= groupmode;
	
	/* add KeyingSet path to KeyingSet */
	BLI_addtail(&ks->paths, ksp);
}	

/* Copy all KeyingSets in the given list */
void BKE_keyingsets_copy(ListBase *newlist, ListBase *list)
{
	KeyingSet *ksn;
	KS_Path *kspn;

	BLI_duplicatelist(newlist, list);

	for(ksn=newlist->first; ksn; ksn=ksn->next) {
		BLI_duplicatelist(&ksn->paths, &ksn->paths);

		for(kspn=ksn->paths.first; kspn; kspn=kspn->next)
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
		
		/* free RNA-path info */
		MEM_freeN(ksp->rna_path);
		
		/* free path itself */
		BLI_freelinkN(&ks->paths, ksp);
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
static short animsys_remap_path (AnimMapper *remap, char *path, char **dst)
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


/* Write the given value to a setting using RNA, and return success */
static short animsys_write_rna_setting (PointerRNA *ptr, char *path, int array_index, float value)
{
	PropertyRNA *prop;
	PointerRNA new_ptr;
	
	/* get property to write to */
	if (RNA_path_resolve(ptr, path, &new_ptr, &prop)) 
	{
		/* set value - only for animatable numerical values */
		if (RNA_property_animateable(&new_ptr, prop)) 
		{
			switch (RNA_property_type(prop)) 
			{
				case PROP_BOOLEAN:
					if (RNA_property_array_length(&new_ptr, prop)) {
						if (RNA_property_editable_index(&new_ptr, prop, array_index))
							RNA_property_boolean_set_index(&new_ptr, prop, array_index, (int)value);
					}
					else
						RNA_property_boolean_set(&new_ptr, prop, (int)value);
					break;
				case PROP_INT:
					if (RNA_property_array_length(&new_ptr, prop)){
						if (RNA_property_editable_index(&new_ptr, prop, array_index))
							RNA_property_int_set_index(&new_ptr, prop, array_index, (int)value);
					}
					else
						RNA_property_int_set(&new_ptr, prop, (int)value);
					break;
				case PROP_FLOAT:
					if (RNA_property_array_length(&new_ptr, prop)) {
						if (RNA_property_editable_index(&new_ptr, prop, array_index))
							RNA_property_float_set_index(&new_ptr, prop, array_index, value);
					}
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
		}
		
		/* successful */
		return 1;
	}
	else {
		/* failed to get path */
		// XXX don't tag as failed yet though, as there are some legit situations (Action Constraint) 
		// where some channels will not exist, but shouldn't lock up Action
		if (G.f & G_DEBUG) {
			printf("Animato: Invalid path. ID = '%s',  '%s [%d]' \n", 
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

/* Evaluate Action Group */
void animsys_evaluate_action_group (PointerRNA *ptr, bAction *act, bActionGroup *agrp, AnimMapper *remap, float ctime)
{
	FCurve *fcu;
	
	/* check if mapper is appropriate for use here (we set to NULL if it's inappropriate) */
	if ELEM(NULL, act, agrp) return;
	if ((remap) && (remap->target != act)) remap= NULL;
	
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
	
	/* calculate then execute each curve */
	animsys_evaluate_fcurves(ptr, &act->curves, remap, ctime);
}

/* ***************************************** */
/* NLA System - Evaluation */

/* calculate influence of strip based for given frame based on blendin/out values */
static float nlastrip_get_influence (NlaStrip *strip, float cframe)
{
	/* sanity checks - normalise the blendin/out values? */
	strip->blendin= (float)fabs(strip->blendin);
	strip->blendout= (float)fabs(strip->blendout);
	
	/* result depends on where frame is in respect to blendin/out values */
	if (IS_EQ(strip->blendin, 0)==0 && (cframe <= (strip->start + strip->blendin))) {
		/* there is some blend-in */
		return (float)fabs(cframe - strip->start) / (strip->blendin);
	}
	else if (IS_EQ(strip->blendout, 0)==0 && (cframe >= (strip->end - strip->blendout))) {
		/* there is some blend-out */
		return (float)fabs(strip->end - cframe) / (strip->blendout);
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
	short free_path=0;
	
	/* sanity checks */
	if (channels == NULL)
		return NULL;
	
	/* get RNA pointer+property info from F-Curve for more convenient handling */
		/* get path, remapped as appropriate to work in its new environment */
	free_path= animsys_remap_path(strip->remap, fcu->rna_path, &path);
	
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
				if (RNA_property_array_length(ptr, prop)) {
					if (RNA_property_editable_index(ptr, prop, array_index))
						RNA_property_boolean_set_index(ptr, prop, array_index, (int)value);
				}
				else
					RNA_property_boolean_set(ptr, prop, (int)value);
				break;
			case PROP_INT:
				if (RNA_property_array_length(ptr, prop)) {
					if (RNA_property_editable_index(ptr, prop, array_index))
						RNA_property_int_set_index(ptr, prop, array_index, (int)value);
				}
				else
					RNA_property_int_set(ptr, prop, (int)value);
				break;
			case PROP_FLOAT:
				if (RNA_property_array_length(ptr, prop)) {
					if (RNA_property_editable_index(ptr, prop, array_index))
						RNA_property_float_set_index(ptr, prop, array_index, value);
				}
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

/* NLA Evaluation function (mostly for use through do_animdata) 
 *	- All channels that will be affected are not cleared anymore. Instead, we just evaluate into 
 *		some temp channels, where values can be accumulated in one go.
 */
static void animsys_evaluate_nla (PointerRNA *ptr, AnimData *adt, float ctime)
{
	ListBase dummy_trackslist = {NULL, NULL};
	NlaStrip dummy_strip;
	
	NlaTrack *nlt;
	short track_index=0;
	short has_strips = 0;
	
	ListBase estrips= {NULL, NULL};
	ListBase echannels= {NULL, NULL};
	NlaEvalStrip *nes;
	
	// TODO: need to zero out all channels used, otherwise we have problems with threadsafety
	// and also when the user jumps between different times instead of moving sequentially...
	
	/* 1. get the stack of strips to evaluate at current time (influence calculated here) */
	for (nlt=adt->nla_tracks.first; nlt; nlt=nlt->next, track_index++) { 
		/* if tweaking is on and this strip is the tweaking track, stop on this one */
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
		if (has_strips) {
			/* make dummy NLA strip, and add that to the stack */
			memset(&dummy_strip, 0, sizeof(NlaStrip));
			dummy_trackslist.first= dummy_trackslist.last= &dummy_strip;
			
			dummy_strip.act= adt->action;
			dummy_strip.remap= adt->remap;
			
			/* action range is calculated taking F-Modifiers into account (which making new strips doesn't do due to the troublesome nature of that) */
			calc_action_range(dummy_strip.act, &dummy_strip.actstart, &dummy_strip.actend, 1);
			dummy_strip.start = dummy_strip.actstart;
			dummy_strip.end = (IS_EQ(dummy_strip.actstart, dummy_strip.actend)) ?  (dummy_strip.actstart + 1.0f): (dummy_strip.actend);
			
			dummy_strip.blendmode= adt->act_blendmode;
			dummy_strip.extendmode= adt->act_extendmode;
			dummy_strip.influence= adt->act_influence;
			
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
		nlastrip_evaluate(ptr, &echannels, NULL, nes);
	
	/* 3. flush effects of accumulating channels in NLA to the actual data they affect */
	nladata_flush_channels(&echannels);
	
	/* 4. free temporary evaluation data */
	BLI_freelistN(&estrips);
	BLI_freelistN(&echannels);
}

/* ***************************************** */ 
/* Overrides System - Public API */

/* Clear all overides */

/* Add or get existing Override for given setting */
AnimOverride *BKE_animsys_validate_override (PointerRNA *ptr, char *path, int array_index)
{
	// FIXME: need to define how to get overrides
	return NULL;
} 

/* -------------------- */

/* Evaluate Overrides */
static void animsys_evaluate_overrides (PointerRNA *ptr, AnimData *adt, float ctime)
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
 *		that all dependences are resolved before dependants.
 *	2) All normal animation is evaluated, so that drivers have some basis values to
 *		work with
 *		a.	NLA stacks are done first, as the Active Actions act as 'tweaking' tracks
 *			which modify the effects of the NLA-stacks
 *		b.	Active Action is evaluated as per normal, on top of the results of the NLA tracks
 *
 * --------------< often in a separate phase... >------------------ 
 *
 *	3) Drivers/expressions are evaluated on top of this, in an order where dependences are
 *		resolved nicely. 
 *	   Note: it may be necessary to have some tools to handle the cases where some higher-level
 *		drivers are added and cause some problematic dependencies that didn't exist in the local levels...
 *
 * --------------< always executed >------------------ 
 *
 * Maintainance of editability of settings (XXX):
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
void BKE_animsys_evaluate_animdata (ID *id, AnimData *adt, float ctime, short recalc)
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
			animsys_evaluate_nla(&id_ptr, adt, ctime);
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
	animsys_evaluate_overrides(&id_ptr, adt, ctime);
	
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
void BKE_animsys_evaluate_all_animation (Main *main, float ctime)
{
	ID *id;
	
	if (G.f & G_DEBUG)
		printf("Evaluate all animation - %f \n", ctime);
	
	/* macro for less typing 
	 *	- only evaluate animation data for id if it has users (and not just fake ones)
	 *	- whether animdata exists is checked for by the evaluation function, though taking 
	 *	  this outside of the function may make things slightly faster?
	 */
#define EVAL_ANIM_IDS(first, aflag) \
	for (id= first; id; id= id->next) { \
		AnimData *adt= BKE_animdata_from_id(id); \
		if ( (id->us > 1) || (id->us && !(id->flag & LIB_FAKEUSER)) ) \
			BKE_animsys_evaluate_animdata(id, adt, ctime, aflag); \
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
	EVAL_ANIM_IDS(main->tex.first, ADT_RECALC_ANIM);
	
	/* lamps */
	EVAL_ANIM_IDS(main->lamp.first, ADT_RECALC_ANIM);
	
	/* materials */
	EVAL_ANIM_IDS(main->mat.first, ADT_RECALC_ANIM);
	
	/* cameras */
	EVAL_ANIM_IDS(main->camera.first, ADT_RECALC_ANIM);
	
	/* shapekeys */
		// TODO: we probably need the same hack as for curves (ctime-hack)
	EVAL_ANIM_IDS(main->key.first, ADT_RECALC_ANIM);
	
	/* metaballs */
	EVAL_ANIM_IDS(main->mball.first, ADT_RECALC_ANIM);
	
	/* curves */
		/* we need to perform a special hack here to ensure that the ctime 
		 * value of the curve gets set in case there's no animation for that
		 *	- it needs to be set before animation is evaluated just so that 
		 *	  animation can successfully override...
		 *	- it shouldn't get set when calculating drivers...
		 */
	for (id= main->curve.first; id; id= id->next) {
		AnimData *adt= BKE_animdata_from_id(id);
		Curve *cu= (Curve *)id;
		
		/* set ctime variable for curve */
		cu->ctime= ctime;
		
		/* now execute animation data on top of this as per normal */
		BKE_animsys_evaluate_animdata(id, adt, ctime, ADT_RECALC_ANIM);
	}
	
	/* armatures */
	EVAL_ANIM_IDS(main->armature.first, ADT_RECALC_ANIM);
	
	/* meshes */
	// TODO...
	
	/* particles */
	EVAL_ANIM_IDS(main->particle.first, ADT_RECALC_ANIM);
	
	/* objects */
		/* ADT_RECALC_ANIM doesn't need to be supplied here, since object AnimData gets 
		 * this tagged by Depsgraph on framechange. This optimisation means that objects
		 * linked from other (not-visible) scenes will not need their data calculated.
		 */
	EVAL_ANIM_IDS(main->object.first, 0); 
	
	/* worlds */
	EVAL_ANIM_IDS(main->world.first, ADT_RECALC_ANIM);
	
	/* scenes */
	for (id= main->scene.first; id; id= id->next) {
		AnimData *adt= BKE_animdata_from_id(id);
		Scene *scene= (Scene *)id;
		
		/* do compositing nodes first (since these aren't included in main tree) */
		if (scene->nodetree) {
			AnimData *adt2= BKE_animdata_from_id((ID *)scene->nodetree);
			BKE_animsys_evaluate_animdata((ID *)scene->nodetree, adt2, ctime, ADT_RECALC_ANIM);
		}
		
		/* now execute scene animation data as per normal */
		BKE_animsys_evaluate_animdata(id, adt, ctime, ADT_RECALC_ANIM);
	}
}

/* ***************************************** */ 
