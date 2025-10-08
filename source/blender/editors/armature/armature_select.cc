/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edarmature
 * API's and Operators for selecting armature bones in EditMode.
 */

#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"
#include "BLI_string_utils.hh"

#include "BKE_action.hh"
#include "BKE_armature.hh"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"
#include "BKE_report.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_armature.hh"
#include "ED_object.hh"
#include "ED_outliner.hh"
#include "ED_screen.hh"
#include "ED_select_utils.hh"
#include "ED_view3d.hh"

#include "DEG_depsgraph.hh"

#include "GPU_select.hh"

#include "ANIM_armature.hh"
#include "ANIM_bone_collections.hh"
#include "ANIM_bonecolor.hh"

#include "armature_intern.hh"

using blender::Span;
using blender::Vector;

/* utility macros for storing a temp int in the bone (selection flag) */
#define EBONE_PREV_FLAG_GET(ebone) ((void)0, (ebone)->temp.i)
#define EBONE_PREV_FLAG_SET(ebone, val) ((ebone)->temp.i = val)

/* -------------------------------------------------------------------- */
/** \name Select Buffer Queries for PoseMode & EditMode
 * \{ */

Base *ED_armature_base_and_ebone_from_select_buffer(const Span<Base *> bases,
                                                    const uint select_id,
                                                    EditBone **r_ebone)
{
  const uint hit_object = select_id & 0xFFFF;
  Base *base = nullptr;
  EditBone *ebone = nullptr;
  /* TODO(@ideasman42): optimize, eg: sort & binary search. */
  for (Base *base_iter : bases) {
    if (base_iter->object->runtime->select_id == hit_object) {
      base = base_iter;
      break;
    }
  }
  if (base != nullptr) {
    const uint hit_bone = (select_id & ~BONESEL_ANY) >> 16;
    bArmature *arm = static_cast<bArmature *>(base->object->data);
    ebone = static_cast<EditBone *>(BLI_findlink(arm->edbo, hit_bone));
  }
  *r_ebone = ebone;
  return base;
}

Object *ED_armature_object_and_ebone_from_select_buffer(const Span<Object *> objects,
                                                        const uint select_id,
                                                        EditBone **r_ebone)
{
  const uint hit_object = select_id & 0xFFFF;
  Object *ob = nullptr;
  EditBone *ebone = nullptr;
  /* TODO(@ideasman42): optimize, eg: sort & binary search. */
  for (Object *object_iter : objects) {
    if (object_iter->runtime->select_id == hit_object) {
      ob = object_iter;
      break;
    }
  }
  if (ob != nullptr) {
    const uint hit_bone = (select_id & ~BONESEL_ANY) >> 16;
    bArmature *arm = static_cast<bArmature *>(ob->data);
    ebone = static_cast<EditBone *>(BLI_findlink(arm->edbo, hit_bone));
  }
  *r_ebone = ebone;
  return ob;
}

Base *ED_armature_base_and_pchan_from_select_buffer(const Span<Base *> bases,
                                                    const uint select_id,
                                                    bPoseChannel **r_pchan)
{
  const uint hit_object = select_id & 0xFFFF;
  Base *base = nullptr;
  bPoseChannel *pchan = nullptr;
  /* TODO(@ideasman42): optimize, eg: sort & binary search. */
  for (Base *base_iter : bases) {
    if (base_iter->object->runtime->select_id == hit_object) {
      base = base_iter;
      break;
    }
  }
  if (base != nullptr) {
    if (base->object->pose != nullptr) {
      const uint hit_bone = (select_id & ~BONESEL_ANY) >> 16;
      /* pchan may be nullptr. */
      pchan = static_cast<bPoseChannel *>(BLI_findlink(&base->object->pose->chanbase, hit_bone));
    }
  }
  *r_pchan = pchan;
  return base;
}

