/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "BLI_math_vector_types.hh"
#include "BLI_set.hh"
#include "BLI_vector.hh"

struct Depsgraph;
struct ImBuf;
struct LinkNode;
struct ListBase;
struct Mask;
struct Scene;
struct RenderData;
struct Strip;

namespace blender::seq {

/* Mutable state while rendering one sequencer frame. */
struct SeqRenderState {
  LinkNode *scene_parents = nullptr;
  Set<Strip *> strips_rendering_seqbase;
};

/* Strip corner coordinates in screen pixel space. Note that they might not be
 * axis aligned when rotation is present. */
struct StripScreenQuad {
  float2 v0, v1, v2, v3;

  bool is_empty() const
  {
    return v0 == v1 && v2 == v3 && v0 == v2;
  }
};

ImBuf *seq_render_give_ibuf_seqbase(const RenderData *context,
                                    SeqRenderState *state,
                                    float timeline_frame,
                                    int chan_shown,
                                    ListBase *channels,
                                    ListBase *seqbasep);
void seq_imbuf_to_sequencer_space(const Scene *scene, ImBuf *ibuf, bool make_float);
Vector<Strip *> seq_shown_strips_get(
    const Scene *scene, ListBase *channels, ListBase *seqbase, int timeline_frame, int chanshown);
ImBuf *seq_render_strip(const RenderData *context,
                        SeqRenderState *state,
                        Strip *strip,
                        float timeline_frame);

/* Renders Mask into an image suitable for sequencer:
 * RGB channels contain mask intensity; alpha channel is opaque. */
ImBuf *seq_render_mask(Depsgraph *depsgraph,
                       int width,
                       int height,
                       const Mask *mask,
                       float frame_index,
                       bool make_float);
void seq_imbuf_assign_spaces(const Scene *scene, ImBuf *ibuf);

StripScreenQuad get_strip_screen_quad(const RenderData *context, const Strip *strip);

}  // namespace blender::seq
