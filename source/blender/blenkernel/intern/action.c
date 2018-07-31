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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Full recode, Ton Roosendaal, Crete 2005
 *				 Full recode, Joshua Leung, 2009
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/action.c
 *  \ingroup bke
 */


#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "BLT_translation.h"

#include "BKE_action.h"
#include "BKE_anim.h"
#include "BKE_animsys.h"
#include "BKE_constraint.h"
#include "BKE_deform.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_library_remap.h"
#include "BKE_main.h"
#include "BKE_object.h"

#include "DEG_depsgraph_build.h"

#include "BIK_api.h"

#include "RNA_access.h"

/* *********************** NOTE ON POSE AND ACTION **********************
 *
 * - Pose is the local (object level) component of armature. The current
 *   object pose is saved in files, and (will be) is presorted for dependency
 * - Actions have fewer (or other) channels, and write data to a Pose
 * - Currently ob->pose data is controlled in BKE_pose_where_is only. The (recalc)
 *   event system takes care of calling that
 * - The NLA system (here too) uses Poses as interpolation format for Actions
 * - Therefore we assume poses to be static, and duplicates of poses have channels in
 *   same order, for quick interpolation reasons
 *
 * ****************************** (ton) ************************************ */

/* ***************** Library data level operations on action ************** */

bAction *BKE_action_add(Main *bmain, const char name[])
{
	bAction *act;

	act = BKE_libblock_alloc(bmain, ID_AC, name, 0);

	return act;
}

/* .................................. */

// does copy_fcurve...
void BKE_action_make_local(Main *bmain, bAction *act, const bool lib_local)
{
	BKE_id_make_local_generic(bmain, &act->id, true, lib_local);
}

/* .................................. */

/** Free (or release) any data used by this action (does not free the action itself). */
void BKE_action_free(bAction *act)
{
	/* No animdata here. */

	/* Free F-Curves */
	free_fcurves(&act->curves);

	/* Free groups */
	BLI_freelistN(&act->groups);

	/* Free pose-references (aka local markers) */
	BLI_freelistN(&act->markers);
}

/* .................................. */

/**
 * Only copy internal data of Action ID from source to already allocated/initialized destination.
 * You probably nerver want to use that directly, use id_copy or BKE_id_copy_ex for typical needs.
 *
 * WARNING! This function will not handle ID user count!
 *
 * \param flag  Copying options (see BKE_library.h's LIB_ID_COPY_... flags for more).
 */
void BKE_action_copy_data(Main *UNUSED(bmain), bAction *act_dst, const bAction *act_src, const int UNUSED(flag))
{
	bActionGroup *grp_dst, *grp_src;
	FCurve *fcu_dst, *fcu_src;

	/* duplicate the lists of groups and markers */
	BLI_duplicatelist(&act_dst->groups, &act_src->groups);
	BLI_duplicatelist(&act_dst->markers, &act_src->markers);

	/* copy F-Curves, fixing up the links as we go */
	BLI_listbase_clear(&act_dst->curves);

	for (fcu_src = act_src->curves.first; fcu_src; fcu_src = fcu_src->next) {
		/* duplicate F-Curve */
		fcu_dst = copy_fcurve(fcu_src);  /* XXX TODO pass subdata flag? But surprisingly does not seem to be doing any ID refcounting... */
		BLI_addtail(&act_dst->curves, fcu_dst);

		/* fix group links (kindof bad list-in-list search, but this is the most reliable way) */
		for (grp_dst = act_dst->groups.first, grp_src = act_src->groups.first;
		     grp_dst && grp_src;
		     grp_dst = grp_dst->next, grp_src = grp_src->next)
		{
			if (fcu_src->grp == grp_src) {
				fcu_dst->grp = grp_dst;

				if (grp_dst->channels.first == fcu_src) {
					grp_dst->channels.first = fcu_dst;
				}
				if (grp_dst->channels.last == fcu_src) {
					grp_dst->channels.last = fcu_dst;
				}
				break;
			}
		}
	}
}

bAction *BKE_action_copy(Main *bmain, const bAction *act_src)
{
	bAction *act_copy;
	BKE_id_copy_ex(bmain, &act_src->id, (ID **)&act_copy, 0, false);
	return act_copy;
}

/* *************** Action Groups *************** */

/* Get the active action-group for an Action */
bActionGroup *get_active_actiongroup(bAction *act)
{
	bActionGroup *agrp = NULL;

	if (act && act->groups.first) {
		for (agrp = act->groups.first; agrp; agrp = agrp->next) {
			if (agrp->flag & AGRP_ACTIVE)
				break;
		}
	}

	return agrp;
}

/* Make the given Action-Group the active one */
void set_active_action_group(bAction *act, bActionGroup *agrp, short select)
{
	bActionGroup *grp;

	/* sanity checks */
	if (act == NULL)
		return;

	/* Deactive all others */
	for (grp = act->groups.first; grp; grp = grp->next) {
		if ((grp == agrp) && (select))
			grp->flag |= AGRP_ACTIVE;
		else
			grp->flag &= ~AGRP_ACTIVE;
	}
}

/* Sync colors used for action/bone group with theme settings */
void action_group_colors_sync(bActionGroup *grp, const bActionGroup *ref_grp)
{
	/* only do color copying if using a custom color (i.e. not default color)  */
	if (grp->customCol) {
		if (grp->customCol > 0) {
			/* copy theme colors on-to group's custom color in case user tries to edit color */
			bTheme *btheme = U.themes.first;
			ThemeWireColor *col_set = &btheme->tarm[(grp->customCol - 1)];

			memcpy(&grp->cs, col_set, sizeof(ThemeWireColor));
		}
		else {
			/* if a reference group is provided, use the custom color from there... */
			if (ref_grp) {
				/* assumption: reference group has a color set */
				memcpy(&grp->cs, &ref_grp->cs, sizeof(ThemeWireColor));
			}
			/* otherwise, init custom color with a generic/placeholder color set if
			 * no previous theme color was used that we can just keep using
			 */
			else if (grp->cs.solid[0] == 0) {
				/* define for setting colors in theme below */
				rgba_char_args_set(grp->cs.solid, 0xff, 0x00, 0x00, 255);
				rgba_char_args_set(grp->cs.select, 0x81, 0xe6, 0x14, 255);
				rgba_char_args_set(grp->cs.active, 0x18, 0xb6, 0xe0, 255);
			}
		}
	}
}

