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

/** \file blender/editors/animation/keyframing.c
 *  \ingroup edanimation
 */


#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_rigidbody_types.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_idcode.h"
#include "BKE_key.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_nla.h"
#include "BKE_report.h"

#include "ED_anim_api.h"
#include "ED_keyframing.h"
#include "ED_keyframes_edit.h"
#include "ED_screen.h"
#include "ED_object.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "anim_intern.h"

/* ************************************************** */
/* Keyframing Setting Wrangling */

/* Get the active settings for keyframing settings from context (specifically the given scene) */
short ANIM_get_keyframing_flags(Scene *scene, short incl_mode)
{
	eInsertKeyFlags flag = INSERTKEY_NOFLAGS;

	/* standard flags */
	{
		/* visual keying */
		if (IS_AUTOKEY_FLAG(scene, AUTOMATKEY))
			flag |= INSERTKEY_MATRIX;

		/* only needed */
		if (IS_AUTOKEY_FLAG(scene, INSERTNEEDED))
			flag |= INSERTKEY_NEEDED;

		/* default F-Curve color mode - RGB from XYZ indices */
		if (IS_AUTOKEY_FLAG(scene, XYZ2RGB))
			flag |= INSERTKEY_XYZ2RGB;
	}

	/* only if including settings from the autokeying mode... */
	if (incl_mode) {
		/* keyframing mode - only replace existing keyframes */
		if (IS_AUTOKEY_MODE(scene, EDITKEYS))
			flag |= INSERTKEY_REPLACE;
	}

	return flag;
}

/* ******************************************* */
/* Animation Data Validation */

/* Get (or add relevant data to be able to do so) the Active Action for the given
 * Animation Data block, given an ID block where the Animation Data should reside.
 */
bAction *verify_adt_action(Main *bmain, ID *id, short add)
{
	AnimData *adt;

	/* init animdata if none available yet */
	adt = BKE_animdata_from_id(id);
	if ((adt == NULL) && (add))
		adt = BKE_animdata_add_id(id);
	if (adt == NULL) {
		/* if still none (as not allowed to add, or ID doesn't have animdata for some reason) */
		printf("ERROR: Couldn't add AnimData (ID = %s)\n", (id) ? (id->name) : "<None>");
		return NULL;
	}

	/* init action if none available yet */
	/* TODO: need some wizardry to handle NLA stuff correct */
	if ((adt->action == NULL) && (add)) {
		/* init action name from name of ID block */
		char actname[sizeof(id->name) - 2];
		BLI_snprintf(actname, sizeof(actname), "%sAction", id->name + 2);

		/* create action */
		adt->action = BKE_action_add(bmain, actname);

		/* set ID-type from ID-block that this is going to be assigned to
		 * so that users can't accidentally break actions by assigning them
		 * to the wrong places
		 */
		adt->action->idroot = GS(id->name);

		/* tag depsgraph to be rebuilt to include time dependency */
		/* XXX: we probably should have bmain passed down, but that involves altering too many API's */
		DAG_relations_tag_update(bmain);
	}

	/* return the action */
	return adt->action;
}

/* Get (or add relevant data to be able to do so) F-Curve from the Active Action,
 * for the given Animation Data block. This assumes that all the destinations are valid.
 */
FCurve *verify_fcurve(bAction *act, const char group[], PointerRNA *ptr,
                      const char rna_path[], const int array_index, short add)
{
	bActionGroup *agrp;
	FCurve *fcu;

	/* sanity checks */
	if (ELEM(NULL, act, rna_path))
		return NULL;

	/* try to find f-curve matching for this setting
	 *	- add if not found and allowed to add one
	 *		TODO: add auto-grouping support? how this works will need to be resolved
	 */
	fcu = list_find_fcurve(&act->curves, rna_path, array_index);

	if ((fcu == NULL) && (add)) {
		/* use default settings to make a F-Curve */
		fcu = MEM_callocN(sizeof(FCurve), "FCurve");

		fcu->flag = (FCURVE_VISIBLE | FCURVE_SELECTED);
		fcu->auto_smoothing = FCURVE_SMOOTH_CONT_ACCEL;
		if (BLI_listbase_is_empty(&act->curves))
			fcu->flag |= FCURVE_ACTIVE;  /* first one added active */

		/* store path - make copy, and store that */
		fcu->rna_path = BLI_strdup(rna_path);
		fcu->array_index = array_index;

		/* if a group name has been provided, try to add or find a group, then add F-Curve to it */
		if (group) {
			/* try to find group */
			agrp = BKE_action_group_find_name(act, group);

			/* no matching groups, so add one */
			if (agrp == NULL) {
				agrp = action_groups_add_new(act, group);

				/* sync bone group colors if applicable */
				if (ptr && (ptr->type == &RNA_PoseBone)) {
					Object *ob = (Object *)ptr->id.data;
					bPoseChannel *pchan = (bPoseChannel *)ptr->data;
					bPose *pose = ob->pose;
					bActionGroup *grp;

					/* find bone group (if present), and use the color from that */
					grp = (bActionGroup *)BLI_findlink(&pose->agroups, (pchan->agrp_index - 1));
					if (grp) {
						agrp->customCol = grp->customCol;
						action_group_colors_sync(agrp, grp);
					}
				}
			}

			/* add F-Curve to group */
			action_groups_add_channel(act, agrp, fcu);
		}
		else {
			/* just add F-Curve to end of Action's list */
			BLI_addtail(&act->curves, fcu);
		}
	}

	/* return the F-Curve */
	return fcu;
}

/* Helper for update_autoflags_fcurve() */
static void update_autoflags_fcurve_direct(FCurve *fcu, PropertyRNA *prop)
{
	/* set additional flags for the F-Curve (i.e. only integer values) */
	fcu->flag &= ~(FCURVE_INT_VALUES | FCURVE_DISCRETE_VALUES);
	switch (RNA_property_type(prop)) {
		case PROP_FLOAT:
			/* do nothing */
			break;
		case PROP_INT:
			/* do integer (only 'whole' numbers) interpolation between all points */
			fcu->flag |= FCURVE_INT_VALUES;
			break;
		default:
			/* do 'discrete' (i.e. enum, boolean values which cannot take any intermediate
			 * values at all) interpolation between all points
			 *    - however, we must also ensure that evaluated values are only integers still
			 */
			fcu->flag |= (FCURVE_DISCRETE_VALUES | FCURVE_INT_VALUES);
			break;
	}
}

/* Update integer/discrete flags of the FCurve (used when creating/inserting keyframes,
 * but also through RNA when editing an ID prop, see T37103).
 */
void update_autoflags_fcurve(FCurve *fcu, bContext *C, ReportList *reports, PointerRNA *ptr)
{
	PointerRNA tmp_ptr;
	PropertyRNA *prop;
	int old_flag = fcu->flag;

	if ((ptr->id.data == NULL) && (ptr->data == NULL)) {
		BKE_report(reports, RPT_ERROR, "No RNA pointer available to retrieve values for this fcurve");
		return;
	}

	/* try to get property we should be affecting */
	if (RNA_path_resolve_property(ptr, fcu->rna_path, &tmp_ptr, &prop) == false) {
		/* property not found... */
		const char *idname = (ptr->id.data) ? ((ID *)ptr->id.data)->name : TIP_("<No ID pointer>");

		BKE_reportf(reports, RPT_ERROR,
		            "Could not update flags for this fcurve, as RNA path is invalid for the given ID "
		            "(ID = %s, path = %s)",
		            idname, fcu->rna_path);
		return;
	}

	/* update F-Curve flags */
	update_autoflags_fcurve_direct(fcu, prop);

	if (old_flag != fcu->flag) {
		/* Same as if keyframes had been changed */
		WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	}
}

/* ************************************************** */
/* KEYFRAME INSERTION */

/* -------------- BezTriple Insertion -------------------- */

/* This function adds a given BezTriple to an F-Curve. It will allocate
 * memory for the array if needed, and will insert the BezTriple into a
 * suitable place in chronological order.
 *
 * NOTE: any recalculate of the F-Curve that needs to be done will need to
 *      be done by the caller.
 */
