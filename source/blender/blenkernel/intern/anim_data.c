/* SPDX-FileCopyrightText: 2009 Blender Foundation, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */
#include "MEM_guardedalloc.h"

#include <string.h>

#include "BKE_action.h"
#include "BKE_anim_data.h"
#include "BKE_animsys.h"
#include "BKE_context.h"
#include "BKE_fcurve.h"
#include "BKE_fcurve_driver.h"
#include "BKE_global.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_nla.h"
#include "BKE_node.h"
#include "BKE_report.h"

#include "DNA_ID.h"
#include "DNA_anim_types.h"
#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_world_types.h"

#include "BLI_alloca.h"
#include "BLI_dynstr.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DEG_depsgraph.h"

#include "BLO_read_write.h"

#include "RNA_access.h"
#include "RNA_path.h"

#include "CLG_log.h"

static CLG_LogRef LOG = {"bke.anim_sys"};

/* ***************************************** */
/* AnimData API */

/* Getter/Setter -------------------------------------------- */

bool id_type_can_have_animdata(const short id_type)
{
  const IDTypeInfo *typeinfo = BKE_idtype_get_info_from_idcode(id_type);
  if (typeinfo != NULL) {
    return (typeinfo->flags & IDTYPE_FLAGS_NO_ANIMDATA) == 0;
  }
  return false;
}

bool id_can_have_animdata(const ID *id)
{
  /* sanity check */
  if (id == NULL) {
    return false;
  }

  return id_type_can_have_animdata(GS(id->name));
}

AnimData *BKE_animdata_from_id(const ID *id)
{
  /* In order for this to work, we assume that the #AnimData pointer is stored
   * immediately after the given ID-block in the struct, as per IdAdtTemplate. */

  /* Only some ID-blocks have this info for now, so we cast the types that do
   * to be of type IdAdtTemplate, and add the AnimData to it using the template. */
  if (id_can_have_animdata(id)) {
    IdAdtTemplate *iat = (IdAdtTemplate *)id;
    return iat->adt;
  }
  return NULL;
}

AnimData *BKE_animdata_ensure_id(ID *id)
{
  /* In order for this to work, we assume that the #AnimData pointer is stored
   * immediately after the given ID-block in the struct, as per IdAdtTemplate. */

  /* Only some ID-blocks have this info for now, so we cast the types that do
   * to be of type IdAdtTemplate, and add the AnimData to it using the template. */
  if (id_can_have_animdata(id)) {
    IdAdtTemplate *iat = (IdAdtTemplate *)id;

    /* check if there's already AnimData, in which case, don't add */
    if (iat->adt == NULL) {
      AnimData *adt;

      /* add animdata */
      adt = iat->adt = MEM_callocN(sizeof(AnimData), "AnimData");

      /* set default settings */
      adt->act_influence = 1.0f;
    }

    return iat->adt;
  }
  return NULL;
}

/* Action / Tmpact Setter shared code -------------------------
 *
 * Both the action and tmpact setter functions have essentially
 * identical semantics, because tmpact is just a place to temporarily
 * store the main action during tweaking.  This function contains the
 * shared code between those two setter functions, setting the action
 * of the passed `act_slot` to `act`.
 *
 * Preconditions:
 * - `id` and `act_slot` must be non-null (but the pointer `act_slot`
 *   points to can be null).
 * - `id` must have animation data.
 * - `act_slot` must be a pointer to either the `action` or `tmpact`
 *   field of `id`'s animation data.
 */
static bool animdata_set_action(ReportList *reports, ID *id, bAction **act_slot, bAction *act)
{
  /* Action must have same type as owner. */
  if (!BKE_animdata_action_ensure_idroot(id, act)) {
    /* Cannot set to this type. */
    BKE_reportf(
        reports,
        RPT_ERROR,
        "Could not set action '%s' onto ID '%s', as it does not have suitably rooted paths "
        "for this purpose",
        act->id.name + 2,
        id->name);
    return false;
  }

  if (*act_slot == act) {
    /* Don't bother reducing and increasing the user count when there is nothing changing. */
    return true;
  }

  /* Unassign current action. */
  if (*act_slot) {
    id_us_min((ID *)*act_slot);
    *act_slot = NULL;
  }

  if (act == NULL) {
    return true;
  }

  *act_slot = act;
  id_us_plus((ID *)*act_slot);

  return true;
}

/* Tmpact Setter --------------------------------------- */
bool BKE_animdata_set_tmpact(ReportList *reports, ID *id, bAction *act)
{
  AnimData *adt = BKE_animdata_from_id(id);

  if (adt == NULL) {
    BKE_report(reports, RPT_WARNING, "No AnimData to set tmpact on");
    return false;
  }

  return animdata_set_action(reports, id, &adt->tmpact, act);
}

/* Action Setter --------------------------------------- */
bool BKE_animdata_set_action(ReportList *reports, ID *id, bAction *act)
{
  AnimData *adt = BKE_animdata_from_id(id);

  if (adt == NULL) {
    BKE_report(reports, RPT_WARNING, "No AnimData to set action on");
    return false;
  }

  if (!BKE_animdata_action_editable(adt)) {
    /* Cannot remove, otherwise things turn to custard. */
    BKE_report(reports, RPT_ERROR, "Cannot change action, as it is still being edited in NLA");
    return false;
  }

  return animdata_set_action(reports, id, &adt->action, act);
}

bool BKE_animdata_action_editable(const AnimData *adt)
{
  /* Active action is only editable when it is not a tweaking strip. */
  const bool is_tweaking_strip = (adt->flag & ADT_NLA_EDIT_ON) || adt->actstrip != NULL ||
                                 adt->tmpact != NULL;
  return !is_tweaking_strip;
}

bool BKE_animdata_action_ensure_idroot(const ID *owner, bAction *action)
{
  const int idcode = GS(owner->name);

  if (action == NULL) {
    /* A NULL action is usable by any ID type. */
    return true;
  }

  if (action->idroot == 0) {
    /* First time this Action is assigned, lock it to this ID type. */
    action->idroot = idcode;
    return true;
  }

  return (action->idroot == idcode);
}

/* Freeing -------------------------------------------- */