/* Add a new action group with the given name to the action */
bActionGroup *action_groups_add_new(bAction *act, const char name[])
{
	bActionGroup *agrp;

	/* sanity check: must have action and name */
	if (ELEM(NULL, act, name))
		return NULL;

	/* allocate a new one */
	agrp = MEM_callocN(sizeof(bActionGroup), "bActionGroup");

	/* make it selected, with default name */
	agrp->flag = AGRP_SELECTED;
	BLI_strncpy(agrp->name, name[0] ? name : DATA_("Group"), sizeof(agrp->name));

	/* add to action, and validate */
	BLI_addtail(&act->groups, agrp);
	BLI_uniquename(&act->groups, agrp, DATA_("Group"), '.', offsetof(bActionGroup, name), sizeof(agrp->name));

	/* return the new group */
	return agrp;
}

/* Add given channel into (active) group
 *	- assumes that channel is not linked to anything anymore
 *	- always adds at the end of the group
 */
void action_groups_add_channel(bAction *act, bActionGroup *agrp, FCurve *fcurve)
{
	/* sanity checks */
	if (ELEM(NULL, act, agrp, fcurve))
		return;

	/* if no channels anywhere, just add to two lists at the same time */
	if (BLI_listbase_is_empty(&act->curves)) {
		fcurve->next = fcurve->prev = NULL;

		agrp->channels.first = agrp->channels.last = fcurve;
		act->curves.first = act->curves.last = fcurve;
	}

	/* if the group already has channels, the F-Curve can simply be added to the list
	 * (i.e. as the last channel in the group)
	 */
	else if (agrp->channels.first) {
		/* if the group's last F-Curve is the action's last F-Curve too,
		 * then set the F-Curve as the last for the action first so that
		 * the lists will be in sync after linking
		 */
		if (agrp->channels.last == act->curves.last)
			act->curves.last = fcurve;

		/* link in the given F-Curve after the last F-Curve in the group,
		 * which means that it should be able to fit in with the rest of the
		 * list seamlessly
		 */
		BLI_insertlinkafter(&agrp->channels, agrp->channels.last, fcurve);
	}

	/* otherwise, need to find the nearest F-Curve in group before/after current to link with */
	else {
		bActionGroup *grp;

		/* firstly, link this F-Curve to the group */
		agrp->channels.first = agrp->channels.last = fcurve;

		/* step through the groups preceding this one, finding the F-Curve there to attach this one after */
		for (grp = agrp->prev; grp; grp = grp->prev) {
			/* if this group has F-Curves, we want weave the given one in right after the last channel there,
			 * but via the Action's list not this group's list
			 *	- this is so that the F-Curve is in the right place in the Action,
			 *	  but won't be included in the previous group
			 */
			if (grp->channels.last) {
				/* once we've added, break here since we don't need to search any further... */
				BLI_insertlinkafter(&act->curves, grp->channels.last, fcurve);
				break;
			}
		}

		/* if grp is NULL, that means we fell through, and this F-Curve should be added as the new first
		 * since group is (effectively) the first group. Thus, the existing first F-Curve becomes the
		 * second in the chain, etc. etc.
		 */
		if (grp == NULL)
			BLI_insertlinkbefore(&act->curves, act->curves.first, fcurve);
	}

	/* set the F-Curve's new group */
	fcurve->grp = agrp;
}

/* Remove the given channel from all groups */
void action_groups_remove_channel(bAction *act, FCurve *fcu)
{
	/* sanity checks */
	if (ELEM(NULL, act, fcu))
		return;

	/* check if any group used this directly */
	if (fcu->grp) {
		bActionGroup *agrp = fcu->grp;

		if (agrp->channels.first == agrp->channels.last) {
			if (agrp->channels.first == fcu) {
				BLI_listbase_clear(&agrp->channels);
			}
		}
		else if (agrp->channels.first == fcu) {
			if ((fcu->next) && (fcu->next->grp == agrp))
				agrp->channels.first = fcu->next;
			else
				agrp->channels.first = NULL;
		}
		else if (agrp->channels.last == fcu) {
			if ((fcu->prev) && (fcu->prev->grp == agrp))
				agrp->channels.last = fcu->prev;
			else
				agrp->channels.last = NULL;
		}

		fcu->grp = NULL;
	}

	/* now just remove from list */
	BLI_remlink(&act->curves, fcu);
}

/* Find a group with the given name */
bActionGroup *BKE_action_group_find_name(bAction *act, const char name[])
{
	/* sanity checks */
	if (ELEM(NULL, act, act->groups.first, name) || (name[0] == 0))
		return NULL;

	/* do string comparisons */
	return BLI_findstring(&act->groups, name, offsetof(bActionGroup, name));
}

/* Clear all 'temp' flags on all groups */
void action_groups_clear_tempflags(bAction *act)
{
	bActionGroup *agrp;

	/* sanity checks */
	if (ELEM(NULL, act, act->groups.first))
		return;

	/* flag clearing loop */
	for (agrp = act->groups.first; agrp; agrp = agrp->next)
		agrp->flag &= ~AGRP_TEMP;
}

/* *************** Pose channels *************** */

/**
 * Return a pointer to the pose channel of the given name
 * from this pose.
 */
