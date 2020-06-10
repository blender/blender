/*
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
 * API's and Operators for selecting armature bones in EditMode
 */

/** \file
 * \ingroup edarmature
 */

#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_string_utils.h"

#include "BKE_action.h"
#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_object.h"
#include "BKE_report.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_armature.h"
#include "ED_object.h"
#include "ED_outliner.h"
#include "ED_screen.h"
#include "ED_select_utils.h"
#include "ED_view3d.h"

#include "DEG_depsgraph.h"

#include "armature_intern.h"

/* utility macros for storing a temp int in the bone (selection flag) */
#define EBONE_PREV_FLAG_GET(ebone) ((void)0, (ebone)->temp.i)
#define EBONE_PREV_FLAG_SET(ebone, val) ((ebone)->temp.i = val)

/* -------------------------------------------------------------------- */
/** \name Select Buffer Queries for PoseMode & EditMode
 * \{ */

Base *ED_armature_base_and_ebone_from_select_buffer(Base **bases,
                                                    uint bases_len,
                                                    int hit,
                                                    EditBone **r_ebone)
{
  const uint hit_object = hit & 0xFFFF;
  Base *base = NULL;
  EditBone *ebone = NULL;
  /* TODO(campbell): optimize, eg: sort & binary search. */
  for (uint base_index = 0; base_index < bases_len; base_index++) {
    if (bases[base_index]->object->runtime.select_id == hit_object) {
      base = bases[base_index];
      break;
    }
  }
  if (base != NULL) {
    const uint hit_bone = (hit & ~BONESEL_ANY) >> 16;
    bArmature *arm = base->object->data;
    ebone = BLI_findlink(arm->edbo, hit_bone);
  }
  *r_ebone = ebone;
  return base;
}

Object *ED_armature_object_and_ebone_from_select_buffer(Object **objects,
                                                        uint objects_len,
                                                        int hit,
                                                        EditBone **r_ebone)
{
  const uint hit_object = hit & 0xFFFF;
  Object *ob = NULL;
  EditBone *ebone = NULL;
  /* TODO(campbell): optimize, eg: sort & binary search. */
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    if (objects[ob_index]->runtime.select_id == hit_object) {
      ob = objects[ob_index];
      break;
    }
  }
  if (ob != NULL) {
    const uint hit_bone = (hit & ~BONESEL_ANY) >> 16;
    bArmature *arm = ob->data;
    ebone = BLI_findlink(arm->edbo, hit_bone);
  }
  *r_ebone = ebone;
  return ob;
}

Base *ED_armature_base_and_pchan_from_select_buffer(Base **bases,
                                                    uint bases_len,
                                                    int hit,
                                                    bPoseChannel **r_pchan)
{
  const uint hit_object = hit & 0xFFFF;
  Base *base = NULL;
  bPoseChannel *pchan = NULL;
  /* TODO(campbell): optimize, eg: sort & binary search. */
  for (uint base_index = 0; base_index < bases_len; base_index++) {
    if (bases[base_index]->object->runtime.select_id == hit_object) {
      base = bases[base_index];
      break;
    }
  }
  if (base != NULL) {
    if (base->object->pose != NULL) {
      const uint hit_bone = (hit & ~BONESEL_ANY) >> 16;
      /* pchan may be NULL. */
      pchan = BLI_findlink(&base->object->pose->chanbase, hit_bone);
    }
  }
  *r_pchan = pchan;
  return base;
}

