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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef ED_CONSOLE_INTERN_H
#define ED_CONSOLE_INTERN_H

/* internal exports only */

struct ConsoleLine;
struct wmOperatorType;
struct ReportList;
struct bContext;

/* console_draw.c */
void console_text_main(struct SpaceConsole *sc, struct ARegion *ar, struct ReportList *reports);
int console_text_height(struct SpaceConsole *sc, struct ARegion *ar, struct ReportList *reports); /* needed to calculate the scrollbar */
void *console_text_pick(struct SpaceConsole *sc, struct ARegion *ar, struct ReportList *reports, int mouse_y); /* needed for selection */
int console_char_pick(struct SpaceConsole *sc, struct ARegion *ar, ReportList *reports, int mval[2]);

/* console_ops.c */
void console_history_free(SpaceConsole *sc, ConsoleLine *cl);
void console_scrollback_free(SpaceConsole *sc, ConsoleLine *cl);
ConsoleLine *console_history_add_str(const struct bContext *C, char *str, int own);
ConsoleLine *console_scrollback_add_str(const struct bContext *C, char *str, int own);

ConsoleLine *console_history_verify(const struct bContext *C);

int console_report_mask(SpaceConsole *sc);


void CONSOLE_OT_move(struct wmOperatorType *ot);
void CONSOLE_OT_delete(struct wmOperatorType *ot);
void CONSOLE_OT_insert(struct wmOperatorType *ot);

void CONSOLE_OT_history_append(struct wmOperatorType *ot);
void CONSOLE_OT_scrollback_append(struct wmOperatorType *ot);

void CONSOLE_OT_clear(struct wmOperatorType *ot);
void CONSOLE_OT_history_cycle(struct wmOperatorType *ot);
void CONSOLE_OT_copy(struct wmOperatorType *ot);
void CONSOLE_OT_paste(struct wmOperatorType *ot);
void CONSOLE_OT_select_set(struct wmOperatorType *ot);



/* console_report.c */
void CONSOLE_OT_select_pick(struct wmOperatorType *ot); /* report selection */
void CONSOLE_OT_select_all_toggle(struct wmOperatorType *ot);
void CONSOLE_OT_select_border(struct wmOperatorType *ot);

void CONSOLE_OT_report_replay(struct wmOperatorType *ot);
void CONSOLE_OT_report_delete(struct wmOperatorType *ot);
void CONSOLE_OT_report_copy(struct wmOperatorType *ot);



enum { LINE_BEGIN, LINE_END, PREV_CHAR, NEXT_CHAR, PREV_WORD, NEXT_WORD };
enum { DEL_ALL, DEL_NEXT_CHAR, DEL_PREV_CHAR, DEL_SELECTION, DEL_NEXT_SEL, DEL_PREV_SEL };

#endif /* ED_CONSOLE_INTERN_H */