int insert_bezt_fcurve(FCurve *fcu, const BezTriple *bezt, eInsertKeyFlags flag)
{
	int i = 0;

	/* are there already keyframes? */
	if (fcu->bezt) {
		bool replace;
		i = binarysearch_bezt_index(fcu->bezt, bezt->vec[1][0], fcu->totvert, &replace);

		/* replace an existing keyframe? */
		if (replace) {
			/* sanity check: 'i' may in rare cases exceed arraylen */
			if ((i >= 0) && (i < fcu->totvert)) {
				if (flag & INSERTKEY_OVERWRITE_FULL) {
					fcu->bezt[i] = *bezt;
				}
				else {
					/* just change the values when replacing, so as to not overwrite handles */
					BezTriple *dst = (fcu->bezt + i);
					float dy = bezt->vec[1][1] - dst->vec[1][1];

					/* just apply delta value change to the handle values */
					dst->vec[0][1] += dy;
					dst->vec[1][1] += dy;
					dst->vec[2][1] += dy;

					dst->f1 = bezt->f1;
					dst->f2 = bezt->f2;
					dst->f3 = bezt->f3;

					/* TODO: perform some other operations? */
				}
			}
		}
		/* keyframing modes allow to not replace keyframe */
		else if ((flag & INSERTKEY_REPLACE) == 0) {
			/* insert new - if we're not restricted to replacing keyframes only */
			BezTriple *newb = MEM_callocN((fcu->totvert + 1) * sizeof(BezTriple), "beztriple");

			/* add the beztriples that should occur before the beztriple to be pasted (originally in fcu) */
			if (i > 0)
				memcpy(newb, fcu->bezt, i * sizeof(BezTriple));

			/* add beztriple to paste at index i */
			*(newb + i) = *bezt;

			/* add the beztriples that occur after the beztriple to be pasted (originally in fcu) */
			if (i < fcu->totvert)
				memcpy(newb + i + 1, fcu->bezt + i, (fcu->totvert - i) * sizeof(BezTriple));

			/* replace (+ free) old with new, only if necessary to do so */
			MEM_freeN(fcu->bezt);
			fcu->bezt = newb;

			fcu->totvert++;
		}
	}
	/* no keyframes already, but can only add if...
	 *	1) keyframing modes say that keyframes can only be replaced, so adding new ones won't know
	 *	2) there are no samples on the curve
	 *		// NOTE: maybe we may want to allow this later when doing samples -> bezt conversions,
	 *		// but for now, having both is asking for trouble
	 */
	else if ((flag & INSERTKEY_REPLACE) == 0 && (fcu->fpt == NULL)) {
		/* create new keyframes array */
		fcu->bezt = MEM_callocN(sizeof(BezTriple), "beztriple");
		*(fcu->bezt) = *bezt;
		fcu->totvert = 1;
	}
	/* cannot add anything */
	else {
		/* return error code -1 to prevent any misunderstandings */
		return -1;
	}


	/* we need to return the index, so that some tools which do post-processing can
	 * detect where we added the BezTriple in the array
	 */
	return i;
}

/**
 * This function is a wrapper for insert_bezt_fcurve_internal(), and should be used when
 * adding a new keyframe to a curve, when the keyframe doesn't exist anywhere else yet.
 * It returns the index at which the keyframe was added.
 *
 * \param keyframe_type: The type of keyframe (eBezTriple_KeyframeType)
 * \param flag: Optional flags (eInsertKeyFlags) for controlling how keys get added
 *              and/or whether updates get done
 */
int insert_vert_fcurve(FCurve *fcu, float x, float y, eBezTriple_KeyframeType keyframe_type, eInsertKeyFlags flag)
{
	BezTriple beztr = {{{0}}};
	unsigned int oldTot = fcu->totvert;
	int a;

	/* set all three points, for nicer start position
	 * NOTE: +/- 1 on vec.x for left and right handles is so that 'free' handles work ok...
	 */
	beztr.vec[0][0] = x - 1.0f;
	beztr.vec[0][1] = y;
	beztr.vec[1][0] = x;
	beztr.vec[1][1] = y;
	beztr.vec[2][0] = x + 1.0f;
	beztr.vec[2][1] = y;
	beztr.f1 = beztr.f2 = beztr.f3 = SELECT;

	/* set default handle types and interpolation mode */
	if (flag & INSERTKEY_NO_USERPREF) {
		/* for Py-API, we want scripts to have predictable behaviour,
		 * hence the option to not depend on the userpref defaults
		 */
		beztr.h1 = beztr.h2 = HD_AUTO_ANIM;
		beztr.ipo = BEZT_IPO_BEZ;
	}
	else {
		/* for UI usage - defaults should come from the userprefs and/or toolsettings */
		beztr.h1 = beztr.h2 = U.keyhandles_new; /* use default handle type here */

		/* use default interpolation mode, with exceptions for int/discrete values */
		beztr.ipo = U.ipo_new;
	}

	/* interpolation type used is constrained by the type of values the curve can take */
	if (fcu->flag & FCURVE_DISCRETE_VALUES) {
		beztr.ipo = BEZT_IPO_CONST;
	}
	else if ((beztr.ipo == BEZT_IPO_BEZ) && (fcu->flag & FCURVE_INT_VALUES)) {
		beztr.ipo = BEZT_IPO_LIN;
	}

	/* set keyframe type value (supplied), which should come from the scene settings in most cases */
	BEZKEYTYPE(&beztr) = keyframe_type;

	/* set default values for "easing" interpolation mode settings
	 * NOTE: Even if these modes aren't currently used, if users switch
	 *       to these later, we want these to work in a sane way out of
	 *       the box.
	 */
	beztr.back = 1.70158f;     /* "back" easing - this value used to be used when overshoot=0, but that        */
	                           /*                 introduced discontinuities in how the param worked           */

	beztr.amplitude = 0.8f;    /* "elastic" easing - values here were hand-optimised for a default duration of */
	beztr.period = 4.1f;       /*                    ~10 frames (typical mograph motion length)                */

	/* add temp beztriple to keyframes */
	a = insert_bezt_fcurve(fcu, &beztr, flag);

	/* what if 'a' is a negative index?
	 * for now, just exit to prevent any segfaults
	 */
	if (a < 0) return -1;

	/* don't recalculate handles if fast is set
	 *	- this is a hack to make importers faster
	 *	- we may calculate twice (due to autohandle needing to be calculated twice)
	 */
	if ((flag & INSERTKEY_FAST) == 0)
		calchandles_fcurve(fcu);

	/* set handletype and interpolation */
	if ((fcu->totvert > 2) && (flag & INSERTKEY_REPLACE) == 0) {
		BezTriple *bezt = (fcu->bezt + a);

		/* set interpolation from previous (if available), but only if we didn't just replace some keyframe
		 *  - replacement is indicated by no-change in number of verts
		 *	- when replacing, the user may have specified some interpolation that should be kept
		 */
		if (fcu->totvert > oldTot) {
			if (a > 0)
				bezt->ipo = (bezt - 1)->ipo;
			else if (a < fcu->totvert - 1)
				bezt->ipo = (bezt + 1)->ipo;
		}

		/* don't recalculate handles if fast is set
		 *	- this is a hack to make importers faster
		 *	- we may calculate twice (due to autohandle needing to be calculated twice)
		 */
		if ((flag & INSERTKEY_FAST) == 0)
			calchandles_fcurve(fcu);
	}

	/* return the index at which the keyframe was added */
	return a;
}

/* -------------- 'Smarter' Keyframing Functions -------------------- */
/* return codes for new_key_needed */
enum {
	KEYNEEDED_DONTADD = 0,
	KEYNEEDED_JUSTADD,
	KEYNEEDED_DELPREV,
	KEYNEEDED_DELNEXT
} /*eKeyNeededStatus*/;

/* This helper function determines whether a new keyframe is needed */
/* Cases where keyframes should not be added:
 *	1. Keyframe to be added between two keyframes with similar values
 *	2. Keyframe to be added on frame where two keyframes are already situated
 *	3. Keyframe lies at point that intersects the linear line between two keyframes
 */
static short new_key_needed(FCurve *fcu, float cFrame, float nValue)
{
	BezTriple *bezt = NULL, *prev = NULL;
	int totCount, i;
	float valA = 0.0f, valB = 0.0f;

	/* safety checking */
	if (fcu == NULL) return KEYNEEDED_JUSTADD;
	totCount = fcu->totvert;
	if (totCount == 0) return KEYNEEDED_JUSTADD;

	/* loop through checking if any are the same */
	bezt = fcu->bezt;
	for (i = 0; i < totCount; i++) {
		float prevPosi = 0.0f, prevVal = 0.0f;
		float beztPosi = 0.0f, beztVal = 0.0f;

		/* get current time+value */
		beztPosi = bezt->vec[1][0];
		beztVal = bezt->vec[1][1];

		if (prev) {
			/* there is a keyframe before the one currently being examined */

			/* get previous time+value */
			prevPosi = prev->vec[1][0];
			prevVal = prev->vec[1][1];

			/* keyframe to be added at point where there are already two similar points? */
			if (IS_EQF(prevPosi, cFrame) && IS_EQF(beztPosi, cFrame) && IS_EQF(beztPosi, prevPosi)) {
				return KEYNEEDED_DONTADD;
			}

			/* keyframe between prev+current points ? */
			if ((prevPosi <= cFrame) && (cFrame <= beztPosi)) {
				/* is the value of keyframe to be added the same as keyframes on either side ? */
				if (IS_EQF(prevVal, nValue) && IS_EQF(beztVal, nValue) && IS_EQF(prevVal, beztVal)) {
					return KEYNEEDED_DONTADD;
				}
				else {
					float realVal;

					/* get real value of curve at that point */
					realVal = evaluate_fcurve(fcu, cFrame);

					/* compare whether it's the same as proposed */
					if (IS_EQF(realVal, nValue))
						return KEYNEEDED_DONTADD;
					else
						return KEYNEEDED_JUSTADD;
				}
			}

			/* new keyframe before prev beztriple? */
			if (cFrame < prevPosi) {
				/* A new keyframe will be added. However, whether the previous beztriple
				 * stays around or not depends on whether the values of previous/current
				 * beztriples and new keyframe are the same.
				 */
				if (IS_EQF(prevVal, nValue) && IS_EQF(beztVal, nValue) && IS_EQF(prevVal, beztVal))
					return KEYNEEDED_DELNEXT;
				else
					return KEYNEEDED_JUSTADD;
			}
		}
		else {
			/* just add a keyframe if there's only one keyframe
			 * and the new one occurs before the existing one does.
			 */
			if ((cFrame < beztPosi) && (totCount == 1))
				return KEYNEEDED_JUSTADD;
		}

		/* continue. frame to do not yet passed (or other conditions not met) */
		if (i < (totCount - 1)) {
			prev = bezt;
			bezt++;
		}
		else
			break;
	}

	/* Frame in which to add a new-keyframe occurs after all other keys
	 * -> If there are at least two existing keyframes, then if the values of the
	 *    last two keyframes and the new-keyframe match, the last existing keyframe
	 *    gets deleted as it is no longer required.
	 * -> Otherwise, a keyframe is just added. 1.0 is added so that fake-2nd-to-last
	 *    keyframe is not equal to last keyframe.
	 */
	bezt = (fcu->bezt + (fcu->totvert - 1));
	valA = bezt->vec[1][1];

	if (prev)
		valB = prev->vec[1][1];
	else
		valB = bezt->vec[1][1] + 1.0f;

	if (IS_EQF(valA, nValue) && IS_EQF(valA, valB))
		return KEYNEEDED_DELPREV;
	else
		return KEYNEEDED_JUSTADD;
}

