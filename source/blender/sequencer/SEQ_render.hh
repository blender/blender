/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

struct Depsgraph;
struct GPUOffScreen;
struct GPUViewport;
struct ImBuf;
struct ListBase;
struct Main;
struct Scene;
struct Strip;
struct StripElem;

enum eSeqTaskId {
  SEQ_TASK_MAIN_RENDER,
  SEQ_TASK_PREFETCH_RENDER,
};

struct SeqRenderData {
  Main *bmain = nullptr;
  Depsgraph *depsgraph = nullptr;
  Scene *scene = nullptr;
  int rectx = 0;
  int recty = 0;
  int preview_render_size = 0;
  bool use_proxies = false;
  bool ignore_missing_media = false;
  int for_render = 0;
  int motion_blur_samples = 0;
  float motion_blur_shutter = 0.0f;
  bool skip_cache = false;
  bool is_proxy_render = false;
  bool is_prefetch_render = false;
  bool is_playing = false;
  bool is_scrubbing = false;
  int view_id = 0;
  /* ID of task for assigning temp cache entries to particular task(thread, etc.) */
  eSeqTaskId task_id = SEQ_TASK_MAIN_RENDER;

  /* special case for OpenGL render */
  GPUOffScreen *gpu_offscreen = nullptr;
  GPUViewport *gpu_viewport = nullptr;
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
                                   Strip *strip);
void SEQ_render_new_render_data(Main *bmain,
                                Depsgraph *depsgraph,
                                Scene *scene,
                                int rectx,
                                int recty,
                                int preview_render_size,
                                int for_render,
                                SeqRenderData *r_context);
StripElem *SEQ_render_give_stripelem(const Scene *scene, const Strip *strip, int timeline_frame);

void SEQ_render_imbuf_from_sequencer_space(const Scene *scene, ImBuf *ibuf);
void SEQ_render_pixel_from_sequencer_space_v4(const Scene *scene, float pixel[4]);
/**
 * Check if `strip` is muted for rendering.
 * This function also checks `SeqTimelineChannel` flag.
 */
bool SEQ_render_is_muted(const ListBase *channels, const Strip *strip);
