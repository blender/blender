/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_session_uuid.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_action.h"
#include "BKE_anim_data.h"
#include "BKE_anim_visualization.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_asset.h"
#include "BKE_constraint.h"
#include "BKE_deform.h"
#include "BKE_fcurve.h"
#include "BKE_icons.h"
#include "BKE_idprop.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_object.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "BIK_api.h"

#include "RNA_access.h"
#include "RNA_path.h"
#include "RNA_prototypes.h"

#include "BLO_read_write.h"

#include "CLG_log.h"

static CLG_LogRef LOG = {"bke.action"};

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

/**************************** Action Datablock ******************************/

/*********************** Armature Datablock ***********************/

/**
 * Only copy internal data of Action ID from source
 * to already allocated/initialized destination.
 * You probably never want to use that directly,
 * use #BKE_id_copy or #BKE_id_copy_ex for typical needs.
 *
 * WARNING! This function will not handle ID user count!
 *
 * \param flag: Copying options (see BKE_lib_id.h's LIB_ID_COPY_... flags for more).
 */
static void action_copy_data(Main *UNUSED(bmain), ID *id_dst, const ID *id_src, const int flag)
{
  bAction *action_dst = (bAction *)id_dst;
  const bAction *action_src = (const bAction *)id_src;

  bActionGroup *group_dst, *group_src;
  FCurve *fcurve_dst, *fcurve_src;

  /* Duplicate the lists of groups and markers. */
  BLI_duplicatelist(&action_dst->groups, &action_src->groups);
  BLI_duplicatelist(&action_dst->markers, &action_src->markers);

  /* Copy F-Curves, fixing up the links as we go. */
  BLI_listbase_clear(&action_dst->curves);

  for (fcurve_src = action_src->curves.first; fcurve_src; fcurve_src = fcurve_src->next) {
    /* Duplicate F-Curve. */

    /* XXX TODO: pass subdata flag?
     * But surprisingly does not seem to be doing any ID reference-counting. */
    fcurve_dst = BKE_fcurve_copy(fcurve_src);

    BLI_addtail(&action_dst->curves, fcurve_dst);

    /* Fix group links (kind of bad list-in-list search, but this is the most reliable way). */
    for (group_dst = action_dst->groups.first, group_src = action_src->groups.first;
         group_dst && group_src;
         group_dst = group_dst->next, group_src = group_src->next)
    {
      if (fcurve_src->grp == group_src) {
        fcurve_dst->grp = group_dst;

        if (group_dst->channels.first == fcurve_src) {
          group_dst->channels.first = fcurve_dst;
        }
        if (group_dst->channels.last == fcurve_src) {
          group_dst->channels.last = fcurve_dst;
        }
        break;
      }
    }
  }

  if (flag & LIB_ID_COPY_NO_PREVIEW) {
    action_dst->preview = NULL;
  }
  else {
    BKE_previewimg_id_copy(&action_dst->id, &action_src->id);
  }
}

/** Free (or release) any data used by this action (does not free the action itself). */
static void action_free_data(ID *id)
{
  bAction *action = (bAction *)id;
  /* No animdata here. */

  /* Free F-Curves. */
  BKE_fcurves_free(&action->curves);

  /* Free groups. */
  BLI_freelistN(&action->groups);

  /* Free pose-references (aka local markers). */
  BLI_freelistN(&action->markers);

  BKE_previewimg_free(&action->preview);
}

static void action_foreach_id(ID *id, LibraryForeachIDData *data)
{
  bAction *act = (bAction *)id;

  LISTBASE_FOREACH (FCurve *, fcu, &act->curves) {
    BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(data, BKE_fcurve_foreach_id(fcu, data));
  }

  LISTBASE_FOREACH (TimeMarker *, marker, &act->markers) {
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, marker->camera, IDWALK_CB_NOP);
  }
}

static void action_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  bAction *act = (bAction *)id;

  BLO_write_id_struct(writer, bAction, id_address, &act->id);
  BKE_id_blend_write(writer, &act->id);

  BKE_fcurve_blend_write(writer, &act->curves);

  LISTBASE_FOREACH (bActionGroup *, grp, &act->groups) {
    BLO_write_struct(writer, bActionGroup, grp);
  }

  LISTBASE_FOREACH (TimeMarker *, marker, &act->markers) {
    BLO_write_struct(writer, TimeMarker, marker);
  }

  BKE_previewimg_blend_write(writer, act->preview);
}

static void action_blend_read_data(BlendDataReader *reader, ID *id)
{
  bAction *act = (bAction *)id;

  BLO_read_list(reader, &act->curves);
  BLO_read_list(reader, &act->chanbase); /* XXX deprecated - old animation system */
  BLO_read_list(reader, &act->groups);
  BLO_read_list(reader, &act->markers);

  /* XXX deprecated - old animation system <<< */
  LISTBASE_FOREACH (bActionChannel *, achan, &act->chanbase) {
    BLO_read_data_address(reader, &achan->grp);

    BLO_read_list(reader, &achan->constraintChannels);
  }
  /* >>> XXX deprecated - old animation system */

  BKE_fcurve_blend_read_data(reader, &act->curves);

  LISTBASE_FOREACH (bActionGroup *, agrp, &act->groups) {
    BLO_read_data_address(reader, &agrp->channels.first);
    BLO_read_data_address(reader, &agrp->channels.last);
  }

  BLO_read_data_address(reader, &act->preview);
  BKE_previewimg_blend_read(reader, act->preview);
}

static void blend_read_lib_constraint_channels(BlendLibReader *reader, ID *id, ListBase *chanbase)
{
  LISTBASE_FOREACH (bConstraintChannel *, chan, chanbase) {
    BLO_read_id_address(reader, id, &chan->ipo);
  }
}

