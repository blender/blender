/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_compiler_attrs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Base;
struct CLG_LogRef;
struct Object;
struct Scene;
struct UndoStack;
struct ViewLayer;
struct bContext;
struct wmOperator;
struct wmOperatorType;

/* undo.c */

/**
 * Run from the main event loop, basic checks that undo is left in a correct state.
 */
bool ED_undo_is_state_valid(struct bContext *C);
void ED_undo_group_begin(struct bContext *C);
void ED_undo_group_end(struct bContext *C);
void ED_undo_push(struct bContext *C, const char *str);
void ED_undo_push_op(struct bContext *C, struct wmOperator *op);
void ED_undo_grouped_push(struct bContext *C, const char *str);
void ED_undo_grouped_push_op(struct bContext *C, struct wmOperator *op);
void ED_undo_pop_op(struct bContext *C, struct wmOperator *op);
void ED_undo_pop(struct bContext *C);
void ED_undo_redo(struct bContext *C);
void ED_OT_undo(struct wmOperatorType *ot);
void ED_OT_undo_push(struct wmOperatorType *ot);
void ED_OT_redo(struct wmOperatorType *ot);
void ED_OT_undo_redo(struct wmOperatorType *ot);
void ED_OT_undo_history(struct wmOperatorType *ot);

/**
 * UI callbacks should call this rather than calling WM_operator_repeat() themselves.
 */
int ED_undo_operator_repeat(struct bContext *C, struct wmOperator *op);
/**
 * Convenience since UI callbacks use this mostly.
 */
void ED_undo_operator_repeat_cb(struct bContext *C, void *arg_op, void *arg_unused);
void ED_undo_operator_repeat_cb_evt(struct bContext *C, void *arg_op, int arg_unused);

/**
 * Name optionally, function used to check for operator redo panel.
 */
bool ED_undo_is_valid(const struct bContext *C, const char *undoname);

bool ED_undo_is_memfile_compatible(const struct bContext *C);

/* Unfortunate workaround for limits mixing undo systems. */

/**
 * When a property of ID changes, return false.
 *
 * This is to avoid changes to a property making undo pushes
 * which are ignored by the undo-system.
 * For example, changing a brush property isn't stored by sculpt-mode undo steps.
 * This workaround is needed until the limitation is removed, see: #61948.
 */
bool ED_undo_is_legacy_compatible_for_property(struct bContext *C, struct ID *id);

/**
 * Load all our objects from `object_array` into edit-mode, clear everything else.
 */
void ED_undo_object_editmode_restore_helper(struct bContext *C,
                                            struct Object **object_array,
                                            uint object_array_len,
                                            uint object_array_stride);

struct Object **ED_undo_editmode_objects_from_view_layer(const struct Scene *scene,
                                                         struct ViewLayer *view_layer,
                                                         uint *r_len);
struct Base **ED_undo_editmode_bases_from_view_layer(const struct Scene *scene,
                                                     struct ViewLayer *view_layer,
                                                     uint *r_len);

/**
 * Ideally we won't access the stack directly,
 * this is needed for modes which handle undo themselves (bypassing #ED_undo_push).
 *
 * Using global isn't great, this just avoids doing inline,
 * causing 'BKE_global.h' & 'BKE_main.h' includes.
 */
struct UndoStack *ED_undo_stack_get(void);

/* Helpers. */

void ED_undo_object_set_active_or_warn(struct Scene *scene,
                                       struct ViewLayer *view_layer,
                                       struct Object *ob,
                                       const char *info,
                                       struct CLG_LogRef *log);

/* `undo_system_types.cc` */

void ED_undosys_type_init(void);
void ED_undosys_type_free(void);

/* `memfile_undo.cc` */

struct MemFile *ED_undosys_stack_memfile_get_active(struct UndoStack *ustack);
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
void ED_undosys_stack_memfile_id_changed_tag(struct UndoStack *ustack, struct ID *id);

#ifdef __cplusplus
}
#endif
