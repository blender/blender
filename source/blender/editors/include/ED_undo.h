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
 */

/** \file
 * \ingroup editors
 */

#ifndef __ED_UNDO_H__
#define __ED_UNDO_H__

#include "BLI_compiler_attrs.h"

struct CLG_LogRef;
struct Object;
struct UndoStack;
struct ViewLayer;
struct bContext;
struct wmOperator;
struct wmOperatorType;

/* undo.c */
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

int ED_undo_operator_repeat(struct bContext *C, struct wmOperator *op);
/* convenience since UI callbacks use this mostly*/
void ED_undo_operator_repeat_cb(struct bContext *C, void *arg_op, void *arg_unused);
void ED_undo_operator_repeat_cb_evt(struct bContext *C, void *arg_op, int arg_unused);

bool ED_undo_is_valid(const struct bContext *C, const char *undoname);

bool ED_undo_is_memfile_compatible(const struct bContext *C);

/* Unfortunate workaround for limits mixing undo systems. */
bool ED_undo_is_legacy_compatible_for_property(struct bContext *C, struct ID *id);

void ED_undo_object_editmode_restore_helper(struct bContext *C,
                                            struct Object **object_array,
                                            uint object_array_len,
                                            uint object_array_stride);

struct UndoStack *ED_undo_stack_get(void);

/* helpers */
void ED_undo_object_set_active_or_warn(struct ViewLayer *view_layer,
                                       struct Object *ob,
                                       const char *info,
                                       struct CLG_LogRef *log);

/* undo_system_types.c */
void ED_undosys_type_init(void);
void ED_undosys_type_free(void);

/* memfile_undo.c */
struct MemFile *ED_undosys_stack_memfile_get_active(struct UndoStack *ustack);

#endif /* __ED_UNDO_H__ */
