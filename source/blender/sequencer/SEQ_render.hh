/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#define SEQ_RENDER_THUMB_SIZE 256

struct Depsgraph;
struct GPUOffScreen;
struct GPUViewport;
struct ImBuf;
struct ListBase;
struct Main;
struct Scene;
struct Sequence;
struct StripElem;
struct rctf;

enum eSeqTaskId {
  SEQ_TASK_MAIN_RENDER,
  SEQ_TASK_PREFETCH_RENDER,
};

struct SeqRenderData {
  Main *bmain;
  Depsgraph *depsgraph;
  Scene *scene;
  int rectx;
  int recty;
  int preview_render_size;
  bool use_proxies;
  bool ignore_missing_media;
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
  GPUOffScreen *gpu_offscreen;
  GPUViewport *gpu_viewport;
  // int gpu_samples;
  // bool gpu_full_samples;
};

/**
 * \return The image buffer or NULL.
 *
 * \note The returned #ImBuf has its reference increased, free after usage!
 */
ImBuf *SEQ_render_give_ibuf(const SeqRenderData *context, float timeline_frame, int chanshown);
ImBuf *SEQ_render_give_ibuf_direct(const SeqRenderData *context,
                                   float timeline_frame,
                                   Sequence *seq);
/**
 * Render the series of thumbnails and store in cache.
 */
void SEQ_render_thumbnails(const SeqRenderData *context,
                           Sequence *seq,
                           Sequence *seq_orig,
                           float frame_step,
                           const rctf *view_area,
                           const bool *stop);
/**
 * Get cached thumbnails.
 */
ImBuf *SEQ_get_thumbnail(
    const SeqRenderData *context, Sequence *seq, float timeline_frame, rcti *crop, bool clipped);
/**
 * Get frame for first thumbnail.
 */
float SEQ_render_thumbnail_first_frame_get(const Scene *scene,
                                           Sequence *seq,
                                           float frame_step,
                                           const rctf *view_area);
/**
 * Get frame for first thumbnail.
 */
float SEQ_render_thumbnail_next_frame_get(const Scene *scene,
                                          Sequence *seq,
                                          float last_frame,
                                          float frame_step);
/**
 * Get frame step for equally spaced thumbnails. These thumbnails should always be present in
 * memory, so they can be used when zooming.
 */
int SEQ_render_thumbnails_guaranteed_set_frame_step_get(const Scene *scene, const Sequence *seq);
/**
 * Render set of evenly spaced thumbnails that are drawn when zooming..
 */
void SEQ_render_thumbnails_base_set(const SeqRenderData *context,
                                    Sequence *seq,
                                    Sequence *seq_orig,
                                    const rctf *view_area,
                                    const bool *stop);

void SEQ_render_init_colorspace(Sequence *seq);
void SEQ_render_new_render_data(Main *bmain,
                                Depsgraph *depsgraph,
                                Scene *scene,
                                int rectx,
                                int recty,
                                int preview_render_size,
                                int for_render,
                                SeqRenderData *r_context);
int SEQ_render_evaluate_frame(ListBase *seqbase, int timeline_frame);
StripElem *SEQ_render_give_stripelem(const Scene *scene, Sequence *seq, int timeline_frame);

void SEQ_render_imbuf_from_sequencer_space(Scene *scene, ImBuf *ibuf);
void SEQ_render_pixel_from_sequencer_space_v4(Scene *scene, float pixel[4]);
/**
 * Check if `seq` is muted for rendering.
 * This function also checks `SeqTimelineChannel` flag.
 */
bool SEQ_render_is_muted(const ListBase *channels, const Sequence *seq);