bPoseChannel *BKE_pose_channel_find_name(const bPose *pose, const char *name)
{
	if (ELEM(NULL, pose, name) || (name[0] == '\0'))
		return NULL;

	if (pose->chanhash)
		return BLI_ghash_lookup(pose->chanhash, (const void *)name);

	return BLI_findstring(&((const bPose *)pose)->chanbase, name, offsetof(bPoseChannel, name));
}

/**
 * Looks to see if the channel with the given name
 * already exists in this pose - if not a new one is
 * allocated and initialized.
 *
 * \note Use with care, not on Armature poses but for temporal ones.
 * \note (currently used for action constraints and in rebuild_pose).
 */
bPoseChannel *BKE_pose_channel_verify(bPose *pose, const char *name)
{
	bPoseChannel *chan;

	if (pose == NULL)
		return NULL;

	/* See if this channel exists */
	chan = BKE_pose_channel_find_name(pose, name);
	if (chan) {
		return chan;
	}

	/* If not, create it and add it */
	chan = MEM_callocN(sizeof(bPoseChannel), "verifyPoseChannel");

	BLI_strncpy(chan->name, name, sizeof(chan->name));

	chan->custom_scale = 1.0f;

	/* init vars to prevent math errors */
	unit_qt(chan->quat);
	unit_axis_angle(chan->rotAxis, &chan->rotAngle);
	chan->size[0] = chan->size[1] = chan->size[2] = 1.0f;

	chan->scaleIn = chan->scaleOut = 1.0f;

	chan->limitmin[0] = chan->limitmin[1] = chan->limitmin[2] = -M_PI;
	chan->limitmax[0] = chan->limitmax[1] = chan->limitmax[2] = M_PI;
	chan->stiffness[0] = chan->stiffness[1] = chan->stiffness[2] = 0.0f;
	chan->ikrotweight = chan->iklinweight = 0.0f;
	unit_m4(chan->constinv);

	chan->protectflag = OB_LOCK_ROT4D;  /* lock by components by default */

	BLI_addtail(&pose->chanbase, chan);
	if (pose->chanhash) {
		BLI_ghash_insert(pose->chanhash, chan->name, chan);
	}

	return chan;
}

#ifndef NDEBUG
bool BKE_pose_channels_is_valid(const bPose *pose)
{
	if (pose->chanhash) {
		bPoseChannel *pchan;
		for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
			if (BLI_ghash_lookup(pose->chanhash, pchan->name) != pchan) {
				return false;
			}
		}
	}

	return true;
}

#endif

/**
 * Find the active posechannel for an object (we can't just use pose, as layer info is in armature)
 *
 * \note: Object, not bPose is used here, as we need layer info from Armature)
 */
bPoseChannel *BKE_pose_channel_active(Object *ob)
{
	bArmature *arm = (ob) ? ob->data : NULL;
	bPoseChannel *pchan;

	if (ELEM(NULL, ob, ob->pose, arm)) {
		return NULL;
	}

	/* find active */
	for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
		if ((pchan->bone) && (pchan->bone == arm->act_bone) && (pchan->bone->layer & arm->layer))
			return pchan;
	}

	return NULL;
}

/**
 * \see #ED_armature_ebone_get_mirrored (edit-mode, matching function)
 */
bPoseChannel *BKE_pose_channel_get_mirrored(const bPose *pose, const char *name)
{
	char name_flip[MAXBONENAME];

	BLI_string_flip_side_name(name_flip, name, false, sizeof(name_flip));

	if (!STREQ(name_flip, name)) {
		return BKE_pose_channel_find_name(pose, name_flip);
	}

	return NULL;
}

const char *BKE_pose_ikparam_get_name(bPose *pose)
{
	if (pose) {
		switch (pose->iksolver) {
			case IKSOLVER_STANDARD:
				return NULL;
			case IKSOLVER_ITASC:
				return "bItasc";
		}
	}
	return NULL;
}

/**
 * Allocate a new pose on the heap, and copy the src pose and it's channels
 * into the new pose. *dst is set to the newly allocated structure, and assumed to be NULL.
 *
 * \param dst  Should be freed already, makes entire duplicate.
 */
void BKE_pose_copy_data_ex(bPose **dst, const bPose *src, const int flag, const bool copy_constraints)
{
	bPose *outPose;
	bPoseChannel *pchan;
	ListBase listb;

	if (!src) {
		*dst = NULL;
		return;
	}

	outPose = MEM_callocN(sizeof(bPose), "pose");

	BLI_duplicatelist(&outPose->chanbase, &src->chanbase);

	/* Rebuild ghash here too, so that name lookups below won't be too bad...
	 * BUT this will have the penalty that the ghash will be built twice
	 * if BKE_pose_rebuild() gets called after this...
	 */
	if (outPose->chanbase.first != outPose->chanbase.last) {
		outPose->chanhash = NULL;
		BKE_pose_channels_hash_make(outPose);
	}

	outPose->iksolver = src->iksolver;
	outPose->ikdata = NULL;
	outPose->ikparam = MEM_dupallocN(src->ikparam);
	outPose->avs = src->avs;

	for (pchan = outPose->chanbase.first; pchan; pchan = pchan->next) {
		if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
			id_us_plus((ID *)pchan->custom);
		}

		/* warning, O(n2) here, if done without the hash, but these are rarely used features. */
		if (pchan->custom_tx) {
			pchan->custom_tx = BKE_pose_channel_find_name(outPose, pchan->custom_tx->name);
		}
		if (pchan->bbone_prev) {
			pchan->bbone_prev = BKE_pose_channel_find_name(outPose, pchan->bbone_prev->name);
		}
		if (pchan->bbone_next) {
			pchan->bbone_next = BKE_pose_channel_find_name(outPose, pchan->bbone_next->name);
		}

		if (copy_constraints) {
			BKE_constraints_copy_ex(&listb, &pchan->constraints, flag, true);  // BKE_constraints_copy NULLs listb
			pchan->constraints = listb;

			/* XXX: This is needed for motionpath drawing to work. Dunno why it was setting to null before... */
			pchan->mpath = animviz_copy_motionpath(pchan->mpath);
		}

		if (pchan->prop) {
			pchan->prop = IDP_CopyProperty_ex(pchan->prop, flag);
		}

		pchan->draw_data = NULL;  /* Drawing cache, no need to copy. */
	}

	/* for now, duplicate Bone Groups too when doing this */
	if (copy_constraints) {
		BLI_duplicatelist(&outPose->agroups, &src->agroups);
	}

	*dst = outPose;
}

