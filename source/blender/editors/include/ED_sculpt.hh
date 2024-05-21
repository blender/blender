/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

struct ARegion;
struct Object;
struct ReportList;
struct UndoType;
struct ViewContext;
struct bContext;
struct rcti;
struct wmOperator;
struct wmKeyConfig;

void ED_object_sculptmode_enter_ex(Main &bmain,
                                   Depsgraph &depsgraph,
                                   Scene &scene,
                                   Object &ob,
                                   bool force_dyntopo,
                                   ReportList *reports);
void ED_object_sculptmode_enter(bContext *C, Depsgraph &depsgraph, ReportList *reports);
void ED_object_sculptmode_exit_ex(Main &bmain, Depsgraph &depsgraph, Scene &scene, Object &ob);
void ED_object_sculptmode_exit(bContext *C, Depsgraph &depsgraph);

/* `sculpt.cc` */

/**
 * Checks if the currently active Sculpt Mode on the object is targeting a locked shape key,
 * and produces an error message if so (unless \a reports is null).
 * \return true if the shape key was locked.
 */
bool ED_sculpt_report_if_shape_key_is_locked(const Object &ob, ReportList *reports);

void ED_operatortypes_sculpt();

void ED_keymap_sculpt(wmKeyConfig *keyconf);

/* `sculpt_transform.cc` */

namespace blender::ed::sculpt_paint {

void update_modal_transform(bContext *C, Object &ob);
void init_transform(bContext *C, Object &ob, const float mval_fl[2], const char *undo_name);
void end_transform(bContext *C, Object &ob);

/* `sculpt_undo.cc` */

namespace undo {

void register_type(UndoType *ut);

/**
 * Pushes an undo step using the operator name. This is necessary for
 * redo panels to work; operators that do not support that may use
 * #geometry_begin_ex instead if so desired.
 */
void geometry_begin(Object &ob, const wmOperator *op);
void geometry_begin_ex(Object &ob, const char *name);
void geometry_end(Object &ob);

/**
 * Undo for changes happening on a base mesh for multires sculpting.
 * if there is no multi-res sculpt active regular undo is used.
 */
void push_multires_mesh_begin(bContext *C, const char *str);
void push_multires_mesh_end(bContext *C, const char *str);

}  // namespace undo

namespace face_set {

int find_next_available_id(Object &object);
void initialize_none_to_id(Mesh *mesh, int new_id);
int active_update_and_get(bContext *C, Object &ob, const float mval_fl[2]);

}  // namespace face_set

/**
 * Fills the object's active color attribute layer with the fill color.
 *
 * \param only_selected: Limit the fill to selected faces or vertices.
 *
 * \return #true if successful.
 */
bool object_active_color_fill(Object &ob, const float fill_color[4], bool only_selected);

}  // namespace blender::ed::sculpt_paint
