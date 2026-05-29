/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BLI_profile.hh"
#include "BLI_task.hh"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "effects.hh"

namespace blender::seq {

static void init_solid_color(Strip *strip)
{
  SolidColorVars *data = MEM_new<SolidColorVars>("solidcolor");
  strip->effectdata = data;
  data->col[0] = data->col[1] = data->col[2] = 0.5;
}

static void free_solid_color(Strip *strip, const bool /*do_id_user*/)
{
  if (strip->effectdata) {
    SolidColorVars *data = static_cast<SolidColorVars *>(strip->effectdata);
    MEM_delete(data);
    strip->effectdata = nullptr;
  }
}

static StripEarlyOut early_out_color(const Strip * /*strip*/, float /*fac*/)
{
  return StripEarlyOut::NoInput;
}

static SeqResult do_solid_color(const RenderData *context,
                                SeqRenderState * /*state*/,
                                Strip *strip,
                                float /*timeline_frame*/,
                                float /*fac*/,
                                const SeqResult & /*ibuf1*/,
                                const SeqResult & /*ibuf2*/)
{
  BLI_profile_scope_with_name("SeqFxColor", ProfileCategory::Draw);
  SeqResult out = prepare_effect_imbufs(context, {}, {});

  SolidColorVars *cv = static_cast<SolidColorVars *>(strip->effectdata);

  uchar color[4];
  rgb_float_to_uchar(color, cv->col);
  color[3] = 255;

  uchar *byte_data = out.image->byte_data_for_write();
  threading::parallel_for(IndexRange(out.image->y), 64, [&](const IndexRange y_range) {
    uchar *dst = byte_data + y_range.first() * out.image->x * 4;
    uchar *dst_end = dst + y_range.size() * out.image->x * 4;
    while (dst < dst_end) {
      memcpy(dst, color, sizeof(color));
      dst += 4;
    }
  });

  out.image->color_mode = ImColorMode::RGB;
  out.is_opaque_before_transform = true;

  return out;
}

void solid_color_effect_get_handle(EffectHandle &rval)
{
  rval.init = init_solid_color;
  rval.free = free_solid_color;
  rval.early_out = early_out_color;
  rval.execute = do_solid_color;
}

}  // namespace blender::seq