void BKE_pose_copy_data(bPose **dst, const bPose *src, const bool copy_constraints)
{
	BKE_pose_copy_data_ex(dst, src, 0, copy_constraints);
}

void BKE_pose_itasc_init(bItasc *itasc)
{
	if (itasc) {
		itasc->iksolver = IKSOLVER_ITASC;
		itasc->minstep = 0.01f;
		itasc->maxstep = 0.06f;
		itasc->numiter = 100;
		itasc->numstep = 4;
		itasc->precision = 0.005f;
		itasc->flag = ITASC_AUTO_STEP | ITASC_INITIAL_REITERATION;
		itasc->feedback = 20.0f;
		itasc->maxvel = 50.0f;
		itasc->solver = ITASC_SOLVER_SDLS;
		itasc->dampmax = 0.5;
		itasc->dampeps = 0.15;
	}
}
void BKE_pose_ikparam_init(bPose *pose)
{
	bItasc *itasc;
	switch (pose->iksolver) {
		case IKSOLVER_ITASC:
			itasc = MEM_callocN(sizeof(bItasc), "itasc");
			BKE_pose_itasc_init(itasc);
			pose->ikparam = itasc;
			break;
		case IKSOLVER_STANDARD:
		default:
			pose->ikparam = NULL;
			break;
	}
}


/* only for real IK, not for auto-IK */
static bool pose_channel_in_IK_chain(Object *ob, bPoseChannel *pchan, int level)
{
	bConstraint *con;
	Bone *bone;

	/* No need to check if constraint is active (has influence),
	 * since all constraints with CONSTRAINT_IK_AUTO are active */
	for (con = pchan->constraints.first; con; con = con->next) {
		if (con->type == CONSTRAINT_TYPE_KINEMATIC) {
			bKinematicConstraint *data = con->data;
			if ((data->rootbone == 0) || (data->rootbone > level)) {
				if ((data->flag & CONSTRAINT_IK_AUTO) == 0)
					return true;
			}
		}
	}
	for (bone = pchan->bone->childbase.first; bone; bone = bone->next) {
		pchan = BKE_pose_channel_find_name(ob->pose, bone->name);
		if (pchan && pose_channel_in_IK_chain(ob, pchan, level + 1))
			return true;
	}
	return false;
}

bool BKE_pose_channel_in_IK_chain(Object *ob, bPoseChannel *pchan)
{
	return pose_channel_in_IK_chain(ob, pchan, 0);
}

/**
 * Removes the hash for quick lookup of channels, must
 * be done when adding/removing channels.
 */
void BKE_pose_channels_hash_make(bPose *pose)
{
	if (!pose->chanhash) {
		bPoseChannel *pchan;

		pose->chanhash = BLI_ghash_str_new("make_pose_chan gh");
		for (pchan = pose->chanbase.first; pchan; pchan = pchan->next)
			BLI_ghash_insert(pose->chanhash, pchan->name, pchan);
	}
}

void BKE_pose_channels_hash_free(bPose *pose)
{
	if (pose->chanhash) {
		BLI_ghash_free(pose->chanhash, NULL, NULL);
		pose->chanhash = NULL;
	}
}

/**
 * Selectively remove pose channels.
 */
void BKE_pose_channels_remove(
        Object *ob,
        bool (*filter_fn)(const char *bone_name, void *user_data), void *user_data)
{
	/* Erase any associated pose channel, along with any references to them */
	if (ob->pose) {
		bPoseChannel *pchan, *pchan_next;
		bConstraint *con;

		for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan_next) {
			pchan_next = pchan->next;

			if (filter_fn(pchan->name, user_data)) {
				/* Bone itself is being removed */
				BKE_pose_channel_free(pchan);
				if (ob->pose->chanhash) {
					BLI_ghash_remove(ob->pose->chanhash, pchan->name, NULL, NULL);
				}
				BLI_freelinkN(&ob->pose->chanbase, pchan);
			}
			else {
				/* Maybe something the bone references is being removed instead? */
				for (con = pchan->constraints.first; con; con = con->next) {
					const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
					ListBase targets = {NULL, NULL};
					bConstraintTarget *ct;

					if (cti && cti->get_constraint_targets) {
						cti->get_constraint_targets(con, &targets);

						for (ct = targets.first; ct; ct = ct->next) {
							if (ct->tar == ob) {
								if (ct->subtarget[0]) {
									if (filter_fn(ct->subtarget, user_data)) {
										con->flag |= CONSTRAINT_DISABLE;
										ct->subtarget[0] = 0;
									}
								}
							}
						}

						if (cti->flush_constraint_targets)
							cti->flush_constraint_targets(con, &targets, 0);
					}
				}

				if (pchan->bbone_prev) {
					if (filter_fn(pchan->bbone_prev->name, user_data))
						pchan->bbone_prev = NULL;
				}
				if (pchan->bbone_next) {
					if (filter_fn(pchan->bbone_next->name, user_data))
						pchan->bbone_next = NULL;
				}

				if (pchan->custom_tx) {
					if (filter_fn(pchan->custom_tx->name, user_data))
						pchan->custom_tx = NULL;
				}
			}
		}
	}
}

/**
 * Deallocates a pose channel.
 * Does not free the pose channel itself.
 */
