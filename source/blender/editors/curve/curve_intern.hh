/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edcurve
 */

#pragma once

/* internal exports only */
struct EditNurb;
struct GHash;
struct ListBase;
struct Object;
struct ViewContext;
struct wmKeyMap;
struct wmOperatorType;

/* `editfont.cc` */

enum {
  DEL_NEXT_CHAR,
  DEL_PREV_CHAR,
  DEL_NEXT_WORD,
  DEL_PREV_WORD,
  DEL_SELECTION,
  DEL_NEXT_SEL,
  DEL_PREV_SEL
};
enum { CASE_LOWER, CASE_UPPER };
enum {
  LINE_BEGIN,
  LINE_END,
  TEXT_BEGIN,
  TEXT_END,
  PREV_CHAR,
  NEXT_CHAR,
  PREV_WORD,
  NEXT_WORD,
  PREV_LINE,
  NEXT_LINE,
  PREV_PAGE,
  NEXT_PAGE
};

enum eVisible_Types {
  HIDDEN = true,
  VISIBLE = false,
};

enum eEndPoint_Types {
  FIRST = true,
  LAST = false,
};

enum eCurveElem_Types {
  CURVE_VERTEX = 0,
  CURVE_SEGMENT,
};

/* internal select utils */
/**
 * Returns 1 in case (de)selection was successful.
 */
bool select_beztriple(BezTriple *bezt, bool selstatus, uint8_t flag, eVisible_Types hidden);
/**
 * Returns 1 in case (de)selection was successful.
 */
bool select_bpoint(BPoint *bp, bool selstatus, uint8_t flag, bool hidden);

void FONT_OT_text_insert(wmOperatorType *ot);
void FONT_OT_line_break(wmOperatorType *ot);

void FONT_OT_case_toggle(wmOperatorType *ot);
void FONT_OT_case_set(wmOperatorType *ot);
void FONT_OT_style_toggle(wmOperatorType *ot);
void FONT_OT_style_set(wmOperatorType *ot);

void FONT_OT_select_all(wmOperatorType *ot);

void FONT_OT_text_copy(wmOperatorType *ot);
void FONT_OT_text_cut(wmOperatorType *ot);
void FONT_OT_text_paste(wmOperatorType *ot);
void FONT_OT_text_paste_from_file(wmOperatorType *ot);
void FONT_OT_text_insert_unicode(wmOperatorType *ot);

void FONT_OT_selection_set(wmOperatorType *ot);
void FONT_OT_select_word(wmOperatorType *ot);

void FONT_OT_move(wmOperatorType *ot);
void FONT_OT_move_select(wmOperatorType *ot);
void FONT_OT_delete(wmOperatorType *ot);

void FONT_OT_change_character(wmOperatorType *ot);
void FONT_OT_change_spacing(wmOperatorType *ot);

void FONT_OT_open(wmOperatorType *ot);
void FONT_OT_unlink(wmOperatorType *ot);

void FONT_OT_textbox_add(wmOperatorType *ot);
void FONT_OT_textbox_remove(wmOperatorType *ot);

/* `editcurve.cc` */

void CURVE_OT_hide(wmOperatorType *ot);
void CURVE_OT_reveal(wmOperatorType *ot);

void CURVE_OT_separate(wmOperatorType *ot);
void CURVE_OT_split(wmOperatorType *ot);
void CURVE_OT_duplicate(wmOperatorType *ot);
void CURVE_OT_delete(wmOperatorType *ot);
void CURVE_OT_dissolve_verts(wmOperatorType *ot);

void CURVE_OT_spline_type_set(wmOperatorType *ot);
void CURVE_OT_radius_set(wmOperatorType *ot);
void CURVE_OT_spline_weight_set(wmOperatorType *ot);
void CURVE_OT_handle_type_set(wmOperatorType *ot);
void CURVE_OT_normals_make_consistent(wmOperatorType *ot);
void CURVE_OT_decimate(wmOperatorType *ot);
void CURVE_OT_shade_smooth(wmOperatorType *ot);
void CURVE_OT_shade_flat(wmOperatorType *ot);
void CURVE_OT_tilt_clear(wmOperatorType *ot);

void CURVE_OT_smooth(wmOperatorType *ot);
void CURVE_OT_smooth_weight(wmOperatorType *ot);
void CURVE_OT_smooth_radius(wmOperatorType *ot);
void CURVE_OT_smooth_tilt(wmOperatorType *ot);

void CURVE_OT_switch_direction(wmOperatorType *ot);
void CURVE_OT_subdivide(wmOperatorType *ot);
void CURVE_OT_make_segment(wmOperatorType *ot);
void CURVE_OT_spin(wmOperatorType *ot);
void CURVE_OT_vertex_add(wmOperatorType *ot);
void CURVE_OT_extrude(wmOperatorType *ot);
void CURVE_OT_cyclic_toggle(wmOperatorType *ot);