Base *ED_armature_base_and_bone_from_select_buffer(const Span<Base *> bases,
                                                   const uint select_id,
                                                   Bone **r_bone)
{
  bPoseChannel *pchan = nullptr;
  Base *base = ED_armature_base_and_pchan_from_select_buffer(bases, select_id, &pchan);
  *r_bone = pchan ? pchan->bone : nullptr;
  return base;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cursor Pick from Select Buffer API
 *
 * Internal #ed_armature_pick_bone_from_selectbuffer_impl is exposed as:
 * - #ED_armature_pick_ebone_from_selectbuffer
 * - #ED_armature_pick_pchan_from_selectbuffer
 * - #ED_armature_pick_bone_from_selectbuffer
 * \{ */

/* See if there are any selected bones in this buffer */
/* only bones from base are checked on */
static void *ed_armature_pick_bone_from_selectbuffer_impl(const bool is_editmode,
                                                          const Span<Base *> bases,
                                                          const Span<GPUSelectResult> hit_results,
                                                          bool findunsel,
                                                          bool do_nearest,
                                                          Base **r_base)
{
  bPoseChannel *pchan;
  EditBone *ebone;
  void *firstunSel = nullptr, *firstSel = nullptr, *data;
  Base *firstunSel_base = nullptr, *firstSel_base = nullptr;
  bool takeNext = false;
  int minsel = 0xffffffff, minunsel = 0xffffffff;

  for (const GPUSelectResult &hit_result : hit_results) {
    uint hit_id = hit_result.id;

    if (hit_id & BONESEL_ANY) { /* to avoid including objects in selection */
      Base *base = nullptr;
      bool sel;

      hit_id &= ~BONESEL_ANY;
      /* Determine what the current bone is */
      if (is_editmode == false) {
        base = ED_armature_base_and_pchan_from_select_buffer(bases, hit_id, &pchan);
        if (pchan != nullptr) {
          if (pchan->bone->flag & BONE_UNSELECTABLE) {
            continue;
          }
          if (findunsel) {
            sel = (pchan->flag & POSE_SELECTED);
          }
          else {
            sel = !(pchan->flag & POSE_SELECTED);
          }

          data = pchan;
        }
        else {
          data = nullptr;
          sel = false;
        }
      }
      else {
        base = ED_armature_base_and_ebone_from_select_buffer(bases, hit_id, &ebone);
        if (ebone->flag & BONE_UNSELECTABLE) {
          continue;
        }
        if (findunsel) {
          sel = (ebone->flag & BONE_SELECTED);
        }
        else {
          sel = !(ebone->flag & BONE_SELECTED);
        }

        data = ebone;
      }

      if (data) {
        if (sel) {
          if (do_nearest) {
            if (minsel > hit_result.depth) {
              firstSel = data;
              firstSel_base = base;
              minsel = hit_result.depth;
            }
          }
          else {
            if (!firstSel) {
              firstSel = data;
              firstSel_base = base;
            }
            takeNext = true;
          }
        }
        else {
          if (do_nearest) {
            if (minunsel > hit_result.depth) {
              firstunSel = data;
              firstunSel_base = base;
              minunsel = hit_result.depth;
            }
          }
          else {
            if (!firstunSel) {
              firstunSel = data;
              firstunSel_base = base;
            }
            if (takeNext) {
              *r_base = base;
              return data;
            }
          }
        }
      }
    }
  }

  if (firstunSel) {
    *r_base = firstunSel_base;
    return firstunSel;
  }
  *r_base = firstSel_base;
  return firstSel;
}

EditBone *ED_armature_pick_ebone_from_selectbuffer(const Span<Base *> bases,
                                                   const GPUSelectResult *hit_results,
                                                   const int hits,
                                                   bool findunsel,
                                                   bool do_nearest,
                                                   Base **r_base)
{
  const bool is_editmode = true;
  return static_cast<EditBone *>(ed_armature_pick_bone_from_selectbuffer_impl(
      is_editmode, bases, {hit_results, hits}, findunsel, do_nearest, r_base));
}

bPoseChannel *ED_armature_pick_pchan_from_selectbuffer(const Span<Base *> bases,
                                                       const GPUSelectResult *hit_results,
                                                       const int hits,
                                                       bool findunsel,
                                                       bool do_nearest,
                                                       Base **r_base)
{
  const bool is_editmode = false;
  return static_cast<bPoseChannel *>(ed_armature_pick_bone_from_selectbuffer_impl(
      is_editmode, bases, {hit_results, hits}, findunsel, do_nearest, r_base));
}

Bone *ED_armature_pick_bone_from_selectbuffer(const Span<Base *> bases,
                                              const GPUSelectResult *hit_results,
                                              const int hits,
                                              bool findunsel,
                                              bool do_nearest,
                                              Base **r_base)
{
  bPoseChannel *pchan = ED_armature_pick_pchan_from_selectbuffer(
      bases, hit_results, hits, findunsel, do_nearest, r_base);
  return pchan ? pchan->bone : nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cursor Pick API
 *
 * Internal #ed_armature_pick_bone_impl is exposed as:
 * - #ED_armature_pick_ebone
 * - #ED_armature_pick_pchan
 * - #ED_armature_pick_bone
 * \{ */

/**
 * \param xy: Cursor coordinates (area space).
 * \return An #EditBone when is_editmode, otherwise a #bPoseChannel.
 * \note Only checks objects in the current mode (edit-mode or pose-mode).
 */
static void *ed_armature_pick_bone_impl(
    const bool is_editmode, bContext *C, const int xy[2], bool findunsel, Base **r_base)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  rcti rect;
  GPUSelectBuffer buffer;
  int hits;

  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);
  BLI_assert((vc.obedit != nullptr) == is_editmode);

  BLI_rcti_init_pt_radius(&rect, xy, 0);

  /* Don't use hits with this ID, (armature drawing uses this). */
  const int select_id_ignore = -1;

  hits = view3d_gpu_select_with_id_filter(
      &vc, &buffer, &rect, VIEW3D_SELECT_PICK_NEAREST, VIEW3D_SELECT_FILTER_NOP, select_id_ignore);

  *r_base = nullptr;

  if (hits > 0) {
    Vector<Base *> bases;

    if (vc.obedit != nullptr) {
      bases = BKE_view_layer_array_from_bases_in_edit_mode(vc.scene, vc.view_layer, vc.v3d);
    }
    else {
      bases = BKE_object_pose_base_array_get(vc.scene, vc.view_layer, vc.v3d);
    }

    void *bone = ed_armature_pick_bone_from_selectbuffer_impl(
        is_editmode, bases, buffer.storage.as_span().take_front(hits), findunsel, true, r_base);

    return bone;
  }
  return nullptr;
}

EditBone *ED_armature_pick_ebone(bContext *C, const int xy[2], bool findunsel, Base **r_base)
{
  const bool is_editmode = true;
  return static_cast<EditBone *>(
      ed_armature_pick_bone_impl(is_editmode, C, xy, findunsel, r_base));
}

bPoseChannel *ED_armature_pick_pchan(bContext *C, const int xy[2], bool findunsel, Base **r_base)
{
  const bool is_editmode = false;
  return static_cast<bPoseChannel *>(
      ed_armature_pick_bone_impl(is_editmode, C, xy, findunsel, r_base));
}

Bone *ED_armature_pick_bone(bContext *C, const int xy[2], bool findunsel, Base **r_base)
{
  bPoseChannel *pchan = ED_armature_pick_pchan(C, xy, findunsel, r_base);
  return pchan ? pchan->bone : nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Linked Implementation
 *
 * Shared logic for select linked all/pick.
 *
 * Use #BONE_DONE flag to select linked.
 * \{ */

/**
 * \param all_forks: Control how chains are stepped over.
 * true: select all connected bones traveling up & down forks.
 * false: select all parents and all children, but not the children of the root bone.
 */
static bool armature_select_linked_impl(Object *ob, const bool select, const bool all_forks)
{
  bool changed = false;
  bArmature *arm = static_cast<bArmature *>(ob->data);

  /* Implementation note, this flood-fills selected bones with the 'TOUCH' flag,
   * even though this is a loop-within a loop, walking up the parent chain only touches new bones.
   * Bones that have been touched are skipped, so the complexity is OK. */

  enum {
    /* Bone has been walked over, its LINK value can be read. */
    TOUCH = (1 << 0),
    /* When TOUCH has been set, this flag can be checked to see if the bone is connected. */
    LINK = (1 << 1),
  };

#define CHECK_PARENT(ebone) \
\
  (((ebone)->flag & BONE_CONNECTED) && \
   ((ebone)->parent ? EBONE_SELECTABLE(arm, (ebone)->parent) : false))

  LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
    ebone->temp.i = 0;
  }

  /* Select parents. */
  LISTBASE_FOREACH (EditBone *, ebone_iter, arm->edbo) {
    if (ebone_iter->temp.i & TOUCH) {
      continue;
    }
    if ((ebone_iter->flag & BONE_DONE) == 0) {
      continue;
    }

    ebone_iter->temp.i |= TOUCH | LINK;

    /* We have an un-touched link. */
    for (EditBone *ebone = ebone_iter; ebone;
         ebone = CHECK_PARENT(ebone) ? ebone->parent : nullptr)
    {
      ED_armature_ebone_select_set(ebone, select);
      changed = true;

      if (all_forks) {
        ebone->temp.i |= (TOUCH | LINK);
      }
      else {
        ebone->temp.i |= TOUCH;
      }
      /* Don't walk onto links (messes up 'all_forks' logic). */
      if (ebone->parent && ebone->parent->temp.i & LINK) {
        break;
      }
    }
  }

  /* Select children. */
  LISTBASE_FOREACH (EditBone *, ebone_iter, arm->edbo) {
    /* No need to 'touch' this bone as it won't be walked over when scanning up the chain. */
    if (!CHECK_PARENT(ebone_iter)) {
      continue;
    }
    if (ebone_iter->temp.i & TOUCH) {
      continue;
    }

    /* First check if we're marked. */
    EditBone *ebone_touched_parent = nullptr;
    for (EditBone *ebone = ebone_iter; ebone;
         ebone = CHECK_PARENT(ebone) ? ebone->parent : nullptr)
    {
      if (ebone->temp.i & TOUCH) {
        ebone_touched_parent = ebone;
        break;
      }
      ebone->temp.i |= TOUCH;
    }

    if ((ebone_touched_parent != nullptr) && (ebone_touched_parent->temp.i & LINK)) {
      for (EditBone *ebone = ebone_iter; ebone != ebone_touched_parent; ebone = ebone->parent) {
        if ((ebone->temp.i & LINK) == 0) {
          ebone->temp.i |= LINK;
          ED_armature_ebone_select_set(ebone, select);
          changed = true;
        }
      }
    }
  }

#undef CHECK_PARENT

  if (changed) {
    ED_armature_edit_sync_selection(arm->edbo);
    DEG_id_tag_update(&arm->id, ID_RECALC_SYNC_TO_EVAL);
    WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, ob);
  }

  return changed;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Linked Operator
 * \{ */

static wmOperatorStatus armature_select_linked_exec(bContext *C, wmOperator *op)
{
  const bool all_forks = RNA_boolean_get(op->ptr, "all_forks");

  bool changed_multi = false;
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *ob : objects) {
    bArmature *arm = static_cast<bArmature *>(ob->data);

    bool found = false;
    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      if (blender::animrig::bone_is_visible(arm, ebone) &&
          (ebone->flag & (BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL)))
      {
        ebone->flag |= BONE_DONE;
        found = true;
      }
      else {
        ebone->flag &= ~BONE_DONE;
      }
    }

    if (found) {
      if (armature_select_linked_impl(ob, true, all_forks)) {
        changed_multi = true;
      }
    }
  }

  if (changed_multi) {
    ED_outliner_select_sync_from_edit_bone_tag(C);
  }
  return OPERATOR_FINISHED;
}

void ARMATURE_OT_select_linked(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Linked All";
  ot->idname = "ARMATURE_OT_select_linked";
  ot->description = "Select all bones linked by parent/child connections to the current selection";

  /* API callbacks. */
  ot->exec = armature_select_linked_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Leave disabled by default as this matches pose mode. */
  RNA_def_boolean(ot->srna, "all_forks", false, "All Forks", "Follow forks in the parents chain");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Linked (Cursor Pick) Operator
 * \{ */

static wmOperatorStatus armature_select_linked_pick_invoke(bContext *C,
                                                           wmOperator *op,
                                                           const wmEvent *event)
{
  const bool select = !RNA_boolean_get(op->ptr, "deselect");
  const bool all_forks = RNA_boolean_get(op->ptr, "all_forks");

  view3d_operator_needs_gpu(C);
  BKE_object_update_select_id(CTX_data_main(C));

  Base *base = nullptr;
  EditBone *ebone_active = ED_armature_pick_ebone(C, event->mval, true, &base);

  if (ebone_active == nullptr) {
    return OPERATOR_CANCELLED;
  }

  bArmature *arm = static_cast<bArmature *>(base->object->data);
  if (!EBONE_SELECTABLE(arm, ebone_active)) {
    return OPERATOR_CANCELLED;
  }

  /* Initialize flags. */
  LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
    ebone->flag &= ~BONE_DONE;
  }
  ebone_active->flag |= BONE_DONE;

  if (armature_select_linked_impl(base->object, select, all_forks)) {
    ED_outliner_select_sync_from_edit_bone_tag(C);
  }

  return OPERATOR_FINISHED;
}

static bool armature_select_linked_pick_poll(bContext *C)
{
  return (ED_operator_view3d_active(C) && ED_operator_editarmature(C));
}

void ARMATURE_OT_select_linked_pick(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Linked";
  ot->idname = "ARMATURE_OT_select_linked_pick";
  ot->description = "(De)select bones linked by parent/child connections under the mouse cursor";

  /* API callbacks. */
  /* leave 'exec' unset */
  ot->invoke = armature_select_linked_pick_invoke;
  ot->poll = armature_select_linked_pick_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "deselect", false, "Deselect", "");
  /* Leave disabled by default as this matches pose mode. */
  RNA_def_boolean(ot->srna, "all_forks", false, "All Forks", "Follow forks in the parents chain");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Buffer Queries EditMode
 * \{ */

/* utility function for get_nearest_editbonepoint */
static int selectbuffer_ret_hits_12(blender::MutableSpan<GPUSelectResult> /*hit_results*/,
                                    const int hits12)
{
  return hits12;
}

static int selectbuffer_ret_hits_5(blender::MutableSpan<GPUSelectResult> hit_results,
                                   const int hits12,
                                   const int hits5)
{
  const int ofs = hits12;
  /* Shift results to beginning. */
  hit_results.slice(0, hits5).copy_from(hit_results.slice(ofs, hits5));
  return hits5;
}

/* does bones and points */
/* note that BONE ROOT only gets drawn for root bones (or without IK) */
static EditBone *get_nearest_editbonepoint(
    ViewContext *vc, bool findunsel, bool use_cycle, Base **r_base, int *r_selmask)
{
  GPUSelectBuffer buffer;
  struct Result {
    uint select_id;
    Base *base;
    EditBone *ebone;
  };
  Result *result = nullptr;
  Result result_cycle{};
  result_cycle.select_id = -1;
  result_cycle.base = nullptr;
  result_cycle.ebone = nullptr;
  Result result_bias{};
  result_bias.select_id = -1;
  result_bias.base = nullptr;
  result_bias.ebone = nullptr;

  /* Find the bone after the current (selected) active bone, so as to bump up its chances in
   * selection. this way overlapping bones will cycle selection state as with objects. */
  Object *obedit_orig = vc->obedit;
  EditBone *ebone_active_orig = static_cast<bArmature *>(obedit_orig->data)->act_edbone;
  if (ebone_active_orig &&
      (ebone_active_orig->flag & (BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL)) == 0)
  {
    ebone_active_orig = nullptr;
  }

  if (ebone_active_orig == nullptr) {
    use_cycle = false;
  }

  if (use_cycle) {
    use_cycle = !WM_cursor_test_motion_and_update(vc->mval);
  }

  const bool do_nearest = !(XRAY_ACTIVE(vc->v3d) || use_cycle);

  /* matching logic from 'mixed_bones_object_selectbuffer' */
  int hits = 0;
  /* Don't use hits with this ID, (armature drawing uses this). */
  const int select_id_ignore = -1;

  /* we _must_ end cache before return, use 'goto cache_end' */
  view3d_gpu_select_cache_begin();

  {
    const eV3DSelectObjectFilter select_filter = VIEW3D_SELECT_FILTER_NOP;

    GPUSelectStorage &storage = buffer.storage;
    rcti rect;
    BLI_rcti_init_pt_radius(&rect, vc->mval, 12);
    /* VIEW3D_SELECT_PICK_ALL needs to be used or unselectable bones can block selectability of
     * bones further back. See #123963. */
    const int hits12 = view3d_gpu_select_with_id_filter(
        vc, &buffer, &rect, VIEW3D_SELECT_PICK_ALL, select_filter, select_id_ignore);

    if (hits12 == 1) {
      hits = selectbuffer_ret_hits_12(storage.as_mutable_span(), hits12);
      goto cache_end;
    }
    else if (hits12 > 0) {
      BLI_rcti_init_pt_radius(&rect, vc->mval, 5);
      const int hits5 = view3d_gpu_select_with_id_filter(
          vc, &buffer, &rect, VIEW3D_SELECT_PICK_ALL, select_filter, select_id_ignore);

      if (hits5 == 1) {
        hits = selectbuffer_ret_hits_5(storage.as_mutable_span(), hits12, hits5);
        goto cache_end;
      }

      if (hits5 > 0) {
        hits = selectbuffer_ret_hits_5(storage.as_mutable_span(), hits12, hits5);
        goto cache_end;
      }
      else {
        hits = selectbuffer_ret_hits_12(storage.as_mutable_span(), hits12);
        goto cache_end;
      }
    }
  }

cache_end:
  view3d_gpu_select_cache_end();

  Vector<Base *> bases = BKE_view_layer_array_from_bases_in_edit_mode_unique_data(
      vc->scene, vc->view_layer, vc->v3d);

  /* See if there are any selected bones in this group */
  if (hits > 0) {
    if (hits == 1) {
      result_bias.select_id = buffer.storage[0].id;
      result_bias.base = ED_armature_base_and_ebone_from_select_buffer(
          bases, result_bias.select_id, &result_bias.ebone);
    }
    else {
      int bias_max = INT_MIN;

      /* Track Cycle Variables
       * - Offset is always set to the active bone.
       * - The object & bone indices subtracted by the 'offset.as_u32' value.
       *   Unsigned subtraction wrapping means we always select the next bone in the cycle.
       */
      struct {
        union {
          uint32_t as_u32;
          struct {
            /* NOTE: this is endianness-sensitive.
             * In Big Endian the order of these two variable would have to be inverted. */
            uint16_t bone;
            uint16_t ob;
          };
        } offset, test, best;
      } cycle_order;

      if (use_cycle) {
        bArmature *arm = static_cast<bArmature *>(obedit_orig->data);
        int ob_index = obedit_orig->runtime->select_id & 0xFFFF;
        int bone_index = BLI_findindex(arm->edbo, ebone_active_orig);
        /* Offset from the current active bone, so we cycle onto the next. */
        cycle_order.offset.ob = ob_index;
        cycle_order.offset.bone = bone_index;
        /* The value of the active bone (with offset subtracted, a signal to always overwrite). */
        cycle_order.best.as_u32 = 0;
      }

      int min_depth = INT_MAX;
      for (int i = 0; i < hits; i++) {
        const GPUSelectResult &hit_result = buffer.storage[i];
        const uint select_id = hit_result.id;

        Base *base = nullptr;
        EditBone *ebone;
        base = ED_armature_base_and_ebone_from_select_buffer(bases, select_id, &ebone);
        /* If this fails, selection code is setting the selection ID's incorrectly. */
        BLI_assert(base && ebone);

        if (ebone->flag & BONE_UNSELECTABLE) {
          continue;
        }

        /* Prioritized selection. */
        {
          int bias;
          /* clicks on bone points get advantage */
          if (select_id & (BONESEL_ROOT | BONESEL_TIP)) {
            /* but also the unselected one */
            if (findunsel) {
              if ((select_id & BONESEL_ROOT) && (ebone->flag & BONE_ROOTSEL) == 0) {
                bias = 4;
              }
              else if ((select_id & BONESEL_TIP) && (ebone->flag & BONE_TIPSEL) == 0) {
                bias = 4;
              }
              else {
                bias = 3;
              }
            }
            else {
              bias = 4;
            }
          }
          else {
            /* bone found */
            if (findunsel) {
              if ((ebone->flag & BONE_SELECTED) == 0) {
                bias = 2;
              }
              else {
                bias = 1;
              }
            }
            else {
              bias = 2;
            }
          }

          if (bias > bias_max) {
            bias_max = bias;

            min_depth = hit_result.depth;
            result_bias.select_id = select_id;
            result_bias.base = base;
            result_bias.ebone = ebone;
          }
          else if (bias == bias_max && do_nearest) {
            if (min_depth > hit_result.depth) {
              min_depth = hit_result.depth;
              result_bias.select_id = select_id;
              result_bias.base = base;
              result_bias.ebone = ebone;
            }
          }
        }

        /* Cycle selected items (objects & bones). */
        if (use_cycle) {
          cycle_order.test.ob = select_id & 0xFFFF;
          cycle_order.test.bone = (select_id & ~BONESEL_ANY) >> 16;
          if (ebone == ebone_active_orig) {
            BLI_assert(cycle_order.test.ob == cycle_order.offset.ob);
            BLI_assert(cycle_order.test.bone == cycle_order.offset.bone);
          }
          /* Subtraction as a single value is needed to support cycling through bones
           * from multiple objects. So once the last bone is selected,
           * the bits for the bone index wrap into the object,
           * causing the next object to be stepped onto. */
          cycle_order.test.as_u32 -= cycle_order.offset.as_u32;

          /* Even though this logic avoids stepping onto the active bone,
           * always set the 'best' value for the first time.
           * Otherwise ensure the value is the smallest it can be,
           * relative to the active bone, as long as it's not the active bone. */
          if ((cycle_order.best.as_u32 == 0) ||
              (cycle_order.test.as_u32 && (cycle_order.test.as_u32 < cycle_order.best.as_u32)))
          {
            cycle_order.best = cycle_order.test;
            result_cycle.select_id = select_id;
            result_cycle.base = base;
            result_cycle.ebone = ebone;
          }
        }
      }
    }

    result = (use_cycle && result_cycle.ebone) ? &result_cycle : &result_bias;

    if (result->select_id != -1) {
      *r_base = result->base;

      *r_selmask = 0;
      if (result->select_id & BONESEL_ROOT) {
        *r_selmask |= BONE_ROOTSEL;
      }
      if (result->select_id & BONESEL_TIP) {
        *r_selmask |= BONE_TIPSEL;
      }
      if (result->select_id & BONESEL_BONE) {
        *r_selmask |= BONE_SELECTED;
      }
      return result->ebone;
    }
  }
  *r_selmask = 0;
  *r_base = nullptr;
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Utility Functions
 * \{ */

bool ED_armature_edit_deselect_all(Object *obedit)
{
  bArmature *arm = static_cast<bArmature *>(obedit->data);
  bool changed = false;
  LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
    if (ebone->flag & (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL)) {
      ebone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
      changed = true;
    }
  }
  return changed;
}

bool ED_armature_edit_deselect_all_visible(Object *obedit)
{
  bArmature *arm = static_cast<bArmature *>(obedit->data);
  bool changed = false;
  LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
    /* first and foremost, bone must be visible and selected */
    if (blender::animrig::bone_is_visible(arm, ebone)) {
      if (ebone->flag & (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL)) {
        ebone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
        changed = true;
      }
    }
  }

  if (changed) {
    ED_armature_edit_sync_selection(arm->edbo);
  }
  return changed;
}

bool ED_armature_edit_deselect_all_multi_ex(const Span<Base *> bases)
{
  bool changed_multi = false;
  for (Base *base : bases) {
    Object *obedit = base->object;
    changed_multi |= ED_armature_edit_deselect_all(obedit);
  }
  return changed_multi;
}

bool ED_armature_edit_deselect_all_visible_multi_ex(const Span<Base *> bases)
{
  bool changed_multi = false;
  for (Base *base : bases) {
    Object *obedit = base->object;
    changed_multi |= ED_armature_edit_deselect_all_visible(obedit);
  }
  return changed_multi;
}

bool ED_armature_edit_deselect_all_visible_multi(bContext *C)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);
  Vector<Base *> bases = BKE_view_layer_array_from_bases_in_edit_mode_unique_data(
      vc.scene, vc.view_layer, vc.v3d);
  return ED_armature_edit_deselect_all_multi_ex(bases);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Cursor Pick API
 * \{ */

bool ED_armature_edit_select_pick_bone(
    bContext *C, Base *basact, EditBone *ebone, const int selmask, const SelectPick_Params &params)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);
  bool changed = false;
  bool found = false;

  if (ebone) {
    bArmature *arm = static_cast<bArmature *>(basact->object->data);
    if (EBONE_SELECTABLE(arm, ebone)) {
      found = true;
    }
  }

  if (params.sel_op == SEL_OP_SET) {
    if ((found && params.select_passthrough) &&
        (ED_armature_ebone_selectflag_get(ebone) & selmask))
    {
      found = false;
    }
    else if (found || params.deselect_all) {
      /* Deselect everything. */
      Vector<Base *> bases = BKE_view_layer_array_from_bases_in_edit_mode_unique_data(
          scene, view_layer, v3d);
      ED_armature_edit_deselect_all_multi_ex(bases);
      changed = true;
    }
  }

  if (found) {
    BLI_assert(BKE_object_is_in_editmode(basact->object));
    bArmature *arm = static_cast<bArmature *>(basact->object->data);

    /* By definition the non-root connected bones have no root point drawn,
     * so a root selection needs to be delivered to the parent tip. */

    if (selmask & BONE_SELECTED) {
      if (ebone->parent && (ebone->flag & BONE_CONNECTED)) {

        /* Bone is in a chain. */
        switch (params.sel_op) {
          case SEL_OP_ADD: {
            /* Select this bone. */
            ebone->flag |= BONE_TIPSEL;
            ebone->parent->flag |= BONE_TIPSEL;
            break;
          }
          case SEL_OP_SUB: {
            /* Deselect this bone. */
            ebone->flag &= ~(BONE_TIPSEL | BONE_SELECTED);
            /* Only deselect parent tip if it is not selected. */
            if (!(ebone->parent->flag & BONE_SELECTED)) {
              ebone->parent->flag &= ~BONE_TIPSEL;
            }
            break;
          }
          case SEL_OP_XOR: {
            /* Toggle inverts this bone's selection. */
            if (ebone->flag & BONE_SELECTED) {
              /* Deselect this bone. */
              ebone->flag &= ~(BONE_TIPSEL | BONE_SELECTED);
              /* Only deselect parent tip if it is not selected. */
              if (!(ebone->parent->flag & BONE_SELECTED)) {
                ebone->parent->flag &= ~BONE_TIPSEL;
              }
            }
            else {
              /* Select this bone. */
              ebone->flag |= BONE_TIPSEL;
              ebone->parent->flag |= BONE_TIPSEL;
            }
            break;
          }
          case SEL_OP_SET: {
            /* Select this bone. */
            ebone->flag |= BONE_TIPSEL;
            ebone->parent->flag |= BONE_TIPSEL;
            break;
          }
          case SEL_OP_AND: {
            BLI_assert_unreachable(); /* Doesn't make sense for picking. */
            break;
          }
        }
      }
      else {
        switch (params.sel_op) {
          case SEL_OP_ADD: {
            ebone->flag |= (BONE_TIPSEL | BONE_ROOTSEL);
            break;
          }
          case SEL_OP_SUB: {
            ebone->flag &= ~(BONE_TIPSEL | BONE_ROOTSEL);
            break;
          }
          case SEL_OP_XOR: {
            /* Toggle inverts this bone's selection. */
            if (ebone->flag & BONE_SELECTED) {
              ebone->flag &= ~(BONE_TIPSEL | BONE_ROOTSEL);
            }
            else {
              ebone->flag |= (BONE_TIPSEL | BONE_ROOTSEL);
            }
            break;
          }
          case SEL_OP_SET: {
            ebone->flag |= (BONE_TIPSEL | BONE_ROOTSEL);
            break;
          }
          case SEL_OP_AND: {
            BLI_assert_unreachable(); /* Doesn't make sense for picking. */
            break;
          }
        }
      }
    }
    else {
      switch (params.sel_op) {
        case SEL_OP_ADD: {
          ebone->flag |= selmask;
          break;
        }
        case SEL_OP_SUB: {
          ebone->flag &= ~selmask;
          break;
        }
        case SEL_OP_XOR: {
          if (ebone->flag & selmask) {
            ebone->flag &= ~selmask;
          }
          else {
            ebone->flag |= selmask;
          }
          break;
        }
        case SEL_OP_SET: {
          ebone->flag |= selmask;
          break;
        }
        case SEL_OP_AND: {
          BLI_assert_unreachable(); /* Doesn't make sense for picking. */
          break;
        }
      }
    }

    ED_armature_edit_sync_selection(arm->edbo);

    /* Now check for active status. */
    if (ED_armature_ebone_selectflag_get(ebone)) {
      arm->act_edbone = ebone;
    }

    BKE_view_layer_synced_ensure(scene, view_layer);
    if (BKE_view_layer_active_base_get(view_layer) != basact) {
      blender::ed::object::base_activate(C, basact);
    }

    WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, basact->object);
    DEG_id_tag_update(&arm->id, ID_RECALC_SYNC_TO_EVAL);
    changed = true;
  }

  if (changed) {
    ED_outliner_select_sync_from_edit_bone_tag(C);
  }

  return changed || found;
}

bool ED_armature_edit_select_pick(bContext *C, const int mval[2], const SelectPick_Params &params)

{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  EditBone *nearBone = nullptr;
  int selmask;
  Base *basact = nullptr;

  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);
  vc.mval[0] = mval[0];
  vc.mval[1] = mval[1];

  nearBone = get_nearest_editbonepoint(&vc, true, true, &basact, &selmask);
  return ED_armature_edit_select_pick_bone(C, basact, nearBone, selmask, params);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Op From Tagged
 *
 * Implements #ED_armature_edit_select_op_from_tagged
 * \{ */

static bool armature_edit_select_op_apply(bArmature *arm,
                                          EditBone *ebone,
                                          const eSelectOp sel_op,
                                          int is_ignore_flag,
                                          int is_inside_flag)
{
  BLI_assert(!(is_ignore_flag & ~(BONESEL_ROOT | BONESEL_TIP)));
  BLI_assert(!(is_inside_flag & ~(BONESEL_ROOT | BONESEL_TIP | BONESEL_BONE)));
  BLI_assert(blender::animrig::bone_is_visible(arm, ebone));
  bool changed = false;
  bool is_point_done = false;
  int points_proj_tot = 0;
  BLI_assert(ebone->flag == ebone->temp.i);
  const int ebone_flag_prev = ebone->flag;

  if ((is_ignore_flag & BONE_ROOTSEL) == 0) {
    points_proj_tot++;
    const bool is_select = ebone->flag & BONE_ROOTSEL;
    const bool is_inside = is_inside_flag & BONESEL_ROOT;
    const int sel_op_result = ED_select_op_action_deselected(sel_op, is_select, is_inside);
    if (sel_op_result != -1) {
      if (sel_op_result == 0 || EBONE_SELECTABLE(arm, ebone)) {
        SET_FLAG_FROM_TEST(ebone->flag, sel_op_result, BONE_ROOTSEL);
      }
    }
    is_point_done |= is_inside;
  }

  if ((is_ignore_flag & BONE_TIPSEL) == 0) {
    points_proj_tot++;
    const bool is_select = ebone->flag & BONE_TIPSEL;
    const bool is_inside = is_inside_flag & BONESEL_TIP;
    const int sel_op_result = ED_select_op_action_deselected(sel_op, is_select, is_inside);
    if (sel_op_result != -1) {
      if (sel_op_result == 0 || EBONE_SELECTABLE(arm, ebone)) {
        SET_FLAG_FROM_TEST(ebone->flag, sel_op_result, BONE_TIPSEL);
      }
    }
    is_point_done |= is_inside;
  }

  /* if one of points selected, we skip the bone itself */
  if ((is_point_done == false) && (points_proj_tot == 2)) {
    const bool is_select = ebone->flag & BONE_SELECTED;
    {
      const bool is_inside = is_inside_flag & BONESEL_BONE;
      const int sel_op_result = ED_select_op_action_deselected(sel_op, is_select, is_inside);
      if (sel_op_result != -1) {
        if (sel_op_result == 0 || EBONE_SELECTABLE(arm, ebone)) {
          SET_FLAG_FROM_TEST(
              ebone->flag, sel_op_result, BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL);
        }
      }
    }

    changed = true;
  }
  changed |= is_point_done;

  if (ebone_flag_prev != ebone->flag) {
    ebone->temp.i = ebone->flag;
    ebone->flag = ebone_flag_prev;
    ebone->flag = ebone_flag_prev | BONE_DONE;
    changed = true;
  }

  return changed;
}

bool ED_armature_edit_select_op_from_tagged(bArmature *arm, const int sel_op)
{
  bool changed = false;

  /* Initialize flags. */
  {
    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {

      /* Flush the parent flag to this bone
       * so we don't need to check the parent when adjusting the selection. */
      if ((ebone->flag & BONE_CONNECTED) && ebone->parent) {
        if (ebone->parent->flag & BONE_TIPSEL) {
          ebone->flag |= BONE_ROOTSEL;
        }
        else {
          ebone->flag &= ~BONE_ROOTSEL;
        }

        /* Flush the 'temp.i' flag. */
        if (ebone->parent->temp.i & BONESEL_TIP) {
          ebone->temp.i |= BONESEL_ROOT;
        }
      }
      ebone->flag &= ~BONE_DONE;
    }
  }

  /* Apply selection from bone selection flags. */
  LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
    if (ebone->temp.i != 0) {
      int is_ignore_flag = ((ebone->temp.i << 16) & (BONESEL_ROOT | BONESEL_TIP));
      int is_inside_flag = (ebone->temp.i & (BONESEL_ROOT | BONESEL_TIP | BONESEL_BONE));

      /* Use as previous bone flag from now on. */
      ebone->temp.i = ebone->flag;

      /* When there is a partial selection without both endpoints, only select an endpoint. */
      if ((is_inside_flag & BONESEL_BONE) &&
          ELEM(is_inside_flag & (BONESEL_ROOT | BONESEL_TIP), BONESEL_ROOT, BONESEL_TIP))
      {
        is_inside_flag &= ~BONESEL_BONE;
      }

      changed |= armature_edit_select_op_apply(
          arm, ebone, eSelectOp(sel_op), is_ignore_flag, is_inside_flag);
    }
  }

  if (changed) {
    /* Cleanup flags. */
    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      if (ebone->flag & BONE_DONE) {
        std::swap(ebone->temp.i, ebone->flag);
        ebone->flag |= BONE_DONE;
        if ((ebone->flag & BONE_CONNECTED) && ebone->parent) {
          if ((ebone->parent->flag & BONE_DONE) == 0) {
            /* Checked below. */
            ebone->parent->temp.i = ebone->parent->flag;
          }
        }
      }
    }

    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      if (ebone->flag & BONE_DONE) {
        if ((ebone->flag & BONE_CONNECTED) && ebone->parent) {
          bool is_parent_tip_changed = (ebone->parent->flag & BONE_TIPSEL) !=
                                       (ebone->parent->temp.i & BONE_TIPSEL);
          if ((ebone->temp.i & BONE_ROOTSEL) == 0) {
            if ((ebone->flag & BONE_ROOTSEL) != 0) {
              ebone->parent->flag |= BONE_TIPSEL;
            }
          }
          else {
            if ((ebone->flag & BONE_ROOTSEL) == 0) {
              ebone->parent->flag &= ~BONE_TIPSEL;
            }
          }

          if (is_parent_tip_changed == false) {
            /* Keep tip selected if the parent remains selected. */
            if (ebone->parent->flag & BONE_SELECTED) {
              ebone->parent->flag |= BONE_TIPSEL;
            }
          }
        }
        ebone->flag &= ~BONE_DONE;
      }
    }

    ED_armature_edit_sync_selection(arm->edbo);
  }

  return changed;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name (De)Select All Operator
 * \{ */

static wmOperatorStatus armature_de_select_all_exec(bContext *C, wmOperator *op)
{
  int action = RNA_enum_get(op->ptr, "action");

  if (action == SEL_TOGGLE) {
    /* Determine if there are any selected bones
     * And therefore whether we are selecting or deselecting */
    action = SEL_SELECT;
    CTX_DATA_BEGIN (C, EditBone *, ebone, visible_bones) {
      if (ebone->flag & (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL)) {
        action = SEL_DESELECT;
        break;
      }
    }
    CTX_DATA_END;
  }

  /* Set the flags. */
  CTX_DATA_BEGIN (C, EditBone *, ebone, visible_bones) {
    /* ignore bone if selection can't change */
    switch (action) {
      case SEL_SELECT:
        if ((ebone->flag & BONE_UNSELECTABLE) == 0) {
          ebone->flag |= (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
          if (ebone->parent && (ebone->flag & BONE_CONNECTED)) {
            ebone->parent->flag |= BONE_TIPSEL;
          }
        }
        break;
      case SEL_DESELECT:
        ebone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
        break;
      case SEL_INVERT:
        if (ebone->flag & BONE_SELECTED) {
          ebone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
        }
        else {
          if ((ebone->flag & BONE_UNSELECTABLE) == 0) {
            ebone->flag |= (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
            if (ebone->parent && (ebone->flag & BONE_CONNECTED)) {
              ebone->parent->flag |= BONE_TIPSEL;
            }
          }
        }
        break;
    }
  }
  CTX_DATA_END;

  ED_outliner_select_sync_from_edit_bone_tag(C);

  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, nullptr);

  /* Tagging only one object to refresh drawing. */
  Object *obedit = CTX_data_edit_object(C);
  DEG_id_tag_update(&obedit->id, ID_RECALC_SELECT);

  return OPERATOR_FINISHED;
}

void ARMATURE_OT_select_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "(De)select All";
  ot->idname = "ARMATURE_OT_select_all";
  ot->description = "Toggle selection status of all bones";

  /* API callbacks. */
  ot->exec = armature_de_select_all_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_select_all(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select More/Less Implementation
 * \{ */

static void armature_select_more(bArmature *arm, EditBone *ebone)
{
  if ((EBONE_PREV_FLAG_GET(ebone) & (BONE_ROOTSEL | BONE_TIPSEL)) != 0) {
    if (EBONE_SELECTABLE(arm, ebone)) {
      ED_armature_ebone_select_set(ebone, true);
    }
  }

  if (ebone->parent && (ebone->flag & BONE_CONNECTED)) {
    /* to parent */
    if ((EBONE_PREV_FLAG_GET(ebone) & BONE_ROOTSEL) != 0) {
      if (EBONE_SELECTABLE(arm, ebone->parent)) {
        ED_armature_ebone_selectflag_enable(ebone->parent,
                                            (BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL));
      }
    }

    /* from parent (difference from select less) */
    if ((EBONE_PREV_FLAG_GET(ebone->parent) & BONE_TIPSEL) != 0) {
      if (EBONE_SELECTABLE(arm, ebone)) {
        ED_armature_ebone_selectflag_enable(ebone, (BONE_SELECTED | BONE_ROOTSEL));
      }
    }
  }
}

static void armature_select_less(bArmature * /*arm*/, EditBone *ebone)
{
  if ((EBONE_PREV_FLAG_GET(ebone) & (BONE_ROOTSEL | BONE_TIPSEL)) != (BONE_ROOTSEL | BONE_TIPSEL))
  {
    ED_armature_ebone_select_set(ebone, false);
  }

  if (ebone->parent && (ebone->flag & BONE_CONNECTED)) {
    /* to parent */
    if ((EBONE_PREV_FLAG_GET(ebone) & BONE_SELECTED) == 0) {
      ED_armature_ebone_selectflag_disable(ebone->parent, (BONE_SELECTED | BONE_TIPSEL));
    }

    /* from parent (difference from select more) */
    if ((EBONE_PREV_FLAG_GET(ebone->parent) & BONE_SELECTED) == 0) {
      ED_armature_ebone_selectflag_disable(ebone, (BONE_SELECTED | BONE_ROOTSEL));
    }
  }
}

static void armature_select_more_less(Object *ob, bool more)
{
  bArmature *arm = static_cast<bArmature *>(ob->data);

  /* XXX(@ideasman42): eventually we shouldn't need this. */
  ED_armature_edit_sync_selection(arm->edbo);

  /* count bones & store selection state */
  LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
    EBONE_PREV_FLAG_SET(ebone, ED_armature_ebone_selectflag_get(ebone));
  }

  /* do selection */
  LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
    if (blender::animrig::bone_is_visible(arm, ebone)) {
      if (more) {
        armature_select_more(arm, ebone);
      }
      else {
        armature_select_less(arm, ebone);
      }
    }
  }

  LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
    if (blender::animrig::bone_is_visible(arm, ebone)) {
      if (more == false) {
        if (ebone->flag & BONE_SELECTED) {
          ED_armature_ebone_select_set(ebone, true);
        }
      }
    }
    ebone->temp.p = nullptr;
  }

  ED_armature_edit_sync_selection(arm->edbo);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select More Operator
 * \{ */

static wmOperatorStatus armature_de_select_more_exec(bContext *C, wmOperator * /*op*/)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *ob : objects) {
    armature_select_more_less(ob, true);
    WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
    DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL);
  }

  ED_outliner_select_sync_from_edit_bone_tag(C);
  return OPERATOR_FINISHED;
}

