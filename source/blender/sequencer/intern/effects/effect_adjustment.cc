/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "PRF_profile.hh"

#include "DNA_sequence_types.h"

#include "SEQ_channels.hh"
#include "SEQ_render.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_utils.hh"

#include "effects.hh"
#include "render.hh"

namespace blender::seq {

static StripEarlyOut early_out_adjustment(const Strip * /*strip*/, float /*fac*/)
{
  return StripEarlyOut::NoInput;
}

static SeqResult do_adjustment_impl(const RenderData *context,
                                    SeqRenderState *state,
                                    Strip *strip,
                                    float timeline_frame)
{
  SeqResult out;
  Editing *ed = context->scene->ed;

  ListBaseT<Strip> *seqbasep = get_seqbase_by_strip(context->scene, strip);
  ListBaseT<SeqTimelineChannel> *channels = get_channels_by_strip(ed, strip);

  /* Clamp timeline_frame to strip range so it behaves as if it had "still frame" offset (last
   * frame is static after end of strip). This is how most strips behave. This way transition
   * effects that doesn't overlap or speed effect can't fail rendering outside of strip range. */
  timeline_frame = clamp_i(
      timeline_frame, strip->left_handle(), strip->right_handle(context->scene) - 1);

  if (strip->channel > 1) {
    out = seq_render_give_ibuf_seqbase(
        context, state, timeline_frame, strip->channel - 1, channels, seqbasep);
  }

  /* Found nothing? Then work our way up the meta-strip stack, as this adjustment strip might be
   * inside a nested meta-strip and affect strips below that meta-strip.
   *
   * NOTE: we should NOT walk past the stack level that the user is currently tabbed into,
   * otherwise the adjustment layer can leak content from outside the meta context. */

  if (!out.is_valid()) {
    Strip *meta = lookup_meta_by_strip(ed, strip);
    if (meta && meta != ed->current_meta_strip) {
      out = do_adjustment_impl(context, state, meta, timeline_frame);
    }
  }

  return out;
}

static SeqResult do_adjustment(const RenderData *context,
                               SeqRenderState *state,
                               Strip *strip,
                               float timeline_frame,
                               float /*fac*/,
                               const SeqResult & /*ibuf1*/,
                               const SeqResult & /*ibuf2*/)
{
  PRF_scope_with_name("SeqFxAdjustment", ProfileCategory::Draw);
  Editing *ed = context->scene->ed;
  if (!ed || state->strips_in_progress.contains(strip)) {
    return {};
  }

  state->strips_in_progress.add(strip);
  SeqResult out = do_adjustment_impl(context, state, strip, timeline_frame);
  state->strips_in_progress.remove(strip);
  return out;
}

void adjustment_effect_get_handle(EffectHandle &rval)
{
  rval.early_out = early_out_adjustment;
  rval.execute = do_adjustment;
}

}  // namespace blender::seq
