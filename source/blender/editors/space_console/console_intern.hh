/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spconsole
 */

#pragma once

#include <optional>

#include "BLI_math_vector_types.hh"

namespace blender {

/* internal exports only */

struct ARegion;
struct ConsoleLine;
struct SpaceConsole;
struct bContext;
struct wmOperatorType;

struct SpaceConsole_Runtime {
  /** Character width in physical pixels. */
  int char_width_px = 0;
  /** Line height in physical pixels. */
  int line_height_px = 0;
};

/* `console_draw.cc` */

void console_textview_main(SpaceConsole *sc, const ARegion *region);
/* Needed to calculate the scroll-bar. */
int console_textview_height(SpaceConsole *sc, const ARegion *region);
int console_char_pick(SpaceConsole *sc, const ARegion *region, const int mval[2]);
/** Get the region-coordinate position for a character \a offset in the input line. */
std::optional<blender::int2> console_cursor_region_xy_get(const SpaceConsole *sc,
                                                          const ARegion *region,
                                                          int offset);

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
void CONSOLE_OT_select_all(wmOperatorType *ot);
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

}  // namespace blender