void ARMATURE_OT_select_more(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select More";
  ot->idname = "ARMATURE_OT_select_more";
  ot->description = "Select those bones connected to the initial selection";

  /* API callbacks. */
  ot->exec = armature_de_select_more_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Less Operator
 * \{ */

static wmOperatorStatus armature_de_select_less_exec(bContext *C, wmOperator * /*op*/)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *ob : objects) {
    armature_select_more_less(ob, false);
    WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
    DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL);
  }

  ED_outliner_select_sync_from_edit_bone_tag(C);
  return OPERATOR_FINISHED;
}

void ARMATURE_OT_select_less(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Less";
  ot->idname = "ARMATURE_OT_select_less";
  ot->description = "Deselect those bones at the boundary of each selection region";

  /* API callbacks. */
  ot->exec = armature_de_select_less_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Similar
 * \{ */

enum {
  SIMEDBONE_CHILDREN = 1,
  SIMEDBONE_CHILDREN_IMMEDIATE,
  SIMEDBONE_SIBLINGS,
  SIMEDBONE_LENGTH,
  SIMEDBONE_DIRECTION,
  SIMEDBONE_PREFIX,
  SIMEDBONE_SUFFIX,
  SIMEDBONE_COLLECTION,
  SIMEDBONE_COLOR,
  SIMEDBONE_SHAPE,
};

static const EnumPropertyItem prop_similar_types[] = {
    {SIMEDBONE_CHILDREN, "CHILDREN", 0, "Children", ""},
    {SIMEDBONE_CHILDREN_IMMEDIATE, "CHILDREN_IMMEDIATE", 0, "Immediate Children", ""},
    {SIMEDBONE_SIBLINGS, "SIBLINGS", 0, "Siblings", ""},
    {SIMEDBONE_LENGTH, "LENGTH", 0, "Length", ""},
    {SIMEDBONE_DIRECTION, "DIRECTION", 0, "Direction (Y Axis)", ""},
    {SIMEDBONE_PREFIX, "PREFIX", 0, "Prefix", ""},
    {SIMEDBONE_SUFFIX, "SUFFIX", 0, "Suffix", ""},
    {SIMEDBONE_COLLECTION, "BONE_COLLECTION", 0, "Bone Collection", ""},
    {SIMEDBONE_COLOR, "COLOR", 0, "Color", ""},
    {SIMEDBONE_SHAPE, "SHAPE", 0, "Shape", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static float bone_length_squared_worldspace_get(Object *ob, EditBone *ebone)
{
  float v1[3], v2[3];
  mul_v3_mat3_m4v3(v1, ob->object_to_world().ptr(), ebone->head);
  mul_v3_mat3_m4v3(v2, ob->object_to_world().ptr(), ebone->tail);
  return len_squared_v3v3(v1, v2);
}

static void select_similar_length(bContext *C, const float thresh)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob_act = CTX_data_edit_object(C);
  EditBone *ebone_act = CTX_data_active_bone(C);

  /* Thresh is always relative to current length. */
  const float len = bone_length_squared_worldspace_get(ob_act, ebone_act);
  const float len_min = len / (1.0f + (thresh - FLT_EPSILON));
  const float len_max = len * (1.0f + (thresh + FLT_EPSILON));

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *ob : objects) {
    bArmature *arm = static_cast<bArmature *>(ob->data);
    bool changed = false;

    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      if (EBONE_SELECTABLE(arm, ebone)) {
        const float len_iter = bone_length_squared_worldspace_get(ob, ebone);
        if ((len_iter > len_min) && (len_iter < len_max)) {
          ED_armature_ebone_select_set(ebone, true);
          changed = true;
        }
      }
    }

    if (changed) {
      WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
      DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL);
    }
  }
}

