/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BLI_task.hh"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "effects.hh"

namespace blender::seq {

static void init_solid_color(Strip *strip)
{
  MEM_SAFE_FREE(strip->effectdata);
  SolidColorVars *data = MEM_callocN<SolidColorVars>("solidcolor");
  strip->effectdata = data;
  data->col[0] = data->col[1] = data->col[2] = 0.5;
}

static int num_inputs_color()
{
  return 0;
}

static StripEarlyOut early_out_color(const Strip * /*strip*/, float /*fac*/)
{
  return StripEarlyOut::NoInput;
}

static ImBuf *do_solid_color(const RenderData *context,
                             SeqRenderState * /*state*/,
                             Strip *strip,
                             float /*timeline_frame*/,
                             float /*fac*/,
                             ImBuf *ibuf1,
                             ImBuf *ibuf2)
{
  using namespace blender;
  ImBuf *out = prepare_effect_imbufs(context, ibuf1, ibuf2);

  SolidColorVars *cv = (SolidColorVars *)strip->effectdata;

  threading::parallel_for(IndexRange(out->y), 64, [&](const IndexRange y_range) {
    if (out->byte_buffer.data) {
      /* Byte image. */
      uchar color[4];
      rgb_float_to_uchar(color, cv->col);
      color[3] = 255;

      uchar *dst = out->byte_buffer.data + y_range.first() * out->x * 4;
      uchar *dst_end = dst + y_range.size() * out->x * 4;
      while (dst < dst_end) {
        memcpy(dst, color, sizeof(color));
        dst += 4;
      }
    }
    else {
      /* Float image. */
      float color[4];
      color[0] = cv->col[0];
      color[1] = cv->col[1];
      color[2] = cv->col[2];
      color[3] = 1.0f;

      float *dst = out->float_buffer.data + y_range.first() * out->x * 4;
      float *dst_end = dst + y_range.size() * out->x * 4;
      while (dst < dst_end) {
        memcpy(dst, color, sizeof(color));
        dst += 4;
      }
    }
  });

  out->planes = R_IMF_PLANES_RGB;

  return out;
}

void solid_color_effect_get_handle(EffectHandle &rval)
{
  rval.init = init_solid_color;
  rval.num_inputs = num_inputs_color;
  rval.early_out = early_out_color;
  rval.execute = do_solid_color;
}

}  // namespace blender::seq
