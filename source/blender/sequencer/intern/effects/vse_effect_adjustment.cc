/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "DNA_sequence_types.h"

#include "SEQ_channels.hh"
#include "SEQ_render.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"
#include "SEQ_utils.hh"

#include "effects.hh"
#include "render.hh"

namespace blender::seq {

/* No effect inputs for adjustment, we use #give_ibuf_seq. */
static int num_inputs_adjustment()
{
  return 0;
}

static StripEarlyOut early_out_adjustment(const Strip * /*strip*/, float /*fac*/)
{
  return StripEarlyOut::NoInput;
}

static ImBuf *do_adjustment_impl(const RenderData *context,
                                 SeqRenderState *state,
                                 Strip *strip,
                                 float timeline_frame)
{
  Editing *ed;
  ImBuf *i = nullptr;

  ed = context->scene->ed;

  ListBase *seqbasep = get_seqbase_by_strip(context->scene, strip);
  ListBase *channels = get_channels_by_strip(ed, strip);

  /* Clamp timeline_frame to strip range so it behaves as if it had "still frame" offset (last
   * frame is static after end of strip). This is how most strips behave. This way transition
   * effects that doesn't overlap or speed effect can't fail rendering outside of strip range. */
  timeline_frame = clamp_i(timeline_frame,
                           time_left_handle_frame_get(context->scene, strip),
                           time_right_handle_frame_get(context->scene, strip) - 1);

  if (strip->channel > 1) {
    i = seq_render_give_ibuf_seqbase(
        context, state, timeline_frame, strip->channel - 1, channels, seqbasep);
  }

  /* Found nothing? so let's work the way up the meta-strip stack, so
   * that it is possible to group a bunch of adjustment strips into
   * a meta-strip and have that work on everything below the meta-strip. */

  if (!i) {
    Strip *meta;

    meta = lookup_meta_by_strip(ed, strip);

    if (meta) {
      i = do_adjustment_impl(context, state, meta, timeline_frame);
    }
  }

  return i;
}

static ImBuf *do_adjustment(const RenderData *context,
                            SeqRenderState *state,
                            Strip *strip,
                            float timeline_frame,
                            float /*fac*/,
                            ImBuf * /*ibuf1*/,
                            ImBuf * /*ibuf2*/)
{
  ImBuf *out;
  Editing *ed;

  ed = context->scene->ed;

  if (!ed || state->strips_rendering_seqbase.contains(strip)) {
    return nullptr;
  }

  state->strips_rendering_seqbase.add(strip);
  out = do_adjustment_impl(context, state, strip, timeline_frame);

  return out;
}

void adjustment_effect_get_handle(EffectHandle &rval)
{
  rval.num_inputs = num_inputs_adjustment;
  rval.early_out = early_out_adjustment;
  rval.execute = do_adjustment;
}

}  // namespace blender::seq