static void bone_direction_worldspace_get(Object *ob, EditBone *ebone, float *r_dir)
{
  float v1[3], v2[3];
  copy_v3_v3(v1, ebone->head);
  copy_v3_v3(v2, ebone->tail);

  mul_m4_v3(ob->object_to_world().ptr(), v1);
  mul_m4_v3(ob->object_to_world().ptr(), v2);

  sub_v3_v3v3(r_dir, v1, v2);
  normalize_v3(r_dir);
}

static void select_similar_direction(bContext *C, const float thresh)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob_act = CTX_data_edit_object(C);
  EditBone *ebone_act = CTX_data_active_bone(C);

  float dir_act[3];
  bone_direction_worldspace_get(ob_act, ebone_act, dir_act);

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *ob : objects) {
    bArmature *arm = static_cast<bArmature *>(ob->data);
    bool changed = false;

    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      if (EBONE_SELECTABLE(arm, ebone)) {
        float dir[3];
        bone_direction_worldspace_get(ob, ebone, dir);

        if (angle_v3v3(dir_act, dir) / float(M_PI) < (thresh + FLT_EPSILON)) {
          ED_armature_ebone_select_set(ebone, true);
          changed = true;
        }
      }
    }

    if (changed) {
      WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
      DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL);
      DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL);
    }
  }
}