void CURVE_OT_match_texture_space(wmOperatorType *ot);

/* exported for editcurve_undo.cc */

GHash *ED_curve_keyindex_hash_duplicate(GHash *keyindex);
void ED_curve_keyindex_update_nurb(EditNurb *editnurb, Nurb *nu, Nurb *newnu);

/* exported for `editcurve_pen.cc` */

int ed_editcurve_addvert(Curve *cu, EditNurb *editnurb, View3D *v3d, const float location_init[3]);
bool curve_toggle_cyclic(View3D *v3d, ListBase *editnurb, int direction);
void ed_dissolve_bez_segment(BezTriple *bezt_prev,
                             BezTriple *bezt_next,
                             const Nurb *nu,
                             const Curve *cu,
                             const uint span_len,
                             const uint span_step[2]);

/* helper functions */
void ed_editnurb_translate_flag(ListBase *editnurb, uint8_t flag, const float vec[3], bool is_2d);
/**
 * Only for #OB_SURF.
 */
bool ed_editnurb_extrude_flag(EditNurb *editnurb, uint8_t flag);
/**
 * \param axis: is in world-space.
 * \param cent: is in object-space.
 */
bool ed_editnurb_spin(
    float viewmat[4][4], View3D *v3d, Object *obedit, const float axis[3], const float cent[3]);

/* `editcurve_select.cc` */

void CURVE_OT_de_select_first(wmOperatorType *ot);
void CURVE_OT_de_select_last(wmOperatorType *ot);
void CURVE_OT_select_all(wmOperatorType *ot);
void CURVE_OT_select_linked(wmOperatorType *ot);
void CURVE_OT_select_linked_pick(wmOperatorType *ot);
void CURVE_OT_select_row(wmOperatorType *ot);
void CURVE_OT_select_next(wmOperatorType *ot);
void CURVE_OT_select_previous(wmOperatorType *ot);
void CURVE_OT_select_more(wmOperatorType *ot);
void CURVE_OT_select_less(wmOperatorType *ot);
void CURVE_OT_select_random(wmOperatorType *ot);
void CURVE_OT_select_nth(wmOperatorType *ot);
void CURVE_OT_select_similar(wmOperatorType *ot);
void CURVE_OT_shortest_path_pick(wmOperatorType *ot);

/* `editcurve_add.cc` */

void CURVE_OT_primitive_bezier_curve_add(wmOperatorType *ot);
void CURVE_OT_primitive_bezier_circle_add(wmOperatorType *ot);
void CURVE_OT_primitive_nurbs_curve_add(wmOperatorType *ot);
void CURVE_OT_primitive_nurbs_circle_add(wmOperatorType *ot);
void CURVE_OT_primitive_nurbs_path_add(wmOperatorType *ot);

void SURFACE_OT_primitive_nurbs_surface_curve_add(wmOperatorType *ot);
void SURFACE_OT_primitive_nurbs_surface_circle_add(wmOperatorType *ot);
void SURFACE_OT_primitive_nurbs_surface_surface_add(wmOperatorType *ot);
void SURFACE_OT_primitive_nurbs_surface_cylinder_add(wmOperatorType *ot);
void SURFACE_OT_primitive_nurbs_surface_sphere_add(wmOperatorType *ot);
void SURFACE_OT_primitive_nurbs_surface_torus_add(wmOperatorType *ot);

/* `editcurve_query.cc` */

bool ED_curve_pick_vert(ViewContext *vc,
                        short sel,
                        Nurb **r_nurb,
                        BezTriple **r_bezt,
                        BPoint **r_bp,
                        short *r_handle,
                        Base **r_base);
/**
 * Pick the nearest `r_nurb` and `r_bezt` or `r_bp`.
 * \param select: selected vertices have a disadvantage.
 * \param sel_dist_mul: A multiplier on the default select distance.
 * \param r_handle: For bezier triples, set the handle index [0, 1, 2].
 */
bool ED_curve_pick_vert_ex(ViewContext *vc,
                           bool select,
                           int dist_px,
                           Nurb **r_nurb,
                           BezTriple **r_bezt,
                           BPoint **r_bp,
                           short *r_handle,
                           Base **r_base);
void ED_curve_nurb_vert_selected_find(
    Curve *cu, View3D *v3d, Nurb **r_nu, BezTriple **r_bezt, BPoint **r_bp);

/* `editcurve_paint.cc` */

void CURVE_OT_draw(wmOperatorType *ot);

/* `editcurve_pen.cc` */

void CURVE_OT_pen(wmOperatorType *ot);
wmKeyMap *curve_pen_modal_keymap(wmKeyConfig *keyconf);