void BKE_animdata_free(ID *id, const bool do_id_user)
{
  /* Only some ID-blocks have this info for now, so we cast the
   * types that do to be of type IdAdtTemplate
   */
  if (id_can_have_animdata(id)) {
    IdAdtTemplate *iat = (IdAdtTemplate *)id;
    AnimData *adt = iat->adt;

    /* check if there's any AnimData to start with */
    if (adt) {
      if (do_id_user) {
        /* unlink action (don't free, as it's in its own list) */
        if (adt->action) {
          id_us_min(&adt->action->id);
        }
        /* same goes for the temporarily displaced action */
        if (adt->tmpact) {
          id_us_min(&adt->tmpact->id);
        }
      }

      /* free nla data */
      BKE_nla_tracks_free(&adt->nla_tracks, do_id_user);

      /* free drivers - stored as a list of F-Curves */
      BKE_fcurves_free(&adt->drivers);

      /* free driver array cache */
      MEM_SAFE_FREE(adt->driver_array);

      /* free overrides */
      /* TODO... */

      /* free animdata now */
      MEM_freeN(adt);
      iat->adt = NULL;
    }
  }
}

bool BKE_animdata_id_is_animated(const ID *id)
{
  if (id == NULL) {
    return false;
  }

  const AnimData *adt = BKE_animdata_from_id((ID *)id);
  if (adt == NULL) {
    return false;
  }

  if (adt->action != NULL && !BLI_listbase_is_empty(&adt->action->curves)) {
    return true;
  }

  return !BLI_listbase_is_empty(&adt->drivers) || !BLI_listbase_is_empty(&adt->nla_tracks) ||
         !BLI_listbase_is_empty(&adt->overrides);
}

void BKE_animdata_foreach_id(AnimData *adt, LibraryForeachIDData *data)
{
  LISTBASE_FOREACH (FCurve *, fcu, &adt->drivers) {
    BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(data, BKE_fcurve_foreach_id(fcu, data));
  }

  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, adt->action, IDWALK_CB_USER);
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, adt->tmpact, IDWALK_CB_USER);

  LISTBASE_FOREACH (NlaTrack *, nla_track, &adt->nla_tracks) {
    LISTBASE_FOREACH (NlaStrip *, nla_strip, &nla_track->strips) {
      BKE_nla_strip_foreach_id(nla_strip, data);
    }
  }
}

/* Copying -------------------------------------------- */

