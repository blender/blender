/*
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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 */

#pragma once

/** \file
 * \ingroup sequencer
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Sequence;
struct ImBuf;
struct SeqRenderData;
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
  void (*copy)(struct Sequence *dst, struct Sequence *src, const int flag);

  /* destruct */
  void (*free)(struct Sequence *seq, const bool do_id_user);

  /* returns: -1: no input needed,
   * 0: no early out,
   * 1: out = ibuf1,
   * 2: out = ibuf2 */
  int (*early_out)(struct Sequence *seq, float facf0, float facf1);

  /* stores the y-range of the effect IPO */
  void (*store_icu_yrange)(struct Sequence *seq, short adrcode, float *ymin, float *ymax);

  /* stores the default facf0 and facf1 if no IPO is present */
  void (*get_default_fac)(struct Sequence *seq, float timeline_frame, float *facf0, float *facf1);

  /* execute the effect
   * sequence effects are only required to either support
   * float-rects or byte-rects
   * (mixed cases are handled one layer up...) */

  struct ImBuf *(*execute)(const struct SeqRenderData *context,
                           struct Sequence *seq,
                           float timeline_frame,
                           float facf0,
                           float facf1,
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
                        float facf0,
                        float facf1,
                        struct ImBuf *ibuf1,
                        struct ImBuf *ibuf2,
                        struct ImBuf *ibuf3,
                        int start_line,
                        int total_lines,
                        struct ImBuf *out);
};

struct SeqEffectHandle SEQ_effect_handle_get(struct Sequence *seq);
int SEQ_effect_get_num_inputs(int seq_type);
void SEQ_effect_text_font_unload(struct TextVars *data, const bool do_id_user);
void SEQ_effect_text_font_load(struct TextVars *data, const bool do_id_user);

#ifdef __cplusplus
}
#endif