static void action_blend_read_lib(BlendLibReader *reader, ID *id)
{
  bAction *act = (bAction *)id;

  /* XXX deprecated - old animation system <<< */
  LISTBASE_FOREACH (bActionChannel *, chan, &act->chanbase) {
    BLO_read_id_address(reader, id, &chan->ipo);
    blend_read_lib_constraint_channels(reader, &act->id, &chan->constraintChannels);
  }
  /* >>> XXX deprecated - old animation system */

  BKE_fcurve_blend_read_lib(reader, id, &act->curves);

  LISTBASE_FOREACH (TimeMarker *, marker, &act->markers) {
    if (marker->camera) {
      BLO_read_id_address(reader, id, &marker->camera);
    }
  }
}

static void blend_read_expand_constraint_channels(BlendExpander *expander, ListBase *chanbase)
{
  LISTBASE_FOREACH (bConstraintChannel *, chan, chanbase) {
    BLO_expand(expander, chan->ipo);
  }
}

static void action_blend_read_expand(BlendExpander *expander, ID *id)
{
  bAction *act = (bAction *)id;

  /* XXX deprecated - old animation system -------------- */
  LISTBASE_FOREACH (bActionChannel *, chan, &act->chanbase) {
    BLO_expand(expander, chan->ipo);
    blend_read_expand_constraint_channels(expander, &chan->constraintChannels);
  }
  /* --------------------------------------------------- */

  /* F-Curves in Action */
  BKE_fcurve_blend_read_expand(expander, &act->curves);

  LISTBASE_FOREACH (TimeMarker *, marker, &act->markers) {
    if (marker->camera) {
      BLO_expand(expander, marker->camera);
    }
  }
}

static IDProperty *action_asset_type_property(const bAction *action)
{
  const bool is_single_frame = BKE_action_has_single_frame(action);

  IDPropertyTemplate idprop = {0};
  idprop.i = is_single_frame;

  IDProperty *property = IDP_New(IDP_INT, &idprop, "is_single_frame");
  return property;
}

static void action_asset_pre_save(void *asset_ptr, AssetMetaData *asset_data)
{
  bAction *action = (bAction *)asset_ptr;
  BLI_assert(GS(action->id.name) == ID_AC);

  IDProperty *action_type = action_asset_type_property(action);
  BKE_asset_metadata_idprop_ensure(asset_data, action_type);
}

static AssetTypeInfo AssetType_AC = {
    /*pre_save_fn*/ action_asset_pre_save,
};

IDTypeInfo IDType_ID_AC = {
    .id_code = ID_AC,
    .id_filter = FILTER_ID_AC,
    .main_listbase_index = INDEX_ID_AC,
    .struct_size = sizeof(bAction),
    .name = "Action",
    .name_plural = "actions",
    .translation_context = BLT_I18NCONTEXT_ID_ACTION,
    .flags = IDTYPE_FLAGS_NO_ANIMDATA,
    .asset_type_info = &AssetType_AC,

    .init_data = NULL,
    .copy_data = action_copy_data,
    .free_data = action_free_data,
    .make_local = NULL,
    .foreach_id = action_foreach_id,
    .foreach_cache = NULL,
    .foreach_path = NULL,
    .owner_pointer_get = NULL,

    .blend_write = action_blend_write,
    .blend_read_data = action_blend_read_data,
    .blend_read_lib = action_blend_read_lib,
    .blend_read_expand = action_blend_read_expand,

    .blend_read_undo_preserve = NULL,

    .lib_override_apply_post = NULL,
};

/* ***************** Library data level operations on action ************** */

bAction *BKE_action_add(Main *bmain, const char name[])
{
  bAction *act;

  act = BKE_id_new(bmain, ID_AC, name);

  return act;
}

/* .................................. */

/* *************** Action Groups *************** */

bActionGroup *get_active_actiongroup(bAction *act)
{
  bActionGroup *agrp = NULL;

  if (act && act->groups.first) {
    for (agrp = act->groups.first; agrp; agrp = agrp->next) {
      if (agrp->flag & AGRP_ACTIVE) {
        break;
      }
    }
  }

  return agrp;
}

void set_active_action_group(bAction *act, bActionGroup *agrp, short select)
{
  bActionGroup *grp;

  /* sanity checks */
  if (act == NULL) {
    return;
  }

  /* Deactivate all others */
  for (grp = act->groups.first; grp; grp = grp->next) {
    if ((grp == agrp) && (select)) {
      grp->flag |= AGRP_ACTIVE;
    }
    else {
      grp->flag &= ~AGRP_ACTIVE;
    }
  }
}

void action_group_colors_sync(bActionGroup *grp, const bActionGroup *ref_grp)
{
  /* Only do color copying if using a custom color (i.e. not default color). */
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
        rgba_uchar_args_set(grp->cs.solid, 0xff, 0x00, 0x00, 255);
        rgba_uchar_args_set(grp->cs.select, 0x81, 0xe6, 0x14, 255);
        rgba_uchar_args_set(grp->cs.active, 0x18, 0xb6, 0xe0, 255);
      }
    }
  }
}

bActionGroup *action_groups_add_new(bAction *act, const char name[])
{
  bActionGroup *agrp;

  /* sanity check: must have action and name */
  if (ELEM(NULL, act, name)) {
    return NULL;
  }

  /* allocate a new one */
  agrp = MEM_callocN(sizeof(bActionGroup), "bActionGroup");

  /* make it selected, with default name */
  agrp->flag = AGRP_SELECTED;
  STRNCPY_UTF8(agrp->name, name[0] ? name : DATA_("Group"));

  /* add to action, and validate */
  BLI_addtail(&act->groups, agrp);
  BLI_uniquename(
      &act->groups, agrp, DATA_("Group"), '.', offsetof(bActionGroup, name), sizeof(agrp->name));

  /* return the new group */
  return agrp;
}