/* For callers that don't need the pose channel. */
Base *ED_armature_base_and_bone_from_select_buffer(Base **bases,
                                                   uint bases_len,
                                                   int hit,
                                                   Bone **r_bone)
{
  bPoseChannel *pchan = NULL;
  Base *base = ED_armature_base_and_pchan_from_select_buffer(bases, bases_len, hit, &pchan);
  *r_bone = pchan ? pchan->bone : NULL;
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
                                                          Base **bases,
                                                          uint bases_len,
                                                          const uint *buffer,
                                                          short hits,
                                                          bool findunsel,
                                                          bool do_nearest,
                                                          Base **r_base)
{
  bPoseChannel *pchan;
  EditBone *ebone;
  void *firstunSel = NULL, *firstSel = NULL, *data;
  Base *firstunSel_base = NULL, *firstSel_base = NULL;
  uint hitresult;
  short i;
  bool takeNext = false;
  int minsel = 0xffffffff, minunsel = 0xffffffff;

  for (i = 0; i < hits; i++) {
    hitresult = buffer[3 + (i * 4)];

    if (hitresult & BONESEL_ANY) { /* to avoid including objects in selection */
      Base *base = NULL;
      bool sel;

      hitresult &= ~(BONESEL_ANY);
      /* Determine what the current bone is */
      if (is_editmode == false) {
        base = ED_armature_base_and_pchan_from_select_buffer(bases, bases_len, hitresult, &pchan);
        if (pchan != NULL) {
          if (findunsel) {
            sel = (pchan->bone->flag & BONE_SELECTED);
          }
          else {
            sel = !(pchan->bone->flag & BONE_SELECTED);
          }

          data = pchan;
        }
        else {
          data = NULL;
          sel = 0;
        }
      }
      else {
        base = ED_armature_base_and_ebone_from_select_buffer(bases, bases_len, hitresult, &ebone);
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
            if (minsel > buffer[4 * i + 1]) {
              firstSel = data;
              firstSel_base = base;
              minsel = buffer[4 * i + 1];
            }
          }
          else {
            if (!firstSel) {
              firstSel = data;
              firstSel_base = base;
            }
            takeNext = 1;
          }
        }
        else {
          if (do_nearest) {
            if (minunsel > buffer[4 * i + 1]) {
              firstunSel = data;
              firstunSel_base = base;
              minunsel = buffer[4 * i + 1];
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
  else {
    *r_base = firstSel_base;
    return firstSel;
  }
}

EditBone *ED_armature_pick_ebone_from_selectbuffer(Base **bases,
                                                   uint bases_len,
                                                   const uint *buffer,
                                                   short hits,
                                                   bool findunsel,
                                                   bool do_nearest,
                                                   Base **r_base)
{
  const bool is_editmode = true;
  return ed_armature_pick_bone_from_selectbuffer_impl(
      is_editmode, bases, bases_len, buffer, hits, findunsel, do_nearest, r_base);
}

bPoseChannel *ED_armature_pick_pchan_from_selectbuffer(Base **bases,
                                                       uint bases_len,
                                                       const uint *buffer,
                                                       short hits,
                                                       bool findunsel,
                                                       bool do_nearest,
                                                       Base **r_base)
{
  const bool is_editmode = false;
  return ed_armature_pick_bone_from_selectbuffer_impl(
      is_editmode, bases, bases_len, buffer, hits, findunsel, do_nearest, r_base);
}

Bone *ED_armature_pick_bone_from_selectbuffer(Base **bases,
                                              uint bases_len,
                                              const uint *buffer,
                                              short hits,
                                              bool findunsel,
                                              bool do_nearest,
                                              Base **r_base)
{
  bPoseChannel *pchan = ED_armature_pick_pchan_from_selectbuffer(
      bases, bases_len, buffer, hits, findunsel, do_nearest, r_base);
  return pchan ? pchan->bone : NULL;
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
  ViewContext vc;
  rcti rect;
  uint buffer[MAXPICKBUF];
  short hits;

  ED_view3d_viewcontext_init(C, &vc, depsgraph);
  BLI_assert((vc.obedit != NULL) == is_editmode);

  BLI_rcti_init_pt_radius(&rect, xy, 0);

  hits = view3d_opengl_select(
      &vc, buffer, MAXPICKBUF, &rect, VIEW3D_SELECT_PICK_NEAREST, VIEW3D_SELECT_FILTER_NOP);

  *r_base = NULL;

  if (hits > 0) {
    uint bases_len = 0;
    Base **bases;

    if (vc.obedit != NULL) {
      bases = BKE_view_layer_array_from_bases_in_mode(vc.view_layer,
                                                      vc.v3d,
                                                      &bases_len,
                                                      {
                                                          .object_mode = OB_MODE_EDIT,
                                                      });
    }
    else {
      bases = BKE_object_pose_base_array_get(vc.view_layer, vc.v3d, &bases_len);
    }

    void *bone = ed_armature_pick_bone_from_selectbuffer_impl(
        is_editmode, bases, bases_len, buffer, hits, findunsel, true, r_base);

    MEM_freeN(bases);

    return bone;
  }
  return NULL;
}

EditBone *ED_armature_pick_ebone(bContext *C, const int xy[2], bool findunsel, Base **r_base)
{
  const bool is_editmode = true;
  return ed_armature_pick_bone_impl(is_editmode, C, xy, findunsel, r_base);
}

bPoseChannel *ED_armature_pick_pchan(bContext *C, const int xy[2], bool findunsel, Base **r_base)
{
  const bool is_editmode = false;
  return ed_armature_pick_bone_impl(is_editmode, C, xy, findunsel, r_base);
}

Bone *ED_armature_pick_bone(bContext *C, const int xy[2], bool findunsel, Base **r_base)
{
  bPoseChannel *pchan = ED_armature_pick_pchan(C, xy, findunsel, r_base);
  return pchan ? pchan->bone : NULL;
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
  bArmature *arm = ob->data;

  /* Implementation note, this flood-fills selected bones with the 'TOUCH' flag,
   * even though this is a loop-within a loop, walking up the parent chain only touches new bones.
   * Bones that have been touched are skipped, so the complexity is OK. */

  enum {
    /* Bone has been walked over, it's LINK value can be read. */
    TOUCH = (1 << 0),
    /* When TOUCH has been set, this flag can be checked to see if the bone is connected. */
    LINK = (1 << 1),
  };

#define CHECK_PARENT(ebone) \
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
    for (EditBone *ebone = ebone_iter; ebone; ebone = CHECK_PARENT(ebone) ? ebone->parent : NULL) {
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
    EditBone *ebone_touched_parent = NULL;
    for (EditBone *ebone = ebone_iter; ebone; ebone = CHECK_PARENT(ebone) ? ebone->parent : NULL) {
      if (ebone->temp.i & TOUCH) {
        ebone_touched_parent = ebone;
        break;
      }
      ebone->temp.i |= TOUCH;
    }

    if ((ebone_touched_parent != NULL) && (ebone_touched_parent->temp.i & LINK)) {
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
    DEG_id_tag_update(&arm->id, ID_RECALC_COPY_ON_WRITE);
    WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, ob);
  }

  return changed;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Linked Operator
 * \{ */

static int armature_select_linked_exec(bContext *C, wmOperator *op)
{
  const bool all_forks = RNA_boolean_get(op->ptr, "all_forks");

  bool changed_multi = false;
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    bArmature *arm = ob->data;

    bool found = false;
    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      if (EBONE_VISIBLE(arm, ebone) &&
          (ebone->flag & (BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL))) {
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
  MEM_freeN(objects);

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

  /* api callbacks */
  ot->exec = armature_select_linked_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Leave disabled by default as this matches pose mode. */
  RNA_def_boolean(ot->srna, "all_forks", 0, "All Forks", "Follow forks in the parents chain");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Linked (Cursor Pick) Operator
 * \{ */

static int armature_select_linked_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const bool select = !RNA_boolean_get(op->ptr, "deselect");
  const bool all_forks = RNA_boolean_get(op->ptr, "all_forks");

  view3d_operator_needs_opengl(C);
  BKE_object_update_select_id(CTX_data_main(C));

  Base *base = NULL;
  EditBone *ebone_active = ED_armature_pick_ebone(C, event->mval, true, &base);

  if (ebone_active == NULL) {
    return OPERATOR_CANCELLED;
  }

  bArmature *arm = base->object->data;
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

  /* api callbacks */
  /* leave 'exec' unset */
  ot->invoke = armature_select_linked_pick_invoke;
  ot->poll = armature_select_linked_pick_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "deselect", 0, "Deselect", "");
  /* Leave disabled by default as this matches pose mode. */
  RNA_def_boolean(ot->srna, "all_forks", 0, "All Forks", "Follow forks in the parents chain");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Buffer Queries EditMode
 * \{ */

/* utility function for get_nearest_editbonepoint */
static int selectbuffer_ret_hits_12(uint *UNUSED(buffer), const int hits12)
{
  return hits12;
}

static int selectbuffer_ret_hits_5(uint *buffer, const int hits12, const int hits5)
{
  const int offs = 4 * hits12;
  memcpy(buffer, buffer + offs, 4 * hits5 * sizeof(uint));
  return hits5;
}

/* does bones and points */
/* note that BONE ROOT only gets drawn for root bones (or without IK) */
static EditBone *get_nearest_editbonepoint(
    ViewContext *vc, bool findunsel, bool use_cycle, Base **r_base, int *r_selmask)
{
  uint buffer[MAXPICKBUF];
  struct {
    uint hitresult;
    Base *base;
    EditBone *ebone;
  } *result = NULL,

    result_cycle = {.hitresult = -1, .base = NULL, .ebone = NULL},
    result_bias = {.hitresult = -1, .base = NULL, .ebone = NULL};

  /* find the bone after the current active bone, so as to bump up its chances in selection.
   * this way overlapping bones will cycle selection state as with objects. */
  Object *obedit_orig = vc->obedit;
  EditBone *ebone_active_orig = ((bArmature *)obedit_orig->data)->act_edbone;
  if (ebone_active_orig == NULL) {
    use_cycle = false;
  }

  if (use_cycle) {
    static int last_mval[2] = {-100, -100};
    if ((len_manhattan_v2v2_int(vc->mval, last_mval) <= WM_EVENT_CURSOR_MOTION_THRESHOLD) == 0) {
      use_cycle = false;
    }
    copy_v2_v2_int(last_mval, vc->mval);
  }

  const bool do_nearest = !(XRAY_ACTIVE(vc->v3d) || use_cycle);

  /* matching logic from 'mixed_bones_object_selectbuffer' */
  int hits = 0;
  /* Don't use hits with this ID, (armature drawing uses this). */
  const int select_id_ignore = -1;

  /* we _must_ end cache before return, use 'goto cache_end' */
  view3d_opengl_select_cache_begin();

  {
    const int select_mode = (do_nearest ? VIEW3D_SELECT_PICK_NEAREST : VIEW3D_SELECT_PICK_ALL);
    const eV3DSelectObjectFilter select_filter = VIEW3D_SELECT_FILTER_NOP;

    rcti rect;
    BLI_rcti_init_pt_radius(&rect, vc->mval, 12);
    const int hits12 = view3d_opengl_select_with_id_filter(
        vc, buffer, MAXPICKBUF, &rect, select_mode, select_filter, select_id_ignore);

    if (hits12 == 1) {
      hits = selectbuffer_ret_hits_12(buffer, hits12);
      goto cache_end;
    }
    else if (hits12 > 0) {
      int offs;

      offs = 4 * hits12;
      BLI_rcti_init_pt_radius(&rect, vc->mval, 5);
      const int hits5 = view3d_opengl_select_with_id_filter(vc,
                                                            buffer + offs,
                                                            MAXPICKBUF - offs,
                                                            &rect,
                                                            select_mode,
                                                            select_filter,
                                                            select_id_ignore);

      if (hits5 == 1) {
        hits = selectbuffer_ret_hits_5(buffer, hits12, hits5);
        goto cache_end;
      }

      if (hits5 > 0) {
        hits = selectbuffer_ret_hits_5(buffer, hits12, hits5);
        goto cache_end;
      }
      else {
        hits = selectbuffer_ret_hits_12(buffer, hits12);
        goto cache_end;
      }
    }
  }

cache_end:
  view3d_opengl_select_cache_end();

  uint bases_len;
  Base **bases = BKE_view_layer_array_from_bases_in_edit_mode_unique_data(
      vc->view_layer, vc->v3d, &bases_len);

  /* See if there are any selected bones in this group */
  if (hits > 0) {
    if (hits == 1) {
      result_bias.hitresult = buffer[3];
      result_bias.base = ED_armature_base_and_ebone_from_select_buffer(
          bases, bases_len, result_bias.hitresult, &result_bias.ebone);
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
#ifdef __BIG_ENDIAN__
            uint16_t ob;
            uint16_t bone;
#else
            uint16_t bone;
            uint16_t ob;
#endif
          };
        } offset, test, best;
      } cycle_order;

      if (use_cycle) {
        bArmature *arm = obedit_orig->data;
        int ob_index = obedit_orig->runtime.select_id & 0xFFFF;
        int bone_index = BLI_findindex(arm->edbo, ebone_active_orig);
        /* Offset from the current active bone, so we cycle onto the next. */
        cycle_order.offset.ob = ob_index;
        cycle_order.offset.bone = bone_index;
        /* The value of the active bone (with offset subtracted, a signal to always overwrite). */
        cycle_order.best.as_u32 = 0;
      }

      for (int i = 0; i < hits; i++) {
        const uint hitresult = buffer[3 + (i * 4)];

        Base *base = NULL;
        EditBone *ebone;
        base = ED_armature_base_and_ebone_from_select_buffer(bases, bases_len, hitresult, &ebone);
        /* If this fails, selection code is setting the selection ID's incorrectly. */
        BLI_assert(base && ebone);

        /* Prioritized selection. */
        {
          int bias;
          /* clicks on bone points get advantage */
          if (hitresult & (BONESEL_ROOT | BONESEL_TIP)) {
            /* but also the unselected one */
            if (findunsel) {
              if ((hitresult & BONESEL_ROOT) && (ebone->flag & BONE_ROOTSEL) == 0) {
                bias = 4;
              }
              else if ((hitresult & BONESEL_TIP) && (ebone->flag & BONE_TIPSEL) == 0) {
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

            result_bias.hitresult = hitresult;
            result_bias.base = base;
            result_bias.ebone = ebone;
          }
        }

        /* Cycle selected items (objects & bones). */
        if (use_cycle) {
          cycle_order.test.ob = hitresult & 0xFFFF;
          cycle_order.test.bone = (hitresult & ~BONESEL_ANY) >> 16;
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
              (cycle_order.test.as_u32 && (cycle_order.test.as_u32 < cycle_order.best.as_u32))) {
            cycle_order.best = cycle_order.test;
            result_cycle.hitresult = hitresult;
            result_cycle.base = base;
            result_cycle.ebone = ebone;
          }
        }
      }
    }

    result = (use_cycle && result_cycle.ebone) ? &result_cycle : &result_bias;

    if (result->hitresult != -1) {
      *r_base = result->base;

      *r_selmask = 0;
      if (result->hitresult & BONESEL_ROOT) {
        *r_selmask |= BONE_ROOTSEL;
      }
      if (result->hitresult & BONESEL_TIP) {
        *r_selmask |= BONE_TIPSEL;
      }
      if (result->hitresult & BONESEL_BONE) {
        *r_selmask |= BONE_SELECTED;
      }
      MEM_freeN(bases);
      return result->ebone;
    }
  }
  *r_selmask = 0;
  *r_base = NULL;
  MEM_freeN(bases);
  return NULL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Utility Functions
 * \{ */

bool ED_armature_edit_deselect_all(Object *obedit)
{
  bArmature *arm = obedit->data;
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
  bArmature *arm = obedit->data;
  bool changed = false;
  LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
    /* first and foremost, bone must be visible and selected */
    if (EBONE_VISIBLE(arm, ebone)) {
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

bool ED_armature_edit_deselect_all_multi_ex(struct Base **bases, uint bases_len)
{
  bool changed_multi = false;
  for (uint base_index = 0; base_index < bases_len; base_index++) {
    Object *obedit = bases[base_index]->object;
    changed_multi |= ED_armature_edit_deselect_all(obedit);
  }
  return changed_multi;
}

bool ED_armature_edit_deselect_all_visible_multi_ex(struct Base **bases, uint bases_len)
{
  bool changed_multi = false;
  for (uint base_index = 0; base_index < bases_len; base_index++) {
    Object *obedit = bases[base_index]->object;
    changed_multi |= ED_armature_edit_deselect_all_visible(obedit);
  }
  return changed_multi;
}

bool ED_armature_edit_deselect_all_visible_multi(bContext *C)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewContext vc;
  ED_view3d_viewcontext_init(C, &vc, depsgraph);
  uint bases_len = 0;
  Base **bases = BKE_view_layer_array_from_bases_in_edit_mode_unique_data(
      vc.view_layer, vc.v3d, &bases_len);
  bool changed_multi = ED_armature_edit_deselect_all_multi_ex(bases, bases_len);
  MEM_freeN(bases);
  return changed_multi;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Cursor Pick API
 * \{ */

/* context: editmode armature in view3d */
bool ED_armature_edit_select_pick(
    bContext *C, const int mval[2], bool extend, bool deselect, bool toggle)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewContext vc;
  EditBone *nearBone = NULL;
  int selmask;
  Base *basact = NULL;

  ED_view3d_viewcontext_init(C, &vc, depsgraph);
  vc.mval[0] = mval[0];
  vc.mval[1] = mval[1];

  nearBone = get_nearest_editbonepoint(&vc, true, true, &basact, &selmask);
  if (nearBone) {
    ED_view3d_viewcontext_init_object(&vc, basact->object);
    bArmature *arm = vc.obedit->data;

    if (!EBONE_SELECTABLE(arm, nearBone)) {
      return false;
    }

    if (!extend && !deselect && !toggle) {
      uint bases_len = 0;
      Base **bases = BKE_view_layer_array_from_bases_in_edit_mode_unique_data(
          vc.view_layer, vc.v3d, &bases_len);
      ED_armature_edit_deselect_all_multi_ex(bases, bases_len);
      MEM_freeN(bases);
    }

    /* by definition the non-root connected bones have no root point drawn,
     * so a root selection needs to be delivered to the parent tip */

    if (selmask & BONE_SELECTED) {
      if (nearBone->parent && (nearBone->flag & BONE_CONNECTED)) {
        /* click in a chain */
        if (extend) {
          /* select this bone */
          nearBone->flag |= BONE_TIPSEL;
          nearBone->parent->flag |= BONE_TIPSEL;
        }
        else if (deselect) {
          /* deselect this bone */
          nearBone->flag &= ~(BONE_TIPSEL | BONE_SELECTED);
          /* only deselect parent tip if it is not selected */
          if (!(nearBone->parent->flag & BONE_SELECTED)) {
            nearBone->parent->flag &= ~BONE_TIPSEL;
          }
        }
        else if (toggle) {
          /* hold shift inverts this bone's selection */
          if (nearBone->flag & BONE_SELECTED) {
            /* deselect this bone */
            nearBone->flag &= ~(BONE_TIPSEL | BONE_SELECTED);
            /* only deselect parent tip if it is not selected */
            if (!(nearBone->parent->flag & BONE_SELECTED)) {
              nearBone->parent->flag &= ~BONE_TIPSEL;
            }
          }
          else {
            /* select this bone */
            nearBone->flag |= BONE_TIPSEL;
            nearBone->parent->flag |= BONE_TIPSEL;
          }
        }
        else {
          /* select this bone */
          nearBone->flag |= BONE_TIPSEL;
          nearBone->parent->flag |= BONE_TIPSEL;
        }
      }
      else {
        if (extend) {
          nearBone->flag |= (BONE_TIPSEL | BONE_ROOTSEL);
        }
        else if (deselect) {
          nearBone->flag &= ~(BONE_TIPSEL | BONE_ROOTSEL);
        }
        else if (toggle) {
          /* hold shift inverts this bone's selection */
          if (nearBone->flag & BONE_SELECTED) {
            nearBone->flag &= ~(BONE_TIPSEL | BONE_ROOTSEL);
          }
          else {
            nearBone->flag |= (BONE_TIPSEL | BONE_ROOTSEL);
          }
        }
        else {
          nearBone->flag |= (BONE_TIPSEL | BONE_ROOTSEL);
        }
      }
    }
    else {
      if (extend) {
        nearBone->flag |= selmask;
      }
      else if (deselect) {
        nearBone->flag &= ~selmask;
      }
      else if (toggle && (nearBone->flag & selmask)) {
        nearBone->flag &= ~selmask;
      }
      else {
        nearBone->flag |= selmask;
      }
    }

    ED_armature_edit_sync_selection(arm->edbo);

    /* then now check for active status */
    if (ED_armature_ebone_selectflag_get(nearBone)) {
      arm->act_edbone = nearBone;
    }

    if (vc.view_layer->basact != basact) {
      ED_object_base_activate(C, basact);
    }

    WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, basact->object);
    DEG_id_tag_update(&arm->id, ID_RECALC_COPY_ON_WRITE);
    return true;
  }

  return false;
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
  BLI_assert(EBONE_VISIBLE(arm, ebone));
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

/**
 * Perform a selection operation on elements which have been 'touched',
 * use for lasso & border select but can be used elsewhere too.
 *
 * Tagging is done via #EditBone.temp.i using: #BONESEL_ROOT, #BONESEL_TIP, #BONESEL_BONE
 * And optionally ignoring end-points using the #BONESEL_ROOT, #BONESEL_TIP right shifted 16 bits.
 * (used when the values are clipped outside the view).
 *
 * \param sel_op: #eSelectOp type.
 *
 * \note Visibility checks must be done by the caller.
 */
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
          ELEM(is_inside_flag & (BONESEL_ROOT | BONESEL_TIP), BONESEL_ROOT, BONESEL_TIP)) {
        is_inside_flag &= ~BONESEL_BONE;
      }

      changed |= armature_edit_select_op_apply(arm, ebone, sel_op, is_ignore_flag, is_inside_flag);
    }
  }

  if (changed) {
    /* Cleanup flags. */
    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      if (ebone->flag & BONE_DONE) {
        SWAP(int, ebone->temp.i, ebone->flag);
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
    ED_armature_edit_validate_active(arm);
  }

  return changed;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name (De)Select All Operator
 * \{ */

static int armature_de_select_all_exec(bContext *C, wmOperator *op)
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
          if (ebone->parent) {
            ebone->parent->flag |= (BONE_TIPSEL);
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
            if (ebone->parent) {
              ebone->parent->flag |= (BONE_TIPSEL);
            }
          }
        }
        break;
    }
  }
  CTX_DATA_END;

  ED_outliner_select_sync_from_edit_bone_tag(C);

  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, NULL);

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

  /* api callbacks */
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

static void armature_select_less(bArmature *UNUSED(arm), EditBone *ebone)
{
  if ((EBONE_PREV_FLAG_GET(ebone) & (BONE_ROOTSEL | BONE_TIPSEL)) !=
      (BONE_ROOTSEL | BONE_TIPSEL)) {
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
  bArmature *arm = (bArmature *)ob->data;
  EditBone *ebone;

  /* XXX, eventually we shouldn't need this - campbell */
  ED_armature_edit_sync_selection(arm->edbo);

  /* count bones & store selection state */
  for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
    EBONE_PREV_FLAG_SET(ebone, ED_armature_ebone_selectflag_get(ebone));
  }

  /* do selection */
  for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
    if (EBONE_VISIBLE(arm, ebone)) {
      if (more) {
        armature_select_more(arm, ebone);
      }
      else {
        armature_select_less(arm, ebone);
      }
    }
  }

  for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
    if (EBONE_VISIBLE(arm, ebone)) {
      if (more == false) {
        if (ebone->flag & BONE_SELECTED) {
          ED_armature_ebone_select_set(ebone, true);
        }
      }
    }
    ebone->temp.p = NULL;
  }

  ED_armature_edit_sync_selection(arm->edbo);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select More Operator
 * \{ */

static int armature_de_select_more_exec(bContext *C, wmOperator *UNUSED(op))
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    armature_select_more_less(ob, true);
    WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
    DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
  }
  MEM_freeN(objects);

  ED_outliner_select_sync_from_edit_bone_tag(C);
  return OPERATOR_FINISHED;
}

void ARMATURE_OT_select_more(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select More";
  ot->idname = "ARMATURE_OT_select_more";
  ot->description = "Select those bones connected to the initial selection";

  /* api callbacks */
  ot->exec = armature_de_select_more_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Less Operator
 * \{ */

static int armature_de_select_less_exec(bContext *C, wmOperator *UNUSED(op))
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    armature_select_more_less(ob, false);
    WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
    DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
  }
  MEM_freeN(objects);

  ED_outliner_select_sync_from_edit_bone_tag(C);
  return OPERATOR_FINISHED;
}

void ARMATURE_OT_select_less(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Less";
  ot->idname = "ARMATURE_OT_select_less";
  ot->description = "Deselect those bones at the boundary of each selection region";

  /* api callbacks */
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
  SIMEDBONE_LAYER,
  SIMEDBONE_GROUP,
  SIMEDBONE_SHAPE,
};

static const EnumPropertyItem prop_similar_types[] = {
    {SIMEDBONE_CHILDREN, "CHILDREN", 0, "Children", ""},
    {SIMEDBONE_CHILDREN_IMMEDIATE, "CHILDREN_IMMEDIATE", 0, "Immediate children", ""},
    {SIMEDBONE_SIBLINGS, "SIBLINGS", 0, "Siblings", ""},
    {SIMEDBONE_LENGTH, "LENGTH", 0, "Length", ""},
    {SIMEDBONE_DIRECTION, "DIRECTION", 0, "Direction (Y axis)", ""},
    {SIMEDBONE_PREFIX, "PREFIX", 0, "Prefix", ""},
    {SIMEDBONE_SUFFIX, "SUFFIX", 0, "Suffix", ""},
    {SIMEDBONE_LAYER, "LAYER", 0, "Layer", ""},
    {SIMEDBONE_GROUP, "GROUP", 0, "Group", ""},
    {SIMEDBONE_SHAPE, "SHAPE", 0, "Shape", ""},
    {0, NULL, 0, NULL, NULL},
};

static float bone_length_squared_worldspace_get(Object *ob, EditBone *ebone)
{
  float v1[3], v2[3];
  mul_v3_mat3_m4v3(v1, ob->obmat, ebone->head);
  mul_v3_mat3_m4v3(v2, ob->obmat, ebone->tail);
  return len_squared_v3v3(v1, v2);
}

static void select_similar_length(bContext *C, const float thresh)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob_act = CTX_data_edit_object(C);
  EditBone *ebone_act = CTX_data_active_bone(C);

  /* Thresh is always relative to current length. */
  const float len = bone_length_squared_worldspace_get(ob_act, ebone_act);
  const float len_min = len / (1.0f + (thresh - FLT_EPSILON));
  const float len_max = len * (1.0f + (thresh + FLT_EPSILON));

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    bArmature *arm = ob->data;
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
      DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
    }
  }
  MEM_freeN(objects);
}

static void bone_direction_worldspace_get(Object *ob, EditBone *ebone, float *r_dir)
{
  float v1[3], v2[3];
  copy_v3_v3(v1, ebone->head);
  copy_v3_v3(v2, ebone->tail);

  mul_m4_v3(ob->obmat, v1);
  mul_m4_v3(ob->obmat, v2);

  sub_v3_v3v3(r_dir, v1, v2);
  normalize_v3(r_dir);
}

static void select_similar_direction(bContext *C, const float thresh)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob_act = CTX_data_edit_object(C);
  EditBone *ebone_act = CTX_data_active_bone(C);

  float dir_act[3];
  bone_direction_worldspace_get(ob_act, ebone_act, dir_act);

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    bArmature *arm = ob->data;
    bool changed = false;

    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      if (EBONE_SELECTABLE(arm, ebone)) {
        float dir[3];
        bone_direction_worldspace_get(ob, ebone, dir);

        if (angle_v3v3(dir_act, dir) / (float)M_PI < (thresh + FLT_EPSILON)) {
          ED_armature_ebone_select_set(ebone, true);
          changed = true;
        }
      }
    }

    if (changed) {
      WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
      DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
      DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
    }
  }
  MEM_freeN(objects);
}