void BKE_pose_channel_free_ex(bPoseChannel *pchan, bool do_id_user)
{
	if (pchan->custom) {
		if (do_id_user) {
			id_us_min(&pchan->custom->id);
		}
		pchan->custom = NULL;
	}

	if (pchan->mpath) {
		animviz_free_motionpath(pchan->mpath);
		pchan->mpath = NULL;
	}

	BKE_constraints_free_ex(&pchan->constraints, do_id_user);

	if (pchan->prop) {
		IDP_FreeProperty(pchan->prop);
		MEM_freeN(pchan->prop);
	}

	/* Cached data, for new draw manager rendering code. */
	MEM_SAFE_FREE(pchan->draw_data);
}

void BKE_pose_channel_free(bPoseChannel *pchan)
{
	BKE_pose_channel_free_ex(pchan, true);
}

/**
 * Removes and deallocates all channels from a pose.
 * Does not free the pose itself.
 */
void BKE_pose_channels_free_ex(bPose *pose, bool do_id_user)
{
	bPoseChannel *pchan;

	if (pose->chanbase.first) {
		for (pchan = pose->chanbase.first; pchan; pchan = pchan->next)
			BKE_pose_channel_free_ex(pchan, do_id_user);

		BLI_freelistN(&pose->chanbase);
	}

	BKE_pose_channels_hash_free(pose);
}

void BKE_pose_channels_free(bPose *pose)
{
	BKE_pose_channels_free_ex(pose, true);
}

void BKE_pose_free_data_ex(bPose *pose, bool do_id_user)
{
	/* free pose-channels */
	BKE_pose_channels_free_ex(pose, do_id_user);

	/* free pose-groups */
	if (pose->agroups.first)
		BLI_freelistN(&pose->agroups);

	/* free IK solver state */
	BIK_clear_data(pose);

	/* free IK solver param */
	if (pose->ikparam)
		MEM_freeN(pose->ikparam);
}

void BKE_pose_free_data(bPose *pose)
{
	BKE_pose_free_data_ex(pose, true);
}

/**
 * Removes and deallocates all data from a pose, and also frees the pose.
 */
void BKE_pose_free_ex(bPose *pose, bool do_id_user)
{
	if (pose) {
		BKE_pose_free_data_ex(pose, do_id_user);
		/* free pose */
		MEM_freeN(pose);
	}
}

void BKE_pose_free(bPose *pose)
{
	BKE_pose_free_ex(pose, true);
}

/**
 * Copy the internal members of each pose channel including constraints
 * and ID-Props, used when duplicating bones in editmode.
 * (unlike copy_pose_channel_data which only does posing-related stuff).
 *
 * \note use when copying bones in editmode (on returned value from #BKE_pose_channel_verify)
 */
void BKE_pose_channel_copy_data(bPoseChannel *pchan, const bPoseChannel *pchan_from)
{
	/* copy transform locks */
	pchan->protectflag = pchan_from->protectflag;

	/* copy rotation mode */
	pchan->rotmode = pchan_from->rotmode;

	/* copy bone group */
	pchan->agrp_index = pchan_from->agrp_index;

	/* ik (dof) settings */
	pchan->ikflag = pchan_from->ikflag;
	copy_v3_v3(pchan->limitmin, pchan_from->limitmin);
	copy_v3_v3(pchan->limitmax, pchan_from->limitmax);
	copy_v3_v3(pchan->stiffness, pchan_from->stiffness);
	pchan->ikstretch = pchan_from->ikstretch;
	pchan->ikrotweight = pchan_from->ikrotweight;
	pchan->iklinweight = pchan_from->iklinweight;

	/* bbone settings (typically not animated) */
	pchan->bboneflag = pchan_from->bboneflag;
	pchan->bbone_next = pchan_from->bbone_next;
	pchan->bbone_prev = pchan_from->bbone_prev;

	/* constraints */
	BKE_constraints_copy(&pchan->constraints, &pchan_from->constraints, true);

	/* id-properties */
	if (pchan->prop) {
		/* unlikely but possible it exists */
		IDP_FreeProperty(pchan->prop);
		MEM_freeN(pchan->prop);
		pchan->prop = NULL;
	}
	if (pchan_from->prop) {
		pchan->prop = IDP_CopyProperty(pchan_from->prop);
	}

	/* custom shape */
	pchan->custom = pchan_from->custom;
	if (pchan->custom) {
		id_us_plus(&pchan->custom->id);
	}

	pchan->custom_scale = pchan_from->custom_scale;
}


/* checks for IK constraint, Spline IK, and also for Follow-Path constraint.
 * can do more constraints flags later
 */
/* pose should be entirely OK */
void BKE_pose_update_constraint_flags(bPose *pose)
{
	bPoseChannel *pchan, *parchan;
	bConstraint *con;

	/* clear */
	for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
		pchan->constflag = 0;
	}
	pose->flag &= ~POSE_CONSTRAINTS_TIMEDEPEND;

	/* detect */
	for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
		for (con = pchan->constraints.first; con; con = con->next) {
			if (con->type == CONSTRAINT_TYPE_KINEMATIC) {
				bKinematicConstraint *data = (bKinematicConstraint *)con->data;

				pchan->constflag |= PCHAN_HAS_IK;

				if (data->tar == NULL || (data->tar->type == OB_ARMATURE && data->subtarget[0] == 0))
					pchan->constflag |= PCHAN_HAS_TARGET;

				/* negative rootbone = recalc rootbone index. used in do_versions */
				if (data->rootbone < 0) {
					data->rootbone = 0;

					if (data->flag & CONSTRAINT_IK_TIP) parchan = pchan;
					else parchan = pchan->parent;

					while (parchan) {
						data->rootbone++;
						if ((parchan->bone->flag & BONE_CONNECTED) == 0)
							break;
						parchan = parchan->parent;
					}
				}
			}
			else if (con->type == CONSTRAINT_TYPE_FOLLOWPATH) {
				bFollowPathConstraint *data = (bFollowPathConstraint *)con->data;

				/* for drawing constraint colors when color set allows this */
				pchan->constflag |= PCHAN_HAS_CONST;

				/* if we have a valid target, make sure that this will get updated on frame-change
				 * (needed for when there is no anim-data for this pose)
				 */
				if ((data->tar) && (data->tar->type == OB_CURVE))
					pose->flag |= POSE_CONSTRAINTS_TIMEDEPEND;
			}
			else if (con->type == CONSTRAINT_TYPE_SPLINEIK)
				pchan->constflag |= PCHAN_HAS_SPLINEIK;
			else
				pchan->constflag |= PCHAN_HAS_CONST;
		}
	}
	pose->flag &= ~POSE_CONSTRAINTS_NEED_UPDATE_FLAGS;
}