/* ------------------ RNA Data-Access Functions ------------------ */

/* Try to read value using RNA-properties obtained already */
static float setting_get_rna_value(PointerRNA *ptr, PropertyRNA *prop, int index)
{
	float value = 0.0f;

	switch (RNA_property_type(prop)) {
		case PROP_BOOLEAN:
			if (RNA_property_array_check(prop))
				value = (float)RNA_property_boolean_get_index(ptr, prop, index);
			else
				value = (float)RNA_property_boolean_get(ptr, prop);
			break;
		case PROP_INT:
			if (RNA_property_array_check(prop))
				value = (float)RNA_property_int_get_index(ptr, prop, index);
			else
				value = (float)RNA_property_int_get(ptr, prop);
			break;
		case PROP_FLOAT:
			if (RNA_property_array_check(prop))
				value = RNA_property_float_get_index(ptr, prop, index);
			else
				value = RNA_property_float_get(ptr, prop);
			break;
		case PROP_ENUM:
			value = (float)RNA_property_enum_get(ptr, prop);
			break;
		default:
			break;
	}

	return value;
}

/* ------------------ 'Visual' Keyframing Functions ------------------ */

/* internal status codes for visualkey_can_use */
enum {
	VISUALKEY_NONE = 0,
	VISUALKEY_LOC,
	VISUALKEY_ROT,
	VISUALKEY_SCA,
};

/* This helper function determines if visual-keyframing should be used when
 * inserting keyframes for the given channel. As visual-keyframing only works
 * on Object and Pose-Channel blocks, this should only get called for those
 * blocktypes, when using "standard" keying but 'Visual Keying' option in Auto-Keying
 * settings is on.
 */
static bool visualkey_can_use(PointerRNA *ptr, PropertyRNA *prop)
{
	bConstraint *con = NULL;
	short searchtype = VISUALKEY_NONE;
	bool has_rigidbody = false;
	bool has_parent = false;
	const char *identifier = NULL;

	/* validate data */
	if (ELEM(NULL, ptr, ptr->data, prop))
		return false;

	/* get first constraint and determine type of keyframe constraints to check for
	 *  - constraints can be on either Objects or PoseChannels, so we only check if the
	 *    ptr->type is RNA_Object or RNA_PoseBone, which are the RNA wrapping-info for
	 *    those structs, allowing us to identify the owner of the data
	 */
	if (ptr->type == &RNA_Object) {
		/* Object */
		Object *ob = (Object *)ptr->data;
		RigidBodyOb *rbo = ob->rigidbody_object;

		con = ob->constraints.first;
		identifier = RNA_property_identifier(prop);
		has_parent = (ob->parent != NULL);

		/* active rigidbody objects only, as only those are affected by sim */
		has_rigidbody = ((rbo) && (rbo->type == RBO_TYPE_ACTIVE));
	}
	else if (ptr->type == &RNA_PoseBone) {
		/* Pose Channel */
		bPoseChannel *pchan = (bPoseChannel *)ptr->data;

		con = pchan->constraints.first;
		identifier = RNA_property_identifier(prop);
		has_parent = (pchan->parent != NULL);
	}

	/* check if any data to search using */
	if (ELEM(NULL, con, identifier) && (has_parent == false) && (has_rigidbody == false))
		return false;

	/* location or rotation identifiers only... */
	if (identifier == NULL) {
		printf("%s failed: NULL identifier\n", __func__);
		return false;
	}
	else if (strstr(identifier, "location")) {
		searchtype = VISUALKEY_LOC;
	}
	else if (strstr(identifier, "rotation")) {
		searchtype = VISUALKEY_ROT;
	}
	else if (strstr(identifier, "scale")) {
		searchtype = VISUALKEY_SCA;
	}
	else {
		printf("%s failed: identifier - '%s'\n", __func__, identifier);
		return false;
	}


	/* only search if a searchtype and initial constraint are available */
	if (searchtype) {
		/* parent or rigidbody are always matching */
		if (has_parent || has_rigidbody)
			return true;

		/* constraints */
		for (; con; con = con->next) {
			/* only consider constraint if it is not disabled, and has influence */
			if (con->flag & CONSTRAINT_DISABLE) continue;
			if (con->enforce == 0.0f) continue;

			/* some constraints may alter these transforms */
			switch (con->type) {
				/* multi-transform constraints */
				case CONSTRAINT_TYPE_CHILDOF:
					return true;
				case CONSTRAINT_TYPE_TRANSFORM:
				case CONSTRAINT_TYPE_TRANSLIKE:
					return true;
				case CONSTRAINT_TYPE_FOLLOWPATH:
					return true;
				case CONSTRAINT_TYPE_KINEMATIC:
					return true;

				/* single-transform constraints  */
				case CONSTRAINT_TYPE_TRACKTO:
					if (searchtype == VISUALKEY_ROT) return true;
					break;
				case CONSTRAINT_TYPE_DAMPTRACK:
					if (searchtype == VISUALKEY_ROT) return true;
					break;
				case CONSTRAINT_TYPE_ROTLIMIT:
					if (searchtype == VISUALKEY_ROT) return true;
					break;
				case CONSTRAINT_TYPE_LOCLIMIT:
					if (searchtype == VISUALKEY_LOC) return true;
					break;
				case CONSTRAINT_TYPE_SIZELIMIT:
					if (searchtype == VISUALKEY_SCA) return true;
					break;
				case CONSTRAINT_TYPE_DISTLIMIT:
					if (searchtype == VISUALKEY_LOC) return true;
					break;
				case CONSTRAINT_TYPE_ROTLIKE:
					if (searchtype == VISUALKEY_ROT) return true;
					break;
				case CONSTRAINT_TYPE_LOCLIKE:
					if (searchtype == VISUALKEY_LOC) return true;
					break;
				case CONSTRAINT_TYPE_SIZELIKE:
					if (searchtype == VISUALKEY_SCA) return true;
					break;
				case CONSTRAINT_TYPE_LOCKTRACK:
					if (searchtype == VISUALKEY_ROT) return true;
					break;
				case CONSTRAINT_TYPE_MINMAX:
					if (searchtype == VISUALKEY_LOC) return true;
					break;

				default:
					break;
			}
		}
	}

	/* when some condition is met, this function returns, so that means we've got nothing */
	return false;
}

/* This helper function extracts the value to use for visual-keyframing
 * In the event that it is not possible to perform visual keying, try to fall-back
 * to using the default method. Assumes that all data it has been passed is valid.
 */
static float visualkey_get_value(PointerRNA *ptr, PropertyRNA *prop, int array_index)
{
	const char *identifier = RNA_property_identifier(prop);
	float tmat[4][4];
	int rotmode;

	/* handle for Objects or PoseChannels only
	 *  - only Location, Rotation or Scale keyframes are supported currently
	 *  - constraints can be on either Objects or PoseChannels, so we only check if the
	 *    ptr->type is RNA_Object or RNA_PoseBone, which are the RNA wrapping-info for
	 *        those structs, allowing us to identify the owner of the data
	 *  - assume that array_index will be sane
	 */
	if (ptr->type == &RNA_Object) {
		Object *ob = (Object *)ptr->data;

		/* Loc code is specific... */
		if (strstr(identifier, "location")) {
			return ob->obmat[3][array_index];
		}

		copy_m4_m4(tmat, ob->obmat);
		rotmode = ob->rotmode;
	}
	else if (ptr->type == &RNA_PoseBone) {
		bPoseChannel *pchan = (bPoseChannel *)ptr->data;

		BKE_armature_mat_pose_to_bone(pchan, pchan->pose_mat, tmat);
		rotmode = pchan->rotmode;

		/* Loc code is specific... */
		if (strstr(identifier, "location")) {
			/* only use for non-connected bones */
			if ((pchan->bone->parent == NULL) || !(pchan->bone->flag & BONE_CONNECTED))
				return tmat[3][array_index];
		}
	}
	else {
		return setting_get_rna_value(ptr, prop, array_index);
	}

	/* Rot/Scale code are common! */
	if (strstr(identifier, "rotation_euler")) {
		float eul[3];

		mat4_to_eulO(eul, rotmode, tmat);
		return eul[array_index];
	}
	else if (strstr(identifier, "rotation_quaternion")) {
		float mat3[3][3], quat[4];

		copy_m3_m4(mat3, tmat);
		mat3_to_quat_is_ok(quat, mat3);

		return quat[array_index];
	}
	else if (strstr(identifier, "rotation_axis_angle")) {
		float axis[3], angle;

		mat4_to_axis_angle(axis, &angle, tmat);

		/* w = 0, x,y,z = 1,2,3 */
		if (array_index == 0)
			return angle;
		else
			return axis[array_index - 1];
	}
	else if (strstr(identifier, "scale")) {
		float scale[3];

		mat4_to_size(scale, tmat);

		return scale[array_index];
	}

	/* as the function hasn't returned yet, read value from system in the default way */
	return setting_get_rna_value(ptr, prop, array_index);
}