void action_groups_add_channel(bAction *act, bActionGroup *agrp, FCurve *fcurve)
{
  /* sanity checks */
  if (ELEM(NULL, act, agrp, fcurve)) {
    return;
  }

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
    if (agrp->channels.last == act->curves.last) {
      act->curves.last = fcurve;
    }

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

    /* Step through the groups preceding this one,
     * finding the F-Curve there to attach this one after. */
    for (grp = agrp->prev; grp; grp = grp->prev) {
      /* if this group has F-Curves, we want weave the given one in right after the last channel
       * there, but via the Action's list not this group's list
       * - this is so that the F-Curve is in the right place in the Action,
       *   but won't be included in the previous group.
       */
      if (grp->channels.last) {
        /* once we've added, break here since we don't need to search any further... */
        BLI_insertlinkafter(&act->curves, grp->channels.last, fcurve);
        break;
      }
    }

    /* If grp is NULL, that means we fell through, and this F-Curve should be added as the new
     * first since group is (effectively) the first group. Thus, the existing first F-Curve becomes
     * the second in the chain, etc. */
    if (grp == NULL) {
      BLI_insertlinkbefore(&act->curves, act->curves.first, fcurve);
    }
  }

  /* set the F-Curve's new group */
  fcurve->grp = agrp;
}

void BKE_action_groups_reconstruct(bAction *act)
{
  /* Sanity check. */
  if (ELEM(NULL, act, act->groups.first)) {
    return;
  }

  /* Clear out all group channels. Channels that are actually in use are
   * reconstructed below; this step is necessary to clear out unused groups. */
  LISTBASE_FOREACH (bActionGroup *, group, &act->groups) {
    BLI_listbase_clear(&group->channels);
  }

  /* Sort the channels into the group lists, destroying the act->curves list. */
  ListBase ungrouped = {NULL, NULL};

  LISTBASE_FOREACH_MUTABLE (FCurve *, fcurve, &act->curves) {
    if (fcurve->grp) {
      BLI_assert(BLI_findindex(&act->groups, fcurve->grp) >= 0);

      BLI_addtail(&fcurve->grp->channels, fcurve);
    }
    else {
      BLI_addtail(&ungrouped, fcurve);
    }
  }

  /* Recombine into the main list. */
  BLI_listbase_clear(&act->curves);

  LISTBASE_FOREACH (bActionGroup *, group, &act->groups) {
    /* Copy the list header to preserve the pointers in the group. */
    ListBase tmp = group->channels;
    BLI_movelisttolist(&act->curves, &tmp);
  }

  BLI_movelisttolist(&act->curves, &ungrouped);
}

void action_groups_remove_channel(bAction *act, FCurve *fcu)
{
  /* sanity checks */
  if (ELEM(NULL, act, fcu)) {
    return;
  }

  /* check if any group used this directly */
  if (fcu->grp) {
    bActionGroup *agrp = fcu->grp;

    if (agrp->channels.first == agrp->channels.last) {
      if (agrp->channels.first == fcu) {
        BLI_listbase_clear(&agrp->channels);
      }
    }
    else if (agrp->channels.first == fcu) {
      if ((fcu->next) && (fcu->next->grp == agrp)) {
        agrp->channels.first = fcu->next;
      }
      else {
        agrp->channels.first = NULL;
      }
    }
    else if (agrp->channels.last == fcu) {
      if ((fcu->prev) && (fcu->prev->grp == agrp)) {
        agrp->channels.last = fcu->prev;
      }
      else {
        agrp->channels.last = NULL;
      }
    }

    fcu->grp = NULL;
  }

  /* now just remove from list */
  BLI_remlink(&act->curves, fcu);
}

bActionGroup *BKE_action_group_find_name(bAction *act, const char name[])
{
  /* sanity checks */
  if (ELEM(NULL, act, act->groups.first, name) || (name[0] == 0)) {
    return NULL;
  }

  /* do string comparisons */
  return BLI_findstring(&act->groups, name, offsetof(bActionGroup, name));
}

void action_groups_clear_tempflags(bAction *act)
{
  bActionGroup *agrp;

  /* sanity checks */
  if (ELEM(NULL, act, act->groups.first)) {
    return;
  }

  /* flag clearing loop */
  for (agrp = act->groups.first; agrp; agrp = agrp->next) {
    agrp->flag &= ~AGRP_TEMP;
  }
}

/* *************** Pose channels *************** */

void BKE_pose_channel_session_uuid_generate(bPoseChannel *pchan)
{
  pchan->runtime.session_uuid = BLI_session_uuid_generate();
}

bPoseChannel *BKE_pose_channel_find_name(const bPose *pose, const char *name)
{
  if (ELEM(NULL, pose, name) || (name[0] == '\0')) {
    return NULL;
  }

  if (pose->chanhash) {
    return BLI_ghash_lookup(pose->chanhash, (const void *)name);
  }

  return BLI_findstring(&pose->chanbase, name, offsetof(bPoseChannel, name));
}

bPoseChannel *BKE_pose_channel_ensure(bPose *pose, const char *name)
{
  bPoseChannel *chan;

  if (pose == NULL) {
    return NULL;
  }

  /* See if this channel exists */
  chan = BKE_pose_channel_find_name(pose, name);
  if (chan) {
    return chan;
  }

  /* If not, create it and add it */
  chan = MEM_callocN(sizeof(bPoseChannel), "verifyPoseChannel");

  BKE_pose_channel_session_uuid_generate(chan);

  STRNCPY(chan->name, name);

  copy_v3_fl(chan->custom_scale_xyz, 1.0f);
  zero_v3(chan->custom_translation);
  zero_v3(chan->custom_rotation_euler);

  /* init vars to prevent math errors */
  unit_qt(chan->quat);
  unit_axis_angle(chan->rotAxis, &chan->rotAngle);
  chan->size[0] = chan->size[1] = chan->size[2] = 1.0f;

  copy_v3_fl(chan->scale_in, 1.0f);
  copy_v3_fl(chan->scale_out, 1.0f);

  chan->limitmin[0] = chan->limitmin[1] = chan->limitmin[2] = -M_PI;
  chan->limitmax[0] = chan->limitmax[1] = chan->limitmax[2] = M_PI;
  chan->stiffness[0] = chan->stiffness[1] = chan->stiffness[2] = 0.0f;
  chan->ikrotweight = chan->iklinweight = 0.0f;
  unit_m4(chan->constinv);

  chan->protectflag = OB_LOCK_ROT4D; /* lock by components by default */

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

bool BKE_pose_is_layer_visible(const bArmature *arm, const bPoseChannel *pchan)
{
  return (pchan->bone->layer & arm->layer);
}

bPoseChannel *BKE_pose_channel_active(Object *ob, const bool check_arm_layer)
{
  bArmature *arm = (ob) ? ob->data : NULL;
  bPoseChannel *pchan;

  if (ELEM(NULL, ob, ob->pose, arm)) {
    return NULL;
  }

  /* find active */
  for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
    if ((pchan->bone) && (pchan->bone == arm->act_bone)) {
      if (!check_arm_layer || BKE_pose_is_layer_visible(arm, pchan)) {
        return pchan;
      }
    }
  }

  return NULL;
}

