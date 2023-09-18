/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sptext
 */

#pragma once

/* internal exports only */

struct ARegion;
struct ScrArea;
struct SpaceText;
struct Text;
struct TextLine;
struct bContext;
struct wmOperatorType;

#ifdef __cplusplus
extern "C" {
#endif

/* `text_draw.cc` */

void draw_text_main(SpaceText *st, ARegion *region);

void text_update_line_edited(TextLine *line);
void text_update_edited(Text *text);
void text_update_character_width(SpaceText *st);
/**
 * Takes an area instead of a region, use for listeners.
 */
void text_scroll_to_cursor__area(SpaceText *st, ScrArea *area, bool center);
void text_update_cursor_moved(bContext *C);

/* Padding around line numbers in character widths. */
#define TXT_NUMCOL_PAD 1.0f
/* Total width of the optional line numbers column. */
#define TXT_NUMCOL_WIDTH(st) \
  ((st)->runtime.cwidth_px * ((st)->runtime.line_number_display_digits + (2 * TXT_NUMCOL_PAD)))

/* Padding on left of body text in character units. */
#define TXT_BODY_LPAD 1.0f
/* Left position of body text. */
#define TXT_BODY_LEFT(st) \
  ((st)->showlinenrs ? TXT_NUMCOL_WIDTH(st) : 0) + (TXT_BODY_LPAD * (st)->runtime.cwidth_px)

#define TXT_SCROLL_WIDTH U.widget_unit
#define TXT_SCROLL_SPACE ((int)(0.1f * U.widget_unit))

/* Space between lines, in relation to letter height. */
#define TXT_LINE_VPAD 0.3f
/* Space between lines. */
#define TXT_LINE_SPACING(st) ((int)(TXT_LINE_VPAD * st->runtime.lheight_px))
/* Total height of each line. */
#define TXT_LINE_HEIGHT(st) ((int)((1.0f + TXT_LINE_VPAD) * st->runtime.lheight_px))

#define SUGG_LIST_SIZE 7
#define SUGG_LIST_WIDTH 20
#define DOC_WIDTH 40
#define DOC_HEIGHT 10

#define TOOL_SUGG_LIST 0x01

int wrap_width(const SpaceText *st, ARegion *region);
/**
 * Sets (offl, offc) for transforming (line, curs) to its wrapped position.
 */
void wrap_offset(
    const SpaceText *st, ARegion *region, TextLine *linein, int cursin, int *offl, int *offc);
/**
 * cursin - mem, offc - view.
 */
void wrap_offset_in_line(
    const SpaceText *st, ARegion *region, TextLine *linein, int cursin, int *offl, int *offc);
int text_get_char_pos(const SpaceText *st, const char *line, int cur);

void text_drawcache_tag_update(SpaceText *st, bool full);
void text_free_caches(SpaceText *st);

bool text_do_suggest_select(SpaceText *st, ARegion *region, const int mval[2]);
void text_pop_suggest_list();

int text_get_visible_lines(const SpaceText *st, ARegion *region, const char *str);
int text_get_span_wrap(const SpaceText *st, ARegion *region, TextLine *from, TextLine *to);
int text_get_total_lines(SpaceText *st, ARegion *region);

/* `text_ops.cc` */

enum {
  LINE_BEGIN,
  LINE_END,
  FILE_TOP,
  FILE_BOTTOM,
  PREV_CHAR,
  NEXT_CHAR,
  PREV_WORD,
  NEXT_WORD,
  PREV_LINE,
  NEXT_LINE,
  PREV_PAGE,
  NEXT_PAGE
};
enum { DEL_NEXT_CHAR, DEL_PREV_CHAR, DEL_NEXT_WORD, DEL_PREV_WORD };

void TEXT_OT_new(wmOperatorType *ot);
void TEXT_OT_open(wmOperatorType *ot);
void TEXT_OT_reload(wmOperatorType *ot);
void TEXT_OT_unlink(wmOperatorType *ot);
void TEXT_OT_save(wmOperatorType *ot);
void TEXT_OT_save_as(wmOperatorType *ot);
void TEXT_OT_make_internal(wmOperatorType *ot);
void TEXT_OT_run_script(wmOperatorType *ot);
void TEXT_OT_refresh_pyconstraints(wmOperatorType *ot);

void TEXT_OT_paste(wmOperatorType *ot);
void TEXT_OT_copy(wmOperatorType *ot);
void TEXT_OT_cut(wmOperatorType *ot);
void TEXT_OT_duplicate_line(wmOperatorType *ot);

void TEXT_OT_convert_whitespace(wmOperatorType *ot);
void TEXT_OT_comment_toggle(wmOperatorType *ot);
void TEXT_OT_unindent(wmOperatorType *ot);
void TEXT_OT_indent(wmOperatorType *ot);
void TEXT_OT_indent_or_autocomplete(wmOperatorType *ot);

void TEXT_OT_line_break(wmOperatorType *ot);
void TEXT_OT_insert(wmOperatorType *ot);

void TEXT_OT_select_line(wmOperatorType *ot);
void TEXT_OT_select_all(wmOperatorType *ot);
void TEXT_OT_select_word(wmOperatorType *ot);

void TEXT_OT_move_lines(wmOperatorType *ot);

void TEXT_OT_jump(wmOperatorType *ot);
void TEXT_OT_move(wmOperatorType *ot);
void TEXT_OT_move_select(wmOperatorType *ot);
void TEXT_OT_delete(wmOperatorType *ot);
void TEXT_OT_overwrite_toggle(wmOperatorType *ot);

void TEXT_OT_scroll(wmOperatorType *ot);
void TEXT_OT_scroll_bar(wmOperatorType *ot);
void TEXT_OT_selection_set(wmOperatorType *ot);
void TEXT_OT_cursor_set(wmOperatorType *ot);
void TEXT_OT_line_number(wmOperatorType *ot);

/* find = find indicated text */
void TEXT_OT_find(wmOperatorType *ot);
void TEXT_OT_find_set_selected(wmOperatorType *ot);
void TEXT_OT_replace(wmOperatorType *ot);
void TEXT_OT_replace_set_selected(wmOperatorType *ot);
void TEXT_OT_jump_to_file_at_point_internal(wmOperatorType *ot);

/* text_find = open properties, activate search button */
void TEXT_OT_start_find(wmOperatorType *ot);

void TEXT_OT_to_3d_object(wmOperatorType *ot);

void TEXT_OT_resolve_conflict(wmOperatorType *ot);

bool text_space_edit_poll(bContext *C);

/* `text_autocomplete.cc` */

void TEXT_OT_autocomplete(wmOperatorType *ot);

/* `space_text.cc` */

extern const char *text_context_dir[]; /* doc access */

#ifdef __cplusplus
}
#endif
