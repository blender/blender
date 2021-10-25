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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/curve/curve_intern.h
 *  \ingroup edcurve
 */


#ifndef __CURVE_INTERN_H__
#define __CURVE_INTERN_H__

/* internal exports only */
struct ListBase;
struct EditNurb;
struct Object;
struct wmOperatorType;
struct ViewContext;

/* editfont.c */
enum { DEL_ALL, DEL_NEXT_CHAR, DEL_PREV_CHAR, DEL_SELECTION, DEL_NEXT_SEL, DEL_PREV_SEL };
enum { CASE_LOWER, CASE_UPPER };
enum { LINE_BEGIN, LINE_END, PREV_CHAR, NEXT_CHAR, PREV_WORD, NEXT_WORD,
       PREV_LINE, NEXT_LINE, PREV_PAGE, NEXT_PAGE };

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
bool select_beztriple(BezTriple *bezt, bool selstatus, short flag, eVisible_Types hidden);
bool select_bpoint(BPoint *bp, bool selstatus, short flag, bool hidden);

void FONT_OT_text_insert(struct wmOperatorType *ot);
void FONT_OT_line_break(struct wmOperatorType *ot);
void FONT_OT_insert_lorem(struct wmOperatorType *ot);

void FONT_OT_case_toggle(struct wmOperatorType *ot);
void FONT_OT_case_set(struct wmOperatorType *ot);
void FONT_OT_style_toggle(struct wmOperatorType *ot);
void FONT_OT_style_set(struct wmOperatorType *ot);

void FONT_OT_select_all(struct wmOperatorType *ot);

void FONT_OT_text_copy(struct wmOperatorType *ot);
void FONT_OT_text_cut(struct wmOperatorType *ot);
void FONT_OT_text_paste(struct wmOperatorType *ot);
void FONT_OT_text_paste_from_file(struct wmOperatorType *ot);

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

bool ED_curve_pick_vert(
        struct ViewContext *vc, short sel, const int mval[2],
        struct Nurb **r_nurb, struct BezTriple **r_bezt, struct BPoint **r_bp, short *r_handle);

/* helper functions */
void ed_editnurb_translate_flag(struct ListBase *editnurb, short flag, const float vec[3]);
bool ed_editnurb_extrude_flag(struct EditNurb *editnurb, const short flag);
bool ed_editnurb_spin(float viewmat[4][4], struct Object *obedit, const float axis[3], const float cent[3]);

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

/* editcurve_paint.c */
void CURVE_OT_draw(struct wmOperatorType *ot);

#endif /* __CURVE_INTERN_H__ */
