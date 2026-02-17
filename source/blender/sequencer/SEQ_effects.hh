/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_sequence_types.h"
#include "DNA_vec_types.h"

#include "BLI_math_vector_types.hh"
#include "BLI_vector.hh"

namespace blender {

/** \file
 * \ingroup sequencer
 */

struct Strip;
struct VFont;

namespace seq {

void effect_ensure_initialized(Strip *strip);
void effect_free(Strip *strip);

/* Returns the minimum number of inputs needed by the effect type.
 * Note: some effects (compositor) will return zero; they can
 * take variable number of inputs. */
int effect_type_get_min_num_inputs(StripType type);
bool strip_type_is_effect(StripType type);
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

}  // namespace seq
}  // namespace blender