void BKE_pose_tag_update_constraint_flags(bPose *pose)
{
	pose->flag |= POSE_CONSTRAINTS_NEED_UPDATE_FLAGS;
}

/* Clears all BONE_UNKEYED flags for every pose channel in every pose
 * This should only be called on frame changing, when it is acceptable to
 * do this. Otherwise, these flags should not get cleared as poses may get lost.
 */
void framechange_poses_clear_unkeyed(Main *bmain)
{
	Object *ob;
	bPose *pose;
	bPoseChannel *pchan;

	/* This needs to be done for each object that has a pose */
	/* TODO: proxies may/may not be correctly handled here... (this needs checking) */
	for (ob = bmain->object.first; ob; ob = ob->id.next) {
		/* we only need to do this on objects with a pose */
		if ((pose = ob->pose)) {
			for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
				if (pchan->bone)
					pchan->bone->flag &= ~BONE_UNKEYED;
			}
		}
	}
}

/* ************************** Bone Groups ************************** */

/* Adds a new bone-group (name may be NULL) */
bActionGroup *BKE_pose_add_group(bPose *pose, const char *name)
{
	bActionGroup *grp;

	if (!name) {
		name = DATA_("Group");
	}

	grp = MEM_callocN(sizeof(bActionGroup), "PoseGroup");
	BLI_strncpy(grp->name, name, sizeof(grp->name));
	BLI_addtail(&pose->agroups, grp);
	BLI_uniquename(&pose->agroups, grp, name, '.', offsetof(bActionGroup, name), sizeof(grp->name));

	pose->active_group = BLI_listbase_count(&pose->agroups);

	return grp;
}

/* Remove the given bone-group (expects 'virtual' index (+1 one, used by active_group etc.))
 * index might be invalid ( < 1), in which case it will be find from grp. */
void BKE_pose_remove_group(bPose *pose, bActionGroup *grp, const int index)
{
	bPoseChannel *pchan;
	int idx = index;

	if (idx < 1) {
		idx = BLI_findindex(&pose->agroups, grp) + 1;
	}

	BLI_assert(idx > 0);

	/* adjust group references (the trouble of using indices!):
	 *  - firstly, make sure nothing references it
	 *  - also, make sure that those after this item get corrected
	 */
	for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
		if (pchan->agrp_index == idx)
			pchan->agrp_index = 0;
		else if (pchan->agrp_index > idx)
			pchan->agrp_index--;
	}

	/* now, remove it from the pose */
	BLI_freelinkN(&pose->agroups, grp);
	if (pose->active_group >= idx) {
		const bool has_groups = !BLI_listbase_is_empty(&pose->agroups);
		pose->active_group--;
		if (pose->active_group == 0 && has_groups) {
			pose->active_group = 1;
		}
		else if (pose->active_group < 0 || !has_groups) {
			pose->active_group = 0;
		}
	}
}

/* Remove the indexed bone-group (expects 'virtual' index (+1 one, used by active_group etc.)) */
void BKE_pose_remove_group_index(bPose *pose, const int index)
{
	bActionGroup *grp = NULL;

	/* get group to remove */
	grp = BLI_findlink(&pose->agroups, index - 1);
	if (grp) {
		BKE_pose_remove_group(pose, grp, index);
	}
}

/* ************** F-Curve Utilities for Actions ****************** */

/* Check if the given action has any keyframes */
bool action_has_motion(const bAction *act)
{
	FCurve *fcu;

	/* return on the first F-Curve that has some keyframes/samples defined */
	if (act) {
		for (fcu = act->curves.first; fcu; fcu = fcu->next) {
			if (fcu->totvert)
				return true;
		}
	}

	/* nothing found */
	return false;
}

/* Calculate the extents of given action */
void calc_action_range(const bAction *act, float *start, float *end, short incl_modifiers)
{
	FCurve *fcu;
	float min = 999999999.0f, max = -999999999.0f;
	short foundvert = 0, foundmod = 0;

	if (act) {
		for (fcu = act->curves.first; fcu; fcu = fcu->next) {
			/* if curve has keyframes, consider them first */
			if (fcu->totvert) {
				float nmin, nmax;

				/* get extents for this curve
				 * - no "selected only", since this is often used in the backend
				 * - no "minimum length" (we will apply this later), otherwise
				 *   single-keyframe curves will increase the overall length by
				 *   a phantom frame (T50354)
				 */
				calc_fcurve_range(fcu, &nmin, &nmax, false, false);

				/* compare to the running tally */
				min = min_ff(min, nmin);
				max = max_ff(max, nmax);

				foundvert = 1;
			}

			/* if incl_modifiers is enabled, need to consider modifiers too
			 *	- only really care about the last modifier
			 */
			if ((incl_modifiers) && (fcu->modifiers.last)) {
				FModifier *fcm = fcu->modifiers.last;

				/* only use the maximum sensible limits of the modifiers if they are more extreme */
				switch (fcm->type) {
					case FMODIFIER_TYPE_LIMITS: /* Limits F-Modifier */
					{
						FMod_Limits *fmd = (FMod_Limits *)fcm->data;

						if (fmd->flag & FCM_LIMIT_XMIN) {
							min = min_ff(min, fmd->rect.xmin);
						}
						if (fmd->flag & FCM_LIMIT_XMAX) {
							max = max_ff(max, fmd->rect.xmax);
						}
						break;
					}
					case FMODIFIER_TYPE_CYCLES: /* Cycles F-Modifier */
					{
						FMod_Cycles *fmd = (FMod_Cycles *)fcm->data;

						if (fmd->before_mode != FCM_EXTRAPOLATE_NONE)
							min = MINAFRAMEF;
						if (fmd->after_mode != FCM_EXTRAPOLATE_NONE)
							max = MAXFRAMEF;
						break;
					}
					/* TODO: function modifier may need some special limits */

					default: /* all other standard modifiers are on the infinite range... */
						min = MINAFRAMEF;
						max = MAXFRAMEF;
						break;
				}

				foundmod = 1;
			}
		}
	}

	if (foundvert || foundmod) {
		/* ensure that action is at least 1 frame long (for NLA strips to have a valid length) */
		if (min == max) max += 1.0f;

		*start = min;
		*end = max;
	}
	else {
		*start = 0.0f;
		*end = 1.0f;
	}
}

