/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_vec_types.h"

#include "BLI_math_vector_types.hh"
#include "BLI_vector.hh"

/** \file
 * \ingroup sequencer
 */

struct ImBuf;
struct Scene;
struct SeqRenderData;
struct Strip;
struct TextVars;

enum class StripEarlyOut {
  NoInput = -1,  /* No input needed. */
  DoEffect = 0,  /* No early out (do the effect). */
  UseInput1 = 1, /* Output = input1. */
  UseInput2 = 2, /* Output = input2. */
};

/* Wipe effect */
enum {
  DO_SINGLE_WIPE,
  DO_DOUBLE_WIPE,
  /* DO_BOX_WIPE, */   /* UNUSED */
  /* DO_CROSS_WIPE, */ /* UNUSED */
  DO_IRIS_WIPE,
  DO_CLOCK_WIPE,
};

struct SeqEffectHandle {
  /* constructors & destructor */
  /* init is _only_ called on first creation */
  void (*init)(Strip *strip);

  /* number of input strips needed
   * (called directly after construction) */
  int (*num_inputs)();

  /* load is called first time after readblenfile in
   * get_sequence_effect automatically */
  void (*load)(Strip *seqconst);

  /* duplicate */
  void (*copy)(Strip *dst, const Strip *src, int flag);

  /* destruct */
  void (*free)(Strip *strip, bool do_id_user);

  StripEarlyOut (*early_out)(const Strip *strip, float fac);

  /* sets the default `fac` value */
  void (*get_default_fac)(const Scene *scene,
                          const Strip *strip,
                          float timeline_frame,
                          float *fac);

  /* execute the effect */
  ImBuf *(*execute)(const SeqRenderData *context,
                    Strip *strip,
                    float timeline_frame,
                    float fac,
                    ImBuf *ibuf1,
                    ImBuf *ibuf2);
};

SeqEffectHandle SEQ_effect_handle_get(Strip *strip);
int SEQ_effect_get_num_inputs(int strip_type);
void SEQ_effect_text_font_unload(TextVars *data, bool do_id_user);
void SEQ_effect_text_font_load(TextVars *data, bool do_id_user);
bool SEQ_effects_can_render_text(const Strip *strip);

namespace blender::seq {

struct CharInfo {
  int index = 0;
  const char *str_ptr = nullptr;
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
