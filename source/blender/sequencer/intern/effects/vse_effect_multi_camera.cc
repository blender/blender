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

/* No effect inputs for multi-camera, we use #give_ibuf_seq. */
static int num_inputs_multicam()
{
  return 0;
}

static StripEarlyOut early_out_multicam(const Strip * /*seq*/, float /*fac*/)
{
  return StripEarlyOut::NoInput;
}

static ImBuf *do_multicam(const SeqRenderData *context,
                          Strip *seq,
                          float timeline_frame,
                          float /*fac*/,
                          ImBuf * /*ibuf1*/,
                          ImBuf * /*ibuf2*/)
{
  ImBuf *out;
  Editing *ed;

  if (seq->multicam_source == 0 || seq->multicam_source >= seq->machine) {
    return nullptr;
  }

  ed = context->scene->ed;
  if (!ed) {
    return nullptr;
  }
  ListBase *seqbasep = SEQ_get_seqbase_by_seq(context->scene, seq);
  ListBase *channels = SEQ_get_channels_by_seq(&ed->seqbase, &ed->channels, seq);
  if (!seqbasep) {
    return nullptr;
  }

  out = seq_render_give_ibuf_seqbase(
      context, timeline_frame, seq->multicam_source, channels, seqbasep);

  return out;
}

void multi_camera_effect_get_handle(SeqEffectHandle &rval)
{
  rval.num_inputs = num_inputs_multicam;
  rval.early_out = early_out_multicam;
  rval.execute = do_multicam;
}