static void select_similar_bone_collection(bContext *C)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  EditBone *ebone_act = CTX_data_active_bone(C);

  /* Build a set of bone collection names, to allow cross-Armature selection. */
  blender::Set<std::string> collection_names;
  LISTBASE_FOREACH (BoneCollectionReference *, bcoll_ref, &ebone_act->bone_collections) {
    collection_names.add(bcoll_ref->bcoll->name);
  }

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *ob : objects) {
    bArmature *arm = static_cast<bArmature *>(ob->data);
    bool changed = false;

    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      if (!EBONE_SELECTABLE(arm, ebone)) {
        continue;
      }

      LISTBASE_FOREACH (BoneCollectionReference *, bcoll_ref, &ebone->bone_collections) {
        if (!collection_names.contains(bcoll_ref->bcoll->name)) {
          continue;
        }

        ED_armature_ebone_select_set(ebone, true);
        changed = true;
        break;
      }
    }

    if (changed) {
      WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
      DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL);
    }
  }
}
static void select_similar_bone_color(bContext *C)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  EditBone *ebone_act = CTX_data_active_bone(C);

  const blender::animrig::BoneColor &active_bone_color = ebone_act->color.wrap();

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *ob : objects) {
    bArmature *arm = static_cast<bArmature *>(ob->data);
    bool changed = false;

    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      if (!EBONE_SELECTABLE(arm, ebone)) {
        continue;
      }

      const blender::animrig::BoneColor &bone_color = ebone->color.wrap();
      if (bone_color != active_bone_color) {
        continue;
      }

      ED_armature_ebone_select_set(ebone, true);
      changed = true;
    }

    if (changed) {
      WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
      DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL);
    }
  }
}

