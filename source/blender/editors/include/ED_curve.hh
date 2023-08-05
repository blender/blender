/* SPDX-FileCopyrightText: 2009 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

struct BPoint;
struct Base;
struct BezTriple;
struct Curve;
struct EditNurb;
struct ListBase;
struct Main;
struct Nurb;
struct Object;
struct SelectPick_Params;
struct Text;
struct UndoType;
struct View3D;
struct bContext;
struct wmKeyConfig;
struct wmOperator;

/* `curve_ops.cc` */

void ED_operatortypes_curve();
void ED_operatormacros_curve();
void ED_keymap_curve(wmKeyConfig *keyconf);

/* `editcurve.cc` */

ListBase *object_editcurve_get(Object *ob);

/**
 * Load editNurb in object.
 */
void ED_curve_editnurb_load(Main *bmain, Object *obedit);
/**
 * Make copy in `cu->editnurb`.
 */
void ED_curve_editnurb_make(Object *obedit);
void ED_curve_editnurb_free(Object *obedit);

/**
 * \param dist_px: Maximum distance to pick (in pixels).
 * \param vert_without_handles: When true, selecting the knot doesn't select the handles.
 */
bool ED_curve_editnurb_select_pick(bContext *C,
                                   const int mval[2],
                                   int dist_px,
                                   bool vert_without_handles,
                                   const SelectPick_Params *params);

Nurb *ED_curve_add_nurbs_primitive(
    bContext *C, Object *obedit, float mat[4][4], int type, int newob);

bool ED_curve_nurb_select_check(const View3D *v3d, const Nurb *nu);
int ED_curve_nurb_select_count(const View3D *v3d, const Nurb *nu);
bool ED_curve_nurb_select_all(const Nurb *nu);
bool ED_curve_nurb_deselect_all(const Nurb *nu);

/**
 * This is used externally, by #OBJECT_OT_join.
 * TODO: shape keys - as with meshes.
 */
int ED_curve_join_objects_exec(bContext *C, wmOperator *op);

/* `editcurve_select.cc` */

bool ED_curve_select_check(const View3D *v3d, const EditNurb *editnurb);
bool ED_curve_deselect_all(EditNurb *editnurb);
bool ED_curve_deselect_all_multi_ex(Base **bases, int bases_len);
bool ED_curve_deselect_all_multi(bContext *C);
bool ED_curve_select_all(EditNurb *editnurb);
bool ED_curve_select_swap(EditNurb *editnurb, bool hide_handles);
int ED_curve_select_count(const View3D *v3d, const EditNurb *editnurb);

/* editcurve_undo.cc */

/** Export for ED_undo_sys */
void ED_curve_undosys_type(UndoType *ut);

/* `editfont.cc` */

void ED_curve_editfont_load(Object *obedit);
void ED_curve_editfont_make(Object *obedit);
void ED_curve_editfont_free(Object *obedit);

void ED_text_to_object(bContext *C, const Text *text, bool split_lines);

void ED_curve_beztcpy(EditNurb *editnurb, BezTriple *dst, BezTriple *src, int count);
void ED_curve_bpcpy(EditNurb *editnurb, BPoint *dst, BPoint *src, int count);

/**
 * Return 0 if animation data wasn't changed, 1 otherwise.
 */
int ED_curve_updateAnimPaths(Main *bmain, Curve *cu);

bool ED_curve_active_center(Curve *cu, float center[3]);

/**
 * Text box selection.
 *
 * \return True when pick finds an element or the selection changed.
 */
bool ED_curve_editfont_select_pick(bContext *C,
                                   const int mval[2],
                                   const SelectPick_Params *params);

/* `editfont_undo.cc` */

/** Export for ED_undo_sys. */
void ED_font_undosys_type(UndoType *ut);

#if 0
/* debug only */
void printknots(struct Object *obedit);
#endif
