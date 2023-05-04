/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2012 Blender Foundation */

/** \file
 * \ingroup edmask
 */

#include "BKE_context.h"
#include "BKE_mask.h"

#include "DNA_scene_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_clip.h"
#include "ED_image.h"
#include "ED_mask.h" /* own include */
#include "ED_sequencer.h"

#include "RNA_access.h"

#include "mask_intern.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name Poll Functions
 * \{ */

bool ED_maskedit_poll(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area) {
    switch (area->spacetype) {
      case SPACE_CLIP:
        return ED_space_clip_maskedit_poll(C);
      case SPACE_SEQ:
        return ED_space_sequencer_maskedit_poll(C);
      case SPACE_IMAGE:
        return ED_space_image_maskedit_poll(C);
    }
  }
  return false;
}

bool ED_maskedit_visible_splines_poll(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area) {
    switch (area->spacetype) {
      case SPACE_CLIP:
        return ED_space_clip_maskedit_visible_splines_poll(C);
      case SPACE_SEQ:
        return ED_space_sequencer_maskedit_poll(C);
      case SPACE_IMAGE:
        return ED_space_image_maskedit_visible_splines_poll(C);
    }
  }
  return false;
}

bool ED_maskedit_mask_poll(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area) {
    switch (area->spacetype) {
      case SPACE_CLIP:
        return ED_space_clip_maskedit_mask_poll(C);
      case SPACE_SEQ:
        return ED_space_sequencer_maskedit_mask_poll(C);
      case SPACE_IMAGE:
        return ED_space_image_maskedit_mask_poll(C);
    }
  }
  return false;
}

bool ED_maskedit_mask_visible_splines_poll(bContext *C)
{
  const ScrArea *area = CTX_wm_area(C);
  if (area) {
    switch (area->spacetype) {
      case SPACE_CLIP:
        return ED_space_clip_maskedit_mask_visible_splines_poll(C);
      case SPACE_SEQ:
        return ED_space_sequencer_maskedit_mask_poll(C);
      case SPACE_IMAGE:
        return ED_space_image_maskedit_mask_visible_splines_poll(C);
    }
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registration
 * \{ */

void ED_operatortypes_mask(void)
{
  WM_operatortype_append(MASK_OT_new);

  /* mask layers */
  WM_operatortype_append(MASK_OT_layer_new);
  WM_operatortype_append(MASK_OT_layer_remove);

  /* add */
  WM_operatortype_append(MASK_OT_add_vertex);
  WM_operatortype_append(MASK_OT_add_feather_vertex);
  WM_operatortype_append(MASK_OT_primitive_circle_add);
  WM_operatortype_append(MASK_OT_primitive_square_add);

  /* geometry */
  WM_operatortype_append(MASK_OT_switch_direction);
  WM_operatortype_append(MASK_OT_normals_make_consistent);
  WM_operatortype_append(MASK_OT_delete);

  /* select */
  WM_operatortype_append(MASK_OT_select);
  WM_operatortype_append(MASK_OT_select_all);
  WM_operatortype_append(MASK_OT_select_box);
  WM_operatortype_append(MASK_OT_select_lasso);
  WM_operatortype_append(MASK_OT_select_circle);
  WM_operatortype_append(MASK_OT_select_linked_pick);
  WM_operatortype_append(MASK_OT_select_linked);
  WM_operatortype_append(MASK_OT_select_more);
  WM_operatortype_append(MASK_OT_select_less);

  /* hide/reveal */
  WM_operatortype_append(MASK_OT_hide_view_clear);
  WM_operatortype_append(MASK_OT_hide_view_set);

  /* feather */
  WM_operatortype_append(MASK_OT_feather_weight_clear);

  /* shape */
  WM_operatortype_append(MASK_OT_slide_point);
  WM_operatortype_append(MASK_OT_slide_spline_curvature);
  WM_operatortype_append(MASK_OT_cyclic_toggle);
  WM_operatortype_append(MASK_OT_handle_type_set);

  /* relationships */
  WM_operatortype_append(MASK_OT_parent_set);
  WM_operatortype_append(MASK_OT_parent_clear);

  /* Shape-keys. */
  WM_operatortype_append(MASK_OT_shape_key_insert);
  WM_operatortype_append(MASK_OT_shape_key_clear);
  WM_operatortype_append(MASK_OT_shape_key_feather_reset);
  WM_operatortype_append(MASK_OT_shape_key_rekey);

  /* layers */
  WM_operatortype_append(MASK_OT_layer_move);

  /* duplicate */
  WM_operatortype_append(MASK_OT_duplicate);

  /* clipboard */
  WM_operatortype_append(MASK_OT_copy_splines);
  WM_operatortype_append(MASK_OT_paste_splines);
}

void ED_keymap_mask(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap = WM_keymap_ensure(keyconf, "Mask Editing", 0, 0);
  keymap->poll = ED_maskedit_poll;
}

void ED_operatormacros_mask(void)
{
  wmOperatorType *ot;
  wmOperatorTypeMacro *otmacro;

  ot = WM_operatortype_append_macro("MASK_OT_add_vertex_slide",
                                    "Add Vertex and Slide",
                                    "Add new vertex and slide it",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  ot->description = "Add new vertex and slide it";
  WM_operatortype_macro_define(ot, "MASK_OT_add_vertex");
  otmacro = WM_operatortype_macro_define(ot, "MASK_OT_slide_point");
  RNA_boolean_set(otmacro->ptr, "is_new_point", true);

  ot = WM_operatortype_append_macro("MASK_OT_add_feather_vertex_slide",
                                    "Add Feather Vertex and Slide",
                                    "Add new vertex to feather and slide it",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  ot->description = "Add new feather vertex and slide it";
  WM_operatortype_macro_define(ot, "MASK_OT_add_feather_vertex");
  otmacro = WM_operatortype_macro_define(ot, "MASK_OT_slide_point");
  RNA_boolean_set(otmacro->ptr, "slide_feather", true);

  ot = WM_operatortype_append_macro("MASK_OT_duplicate_move",
                                    "Add Duplicate",
                                    "Duplicate mask and move",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "MASK_OT_duplicate");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
  RNA_boolean_set(otmacro->ptr, "mirror", false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lock-to-selection viewport preservation
 * \{ */

void ED_mask_view_lock_state_store(const bContext *C, MaskViewLockState *state)
{
  SpaceClip *space_clip = CTX_wm_space_clip(C);
  if (space_clip != nullptr) {
    ED_clip_view_lock_state_store(C, &state->space_clip_state);
  }
}

void ED_mask_view_lock_state_restore_no_jump(const bContext *C, const MaskViewLockState *state)
{
  SpaceClip *space_clip = CTX_wm_space_clip(C);
  if (space_clip != nullptr) {
    if ((space_clip->flag & SC_LOCK_SELECTION) == 0) {
      /* Early output if the editor is not locked to selection.
       * Avoids forced dependency graph evaluation here. */
      return;
    }

    /* Mask's lock-to-selection requires deformed splines to be evaluated to calculate bounds of
     * points after animation has been evaluated. The restore-no-jump type of function does
     * calculation of new offset for the view for an updated state of mask to cancel the offset out
     * by modifying locked offset. In order to do such calculation mask needs to be evaluated after
     * modification by an operator. */
    struct Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    (void)depsgraph;

    ED_clip_view_lock_state_restore_no_jump(C, &state->space_clip_state);
  }
}

/** \} */
