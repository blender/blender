/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */
#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Main;
struct Text;
struct TextLine;

#include "BLI_compiler_attrs.h"

/**
 * \note caller must handle `compiled` member.
 */
void BKE_text_free_lines(struct Text *text);
struct Text *BKE_text_add(struct Main *bmain, const char *name);
/**
 * Use to a valid UTF-8 sequences.
 * this function replaces extended ascii characters.
 */
int txt_extended_ascii_as_utf8(char **str);
bool BKE_text_reload(struct Text *text);
/**
 * Load a text file.
 *
 * \param is_internal: If \a true, this text data-block only exists in memory,
 * not as a file on disk.
 *
 * \note text data-blocks have no real user but have 'fake user' enabled by default
 */
struct Text *BKE_text_load_ex(struct Main *bmain,
                              const char *filepath,
                              const char *relbase,
                              bool is_internal) ATTR_NONNULL(1, 2, 3);
/**
 * Load a text file.
 *
 * \note Text data-blocks have no user by default, only the 'real user' flag.
 */
struct Text *BKE_text_load(struct Main *bmain, const char *filepath, const char *relbase)
    ATTR_NONNULL(1, 2, 3);
void BKE_text_clear(struct Text *text) ATTR_NONNULL(1);
void BKE_text_write(struct Text *text, const char *str, int str_len) ATTR_NONNULL(1, 2);
/**
 * \return codes:
 * -  0 if filepath on disk is the same or Text is in memory only.
 * -  1 if filepath has been modified on disk since last local edit.
 * -  2 if filepath on disk has been deleted.
 * - -1 is returned if an error occurs.
 */
int BKE_text_file_modified_check(struct Text *text);
void BKE_text_file_modified_ignore(struct Text *text);

char *txt_to_buf(struct Text *text, size_t *r_buf_strlen)
    ATTR_NONNULL(1, 2) ATTR_WARN_UNUSED_RESULT ATTR_RETURNS_NONNULL;
void txt_clean_text(struct Text *text);
void txt_order_cursors(struct Text *text, bool reverse);
int txt_find_string(struct Text *text, const char *findstr, int wrap, int match_case);
bool txt_has_sel(const struct Text *text);
int txt_get_span(struct TextLine *from, struct TextLine *to);
void txt_move_up(struct Text *text, bool sel);
void txt_move_down(struct Text *text, bool sel);
void txt_move_left(struct Text *text, bool sel);
void txt_move_right(struct Text *text, bool sel);
void txt_jump_left(struct Text *text, bool sel, bool use_init_step);
void txt_jump_right(struct Text *text, bool sel, bool use_init_step);
void txt_move_bof(struct Text *text, bool sel);
void txt_move_eof(struct Text *text, bool sel);
void txt_move_bol(struct Text *text, bool sel);
void txt_move_eol(struct Text *text, bool sel);
void txt_move_toline(struct Text *text, unsigned int line, bool sel);
/**
 * Moves to a certain byte in a line, not a certain utf8-character.
 */
void txt_move_to(struct Text *text, unsigned int line, unsigned int ch, bool sel);
void txt_pop_sel(struct Text *text);
void txt_delete_char(struct Text *text);
void txt_delete_word(struct Text *text);
void txt_delete_selected(struct Text *text);
void txt_sel_all(struct Text *text);
/**
 * Reverse of #txt_pop_sel
 * Clears the selection and ensures the cursor is located
 * at the selection (where the cursor is visually while editing).
 */
void txt_sel_clear(struct Text *text);
void txt_sel_line(struct Text *text);
void txt_sel_set(struct Text *text, int startl, int startc, int endl, int endc);
char *txt_sel_to_buf(const struct Text *text, size_t *r_buf_strlen);
void txt_insert_buf(struct Text *text, const char *in_buffer, int in_buffer_len)
    ATTR_NONNULL(1, 2);
void txt_split_curline(struct Text *text);
void txt_backspace_char(struct Text *text);
void txt_backspace_word(struct Text *text);
bool txt_add_char(struct Text *text, unsigned int add);
bool txt_add_raw_char(struct Text *text, unsigned int add);
bool txt_replace_char(struct Text *text, unsigned int add);
bool txt_unindent(struct Text *text);
void txt_indent(struct Text *text);
void txt_comment(struct Text *text, const char *prefix);
bool txt_uncomment(struct Text *text, const char *prefix);
void txt_move_lines(struct Text *text, int direction);
void txt_duplicate_line(struct Text *text);
int txt_setcurr_tab_spaces(struct Text *text, int space);
bool txt_cursor_is_line_start(const struct Text *text);
bool txt_cursor_is_line_end(const struct Text *text);

int txt_calc_tab_left(struct TextLine *tl, int ch);
int txt_calc_tab_right(struct TextLine *tl, int ch);

/**
 * Utility functions, could be moved somewhere more generic but are python/text related.
 */
int text_check_bracket(char ch);
bool text_check_delim(char ch);
bool text_check_digit(char ch);
bool text_check_identifier(char ch);
bool text_check_identifier_nodigit(char ch);
bool text_check_whitespace(char ch);
int text_find_identifier_start(const char *str, int i);

/* EVIL: defined in `bpy_interface.c`. */
extern int text_check_identifier_unicode(unsigned int ch);
extern int text_check_identifier_nodigit_unicode(unsigned int ch);

enum {
  TXT_MOVE_LINE_UP = -1,
  TXT_MOVE_LINE_DOWN = 1,
};

/* Fast non-validating buffer conversion for undo. */

/**
 * Create a buffer, the only requirement is #txt_from_buf_for_undo can decode it.
 */
char *txt_to_buf_for_undo(struct Text *text, size_t *r_buf_len)
    ATTR_NONNULL(1, 2) ATTR_WARN_UNUSED_RESULT ATTR_RETURNS_NONNULL;
/**
 * Decode a buffer from #txt_to_buf_for_undo.
 */
void txt_from_buf_for_undo(struct Text *text, const char *buf, size_t buf_len) ATTR_NONNULL(1, 2);

#ifdef __cplusplus
}
#endif
