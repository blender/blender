/* Testing code for new animation system in 2.5 
 * Copyright 2009, Joshua Leung
 */

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <float.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_dynstr.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_fcurve.h"
#include "BKE_nla.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "DNA_anim_types.h"

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
		case ID_CU:
		case ID_KE:
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
		if (iat->adt == NULL)
			iat->adt= MEM_callocN(sizeof(AnimData), "AnimData");
		
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
	// XXX review this... it might not be optimal behaviour yet...
	//id_us_plus((ID *)dadt->action);
	dadt->action= copy_action(adt->action);
	
	/* duplicate NLA data */
	copy_nladata(&dadt->nla_tracks, &adt->nla_tracks);
	
	/* duplicate drivers (F-Curves) */
	copy_fcurves(&dadt->drivers, &adt->drivers);
	
	/* don't copy overrides */
	dadt->overrides.first= dadt->overrides.last= NULL;
	
	/* return */
	return dadt;
}

/* *********************************** */ 
/* KeyingSet API */

/* NOTES:
 * It is very likely that there will be two copies of the api - one for internal use,
 * and one 'operator' based wrapper of the internal API, which should allow for access
 * from Python/scripts so that riggers can automate the creation of KeyingSets for their rigs.
 */

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
			
		/* index */
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
		strcpy(ks->name, "Keying Set");
	
	ks->flag= flag;
	ks->keyingflag= keyingflag;
	
	/* add KeyingSet to list */
	BLI_addtail(list, ks);
	
	/* make sure KeyingSet has a unique name (this helps with identification) */
	BLI_uniquename(list, ks, "Keying Set", ' ', offsetof(KeyingSet, name), 64);
	
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
	if ELEM(NULL, ks, rna_path)
		return;
	
	/* ID is optional for relative KeyingSets, but is necessary for absolute KeyingSets */
	if (id == NULL) {
		if (ks->flag & KEYINGSET_ABSOLUTE)
			return;
	}
	
	/* don't add if there is already a matching KS_Path in the KeyingSet */
	if (BKE_keyingset_find_destination(ks, id, group_name, rna_path, array_index, groupmode))
		return;
	
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
short animsys_remap_path (AnimMapper *remap, char *path, char **dst)
{
	/* is there a valid remapping table to use? */
	if (remap) {
		/* find a matching entry... to use to remap */
		// ...TODO...
	}
	
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
					if (RNA_property_array_length(prop))
						RNA_property_boolean_set_index(&new_ptr, prop, array_index, (int)value);
					else
						RNA_property_boolean_set(&new_ptr, prop, (int)value);
					break;
				case PROP_INT:
					if (RNA_property_array_length(prop))
						RNA_property_int_set_index(&new_ptr, prop, array_index, (int)value);
					else
						RNA_property_int_set(&new_ptr, prop, (int)value);
					break;
				case PROP_FLOAT:
					if (RNA_property_array_length(prop))
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
		/* check if this curve should be skipped */
		if ((fcu->flag & (FCURVE_MUTED|FCURVE_DISABLED)) == 0) 
		{
			calculate_fcurve(fcu, ctime);
			animsys_execute_fcurve(ptr, remap, fcu); 
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
		// FIXME: maybe we shouldn't check for muted, though that would make things more confusing, as there's already too many ways to disable?
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

/* used for list of strips to accumulate at current time */
typedef struct NlaEvalStrip {
	struct NlaEvalStrip *next, *prev;
	
	NlaTrack *track;			/* track that this strip belongs to */
	NlaStrip *strip;			/* strip that's being used */
	
	short track_index;			/* the index of the track within the list */
	short strip_mode;			/* which end of the strip are we looking at */
} NlaEvalStrip;

/* NlaEvalStrip->strip_mode */
enum {
	NES_TIME_BEFORE = -1,
	NES_TIME_WITHIN,
	NES_TIME_AFTER,
} eNlaEvalStrip_StripMode;


/* temp channel for accumulating data from NLA (avoids needing to clear all values first) */
// TODO: maybe this will be used as the 'cache' stuff needed for editable values too?
typedef struct NlaEvalChannel {
	struct NlaEvalChannel *next, *prev;
	
	PointerRNA ptr;			/* pointer to struct containing property to use */
	PropertyRNA *prop;		/* RNA-property type to use (should be in the struct given) */
	int index;				/* array index (where applicable) */
	
	float value;			/* value of this channel */
} NlaEvalChannel;


/* ---------------------- */

/* non clipped mapping for strip-time <-> global time 
 *	invert = convert action-strip time to global time 
 */
static float nlastrip_get_frame (NlaStrip *strip, float cframe, short invert)
{
	float length, actlength, repeat, scale;
	
	/* get number of repeats */
	if (strip->repeat == 0.0f) strip->repeat = 1.0f;
	repeat = strip->repeat;
	
	/* scaling */
	if (strip->scale == 0.0f) strip->scale= 1.0f;
	scale = (float)fabs(strip->scale); /* scale must be positive - we've got a special flag for reversing */
	
	/* length of referenced action */
	actlength = strip->actend-strip->actstart;
	if (actlength == 0.0f) actlength = 1.0f;
	
	/* length of strip */
	length = repeat * scale * actlength;
	
	/* reversed = play strip backwards */
	if (strip->flag & NLASTRIP_FLAG_REVERSE) {
		// FIXME: verify these 
		/* invert = convert action-strip time to global time */
		if (invert)
			return length*(strip->actend - cframe)/(repeat*actlength) + strip->start;
		else
			return strip->actend - repeat*actlength*(cframe - strip->start)/length;
	}
	else {
		/* invert = convert action-strip time to global time */
		if (invert)
			return length*(cframe - strip->actstart)/(repeat*actlength) + strip->start;
		else
			return repeat*actlength*(cframe - strip->start)/length + strip->actstart;
	}
}

/* calculate influence of strip based for given frame based on blendin/out values */
static float nlastrip_get_influence (NlaStrip *strip, float cframe)
{
	/* sanity checks - normalise the blendin/out values? */
	strip->blendin= (float)fabs(strip->blendin);
	strip->blendout= (float)fabs(strip->blendout);
	
	/* result depends on where frame is in respect to blendin/out values */
	// the +0.0001 factors are to combat rounding errors
	if (IS_EQ(strip->blendin, 0)==0 && (cframe <= (strip->start + strip->blendin))) {
		/* there is some blend-in */
		return (float)(fabs(cframe - strip->start) + 0.0001) / (strip->blendin);
	}
	else if (IS_EQ(strip->blendout, 0)==0 && (cframe >= (strip->end - strip->blendout))) {
		/* there is some blend-out */
		return (float)(fabs(strip->end - cframe) + 0.0001) / (strip->blendout);
	}
	else {
		/* in the middle of the strip, we should be full strength */
		return 1.0f;
	}
}

/* evaluate the evaluation time and influence for the strip, storing the results in the strip */
void nlastrip_evaluate_controls (NlaStrip *strip, float ctime)
{
	/* firstly, analytically generate values for influence and time (if applicable) */
	if ((strip->flag & NLASTRIP_FLAG_USR_TIME) == 0)
		strip->strip_time= nlastrip_get_frame(strip, ctime, 1); /* last arg '1' means current time to 'strip'/action time */
	if ((strip->flag & NLASTRIP_FLAG_USR_INFLUENCE) == 0)
		strip->influence= nlastrip_get_influence(strip, ctime);
	
	/* now strip's evaluate F-Curves for these settings (if applicable) */
	if (strip->fcurves.first) {
#if 0
		PointerRNA strip_ptr;
		FCurve *fcu;
		
		/* create RNA-pointer needed to set values */
		RNA_pointer_create(NULL, &RNA_NlaStrip, strip, &strip_ptr);
		
		/* execute these settings as per normal */
		animsys_evaluate_fcurves(&actstrip_ptr, &strip->fcurves, NULL, ctime);
#endif
	}
}


/* gets the strip active at the current time for a track for evaluation purposes */
static void nlatrack_ctime_get_strip (ListBase *list, NlaTrack *nlt, short index, float ctime)
{
	NlaStrip *strip, *estrip=NULL;
	NlaEvalStrip *nes;
	short side= 0;
	
	/* skip if track is muted */
	if (nlt->flag & NLATRACK_MUTED) 
		return;
	
	/* loop over strips, checking if they fall within the range */
	for (strip= nlt->strips.first; strip; strip= strip->next) {
		/* check if current time occurs within this strip  */
		if (IN_RANGE(ctime, strip->start, strip->end)) {
			/* this strip is active, so try to use it */
			estrip= strip;
			side= NES_TIME_WITHIN;
			break;
		}
		
		/* if time occurred before current strip... */
		if (ctime < strip->start) {
			if (strip == nlt->strips.first) {
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
			if (strip == nlt->strips.last) {
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
		return;
	
	/* evaluate strip's evaluation controls  
	 * 	- skip if no influence (i.e. same effect as muting the strip)
	 *	- negative influence is not supported yet... how would that be defined?
	 */
	// TODO: this sounds a bit hacky having a few isolated F-Curves stuck on some data it operates on...
	// TODO: should we clamp the time to only be on the endpoints of the strip?
	nlastrip_evaluate_controls(estrip, ctime);
	if (estrip->influence <= 0.0f) // XXX is it useful to invert the strip?
		return;
		
	/* check if strip has valid data to evaluate */
	switch (estrip->type) {
		case NLASTRIP_TYPE_CLIP: 
			/* clip must have some action to evaluate */
			if (estrip->act == NULL)
				return;
			break;
		case NLASTRIP_TYPE_TRANSITION:
			/* there must be strips to transition from and to (i.e. prev and next required) */
			// TODO: what happens about cross-track transitions? 
			if (ELEM(NULL, estrip->prev, estrip->next))
				return;
			break;
	}
	
	/* add to list of strips we need to evaluate */
	nes= MEM_callocN(sizeof(NlaEvalStrip), "NlaEvalStrip");
	
	nes->track= nlt;
	nes->strip= estrip;
	nes->strip_mode= side;
	nes->track_index= index;
	
	BLI_addtail(list, nes);
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
	
	/* we can now return */
	return nec;
}

/* accumulate (i.e. blend) the given value on to the channel it affects */
static void nlaevalchan_accumulate (NlaEvalChannel *nec, NlaEvalStrip *nes, short newChan, float value)
{
	NlaStrip *strip= nes->strip;
	float inf= strip->influence;
	
	/* if channel is new, just store value regardless of blending factors, etc. */
	if (newChan) {
		nec->value= value;
		return;
	}
	
	/* premultiply the value by the weighting factor */
	if (IS_EQ(inf, 0)) return;
	value *= inf;
	
	/* perform blending */
	switch (strip->blendmode) {
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
		
		case NLASTRIP_MODE_BLEND:
		default: // TODO: do we really want to blend by default? it seems more uses might prefer add...
			/* do linear interpolation 
			 *	- the influence of the accumulated data (elsewhere, that is called dstweight) 
			 *	  is 1 - influence, since the strip's influence is srcweight
			 */
			nec->value= nec->value * (1.0f - inf)   +   value;
			break;
	}
}

/* ---------------------- */

/* evaluate action-clip strip */
static void nlastrip_evaluate_actionclip (PointerRNA *ptr, ListBase *channels, NlaEvalStrip *nes)
{
	NlaStrip *strip= nes->strip;
	FCurve *fcu;
	float evaltime;
	
	/* evaluate strip's modifiers which modify time to evaluate the base curves at */
	evaltime= evaluate_time_fmodifiers(&strip->modifiers, NULL, 0.0f, strip->strip_time);
	
	/* evaluate all the F-Curves in the action, saving the relevant pointers to data that will need to be used */
	for (fcu= strip->act->curves.first; fcu; fcu= fcu->next) {
		NlaEvalChannel *nec;
		float value = 0.0f;
		short newChan = -1;
		
		/* check if this curve should be skipped */
		if (fcu->flag & (FCURVE_MUTED|FCURVE_DISABLED)) 
			continue;
			
		/* evaluate the F-Curve's value for the time given in the strip 
		 * NOTE: we use the modified time here, since strip's F-Curve Modifiers are applied on top of this 
		 */
		value= evaluate_fcurve(fcu, evaltime);
		
		/* apply strip's F-Curve Modifiers on this value 
		 * NOTE: we apply the strip's original evaluation time not the modified one (as per standard F-Curve eval)
		 */
		evaluate_value_fmodifiers(&strip->modifiers, fcu, &value, strip->strip_time);
		
		
		/* get an NLA evaluation channel to work with, and accumulate the evaluated value with the value(s)
		 * stored in this channel if it has been used already
		 */
		nec= nlaevalchan_verify(ptr, channels, nes, fcu, &newChan);
		if (nec)
			nlaevalchan_accumulate(nec, nes, newChan, value);
	}
}

/* evaluates the given evaluation strip */
// TODO: only evaluate here, but flush in one go using the accumulated channels at end...
static void nlastrip_evaluate (PointerRNA *ptr, ListBase *channels, NlaEvalStrip *nes)
{
	/* actions to take depend on the type of strip */
	switch (nes->strip->type) {
		case NLASTRIP_TYPE_CLIP: /* action-clip */
			nlastrip_evaluate_actionclip(ptr, channels, nes);
			break;
		case NLASTRIP_TYPE_TRANSITION: /* transition */
			// XXX code this...
			break;
	}
}

/* write the accumulated settings to */
static void nladata_flush_channels (ListBase *channels)
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
				if (RNA_property_array_length(prop))
					RNA_property_boolean_set_index(ptr, prop, array_index, (int)value);
				else
					RNA_property_boolean_set(ptr, prop, (int)value);
				break;
			case PROP_INT:
				if (RNA_property_array_length(prop))
					RNA_property_int_set_index(ptr, prop, array_index, (int)value);
				else
					RNA_property_int_set(ptr, prop, (int)value);
				break;
			case PROP_FLOAT:
				if (RNA_property_array_length(prop))
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

/* NLA Evaluation function (mostly for use through do_animdata) 
 *	- All channels that will be affected are not cleared anymore. Instead, we just evaluate into 
 *		some temp channels, where values can be accumulated in one go.
 */
static void animsys_evaluate_nla (PointerRNA *ptr, AnimData *adt, float ctime)
{
	NlaTrack *nlt;
	short track_index=0;
	
	ListBase estrips= {NULL, NULL};
	ListBase echannels= {NULL, NULL};
	NlaEvalStrip *nes;
	
	/* 1. get the stack of strips to evaluate at current time (influence calculated here) */
	for (nlt=adt->nla_tracks.first; nlt; nlt=nlt->next, track_index++) 
		nlatrack_ctime_get_strip(&estrips, nlt, track_index, ctime);
	
	/* only continue if there are strips to evaluate */
	if (estrips.first == NULL)
		return;
	
	
	/* 2. for each strip, evaluate then accumulate on top of existing channels, but don't set values yet */
	for (nes= estrips.first; nes; nes= nes->next) 
		nlastrip_evaluate(ptr, &echannels, nes);
	
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
	if ((recalc & ADT_RECALC_ANIM) || (adt->recalc & ADT_RECALC_ANIM))
 	{
		/* evaluate NLA data */
		if ((adt->nla_tracks.first) && !(adt->flag & ADT_NLA_EVAL_OFF))
		{
			animsys_evaluate_nla(&id_ptr, adt, ctime);
		}
		
		/* evaluate Action data */
		// FIXME: what if the solo track was not tweaking one, then nla-solo should be checked too?
		if (adt->action) 
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
// TODO: we currently go over entire 'main' database...
void BKE_animsys_evaluate_all_animation (Main *main, float ctime)
{
	ID *id;
	
	if (G.f & G_DEBUG)
		printf("Evaluate all animation - %f \n", ctime);

	/* macro for less typing */
#define EVAL_ANIM_IDS(first, flag) \
	for (id= first; id; id= id->next) { \
		AnimData *adt= BKE_animdata_from_id(id); \
		BKE_animsys_evaluate_animdata(id, adt, ctime, flag); \
	}
	
	/* nodes */
	// TODO...
	
	/* textures */
	EVAL_ANIM_IDS(main->tex.first, ADT_RECALC_ANIM);
	
	/* lamps */
	EVAL_ANIM_IDS(main->lamp.first, ADT_RECALC_ANIM);
	
	/* materials */
	EVAL_ANIM_IDS(main->mat.first, ADT_RECALC_ANIM);
	
	/* cameras */
	EVAL_ANIM_IDS(main->camera.first, ADT_RECALC_ANIM);
	
	/* shapekeys */
	EVAL_ANIM_IDS(main->key.first, ADT_RECALC_ANIM);
	
	/* curves */
	// TODO...
	
	/* meshes */
	// TODO...
	
	/* objects */
		/* ADT_RECALC_ANIM doesn't need to be supplied here, since object AnimData gets 
		 * this tagged by Depsgraph on framechange 
		 */
	EVAL_ANIM_IDS(main->object.first, /*ADT_RECALC_ANIM*/0); 
	
	/* worlds */
	EVAL_ANIM_IDS(main->world.first, ADT_RECALC_ANIM);
	
	/* scenes */
	EVAL_ANIM_IDS(main->scene.first, ADT_RECALC_ANIM);
}

/* ***************************************** */ 