/* ------------------------- Insert Key API ------------------------- */

/* Secondary Keyframing API call:
 *  Use this when validation of necessary animation data is not necessary, since an RNA-pointer to the necessary
 *	data being keyframed, and a pointer to the F-Curve to use have both been provided.
 *
 *  keytype is the "keyframe type" (eBezTriple_KeyframeType), as shown in the Dope Sheet.
 *
 *	The flag argument is used for special settings that alter the behavior of
 *	the keyframe insertion. These include the 'visual' keyframing modes, quick refresh,
 *	and extra keyframe filtering.
 */
bool insert_keyframe_direct(ReportList *reports, PointerRNA ptr, PropertyRNA *prop, FCurve *fcu, float cfra, eBezTriple_KeyframeType keytype, eInsertKeyFlags flag)
{
	float curval = 0.0f;

	/* no F-Curve to add keyframe to? */
	if (fcu == NULL) {
		BKE_report(reports, RPT_ERROR, "No F-Curve to add keyframes to");
		return false;
	}
	/* F-Curve not editable? */
	if (fcurve_is_keyframable(fcu) == 0) {
		BKE_reportf(reports, RPT_ERROR,
		            "F-Curve with path '%s[%d]' cannot be keyframed, ensure that it is not locked or sampled, "
		            "and try removing F-Modifiers",
		            fcu->rna_path, fcu->array_index);
		return false;
	}

	/* if no property given yet, try to validate from F-Curve info */
	if ((ptr.id.data == NULL) && (ptr.data == NULL)) {
		BKE_report(reports, RPT_ERROR, "No RNA pointer available to retrieve values for keyframing from");
		return false;
	}
	if (prop == NULL) {
		PointerRNA tmp_ptr;

		/* try to get property we should be affecting */
		if (RNA_path_resolve_property(&ptr, fcu->rna_path, &tmp_ptr, &prop) == false) {
			/* property not found... */
			const char *idname = (ptr.id.data) ? ((ID *)ptr.id.data)->name : TIP_("<No ID pointer>");

			BKE_reportf(reports, RPT_ERROR,
			            "Could not insert keyframe, as RNA path is invalid for the given ID (ID = %s, path = %s)",
			            idname, fcu->rna_path);
			return false;
		}
		else {
			/* property found, so overwrite 'ptr' to make later code easier */
			ptr = tmp_ptr;
		}
	}

	/* update F-Curve flags to ensure proper behaviour for property type */
	update_autoflags_fcurve_direct(fcu, prop);

	/* adjust frame on which to add keyframe */
	if ((flag & INSERTKEY_DRIVER) && (fcu->driver)) {
		PathResolvedRNA anim_rna;

		if (RNA_path_resolved_create(&ptr, prop, fcu->array_index, &anim_rna)) {
			/* for making it easier to add corrective drivers... */
			cfra = evaluate_driver(&anim_rna, fcu->driver, cfra);
		}
		else {
			cfra = 0.0f;
		}
	}

	/* obtain value to give keyframe */
	if ( (flag & INSERTKEY_MATRIX) &&
	     (visualkey_can_use(&ptr, prop)) )
	{
		/* visual-keying is only available for object and pchan datablocks, as
		 * it works by keyframing using a value extracted from the final matrix
		 * instead of using the kt system to extract a value.
		 */
		curval = visualkey_get_value(&ptr, prop, fcu->array_index);
	}
	else {
		/* read value from system */
		curval = setting_get_rna_value(&ptr, prop, fcu->array_index);
	}

	/* only insert keyframes where they are needed */
	if (flag & INSERTKEY_NEEDED) {
		short insert_mode;

		/* check whether this curve really needs a new keyframe */
		insert_mode = new_key_needed(fcu, cfra, curval);

		/* insert new keyframe at current frame */
		if (insert_mode)
			insert_vert_fcurve(fcu, cfra, curval, keytype, flag);

		/* delete keyframe immediately before/after newly added */
		switch (insert_mode) {
			case KEYNEEDED_DELPREV:
				delete_fcurve_key(fcu, fcu->totvert - 2, 1);
				break;
			case KEYNEEDED_DELNEXT:
				delete_fcurve_key(fcu, 1, 1);
				break;
		}

		/* only return success if keyframe added */
		if (insert_mode)
			return true;
	}
	else {
		/* just insert keyframe */
		insert_vert_fcurve(fcu, cfra, curval, keytype, flag);

		/* return success */
		return true;
	}

	/* failed */
	return false;
}

/* Main Keyframing API call:
 *	Use this when validation of necessary animation data is necessary, since it may not exist yet.
 *
 *	The flag argument is used for special settings that alter the behavior of
 *	the keyframe insertion. These include the 'visual' keyframing modes, quick refresh,
 *	and extra keyframe filtering.
 *
 *	index of -1 keys all array indices
 */
short insert_keyframe(
        Main *bmain, ReportList *reports, ID *id, bAction *act,
        const char group[], const char rna_path[], int array_index, float cfra, eBezTriple_KeyframeType keytype, eInsertKeyFlags flag)
{
	PointerRNA id_ptr, ptr;
	PropertyRNA *prop = NULL;
	AnimData *adt;
	FCurve *fcu;
	int array_index_max = array_index + 1;
	int ret = 0;

	/* validate pointer first - exit if failure */
	if (id == NULL) {
		BKE_reportf(reports, RPT_ERROR, "No ID block to insert keyframe in (path = %s)", rna_path);
		return 0;
	}

	RNA_id_pointer_create(id, &id_ptr);
	if (RNA_path_resolve_property(&id_ptr, rna_path, &ptr, &prop) == false) {
		BKE_reportf(reports, RPT_ERROR,
		            "Could not insert keyframe, as RNA path is invalid for the given ID (ID = %s, path = %s)",
		            (id) ? id->name : TIP_("<Missing ID block>"), rna_path);
		return 0;
	}

	/* if no action is provided, keyframe to the default one attached to this ID-block */
	if (act == NULL) {
		/* get action to add F-Curve+keyframe to */
		act = verify_adt_action(bmain, id, 1);

		if (act == NULL) {
			BKE_reportf(reports, RPT_ERROR,
			            "Could not insert keyframe, as this type does not support animation data (ID = %s, path = %s)",
			            id->name, rna_path);
			return 0;
		}
	}

	/* apply NLA-mapping to frame to use (if applicable) */
	adt = BKE_animdata_from_id(id);
	cfra = BKE_nla_tweakedit_remap(adt, cfra, NLATIME_CONVERT_UNMAP);

	/* key entire array convenience method */
	if (array_index == -1) {
		array_index = 0;
		array_index_max = RNA_property_array_length(&ptr, prop);

		/* for single properties, increase max_index so that the property itself gets included,
		 * but don't do this for standard arrays since that can cause corruption issues
		 * (extra unused curves)
		 */
		if (array_index_max == array_index)
			array_index_max++;
	}

	/* will only loop once unless the array index was -1 */
	for (; array_index < array_index_max; array_index++) {
		/* make sure the F-Curve exists
		 *	- if we're replacing keyframes only, DO NOT create new F-Curves if they do not exist yet
		 *	  but still try to get the F-Curve if it exists...
		 */
		fcu = verify_fcurve(act, group, &ptr, rna_path, array_index, (flag & INSERTKEY_REPLACE) == 0);

		/* we may not have a F-Curve when we're replacing only... */
		if (fcu) {
			/* set color mode if the F-Curve is new (i.e. without any keyframes) */
			if ((fcu->totvert == 0) && (flag & INSERTKEY_XYZ2RGB)) {
				/* for Loc/Rot/Scale and also Color F-Curves, the color of the F-Curve in the Graph Editor,
				 * is determined by the array index for the F-Curve
				 */
				PropertySubType prop_subtype = RNA_property_subtype(prop);
				if (ELEM(prop_subtype, PROP_TRANSLATION, PROP_XYZ, PROP_EULER, PROP_COLOR, PROP_COORDS)) {
					fcu->color_mode = FCURVE_COLOR_AUTO_RGB;
				}
				else if (ELEM(prop_subtype, PROP_QUATERNION)) {
					fcu->color_mode = FCURVE_COLOR_AUTO_YRGB;
				}
			}

			/* insert keyframe */
			ret += insert_keyframe_direct(reports, ptr, prop, fcu, cfra, keytype, flag);
		}
	}

	return ret;
}

/* ************************************************** */
/* KEYFRAME DELETION */

/* Main Keyframing API call:
 *	Use this when validation of necessary animation data isn't necessary as it
 *	already exists. It will delete a keyframe at the current frame.
 *
 *	The flag argument is used for special settings that alter the behavior of
 *	the keyframe deletion. These include the quick refresh options.
 */



/**
 * \note caller needs to run #BKE_nla_tweakedit_remap to get NLA relative frame.
 *       caller should also check #BKE_fcurve_is_protected before keying.
 */
