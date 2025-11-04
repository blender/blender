/* SPDX-FileCopyrightText: 2009 Blender Authors, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */
#include "MEM_guardedalloc.h"

#include <cstring>
#include <optional>

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_animsys.h"
#include "BKE_context.hh"
#include "BKE_fcurve.hh"
#include "BKE_fcurve_driver.h"
#include "BKE_global.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_main.hh"
#include "BKE_nla.hh"
#include "BKE_node.hh"
#include "BKE_report.hh"

#include "DNA_ID.h"
#include "DNA_anim_types.h"
#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_world_types.h"

#include "BLI_alloca.h"
#include "BLI_dynstr.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "DEG_depsgraph.hh"

#include "BLO_read_write.hh"

#include "RNA_access.hh"
#include "RNA_path.hh"

#include "ANIM_action_iterators.hh"
#include "ANIM_action_legacy.hh"

#include "CLG_log.h"

static CLG_LogRef LOG = {"anim.data"};

using namespace blender;

/* ***************************************** */
/* AnimData API */

/* Getter/Setter -------------------------------------------- */

bool id_type_can_have_animdata(const short id_type)
{
  const IDTypeInfo *typeinfo = BKE_idtype_get_info_from_idcode(id_type);
  if (typeinfo != nullptr) {
    return (typeinfo->flags & IDTYPE_FLAGS_NO_ANIMDATA) == 0;
  }
  return false;
}

bool id_can_have_animdata(const ID *id)
{
  /* sanity check */
  if (id == nullptr) {
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
  return nullptr;
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
    if (iat->adt == nullptr) {
      AnimData *adt;

      /* add animdata */
      adt = iat->adt = MEM_callocN<AnimData>("AnimData");

      /* set default settings */
      adt->act_influence = 1.0f;
    }

    return iat->adt;
  }
  return nullptr;
}

/* Action Setter --------------------------------------- */

bool BKE_animdata_set_action(ReportList *reports, ID *id, bAction *act)
{
  using namespace blender;

  /* If we're unassigning (null action pointer) and there's no animdata, we can
   * skip the whole song and dance of creating animdata just to "unassign" the
   * action from it. */
  if (act == nullptr && BKE_animdata_from_id(id) == nullptr) {
    return true;
  }

  AnimData *adt = BKE_animdata_ensure_id(id);
  if (adt == nullptr) {
    BKE_report(reports, RPT_WARNING, "Attempt to set action on non-animatable ID");
    return false;
  }

  if (!BKE_animdata_action_editable(adt)) {
    /* Cannot remove, otherwise things turn to custard. */
    BKE_report(reports, RPT_ERROR, "Cannot change action, as it is still being edited in NLA");
    return false;
  }

  return animrig::assign_action(act, {*id, *adt});
}

bool BKE_animdata_action_editable(const AnimData *adt)
{
  /* Active action is only editable when it is not a tweaking strip. */
  const bool is_tweaking_strip = (adt->flag & ADT_NLA_EDIT_ON) || adt->actstrip != nullptr ||
                                 adt->tmpact != nullptr;
  return !is_tweaking_strip;
}