/* Return flags indicating which transforms the given object/posechannel has
 *	- if 'curves' is provided, a list of links to these curves are also returned
 */
short action_get_item_transforms(bAction *act, Object *ob, bPoseChannel *pchan, ListBase *curves)
{
	PointerRNA ptr;
	FCurve *fcu;
	char *basePath = NULL;
	short flags = 0;

	/* build PointerRNA from provided data to obtain the paths to use */
	if (pchan)
		RNA_pointer_create((ID *)ob, &RNA_PoseBone, pchan, &ptr);
	else if (ob)
		RNA_id_pointer_create((ID *)ob, &ptr);
	else
		return 0;

	/* get the basic path to the properties of interest */
	basePath = RNA_path_from_ID_to_struct(&ptr);
	if (basePath == NULL)
		return 0;

	/* search F-Curves for the given properties
	 *	- we cannot use the groups, since they may not be grouped in that way...
	 */
	for (fcu = act->curves.first; fcu; fcu = fcu->next) {
		const char *bPtr = NULL, *pPtr = NULL;

		/* if enough flags have been found, we can stop checking unless we're also getting the curves */
		if ((flags == ACT_TRANS_ALL) && (curves == NULL))
			break;

		/* just in case... */
		if (fcu->rna_path == NULL)
			continue;

		/* step 1: check for matching base path */
		bPtr = strstr(fcu->rna_path, basePath);

		if (bPtr) {
			/* we must add len(basePath) bytes to the match so that we are at the end of the
			 * base path so that we don't get false positives with these strings in the names
			 */
			bPtr += strlen(basePath);

			/* step 2: check for some property with transforms
			 *	- to speed things up, only check for the ones not yet found
			 *    unless we're getting the curves too
			 *	- if we're getting the curves, the BLI_genericNodeN() creates a LinkData
			 *	  node wrapping the F-Curve, which then gets added to the list
			 *	- once a match has been found, the curve cannot possibly be any other one
			 */
			if ((curves) || (flags & ACT_TRANS_LOC) == 0) {
				pPtr = strstr(bPtr, "location");
				if (pPtr) {
					flags |= ACT_TRANS_LOC;

					if (curves)
						BLI_addtail(curves, BLI_genericNodeN(fcu));
					continue;
				}
			}

			if ((curves) || (flags & ACT_TRANS_SCALE) == 0) {
				pPtr = strstr(bPtr, "scale");
				if (pPtr) {
					flags |= ACT_TRANS_SCALE;

					if (curves)
						BLI_addtail(curves, BLI_genericNodeN(fcu));
					continue;
				}
			}

			if ((curves) || (flags & ACT_TRANS_ROT) == 0) {
				pPtr = strstr(bPtr, "rotation");
				if (pPtr) {
					flags |= ACT_TRANS_ROT;

					if (curves)
						BLI_addtail(curves, BLI_genericNodeN(fcu));
					continue;
				}
			}

			if ((curves) || (flags & ACT_TRANS_BBONE) == 0) {
				/* bbone shape properties */
				pPtr = strstr(bPtr, "bbone_");
				if (pPtr) {
					flags |= ACT_TRANS_BBONE;

					if (curves)
						BLI_addtail(curves, BLI_genericNodeN(fcu));
					continue;
				}
			}

			if ((curves) || (flags & ACT_TRANS_PROP) == 0) {
				/* custom properties only */
				pPtr = strstr(bPtr, "[\""); /* extra '"' comment here to keep my texteditor functionlist working :) */
				if (pPtr) {
					flags |= ACT_TRANS_PROP;

					if (curves)
						BLI_addtail(curves, BLI_genericNodeN(fcu));
					continue;
				}
			}
		}
	}

	/* free basePath */
	MEM_freeN(basePath);

	/* return flags found */
	return flags;
}

/* ************** Pose Management Tools ****************** */

/* for do_all_pose_actions, clears the pose. Now also exported for proxy and tools */
void BKE_pose_rest(bPose *pose)
{
	bPoseChannel *pchan;

	if (!pose)
		return;

	memset(pose->stride_offset, 0, sizeof(pose->stride_offset));
	memset(pose->cyclic_offset, 0, sizeof(pose->cyclic_offset));

	for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
		zero_v3(pchan->loc);
		zero_v3(pchan->eul);
		unit_qt(pchan->quat);
		unit_axis_angle(pchan->rotAxis, &pchan->rotAngle);
		pchan->size[0] = pchan->size[1] = pchan->size[2] = 1.0f;

		pchan->roll1 = pchan->roll2 = 0.0f;
		pchan->curveInX = pchan->curveInY = 0.0f;
		pchan->curveOutX = pchan->curveOutY = 0.0f;
		pchan->ease1 = pchan->ease2 = 0.0f;
		pchan->scaleIn = pchan->scaleOut = 1.0f;

		pchan->flag &= ~(POSE_LOC | POSE_ROT | POSE_SIZE | POSE_BBONE_SHAPE);
	}
}