static bool delete_keyframe_fcurve(AnimData *adt, FCurve *fcu, float cfra)
{
	bool found;
	int i;

	/* try to find index of beztriple to get rid of */
	i = binarysearch_bezt_index(fcu->bezt, cfra, fcu->totvert, &found);
	if (found) {
		/* delete the key at the index (will sanity check + do recalc afterwards) */
		delete_fcurve_key(fcu, i, 1);

		/* Only delete curve too if it won't be doing anything anymore */
		if ((fcu->totvert == 0) && (list_has_suitable_fmodifier(&fcu->modifiers, 0, FMI_TYPE_GENERATE_CURVE) == 0))
			ANIM_fcurve_delete_from_animdata(NULL, adt, fcu);

		/* return success */
		return true;
	}
	return false;
}

short delete_keyframe(ReportList *reports, ID *id, bAction *act, const char group[], const char rna_path[], int array_index, float cfra, eInsertKeyFlags UNUSED(flag))
{
	AnimData *adt = BKE_animdata_from_id(id);
	PointerRNA id_ptr, ptr;
	PropertyRNA *prop;
	int array_index_max = array_index + 1;
	int ret = 0;

	/* sanity checks */
	if (ELEM(NULL, id, adt)) {
		BKE_report(reports, RPT_ERROR, "No ID block and/or AnimData to delete keyframe from");
		return 0;
	}

	/* validate pointer first - exit if failure */
	RNA_id_pointer_create(id, &id_ptr);
	if (RNA_path_resolve_property(&id_ptr, rna_path, &ptr, &prop) == false) {
		BKE_reportf(reports, RPT_ERROR,
		            "Could not delete keyframe, as RNA path is invalid for the given ID (ID = %s, path = %s)",
		            id->name, rna_path);
		return 0;
	}

	/* get F-Curve
	 * Note: here is one of the places where we don't want new Action + F-Curve added!
	 *      so 'add' var must be 0
	 */
	if (act == NULL) {
		/* if no action is provided, use the default one attached to this ID-block
		 *  - if it doesn't exist, then we're out of options...
		 */
		if (adt->action) {
			act = adt->action;

			/* apply NLA-mapping to frame to use (if applicable) */
			cfra = BKE_nla_tweakedit_remap(adt, cfra, NLATIME_CONVERT_UNMAP);
		}
		else {
			BKE_reportf(reports, RPT_ERROR, "No action to delete keyframes from for ID = %s", id->name);
			return 0;
		}
	}

	/* key entire array convenience method */
	if (array_index == -1) {
		array_index = 0;
		array_index_max = RNA_property_array_length(&ptr, prop);

		/* for single properties, increase max_index so that the property itself gets included,
		 * but don't do this for standard arrays since that can cause corruption issues
		 * (extra unused curves)
		 */
		if (array_index_max == array_index)
			array_index_max++;
	}

	/* will only loop once unless the array index was -1 */
	for (; array_index < array_index_max; array_index++) {
		FCurve *fcu = verify_fcurve(act, group, &ptr, rna_path, array_index, 0);

		/* check if F-Curve exists and/or whether it can be edited */
		if (fcu == NULL)
			continue;

		if (BKE_fcurve_is_protected(fcu)) {
			BKE_reportf(reports, RPT_WARNING,
			            "Not deleting keyframe for locked F-Curve '%s' for %s '%s'",
			            fcu->rna_path, BKE_idcode_to_name(GS(id->name)), id->name + 2);
			continue;
		}

		ret += delete_keyframe_fcurve(adt, fcu, cfra);

	}

	/* return success/failure */
	return ret;
}

/* ************************************************** */
/* KEYFRAME CLEAR */

/* Main Keyframing API call:
 *	Use this when validation of necessary animation data isn't necessary as it
 *	already exists. It will clear the current buttons fcurve(s).
 *
 *	The flag argument is used for special settings that alter the behavior of
 *	the keyframe deletion. These include the quick refresh options.
 */
static short clear_keyframe(ReportList *reports, ID *id, bAction *act, const char group[], const char rna_path[], int array_index, eInsertKeyFlags UNUSED(flag))
{
	AnimData *adt = BKE_animdata_from_id(id);
	PointerRNA id_ptr, ptr;
	PropertyRNA *prop;
	int array_index_max = array_index + 1;
	int ret = 0;

	/* sanity checks */
	if (ELEM(NULL, id, adt)) {
		BKE_report(reports, RPT_ERROR, "No ID block and/or AnimData to delete keyframe from");
		return 0;
	}

	/* validate pointer first - exit if failure */
	RNA_id_pointer_create(id, &id_ptr);
	if (RNA_path_resolve_property(&id_ptr, rna_path, &ptr, &prop) == false) {
		BKE_reportf(reports, RPT_ERROR,
		            "Could not clear keyframe, as RNA path is invalid for the given ID (ID = %s, path = %s)",
		            id->name, rna_path);
		return 0;
	}

	/* get F-Curve
	 * Note: here is one of the places where we don't want new Action + F-Curve added!
	 *      so 'add' var must be 0
	 */
	if (act == NULL) {
		/* if no action is provided, use the default one attached to this ID-block
		 *  - if it doesn't exist, then we're out of options...
		 */
		if (adt->action) {
			act = adt->action;
		}
		else {
			BKE_reportf(reports, RPT_ERROR, "No action to delete keyframes from for ID = %s", id->name);
			return 0;
		}
	}

	/* key entire array convenience method */
	if (array_index == -1) {
		array_index = 0;
		array_index_max = RNA_property_array_length(&ptr, prop);

		/* for single properties, increase max_index so that the property itself gets included,
		 * but don't do this for standard arrays since that can cause corruption issues
		 * (extra unused curves)
		 */
		if (array_index_max == array_index)
			array_index_max++;
	}

	/* will only loop once unless the array index was -1 */
	for (; array_index < array_index_max; array_index++) {
		FCurve *fcu = verify_fcurve(act, group, &ptr, rna_path, array_index, 0);

		/* check if F-Curve exists and/or whether it can be edited */
		if (fcu == NULL)
			continue;

		if (BKE_fcurve_is_protected(fcu)) {
			BKE_reportf(reports, RPT_WARNING,
			            "Not clearing all keyframes from locked F-Curve '%s' for %s '%s'",
			            fcu->rna_path, BKE_idcode_to_name(GS(id->name)), id->name + 2);
			continue;
		}

		ANIM_fcurve_delete_from_animdata(NULL, adt, fcu);

		/* return success */
		ret++;
	}

	/* return success/failure */
	return ret;
}

/* ******************************************* */
/* KEYFRAME MODIFICATION */

/* mode for commonkey_modifykey */
enum {
	COMMONKEY_MODE_INSERT = 0,
	COMMONKEY_MODE_DELETE,
} /*eCommonModifyKey_Modes*/;

/* Polling callback for use with ANIM_*_keyframe() operators
 * This is based on the standard ED_operator_areaactive callback,
 * except that it does special checks for a few spacetypes too...
 */
static bool modify_key_op_poll(bContext *C)
{
	ScrArea *sa = CTX_wm_area(C);
	Scene *scene = CTX_data_scene(C);

	/* if no area or active scene */
	if (ELEM(NULL, sa, scene))
		return false;

	/* should be fine */
	return true;
}

/* Insert Key Operator ------------------------ */

static int insert_key_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	bool ob_edit_mode = false;
	KeyingSet *ks = NULL;
	int type = RNA_enum_get(op->ptr, "type");
	float cfra = (float)CFRA; // XXX for now, don't bother about all the yucky offset crap
	short success;

	/* type is the Keying Set the user specified to use when calling the operator:
	 *	- type == 0: use scene's active Keying Set
	 *	- type > 0: use a user-defined Keying Set from the active scene
	 *	- type < 0: use a builtin Keying Set
	 */
	if (type == 0)
		type = scene->active_keyingset;
	if (type > 0)
		ks = BLI_findlink(&scene->keyingsets, type - 1);
	else
		ks = BLI_findlink(&builtin_keyingsets, -type - 1);

	/* report failures */
	if (ks == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No active keying set");
		return OPERATOR_CANCELLED;
	}

	/* exit the edit mode to make sure that those object data properties that have been
	 * updated since the last switching to the edit mode will be keyframed correctly
	 */
	if (obedit && ANIM_keyingset_find_id(ks, (ID *)obedit->data)) {
		ED_object_mode_toggle(C, OB_MODE_EDIT);
		ob_edit_mode = true;
	}

	/* try to insert keyframes for the channels specified by KeyingSet */
	success = ANIM_apply_keyingset(C, NULL, NULL, ks, MODIFYKEY_MODE_INSERT, cfra);
	if (G.debug & G_DEBUG)
		BKE_reportf(op->reports, RPT_INFO, "Keying set '%s' - successfully added %d keyframes", ks->name, success);

	/* restore the edit mode if necessary */
	if (ob_edit_mode) {
		ED_object_mode_toggle(C, OB_MODE_EDIT);
	}

	/* report failure or do updates? */
	if (success == MODIFYKEY_INVALID_CONTEXT) {
		BKE_report(op->reports, RPT_ERROR, "No suitable context info for active keying set");
		return OPERATOR_CANCELLED;
	}
	else if (success) {
		/* if the appropriate properties have been set, make a note that we've inserted something */
		if (RNA_boolean_get(op->ptr, "confirm_success"))
			BKE_reportf(op->reports, RPT_INFO, "Successfully added %d keyframes for keying set '%s'", success, ks->name);

		/* send notifiers that keyframes have been changed */
		WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_ADDED, NULL);
	}
	else
		BKE_report(op->reports, RPT_WARNING, "Keying set failed to insert any keyframes");

	return OPERATOR_FINISHED;
}