static void select_similar_layer(bContext *C)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  EditBone *ebone_act = CTX_data_active_bone(C);

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    bArmature *arm = ob->data;
    bool changed = false;

    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      if (EBONE_SELECTABLE(arm, ebone)) {
        if (ebone->layer & ebone_act->layer) {
          ED_armature_ebone_select_set(ebone, true);
          changed = true;
        }
      }
    }

    if (changed) {
      WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
      DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
    }
  }
  MEM_freeN(objects);
}

static void select_similar_prefix(bContext *C)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  EditBone *ebone_act = CTX_data_active_bone(C);

  char body_tmp[MAXBONENAME];
  char prefix_act[MAXBONENAME];

  BLI_string_split_prefix(ebone_act->name, prefix_act, body_tmp, sizeof(ebone_act->name));

  if (prefix_act[0] == '\0') {
    return;
  }

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    bArmature *arm = ob->data;
    bool changed = false;

    /* Find matches */
    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      if (EBONE_SELECTABLE(arm, ebone)) {
        char prefix_other[MAXBONENAME];
        BLI_string_split_prefix(ebone->name, prefix_other, body_tmp, sizeof(ebone->name));
        if (STREQ(prefix_act, prefix_other)) {
          ED_armature_ebone_select_set(ebone, true);
          changed = true;
        }
      }
    }

    if (changed) {
      WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
      DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
    }
  }
  MEM_freeN(objects);
}