bool BKE_animdata_action_ensure_idroot(const ID *owner, bAction *action)
{
  const int idcode = GS(owner->name);

  if (action == nullptr) {
    /* A nullptr action is usable by any ID type. */
    return true;
  }

  if (!blender::animrig::legacy::action_treat_as_legacy(*action)) {
    /* TODO: for layered Actions, this function doesn't make sense. Once all Actions are
     * auto-versioned to layered Actions, this entire function can be removed. */
    action->idroot = 0;
    /* Layered Actions can always be assigned to any ID type. It's the slots
     * that are specialized. */
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
  if (!id_can_have_animdata(id)) {
    return;
  }

  IdAdtTemplate *iat = (IdAdtTemplate *)id;
  AnimData *adt = iat->adt;
  if (!adt) {
    return;
  }

  if (do_id_user) {
    /* The ADT is going to be freed, which means that if it's in tweak mode, it'll have to exit
     * that first. Otherwise we cannot un-assign its Action. */
    BKE_nla_tweakmode_exit({*id, *adt});

    if (adt->action) {
      const bool unassign_ok = blender::animrig::unassign_action(*id);
      BLI_assert_msg(unassign_ok,
                     "Expecting action un-assignment to always work when not in NLA tweak mode");
      UNUSED_VARS_NDEBUG(unassign_ok);
    }
    /* same goes for the temporarily displaced action */
    if (adt->tmpact) {
      /* This should never happen, as we _just_ exited tweak mode. */
      BLI_assert_unreachable();
      const bool unassign_ok = blender::animrig::assign_tmpaction(nullptr, {*id, *adt});
      BLI_assert_msg(unassign_ok, "Expecting tmpaction un-assignment to always work");
      UNUSED_VARS_NDEBUG(unassign_ok);
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
  iat->adt = nullptr;
}

bool BKE_animdata_id_is_animated(const ID *id)
{
  if (id == nullptr) {
    return false;
  }

  const AnimData *adt = BKE_animdata_from_id((ID *)id);
  if (adt == nullptr) {
    return false;
  }

  if (adt->action) {
    const blender::animrig::Action &action = adt->action->wrap();
    if (action.is_action_layered() && action.is_slot_animated(adt->slot_handle)) {
      return true;
    }
    if (action.is_action_legacy() && !BLI_listbase_is_empty(&action.curves)) {
      return true;
    }
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

AnimData *BKE_animdata_copy_in_lib(Main *bmain,
                                   std::optional<Library *> owner_library,
                                   AnimData *adt,
                                   const int flag)
{
  AnimData *dadt;

  const bool do_action = (flag & LIB_ID_COPY_ACTIONS) != 0 && (flag & LIB_ID_CREATE_NO_MAIN) == 0;
  const bool do_id_user = (flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0;

  /* sanity check before duplicating struct */
  if (adt == nullptr) {
    return nullptr;
  }
  dadt = static_cast<AnimData *>(MEM_dupallocN(adt));

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
    BLI_assert(bmain != nullptr);
    BLI_assert(dadt->action == nullptr || dadt->action != dadt->tmpact);
    dadt->action = reinterpret_cast<bAction *>(
        BKE_id_copy_in_lib(bmain,
                           owner_library,
                           reinterpret_cast<ID *>(dadt->action),
                           std::nullopt,
                           nullptr,
                           id_copy_flag));
    dadt->tmpact = reinterpret_cast<bAction *>(
        BKE_id_copy_in_lib(bmain,
                           owner_library,
                           reinterpret_cast<ID *>(dadt->tmpact),
                           std::nullopt,
                           nullptr,
                           id_copy_flag));
  }
  else if (do_id_user) {
    id_us_plus((ID *)dadt->action);
    id_us_plus((ID *)dadt->tmpact);
  }

  /* duplicate NLA data */
  BKE_nla_tracks_copy_from_adt(bmain, dadt, adt, flag);

  /* duplicate drivers (F-Curves) */
  BKE_fcurves_copy(&dadt->drivers, &adt->drivers);
  dadt->driver_array = nullptr;

  /* don't copy overrides */
  BLI_listbase_clear(&dadt->overrides);

  const bool is_main = (flag & LIB_ID_CREATE_NO_MAIN) == 0;
  if (is_main) {
    /* Action references were changed, so the Slot-to-user map is incomplete now. Only necessary
     * when this happens in the main database though, as the user cache only tracks original IDs,
     * not evaluated copies.
     *
     * This function does not have access to the animated ID, so it cannot just add that ID to the
     * slot's users, hence the invalidation of the users map.
     *
     * TODO: refactor to pass the owner ID to this function, and just add it to the Slot's
     * users. */
    if (bmain) {
      blender::animrig::Slot::users_invalidate(*bmain);
    }
  }

  /* return */
  return dadt;
}

AnimData *BKE_animdata_copy(Main *bmain, AnimData *adt, const int flag)
{
  return BKE_animdata_copy_in_lib(bmain, std::nullopt, adt, flag);
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
  using namespace blender::animrig;

  AnimData *adt = BKE_animdata_from_id(id);
  if (adt) {
    if (adt->action && (do_linked_id || !ID_IS_LINKED(adt->action))) {
      bAction *cloned_action = reinterpret_cast<bAction *>(BKE_id_copy(bmain, &adt->action->id));
      if (set_newid) {
        ID_NEW_SET(adt->action, cloned_action);
      }

      /* The Action was cloned, so this should find the same-named slot automatically. */
      const slot_handle_t orig_slot_handle = adt->slot_handle;
      const bool assign_ok = assign_action(&cloned_action->wrap(), *id);
      BLI_assert_msg(assign_ok, "Expected action assignment to work when copying animdata");
      BLI_assert(orig_slot_handle == adt->slot_handle);
      UNUSED_VARS_NDEBUG(assign_ok, orig_slot_handle);
    }
    if (adt->tmpact && (do_linked_id || !ID_IS_LINKED(adt->tmpact))) {
      bAction *cloned_action = reinterpret_cast<bAction *>(BKE_id_copy(bmain, &adt->tmpact->id));
      if (set_newid) {
        ID_NEW_SET(adt->tmpact, cloned_action);
      }

      /* The Action was cloned, so this should find the same-named slot automatically. */
      const slot_handle_t orig_slot_handle = adt->tmp_slot_handle;
      const bool assign_ok = assign_tmpaction(&cloned_action->wrap(), {*id, *adt});
      BLI_assert_msg(assign_ok, "Expected tmp-action assignment to work when copying animdata");
      BLI_assert(orig_slot_handle == adt->tmp_slot_handle);
      UNUSED_VARS_NDEBUG(assign_ok, orig_slot_handle);
    }
  }
  bNodeTree *ntree = blender::bke::node_tree_from_id(id);
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
  if (ELEM(nullptr, dst, src)) {
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
  dst->slot_handle = src->slot_handle;
  dst->tmp_slot_handle = src->tmp_slot_handle;
  STRNCPY_UTF8(dst->last_slot_identifier, src->last_slot_identifier);
  STRNCPY_UTF8(dst->tmp_last_slot_identifier, src->tmp_last_slot_identifier);

  /* duplicate NLA data */
  if (src->nla_tracks.first) {
    ListBase tracks = {nullptr, nullptr};

    BKE_nla_tracks_copy(bmain, &tracks, &src->nla_tracks, 0);
    BLI_movelisttolist(&dst->nla_tracks, &tracks);
  }

  /* duplicate drivers (F-Curves) */
  if (src->drivers.first) {
    ListBase drivers = {nullptr, nullptr};

    BKE_fcurves_copy(&drivers, &src->drivers);

    /* Fix up all driver targets using the old target id
     * - This assumes that the src ID is being merged into the dst ID
     */
    if (fix_drivers) {
      LISTBASE_FOREACH (FCurve *, fcu, &drivers) {
        ChannelDriver *driver = fcu->driver;
        LISTBASE_FOREACH (DriverVar *, dvar, &driver->variables) {
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
                                            const animrig::slot_handle_t src_slot_handle,
                                            bAction *dstAct,
                                            const animrig::slot_handle_t dst_slot_handle,
                                            const char *src_basepath,
                                            const char *dst_basepath)
{
  /* sanity checks */
  if (ELEM(nullptr, srcAct, dstAct, src_basepath, dst_basepath)) {
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

  animrig::Action &source_action = srcAct->wrap();
  animrig::Action &dest_action = dstAct->wrap();

  /* Get a list of all F-Curves to move. This is done in a separate step so we
   * don't move the curves while iterating over them at the same time. */
  Vector<FCurve *> fcurves_to_move;
  animrig::foreach_fcurve_in_action_slot(source_action, src_slot_handle, [&](FCurve &fcurve) {
    if (animpath_matches_basepath(fcurve.rna_path, src_basepath)) {
      fcurves_to_move.append(&fcurve);
    }
  });

  /* Move the curves from one Action to the other, and change its path to match the destination. */
  for (FCurve *fcurve_to_move : fcurves_to_move) {
    animpath_update_basepath(fcurve_to_move, src_basepath, dst_basepath);
    animrig::action_fcurve_move(dest_action, dst_slot_handle, source_action, *fcurve_to_move);
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
  AnimData *srcAdt = nullptr, *dstAdt = nullptr;

  /* sanity checks */
  if (ELEM(nullptr, srcID, dstID)) {
    if (G.debug & G_DEBUG) {
      CLOG_ERROR(&LOG, "no source or destination ID to separate AnimData with");
    }
    return;
  }

  /* get animdata from src, and create for destination (if needed) */
  srcAdt = BKE_animdata_from_id(srcID);
  dstAdt = BKE_animdata_ensure_id(dstID);

  if (ELEM(nullptr, srcAdt, dstAdt)) {
    if (G.debug & G_DEBUG) {
      CLOG_ERROR(&LOG, "no AnimData for this pair of ID's");
    }
    return;
  }

  /* active action */
  if (srcAdt->action) {
    const OwnedAnimData dst_owned_adt = {*dstID, *dstAdt};
    if (dstAdt->action == srcAdt->action) {
      CLOG_WARN(&LOG,
                "Source and Destination share animation! "
                "('%s' and '%s' both use '%s') Making new empty action",
                srcID->name,
                dstID->name,
                srcAdt->action->id.name);

      /* This sets dstAdt->action to nullptr. */
      const bool unassign_ok = animrig::unassign_action(dst_owned_adt);
      BLI_assert_msg(unassign_ok, "Expected Action unassignment to work");
      UNUSED_VARS_NDEBUG(unassign_ok);
    }

    /* Set up an action if necessary, and name it in a similar way so that it
     * can be easily found again. */
    if (!dstAdt->action) {
      animrig::Action &new_action = animrig::action_add(*bmain, srcAdt->action->id.name + 2);
      new_action.slot_add_for_id(*dstID);

      const bool assign_ok = animrig::assign_action(&new_action, dst_owned_adt);
      BLI_assert_msg(assign_ok, "Expected Action assignment to work");
      UNUSED_VARS_NDEBUG(assign_ok);
      BLI_assert(dstAdt->slot_handle != animrig::Slot::unassigned);
    }

    /* loop over base paths, trying to fix for each one... */
    LISTBASE_FOREACH (const AnimationBasePathChange *, basepath_change, basepaths) {
      action_move_fcurves_by_basepath(srcAdt->action,
                                      srcAdt->slot_handle,
                                      dstAdt->action,
                                      dstAdt->slot_handle,
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
  DEG_id_tag_update(&srcAdt->action->id, ID_RECALC_SYNC_TO_EVAL);
}

/* Path Validation -------------------------------------------- */

/* Check if a given RNA Path is valid, by tracing it from the given ID,
 * and seeing if we can resolve it. */
static bool check_rna_path_is_valid(ID *owner_id, const char *path)
{
  PointerRNA ptr;
  PropertyRNA *prop = nullptr;

  /* make initial RNA pointer to start resolving from */
  PointerRNA id_ptr = RNA_id_pointer_create(owner_id);

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
  if (prefixPtr == nullptr) {
    return oldpath;
  }

  char *oldNamePtr = strstr(oldpath, oldName);
  if (oldNamePtr == nullptr) {
    return oldpath;
  }

  int prefixLen = strlen(prefix);
  int oldNameLen = strlen(oldName);

  /* only start fixing the path if the prefix and oldName feature in the path,
   * and prefix occurs immediately before oldName
   */
  if (prefixPtr + prefixLen == oldNamePtr) {
    /* if we haven't aren't able to resolve the path now, try again after fixing it */
    if (!verify_paths || check_rna_path_is_valid(owner_id, oldpath) == 0) {
      DynStr *ds = BLI_dynstr_new();
      const char *postfixPtr = oldNamePtr + oldNameLen;
      char *newPath = nullptr;

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
                                    blender::Span<FCurve *> curves,
                                    bool verify_paths)
{
  bool is_changed = false;
  /* We need to check every curve. */
  for (FCurve *fcu : curves) {
    if (fcu->rna_path == nullptr) {
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
      if (oldName != nullptr && (agrp != nullptr) && STREQ(oldName, agrp->name)) {
        STRNCPY_UTF8(agrp->name, newName);
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
  /* We need to check every curve - drivers are F-Curves too. */
  LISTBASE_FOREACH (FCurve *, fcu, curves) {
    /* firstly, handle the F-Curve's own path */
    if (fcu->rna_path != nullptr) {
      const char *old_rna_path = fcu->rna_path;
      fcu->rna_path = rna_path_rename_fix(
          owner_id, prefix, oldKey, newKey, fcu->rna_path, verify_paths);
      is_changed |= (fcu->rna_path != old_rna_path);
    }
    if (fcu->driver == nullptr) {
      continue;
    }
    ChannelDriver *driver = fcu->driver;
    /* driver variables */
    LISTBASE_FOREACH (DriverVar *, dvar, &driver->variables) {
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
  bool is_changed = false;
  /* Recursively check strips, fixing only actions. */
  LISTBASE_FOREACH (NlaStrip *, strip, strips) {
    /* fix strip's action */
    if (strip->act != nullptr) {
      const Vector<FCurve *> fcurves = blender::animrig::legacy::fcurves_for_action_slot(
          strip->act, strip->action_slot_handle);
      const bool is_changed_action = fcurves_path_rename_fix(
          owner_id, prefix, oldName, newName, oldKey, newKey, fcurves, verify_paths);
      if (is_changed_action) {
        DEG_id_tag_update(&strip->act->id, ID_RECALC_ANIMATION);
      }
      is_changed |= is_changed_action;
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
  if (ELEM(nullptr, owner_id, old_path)) {
    if (G.debug & G_DEBUG) {
      CLOG_WARN(&LOG, "early abort");
    }
    return old_path;
  }

  /* Name sanitation logic - copied from BKE_animdata_fix_paths_rename() */
  if ((oldName != nullptr) && (newName != nullptr)) {
    /* pad the names with [" "] so that only exact matches are made */
    const size_t name_old_len = strlen(oldName);
    const size_t name_new_len = strlen(newName);
    char *name_old_esc = static_cast<char *>(
        BLI_array_alloca(name_old_esc, (name_old_len * 2) + 1));
    char *name_new_esc = static_cast<char *>(
        BLI_array_alloca(name_new_esc, (name_new_len * 2) + 1));

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
                                 animrig::slot_handle_t slot_handle,
                                 const char *prefix,
                                 const char *oldName,
                                 const char *newName,
                                 int oldSubscript,
                                 int newSubscript,
                                 bool verify_paths)
{
  char *oldN, *newN;

  /* if no action, no need to proceed */
  if (ELEM(nullptr, owner_id, act)) {
    return;
  }

  /* Name sanitation logic - copied from BKE_animdata_fix_paths_rename() */
  if ((oldName != nullptr) && (newName != nullptr)) {
    /* pad the names with [" "] so that only exact matches are made */
    const size_t name_old_len = strlen(oldName);
    const size_t name_new_len = strlen(newName);
    char *name_old_esc = static_cast<char *>(
        BLI_array_alloca(name_old_esc, (name_old_len * 2) + 1));
    char *name_new_esc = static_cast<char *>(
        BLI_array_alloca(name_new_esc, (name_new_len * 2) + 1));

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
  fcurves_path_rename_fix(owner_id,
                          prefix,
                          oldName,
                          newName,
                          oldN,
                          newN,
                          blender::animrig::legacy::fcurves_for_action_slot(act, slot_handle),
                          verify_paths);

  /* free the temp names */
  MEM_freeN(oldN);
  MEM_freeN(newN);

  DEG_id_tag_update(&act->id, ID_RECALC_ANIMATION);
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
  char *oldN, *newN;
  /* If no AnimData, no need to proceed. */
  if (ELEM(nullptr, owner_id, adt)) {
    return;
  }
  bool is_self_changed = false;
  /* Name sanitation logic - shared with BKE_action_fix_paths_rename(). */
  if ((oldName != nullptr) && (newName != nullptr)) {
    /* Pad the names with [" "] so that only exact matches are made. */
    const size_t name_old_len = strlen(oldName);
    const size_t name_new_len = strlen(newName);
    char *name_old_esc = static_cast<char *>(
        BLI_array_alloca(name_old_esc, (name_old_len * 2) + 1));
    char *name_new_esc = static_cast<char *>(
        BLI_array_alloca(name_new_esc, (name_new_len * 2) + 1));

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
  if (adt->action != nullptr && adt->slot_handle != blender::animrig::Slot::unassigned) {
    const Vector<FCurve *> fcurves = blender::animrig::legacy::fcurves_for_action_slot(
        adt->action, adt->slot_handle);
    if (fcurves_path_rename_fix(
            owner_id, prefix, oldName, newName, oldN, newN, fcurves, verify_paths))
    {
      DEG_id_tag_update(&adt->action->id, ID_RECALC_SYNC_TO_EVAL);
    }
  }
  if (adt->tmpact) {
    const Vector<FCurve *> fcurves = blender::animrig::legacy::fcurves_for_action_slot(
        adt->tmpact, adt->tmp_slot_handle);
    if (fcurves_path_rename_fix(
            owner_id, prefix, oldName, newName, oldN, newN, fcurves, verify_paths))
    {
      DEG_id_tag_update(&adt->tmpact->id, ID_RECALC_SYNC_TO_EVAL);
    }
  }
  /* Drivers - Drivers are really F-Curves */
  is_self_changed |= drivers_path_rename_fix(
      owner_id, ref_id, prefix, oldName, newName, oldN, newN, &adt->drivers, verify_paths);
  /* NLA Data - Animation Data for Strips */
  LISTBASE_FOREACH (NlaTrack *, nlt, &adt->nla_tracks) {
    is_self_changed |= nlastrips_path_rename_fix(
        owner_id, prefix, oldName, newName, oldN, newN, &nlt->strips, verify_paths);
  }
  /* Tag owner ID if it */
  if (is_self_changed) {
    DEG_id_tag_update(owner_id, ID_RECALC_SYNC_TO_EVAL);
  }
  /* free the temp names */
  MEM_freeN(oldN);
  MEM_freeN(newN);
}

/* Remove FCurves with Prefix  -------------------------------------- */

/** Remove F-Curves from the listbase when their RNA path starts with `prefix`. */
static bool fcurves_path_remove_from_listbase(const char *prefix, ListBase *curves)
{
  FCurve *fcu, *fcn;
  bool any_removed = false;
  if (!prefix) {
    return any_removed;
  }

  /* we need to check every curve... */
  for (fcu = static_cast<FCurve *>(curves->first); fcu; fcu = fcn) {
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
  bool any_removed = false;

  /* recursively check strips, fixing only actions... */
  LISTBASE_FOREACH (NlaStrip *, strip, strips) {
    /* fix strip's action */
    if (strip->act) {
      any_removed |= animrig::legacy::action_fcurves_remove(
          *strip->act, strip->action_slot_handle, prefix);
    }

    /* Check sub-strips (if meta-strips). */
    any_removed |= nlastrips_path_remove_fix(prefix, &strip->strips);
  }

  return any_removed;
}

bool BKE_animdata_fix_paths_remove(ID *id, const char *prefix)
{
  AnimData *adt = BKE_animdata_from_id(id);
  if (!adt) {
    return false;
  }

  bool any_removed = false;

  /* Actions. */
  if (adt->action) {
    any_removed |= animrig::legacy::action_fcurves_remove(*adt->action, adt->slot_handle, prefix);
  }
  if (adt->tmpact) {
    any_removed |= animrig::legacy::action_fcurves_remove(
        *adt->action, adt->tmp_slot_handle, prefix);
  }

  /* Drivers. */
  any_removed |= fcurves_path_remove_from_listbase(prefix, &adt->drivers);

  /* NLA strips. */
  LISTBASE_FOREACH (NlaTrack *, nlt, &adt->nla_tracks) {
    any_removed |= nlastrips_path_remove_fix(prefix, &nlt->strips);
  }

  return any_removed;
}

bool BKE_animdata_driver_path_remove(ID *id, const char *prefix)
{
  AnimData *adt = BKE_animdata_from_id(id);
  if (!adt) {
    return false;
  }

  const bool any_removed = fcurves_path_remove_from_listbase(prefix, &adt->drivers);
  return any_removed;
}

bool BKE_animdata_drivers_remove_for_rna_struct(ID &owner_id, StructRNA &type, void *data)
{
  PointerRNA constraint_ptr = RNA_pointer_create_discrete(&owner_id, &type, data);
  const std::optional<std::string> base_path = RNA_path_from_ID_to_struct(&constraint_ptr);
  if (!base_path.has_value()) {
    /* The data should exist, so the path should always resolve. */
    BLI_assert_unreachable();
  }

  return BKE_animdata_driver_path_remove(&owner_id, base_path.value().c_str());
}

/* Apply Op to All FCurves in Database --------------------------- */

/**
 * Callback function for ID & F-Curve reporting when looping over all F-Curves of an ID.
 *
 * \returns whether looping should continue (true = keep going, false = stop).
 */
using IDFCurveCallback = FunctionRef<bool(ID *, FCurve *)>;

/* Helper for adt_apply_all_fcurves_cb() - Apply wrapped operator to list of F-Curves */
static bool fcurves_apply_cb(ID *id, blender::Span<FCurve *> fcurves, const IDFCurveCallback func)
{
  for (FCurve *fcu : fcurves) {
    if (!func(id, fcu)) {
      return false;
    }
  }
  return true;
}
static bool fcurves_listbase_apply_cb(ID *id, ListBase *fcurves, const IDFCurveCallback func)
{
  LISTBASE_FOREACH (FCurve *, fcu, fcurves) {
    if (!func(id, fcu)) {
      return false;
    }
  }
  return true;
}

/* Helper for adt_apply_all_fcurves_cb() - Recursively go through each NLA strip */
static bool nlastrips_apply_all_curves_cb(ID *id, ListBase *strips, const IDFCurveCallback func)
{
  /* This function is used (via `BKE_fcurves_id_cb()`) by the versioning system.
   * As such, legacy Actions should always be expected here. */

  LISTBASE_FOREACH (NlaStrip *, strip, strips) {
    if (strip->act) {
      const Vector<FCurve *> fcurves = blender::animrig::legacy::fcurves_for_action_slot(
          strip->act, strip->action_slot_handle);
      if (!fcurves_apply_cb(id, fcurves, func)) {
        return false;
      }
    }

    /* Check sub-strips (if meta-strips). */
    if (!nlastrips_apply_all_curves_cb(id, &strip->strips, func)) {
      return false;
    }
  }
  return true;
}

/**
 * Call the callback function for all F-Curves on the ID. Muted NLA Tracks are
 * ignored, drivers and Actions on NLA strips are included.
 *
 * \returns whether the loop was completed to the end, so false if any call of the callback
 * returned false.
 */
static bool adt_apply_all_fcurves_cb(ID *id, AnimData *adt, const IDFCurveCallback func)
{
  /* This function is used (via `BKE_fcurves_id_cb()`) by the versioning system.
   * As such, legacy Actions should always be expected here. */

  if (adt->action) {
    if (!fcurves_apply_cb(
            id,
            blender::animrig::legacy::fcurves_for_action_slot(adt->action, adt->slot_handle),
            func))
    {
      return false;
    }
  }

  if (adt->tmpact) {
    if (!fcurves_apply_cb(
            id,
            blender::animrig::legacy::fcurves_for_action_slot(adt->tmpact, adt->tmp_slot_handle),
            func))
    {
      return false;
    }
  }

  /* Drivers, stored as a list of F-Curves. */
  if (!fcurves_listbase_apply_cb(id, &adt->drivers, func)) {
    return false;
  }

  /* NLA Data - Animation Data for Strips */
  LISTBASE_FOREACH (NlaTrack *, nlt, &adt->nla_tracks) {
    if (!BKE_nlatrack_is_enabled(*adt, *nlt)) {
      continue;
    }
    if (!nlastrips_apply_all_curves_cb(id, &nlt->strips, func)) {
      return false;
    }
  }
  return true;
}

void BKE_fcurves_id_cb(ID *id, const FunctionRef<void(ID *, FCurve *)> func)
{
  AnimData *adt = BKE_animdata_from_id(id);
  if (adt != nullptr) {
    /* Use a little wrapper function to always return 'true' and thus keep the loop looping. */
    const auto wrapper = [&func](ID *id, FCurve *fcurve) {
      func(id, fcurve);
      return true;
    };
    adt_apply_all_fcurves_cb(id, adt, wrapper);
  }
}

void BKE_fcurves_main_cb(Main *bmain, const FunctionRef<void(ID *, FCurve *)> func)
{
  /* Use a little wrapper function to always return 'true' and thus keep the loop looping. */
  const auto wrapper = [&func](ID *id, FCurve *fcurve) {
    func(id, fcurve);
    return true;
  };

  /* Use the AnimData-based function so that we don't have to reimplement all that stuff */
  BKE_animdata_main_cb(bmain,
                       [&](ID *id, AnimData *adt) { adt_apply_all_fcurves_cb(id, adt, wrapper); });
}

/* .blend file API -------------------------------------------- */

void BKE_animdata_blend_write(BlendWriter *writer, ID *id)
{
  AnimData *adt = BKE_animdata_from_id(id);
  if (!adt) {
    return;
  }

  /* firstly, just write the AnimData block */
  BLO_write_struct(writer, AnimData, adt);

  /* write drivers */
  BKE_fcurve_blend_write_listbase(writer, &adt->drivers);

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

void BKE_animdata_blend_read_data(BlendDataReader *reader, ID *id)
{
  IdAdtTemplate *iat = id_can_have_animdata(id) ? reinterpret_cast<IdAdtTemplate *>(id) : nullptr;
  if (!iat || !iat->adt) {
    return;
  }

  AnimData *adt = static_cast<AnimData *>(BLO_read_struct(reader, AnimData, &iat->adt));
  if (adt == nullptr) {
    return;
  }

  /* link drivers */
  BLO_read_struct_list(reader, FCurve, &adt->drivers);
  BKE_fcurve_blend_read_data_listbase(reader, &adt->drivers);
  adt->driver_array = nullptr;

  /* link overrides */
  /* TODO... */

  /* link NLA-data */
  BLO_read_struct_list(reader, NlaTrack, &adt->nla_tracks);
  BKE_nla_blend_read_data(reader, id, &adt->nla_tracks);

  /* relink active track/strip - even though strictly speaking this should only be used
   * if we're in 'tweaking mode', we need to be able to have this loaded back for
   * undo, but also since users may not exit tweak-mode before saving (#24535).
   */
  /* TODO: it's not really nice that anyone should be able to save the file in this
   *       state, but it's going to be too hard to enforce this single case. */
  BLO_read_struct(reader, NlaTrack, &adt->act_track);
  BLO_read_struct(reader, NlaStrip, &adt->actstrip);

  if (ID_IS_LINKED(id)) {
    /* Linked NLAs should never be in tweak mode, as you cannot exit that on linked data. */
    BKE_nla_tweakmode_exit_nofollowptr(adt);
  }
}

void BKE_animdata_liboverride_post_process(ID *id)
{
  AnimData *adt = BKE_animdata_from_id(id);
  if (!adt) {
    return;
  }

  BKE_nla_liboverride_post_process(id, adt);
}

namespace blender::bke::animdata {

void action_slots_user_cache_invalidate(Main &bmain)
{
  blender::animrig::Slot::users_invalidate(bmain);
}

bool prop_is_animated(const AnimData *adt, const StringRefNull rna_path, const int array_index)
{
  if (!adt) {
    /* If there is no animdata, it's clear the property is not animated. */
    return false;
  }

  /* The const_cast is used because adt_apply_all_fcurves_cb() wants to yield a
   * mutable F-Curve and thus gets a mutable AnimData. The function itself is
   * not modifying anything, so this case should be safe. */
  const bool looped_until_end = adt_apply_all_fcurves_cb(
      nullptr, const_cast<AnimData *>(adt), [&](const ID *, const FCurve *fcurve) {
        /* Looping should stop (so return false) when the F-Curve was found. */
        return !(array_index == fcurve->array_index && rna_path == fcurve->rna_path);
      });

  return !looped_until_end;
}

}  // namespace blender::bke::animdata
