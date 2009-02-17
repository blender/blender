/**
 * $Id:
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef ED_CURVE_INTERN_H
#define ED_CURVE_INTERN_H

/* internal exports only */
struct wmOperatorType;

/* lorem.c */
char *ED_lorem;

/* editfont.c */
enum { DEL_ALL, DEL_NEXT_CHAR, DEL_PREV_CHAR, DEL_SELECTION, DEL_NEXT_SEL, DEL_PREV_SEL };
enum { CASE_LOWER, CASE_UPPER };
enum { LINE_BEGIN, LINE_END, PREV_CHAR, NEXT_CHAR, PREV_WORD, NEXT_WORD,
       PREV_LINE, NEXT_LINE, PREV_PAGE, NEXT_PAGE };

void FONT_OT_insert_text(struct wmOperatorType *ot);
void FONT_OT_line_break(struct wmOperatorType *ot);
void FONT_OT_insert_lorem(struct wmOperatorType *ot);
void FONT_OT_text_to_object(struct wmOperatorType *ot);

void FONT_OT_toggle_case(struct wmOperatorType *ot);
void FONT_OT_set_case(struct wmOperatorType *ot);
void FONT_OT_toggle_style(struct wmOperatorType *ot);
void FONT_OT_set_style(struct wmOperatorType *ot);
void FONT_OT_set_material(struct wmOperatorType *ot);

void FONT_OT_copy_text(struct wmOperatorType *ot);
void FONT_OT_cut_text(struct wmOperatorType *ot);
void FONT_OT_paste_text(struct wmOperatorType *ot);
void FONT_OT_paste_file(struct wmOperatorType *ot);
void FONT_OT_paste_buffer(struct wmOperatorType *ot);

void FONT_OT_move(struct wmOperatorType *ot);
void FONT_OT_move_select(struct wmOperatorType *ot);
void FONT_OT_delete(struct wmOperatorType *ot);

void FONT_OT_change_character(struct wmOperatorType *ot);
void FONT_OT_change_spacing(struct wmOperatorType *ot);

/* editcurve.c */
void CURVE_OT_hide(struct wmOperatorType *ot);
void CURVE_OT_reveal(struct wmOperatorType *ot);

void CURVE_OT_separate(struct wmOperatorType *ot);
void CURVE_OT_duplicate(struct wmOperatorType *ot);
void CURVE_OT_delete(struct wmOperatorType *ot);

void CURVE_OT_set_weight(struct wmOperatorType *ot);
void CURVE_OT_set_radius(struct wmOperatorType *ot);
void CURVE_OT_set_spline_type(struct wmOperatorType *ot);
void CURVE_OT_set_handle_type(struct wmOperatorType *ot);
void CURVE_OT_set_smooth(struct wmOperatorType *ot);
void CURVE_OT_clear_tilt(struct wmOperatorType *ot);

void CURVE_OT_smooth(struct wmOperatorType *ot);
void CURVE_OT_smooth_radius(struct wmOperatorType *ot);

void CURVE_OT_de_select_first(struct wmOperatorType *ot);
void CURVE_OT_de_select_last(struct wmOperatorType *ot);
void CURVE_OT_de_select_all(struct wmOperatorType *ot);
void CURVE_OT_select_inverse(struct wmOperatorType *ot);
void CURVE_OT_select_linked(struct wmOperatorType *ot);
void CURVE_OT_select_row(struct wmOperatorType *ot);
void CURVE_OT_select_next(struct wmOperatorType *ot);
void CURVE_OT_select_previous(struct wmOperatorType *ot);
void CURVE_OT_select_more(struct wmOperatorType *ot);
void CURVE_OT_select_less(struct wmOperatorType *ot);
void CURVE_OT_select_random(struct wmOperatorType *ot);
void CURVE_OT_select_every_nth(struct wmOperatorType *ot);

void CURVE_OT_switch_direction(struct wmOperatorType *ot);
void CURVE_OT_subdivide(struct wmOperatorType *ot);
void CURVE_OT_make_segment(struct wmOperatorType *ot);
void CURVE_OT_spin(struct wmOperatorType *ot);
void CURVE_OT_add_vertex(struct wmOperatorType *ot);
void CURVE_OT_extrude(struct wmOperatorType *ot);
void CURVE_OT_toggle_cyclic(struct wmOperatorType *ot);

void CURVE_OT_specials_menu(struct wmOperatorType *ot);

#endif /* ED_UTIL_INTERN_H */

