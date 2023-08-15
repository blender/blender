/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

struct ARegion;
struct Object;
struct UndoType;
struct ViewContext;
struct bContext;
struct rcti;
struct wmOperator;
struct wmKeyConfig;

/* sculpt.cc */

void ED_operatortypes_sculpt();
void ED_sculpt_redraw_planes_get(float planes[4][4], ARegion *region, Object *ob);
bool ED_sculpt_mask_box_select(bContext *C, ViewContext *vc, const rcti *rect, bool select);

void ED_keymap_sculpt(wmKeyConfig *keyconf);
/* sculpt_transform.cc */

void ED_sculpt_update_modal_transform(bContext *C, Object *ob);
void ED_sculpt_init_transform(bContext *C,
                              Object *ob,
                              const float mval_fl[2],
                              const char *undo_name);
void ED_sculpt_end_transform(bContext *C, Object *ob);

/* sculpt_undo.cc */

/** Export for ED_undo_sys. */
void ED_sculpt_undosys_type(UndoType *ut);

/**
 * Pushes an undo step using the operator name. This is necessary for
 * redo panels to work; operators that do not support that may use
 * #ED_sculpt_undo_geometry_begin_ex instead if so desired.
 */
void ED_sculpt_undo_geometry_begin(Object *ob, const wmOperator *op);
void ED_sculpt_undo_geometry_begin_ex(Object *ob, const char *name);
void ED_sculpt_undo_geometry_end(Object *ob);

/* Face sets. */

int ED_sculpt_face_sets_find_next_available_id(Mesh *mesh);
void ED_sculpt_face_sets_initialize_none_to_id(Mesh *mesh, int new_id);

int ED_sculpt_face_sets_active_update_and_get(bContext *C, Object *ob, const float mval_fl[2]);

/* Undo for changes happening on a base mesh for multires sculpting.
 * if there is no multi-res sculpt active regular undo is used. */
void ED_sculpt_undo_push_multires_mesh_begin(bContext *C, const char *str);
void ED_sculpt_undo_push_multires_mesh_end(bContext *C, const char *str);
