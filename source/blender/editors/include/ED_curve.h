/* SPDX-FileCopyrightText: 2009 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct BPoint;
struct Base;
struct BezTriple;
struct Curve;
struct EditNurb;
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

void ED_operatortypes_curve(void);
void ED_operatormacros_curve(void);
void ED_keymap_curve(struct wmKeyConfig *keyconf);

/* `editcurve.cc` */

struct ListBase *object_editcurve_get(struct Object *ob);

/**
 * Load editNurb in object.
 */
void ED_curve_editnurb_load(struct Main *bmain, struct Object *obedit);
/**
 * Make copy in `cu->editnurb`.
 */
void ED_curve_editnurb_make(struct Object *obedit);
void ED_curve_editnurb_free(struct Object *obedit);

/**
 * \param dist_px: Maximum distance to pick (in pixels).
 * \param vert_without_handles: When true, selecting the knot doesn't select the handles.
 */
bool ED_curve_editnurb_select_pick(struct bContext *C,
                                   const int mval[2],
                                   int dist_px,
                                   bool vert_without_handles,
                                   const struct SelectPick_Params *params);

struct Nurb *ED_curve_add_nurbs_primitive(
    struct bContext *C, struct Object *obedit, float mat[4][4], int type, int newob);

bool ED_curve_nurb_select_check(const struct View3D *v3d, const struct Nurb *nu);
int ED_curve_nurb_select_count(const struct View3D *v3d, const struct Nurb *nu);
bool ED_curve_nurb_select_all(const struct Nurb *nu);
bool ED_curve_nurb_deselect_all(const struct Nurb *nu);

/**
 * This is used externally, by #OBJECT_OT_join.
 * TODO: shape keys - as with meshes.
 */
int ED_curve_join_objects_exec(struct bContext *C, struct wmOperator *op);

/* `editcurve_select.cc` */

bool ED_curve_select_check(const struct View3D *v3d, const struct EditNurb *editnurb);
bool ED_curve_deselect_all(struct EditNurb *editnurb);
bool ED_curve_deselect_all_multi_ex(struct Base **bases, int bases_len);
bool ED_curve_deselect_all_multi(struct bContext *C);
bool ED_curve_select_all(struct EditNurb *editnurb);
bool ED_curve_select_swap(struct EditNurb *editnurb, bool hide_handles);
int ED_curve_select_count(const struct View3D *v3d, const struct EditNurb *editnurb);

/* editcurve_undo.cc */

/** Export for ED_undo_sys */
void ED_curve_undosys_type(struct UndoType *ut);

/* `editfont.cc` */

void ED_curve_editfont_load(struct Object *obedit);
void ED_curve_editfont_make(struct Object *obedit);
void ED_curve_editfont_free(struct Object *obedit);

void ED_text_to_object(struct bContext *C, const struct Text *text, bool split_lines);

void ED_curve_beztcpy(struct EditNurb *editnurb,
                      struct BezTriple *dst,
                      struct BezTriple *src,
                      int count);
void ED_curve_bpcpy(struct EditNurb *editnurb, struct BPoint *dst, struct BPoint *src, int count);

/**
 * Return 0 if animation data wasn't changed, 1 otherwise.
 */
int ED_curve_updateAnimPaths(struct Main *bmain, struct Curve *cu);

bool ED_curve_active_center(struct Curve *cu, float center[3]);

/**
 * Text box selection.
 *
 * \return True when pick finds an element or the selection changed.
 */
bool ED_curve_editfont_select_pick(struct bContext *C,
                                   const int mval[2],
                                   const struct SelectPick_Params *params);

/* `editfont_undo.cc` */

/** Export for ED_undo_sys. */
void ED_font_undosys_type(struct UndoType *ut);

#if 0
/* debug only */
void printknots(struct Object *obedit);
#endif

#ifdef __cplusplus
}
#endif