bPoseChannel *BKE_pose_channel_active_if_layer_visible(Object *ob)
{
  return BKE_pose_channel_active(ob, true);
}

bPoseChannel *BKE_pose_channel_active_or_first_selected(Object *ob)
{
  bArmature *arm = (ob) ? ob->data : NULL;

  if (ELEM(NULL, ob, ob->pose, arm)) {
    return NULL;
  }

  bPoseChannel *pchan = BKE_pose_channel_active_if_layer_visible(ob);
  if (pchan && (pchan->bone->flag & BONE_SELECTED) && PBONE_VISIBLE(arm, pchan->bone)) {
    return pchan;
  }

  for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
    if (pchan->bone != NULL) {
      if ((pchan->bone->flag & BONE_SELECTED) && PBONE_VISIBLE(arm, pchan->bone)) {
        return pchan;
      }
    }
  }
  return NULL;
}

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

void BKE_pose_copy_data_ex(bPose **dst,
                           const bPose *src,
                           const int flag,
                           const bool copy_constraints)
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
    BKE_pose_channels_hash_ensure(outPose);
  }

  outPose->iksolver = src->iksolver;
  outPose->ikdata = NULL;
  outPose->ikparam = MEM_dupallocN(src->ikparam);
  outPose->avs = src->avs;

  for (pchan = outPose->chanbase.first; pchan; pchan = pchan->next) {
    if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
      id_us_plus((ID *)pchan->custom);
    }

    if ((flag & LIB_ID_CREATE_NO_MAIN) == 0) {
      BKE_pose_channel_session_uuid_generate(pchan);
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
      /* #BKE_constraints_copy NULL's `listb` */
      BKE_constraints_copy_ex(&listb, &pchan->constraints, flag, true);

      pchan->constraints = listb;

      /* XXX: This is needed for motionpath drawing to work.
       * Dunno why it was setting to null before... */
      pchan->mpath = animviz_copy_motionpath(pchan->mpath);
    }

    if (pchan->prop) {
      pchan->prop = IDP_CopyProperty_ex(pchan->prop, flag);
    }

    pchan->draw_data = NULL; /* Drawing cache, no need to copy. */

    /* Runtime data, no need to copy. */
    BKE_pose_channel_runtime_reset_on_copy(&pchan->runtime);
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
        if ((data->flag & CONSTRAINT_IK_AUTO) == 0) {
          return true;
        }
      }
    }
  }
  for (bone = pchan->bone->childbase.first; bone; bone = bone->next) {
    pchan = BKE_pose_channel_find_name(ob->pose, bone->name);
    if (pchan && pose_channel_in_IK_chain(ob, pchan, level + 1)) {
      return true;
    }
  }
  return false;
}

bool BKE_pose_channel_in_IK_chain(Object *ob, bPoseChannel *pchan)
{
  return pose_channel_in_IK_chain(ob, pchan, 0);
}

void BKE_pose_channels_hash_ensure(bPose *pose)
{
  if (!pose->chanhash) {
    bPoseChannel *pchan;

    pose->chanhash = BLI_ghash_str_new("make_pose_chan gh");
    for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
      BLI_ghash_insert(pose->chanhash, pchan->name, pchan);
    }
  }
}

void BKE_pose_channels_hash_free(bPose *pose)
{
  if (pose->chanhash) {
    BLI_ghash_free(pose->chanhash, NULL, NULL);
    pose->chanhash = NULL;
  }
}

static void pose_channels_remove_internal_links(Object *ob, bPoseChannel *unlinked_pchan)
{
  LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
    if (pchan->bbone_prev == unlinked_pchan) {
      pchan->bbone_prev = NULL;
    }
    if (pchan->bbone_next == unlinked_pchan) {
      pchan->bbone_next = NULL;
    }
    if (pchan->custom_tx == unlinked_pchan) {
      pchan->custom_tx = NULL;
    }
  }
}

void BKE_pose_channels_remove(Object *ob,
                              bool (*filter_fn)(const char *bone_name, void *user_data),
                              void *user_data)
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
        pose_channels_remove_internal_links(ob, pchan);
        if (ob->pose->chanhash) {
          BLI_ghash_remove(ob->pose->chanhash, pchan->name, NULL, NULL);
        }
        BLI_freelinkN(&ob->pose->chanbase, pchan);
      }
      else {
        /* Maybe something the bone references is being removed instead? */
        for (con = pchan->constraints.first; con; con = con->next) {
          ListBase targets = {NULL, NULL};
          bConstraintTarget *ct;

          if (BKE_constraint_targets_get(con, &targets)) {
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

            BKE_constraint_targets_flush(con, &targets, 0);
          }
        }

        if (pchan->bbone_prev) {
          if (filter_fn(pchan->bbone_prev->name, user_data)) {
            pchan->bbone_prev = NULL;
          }
        }
        if (pchan->bbone_next) {
          if (filter_fn(pchan->bbone_next->name, user_data)) {
            pchan->bbone_next = NULL;
          }
        }

        if (pchan->custom_tx) {
          if (filter_fn(pchan->custom_tx->name, user_data)) {
            pchan->custom_tx = NULL;
          }
        }
      }
    }
  }
}

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
    IDP_FreeProperty_ex(pchan->prop, do_id_user);
    pchan->prop = NULL;
  }

  /* Cached data, for new draw manager rendering code. */
  MEM_SAFE_FREE(pchan->draw_data);

  /* Cached B-Bone shape and other data. */
  BKE_pose_channel_runtime_free(&pchan->runtime);
}

