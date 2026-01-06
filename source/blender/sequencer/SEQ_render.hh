/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "DNA_listBase.h"
#include "DNA_space_enums.h"

namespace blender {

struct Depsgraph;
struct GPUOffScreen;
struct GPUViewport;
struct ImBuf;
struct Main;
struct Render;
struct Scene;
struct SeqTimelineChannel;
struct Strip;
struct StripElem;

namespace seq {

enum eTaskId {
  SEQ_TASK_MAIN_RENDER,
  SEQ_TASK_PREFETCH_RENDER,
};

struct RenderData {
  Main *bmain = nullptr;
  Depsgraph *depsgraph = nullptr;
  Scene *scene = nullptr;
  int rectx = 0;
  int recty = 0;
  eSpaceSeq_Proxy_RenderSize preview_render_size = SEQ_RENDER_SIZE_SCENE;
  bool use_proxies = false;
  bool ignore_missing_media = false;
  int motion_blur_samples = 0;
  float motion_blur_shutter = 0.0f;
  bool skip_cache = false;
  bool is_proxy_render = false;
  bool is_prefetch_render = false;
  bool is_playing = false;
  bool is_scrubbing = false;
  int view_id = 0;
  /* ID of task for assigning temp cache entries to particular task(thread, etc.) */
  eTaskId task_id = SEQ_TASK_MAIN_RENDER;

  /* Set when executing as part of a frame or animation render. */
  Render *render = nullptr;

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
ImBuf *render_give_ibuf(const RenderData *context, float timeline_frame, int chanshown);
ImBuf *render_give_ibuf_direct(const RenderData *context, float timeline_frame, Strip *strip);
void render_new_render_data(Main *bmain,
                            Depsgraph *depsgraph,
                            Scene *scene,
                            int rectx,
                            int recty,
                            eSpaceSeq_Proxy_RenderSize preview_render_size,
                            Render *render,
                            RenderData *r_context);
StripElem *render_give_stripelem(const Scene *scene, const Strip *strip, int timeline_frame);

void render_imbuf_from_sequencer_space(const Scene *scene, ImBuf *ibuf);
void render_pixel_from_sequencer_space_v4(const Scene *scene, float pixel[4]);
/**
 * Check if `strip` is muted for rendering.
 * This function also checks `SeqTimelineChannel` flag.
 */
bool render_is_muted(const ListBaseT<SeqTimelineChannel> *channels, const Strip *strip);

/**
 * Calculate render scale factor relative to full size. This can be due to render
 * scale setting in output settings, or preview proxy size.
 */
float get_render_scale_factor(eSpaceSeq_Proxy_RenderSize render_size, short scene_render_scale);
float get_render_scale_factor(const RenderData &context);

}  // namespace seq
}  // namespace blender
