/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

struct ImBuf;
struct SeqRenderData;
struct Sequence;
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
  bool multithreaded;
  bool supports_mask;

  /* constructors & destructor */
  /* init is _only_ called on first creation */
  void (*init)(Sequence *seq);

  /* number of input strips needed
   * (called directly after construction) */
  int (*num_inputs)();

  /* load is called first time after readblenfile in
   * get_sequence_effect automatically */
  void (*load)(Sequence *seqconst);

  /* duplicate */
  void (*copy)(Sequence *dst, const Sequence *src, int flag);

  /* destruct */
  void (*free)(Sequence *seq, bool do_id_user);

  StripEarlyOut (*early_out)(const Sequence *seq, float fac);

  /* sets the default `fac` value */
  void (*get_default_fac)(const Scene *scene,
                          const Sequence *seq,
                          float timeline_frame,
                          float *fac);

  /* execute the effect
   * sequence effects are only required to either support
   * float-rects or byte-rects
   * (mixed cases are handled one layer up...) */

  ImBuf *(*execute)(const SeqRenderData *context,
                    Sequence *seq,
                    float timeline_frame,
                    float fac,
                    ImBuf *ibuf1,
                    ImBuf *ibuf2,
                    ImBuf *ibuf3);

  ImBuf *(*init_execution)(const SeqRenderData *context, ImBuf *ibuf1, ImBuf *ibuf2, ImBuf *ibuf3);

  void (*execute_slice)(const SeqRenderData *context,
                        Sequence *seq,
                        float timeline_frame,
                        float fac,
                        const ImBuf *ibuf1,
                        const ImBuf *ibuf2,
                        const ImBuf *ibuf3,
                        int start_line,
                        int total_lines,
                        ImBuf *out);
};

SeqEffectHandle SEQ_effect_handle_get(Sequence *seq);
int SEQ_effect_get_num_inputs(int seq_type);
void SEQ_effect_text_font_unload(TextVars *data, bool do_id_user);
void SEQ_effect_text_font_load(TextVars *data, bool do_id_user);