static void select_similar_prefix(bContext *C)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  EditBone *ebone_act = CTX_data_active_bone(C);

  char body_tmp[MAXBONENAME];
  char prefix_act[MAXBONENAME];

  BLI_string_split_prefix(ebone_act->name, sizeof(ebone_act->name), prefix_act, body_tmp);

  if (prefix_act[0] == '\0') {
    return;
  }

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *ob : objects) {
    bArmature *arm = static_cast<bArmature *>(ob->data);
    bool changed = false;

    /* Find matches */
    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      if (EBONE_SELECTABLE(arm, ebone)) {
        char prefix_other[MAXBONENAME];
        BLI_string_split_prefix(ebone->name, sizeof(ebone->name), prefix_other, body_tmp);
        if (STREQ(prefix_act, prefix_other)) {
          ED_armature_ebone_select_set(ebone, true);
          changed = true;
        }
      }
    }

    if (changed) {
      WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
      DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL);
    }
  }
}

static void select_similar_suffix(bContext *C)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  EditBone *ebone_act = CTX_data_active_bone(C);

  char body_tmp[MAXBONENAME];
  char suffix_act[MAXBONENAME];

  BLI_string_split_suffix(ebone_act->name, sizeof(ebone_act->name), body_tmp, suffix_act);

  if (suffix_act[0] == '\0') {
    return;
  }

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *ob : objects) {
    bArmature *arm = static_cast<bArmature *>(ob->data);
    bool changed = false;

    /* Find matches */
    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      if (EBONE_SELECTABLE(arm, ebone)) {
        char suffix_other[MAXBONENAME];
        BLI_string_split_suffix(ebone->name, sizeof(ebone->name), body_tmp, suffix_other);
        if (STREQ(suffix_act, suffix_other)) {
          ED_armature_ebone_select_set(ebone, true);
          changed = true;
        }
      }
    }

    if (changed) {
      WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
      DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL);
    }
  }
}

