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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_text/text_intern.h
 *  \ingroup sptext
 */

#ifndef __TEXT_INTERN_H__
#define __TEXT_INTERN_H__

/* internal exports only */

struct ARegion;
struct ARegionType;
struct bContext;
struct ReportList;
struct ScrArea;
struct SpaceText;
struct Text;
struct TextLine;
struct wmOperatorType;
struct wmWindowManager;

/* text_draw.c */
void draw_text_main(struct SpaceText *st, struct ARegion *ar);

void text_update_line_edited(struct TextLine *line);
void text_update_edited(struct Text *text);
void text_update_character_width(struct SpaceText *st);
void text_scroll_to_cursor(struct SpaceText *st, struct ARegion *ar, const bool center);
void text_scroll_to_cursor__area(struct SpaceText *st, struct ScrArea *sa, const bool center);
void text_update_cursor_moved(struct bContext *C);

#define TXT_OFFSET			((int)(0.5f * U.widget_unit))
#define TXT_SCROLL_WIDTH	U.widget_unit
#define TXT_SCROLL_SPACE	((int)(0.1f * U.widget_unit))
#define TXT_LINE_SPACING	((int)(0.3f * st->lheight_dpi)) /* space between lines */
#define TEXTXLOC			(st->cwidth * st->linenrs_tot)

#define SUGG_LIST_SIZE	7
#define SUGG_LIST_WIDTH	20
#define DOC_WIDTH		40
#define DOC_HEIGHT		10

#define TOOL_SUGG_LIST	0x01
#define TOOL_DOCUMENT	0x02

int wrap_width(struct SpaceText *st, struct ARegion *ar);
void wrap_offset(struct SpaceText *st, struct ARegion *ar, struct TextLine *linein, int cursin, int *offl, int *offc);
void wrap_offset_in_line(struct SpaceText *st, struct ARegion *ar, struct TextLine *linep, int cursin, int *offl, int *offc);
int text_get_char_pos(struct SpaceText *st, const char *line, int cur);

void text_drawcache_tag_update(struct SpaceText *st, int full);
void text_free_caches(struct SpaceText *st);

int text_do_suggest_select(struct SpaceText *st, struct ARegion *ar);
void text_pop_suggest_list(void);

int text_get_visible_lines(struct SpaceText *st, struct ARegion *ar, const char *str);
int text_get_span_wrap(struct SpaceText *st, struct ARegion *ar, struct TextLine *from, struct TextLine *to);
int text_get_total_lines(struct SpaceText *st, struct ARegion *ar);

/* text_ops.c */
enum { LINE_BEGIN, LINE_END, FILE_TOP, FILE_BOTTOM, PREV_CHAR, NEXT_CHAR,
       PREV_WORD, NEXT_WORD, PREV_LINE, NEXT_LINE, PREV_PAGE, NEXT_PAGE };
enum { DEL_NEXT_CHAR, DEL_PREV_CHAR, DEL_NEXT_WORD, DEL_PREV_WORD };

void TEXT_OT_new(struct wmOperatorType *ot);
void TEXT_OT_open(struct wmOperatorType *ot);
void TEXT_OT_reload(struct wmOperatorType *ot);
void TEXT_OT_unlink(struct wmOperatorType *ot);
void TEXT_OT_save(struct wmOperatorType *ot);
void TEXT_OT_save_as(struct wmOperatorType *ot);
void TEXT_OT_make_internal(struct wmOperatorType *ot);
void TEXT_OT_run_script(struct wmOperatorType *ot);
void TEXT_OT_refresh_pyconstraints(struct wmOperatorType *ot);

void TEXT_OT_paste(struct wmOperatorType *ot);
void TEXT_OT_copy(struct wmOperatorType *ot);
void TEXT_OT_cut(struct wmOperatorType *ot);
void TEXT_OT_duplicate_line(struct wmOperatorType *ot);

void TEXT_OT_convert_whitespace(struct wmOperatorType *ot);
void TEXT_OT_uncomment(struct wmOperatorType *ot);
void TEXT_OT_comment(struct wmOperatorType *ot);
void TEXT_OT_unindent(struct wmOperatorType *ot);
void TEXT_OT_indent(struct wmOperatorType *ot);

void TEXT_OT_line_break(struct wmOperatorType *ot);
void TEXT_OT_insert(struct wmOperatorType *ot);

void TEXT_OT_select_line(struct wmOperatorType *ot);
void TEXT_OT_select_all(struct wmOperatorType *ot);
void TEXT_OT_select_word(struct wmOperatorType *ot);

void TEXT_OT_move_lines(struct wmOperatorType *ot);

void TEXT_OT_jump(struct wmOperatorType *ot);
void TEXT_OT_move(struct wmOperatorType *ot);
void TEXT_OT_move_select(struct wmOperatorType *ot);
void TEXT_OT_delete(struct wmOperatorType *ot);
void TEXT_OT_overwrite_toggle(struct wmOperatorType *ot);

void TEXT_OT_scroll(struct wmOperatorType *ot);
void TEXT_OT_scroll_bar(struct wmOperatorType *ot);
void TEXT_OT_selection_set(struct wmOperatorType *ot);
void TEXT_OT_cursor_set(struct wmOperatorType *ot);
void TEXT_OT_line_number(struct wmOperatorType *ot);

void TEXT_OT_properties(struct wmOperatorType *ot);

/* find = find indicated text */
void TEXT_OT_find(struct wmOperatorType *ot);
void TEXT_OT_find_set_selected(struct wmOperatorType *ot);
void TEXT_OT_replace(struct wmOperatorType *ot);
void TEXT_OT_replace_set_selected(struct wmOperatorType *ot);

/* text_find = open properties, activate search button */
void TEXT_OT_start_find(struct wmOperatorType *ot);

void TEXT_OT_to_3d_object(struct wmOperatorType *ot);

void TEXT_OT_resolve_conflict(struct wmOperatorType *ot);

int text_space_edit_poll(struct bContext *C);

/* text_autocomplete.c */
void TEXT_OT_autocomplete(struct wmOperatorType *ot);

/* space_text.c */
extern const char *text_context_dir[]; /* doc access */

#endif /* __TEXT_INTERN_H__ */

