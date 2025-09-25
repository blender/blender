/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "SEQ_channels.hh"
#include "SEQ_render.hh"
#include "SEQ_utils.hh"

#include "effects.hh"
#include "render.hh"

namespace blender::seq {

/* No effect inputs for multi-camera, we use #give_ibuf_seq. */
static int num_inputs_multicam()
{
  return 0;
}

static StripEarlyOut early_out_multicam(const Strip * /*strip*/, float /*fac*/)
{
  return StripEarlyOut::NoInput;
}

static ImBuf *do_multicam(const RenderData *context,
                          SeqRenderState *state,
                          Strip *strip,
                          float timeline_frame,
                          float /*fac*/,
                          ImBuf * /*ibuf1*/,
                          ImBuf * /*ibuf2*/)
{
  ImBuf *out;
  Editing *ed;

  if (strip->multicam_source == 0 || strip->multicam_source >= strip->channel) {
    return nullptr;
  }

  ed = context->scene->ed;
  if (!ed || state->strips_rendering_seqbase.contains(strip)) {
    return nullptr;
  }
  ListBase *seqbasep = get_seqbase_by_strip(context->scene, strip);
  ListBase *channels = get_channels_by_strip(ed, strip);
  if (!seqbasep) {
    return nullptr;
  }

  state->strips_rendering_seqbase.add(strip);
  out = seq_render_give_ibuf_seqbase(
      context, state, timeline_frame, strip->multicam_source, channels, seqbasep);

  return out;
}

void multi_camera_effect_get_handle(EffectHandle &rval)
{
  rval.num_inputs = num_inputs_multicam;
  rval.early_out = early_out_multicam;
  rval.execute = do_multicam;
}

}  // namespace blender::seq
