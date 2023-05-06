/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation */

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
struct wmOperatorType;

#ifdef __cplusplus
extern "C" {
#endif

/* editfont.c */

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

typedef enum eVisible_Types {
  HIDDEN = true,
  VISIBLE = false,
} eVisible_Types;

typedef enum eEndPoint_Types {
  FIRST = true,
  LAST = false,
} eEndPoint_Types;

typedef enum eCurveElem_Types {
  CURVE_VERTEX = 0,
  CURVE_SEGMENT,
} eCurveElem_Types;

/* internal select utils */
/**
 * Returns 1 in case (de)selection was successful.
 */
bool select_beztriple(BezTriple *bezt, bool selstatus, uint8_t flag, eVisible_Types hidden);
/**
 * Returns 1 in case (de)selection was successful.
 */
bool select_bpoint(BPoint *bp, bool selstatus, uint8_t flag, bool hidden);

void FONT_OT_text_insert(struct wmOperatorType *ot);
void FONT_OT_line_break(struct wmOperatorType *ot);

void FONT_OT_case_toggle(struct wmOperatorType *ot);
void FONT_OT_case_set(struct wmOperatorType *ot);
void FONT_OT_style_toggle(struct wmOperatorType *ot);
void FONT_OT_style_set(struct wmOperatorType *ot);

void FONT_OT_select_all(struct wmOperatorType *ot);

void FONT_OT_text_copy(struct wmOperatorType *ot);
void FONT_OT_text_cut(struct wmOperatorType *ot);
void FONT_OT_text_paste(struct wmOperatorType *ot);
void FONT_OT_text_paste_from_file(struct wmOperatorType *ot);

void FONT_OT_selection_set(struct wmOperatorType *ot);
void FONT_OT_select_word(struct wmOperatorType *ot);

void FONT_OT_move(struct wmOperatorType *ot);
void FONT_OT_move_select(struct wmOperatorType *ot);
void FONT_OT_delete(struct wmOperatorType *ot);

void FONT_OT_change_character(struct wmOperatorType *ot);
void FONT_OT_change_spacing(struct wmOperatorType *ot);

void FONT_OT_open(struct wmOperatorType *ot);
void FONT_OT_unlink(struct wmOperatorType *ot);

void FONT_OT_textbox_add(struct wmOperatorType *ot);
void FONT_OT_textbox_remove(struct wmOperatorType *ot);

/* editcurve.c */

void CURVE_OT_hide(struct wmOperatorType *ot);
void CURVE_OT_reveal(struct wmOperatorType *ot);

void CURVE_OT_separate(struct wmOperatorType *ot);
void CURVE_OT_split(struct wmOperatorType *ot);
void CURVE_OT_duplicate(struct wmOperatorType *ot);
void CURVE_OT_delete(struct wmOperatorType *ot);
void CURVE_OT_dissolve_verts(struct wmOperatorType *ot);

void CURVE_OT_spline_type_set(struct wmOperatorType *ot);
void CURVE_OT_radius_set(struct wmOperatorType *ot);
void CURVE_OT_spline_weight_set(struct wmOperatorType *ot);
void CURVE_OT_handle_type_set(struct wmOperatorType *ot);
void CURVE_OT_normals_make_consistent(struct wmOperatorType *ot);
void CURVE_OT_decimate(struct wmOperatorType *ot);
void CURVE_OT_shade_smooth(struct wmOperatorType *ot);
void CURVE_OT_shade_flat(struct wmOperatorType *ot);
void CURVE_OT_tilt_clear(struct wmOperatorType *ot);

void CURVE_OT_smooth(struct wmOperatorType *ot);
void CURVE_OT_smooth_weight(struct wmOperatorType *ot);
void CURVE_OT_smooth_radius(struct wmOperatorType *ot);
void CURVE_OT_smooth_tilt(struct wmOperatorType *ot);

void CURVE_OT_switch_direction(struct wmOperatorType *ot);
void CURVE_OT_subdivide(struct wmOperatorType *ot);
void CURVE_OT_make_segment(struct wmOperatorType *ot);
void CURVE_OT_spin(struct wmOperatorType *ot);
void CURVE_OT_vertex_add(struct wmOperatorType *ot);
void CURVE_OT_extrude(struct wmOperatorType *ot);
void CURVE_OT_cyclic_toggle(struct wmOperatorType *ot);

void CURVE_OT_match_texture_space(struct wmOperatorType *ot);

/* exported for editcurve_undo.cc */

struct GHash *ED_curve_keyindex_hash_duplicate(struct GHash *keyindex);
void ED_curve_keyindex_update_nurb(struct EditNurb *editnurb, struct Nurb *nu, struct Nurb *newnu);

/* exported for editcurve_pen.c */

int ed_editcurve_addvert(Curve *cu, EditNurb *editnurb, View3D *v3d, const float location_init[3]);
bool curve_toggle_cyclic(View3D *v3d, ListBase *editnurb, int direction);
void ed_dissolve_bez_segment(BezTriple *bezt_prev,
                             BezTriple *bezt_next,
                             const Nurb *nu,
                             const Curve *cu,
                             const uint span_len,
                             const uint span_step[2]);

/* helper functions */
void ed_editnurb_translate_flag(struct ListBase *editnurb,
                                uint8_t flag,
                                const float vec[3],
                                bool is_2d);
/**
 * Only for #OB_SURF.
 */
bool ed_editnurb_extrude_flag(struct EditNurb *editnurb, uint8_t flag);
/**
 * \param axis: is in world-space.
 * \param cent: is in object-space.
 */
bool ed_editnurb_spin(float viewmat[4][4],
                      struct View3D *v3d,
                      struct Object *obedit,
                      const float axis[3],
                      const float cent[3]);

/* editcurve_select.c */

void CURVE_OT_de_select_first(struct wmOperatorType *ot);
void CURVE_OT_de_select_last(struct wmOperatorType *ot);
void CURVE_OT_select_all(struct wmOperatorType *ot);
void CURVE_OT_select_linked(struct wmOperatorType *ot);
void CURVE_OT_select_linked_pick(struct wmOperatorType *ot);
void CURVE_OT_select_row(struct wmOperatorType *ot);
void CURVE_OT_select_next(struct wmOperatorType *ot);
void CURVE_OT_select_previous(struct wmOperatorType *ot);
void CURVE_OT_select_more(struct wmOperatorType *ot);
void CURVE_OT_select_less(struct wmOperatorType *ot);
void CURVE_OT_select_random(struct wmOperatorType *ot);
void CURVE_OT_select_nth(struct wmOperatorType *ot);
void CURVE_OT_select_similar(struct wmOperatorType *ot);
void CURVE_OT_shortest_path_pick(struct wmOperatorType *ot);

/* editcurve_add.c */

void CURVE_OT_primitive_bezier_curve_add(struct wmOperatorType *ot);
void CURVE_OT_primitive_bezier_circle_add(struct wmOperatorType *ot);
void CURVE_OT_primitive_nurbs_curve_add(struct wmOperatorType *ot);
void CURVE_OT_primitive_nurbs_circle_add(struct wmOperatorType *ot);
void CURVE_OT_primitive_nurbs_path_add(struct wmOperatorType *ot);

void SURFACE_OT_primitive_nurbs_surface_curve_add(struct wmOperatorType *ot);
void SURFACE_OT_primitive_nurbs_surface_circle_add(struct wmOperatorType *ot);
void SURFACE_OT_primitive_nurbs_surface_surface_add(struct wmOperatorType *ot);
void SURFACE_OT_primitive_nurbs_surface_cylinder_add(struct wmOperatorType *ot);
void SURFACE_OT_primitive_nurbs_surface_sphere_add(struct wmOperatorType *ot);
void SURFACE_OT_primitive_nurbs_surface_torus_add(struct wmOperatorType *ot);

/* editcurve_query.c */

bool ED_curve_pick_vert(struct ViewContext *vc,
                        short sel,
                        struct Nurb **r_nurb,
                        struct BezTriple **r_bezt,
                        struct BPoint **r_bp,
                        short *r_handle,
                        struct Base **r_base);
/**
 * \param sel_dist_mul: A multiplier on the default select distance.
 */
bool ED_curve_pick_vert_ex(struct ViewContext *vc,
                           short sel,
                           int dist_px,
                           struct Nurb **r_nurb,
                           struct BezTriple **r_bezt,
                           struct BPoint **r_bp,
                           short *r_handle,
                           struct Base **r_base);
void ED_curve_nurb_vert_selected_find(
    Curve *cu, View3D *v3d, Nurb **r_nu, BezTriple **r_bezt, BPoint **r_bp);

/* editcurve_paint.c */

void CURVE_OT_draw(struct wmOperatorType *ot);

/* editcurve_pen.c */

void CURVE_OT_pen(struct wmOperatorType *ot);
struct wmKeyMap *curve_pen_modal_keymap(struct wmKeyConfig *keyconf);

#ifdef __cplusplus
}
#endif
