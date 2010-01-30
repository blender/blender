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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef ED_TEXT_INTERN_H
#define ED_TEXT_INTERN_H

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

int text_check_bracket(char ch);
int text_check_delim(char ch);
int text_check_digit(char ch);
int text_check_identifier(char ch);
int text_check_whitespace(char ch);

int text_font_width_character(struct SpaceText *st);
int text_font_width(struct SpaceText *st, char *str);

void text_update_line_edited(struct Text *text, struct TextLine *line);
void text_update_edited(struct Text *text);
void text_update_character_width(struct SpaceText *st);
void text_update_cursor_moved(struct bContext *C);

#define TEXTXLOC		(st->cwidth * st->linenrs_tot)

#define SUGG_LIST_SIZE	7
#define SUGG_LIST_WIDTH	20
#define DOC_WIDTH		40
#define DOC_HEIGHT		10

#define TOOL_SUGG_LIST	0x01
#define TOOL_DOCUMENT	0x02

#define TMARK_GRP_CUSTOM	0x00010000	/* Lower 2 bytes used for Python groups */
#define TMARK_GRP_FINDALL	0x00020000

typedef struct FlattenString {
	char fixedbuf[256];
	int fixedaccum[256];

	char *buf;
	int *accum;
	int pos, len;
} FlattenString;

int flatten_string(struct SpaceText *st, FlattenString *fs, char *in);
void flatten_string_free(FlattenString *fs);

int wrap_width(struct SpaceText *st, struct ARegion *ar);
void wrap_offset(struct SpaceText *st, struct ARegion *ar, struct TextLine *linein, int cursin, int *offl, int *offc);

int text_file_modified(struct Text *text);

int text_do_suggest_select(struct SpaceText *st, struct ARegion *ar);
void text_pop_suggest_list();


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

void TEXT_OT_convert_whitespace(struct wmOperatorType *ot);
void TEXT_OT_uncomment(struct wmOperatorType *ot);
void TEXT_OT_comment(struct wmOperatorType *ot);
void TEXT_OT_unindent(struct wmOperatorType *ot);
void TEXT_OT_indent(struct wmOperatorType *ot);

void TEXT_OT_line_break(struct wmOperatorType *ot);
void TEXT_OT_insert(struct wmOperatorType *ot);

void TEXT_OT_markers_clear(struct wmOperatorType *ot);
void TEXT_OT_next_marker(struct wmOperatorType *ot);
void TEXT_OT_previous_marker(struct wmOperatorType *ot);

void TEXT_OT_select_line(struct wmOperatorType *ot);
void TEXT_OT_select_all(struct wmOperatorType *ot);

void TEXT_OT_jump(struct wmOperatorType *ot);
void TEXT_OT_move(struct wmOperatorType *ot);
void TEXT_OT_move_select(struct wmOperatorType *ot);
void TEXT_OT_delete(struct wmOperatorType *ot);
void TEXT_OT_overwrite_toggle(struct wmOperatorType *ot);

void TEXT_OT_scroll(struct wmOperatorType *ot);
void TEXT_OT_scroll_bar(struct wmOperatorType *ot);
void TEXT_OT_cursor_set(struct wmOperatorType *ot);
void TEXT_OT_line_number(struct wmOperatorType *ot);

void TEXT_OT_properties(struct wmOperatorType *ot);

void TEXT_OT_find(struct wmOperatorType *ot);
void TEXT_OT_find_set_selected(struct wmOperatorType *ot);
void TEXT_OT_replace(struct wmOperatorType *ot);
void TEXT_OT_replace_set_selected(struct wmOperatorType *ot);
void TEXT_OT_mark_all(struct wmOperatorType *ot);

void TEXT_OT_to_3d_object(struct wmOperatorType *ot);

void TEXT_OT_resolve_conflict(struct wmOperatorType *ot);

#endif /* ED_TEXT_INTERN_H */

