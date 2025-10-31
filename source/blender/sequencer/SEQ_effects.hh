/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_sequence_types.h"
#include "DNA_vec_types.h"

#include "BLI_math_vector_types.hh"
#include "BLI_vector.hh"

/** \file
 * \ingroup sequencer
 */

struct Strip;
struct VFont;

namespace blender::seq {

void effect_ensure_initialized(Strip *strip);
void effect_free(Strip *strip);
int effect_get_num_inputs(int strip_type);
bool effect_is_transition(StripType type);
void effect_text_font_set(Strip *strip, VFont *font);
bool effects_can_render_text(const Strip *strip);

struct CharInfo {
  int index = 0;
  int offset = 0; /* Offset in bytes within text buffer. */
  int byte_length = 0;
  float2 position{0.0f, 0.0f};
  int advance_x = 0;
  bool do_wrap = false;
};

struct LineInfo {
  Vector<CharInfo> characters;
  int width;
};

struct TextVarsRuntime {
  Vector<LineInfo> lines;

  rcti text_boundbox; /* Bound-box used for box drawing and selection. */
  int line_height;
  int font_descender;
  int character_count;
  int font;
  bool editing_is_active; /* UI uses this to differentiate behavior. */
};

}  // namespace blender::seq
