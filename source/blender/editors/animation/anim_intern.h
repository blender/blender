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
 * The Original Code is Copyright (C) 2009, Blender Foundation, Joshua Leung
 * This is a new part of Blender (with some old code)
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/animation/anim_intern.h
 *  \ingroup edanimation
 */

 
#ifndef __ANIM_INTERN_H__
#define __ANIM_INTERN_H__

/* KeyingSets/Keyframing Interface ------------- */

/* list of builtin KeyingSets (defined in keyingsets.c) */
extern ListBase builtin_keyingsets;

/* Operator Define Prototypes ------------------- */

/* Main Keyframe Management operators: 
 *	These handle keyframes management from various spaces. They only make use of
 *  Keying Sets.
 */
void ANIM_OT_keyframe_insert(struct wmOperatorType *ot);
void ANIM_OT_keyframe_delete(struct wmOperatorType *ot);

/* Main Keyframe Management operators: 
 *	These handle keyframes management from various spaces. They will handle the menus 
 *  required for each space.
 */
void ANIM_OT_keyframe_insert_menu(struct wmOperatorType *ot);

void ANIM_OT_keyframe_delete_v3d(struct wmOperatorType *ot);
void ANIM_OT_keyframe_clear_v3d(struct wmOperatorType *ot);

/* Keyframe management operators for UI buttons (RMB menu). */
void ANIM_OT_keyframe_insert_button(struct wmOperatorType *ot);
void ANIM_OT_keyframe_delete_button(struct wmOperatorType *ot);
void ANIM_OT_keyframe_clear_button(struct wmOperatorType *ot);

/* .......... */

/* KeyingSet management operators for UI buttons (RMB menu) */
void ANIM_OT_keyingset_button_add(struct wmOperatorType *ot);
void ANIM_OT_keyingset_button_remove(struct wmOperatorType *ot);

/* KeyingSet management operators for RNA collections/UI buttons */
void ANIM_OT_keying_set_add(struct wmOperatorType *ot);
void ANIM_OT_keying_set_remove(struct wmOperatorType *ot);
void ANIM_OT_keying_set_path_add(struct wmOperatorType *ot);
void ANIM_OT_keying_set_path_remove(struct wmOperatorType *ot);

/* KeyingSet general operators */
void ANIM_OT_keying_set_active_set(struct wmOperatorType *ot);

/* .......... */

/* Driver management operators for UI buttons (RMB menu) */
void ANIM_OT_driver_button_add(struct wmOperatorType *ot);
void ANIM_OT_driver_button_remove(struct wmOperatorType *ot);
void ANIM_OT_copy_driver_button(struct wmOperatorType *ot);
void ANIM_OT_paste_driver_button(struct wmOperatorType *ot);

#endif  /* __ANIM_INTERN_H__ */
