/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spconsole
 */

#pragma once

/* internal exports only */

struct ConsoleLine;
struct bContext;
struct wmOperatorType;

/* console_draw.c */

void console_textview_main(struct SpaceConsole *sc, const struct ARegion *region);
/* Needed to calculate the scroll-bar. */
int console_textview_height(struct SpaceConsole *sc, const struct ARegion *region);
int console_char_pick(struct SpaceConsole *sc, const struct ARegion *region, const int mval[2]);

void console_scrollback_prompt_begin(struct SpaceConsole *sc, ConsoleLine *cl_dummy);
void console_scrollback_prompt_end(struct SpaceConsole *sc, ConsoleLine *cl_dummy);

/* console_ops.c */

void console_history_free(SpaceConsole *sc, ConsoleLine *cl);
void console_scrollback_free(SpaceConsole *sc, ConsoleLine *cl);
ConsoleLine *console_history_add_str(struct SpaceConsole *sc, char *str, bool own);
ConsoleLine *console_scrollback_add_str(struct SpaceConsole *sc, char *str, bool own);

ConsoleLine *console_history_verify(const struct bContext *C);

void console_textview_update_rect(SpaceConsole *sc, ARegion *region);

void CONSOLE_OT_move(struct wmOperatorType *ot);
void CONSOLE_OT_delete(struct wmOperatorType *ot);
void CONSOLE_OT_insert(struct wmOperatorType *ot);

void CONSOLE_OT_indent(struct wmOperatorType *ot);
void CONSOLE_OT_indent_or_autocomplete(struct wmOperatorType *ot);
void CONSOLE_OT_unindent(struct wmOperatorType *ot);

void CONSOLE_OT_history_append(struct wmOperatorType *ot);
void CONSOLE_OT_scrollback_append(struct wmOperatorType *ot);

void CONSOLE_OT_clear(struct wmOperatorType *ot);
void CONSOLE_OT_clear_line(struct wmOperatorType *ot);
void CONSOLE_OT_history_cycle(struct wmOperatorType *ot);
void CONSOLE_OT_copy(struct wmOperatorType *ot);
void CONSOLE_OT_paste(struct wmOperatorType *ot);
void CONSOLE_OT_select_set(struct wmOperatorType *ot);
void CONSOLE_OT_select_word(struct wmOperatorType *ot);

enum { LINE_BEGIN, LINE_END, PREV_CHAR, NEXT_CHAR, PREV_WORD, NEXT_WORD };
enum {
  DEL_NEXT_CHAR,
  DEL_PREV_CHAR,
  DEL_NEXT_WORD,
  DEL_PREV_WORD,
  DEL_SELECTION,
  DEL_NEXT_SEL,
  DEL_PREV_SEL
};
