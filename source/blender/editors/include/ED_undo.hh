/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_sys_types.h"
#include "BLI_vector.hh"

struct Base;
struct CLG_LogRef;
struct ID;
struct MemFile;
struct PointerRNA;
struct Object;
struct Scene;
struct UndoStack;
struct ViewLayer;
struct bContext;
struct wmOperator;
struct wmOperatorType;
struct wmWindowManager;

/* undo.c */

/**
 * Run from the main event loop, basic checks that undo is left in a correct state.
 */
bool ED_undo_is_state_valid(bContext *C);
void ED_undo_group_begin(bContext *C);
void ED_undo_group_end(bContext *C);
void ED_undo_push(bContext *C, const char *str);
void ED_undo_push_op(bContext *C, wmOperator *op);
void ED_undo_grouped_push(bContext *C, const char *str);
void ED_undo_grouped_push_op(bContext *C, wmOperator *op);
void ED_undo_pop_op(bContext *C, wmOperator *op);
void ED_undo_pop(bContext *C);
void ED_undo_redo(bContext *C);
void ED_OT_undo(wmOperatorType *ot);
void ED_OT_undo_push(wmOperatorType *ot);
void ED_OT_redo(wmOperatorType *ot);
void ED_OT_undo_redo(wmOperatorType *ot);
void ED_OT_undo_history(wmOperatorType *ot);

/**
 * UI callbacks should call this rather than calling WM_operator_repeat() themselves.
 *
 * \return true when repeat succeeded.
 */
bool ED_undo_operator_repeat(bContext *C, wmOperator *op);
/**
 * Convenience since UI callbacks use this mostly.
 */
void ED_undo_operator_repeat_cb(bContext *C, void *arg_op, void *arg_unused);
void ED_undo_operator_repeat_cb_evt(bContext *C, void *arg_op, int arg_unused);

/**
 * Name optionally, function used to check for operator redo panel.
 */
bool ED_undo_is_valid(const bContext *C, const char *undoname);

bool ED_undo_is_memfile_compatible(const bContext *C);

/* Unfortunate workaround for limits mixing undo systems. */

/**
 * When a property of ID changes, return false.
 *
 * This is to avoid changes to a property making undo pushes
 * which are ignored by the undo-system.
 * For example, changing a brush property isn't stored by sculpt-mode undo steps.
 * This workaround is needed until the limitation is removed, see: #61948.
 */
bool ED_undo_is_legacy_compatible_for_property(bContext *C, ID *id, PointerRNA &ptr);

/**
 * This function addresses the problem of restoring undo steps when multiple windows are used.
 * Since undo steps don't track the full context that created them it's possible an edit-mode
 * undo step will attempt to restore edit-mode into a different window, scene or view-layer.
 *
 * Values `scene_p` & `view_layer_p` (typically initialized from the context)
 * are updated from the visible windows using `scene_ref` as a reference.
 * If the no window can be found, the values are left as-is.
 *
 * Since users may close windows before undoing, it's expected the window may be unavailable.
 * When this happens the edit-mode objects wont be restored into edit-mode by
 * #ED_undo_object_editmode_restore_helper which is acceptable since objects
 * which aren't visible in any window don't need to enter edit-mode.
 */
void ED_undo_object_editmode_validate_scene_from_windows(wmWindowManager *wm,
                                                         const Scene *scene_ref,
                                                         Scene **scene_p,
                                                         ViewLayer **view_layer_p);

/**
 * Load all our objects from `object_array` into edit-mode, clear everything else.
 */
void ED_undo_object_editmode_restore_helper(Scene *scene,
                                            ViewLayer *view_layer,
                                            Object **object_array,
                                            uint object_array_len,
                                            uint object_array_stride);

blender::Vector<Object *> ED_undo_editmode_objects_from_view_layer(const Scene *scene,
                                                                   ViewLayer *view_layer);
blender::Vector<Base *> ED_undo_editmode_bases_from_view_layer(const Scene *scene,
                                                               ViewLayer *view_layer);

/**
 * Ideally we won't access the stack directly,
 * this is needed for modes which handle undo themselves (bypassing #ED_undo_push).
 *
 * Using global isn't great, this just avoids doing inline,
 * causing `BKE_global.hh` & `BKE_main.hh` includes.
 */
UndoStack *ED_undo_stack_get();

/* Helpers. */

void ED_undo_object_set_active_or_warn(
    Scene *scene, ViewLayer *view_layer, Object *ob, const char *info, CLG_LogRef *log);

/* `undo_system_types.cc` */

void ED_undosys_type_init();
void ED_undosys_type_free();

/* `memfile_undo.cc` */

MemFile *ED_undosys_stack_memfile_get_if_active(UndoStack *ustack);
/**
 * If the last undo step is a memfile one, find the first #MemFileChunk matching given ID
 * (using its session UUID), and tag it as "changed in the future".
 *
 * Since non-memfile undo-steps cannot automatically set this flag in the previous step as done
 * with memfile ones, this has to be called manually by relevant undo code.
 *
 * \note Only current known case for this is undoing a switch from Object to Sculpt mode (see
 * #82388).
 *
 * \note Calling this ID by ID is not optimal, as it will loop over all #MemFile.chunks until it
 * finds the expected one. If this becomes an issue we'll have to add a mapping from session UUID
 * to first #MemFileChunk in #MemFile itself
 * (currently we only do that in #MemFileWriteData when writing a new step).
 */
void ED_undosys_stack_memfile_id_changed_tag(UndoStack *ustack, ID *id);
/**
 * Get the total memory usage of all undo steps in the current undo stack.
 *
 * This function iterates through all undo steps and calculates their memory consumption.
 * For sculpt undo steps, it uses the specialized sculpt memory calculation function.
 * For other undo step types, it uses the generic `data_size` field.
 *
 * \return Total memory usage in bytes, or 0 if no undo stack is available.
 */
size_t ED_undosys_total_memory_calc(UndoStack *ustack);
