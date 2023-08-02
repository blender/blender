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

/* `console_draw.cc` */

void console_textview_main(SpaceConsole *sc, const ARegion *region);
/* Needed to calculate the scroll-bar. */
int console_textview_height(SpaceConsole *sc, const ARegion *region);
int console_char_pick(SpaceConsole *sc, const ARegion *region, const int mval[2]);

void console_scrollback_prompt_begin(SpaceConsole *sc, ConsoleLine *cl_dummy);
void console_scrollback_prompt_end(SpaceConsole *sc, ConsoleLine *cl_dummy);

/* `console_ops.cc` */

void console_history_free(SpaceConsole *sc, ConsoleLine *cl);
void console_scrollback_free(SpaceConsole *sc, ConsoleLine *cl);
ConsoleLine *console_history_add_str(SpaceConsole *sc, char *str, bool own);
ConsoleLine *console_scrollback_add_str(SpaceConsole *sc, char *str, bool own);

ConsoleLine *console_history_verify(const bContext *C);

void console_textview_update_rect(SpaceConsole *sc, ARegion *region);

void CONSOLE_OT_move(wmOperatorType *ot);
void CONSOLE_OT_delete(wmOperatorType *ot);
void CONSOLE_OT_insert(wmOperatorType *ot);

void CONSOLE_OT_indent(wmOperatorType *ot);
void CONSOLE_OT_indent_or_autocomplete(wmOperatorType *ot);
void CONSOLE_OT_unindent(wmOperatorType *ot);

void CONSOLE_OT_history_append(wmOperatorType *ot);
void CONSOLE_OT_scrollback_append(wmOperatorType *ot);

void CONSOLE_OT_clear(wmOperatorType *ot);
void CONSOLE_OT_clear_line(wmOperatorType *ot);
void CONSOLE_OT_history_cycle(wmOperatorType *ot);
void CONSOLE_OT_copy(wmOperatorType *ot);
void CONSOLE_OT_paste(wmOperatorType *ot);
void CONSOLE_OT_select_set(wmOperatorType *ot);
void CONSOLE_OT_select_word(wmOperatorType *ot);

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