void ANIM_OT_keyframe_insert(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Insert Keyframe";
	ot->idname = "ANIM_OT_keyframe_insert";
	ot->description = "Insert keyframes on the current frame for all properties in the specified Keying Set";

	/* callbacks */
	ot->exec = insert_key_exec;
	ot->poll = modify_key_op_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* keyingset to use (dynamic enum) */
	prop = RNA_def_enum(ot->srna, "type", DummyRNA_DEFAULT_items, 0, "Keying Set", "The Keying Set to use");
	RNA_def_enum_funcs(prop, ANIM_keying_sets_enum_itemf);
	RNA_def_property_flag(prop, PROP_HIDDEN);
	ot->prop = prop;

	/* confirm whether a keyframe was added by showing a popup
	 *	- by default, this is enabled, since this operator is assumed to be called independently
	 */
	prop = RNA_def_boolean(ot->srna, "confirm_success", 1, "Confirm Successful Insert",
	                       "Show a popup when the keyframes get successfully added");
	RNA_def_property_flag(prop, PROP_HIDDEN);
}

/* Insert Key Operator (With Menu) ------------------------ */
/* This operator checks if a menu should be shown for choosing the KeyingSet to use,
 * then calls the menu if necessary before
 */

static int insert_key_menu_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	Scene *scene = CTX_data_scene(C);

	/* if prompting or no active Keying Set, show the menu */
	if ((scene->active_keyingset == 0) || RNA_boolean_get(op->ptr, "always_prompt")) {
		uiPopupMenu *pup;
		uiLayout *layout;

		/* call the menu, which will call this operator again, hence the canceled */
		pup = UI_popup_menu_begin(C, RNA_struct_ui_name(op->type->srna), ICON_NONE);
		layout = UI_popup_menu_layout(pup);
		uiItemsEnumO(layout, "ANIM_OT_keyframe_insert_menu", "type");
		UI_popup_menu_end(C, pup);

		return OPERATOR_INTERFACE;
	}
	else {
		/* just call the exec() on the active keyingset */
		RNA_enum_set(op->ptr, "type", 0);
		RNA_boolean_set(op->ptr, "confirm_success", true);

		return op->type->exec(C, op);
	}
}

void ANIM_OT_keyframe_insert_menu(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Insert Keyframe Menu";
	ot->idname = "ANIM_OT_keyframe_insert_menu";
	ot->description = "Insert Keyframes for specified Keying Set, with menu of available Keying Sets if undefined";

	/* callbacks */
	ot->invoke = insert_key_menu_invoke;
	ot->exec = insert_key_exec;
	ot->poll = ED_operator_areaactive;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* keyingset to use (dynamic enum) */
	prop = RNA_def_enum(ot->srna, "type", DummyRNA_DEFAULT_items, 0, "Keying Set", "The Keying Set to use");
	RNA_def_enum_funcs(prop, ANIM_keying_sets_enum_itemf);
	RNA_def_property_flag(prop, PROP_HIDDEN);
	ot->prop = prop;

	/* confirm whether a keyframe was added by showing a popup
	 *	- by default, this is disabled so that if a menu is shown, this doesn't come up too
	 */
	// XXX should this just be always on?
	prop = RNA_def_boolean(ot->srna, "confirm_success", 0, "Confirm Successful Insert",
	                       "Show a popup when the keyframes get successfully added");
	RNA_def_property_flag(prop, PROP_HIDDEN);

	/* whether the menu should always be shown
	 *	- by default, the menu should only be shown when there is no active Keying Set (2.5 behavior),
	 *	  although in some cases it might be useful to always shown (pre 2.5 behavior)
	 */
	prop = RNA_def_boolean(ot->srna, "always_prompt", 0, "Always Show Menu", "");
	RNA_def_property_flag(prop, PROP_HIDDEN);
}

/* Delete Key Operator ------------------------ */

static int delete_key_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	KeyingSet *ks = NULL;
	int type = RNA_enum_get(op->ptr, "type");
	float cfra = (float)CFRA; // XXX for now, don't bother about all the yucky offset crap
	short success;

	/* type is the Keying Set the user specified to use when calling the operator:
	 *	- type == 0: use scene's active Keying Set
	 *	- type > 0: use a user-defined Keying Set from the active scene
	 *	- type < 0: use a builtin Keying Set
	 */
	if (type == 0)
		type = scene->active_keyingset;
	if (type > 0)
		ks = BLI_findlink(&scene->keyingsets, type - 1);
	else
		ks = BLI_findlink(&builtin_keyingsets, -type - 1);

	/* report failure */
	if (ks == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No active Keying Set");
		return OPERATOR_CANCELLED;
	}

	/* try to delete keyframes for the channels specified by KeyingSet */
	success = ANIM_apply_keyingset(C, NULL, NULL, ks, MODIFYKEY_MODE_DELETE, cfra);
	if (G.debug & G_DEBUG)
		printf("KeyingSet '%s' - Successfully removed %d Keyframes\n", ks->name, success);

	/* report failure or do updates? */
	if (success == MODIFYKEY_INVALID_CONTEXT) {
		BKE_report(op->reports, RPT_ERROR, "No suitable context info for active keying set");
		return OPERATOR_CANCELLED;
	}
	else if (success) {
		/* if the appropriate properties have been set, make a note that we've inserted something */
		if (RNA_boolean_get(op->ptr, "confirm_success"))
			BKE_reportf(op->reports, RPT_INFO, "Successfully removed %d keyframes for keying set '%s'", success, ks->name);

		/* send notifiers that keyframes have been changed */
		WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_REMOVED, NULL);
	}
	else
		BKE_report(op->reports, RPT_WARNING, "Keying set failed to remove any keyframes");

	return OPERATOR_FINISHED;
}

void ANIM_OT_keyframe_delete(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Delete Keying-Set Keyframe";
	ot->idname = "ANIM_OT_keyframe_delete";
	ot->description = "Delete keyframes on the current frame for all properties in the specified Keying Set";

	/* callbacks */
	ot->exec = delete_key_exec;
	ot->poll = modify_key_op_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* keyingset to use (dynamic enum) */
	prop = RNA_def_enum(ot->srna, "type", DummyRNA_DEFAULT_items, 0, "Keying Set", "The Keying Set to use");
	RNA_def_enum_funcs(prop, ANIM_keying_sets_enum_itemf);
	RNA_def_property_flag(prop, PROP_HIDDEN);
	ot->prop = prop;

	/* confirm whether a keyframe was added by showing a popup
	 *	- by default, this is enabled, since this operator is assumed to be called independently
	 */
	RNA_def_boolean(ot->srna, "confirm_success", 1, "Confirm Successful Delete",
	                "Show a popup when the keyframes get successfully removed");
}

/* Delete Key Operator ------------------------ */
/* NOTE: Although this version is simpler than the more generic version for KeyingSets,
 * it is more useful for animators working in the 3D view.
 */

static int clear_anim_v3d_exec(bContext *C, wmOperator *UNUSED(op))
{
	bool changed = false;

	CTX_DATA_BEGIN (C, Object *, ob, selected_objects)
	{
		/* just those in active action... */
		if ((ob->adt) && (ob->adt->action)) {
			AnimData *adt = ob->adt;
			bAction *act = adt->action;
			FCurve *fcu, *fcn;

			for (fcu = act->curves.first; fcu; fcu = fcn) {
				bool can_delete = false;

				fcn = fcu->next;

				/* in pose mode, only delete the F-Curve if it belongs to a selected bone */
				if (ob->mode & OB_MODE_POSE) {
					if ((fcu->rna_path) && strstr(fcu->rna_path, "pose.bones[")) {
						bPoseChannel *pchan;
						char *bone_name;

						/* get bone-name, and check if this bone is selected */
						bone_name = BLI_str_quoted_substrN(fcu->rna_path, "pose.bones[");
						pchan = BKE_pose_channel_find_name(ob->pose, bone_name);
						if (bone_name) MEM_freeN(bone_name);

						/* delete if bone is selected*/
						if ((pchan) && (pchan->bone)) {
							if (pchan->bone->flag & BONE_SELECTED)
								can_delete = true;
						}
					}
				}
				else {
					/* object mode - all of Object's F-Curves are affected */
					can_delete = true;
				}

				/* delete F-Curve completely */
				if (can_delete) {
					ANIM_fcurve_delete_from_animdata(NULL, adt, fcu);
					DAG_id_tag_update(&ob->id, OB_RECALC_OB);
					changed = true;
				}
			}
		}
	}
	CTX_DATA_END;

	if (!changed) {
		return OPERATOR_CANCELLED;
	}

	/* send updates */
	WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, NULL);

	return OPERATOR_FINISHED;
}

