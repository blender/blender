/* SPDX-FileCopyrightText: 2009 Blender Foundation, Joshua Leung.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edanimation
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* KeyingSets/Keyframing Interface ------------- */

/* list of builtin KeyingSets (defined in keyingsets.c) */
extern ListBase builtin_keyingsets;

/* Operator Define Prototypes ------------------- */

/* -------------------------------------------------------------------- */
/** \name Main Keyframe Management operators
 *
 * These handle keyframes management from various spaces.
 * They only make use of Keying Sets.
 * \{ */

void ANIM_OT_keyframe_insert(struct wmOperatorType *ot);
void ANIM_OT_keyframe_delete(struct wmOperatorType *ot);
void ANIM_OT_keyframe_insert_by_name(struct wmOperatorType *ot);
void ANIM_OT_keyframe_delete_by_name(struct wmOperatorType *ot);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Keyframe Management operators
 *
 * These handle keyframes management from various spaces.
 * They will handle the menus required for each space.
 * \{ */

void ANIM_OT_keyframe_insert_menu(struct wmOperatorType *ot);

void ANIM_OT_keyframe_delete_v3d(struct wmOperatorType *ot);
void ANIM_OT_keyframe_clear_v3d(struct wmOperatorType *ot);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Keyframe management operators for UI buttons (RMB menu)
 * \{ */

void ANIM_OT_keyframe_insert_button(struct wmOperatorType *ot);
void ANIM_OT_keyframe_delete_button(struct wmOperatorType *ot);
void ANIM_OT_keyframe_clear_button(struct wmOperatorType *ot);

/** \} */

/* -------------------------------------------------------------------- */
/** \name KeyingSet management operators for UI buttons (RMB menu)
 * \{ */

void ANIM_OT_keyingset_button_add(struct wmOperatorType *ot);
void ANIM_OT_keyingset_button_remove(struct wmOperatorType *ot);

/** \} */

/* -------------------------------------------------------------------- */
/** \name KeyingSet management operators for RNA collections/UI buttons
 * \{ */

void ANIM_OT_keying_set_add(struct wmOperatorType *ot);
void ANIM_OT_keying_set_remove(struct wmOperatorType *ot);
void ANIM_OT_keying_set_path_add(struct wmOperatorType *ot);
void ANIM_OT_keying_set_path_remove(struct wmOperatorType *ot);

/** \} */

/* -------------------------------------------------------------------- */
/** \name KeyingSet general operators
 * \{ */

void ANIM_OT_keying_set_active_set(struct wmOperatorType *ot);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Driver management operators for UI buttons (RMB menu)
 * \{ */

void ANIM_OT_driver_button_add(struct wmOperatorType *ot);
void ANIM_OT_driver_button_remove(struct wmOperatorType *ot);
void ANIM_OT_driver_button_edit(struct wmOperatorType *ot);
void ANIM_OT_copy_driver_button(struct wmOperatorType *ot);
void ANIM_OT_paste_driver_button(struct wmOperatorType *ot);

/** \} */

#ifdef __cplusplus
}
#endif