void BKE_pose_channel_runtime_reset(bPoseChannel_Runtime *runtime)
{
  memset(runtime, 0, sizeof(*runtime));
}

void BKE_pose_channel_runtime_reset_on_copy(bPoseChannel_Runtime *runtime)
{
  const SessionUUID uuid = runtime->session_uuid;
  memset(runtime, 0, sizeof(*runtime));
  runtime->session_uuid = uuid;
}

void BKE_pose_channel_runtime_free(bPoseChannel_Runtime *runtime)
{
  BKE_pose_channel_free_bbone_cache(runtime);
}

void BKE_pose_channel_free_bbone_cache(bPoseChannel_Runtime *runtime)
{
  runtime->bbone_segments = 0;
  MEM_SAFE_FREE(runtime->bbone_rest_mats);
  MEM_SAFE_FREE(runtime->bbone_pose_mats);
  MEM_SAFE_FREE(runtime->bbone_deform_mats);
  MEM_SAFE_FREE(runtime->bbone_dual_quats);
}

void BKE_pose_channel_free(bPoseChannel *pchan)
{
  BKE_pose_channel_free_ex(pchan, true);
}

void BKE_pose_channels_free_ex(bPose *pose, bool do_id_user)
{
  bPoseChannel *pchan;

  if (pose->chanbase.first) {
    for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
      BKE_pose_channel_free_ex(pchan, do_id_user);
    }

    BLI_freelistN(&pose->chanbase);
  }

  BKE_pose_channels_hash_free(pose);

  MEM_SAFE_FREE(pose->chan_array);
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
  if (pose->agroups.first) {
    BLI_freelistN(&pose->agroups);
  }

  /* free IK solver state */
  BIK_clear_data(pose);

  /* free IK solver param */
  if (pose->ikparam) {
    MEM_freeN(pose->ikparam);
  }
}

void BKE_pose_free_data(bPose *pose)
{
  BKE_pose_free_data_ex(pose, true);
}

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

void BKE_pose_channel_copy_data(bPoseChannel *pchan, const bPoseChannel *pchan_from)
{
  /* copy transform locks */
  pchan->protectflag = pchan_from->protectflag;

  /* copy rotation mode */
  pchan->rotmode = pchan_from->rotmode;

  /* copy bone group */
  pchan->agrp_index = pchan_from->agrp_index;

  /* IK (DOF) settings. */
  pchan->ikflag = pchan_from->ikflag;
  copy_v3_v3(pchan->limitmin, pchan_from->limitmin);
  copy_v3_v3(pchan->limitmax, pchan_from->limitmax);
  copy_v3_v3(pchan->stiffness, pchan_from->stiffness);
  pchan->ikstretch = pchan_from->ikstretch;
  pchan->ikrotweight = pchan_from->ikrotweight;
  pchan->iklinweight = pchan_from->iklinweight;

  /* bbone settings (typically not animated) */
  pchan->bbone_next = pchan_from->bbone_next;
  pchan->bbone_prev = pchan_from->bbone_prev;

  /* constraints */
  BKE_constraints_copy(&pchan->constraints, &pchan_from->constraints, true);

  /* id-properties */
  if (pchan->prop) {
    /* unlikely but possible it exists */
    IDP_FreeProperty(pchan->prop);
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
  copy_v3_v3(pchan->custom_scale_xyz, pchan_from->custom_scale_xyz);
  copy_v3_v3(pchan->custom_translation, pchan_from->custom_translation);
  copy_v3_v3(pchan->custom_rotation_euler, pchan_from->custom_rotation_euler);

  pchan->drawflag = pchan_from->drawflag;
}

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

        if (data->tar == NULL || (data->tar->type == OB_ARMATURE && data->subtarget[0] == 0)) {
          pchan->constflag |= PCHAN_HAS_TARGET;
        }

        /* negative rootbone = recalc rootbone index. used in do_versions */
        if (data->rootbone < 0) {
          data->rootbone = 0;

          if (data->flag & CONSTRAINT_IK_TIP) {
            parchan = pchan;
          }
          else {
            parchan = pchan->parent;
          }

          while (parchan) {
            data->rootbone++;
            if ((parchan->bone->flag & BONE_CONNECTED) == 0) {
              break;
            }
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
        if ((data->tar) && (data->tar->type == OB_CURVES_LEGACY)) {
          pose->flag |= POSE_CONSTRAINTS_TIMEDEPEND;
        }
      }
      else if (con->type == CONSTRAINT_TYPE_SPLINEIK) {
        pchan->constflag |= PCHAN_HAS_SPLINEIK;
      }
      else {
        pchan->constflag |= PCHAN_HAS_CONST;
      }
    }
  }
  pose->flag &= ~POSE_CONSTRAINTS_NEED_UPDATE_FLAGS;
}

void BKE_pose_tag_update_constraint_flags(bPose *pose)
{
  pose->flag |= POSE_CONSTRAINTS_NEED_UPDATE_FLAGS;
}

/* ************************** Bone Groups ************************** */

bActionGroup *BKE_pose_add_group(bPose *pose, const char *name)
{
  bActionGroup *grp;

  if (!name) {
    name = DATA_("Group");
  }

  grp = MEM_callocN(sizeof(bActionGroup), "PoseGroup");
  STRNCPY(grp->name, name);
  BLI_addtail(&pose->agroups, grp);
  BLI_uniquename(&pose->agroups, grp, name, '.', offsetof(bActionGroup, name), sizeof(grp->name));

  pose->active_group = BLI_listbase_count(&pose->agroups);

  return grp;
}

