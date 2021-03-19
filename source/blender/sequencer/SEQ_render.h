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

struct ListBase;
struct Main;
struct Scene;
struct Sequence;

typedef enum eSeqTaskId {
  SEQ_TASK_MAIN_RENDER,
  SEQ_TASK_PREFETCH_RENDER,
} eSeqTaskId;

typedef struct SeqRenderData {
  struct Main *bmain;
  struct Depsgraph *depsgraph;
  struct Scene *scene;
  int rectx;
  int recty;
  int preview_render_size;
  bool use_proxies;
  int for_render;
  int motion_blur_samples;
  float motion_blur_shutter;
  bool skip_cache;
  bool is_proxy_render;
  bool is_prefetch_render;
  int view_id;
  /* ID of task for assigning temp cache entries to particular task(thread, etc.) */
  eSeqTaskId task_id;

  /* special case for OpenGL render */
  struct GPUOffScreen *gpu_offscreen;
  // int gpu_samples;
  // bool gpu_full_samples;
} SeqRenderData;

struct ImBuf *SEQ_render_give_ibuf(const SeqRenderData *context,
                                   float timeline_frame,
                                   int chanshown);
struct ImBuf *SEQ_render_give_ibuf_direct(const SeqRenderData *context,
                                          float timeline_frame,
                                          struct Sequence *seq);
void SEQ_render_init_colorspace(struct Sequence *seq);
void SEQ_render_new_render_data(struct Main *bmain,
                                struct Depsgraph *depsgraph,
                                struct Scene *scene,
                                int rectx,
                                int recty,
                                int preview_render_size,
                                int for_render,
                                SeqRenderData *r_context);
int SEQ_render_evaluate_frame(struct ListBase *seqbase, int timeline_frame);
struct StripElem *SEQ_render_give_stripelem(struct Sequence *seq, int timeline_frame);

void SEQ_render_imbuf_from_sequencer_space(struct Scene *scene, struct ImBuf *ibuf);
void SEQ_render_pixel_from_sequencer_space_v4(struct Scene *scene, float pixel[4]);

#ifdef __cplusplus
}
#endif