AnimData *BKE_animdata_copy(Main *bmain, AnimData *adt, const int flag)
{
  AnimData *dadt;

  const bool do_action = (flag & LIB_ID_COPY_ACTIONS) != 0 && (flag & LIB_ID_CREATE_NO_MAIN) == 0;
  const bool do_id_user = (flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0;

  /* sanity check before duplicating struct */
  if (adt == NULL) {
    return NULL;
  }
  dadt = MEM_dupallocN(adt);

  /* make a copy of action - at worst, user has to delete copies... */
  if (do_action) {
    /* Recursive copy of 'real' IDs is a bit hairy. Even if do not want to deal with user-count
     * when copying ID's data itself, we still need to do so with sub-IDs, since those will not be
     * handled by later 'update user-counts of used IDs' code as used e.g. at end of
     * #BKE_id_copy_ex().
     * So in case we do copy the ID and its sub-IDs in bmain, silence the 'no user-count' flag for
     * the sub-IDs copying.
     * NOTE: This is a bit weak, as usually when it comes to recursive ID copy. Should work for
     * now, but we may have to revisit this at some point and add a proper extra flag to deal with
     * that situation. Or refactor completely the way we handle such recursion, by flattening it
     * e.g. */
    const int id_copy_flag = (flag & LIB_ID_CREATE_NO_MAIN) == 0 ?
                                 flag & ~LIB_ID_CREATE_NO_USER_REFCOUNT :
                                 flag;
    BLI_assert(bmain != NULL);
    BLI_assert(dadt->action == NULL || dadt->action != dadt->tmpact);
    dadt->action = (bAction *)BKE_id_copy_ex(bmain, (ID *)dadt->action, NULL, id_copy_flag);
    dadt->tmpact = (bAction *)BKE_id_copy_ex(bmain, (ID *)dadt->tmpact, NULL, id_copy_flag);
  }
  else if (do_id_user) {
    id_us_plus((ID *)dadt->action);
    id_us_plus((ID *)dadt->tmpact);
  }

  /* duplicate NLA data */
  BKE_nla_tracks_copy_from_adt(bmain, dadt, adt, flag);

  /* duplicate drivers (F-Curves) */
  BKE_fcurves_copy(&dadt->drivers, &adt->drivers);
  dadt->driver_array = NULL;

  /* don't copy overrides */
  BLI_listbase_clear(&dadt->overrides);

  /* return */
  return dadt;
}

bool BKE_animdata_copy_id(Main *bmain, ID *id_to, ID *id_from, const int flag)
{
  AnimData *adt;

  if ((id_to && id_from) && (GS(id_to->name) != GS(id_from->name))) {
    return false;
  }

  BKE_animdata_free(id_to, (flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0);

  adt = BKE_animdata_from_id(id_from);
  if (adt) {
    IdAdtTemplate *iat = (IdAdtTemplate *)id_to;
    iat->adt = BKE_animdata_copy(bmain, adt, flag);
  }

  return true;
}

static void animdata_copy_id_action(Main *bmain,
                                    ID *id,
                                    const bool set_newid,
                                    const bool do_linked_id)
{
  AnimData *adt = BKE_animdata_from_id(id);
  if (adt) {
    if (adt->action && (do_linked_id || !ID_IS_LINKED(adt->action))) {
      id_us_min((ID *)adt->action);
      adt->action = set_newid ? ID_NEW_SET(adt->action, BKE_id_copy(bmain, &adt->action->id)) :
                                BKE_id_copy(bmain, &adt->action->id);
    }
    if (adt->tmpact && (do_linked_id || !ID_IS_LINKED(adt->tmpact))) {
      id_us_min((ID *)adt->tmpact);
      adt->tmpact = set_newid ? ID_NEW_SET(adt->tmpact, BKE_id_copy(bmain, &adt->tmpact->id)) :
                                BKE_id_copy(bmain, &adt->tmpact->id);
    }
  }
  bNodeTree *ntree = ntreeFromID(id);
  if (ntree) {
    animdata_copy_id_action(bmain, &ntree->id, set_newid, do_linked_id);
  }
  /* Note that collections are not animatable currently, so no need to handle scenes' master
   * collection here. */
}

void BKE_animdata_copy_id_action(Main *bmain, ID *id)
{
  const bool is_id_liboverride = ID_IS_OVERRIDE_LIBRARY(id);
  animdata_copy_id_action(bmain, id, false, !is_id_liboverride);
}

void BKE_animdata_duplicate_id_action(Main *bmain,
                                      ID *id,
                                      const /*eDupli_ID_Flags*/ uint duplicate_flags)
{
  if (duplicate_flags & USER_DUP_ACT) {
    animdata_copy_id_action(bmain, id, true, (duplicate_flags & USER_DUP_LINKED_ID) != 0);
  }
}

void BKE_animdata_merge_copy(
    Main *bmain, ID *dst_id, ID *src_id, eAnimData_MergeCopy_Modes action_mode, bool fix_drivers)
{
  AnimData *src = BKE_animdata_from_id(src_id);
  AnimData *dst = BKE_animdata_from_id(dst_id);

  /* sanity checks */
  if (ELEM(NULL, dst, src)) {
    return;
  }

  /* TODO: we must unset all "tweak-mode" flags. */
  if ((src->flag & ADT_NLA_EDIT_ON) || (dst->flag & ADT_NLA_EDIT_ON)) {
    CLOG_ERROR(
        &LOG,
        "Merging AnimData blocks while editing NLA is dangerous as it may cause data corruption");
    return;
  }

  /* handle actions... */
  if (action_mode == ADT_MERGECOPY_SRC_COPY) {
    /* make a copy of the actions */
    dst->action = (bAction *)BKE_id_copy(bmain, &src->action->id);
    dst->tmpact = (bAction *)BKE_id_copy(bmain, &src->tmpact->id);
  }
  else if (action_mode == ADT_MERGECOPY_SRC_REF) {
    /* make a reference to it */
    dst->action = src->action;
    id_us_plus((ID *)dst->action);

    dst->tmpact = src->tmpact;
    id_us_plus((ID *)dst->tmpact);
  }

  /* duplicate NLA data */
  if (src->nla_tracks.first) {
    ListBase tracks = {NULL, NULL};

    BKE_nla_tracks_copy(bmain, &tracks, &src->nla_tracks, 0);
    BLI_movelisttolist(&dst->nla_tracks, &tracks);
  }

  /* duplicate drivers (F-Curves) */
  if (src->drivers.first) {
    ListBase drivers = {NULL, NULL};

    BKE_fcurves_copy(&drivers, &src->drivers);

    /* Fix up all driver targets using the old target id
     * - This assumes that the src ID is being merged into the dst ID
     */
    if (fix_drivers) {
      FCurve *fcu;

      for (fcu = drivers.first; fcu; fcu = fcu->next) {
        ChannelDriver *driver = fcu->driver;
        DriverVar *dvar;

        for (dvar = driver->variables.first; dvar; dvar = dvar->next) {
          DRIVER_TARGETS_USED_LOOPER_BEGIN (dvar) {
            if (dtar->id == src_id) {
              dtar->id = dst_id;
            }
          }
          DRIVER_TARGETS_LOOPER_END;
        }
      }
    }

    BLI_movelisttolist(&dst->drivers, &drivers);
  }
}

/* Sub-ID Regrouping ------------------------------------------- */

/**
 * Helper heuristic for determining if a path is compatible with the basepath
 *
 * \param path: Full RNA-path from some data (usually an F-Curve) to compare
 * \param basepath: Shorter path fragment to look for
 * \return Whether there is a match
 */
static bool animpath_matches_basepath(const char path[], const char basepath[])
{
  /* we need start of path to be basepath */
  return (path && basepath) && STRPREFIX(path, basepath);
}

static void animpath_update_basepath(FCurve *fcu,
                                     const char *old_basepath,
                                     const char *new_basepath)
{
  BLI_assert(animpath_matches_basepath(fcu->rna_path, old_basepath));
  if (STREQ(old_basepath, new_basepath)) {
    return;
  }

  char *new_path = BLI_sprintfN("%s%s", new_basepath, fcu->rna_path + strlen(old_basepath));
  MEM_freeN(fcu->rna_path);
  fcu->rna_path = new_path;
}

/* Move F-Curves in src action to dst action, setting up all the necessary groups
 * for this to happen, but only if the F-Curves being moved have the appropriate
 * "base path".
 * - This is used when data moves from one data-block to another, causing the
 *   F-Curves to need to be moved over too
 */
static void action_move_fcurves_by_basepath(bAction *srcAct,
                                            bAction *dstAct,
                                            const char *src_basepath,
                                            const char *dst_basepath)
{
  FCurve *fcu, *fcn = NULL;

  /* sanity checks */
  if (ELEM(NULL, srcAct, dstAct, src_basepath, dst_basepath)) {
    if (G.debug & G_DEBUG) {
      CLOG_ERROR(&LOG,
                 "srcAct: %p, dstAct: %p, src_basepath: %p, dst_basepath: %p has insufficient "
                 "info to work with",
                 (void *)srcAct,
                 (void *)dstAct,
                 (void *)src_basepath,
                 (void *)dst_basepath);
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
     * - we only need the start of the path to match basepath
     */
    if (animpath_matches_basepath(fcu->rna_path, src_basepath)) {
      bActionGroup *agrp = NULL;

      /* if grouped... */
      if (fcu->grp) {
        /* make sure there will be a matching group on the other side for the migrants */
        agrp = BKE_action_group_find_name(dstAct, fcu->grp->name);

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

      animpath_update_basepath(fcu, src_basepath, dst_basepath);

      if (agrp) {
        action_groups_add_channel(dstAct, agrp, fcu);
      }
      else {
        BLI_addtail(&dstAct->curves, fcu);
      }
    }
  }

  /* cleanup groups (if present) */
  if (srcAct->groups.first) {
    bActionGroup *agrp, *grp = NULL;

    for (agrp = srcAct->groups.first; agrp; agrp = grp) {
      grp = agrp->next;

      /* only tagged groups need to be considered - clearing these tags or removing them */
      if (agrp->flag & AGRP_TEMP) {
        /* if group is empty and tagged, then we can remove as this operation
         * moved out all the channels that were formerly here
         */
        if (BLI_listbase_is_empty(&agrp->channels)) {
          BLI_freelinkN(&srcAct->groups, agrp);
        }
        else {
          agrp->flag &= ~AGRP_TEMP;
        }
      }
    }
  }
}

static void animdata_move_drivers_by_basepath(AnimData *srcAdt,
                                              AnimData *dstAdt,
                                              const char *src_basepath,
                                              const char *dst_basepath)
{
  LISTBASE_FOREACH_MUTABLE (FCurve *, fcu, &srcAdt->drivers) {
    if (animpath_matches_basepath(fcu->rna_path, src_basepath)) {
      animpath_update_basepath(fcu, src_basepath, dst_basepath);
      BLI_remlink(&srcAdt->drivers, fcu);
      BLI_addtail(&dstAdt->drivers, fcu);

      /* TODO: add depsgraph flushing calls? */
    }
  }
}

void BKE_animdata_transfer_by_basepath(Main *bmain, ID *srcID, ID *dstID, ListBase *basepaths)
{
  AnimData *srcAdt = NULL, *dstAdt = NULL;

  /* sanity checks */
  if (ELEM(NULL, srcID, dstID)) {
    if (G.debug & G_DEBUG) {
      CLOG_ERROR(&LOG, "no source or destination ID to separate AnimData with");
    }
    return;
  }

  /* get animdata from src, and create for destination (if needed) */
  srcAdt = BKE_animdata_from_id(srcID);
  dstAdt = BKE_animdata_ensure_id(dstID);

  if (ELEM(NULL, srcAdt, dstAdt)) {
    if (G.debug & G_DEBUG) {
      CLOG_ERROR(&LOG, "no AnimData for this pair of ID's");
    }
    return;
  }

  /* active action */
  if (srcAdt->action) {
    /* Set up an action if necessary,
     * and name it in a similar way so that it can be easily found again. */
    if (dstAdt->action == NULL) {
      dstAdt->action = BKE_action_add(bmain, srcAdt->action->id.name + 2);
      BKE_animdata_action_ensure_idroot(dstID, dstAdt->action);
    }
    else if (dstAdt->action == srcAdt->action) {
      CLOG_WARN(&LOG,
                "Argh! Source and Destination share animation! "
                "('%s' and '%s' both use '%s') Making new empty action",
                srcID->name,
                dstID->name,
                srcAdt->action->id.name);

      /* TODO: review this... */
      id_us_min(&dstAdt->action->id);
      dstAdt->action = BKE_action_add(bmain, dstAdt->action->id.name + 2);
      BKE_animdata_action_ensure_idroot(dstID, dstAdt->action);
    }

    /* loop over base paths, trying to fix for each one... */
    LISTBASE_FOREACH (const AnimationBasePathChange *, basepath_change, basepaths) {
      action_move_fcurves_by_basepath(srcAdt->action,
                                      dstAdt->action,
                                      basepath_change->src_basepath,
                                      basepath_change->dst_basepath);
    }
  }

  /* drivers */
  if (srcAdt->drivers.first) {
    LISTBASE_FOREACH (const AnimationBasePathChange *, basepath_change, basepaths) {
      animdata_move_drivers_by_basepath(
          srcAdt, dstAdt, basepath_change->src_basepath, basepath_change->dst_basepath);
    }
  }
  /* Tag source action because list of fcurves changed. */
  DEG_id_tag_update(&srcAdt->action->id, ID_RECALC_COPY_ON_WRITE);
}

/* Path Validation -------------------------------------------- */

/* Check if a given RNA Path is valid, by tracing it from the given ID,
 * and seeing if we can resolve it. */
static bool check_rna_path_is_valid(ID *owner_id, const char *path)
{
  PointerRNA id_ptr, ptr;
  PropertyRNA *prop = NULL;

  /* make initial RNA pointer to start resolving from */
  RNA_id_pointer_create(owner_id, &id_ptr);

  /* try to resolve */
  return RNA_path_resolve_property(&id_ptr, path, &ptr, &prop);
}

/* Check if some given RNA Path needs fixing - free the given path and set a new one as appropriate
 * NOTE: we assume that oldName and newName have [" "] padding around them
 */
static char *rna_path_rename_fix(ID *owner_id,
                                 const char *prefix,
                                 const char *oldName,
                                 const char *newName,
                                 char *oldpath,
                                 bool verify_paths)
{
  char *prefixPtr = strstr(oldpath, prefix);
  char *oldNamePtr = strstr(oldpath, oldName);
  int prefixLen = strlen(prefix);
  int oldNameLen = strlen(oldName);

  /* only start fixing the path if the prefix and oldName feature in the path,
   * and prefix occurs immediately before oldName
   */
  if ((prefixPtr && oldNamePtr) && (prefixPtr + prefixLen == oldNamePtr)) {
    /* if we haven't aren't able to resolve the path now, try again after fixing it */
    if (!verify_paths || check_rna_path_is_valid(owner_id, oldpath) == 0) {
      DynStr *ds = BLI_dynstr_new();
      const char *postfixPtr = oldNamePtr + oldNameLen;
      char *newPath = NULL;

      /* add the part of the string that goes up to the start of the prefix */
      if (prefixPtr > oldpath) {
        BLI_dynstr_nappend(ds, oldpath, prefixPtr - oldpath);
      }

      /* add the prefix */
      BLI_dynstr_append(ds, prefix);

      /* add the new name (complete with brackets) */
      BLI_dynstr_append(ds, newName);

      /* add the postfix */
      BLI_dynstr_append(ds, postfixPtr);

      /* create new path, and cleanup old data */
      newPath = BLI_dynstr_get_cstring(ds);
      BLI_dynstr_free(ds);

      /* check if the new path will solve our problems */
      /* TODO: will need to check whether this step really helps in practice */
      if (!verify_paths || check_rna_path_is_valid(owner_id, newPath)) {
        /* free the old path, and return the new one, since we've solved the issues */
        MEM_freeN(oldpath);
        return newPath;
      }

      /* still couldn't resolve the path... so, might as well just leave it alone */
      MEM_freeN(newPath);
    }
  }

  /* the old path doesn't need to be changed */
  return oldpath;
}

/* Check RNA-Paths for a list of F-Curves */
static bool fcurves_path_rename_fix(ID *owner_id,
                                    const char *prefix,
                                    const char *oldName,
                                    const char *newName,
                                    const char *oldKey,
                                    const char *newKey,
                                    ListBase *curves,
                                    bool verify_paths)
{
  FCurve *fcu;
  bool is_changed = false;
  /* We need to check every curve. */
  for (fcu = curves->first; fcu; fcu = fcu->next) {
    if (fcu->rna_path == NULL) {
      continue;
    }
    const char *old_path = fcu->rna_path;
    /* Firstly, handle the F-Curve's own path. */
    fcu->rna_path = rna_path_rename_fix(
        owner_id, prefix, oldKey, newKey, fcu->rna_path, verify_paths);
    /* if path changed and the F-Curve is grouped, check if its group also needs renaming
     * (i.e. F-Curve is first of a bone's F-Curves;
     * hence renaming this should also trigger rename) */
    if (fcu->rna_path != old_path) {
      bActionGroup *agrp = fcu->grp;
      is_changed = true;
      if (oldName != NULL && (agrp != NULL) && STREQ(oldName, agrp->name)) {
        STRNCPY(agrp->name, newName);
      }
    }
  }
  return is_changed;
}

/* Check RNA-Paths for a list of Drivers */
static bool drivers_path_rename_fix(ID *owner_id,
                                    ID *ref_id,
                                    const char *prefix,
                                    const char *oldName,
                                    const char *newName,
                                    const char *oldKey,
                                    const char *newKey,
                                    ListBase *curves,
                                    bool verify_paths)
{
  bool is_changed = false;
  FCurve *fcu;
  /* We need to check every curve - drivers are F-Curves too. */
  for (fcu = curves->first; fcu; fcu = fcu->next) {
    /* firstly, handle the F-Curve's own path */
    if (fcu->rna_path != NULL) {
      const char *old_rna_path = fcu->rna_path;
      fcu->rna_path = rna_path_rename_fix(
          owner_id, prefix, oldKey, newKey, fcu->rna_path, verify_paths);
      is_changed |= (fcu->rna_path != old_rna_path);
    }
    if (fcu->driver == NULL) {
      continue;
    }
    ChannelDriver *driver = fcu->driver;
    DriverVar *dvar;
    /* driver variables */
    for (dvar = driver->variables.first; dvar; dvar = dvar->next) {
      /* only change the used targets, since the others will need fixing manually anyway */
      DRIVER_TARGETS_USED_LOOPER_BEGIN (dvar) {
        /* rename RNA path */
        if (dtar->rna_path && dtar->id) {
          const char *old_rna_path = dtar->rna_path;
          dtar->rna_path = rna_path_rename_fix(
              dtar->id, prefix, oldKey, newKey, dtar->rna_path, verify_paths);
          is_changed |= (dtar->rna_path != old_rna_path);
        }
        /* also fix the bone-name (if applicable) */
        if (strstr(prefix, "bones")) {
          if (((dtar->id) && (GS(dtar->id->name) == ID_OB) &&
               (!ref_id || ((Object *)(dtar->id))->data == ref_id)) &&
              (dtar->pchan_name[0]) && STREQ(oldName, dtar->pchan_name))
          {
            is_changed = true;
            STRNCPY(dtar->pchan_name, newName);
          }
        }
      }
      DRIVER_TARGETS_LOOPER_END;
    }
  }
  return is_changed;
}

/* Fix all RNA-Paths for Actions linked to NLA Strips */
static bool nlastrips_path_rename_fix(ID *owner_id,
                                      const char *prefix,
                                      const char *oldName,
                                      const char *newName,
                                      const char *oldKey,
                                      const char *newKey,
                                      ListBase *strips,
                                      bool verify_paths)
{
  NlaStrip *strip;
  bool is_changed = false;
  /* Recursively check strips, fixing only actions. */
  for (strip = strips->first; strip; strip = strip->next) {
    /* fix strip's action */
    if (strip->act != NULL) {
      is_changed |= fcurves_path_rename_fix(
          owner_id, prefix, oldName, newName, oldKey, newKey, &strip->act->curves, verify_paths);
    }
    /* Ignore own F-Curves, since those are local. */
    /* Check sub-strips (if meta-strips). */
    is_changed |= nlastrips_path_rename_fix(
        owner_id, prefix, oldName, newName, oldKey, newKey, &strip->strips, verify_paths);
  }
  return is_changed;
}

/* Rename Sub-ID Entities in RNA Paths ----------------------- */

char *BKE_animsys_fix_rna_path_rename(ID *owner_id,
                                      char *old_path,
                                      const char *prefix,
                                      const char *oldName,
                                      const char *newName,
                                      int oldSubscript,
                                      int newSubscript,
                                      bool verify_paths)
{
  char *oldN, *newN;
  char *result;

  /* if no action, no need to proceed */
  if (ELEM(NULL, owner_id, old_path)) {
    if (G.debug & G_DEBUG) {
      CLOG_WARN(&LOG, "early abort");
    }
    return old_path;
  }

  /* Name sanitation logic - copied from BKE_animdata_fix_paths_rename() */
  if ((oldName != NULL) && (newName != NULL)) {
    /* pad the names with [" "] so that only exact matches are made */
    const size_t name_old_len = strlen(oldName);
    const size_t name_new_len = strlen(newName);
    char *name_old_esc = BLI_array_alloca(name_old_esc, (name_old_len * 2) + 1);
    char *name_new_esc = BLI_array_alloca(name_new_esc, (name_new_len * 2) + 1);

    BLI_str_escape(name_old_esc, oldName, (name_old_len * 2) + 1);
    BLI_str_escape(name_new_esc, newName, (name_new_len * 2) + 1);
    oldN = BLI_sprintfN("[\"%s\"]", name_old_esc);
    newN = BLI_sprintfN("[\"%s\"]", name_new_esc);
  }
  else {
    oldN = BLI_sprintfN("[%d]", oldSubscript);
    newN = BLI_sprintfN("[%d]", newSubscript);
  }

  /* fix given path */
  if (G.debug & G_DEBUG) {
    printf("%s | %s  | oldpath = %p ", oldN, newN, old_path);
  }
  result = rna_path_rename_fix(owner_id, prefix, oldN, newN, old_path, verify_paths);
  if (G.debug & G_DEBUG) {
    printf("path rename result = %p\n", result);
  }

  /* free the temp names */
  MEM_freeN(oldN);
  MEM_freeN(newN);

  /* return the resulting path - may be the same path again if nothing changed */
  return result;
}

void BKE_action_fix_paths_rename(ID *owner_id,
                                 bAction *act,
                                 const char *prefix,
                                 const char *oldName,
                                 const char *newName,
                                 int oldSubscript,
                                 int newSubscript,
                                 bool verify_paths)
{
  char *oldN, *newN;

  /* if no action, no need to proceed */
  if (ELEM(NULL, owner_id, act)) {
    return;
  }

  /* Name sanitation logic - copied from BKE_animdata_fix_paths_rename() */
  if ((oldName != NULL) && (newName != NULL)) {
    /* pad the names with [" "] so that only exact matches are made */
    const size_t name_old_len = strlen(oldName);
    const size_t name_new_len = strlen(newName);
    char *name_old_esc = BLI_array_alloca(name_old_esc, (name_old_len * 2) + 1);
    char *name_new_esc = BLI_array_alloca(name_new_esc, (name_new_len * 2) + 1);

    BLI_str_escape(name_old_esc, oldName, (name_old_len * 2) + 1);
    BLI_str_escape(name_new_esc, newName, (name_new_len * 2) + 1);
    oldN = BLI_sprintfN("[\"%s\"]", name_old_esc);
    newN = BLI_sprintfN("[\"%s\"]", name_new_esc);
  }
  else {
    oldN = BLI_sprintfN("[%d]", oldSubscript);
    newN = BLI_sprintfN("[%d]", newSubscript);
  }

  /* fix paths in action */
  fcurves_path_rename_fix(
      owner_id, prefix, oldName, newName, oldN, newN, &act->curves, verify_paths);

  /* free the temp names */
  MEM_freeN(oldN);
  MEM_freeN(newN);
}

void BKE_animdata_fix_paths_rename(ID *owner_id,
                                   AnimData *adt,
                                   ID *ref_id,
                                   const char *prefix,
                                   const char *oldName,
                                   const char *newName,
                                   int oldSubscript,
                                   int newSubscript,
                                   bool verify_paths)
{
  NlaTrack *nlt;
  char *oldN, *newN;
  /* If no AnimData, no need to proceed. */
  if (ELEM(NULL, owner_id, adt)) {
    return;
  }
  bool is_self_changed = false;
  /* Name sanitation logic - shared with BKE_action_fix_paths_rename(). */
  if ((oldName != NULL) && (newName != NULL)) {
    /* Pad the names with [" "] so that only exact matches are made. */
    const size_t name_old_len = strlen(oldName);
    const size_t name_new_len = strlen(newName);
    char *name_old_esc = BLI_array_alloca(name_old_esc, (name_old_len * 2) + 1);
    char *name_new_esc = BLI_array_alloca(name_new_esc, (name_new_len * 2) + 1);

    BLI_str_escape(name_old_esc, oldName, (name_old_len * 2) + 1);
    BLI_str_escape(name_new_esc, newName, (name_new_len * 2) + 1);
    oldN = BLI_sprintfN("[\"%s\"]", name_old_esc);
    newN = BLI_sprintfN("[\"%s\"]", name_new_esc);
  }
  else {
    oldN = BLI_sprintfN("[%d]", oldSubscript);
    newN = BLI_sprintfN("[%d]", newSubscript);
  }
  /* Active action and temp action. */
  if (adt->action != NULL) {
    if (fcurves_path_rename_fix(
            owner_id, prefix, oldName, newName, oldN, newN, &adt->action->curves, verify_paths))
    {
      DEG_id_tag_update(&adt->action->id, ID_RECALC_COPY_ON_WRITE);
    }
  }
  if (adt->tmpact) {
    if (fcurves_path_rename_fix(
            owner_id, prefix, oldName, newName, oldN, newN, &adt->tmpact->curves, verify_paths))
    {
      DEG_id_tag_update(&adt->tmpact->id, ID_RECALC_COPY_ON_WRITE);
    }
  }
  /* Drivers - Drivers are really F-Curves */
  is_self_changed |= drivers_path_rename_fix(
      owner_id, ref_id, prefix, oldName, newName, oldN, newN, &adt->drivers, verify_paths);
  /* NLA Data - Animation Data for Strips */
  for (nlt = adt->nla_tracks.first; nlt; nlt = nlt->next) {
    is_self_changed |= nlastrips_path_rename_fix(
        owner_id, prefix, oldName, newName, oldN, newN, &nlt->strips, verify_paths);
  }
  /* Tag owner ID if it */
  if (is_self_changed) {
    DEG_id_tag_update(owner_id, ID_RECALC_COPY_ON_WRITE);
  }
  /* free the temp names */
  MEM_freeN(oldN);
  MEM_freeN(newN);
}

/* Remove FCurves with Prefix  -------------------------------------- */

/* Check RNA-Paths for a list of F-Curves */
static bool fcurves_path_remove_fix(const char *prefix, ListBase *curves)
{
  FCurve *fcu, *fcn;
  bool any_removed = false;
  if (!prefix) {
    return any_removed;
  }

  /* we need to check every curve... */
  for (fcu = curves->first; fcu; fcu = fcn) {
    fcn = fcu->next;

    if (fcu->rna_path) {
      if (STRPREFIX(fcu->rna_path, prefix)) {
        BLI_remlink(curves, fcu);
        BKE_fcurve_free(fcu);
        any_removed = true;
      }
    }
  }
  return any_removed;
}

/* Check RNA-Paths for a list of F-Curves */
static bool nlastrips_path_remove_fix(const char *prefix, ListBase *strips)
{
  NlaStrip *strip;
  bool any_removed = false;

  /* recursively check strips, fixing only actions... */
  for (strip = strips->first; strip; strip = strip->next) {
    /* fix strip's action */
    if (strip->act) {
      any_removed |= fcurves_path_remove_fix(prefix, &strip->act->curves);
    }

    /* Check sub-strips (if meta-strips). */
    any_removed |= nlastrips_path_remove_fix(prefix, &strip->strips);
  }
  return any_removed;
}

bool BKE_animdata_fix_paths_remove(ID *id, const char *prefix)
{
  /* Only some ID-blocks have this info for now, so we cast the
   * types that do to be of type IdAdtTemplate
   */
  if (!id_can_have_animdata(id)) {
    return false;
  }
  bool any_removed = false;
  IdAdtTemplate *iat = (IdAdtTemplate *)id;
  AnimData *adt = iat->adt;
  /* check if there's any AnimData to start with */
  if (adt) {
    /* free fcurves */
    if (adt->action != NULL) {
      any_removed |= fcurves_path_remove_fix(prefix, &adt->action->curves);
    }
    if (adt->tmpact != NULL) {
      any_removed |= fcurves_path_remove_fix(prefix, &adt->tmpact->curves);
    }
    /* free drivers - stored as a list of F-Curves */
    any_removed |= fcurves_path_remove_fix(prefix, &adt->drivers);
    /* NLA Data - Animation Data for Strips */
    LISTBASE_FOREACH (NlaTrack *, nlt, &adt->nla_tracks) {
      any_removed |= nlastrips_path_remove_fix(prefix, &nlt->strips);
    }
  }
  return any_removed;
}

/* Apply Op to All FCurves in Database --------------------------- */

/* "User-Data" wrapper used by BKE_fcurves_main_cb() */
typedef struct AllFCurvesCbWrapper {
  ID_FCurve_Edit_Callback func; /* Operation to apply on F-Curve */
  void *user_data;              /* Custom data for that operation */
} AllFCurvesCbWrapper;

/* Helper for adt_apply_all_fcurves_cb() - Apply wrapped operator to list of F-Curves */
static void fcurves_apply_cb(ID *id,
                             ListBase *fcurves,
                             ID_FCurve_Edit_Callback func,
                             void *user_data)
{
  FCurve *fcu;

  for (fcu = fcurves->first; fcu; fcu = fcu->next) {
    func(id, fcu, user_data);
  }
}

/* Helper for adt_apply_all_fcurves_cb() - Recursively go through each NLA strip */
static void nlastrips_apply_all_curves_cb(ID *id, ListBase *strips, AllFCurvesCbWrapper *wrapper)
{
  NlaStrip *strip;

  for (strip = strips->first; strip; strip = strip->next) {
    /* fix strip's action */
    if (strip->act) {
      fcurves_apply_cb(id, &strip->act->curves, wrapper->func, wrapper->user_data);
    }

    /* Check sub-strips (if meta-strips). */
    nlastrips_apply_all_curves_cb(id, &strip->strips, wrapper);
  }
}

/* Helper for BKE_fcurves_main_cb() - Dispatch wrapped operator to all F-Curves */
static void adt_apply_all_fcurves_cb(ID *id, AnimData *adt, void *wrapper_data)
{
  AllFCurvesCbWrapper *wrapper = wrapper_data;
  NlaTrack *nlt;

  if (adt->action) {
    fcurves_apply_cb(id, &adt->action->curves, wrapper->func, wrapper->user_data);
  }

  if (adt->tmpact) {
    fcurves_apply_cb(id, &adt->tmpact->curves, wrapper->func, wrapper->user_data);
  }

  /* free drivers - stored as a list of F-Curves */
  fcurves_apply_cb(id, &adt->drivers, wrapper->func, wrapper->user_data);

  /* NLA Data - Animation Data for Strips */
  for (nlt = adt->nla_tracks.first; nlt; nlt = nlt->next) {
    nlastrips_apply_all_curves_cb(id, &nlt->strips, wrapper);
  }
}

void BKE_fcurves_id_cb(ID *id, ID_FCurve_Edit_Callback func, void *user_data)
{
  AnimData *adt = BKE_animdata_from_id(id);
  if (adt != NULL) {
    AllFCurvesCbWrapper wrapper = {func, user_data};
    adt_apply_all_fcurves_cb(id, adt, &wrapper);
  }
}

void BKE_fcurves_main_cb(Main *bmain, ID_FCurve_Edit_Callback func, void *user_data)
{
  /* Wrap F-Curve operation stuff to pass to the general AnimData-level func */
  AllFCurvesCbWrapper wrapper = {func, user_data};

  /* Use the AnimData-based function so that we don't have to reimplement all that stuff */
  BKE_animdata_main_cb(bmain, adt_apply_all_fcurves_cb, &wrapper);
}

/* Whole Database Ops -------------------------------------------- */

void BKE_animdata_main_cb(Main *bmain, ID_AnimData_Edit_Callback func, void *user_data)
{
  ID *id;

  /* standard data version */
#define ANIMDATA_IDS_CB(first) \
  for (id = first; id; id = id->next) { \
    AnimData *adt = BKE_animdata_from_id(id); \
    if (adt) { \
      func(id, adt, user_data); \
    } \
  } \
  (void)0

  /* "embedded" nodetree cases (i.e. scene/material/texture->nodetree) */
#define ANIMDATA_NODETREE_IDS_CB(first, NtId_Type) \
  for (id = first; id; id = id->next) { \
    AnimData *adt = BKE_animdata_from_id(id); \
    NtId_Type *ntp = (NtId_Type *)id; \
    if (ntp->nodetree) { \
      AnimData *adt2 = BKE_animdata_from_id((ID *)ntp->nodetree); \
      if (adt2) { \
        func(id, adt2, user_data); \
      } \
    } \
    if (adt) { \
      func(id, adt, user_data); \
    } \
  } \
  (void)0

  /* nodes */
  ANIMDATA_IDS_CB(bmain->nodetrees.first);

  /* textures */
  ANIMDATA_NODETREE_IDS_CB(bmain->textures.first, Tex);

  /* lights */
  ANIMDATA_NODETREE_IDS_CB(bmain->lights.first, Light);

  /* materials */
  ANIMDATA_NODETREE_IDS_CB(bmain->materials.first, Material);

  /* cameras */
  ANIMDATA_IDS_CB(bmain->cameras.first);

  /* shapekeys */
  ANIMDATA_IDS_CB(bmain->shapekeys.first);

  /* metaballs */
  ANIMDATA_IDS_CB(bmain->metaballs.first);

  /* curves */
  ANIMDATA_IDS_CB(bmain->curves.first);

  /* armatures */
  ANIMDATA_IDS_CB(bmain->armatures.first);

  /* lattices */
  ANIMDATA_IDS_CB(bmain->lattices.first);

  /* meshes */
  ANIMDATA_IDS_CB(bmain->meshes.first);

  /* particles */
  ANIMDATA_IDS_CB(bmain->particles.first);

  /* speakers */
  ANIMDATA_IDS_CB(bmain->speakers.first);

  /* movie clips */
  ANIMDATA_IDS_CB(bmain->movieclips.first);

  /* objects */
  ANIMDATA_IDS_CB(bmain->objects.first);

  /* masks */
  ANIMDATA_IDS_CB(bmain->masks.first);

  /* worlds */
  ANIMDATA_NODETREE_IDS_CB(bmain->worlds.first, World);

  /* scenes */
  ANIMDATA_NODETREE_IDS_CB(bmain->scenes.first, Scene);

  /* line styles */
  ANIMDATA_IDS_CB(bmain->linestyles.first);

  /* grease pencil */
  ANIMDATA_IDS_CB(bmain->gpencils.first);

  /* palettes */
  ANIMDATA_IDS_CB(bmain->palettes.first);

  /* cache files */
  ANIMDATA_IDS_CB(bmain->cachefiles.first);

  /* Hair Curves. */
  ANIMDATA_IDS_CB(bmain->hair_curves.first);

  /* pointclouds */
  ANIMDATA_IDS_CB(bmain->pointclouds.first);

  /* volumes */
  ANIMDATA_IDS_CB(bmain->volumes.first);

  /* simulations */
  ANIMDATA_IDS_CB(bmain->simulations.first);
}

void BKE_animdata_fix_paths_rename_all(ID *ref_id,
                                       const char *prefix,
                                       const char *oldName,
                                       const char *newName)
{
  Main *bmain = G.main; /* XXX UGLY! */
  BKE_animdata_fix_paths_rename_all_ex(bmain, ref_id, prefix, oldName, newName, 0, 0, 1);
}

void BKE_animdata_fix_paths_rename_all_ex(Main *bmain,
                                          ID *ref_id,
                                          const char *prefix,
                                          const char *oldName,
                                          const char *newName,
                                          const int oldSubscript,
                                          const int newSubscript,
                                          const bool verify_paths)
{
  /* TODO: use BKE_animdata_main_cb for looping over all data. */

  ID *id;

  /* macro for less typing
   * - whether animdata exists is checked for by the main renaming callback, though taking
   *   this outside of the function may make things slightly faster?
   */
#define RENAMEFIX_ANIM_IDS(first) \
  for (id = first; id; id = id->next) { \
    AnimData *adt = BKE_animdata_from_id(id); \
    BKE_animdata_fix_paths_rename( \
        id, adt, ref_id, prefix, oldName, newName, oldSubscript, newSubscript, verify_paths); \
  } \
  (void)0

  /* another version of this macro for nodetrees */
#define RENAMEFIX_ANIM_NODETREE_IDS(first, NtId_Type) \
  for (id = first; id; id = id->next) { \
    AnimData *adt = BKE_animdata_from_id(id); \
    NtId_Type *ntp = (NtId_Type *)id; \
    if (ntp->nodetree) { \
      AnimData *adt2 = BKE_animdata_from_id((ID *)ntp->nodetree); \
      BKE_animdata_fix_paths_rename((ID *)ntp->nodetree, \
                                    adt2, \
                                    ref_id, \
                                    prefix, \
                                    oldName, \
                                    newName, \
                                    oldSubscript, \
                                    newSubscript, \
                                    verify_paths); \
    } \
    BKE_animdata_fix_paths_rename( \
        id, adt, ref_id, prefix, oldName, newName, oldSubscript, newSubscript, verify_paths); \
  } \
  (void)0

  /* nodes */
  RENAMEFIX_ANIM_IDS(bmain->nodetrees.first);

  /* textures */
  RENAMEFIX_ANIM_NODETREE_IDS(bmain->textures.first, Tex);

  /* lights */
  RENAMEFIX_ANIM_NODETREE_IDS(bmain->lights.first, Light);

  /* materials */
  RENAMEFIX_ANIM_NODETREE_IDS(bmain->materials.first, Material);

  /* cameras */
  RENAMEFIX_ANIM_IDS(bmain->cameras.first);

  /* shapekeys */
  RENAMEFIX_ANIM_IDS(bmain->shapekeys.first);

  /* metaballs */
  RENAMEFIX_ANIM_IDS(bmain->metaballs.first);

  /* curves */
  RENAMEFIX_ANIM_IDS(bmain->curves.first);

  /* armatures */
  RENAMEFIX_ANIM_IDS(bmain->armatures.first);

  /* lattices */
  RENAMEFIX_ANIM_IDS(bmain->lattices.first);

  /* meshes */
  RENAMEFIX_ANIM_IDS(bmain->meshes.first);

  /* particles */
  RENAMEFIX_ANIM_IDS(bmain->particles.first);

  /* speakers */
  RENAMEFIX_ANIM_IDS(bmain->speakers.first);

  /* movie clips */
  RENAMEFIX_ANIM_IDS(bmain->movieclips.first);

  /* objects */
  RENAMEFIX_ANIM_IDS(bmain->objects.first);

  /* masks */
  RENAMEFIX_ANIM_IDS(bmain->masks.first);

  /* worlds */
  RENAMEFIX_ANIM_NODETREE_IDS(bmain->worlds.first, World);

  /* linestyles */
  RENAMEFIX_ANIM_IDS(bmain->linestyles.first);

  /* grease pencil */
  RENAMEFIX_ANIM_IDS(bmain->gpencils.first);

  /* cache files */
  RENAMEFIX_ANIM_IDS(bmain->cachefiles.first);

  /* Hair Curves. */
  RENAMEFIX_ANIM_IDS(bmain->hair_curves.first);

  /* pointclouds */
  RENAMEFIX_ANIM_IDS(bmain->pointclouds.first);

  /* volumes */
  RENAMEFIX_ANIM_IDS(bmain->volumes.first);

  /* simulations */
  RENAMEFIX_ANIM_IDS(bmain->simulations.first);

  /* scenes */
  RENAMEFIX_ANIM_NODETREE_IDS(bmain->scenes.first, Scene);
}

/* .blend file API -------------------------------------------- */

void BKE_animdata_blend_write(BlendWriter *writer, AnimData *adt)
{
  /* firstly, just write the AnimData block */
  BLO_write_struct(writer, AnimData, adt);

  /* write drivers */
  BKE_fcurve_blend_write(writer, &adt->drivers);

  /* write overrides */
  /* FIXME: are these needed? */
  LISTBASE_FOREACH (AnimOverride *, aor, &adt->overrides) {
    /* overrides consist of base data + rna_path */
    BLO_write_struct(writer, AnimOverride, aor);
    BLO_write_string(writer, aor->rna_path);
  }

  /* TODO: write the remaps (if they are needed). */

  /* write NLA data */
  BKE_nla_blend_write(writer, &adt->nla_tracks);
}

void BKE_animdata_blend_read_data(BlendDataReader *reader, AnimData *adt)
{
  /* NOTE: must have called BLO_read_data_address already before doing this... */
  if (adt == NULL) {
    return;
  }

  /* link drivers */
  BLO_read_list(reader, &adt->drivers);
  BKE_fcurve_blend_read_data(reader, &adt->drivers);
  adt->driver_array = NULL;

  /* link overrides */
  /* TODO... */

  /* link NLA-data */
  BLO_read_list(reader, &adt->nla_tracks);
  BKE_nla_blend_read_data(reader, &adt->nla_tracks);

  /* relink active track/strip - even though strictly speaking this should only be used
   * if we're in 'tweaking mode', we need to be able to have this loaded back for
   * undo, but also since users may not exit tweak-mode before saving (#24535).
   */
  /* TODO: it's not really nice that anyone should be able to save the file in this
   *       state, but it's going to be too hard to enforce this single case. */
  BLO_read_data_address(reader, &adt->act_track);
  BLO_read_data_address(reader, &adt->actstrip);
}

void BKE_animdata_blend_read_lib(BlendLibReader *reader, ID *id, AnimData *adt)
{
  if (adt == NULL) {
    return;
  }

  /* link action data */
  BLO_read_id_address(reader, id, &adt->action);
  BLO_read_id_address(reader, id, &adt->tmpact);

  /* link drivers */
  BKE_fcurve_blend_read_lib(reader, id, &adt->drivers);

  /* overrides don't have lib-link for now, so no need to do anything */

  /* link NLA-data */
  BKE_nla_blend_read_lib(reader, id, &adt->nla_tracks);
}

void BKE_animdata_blend_read_expand(BlendExpander *expander, AnimData *adt)
{
  /* own action */
  BLO_expand(expander, adt->action);
  BLO_expand(expander, adt->tmpact);

  /* drivers - assume that these F-Curves have driver data to be in this list... */
  BKE_fcurve_blend_read_expand(expander, &adt->drivers);

  /* NLA data - referenced actions. */
  BKE_nla_blend_read_expand(expander, &adt->nla_tracks);
}