void BKE_pose_remove_group(bPose *pose, bActionGroup *grp, const int index)
{
  bPoseChannel *pchan;
  int idx = index;

  if (idx < 1) {
    idx = BLI_findindex(&pose->agroups, grp) + 1;
  }

  BLI_assert(idx > 0);

  /* adjust group references (the trouble of using indices!):
   * - firstly, make sure nothing references it
   * - also, make sure that those after this item get corrected
   */
  for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
    if (pchan->agrp_index == idx) {
      pchan->agrp_index = 0;
    }
    else if (pchan->agrp_index > idx) {
      pchan->agrp_index--;
    }
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

bool action_has_motion(const bAction *act)
{
  FCurve *fcu;

  /* return on the first F-Curve that has some keyframes/samples defined */
  if (act) {
    for (fcu = act->curves.first; fcu; fcu = fcu->next) {
      if (fcu->totvert) {
        return true;
      }
    }
  }

  /* nothing found */
  return false;
}

bool BKE_action_has_single_frame(const bAction *act)
{
  if (act == NULL || BLI_listbase_is_empty(&act->curves)) {
    return false;
  }

  bool found_key = false;
  float found_key_frame = 0.0f;

  LISTBASE_FOREACH (FCurve *, fcu, &act->curves) {
    switch (fcu->totvert) {
      case 0:
        /* No keys, so impossible to come to a conclusion on this curve alone. */
        continue;
      case 1:
        /* Single key, which is the complex case, so handle below. */
        break;
      default:
        /* Multiple keys, so there is animation. */
        return false;
    }

    const float this_key_frame = fcu->bezt != NULL ? fcu->bezt[0].vec[1][0] : fcu->fpt[0].vec[0];
    if (!found_key) {
      found_key = true;
      found_key_frame = this_key_frame;
      continue;
    }

    /* The graph editor rounds to 1/1000th of a frame, so it's not necessary to be really precise
     * with these comparisons. */
    if (!compare_ff(found_key_frame, this_key_frame, 0.001f)) {
      /* This key differs from the already-found key, so this Action represents animation. */
      return false;
    }
  }

  /* There is only a single frame if we found at least one key. */
  return found_key;
}

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
         *   a phantom frame (#50354)
         */
        BKE_fcurve_calc_range(fcu, &nmin, &nmax, false);

        /* compare to the running tally */
        min = min_ff(min, nmin);
        max = max_ff(max, nmax);

        foundvert = 1;
      }

      /* if incl_modifiers is enabled, need to consider modifiers too
       * - only really care about the last modifier
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

            if (fmd->before_mode != FCM_EXTRAPOLATE_NONE) {
              min = MINAFRAMEF;
            }
            if (fmd->after_mode != FCM_EXTRAPOLATE_NONE) {
              max = MAXFRAMEF;
            }
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
    if (min == max) {
      max += 1.0f;
    }

    *start = min;
    *end = max;
  }
  else {
    *start = 0.0f;
    *end = 1.0f;
  }
}

void BKE_action_get_frame_range(const bAction *act, float *r_start, float *r_end)
{
  if (act && (act->flag & ACT_FRAME_RANGE)) {
    *r_start = act->frame_start;
    *r_end = act->frame_end;
  }
  else {
    calc_action_range(act, r_start, r_end, false);
  }

  /* Ensure that action is at least 1 frame long (for NLA strips to have a valid length). */
  if (*r_start >= *r_end) {
    *r_end = *r_start + 1.0f;
  }
}

bool BKE_action_is_cyclic(const bAction *act)
{
  return act && (act->flag & ACT_FRAME_RANGE) && (act->flag & ACT_CYCLIC);
}