static void select_similar_suffix(bContext *C)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  EditBone *ebone_act = CTX_data_active_bone(C);

  char body_tmp[MAXBONENAME];
  char suffix_act[MAXBONENAME];

  BLI_string_split_suffix(ebone_act->name, body_tmp, suffix_act, sizeof(ebone_act->name));

  if (suffix_act[0] == '\0') {
    return;
  }

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    bArmature *arm = ob->data;
    bool changed = false;

    /* Find matches */
    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      if (EBONE_SELECTABLE(arm, ebone)) {
        char suffix_other[MAXBONENAME];
        BLI_string_split_suffix(ebone->name, body_tmp, suffix_other, sizeof(ebone->name));
        if (STREQ(suffix_act, suffix_other)) {
          ED_armature_ebone_select_set(ebone, true);
          changed = true;
        }
      }
    }

    if (changed) {
      WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
      DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
    }
  }
  MEM_freeN(objects);
}

/** Use for matching any pose channel data. */
static void select_similar_data_pchan(bContext *C, const size_t bytes_size, const int offset)
{
  Object *obedit = CTX_data_edit_object(C);
  bArmature *arm = obedit->data;
  EditBone *ebone_act = CTX_data_active_bone(C);

  const bPoseChannel *pchan_active = BKE_pose_channel_find_name(obedit->pose, ebone_act->name);

  /* This will mostly happen for corner cases where the user tried to access this
   * before having any valid pose data for the armature. */
  if (pchan_active == NULL) {
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
  DEG_id_tag_update(&obedit->id, ID_RECALC_COPY_ON_WRITE);
}

static void is_ancestor(EditBone *bone, EditBone *ancestor)
{
  if (bone->temp.ebone == ancestor || bone->temp.ebone == NULL) {
    return;
  }

  if (bone->temp.ebone->temp.ebone != NULL && bone->temp.ebone->temp.ebone != ancestor) {
    is_ancestor(bone->temp.ebone, ancestor);
  }

  bone->temp.ebone = bone->temp.ebone->temp.ebone;
}

static void select_similar_children(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  bArmature *arm = obedit->data;
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
  DEG_id_tag_update(&obedit->id, ID_RECALC_COPY_ON_WRITE);
}

static void select_similar_children_immediate(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  bArmature *arm = obedit->data;
  EditBone *ebone_act = CTX_data_active_bone(C);

  LISTBASE_FOREACH (EditBone *, ebone_iter, arm->edbo) {
    if (ebone_iter->parent == ebone_act && EBONE_SELECTABLE(arm, ebone_iter)) {
      ED_armature_ebone_select_set(ebone_iter, true);
    }
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);
  DEG_id_tag_update(&obedit->id, ID_RECALC_COPY_ON_WRITE);
}

static void select_similar_siblings(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  bArmature *arm = obedit->data;
  EditBone *ebone_act = CTX_data_active_bone(C);

  if (ebone_act->parent == NULL) {
    return;
  }

  LISTBASE_FOREACH (EditBone *, ebone_iter, arm->edbo) {
    if (ebone_iter->parent == ebone_act->parent && EBONE_SELECTABLE(arm, ebone_iter)) {
      ED_armature_ebone_select_set(ebone_iter, true);
    }
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);
  DEG_id_tag_update(&obedit->id, ID_RECALC_COPY_ON_WRITE);
}

static int armature_select_similar_exec(bContext *C, wmOperator *op)
{
  /* Get props */
  int type = RNA_enum_get(op->ptr, "type");
  float thresh = RNA_float_get(op->ptr, "threshold");

  /* Check for active bone */
  if (CTX_data_active_bone(C) == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Operation requires an active bone");
    return OPERATOR_CANCELLED;
  }

#define STRUCT_SIZE_AND_OFFSET(_struct, _member) \
  sizeof(((_struct *)NULL)->_member), offsetof(_struct, _member)

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
    case SIMEDBONE_LAYER:
      select_similar_layer(C);
      break;
    case SIMEDBONE_GROUP:
      select_similar_data_pchan(C, STRUCT_SIZE_AND_OFFSET(bPoseChannel, agrp_index));
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
static int armature_select_hierarchy_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_edit_object(C);
  EditBone *ebone_active;
  int direction = RNA_enum_get(op->ptr, "direction");
  const bool add_to_sel = RNA_boolean_get(op->ptr, "extend");
  bool changed = false;
  bArmature *arm = (bArmature *)ob->data;

  ebone_active = arm->act_edbone;
  if (ebone_active == NULL) {
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
    EditBone *ebone_iter, *ebone_child = NULL;
    int pass;

    /* first pass, only connected bones (the logical direct child) */
    for (pass = 0; pass < 2 && (ebone_child == NULL); pass++) {
      for (ebone_iter = arm->edbo->first; ebone_iter; ebone_iter = ebone_iter->next) {
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
  DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);

  return OPERATOR_FINISHED;
}

void ARMATURE_OT_select_hierarchy(wmOperatorType *ot)
{
  static const EnumPropertyItem direction_items[] = {
      {BONE_SELECT_PARENT, "PARENT", 0, "Select Parent", ""},
      {BONE_SELECT_CHILD, "CHILD", 0, "Select Child", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Select Hierarchy";
  ot->idname = "ARMATURE_OT_select_hierarchy";
  ot->description = "Select immediate parent/children of selected bones";

  /* api callbacks */
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
static int armature_select_mirror_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const bool active_only = RNA_boolean_get(op->ptr, "only_active");
  const bool extend = RNA_boolean_get(op->ptr, "extend");

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    bArmature *arm = ob->data;

    EditBone *ebone, *ebone_mirror_act = NULL;

    for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
      const int flag = ED_armature_ebone_selectflag_get(ebone);
      EBONE_PREV_FLAG_SET(ebone, flag);
    }

    for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
      if (EBONE_SELECTABLE(arm, ebone)) {
        EditBone *ebone_mirror;
        int flag_new = extend ? EBONE_PREV_FLAG_GET(ebone) : 0;

        if ((ebone_mirror = ED_armature_ebone_get_mirrored(arm->edbo, ebone)) &&
            (EBONE_VISIBLE(arm, ebone_mirror))) {
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
    DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
  }
  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

void ARMATURE_OT_select_mirror(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Flip Active/Selected Bone";
  ot->idname = "ARMATURE_OT_select_mirror";
  ot->description = "Mirror the bone selection";

  /* api callbacks */
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

static int armature_shortest_path_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *obedit = CTX_data_edit_object(C);
  bArmature *arm = obedit->data;
  EditBone *ebone_src, *ebone_dst;
  EditBone *ebone_isect_parent = NULL;
  EditBone *ebone_isect_child[2];
  bool changed;
  Base *base_dst = NULL;

  view3d_operator_needs_opengl(C);
  BKE_object_update_select_id(CTX_data_main(C));

  ebone_src = arm->act_edbone;
  ebone_dst = ED_armature_pick_ebone(C, event->mval, false, &base_dst);

  /* fallback to object selection */
  if (ELEM(NULL, ebone_src, ebone_dst) || (ebone_src == ebone_dst)) {
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
    SWAP(EditBone *, ebone_src, ebone_dst);
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
        armature_shortest_path_select(arm, ebone_isect_parent, ebone_dst, false, true)) {
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
    DEG_id_tag_update(&obedit->id, ID_RECALC_COPY_ON_WRITE);

    return OPERATOR_FINISHED;
  }
  else {
    BKE_report(op->reports, RPT_WARNING, "Unselectable bone in chain");
    return OPERATOR_CANCELLED;
  }
}

void ARMATURE_OT_shortest_path_pick(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Pick Shortest Path";
  ot->idname = "ARMATURE_OT_shortest_path_pick";
  ot->description = "Select shortest path between two bones";

  /* api callbacks */
  ot->invoke = armature_shortest_path_pick_invoke;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */
