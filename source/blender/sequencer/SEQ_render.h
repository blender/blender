/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2004 Blender Foundation */

#pragma once

/** \file
 * \ingroup sequencer
 */

#ifdef __cplusplus
extern "C" {
#endif

#define SEQ_RENDER_THUMB_SIZE 256

struct ListBase;
struct Main;
struct Scene;
struct Sequence;
struct rctf;

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

/**
 * \return The image buffer or NULL.
 *
 * \note The returned #ImBuf has its reference increased, free after usage!
 */
struct ImBuf *SEQ_render_give_ibuf(const SeqRenderData *context,
                                   float timeline_frame,
                                   int chanshown);
struct ImBuf *SEQ_render_give_ibuf_direct(const SeqRenderData *context,
                                          float timeline_frame,
                                          struct Sequence *seq);
/**
 * Render the series of thumbnails and store in cache.
 */
void SEQ_render_thumbnails(const struct SeqRenderData *context,
                           struct Sequence *seq,
                           struct Sequence *seq_orig,
                           float frame_step,
                           const struct rctf *view_area,
                           const bool *stop);
/**
 * Get cached thumbnails.
 */
struct ImBuf *SEQ_get_thumbnail(const struct SeqRenderData *context,
                                struct Sequence *seq,
                                float timeline_frame,
                                rcti *crop,
                                bool clipped);
/**
 * Get frame for first thumbnail.
 */
float SEQ_render_thumbnail_first_frame_get(const struct Scene *scene,
                                           struct Sequence *seq,
                                           float frame_step,
                                           const struct rctf *view_area);
/**
 * Get frame for first thumbnail.
 */
float SEQ_render_thumbnail_next_frame_get(const struct Scene *scene,
                                          struct Sequence *seq,
                                          float last_frame,
                                          float frame_step);
/**
 * Get frame step for equally spaced thumbnails. These thumbnails should always be present in
 * memory, so they can be used when zooming.
 */
int SEQ_render_thumbnails_guaranteed_set_frame_step_get(const struct Scene *scene,
                                                        const struct Sequence *seq);
/**
 * Render set of evenly spaced thumbnails that are drawn when zooming..
 */
void SEQ_render_thumbnails_base_set(const struct SeqRenderData *context,
                                    struct Sequence *seq,
                                    struct Sequence *seq_orig,
                                    const struct rctf *view_area,
                                    const bool *stop);

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
struct StripElem *SEQ_render_give_stripelem(const struct Scene *scene,
                                            struct Sequence *seq,
                                            int timeline_frame);

void SEQ_render_imbuf_from_sequencer_space(struct Scene *scene, struct ImBuf *ibuf);
void SEQ_render_pixel_from_sequencer_space_v4(struct Scene *scene, float pixel[4]);
/**
 * Check if `seq` is muted for rendering.
 * This function also checks `SeqTimelineChannel` flag.
 */
bool SEQ_render_is_muted(const struct ListBase *channels, const struct Sequence *seq);

#ifdef __cplusplus
}
#endif
