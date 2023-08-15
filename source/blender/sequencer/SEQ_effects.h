/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#ifdef __cplusplus
extern "C" {
#endif

struct ImBuf;
struct SeqRenderData;
struct Sequence;
struct TextVars;

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
  void (*init)(struct Sequence *seq);

  /* number of input strips needed
   * (called directly after construction) */
  int (*num_inputs)(void);

  /* load is called first time after readblenfile in
   * get_sequence_effect automatically */
  void (*load)(struct Sequence *seqconst);

  /* duplicate */
  void (*copy)(struct Sequence *dst, struct Sequence *src, int flag);

  /* destruct */
  void (*free)(struct Sequence *seq, bool do_id_user);

  /* returns: -1: no input needed,
   * 0: no early out,
   * 1: out = ibuf1,
   * 2: out = ibuf2 */
  int (*early_out)(struct Sequence *seq, float fac);

  /* sets the default `fac` value */
  void (*get_default_fac)(const struct Scene *scene,
                          struct Sequence *seq,
                          float timeline_frame,
                          float *fac);

  /* execute the effect
   * sequence effects are only required to either support
   * float-rects or byte-rects
   * (mixed cases are handled one layer up...) */

  struct ImBuf *(*execute)(const struct SeqRenderData *context,
                           struct Sequence *seq,
                           float timeline_frame,
                           float fac,
                           struct ImBuf *ibuf1,
                           struct ImBuf *ibuf2,
                           struct ImBuf *ibuf3);

  struct ImBuf *(*init_execution)(const struct SeqRenderData *context,
                                  struct ImBuf *ibuf1,
                                  struct ImBuf *ibuf2,
                                  struct ImBuf *ibuf3);

  void (*execute_slice)(const struct SeqRenderData *context,
                        struct Sequence *seq,
                        float timeline_frame,
                        float fac,
                        struct ImBuf *ibuf1,
                        struct ImBuf *ibuf2,
                        struct ImBuf *ibuf3,
                        int start_line,
                        int total_lines,
                        struct ImBuf *out);
};

struct SeqEffectHandle SEQ_effect_handle_get(struct Sequence *seq);
int SEQ_effect_get_num_inputs(int seq_type);
void SEQ_effect_text_font_unload(struct TextVars *data, bool do_id_user);
void SEQ_effect_text_font_load(struct TextVars *data, bool do_id_user);

#ifdef __cplusplus
}
#endif