void ANIM_OT_keyframe_clear_v3d(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Animation";
	ot->description = "Remove all keyframe animation for selected objects";
	ot->idname = "ANIM_OT_keyframe_clear_v3d";

	/* callbacks */
	ot->invoke = WM_operator_confirm;
	ot->exec = clear_anim_v3d_exec;

	ot->poll = ED_operator_areaactive;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


static int delete_key_v3d_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	float cfra = (float)CFRA;

	CTX_DATA_BEGIN (C, Object *, ob, selected_objects)
	{
		ID *id = &ob->id;
		int success = 0;

		/* just those in active action... */
		if ((ob->adt) && (ob->adt->action)) {
			AnimData *adt = ob->adt;
			bAction *act = adt->action;
			FCurve *fcu, *fcn;
			const float cfra_unmap = BKE_nla_tweakedit_remap(adt, cfra, NLATIME_CONVERT_UNMAP);

			for (fcu = act->curves.first; fcu; fcu = fcn) {
				fcn = fcu->next;

				/* don't touch protected F-Curves */
				if (BKE_fcurve_is_protected(fcu)) {
					BKE_reportf(op->reports, RPT_WARNING,
					            "Not deleting keyframe for locked F-Curve '%s', object '%s'",
					            fcu->rna_path, id->name + 2);
					continue;
				}

				/* special exception for bones, as this makes this operator more convenient to use
				 * NOTE: This is only done in pose mode. In object mode, we're dealing with the entire object.
				 */
				if ((ob->mode & OB_MODE_POSE) && strstr(fcu->rna_path, "pose.bones[\"")) {
					bPoseChannel *pchan;
					char *bone_name;

					/* get bone-name, and check if this bone is selected */
					bone_name = BLI_str_quoted_substrN(fcu->rna_path, "pose.bones[");
					pchan = BKE_pose_channel_find_name(ob->pose, bone_name);
					if (bone_name) MEM_freeN(bone_name);

					/* skip if bone is not selected */
					if ((pchan) && (pchan->bone)) {
						/* bones are only selected/editable if visible... */
						bArmature *arm = (bArmature *)ob->data;

						/* skipping - not visible on currently visible layers */
						if ((arm->layer & pchan->bone->layer) == 0)
							continue;
						/* skipping - is currently hidden */
						if (pchan->bone->flag & BONE_HIDDEN_P)
							continue;

						/* selection flag... */
						if ((pchan->bone->flag & BONE_SELECTED) == 0)
							continue;
					}
				}

				/* delete keyframes on current frame
				 * WARNING: this can delete the next F-Curve, hence the "fcn" copying
				 */
				success += delete_keyframe_fcurve(adt, fcu, cfra_unmap);
			}
		}

		/* report success (or failure) */
		if (success)
			BKE_reportf(op->reports, RPT_INFO, "Object '%s' successfully had %d keyframes removed", id->name + 2, success);
		else
			BKE_reportf(op->reports, RPT_ERROR, "No keyframes removed from Object '%s'", id->name + 2);

		DAG_id_tag_update(&ob->id, OB_RECALC_OB);
	}
	CTX_DATA_END;

	/* send updates */
	WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, NULL);

	return OPERATOR_FINISHED;
}

void ANIM_OT_keyframe_delete_v3d(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete Keyframe";
	ot->description = "Remove keyframes on current frame for selected objects and bones";
	ot->idname = "ANIM_OT_keyframe_delete_v3d";

	/* callbacks */
	ot->invoke = WM_operator_confirm;
	ot->exec = delete_key_v3d_exec;

	ot->poll = ED_operator_areaactive;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


/* Insert Key Button Operator ------------------------ */

static int insert_key_button_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = scene->toolsettings;
	PointerRNA ptr = {{NULL}};
	PropertyRNA *prop = NULL;
	char *path;
	uiBut *but;
	float cfra = (float)CFRA;
	short success = 0;
	int index;
	const bool all = RNA_boolean_get(op->ptr, "all");
	eInsertKeyFlags flag = INSERTKEY_NOFLAGS;


	/* flags for inserting keyframes */
	flag = ANIM_get_keyframing_flags(scene, 1);

	/* try to insert keyframe using property retrieved from UI */
	if (!(but = UI_context_active_but_prop_get(C, &ptr, &prop, &index))) {
		/* pass event on if no active button found */
		return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
	}

	if ((ptr.id.data && ptr.data && prop) && RNA_property_animateable(&ptr, prop)) {
		if (ptr.type == &RNA_NlaStrip) {
			/* Handle special properties for NLA Strips, whose F-Curves are stored on the
			 * strips themselves. These are stored separately or else the properties will
			 * not have any effect.
			 */
			NlaStrip *strip = (NlaStrip *)ptr.data;
			FCurve *fcu = list_find_fcurve(&strip->fcurves, RNA_property_identifier(prop), index);

			if (fcu) {
				success = insert_keyframe_direct(op->reports, ptr, prop, fcu, cfra, ts->keyframe_type, 0);
			}
			else {
				BKE_report(op->reports, RPT_ERROR,
				           "This property cannot be animated as it will not get updated correctly");
			}
		}
		else if (UI_but_flag_is_set(but, UI_BUT_DRIVEN)) {
			/* Driven property - Find driver */
			FCurve *fcu;
			bool driven, special;

			fcu = rna_get_fcurve_context_ui(C, &ptr, prop, index, NULL, NULL, &driven, &special);

			if (fcu && driven) {
				success = insert_keyframe_direct(op->reports, ptr, prop, fcu, cfra, ts->keyframe_type, INSERTKEY_DRIVER);
			}
		}
		else {
			/* standard properties */
			path = RNA_path_from_ID_to_property(&ptr, prop);

			if (path) {
				if (all) {
					/* -1 indicates operating on the entire array (or the property itself otherwise) */
					index = -1;
				}

				success = insert_keyframe(bmain, op->reports, ptr.id.data, NULL, NULL, path, index, cfra, ts->keyframe_type, flag);

				MEM_freeN(path);
			}
			else {
				BKE_report(op->reports, RPT_WARNING,
				           "Failed to resolve path to property, "
				           "try manually specifying this using a Keying Set instead");
			}
		}
	}
	else {
		if (prop && !RNA_property_animateable(&ptr, prop)) {
			BKE_reportf(op->reports, RPT_WARNING,
			           "\"%s\" property cannot be animated",
			           RNA_property_identifier(prop));
		}
		else {
			BKE_reportf(op->reports, RPT_WARNING,
			            "Button doesn't appear to have any property information attached (ptr.data = %p, prop = %p)",
			            (void *)ptr.data, (void *)prop);
		}
	}

	if (success) {
		/* send updates */
		UI_context_update_anim_flag(C);

		/* send notifiers that keyframes have been changed */
		WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_ADDED, NULL);
	}

	return (success) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void ANIM_OT_keyframe_insert_button(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Insert Keyframe (Buttons)";
	ot->idname = "ANIM_OT_keyframe_insert_button";
	ot->description = "Insert a keyframe for current UI-active property";

	/* callbacks */
	ot->exec = insert_key_button_exec;
	ot->poll = modify_key_op_poll;

	/* flags */
	ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

	/* properties */
	RNA_def_boolean(ot->srna, "all", 1, "All", "Insert a keyframe for all element of the array");
}

/* Delete Key Button Operator ------------------------ */

static int delete_key_button_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	PointerRNA ptr = {{NULL}};
	PropertyRNA *prop = NULL;
	char *path;
	float cfra = (float)CFRA; // XXX for now, don't bother about all the yucky offset crap
	short success = 0;
	int index;
	const bool all = RNA_boolean_get(op->ptr, "all");

	/* try to insert keyframe using property retrieved from UI */
	if (!UI_context_active_but_prop_get(C, &ptr, &prop, &index)) {
		/* pass event on if no active button found */
		return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
	}

	if (ptr.id.data && ptr.data && prop) {
		if (BKE_nlastrip_has_curves_for_property(&ptr, prop)) {
			/* Handle special properties for NLA Strips, whose F-Curves are stored on the
			 * strips themselves. These are stored separately or else the properties will
			 * not have any effect.
			 */
			ID *id = ptr.id.data;
			NlaStrip *strip = (NlaStrip *)ptr.data;
			FCurve *fcu = list_find_fcurve(&strip->fcurves, RNA_property_identifier(prop), 0);

			if (fcu) {
				if (BKE_fcurve_is_protected(fcu)) {
					BKE_reportf(op->reports, RPT_WARNING,
					            "Not deleting keyframe for locked F-Curve for NLA Strip influence on %s - %s '%s'",
					            strip->name, BKE_idcode_to_name(GS(id->name)), id->name + 2);
				}
				else {
					/* remove the keyframe directly
					 * NOTE: cannot use delete_keyframe_fcurve(), as that will free the curve,
					 *       and delete_keyframe() expects the FCurve to be part of an action
					 */
					bool found = false;
					int i;

					/* try to find index of beztriple to get rid of */
					i = binarysearch_bezt_index(fcu->bezt, cfra, fcu->totvert, &found);
					if (found) {
						/* delete the key at the index (will sanity check + do recalc afterwards) */
						delete_fcurve_key(fcu, i, 1);
						success = true;
					}
				}
			}
		}
		else {
			/* standard properties */
			path = RNA_path_from_ID_to_property(&ptr, prop);

			if (path) {
				if (all) {
					/* -1 indicates operating on the entire array (or the property itself otherwise) */
					index = -1;
				}

				success = delete_keyframe(op->reports, ptr.id.data, NULL, NULL, path, index, cfra, 0);
				MEM_freeN(path);
			}
			else if (G.debug & G_DEBUG)
				printf("Button Delete-Key: no path to property\n");
		}
	}
	else if (G.debug & G_DEBUG) {
		printf("ptr.data = %p, prop = %p\n", (void *)ptr.data, (void *)prop);
	}


	if (success) {
		/* send updates */
		UI_context_update_anim_flag(C);

		/* send notifiers that keyframes have been changed */
		WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_REMOVED, NULL);
	}

	return (success) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void ANIM_OT_keyframe_delete_button(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete Keyframe (Buttons)";
	ot->idname = "ANIM_OT_keyframe_delete_button";
	ot->description = "Delete current keyframe of current UI-active property";

	/* callbacks */
	ot->exec = delete_key_button_exec;
	ot->poll = modify_key_op_poll;

	/* flags */
	ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

	/* properties */
	RNA_def_boolean(ot->srna, "all", 1, "All", "Delete keyframes from all elements of the array");
}