short action_get_item_transforms(bAction *act, Object *ob, bPoseChannel *pchan, ListBase *curves)
{
  PointerRNA ptr;
  FCurve *fcu;
  char *basePath = NULL;
  short flags = 0;

  /* build PointerRNA from provided data to obtain the paths to use */
  if (pchan) {
    RNA_pointer_create((ID *)ob, &RNA_PoseBone, pchan, &ptr);
  }
  else if (ob) {
    RNA_id_pointer_create((ID *)ob, &ptr);
  }
  else {
    return 0;
  }

  /* get the basic path to the properties of interest */
  basePath = RNA_path_from_ID_to_struct(&ptr);
  if (basePath == NULL) {
    return 0;
  }

  /* search F-Curves for the given properties
   * - we cannot use the groups, since they may not be grouped in that way...
   */
  for (fcu = act->curves.first; fcu; fcu = fcu->next) {
    const char *bPtr = NULL, *pPtr = NULL;

    /* If enough flags have been found,
     * we can stop checking unless we're also getting the curves. */
    if ((flags == ACT_TRANS_ALL) && (curves == NULL)) {
      break;
    }

    /* just in case... */
    if (fcu->rna_path == NULL) {
      continue;
    }

    /* step 1: check for matching base path */
    bPtr = strstr(fcu->rna_path, basePath);

    if (bPtr) {
      /* we must add len(basePath) bytes to the match so that we are at the end of the
       * base path so that we don't get false positives with these strings in the names
       */
      bPtr += strlen(basePath);

      /* step 2: check for some property with transforms
       * - to speed things up, only check for the ones not yet found
       *   unless we're getting the curves too
       * - if we're getting the curves, the BLI_genericNodeN() creates a LinkData
       *   node wrapping the F-Curve, which then gets added to the list
       * - once a match has been found, the curve cannot possibly be any other one
       */
      if ((curves) || (flags & ACT_TRANS_LOC) == 0) {
        pPtr = strstr(bPtr, "location");
        if (pPtr) {
          flags |= ACT_TRANS_LOC;

          if (curves) {
            BLI_addtail(curves, BLI_genericNodeN(fcu));
          }
          continue;
        }
      }

      if ((curves) || (flags & ACT_TRANS_SCALE) == 0) {
        pPtr = strstr(bPtr, "scale");
        if (pPtr) {
          flags |= ACT_TRANS_SCALE;

          if (curves) {
            BLI_addtail(curves, BLI_genericNodeN(fcu));
          }
          continue;
        }
      }

      if ((curves) || (flags & ACT_TRANS_ROT) == 0) {
        pPtr = strstr(bPtr, "rotation");
        if (pPtr) {
          flags |= ACT_TRANS_ROT;

          if (curves) {
            BLI_addtail(curves, BLI_genericNodeN(fcu));
          }
          continue;
        }
      }

      if ((curves) || (flags & ACT_TRANS_BBONE) == 0) {
        /* bbone shape properties */
        pPtr = strstr(bPtr, "bbone_");
        if (pPtr) {
          flags |= ACT_TRANS_BBONE;

          if (curves) {
            BLI_addtail(curves, BLI_genericNodeN(fcu));
          }
          continue;
        }
      }

      if ((curves) || (flags & ACT_TRANS_PROP) == 0) {
        /* custom properties only */
        pPtr = strstr(bPtr, "[\"");
        if (pPtr) {
          flags |= ACT_TRANS_PROP;

          if (curves) {
            BLI_addtail(curves, BLI_genericNodeN(fcu));
          }
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

void BKE_pose_rest(bPose *pose, bool selected_bones_only)
{
  bPoseChannel *pchan;

  if (!pose) {
    return;
  }

  memset(pose->stride_offset, 0, sizeof(pose->stride_offset));
  memset(pose->cyclic_offset, 0, sizeof(pose->cyclic_offset));

  for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
    if (selected_bones_only && pchan->bone != NULL && (pchan->bone->flag & BONE_SELECTED) == 0) {
      continue;
    }
    zero_v3(pchan->loc);
    zero_v3(pchan->eul);
    unit_qt(pchan->quat);
    unit_axis_angle(pchan->rotAxis, &pchan->rotAngle);
    pchan->size[0] = pchan->size[1] = pchan->size[2] = 1.0f;

    pchan->roll1 = pchan->roll2 = 0.0f;
    pchan->curve_in_x = pchan->curve_in_z = 0.0f;
    pchan->curve_out_x = pchan->curve_out_z = 0.0f;
    pchan->ease1 = pchan->ease2 = 0.0f;

    copy_v3_fl(pchan->scale_in, 1.0f);
    copy_v3_fl(pchan->scale_out, 1.0f);

    pchan->flag &= ~(POSE_LOC | POSE_ROT | POSE_SIZE | POSE_BBONE_SHAPE);
  }
}

void BKE_pose_copy_pchan_result(bPoseChannel *pchanto, const bPoseChannel *pchanfrom)
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
  pchanto->curve_in_x = pchanfrom->curve_in_x;
  pchanto->curve_in_z = pchanfrom->curve_in_z;
  pchanto->curve_out_x = pchanfrom->curve_out_x;
  pchanto->curve_out_z = pchanfrom->curve_out_z;
  pchanto->ease1 = pchanfrom->ease1;
  pchanto->ease2 = pchanfrom->ease2;

  copy_v3_v3(pchanto->scale_in, pchanfrom->scale_in);
  copy_v3_v3(pchanto->scale_out, pchanfrom->scale_out);

  pchanto->rotmode = pchanfrom->rotmode;
  pchanto->flag = pchanfrom->flag;
  pchanto->protectflag = pchanfrom->protectflag;
}

bool BKE_pose_copy_result(bPose *to, bPose *from)
{
  bPoseChannel *pchanto, *pchanfrom;

  if (to == NULL || from == NULL) {
    CLOG_ERROR(
        &LOG, "Pose copy error, pose to:%p from:%p", (void *)to, (void *)from); /* debug temp */
    return false;
  }

  if (to == from) {
    CLOG_ERROR(&LOG, "source and target are the same");
    return false;
  }

  for (pchanfrom = from->chanbase.first; pchanfrom; pchanfrom = pchanfrom->next) {
    pchanto = BKE_pose_channel_find_name(to, pchanfrom->name);
    if (pchanto != NULL) {
      BKE_pose_copy_pchan_result(pchanto, pchanfrom);
    }
  }
  return true;
}

void BKE_pose_tag_recalc(Main *bmain, bPose *pose)
{
  pose->flag |= POSE_RECALC;
  /* Depsgraph components depends on actual pose state,
   * if pose was changed depsgraph is to be updated as well.
   */
  DEG_relations_tag_update(bmain);
}

void what_does_obaction(Object *ob,
                        Object *workob,
                        bPose *pose,
                        bAction *act,
                        char groupname[],
                        const AnimationEvalContext *anim_eval_context)
{
  bActionGroup *agrp = BKE_action_group_find_name(act, groupname);

  /* clear workob */
  BKE_object_workob_clear(workob);

  /* init workob */
  copy_m4_m4(workob->object_to_world, ob->object_to_world);
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

  /* Need to set pose too, since this is used for both types of Action Constraint. */
  workob->pose = pose;
  if (pose) {
    /* This function is most likely to be used with a temporary pose with a single bone in there.
     * For such cases it makes no sense to create hash since it'll only waste CPU ticks on memory
     * allocation and also will make lookup slower.
     */
    if (pose->chanbase.first != pose->chanbase.last) {
      BKE_pose_channels_hash_ensure(pose);
    }
    if (pose->flag & POSE_CONSTRAINTS_NEED_UPDATE_FLAGS) {
      BKE_pose_update_constraint_flags(pose);
    }
  }

  STRNCPY(workob->parsubstr, ob->parsubstr);

  /* we don't use real object name, otherwise RNA screws with the real thing */
  STRNCPY(workob->id.name, "OB<ConstrWorkOb>");

  /* If we're given a group to use, it's likely to be more efficient
   * (though a bit more dangerous). */
  if (agrp) {
    /* specifically evaluate this group only */
    PointerRNA id_ptr;

    /* get RNA-pointer for the workob's ID */
    RNA_id_pointer_create(&workob->id, &id_ptr);

    /* execute action for this group only */
    animsys_evaluate_action_group(&id_ptr, act, agrp, anim_eval_context);
  }
  else {
    AnimData adt = {NULL};

    /* init animdata, and attach to workob */
    workob->adt = &adt;

    adt.action = act;
    BKE_animdata_action_ensure_idroot(&workob->id, act);

    /* execute effects of Action on to workob (or its PoseChannels) */
    BKE_animsys_evaluate_animdata(&workob->id, &adt, anim_eval_context, ADT_RECALC_ANIM, false);
  }
}

void BKE_pose_check_uuids_unique_and_report(const bPose *pose)
{
  if (pose == NULL) {
    return;
  }

  GSet *used_uuids = BLI_gset_new(
      BLI_session_uuid_ghash_hash, BLI_session_uuid_ghash_compare, "sequencer used uuids");

  LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
    const SessionUUID *session_uuid = &pchan->runtime.session_uuid;
    if (!BLI_session_uuid_is_generated(session_uuid)) {
      printf("Pose channel %s does not have UUID generated.\n", pchan->name);
      continue;
    }

    if (BLI_gset_lookup(used_uuids, session_uuid) != NULL) {
      printf("Pose channel %s has duplicate UUID generated.\n", pchan->name);
      continue;
    }

    BLI_gset_insert(used_uuids, (void *)session_uuid);
  }

  BLI_gset_free(used_uuids, NULL);
}

void BKE_pose_blend_write(BlendWriter *writer, bPose *pose, bArmature *arm)
{
  /* Write each channel */
  if (pose == NULL) {
    return;
  }

  BLI_assert(arm != NULL);

  /* Write channels */
  LISTBASE_FOREACH (bPoseChannel *, chan, &pose->chanbase) {
    /* Write ID Properties -- and copy this comment EXACTLY for easy finding
     * of library blocks that implement this. */
    if (chan->prop) {
      IDP_BlendWrite(writer, chan->prop);
    }

    BKE_constraint_blend_write(writer, &chan->constraints);

    animviz_motionpath_blend_write(writer, chan->mpath);

    /* Prevent crashes with autosave,
     * when a bone duplicated in edit-mode has not yet been assigned to its pose-channel.
     * Also needed with memundo, in some cases we can store a step before pose has been
     * properly rebuilt from previous undo step. */
    Bone *bone = (pose->flag & POSE_RECALC) ? BKE_armature_find_bone_name(arm, chan->name) :
                                              chan->bone;
    if (bone != NULL) {
      /* gets restored on read, for library armatures */
      chan->selectflag = bone->flag & BONE_SELECTED;
    }

    BLO_write_struct(writer, bPoseChannel, chan);
  }

  /* Write groups */
  LISTBASE_FOREACH (bActionGroup *, grp, &pose->agroups) {
    BLO_write_struct(writer, bActionGroup, grp);
  }

  /* write IK param */
  if (pose->ikparam) {
    const char *structname = BKE_pose_ikparam_get_name(pose);
    if (structname) {
      BLO_write_struct_by_name(writer, structname, pose->ikparam);
    }
  }

  /* Write this pose */
  BLO_write_struct(writer, bPose, pose);
}

void BKE_pose_blend_read_data(BlendDataReader *reader, bPose *pose)
{
  if (!pose) {
    return;
  }

  BLO_read_list(reader, &pose->chanbase);
  BLO_read_list(reader, &pose->agroups);

  pose->chanhash = NULL;
  pose->chan_array = NULL;

  LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
    BKE_pose_channel_runtime_reset(&pchan->runtime);
    BKE_pose_channel_session_uuid_generate(pchan);

    pchan->bone = NULL;
    BLO_read_data_address(reader, &pchan->parent);
    BLO_read_data_address(reader, &pchan->child);
    BLO_read_data_address(reader, &pchan->custom_tx);

    BLO_read_data_address(reader, &pchan->bbone_prev);
    BLO_read_data_address(reader, &pchan->bbone_next);

    BKE_constraint_blend_read_data(reader, &pchan->constraints);

    BLO_read_data_address(reader, &pchan->prop);
    IDP_BlendDataRead(reader, &pchan->prop);

    BLO_read_data_address(reader, &pchan->mpath);
    if (pchan->mpath) {
      animviz_motionpath_blend_read_data(reader, pchan->mpath);
    }

    BLI_listbase_clear(&pchan->iktree);
    BLI_listbase_clear(&pchan->siktree);

    /* in case this value changes in future, clamp else we get undefined behavior */
    CLAMP(pchan->rotmode, ROT_MODE_MIN, ROT_MODE_MAX);

    pchan->draw_data = NULL;
  }
  pose->ikdata = NULL;
  if (pose->ikparam != NULL) {
    BLO_read_data_address(reader, &pose->ikparam);
  }
}

void BKE_pose_blend_read_lib(BlendLibReader *reader, Object *ob, bPose *pose)
{
  bArmature *arm = ob->data;

  if (!pose || !arm) {
    return;
  }

  /* Always rebuild to match library changes, except on Undo. */
  bool rebuild = false;

  if (!BLO_read_lib_is_undo(reader)) {
    if (ob->id.lib != arm->id.lib) {
      rebuild = true;
    }
  }

  LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
    BKE_constraint_blend_read_lib(reader, (ID *)ob, &pchan->constraints);

    pchan->bone = BKE_armature_find_bone_name(arm, pchan->name);

    IDP_BlendReadLib(reader, &ob->id, pchan->prop);

    BLO_read_id_address(reader, &ob->id, &pchan->custom);
    if (UNLIKELY(pchan->bone == NULL)) {
      rebuild = true;
    }
    else if (!ID_IS_LINKED(ob) && ID_IS_LINKED(arm)) {
      /* local pose selection copied to armature, bit hackish */
      pchan->bone->flag &= ~BONE_SELECTED;
      pchan->bone->flag |= pchan->selectflag;
    }
  }

  if (rebuild) {
    Main *bmain = BLO_read_lib_get_main(reader);
    DEG_id_tag_update_ex(
        bmain, &ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
    BKE_pose_tag_recalc(bmain, pose);
  }
}

void BKE_pose_blend_read_expand(BlendExpander *expander, bPose *pose)
{
  if (!pose) {
    return;
  }

  LISTBASE_FOREACH (bPoseChannel *, chan, &pose->chanbase) {
    BKE_constraint_blend_read_expand(expander, &chan->constraints);
    IDP_BlendReadExpand(expander, chan->prop);
    BLO_expand(expander, chan->custom);
  }
}

void BKE_action_fcurves_clear(bAction *act)
{
  if (!act) {
    return;
  }
  while (act->curves.first) {
    FCurve *fcu = act->curves.first;
    action_groups_remove_channel(act, fcu);
    BKE_fcurve_free(fcu);
  }
  DEG_id_tag_update(&act->id, ID_RECALC_ANIMATION_NO_FLUSH);
}
