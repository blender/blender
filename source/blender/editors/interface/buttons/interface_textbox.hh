/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup buttons
 */

#pragma once

#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_string_cursor_utf8.h"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

namespace blender {
struct ARegion;
struct TextboxState;

namespace ui {

struct ButtonTextBox;

constexpr int textbox_minimum_visible_lines = 1;

void textbox_add_scroll(ButtonTextBox *textbox, int step);

/** Scroll the textbox to make the text cursor visible. */
void textbox_scroll_to_cursor(ButtonTextBox *textbox);

/** Moves the text cursor under the `xy` point. */
void textbox_textedit_set_cursor_pos(ButtonTextBox *textbox,
                                     const ARegion *region,
                                     const float2 xy);

/** Returns the index of the line which contains the string offset. */
int textbox_wrapped_line_index_from_char_offset(Span<StringRef> lines, int offset);

/**
 * Moves te cursor in the textbox one line up/down and tries to maintain the horizontal offset in
 * pixels from the current line.
 */
void textbox_jump_line(ButtonTextBox *textbox,
                       eStrCursorJumpDirection direction,
                       const bool select);

/**
 * Wraps input text into lines, this may overwrite draw string if there is IME data available.
 * This also may override active font style.
 */
Vector<StringRef> textbox_wrap_lines(ButtonTextBox *textbox);

Vector<StringRef> textbox_wrap_placeholder(ButtonTextBox *textbox);

float textbox_grip_height();

/* Top/Bottom padding for text in a text-box. */
float textbox_vertical_padding();

TextboxState *textbox_ensure_state(ARegion *region, StringRefNull idname);

}  // namespace ui
}  // namespace blender