/* Clear Key Button Operator ------------------------ */

static int clear_key_button_exec(bContext *C, wmOperator *op)
{
	PointerRNA ptr = {{NULL}};
	PropertyRNA *prop = NULL;
	char *path;
	short success = 0;
	int index;
	const bool all = RNA_boolean_get(op->ptr, "all");

	/* try to insert keyframe using property retrieved from UI */
	if (!UI_context_active_but_prop_get(C, &ptr, &prop, &index)) {
		/* pass event on if no active button found */
		return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
	}

	if (ptr.id.data && ptr.data && prop) {
		path = RNA_path_from_ID_to_property(&ptr, prop);

		if (path) {
			if (all) {
				/* -1 indicates operating on the entire array (or the property itself otherwise) */
				index = -1;
			}

			success += clear_keyframe(op->reports, ptr.id.data, NULL, NULL, path, index, 0);
			MEM_freeN(path);
		}
		else if (G.debug & G_DEBUG)
			printf("Button Clear-Key: no path to property\n");
	}
	else if (G.debug & G_DEBUG) {
		printf("ptr.data = %p, prop = %p\n", (void *)ptr.data, (void *)prop);
	}


	if (success) {
		/* send updates */
		UI_context_update_anim_flag(C);

		/* send notifiers that keyframes have been changed */
		WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_REMOVED, NULL);
	}

	return (success) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void ANIM_OT_keyframe_clear_button(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Keyframe (Buttons)";
	ot->idname = "ANIM_OT_keyframe_clear_button";
	ot->description = "Clear all keyframes on the currently active property";

	/* callbacks */
	ot->exec = clear_key_button_exec;
	ot->poll = modify_key_op_poll;

	/* flags */
	ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

	/* properties */
	RNA_def_boolean(ot->srna, "all", 1, "All", "Clear keyframes from all elements of the array");
}

/* ******************************************* */
/* AUTO KEYFRAME */

bool autokeyframe_cfra_can_key(Scene *scene, ID *id)
{
	float cfra = (float)CFRA; // XXX for now, this will do

	/* only filter if auto-key mode requires this */
	if (IS_AUTOKEY_ON(scene) == 0)
		return false;

	if (IS_AUTOKEY_MODE(scene, EDITKEYS)) {
		/* Replace Mode:
		 * For whole block, only key if there's a keyframe on that frame already
		 * This is a valid assumption when we're blocking + tweaking
		 */
		return id_frame_has_keyframe(id, cfra, ANIMFILTER_KEYS_LOCAL);
	}
	else {
		/* Normal Mode (or treat as being normal mode):
		 *
		 * Just in case the flags aren't set properly (i.e. only on/off is set, without a mode)
		 * let's set the "normal" flag too, so that it will all be sane everywhere...
		 */
		scene->toolsettings->autokey_mode = AUTOKEY_MODE_NORMAL;

		/* Can insert anytime we like... */
		return true;
	}
}

/* ******************************************* */
/* KEYFRAME DETECTION */

/* --------------- API/Per-Datablock Handling ------------------- */

/* Checks if some F-Curve has a keyframe for a given frame */
bool fcurve_frame_has_keyframe(FCurve *fcu, float frame, short filter)
{
	/* quick sanity check */
	if (ELEM(NULL, fcu, fcu->bezt))
		return false;

	/* we either include all regardless of muting, or only non-muted  */
	if ((filter & ANIMFILTER_KEYS_MUTED) || (fcu->flag & FCURVE_MUTED) == 0) {
		bool replace;
		int i = binarysearch_bezt_index(fcu->bezt, frame, fcu->totvert, &replace);

		/* binarysearch_bezt_index will set replace to be 0 or 1
		 * - obviously, 1 represents a match
		 */
		if (replace) {
			/* sanity check: 'i' may in rare cases exceed arraylen */
			if ((i >= 0) && (i < fcu->totvert))
				return true;
		}
	}

	return false;
}

/* Checks whether an Action has a keyframe for a given frame
 * Since we're only concerned whether a keyframe exists, we can simply loop until a match is found...
 */
static bool action_frame_has_keyframe(bAction *act, float frame, short filter)
{
	FCurve *fcu;

	/* can only find if there is data */
	if (act == NULL)
		return false;

	/* if only check non-muted, check if muted */
	if ((filter & ANIMFILTER_KEYS_MUTED) || (act->flag & ACT_MUTED))
		return false;

	/* loop over F-Curves, using binary-search to try to find matches
	 *	- this assumes that keyframes are only beztriples
	 */
	for (fcu = act->curves.first; fcu; fcu = fcu->next) {
		/* only check if there are keyframes (currently only of type BezTriple) */
		if (fcu->bezt && fcu->totvert) {
			if (fcurve_frame_has_keyframe(fcu, frame, filter))
				return true;
		}
	}

	/* nothing found */
	return false;
}

/* Checks whether an Object has a keyframe for a given frame */
static bool object_frame_has_keyframe(Object *ob, float frame, short filter)
{
	/* error checking */
	if (ob == NULL)
		return false;

	/* check own animation data - specifically, the action it contains */
	if ((ob->adt) && (ob->adt->action)) {
		/* T41525 - When the active action is a NLA strip being edited,
		 * we need to correct the frame number to "look inside" the
		 * remapped action
		 */
		float ob_frame = BKE_nla_tweakedit_remap(ob->adt, frame, NLATIME_CONVERT_UNMAP);

		if (action_frame_has_keyframe(ob->adt->action, ob_frame, filter))
			return true;
	}

	/* try shapekey keyframes (if available, and allowed by filter) */
	if (!(filter & ANIMFILTER_KEYS_LOCAL) && !(filter & ANIMFILTER_KEYS_NOSKEY)) {
		Key *key = BKE_key_from_object(ob);

		/* shapekeys can have keyframes ('Relative Shape Keys')
		 * or depend on time (old 'Absolute Shape Keys')
		 */

		/* 1. test for relative (with keyframes) */
		if (id_frame_has_keyframe((ID *)key, frame, filter))
			return true;

		/* 2. test for time */
		/* TODO... yet to be implemented (this feature may evolve before then anyway) */
	}

	/* try materials */
	if (!(filter & ANIMFILTER_KEYS_LOCAL) && !(filter & ANIMFILTER_KEYS_NOMAT)) {
		/* if only active, then we can skip a lot of looping */
		if (filter & ANIMFILTER_KEYS_ACTIVE) {
			Material *ma = give_current_material(ob, (ob->actcol + 1));

			/* we only retrieve the active material... */
			if (id_frame_has_keyframe((ID *)ma, frame, filter))
				return true;
		}
		else {
			int a;

			/* loop over materials */
			for (a = 0; a < ob->totcol; a++) {
				Material *ma = give_current_material(ob, a + 1);

				if (id_frame_has_keyframe((ID *)ma, frame, filter))
					return true;
			}
		}
	}

	/* nothing found */
	return false;
}

/* --------------- API ------------------- */

/* Checks whether a keyframe exists for the given ID-block one the given frame */
bool id_frame_has_keyframe(ID *id, float frame, short filter)
{
	/* sanity checks */
	if (id == NULL)
		return false;

	/* perform special checks for 'macro' types */
	switch (GS(id->name)) {
		case ID_OB: /* object */
			return object_frame_has_keyframe((Object *)id, frame, filter);
#if 0
		// XXX TODO... for now, just use 'normal' behavior
		case ID_SCE: /* scene */
			break;
#endif
		default:  /* 'normal type' */
		{
			AnimData *adt = BKE_animdata_from_id(id);

			/* only check keyframes in active action */
			if (adt)
				return action_frame_has_keyframe(adt->action, frame, filter);
			break;
		}
	}


	/* no keyframe found */
	return false;
}

/* ************************************************** */

bool ED_autokeyframe_object(bContext *C, Scene *scene, Object *ob, KeyingSet *ks)
{
	/* auto keyframing */
	if (autokeyframe_cfra_can_key(scene, &ob->id)) {
		ListBase dsources = {NULL, NULL};

		/* now insert the keyframe(s) using the Keying Set
		 *	1) add datasource override for the Object
		 *	2) insert keyframes
		 *	3) free the extra info
		 */
		ANIM_relative_keyingset_add_source(&dsources, &ob->id, NULL, NULL);
		ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, (float)CFRA);
		BLI_freelistN(&dsources);

		return true;
	}
	else {
		return false;
	}
}

bool ED_autokeyframe_pchan(bContext *C, Scene *scene, Object *ob, bPoseChannel *pchan, KeyingSet *ks)
{
	if (autokeyframe_cfra_can_key(scene, &ob->id)) {
		ListBase dsources = {NULL, NULL};

		/* now insert the keyframe(s) using the Keying Set
		 *	1) add datasource override for the PoseChannel
		 *	2) insert keyframes
		 *	3) free the extra info
		 */
		ANIM_relative_keyingset_add_source(&dsources, &ob->id, &RNA_PoseBone, pchan);
		ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, (float)CFRA);
		BLI_freelistN(&dsources);

		/* clear any unkeyed tags */
		if (pchan->bone) {
			pchan->bone->flag &= ~BONE_UNKEYED;
		}

		return true;
	}
	else {
		/* add unkeyed tags */
		if (pchan->bone) {
			pchan->bone->flag |= BONE_UNKEYED;
		}

		return false;
	}
}
