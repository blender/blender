/* SPDX-FileCopyrightText: 2009 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

struct ARegion;
struct SpaceText;
struct Text;
struct UndoStep;
struct UndoType;
struct bContext;

bool ED_text_activate_in_screen(bContext *C, Text *text);

/**
 * Moves the view to the cursor location, also used to make sure the view isn't outside the file.
 */
void ED_text_scroll_to_cursor(SpaceText *st, ARegion *region, bool center);

/**
 * Takes a cursor (row, character) and returns x,y pixel coords.
 */
bool ED_text_region_location_from_cursor(SpaceText *st,
                                         ARegion *region,
                                         const int cursor_co[2],
                                         int r_pixel_co[2]);

/* text_undo.cc */

/** Export for ED_undo_sys. */
void ED_text_undosys_type(UndoType *ut);

/** Use operator system to finish the undo step. */
UndoStep *ED_text_undo_push_init(bContext *C);

/* `text_format.cc` */

const char *ED_text_format_comment_line_prefix(Text *text);

bool ED_text_is_syntax_highlight_supported(Text *text);