void BKE_pose_copyesult_pchan_result(bPoseChannel *pchanto, const bPoseChannel *pchanfrom)
{
	copy_m4_m4(pchanto->pose_mat, pchanfrom->pose_mat);
	copy_m4_m4(pchanto->chan_mat, pchanfrom->chan_mat);

	/* used for local constraints */
	copy_v3_v3(pchanto->loc, pchanfrom->loc);
	copy_qt_qt(pchanto->quat, pchanfrom->quat);
	copy_v3_v3(pchanto->eul, pchanfrom->eul);
	copy_v3_v3(pchanto->size, pchanfrom->size);

	copy_v3_v3(pchanto->pose_head, pchanfrom->pose_head);
	copy_v3_v3(pchanto->pose_tail, pchanfrom->pose_tail);

	pchanto->roll1 = pchanfrom->roll1;
	pchanto->roll2 = pchanfrom->roll2;
	pchanto->curveInX = pchanfrom->curveInX;
	pchanto->curveInY = pchanfrom->curveInY;
	pchanto->curveOutX = pchanfrom->curveOutX;
	pchanto->curveOutY = pchanfrom->curveOutY;
	pchanto->ease1 = pchanfrom->ease1;
	pchanto->ease2 = pchanfrom->ease2;
	pchanto->scaleIn = pchanfrom->scaleIn;
	pchanto->scaleOut = pchanfrom->scaleOut;

	pchanto->rotmode = pchanfrom->rotmode;
	pchanto->flag = pchanfrom->flag;
	pchanto->protectflag = pchanfrom->protectflag;
	pchanto->bboneflag = pchanfrom->bboneflag;
}

/* both poses should be in sync */
bool BKE_pose_copy_result(bPose *to, bPose *from)
{
	bPoseChannel *pchanto, *pchanfrom;

	if (to == NULL || from == NULL) {
		printf("Pose copy error, pose to:%p from:%p\n", (void *)to, (void *)from); /* debug temp */
		return false;
	}

	if (to == from) {
		printf("BKE_pose_copy_result source and target are the same\n");
		return false;
	}


	for (pchanfrom = from->chanbase.first; pchanfrom; pchanfrom = pchanfrom->next) {
		pchanto = BKE_pose_channel_find_name(to, pchanfrom->name);
		if (pchanto != NULL) {
			BKE_pose_copyesult_pchan_result(pchanto, pchanfrom);
		}
	}
	return true;
}

/* Tag pose for recalc. Also tag all related data to be recalc. */
void BKE_pose_tag_recalc(Main *bmain, bPose *pose)
{
	pose->flag |= POSE_RECALC;
	/* Depsgraph components depends on actual pose state,
	 * if pose was changed depsgraph is to be updated as well.
	 */
	DEG_relations_tag_update(bmain);
}

/* For the calculation of the effects of an Action at the given frame on an object
 * This is currently only used for the Action Constraint
 */
void what_does_obaction(Object *ob, Object *workob, bPose *pose, bAction *act, char groupname[], float cframe)
{
	bActionGroup *agrp = BKE_action_group_find_name(act, groupname);

	/* clear workob */
	BKE_object_workob_clear(workob);

	/* init workob */
	copy_m4_m4(workob->obmat, ob->obmat);
	copy_m4_m4(workob->parentinv, ob->parentinv);
	copy_m4_m4(workob->constinv, ob->constinv);
	workob->parent = ob->parent;

	workob->rotmode = ob->rotmode;

	workob->trackflag = ob->trackflag;
	workob->upflag = ob->upflag;

	workob->partype = ob->partype;
	workob->par1 = ob->par1;
	workob->par2 = ob->par2;
	workob->par3 = ob->par3;

	workob->constraints.first = ob->constraints.first;
	workob->constraints.last = ob->constraints.last;

	workob->pose = pose; /* need to set pose too, since this is used for both types of Action Constraint */
	if (pose) {
		/* This function is most likely to be used with a temporary pose with a single bone in there.
		 * For such cases it makes no sense to create hash since it'll only waste CPU ticks on memory
		 * allocation and also will make lookup slower.
		 */
		if (pose->chanbase.first != pose->chanbase.last) {
			BKE_pose_channels_hash_make(pose);
		}
		if (pose->flag & POSE_CONSTRAINTS_NEED_UPDATE_FLAGS) {
			BKE_pose_update_constraint_flags(pose);
		}
	}

	BLI_strncpy(workob->parsubstr, ob->parsubstr, sizeof(workob->parsubstr));
	BLI_strncpy(workob->id.name, "OB<ConstrWorkOb>", sizeof(workob->id.name)); /* we don't use real object name, otherwise RNA screws with the real thing */

	/* if we're given a group to use, it's likely to be more efficient (though a bit more dangerous) */
	if (agrp) {
		/* specifically evaluate this group only */
		PointerRNA id_ptr;

		/* get RNA-pointer for the workob's ID */
		RNA_id_pointer_create(&workob->id, &id_ptr);

		/* execute action for this group only */
		animsys_evaluate_action_group(&id_ptr, act, agrp, NULL, cframe);
	}
	else {
		AnimData adt = {NULL};

		/* init animdata, and attach to workob */
		workob->adt = &adt;

		adt.recalc = ADT_RECALC_ANIM;
		adt.action = act;

		/* execute effects of Action on to workob (or it's PoseChannels) */
		BKE_animsys_evaluate_animdata(NULL, NULL, &workob->id, &adt, cframe, ADT_RECALC_ANIM);
	}
}