/** Use for matching any pose channel data. */
static void select_similar_data_pchan(bContext *C, const size_t bytes_size, const int offset)
{
  Object *obedit = CTX_data_edit_object(C);
  bArmature *arm = static_cast<bArmature *>(obedit->data);
  EditBone *ebone_act = CTX_data_active_bone(C);

  const bPoseChannel *pchan_active = BKE_pose_channel_find_name(obedit->pose, ebone_act->name);

  /* This will mostly happen for corner cases where the user tried to access this
   * before having any valid pose data for the armature. */
  if (pchan_active == nullptr) {
    return;
  }

  const char *data_active = (const char *)POINTER_OFFSET(pchan_active, offset);
  LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
    if (EBONE_SELECTABLE(arm, ebone)) {
      const bPoseChannel *pchan = BKE_pose_channel_find_name(obedit->pose, ebone->name);
      if (pchan) {
        const char *data_test = (const char *)POINTER_OFFSET(pchan, offset);
        if (memcmp(data_active, data_test, bytes_size) == 0) {
          ED_armature_ebone_select_set(ebone, true);
        }
      }
    }
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);
  DEG_id_tag_update(&obedit->id, ID_RECALC_SYNC_TO_EVAL);
}

static void is_ancestor(EditBone *bone, EditBone *ancestor)
{
  if (ELEM(bone->temp.ebone, ancestor, nullptr)) {
    return;
  }

  if (!ELEM(bone->temp.ebone->temp.ebone, nullptr, ancestor)) {
    is_ancestor(bone->temp.ebone, ancestor);
  }

  bone->temp.ebone = bone->temp.ebone->temp.ebone;
}

static void select_similar_children(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  bArmature *arm = static_cast<bArmature *>(obedit->data);
  EditBone *ebone_act = CTX_data_active_bone(C);

  LISTBASE_FOREACH (EditBone *, ebone_iter, arm->edbo) {
    ebone_iter->temp.ebone = ebone_iter->parent;
  }

  LISTBASE_FOREACH (EditBone *, ebone_iter, arm->edbo) {
    is_ancestor(ebone_iter, ebone_act);

    if (ebone_iter->temp.ebone == ebone_act && EBONE_SELECTABLE(arm, ebone_iter)) {
      ED_armature_ebone_select_set(ebone_iter, true);
    }
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);
  DEG_id_tag_update(&obedit->id, ID_RECALC_SYNC_TO_EVAL);
}

static void select_similar_children_immediate(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  bArmature *arm = static_cast<bArmature *>(obedit->data);
  EditBone *ebone_act = CTX_data_active_bone(C);

  LISTBASE_FOREACH (EditBone *, ebone_iter, arm->edbo) {
    if (ebone_iter->parent == ebone_act && EBONE_SELECTABLE(arm, ebone_iter)) {
      ED_armature_ebone_select_set(ebone_iter, true);
    }
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);
  DEG_id_tag_update(&obedit->id, ID_RECALC_SYNC_TO_EVAL);
}

static void select_similar_siblings(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  bArmature *arm = static_cast<bArmature *>(obedit->data);
  EditBone *ebone_act = CTX_data_active_bone(C);

  if (ebone_act->parent == nullptr) {
    return;
  }

  LISTBASE_FOREACH (EditBone *, ebone_iter, arm->edbo) {
    if (ebone_iter->parent == ebone_act->parent && EBONE_SELECTABLE(arm, ebone_iter)) {
      ED_armature_ebone_select_set(ebone_iter, true);
    }
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);
  DEG_id_tag_update(&obedit->id, ID_RECALC_SYNC_TO_EVAL);
}

static wmOperatorStatus armature_select_similar_exec(bContext *C, wmOperator *op)
{
  /* Get props */
  int type = RNA_enum_get(op->ptr, "type");
  float thresh = RNA_float_get(op->ptr, "threshold");

  /* Check for active bone */
  if (CTX_data_active_bone(C) == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Operation requires an active bone");
    return OPERATOR_CANCELLED;
  }

#define STRUCT_SIZE_AND_OFFSET(_struct, _member) \
  sizeof(_struct::_member), offsetof(_struct, _member)

  switch (type) {
    case SIMEDBONE_CHILDREN:
      select_similar_children(C);
      break;
    case SIMEDBONE_CHILDREN_IMMEDIATE:
      select_similar_children_immediate(C);
      break;
    case SIMEDBONE_SIBLINGS:
      select_similar_siblings(C);
      break;
    case SIMEDBONE_LENGTH:
      select_similar_length(C, thresh);
      break;
    case SIMEDBONE_DIRECTION:
      select_similar_direction(C, thresh);
      break;
    case SIMEDBONE_PREFIX:
      select_similar_prefix(C);
      break;
    case SIMEDBONE_SUFFIX:
      select_similar_suffix(C);
      break;
    case SIMEDBONE_COLLECTION:
      select_similar_bone_collection(C);
      break;
    case SIMEDBONE_COLOR:
      select_similar_bone_color(C);
      break;
    case SIMEDBONE_SHAPE:
      select_similar_data_pchan(C, STRUCT_SIZE_AND_OFFSET(bPoseChannel, custom));
      break;
  }

#undef STRUCT_SIZE_AND_OFFSET

  ED_outliner_select_sync_from_edit_bone_tag(C);

  return OPERATOR_FINISHED;
}

void ARMATURE_OT_select_similar(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Similar";
  ot->idname = "ARMATURE_OT_select_similar";

  /* callback functions */
  ot->invoke = WM_menu_invoke;
  ot->exec = armature_select_similar_exec;
  ot->poll = ED_operator_editarmature;
  ot->description = "Select similar bones by property types";

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", prop_similar_types, SIMEDBONE_LENGTH, "Type", "");
  RNA_def_float(ot->srna, "threshold", 0.1f, 0.0f, 1.0f, "Threshold", "", 0.0f, 1.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Hierarchy Operator
 * \{ */

/* No need to convert to multi-objects. Just like we keep the non-active bones
 * selected we then keep the non-active objects untouched (selected/unselected). */
static wmOperatorStatus armature_select_hierarchy_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_edit_object(C);
  EditBone *ebone_active;
  int direction = RNA_enum_get(op->ptr, "direction");
  const bool add_to_sel = RNA_boolean_get(op->ptr, "extend");
  bool changed = false;
  bArmature *arm = static_cast<bArmature *>(ob->data);

  ebone_active = arm->act_edbone;
  if (ebone_active == nullptr) {
    return OPERATOR_CANCELLED;
  }

  if (direction == BONE_SELECT_PARENT) {
    if (ebone_active->parent) {
      EditBone *ebone_parent;

      ebone_parent = ebone_active->parent;

      if (EBONE_SELECTABLE(arm, ebone_parent)) {
        arm->act_edbone = ebone_parent;

        if (!add_to_sel) {
          ED_armature_ebone_select_set(ebone_active, false);
        }
        ED_armature_ebone_select_set(ebone_parent, true);

        changed = true;
      }
    }
  }
  else { /* BONE_SELECT_CHILD */
    EditBone *ebone_child = nullptr;
    int pass;

    /* first pass, only connected bones (the logical direct child) */
    for (pass = 0; pass < 2 && (ebone_child == nullptr); pass++) {
      LISTBASE_FOREACH (EditBone *, ebone_iter, arm->edbo) {
        /* possible we have multiple children, some invisible */
        if (EBONE_SELECTABLE(arm, ebone_iter)) {
          if (ebone_iter->parent == ebone_active) {
            if ((pass == 1) || (ebone_iter->flag & BONE_CONNECTED)) {
              ebone_child = ebone_iter;
              break;
            }
          }
        }
      }
    }

    if (ebone_child) {
      arm->act_edbone = ebone_child;

      if (!add_to_sel) {
        ED_armature_ebone_select_set(ebone_active, false);
      }
      ED_armature_ebone_select_set(ebone_child, true);

      changed = true;
    }
  }

  if (changed == false) {
    return OPERATOR_CANCELLED;
  }

  ED_outliner_select_sync_from_edit_bone_tag(C);

  ED_armature_edit_sync_selection(arm->edbo);

  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
  DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL);

  return OPERATOR_FINISHED;
}

void ARMATURE_OT_select_hierarchy(wmOperatorType *ot)
{
  static const EnumPropertyItem direction_items[] = {
      {BONE_SELECT_PARENT, "PARENT", 0, "Select Parent", ""},
      {BONE_SELECT_CHILD, "CHILD", 0, "Select Child", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Select Hierarchy";
  ot->idname = "ARMATURE_OT_select_hierarchy";
  ot->description = "Select immediate parent/children of selected bones";

  /* API callbacks. */
  ot->exec = armature_select_hierarchy_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_enum(ot->srna, "direction", direction_items, BONE_SELECT_PARENT, "Direction", "");
  RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Mirror Operator
 * \{ */

/**
 * \note clone of #pose_select_mirror_exec keep in sync
 */
static wmOperatorStatus armature_select_mirror_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const bool active_only = RNA_boolean_get(op->ptr, "only_active");
  const bool extend = RNA_boolean_get(op->ptr, "extend");

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *ob : objects) {
    bArmature *arm = static_cast<bArmature *>(ob->data);

    EditBone *ebone_mirror_act = nullptr;

    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      const int flag = ED_armature_ebone_selectflag_get(ebone);
      EBONE_PREV_FLAG_SET(ebone, flag);
    }

    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      if (EBONE_SELECTABLE(arm, ebone)) {
        EditBone *ebone_mirror;
        int flag_new = extend ? EBONE_PREV_FLAG_GET(ebone) : 0;

        if ((ebone_mirror = ED_armature_ebone_get_mirrored(arm->edbo, ebone)) &&
            blender::animrig::bone_is_visible(arm, ebone_mirror))
        {
          const int flag_mirror = EBONE_PREV_FLAG_GET(ebone_mirror);
          flag_new |= flag_mirror;

          if (ebone == arm->act_edbone) {
            ebone_mirror_act = ebone_mirror;
          }

          /* skip all but the active or its mirror */
          if (active_only && !ELEM(arm->act_edbone, ebone, ebone_mirror)) {
            continue;
          }
        }

        ED_armature_ebone_selectflag_set(ebone, flag_new);
      }
    }

    if (ebone_mirror_act) {
      arm->act_edbone = ebone_mirror_act;
    }

    ED_outliner_select_sync_from_edit_bone_tag(C);

    ED_armature_edit_sync_selection(arm->edbo);

    WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
    DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL);
  }

  return OPERATOR_FINISHED;
}

void ARMATURE_OT_select_mirror(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Mirror";
  ot->idname = "ARMATURE_OT_select_mirror";
  ot->description = "Mirror the bone selection";

  /* API callbacks. */
  ot->exec = armature_select_mirror_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(
      ot->srna, "only_active", false, "Active Only", "Only operate on the active bone");
  RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Path Operator
 * \{ */

static bool armature_shortest_path_select(
    bArmature *arm, EditBone *ebone_parent, EditBone *ebone_child, bool use_parent, bool is_test)
{
  do {

    if (!use_parent && (ebone_child == ebone_parent)) {
      break;
    }

    if (is_test) {
      if (!EBONE_SELECTABLE(arm, ebone_child)) {
        return false;
      }
    }
    else {
      ED_armature_ebone_selectflag_set(ebone_child, (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL));
    }

    if (ebone_child == ebone_parent) {
      break;
    }

    ebone_child = ebone_child->parent;
  } while (true);

  return true;
}

static wmOperatorStatus armature_shortest_path_pick_invoke(bContext *C,
                                                           wmOperator *op,
                                                           const wmEvent *event)
{
  Object *obedit = CTX_data_edit_object(C);
  bArmature *arm = static_cast<bArmature *>(obedit->data);
  EditBone *ebone_src, *ebone_dst;
  EditBone *ebone_isect_parent = nullptr;
  EditBone *ebone_isect_child[2];
  bool changed;
  Base *base_dst = nullptr;

  view3d_operator_needs_gpu(C);
  BKE_object_update_select_id(CTX_data_main(C));

  ebone_src = arm->act_edbone;
  ebone_dst = ED_armature_pick_ebone(C, event->mval, false, &base_dst);

  /* fall back to object selection */
  if (ELEM(nullptr, ebone_src, ebone_dst) || (ebone_src == ebone_dst)) {
    return OPERATOR_PASS_THROUGH;
  }

  if (base_dst && base_dst->object != obedit) {
    /* Disconnected, ignore. */
    return OPERATOR_CANCELLED;
  }

  ebone_isect_child[0] = ebone_src;
  ebone_isect_child[1] = ebone_dst;

  /* ensure 'ebone_src' is the parent of 'ebone_dst', or set 'ebone_isect_parent' */
  if (ED_armature_ebone_is_child_recursive(ebone_src, ebone_dst)) {
    /* pass */
  }
  else if (ED_armature_ebone_is_child_recursive(ebone_dst, ebone_src)) {
    std::swap(ebone_src, ebone_dst);
  }
  else if ((ebone_isect_parent = ED_armature_ebone_find_shared_parent(ebone_isect_child, 2))) {
    /* pass */
  }
  else {
    /* disconnected bones */
    return OPERATOR_CANCELLED;
  }

  if (ebone_isect_parent) {
    if (armature_shortest_path_select(arm, ebone_isect_parent, ebone_src, false, true) &&
        armature_shortest_path_select(arm, ebone_isect_parent, ebone_dst, false, true))
    {
      armature_shortest_path_select(arm, ebone_isect_parent, ebone_src, false, false);
      armature_shortest_path_select(arm, ebone_isect_parent, ebone_dst, false, false);
      changed = true;
    }
    else {
      /* unselectable */
      changed = false;
    }
  }
  else {
    if (armature_shortest_path_select(arm, ebone_src, ebone_dst, true, true)) {
      armature_shortest_path_select(arm, ebone_src, ebone_dst, true, false);
      changed = true;
    }
    else {
      /* unselectable */
      changed = false;
    }
  }

  if (changed) {
    arm->act_edbone = ebone_dst;
    ED_outliner_select_sync_from_edit_bone_tag(C);
    ED_armature_edit_sync_selection(arm->edbo);
    WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);
    DEG_id_tag_update(&obedit->id, ID_RECALC_SYNC_TO_EVAL);

    return OPERATOR_FINISHED;
  }

  BKE_report(op->reports, RPT_WARNING, "Unselectable bone in chain");
  return OPERATOR_CANCELLED;
}

void ARMATURE_OT_shortest_path_pick(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Pick Shortest Path";
  ot->idname = "ARMATURE_OT_shortest_path_pick";
  ot->description = "Select shortest path between two bones";

  /* API callbacks. */
  ot->invoke = armature_shortest_path_pick_invoke;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */
