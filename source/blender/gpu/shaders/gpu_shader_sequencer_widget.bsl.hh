/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GPU_shader_shared.hh"

namespace sequencer {

struct FragOut {
  [[frag_color(0)]] float4 color;
};

/* Signed distance to rounded box, centered at origin.
 * Reference: https://iquilezles.org/articles/distfunctions2d/ */
float sdf_rounded_box(float2 pos, float2 size, float radius)
{
  float2 q = abs(pos) - size + radius;
  return min(max(q.x, q.y), 0.0f) + length(max(q, 0.0f)) - radius;
}

void strip_box(float left,
               float right,
               float bottom,
               float top,
               float radius,
               float2 pos,
               float2 &r_pos1,
               float2 &r_pos2,
               float2 &r_size,
               float2 &r_center,
               float2 &r_pos,
               float &r_radius)
{
  /* Snap to pixel grid coordinates, so that outline/border is non-fractional
   * pixel sizes. */
  r_pos1 = round(float2(left, bottom));
  r_pos2 = round(float2(right, top));
  /* Make sure strip is at least 1px wide. */
  r_pos2.x = max(r_pos2.x, r_pos1.x + 1.0f);
  r_size = (r_pos2 - r_pos1) * 0.5f;
  r_center = (r_pos1 + r_pos2) * 0.5f;
  r_pos = round(pos);

  r_radius = radius;
  if (r_radius > r_size.x) {
    r_radius = 0.0f;
  }
}

namespace strip {

float3 color_shade(float3 rgb, float shade)
{
  rgb += float3(shade / 255.0f);
  rgb = clamp(rgb, float3(0.0f), float3(1.0f));
  return rgb;
}

/* Blends in a straight alpha `color` into premultiplied `cur` and returns premultiplied result.
 */
float4 blend_color(float4 cur, float4 color)
{
  float t = color.a;
  return cur * (1.0f - t) + float4(color.rgb * t, t);
}

struct Resources {
  [[push_constant]] float4x4 ModelViewProjectionMatrix;
  [[uniform(0)]] SeqStripDrawData (&strip_data)[GPU_SEQ_STRIP_DRAW_DATA_LEN];
  [[uniform(1)]] SeqContextDrawData &context_data;

  /* Given signed distance `d` to a shape and current premultiplied color `cur`, blends
   * in an outline at distance between `edge1` and `edge2`.
   * Outline color `outline_color` is in straight alpha. */
  float4 add_outline(float d, float edge1, float edge2, float4 cur, float4 outline_color) const
  {
    d -= 0.5f;
    edge1 *= context_data.pixelsize;
    edge2 *= context_data.pixelsize;
    float f = abs(d + (edge1 + edge2) * 0.5f) - abs(edge2 - edge1) * 0.5f + 0.5f;
    float a = clamp(1.0f - f, 0.0f, 1.0f);
    outline_color.a *= a;
    return blend_color(cur, outline_color);
  }
};

struct VertOut {
  [[no_perspective]] float2 co_interp;
  [[flat]] uint strip_id;
};

[[vertex]] void main_vert([[vertex_id]] const int vert_id,
                          [[instance_id]] const int inst_id,
                          [[resource_table]] Resources &srt,
                          [[out]] VertOut &v_out,
                          [[position]] float4 &out_pos)
{
  v_out.strip_id = uint(inst_id);
  SeqStripDrawData strip = srt.strip_data[inst_id];
  float4 rect = float4(strip.left_handle, strip.bottom, strip.right_handle, strip.top);
  /* Expand by 1px to fit pixel grid rounding. */
  float2 expand = float2(1.0f, 1.0f);
  rect.xy -= expand;
  rect.zw += expand;

  float2 co;
  if (vert_id == 0) {
    co = rect.xw;
  }
  else if (vert_id == 1) {
    co = rect.xy;
  }
  else if (vert_id == 2) {
    co = rect.zw;
  }
  else {
    co = rect.zy;
  }

  v_out.co_interp = co;
  out_pos = srt.ModelViewProjectionMatrix * float4(co, 0.0f, 1.0f);
}

[[fragment]] void main_frag([[resource_table]] const Resources &srt,
                            [[frag_coord]] const float4 frag_co,
                            [[in]] const VertOut &v_out,
                            [[out]] FragOut &frag_out)
{
  float2 co = v_out.co_interp;

  SeqStripDrawData strip = srt.strip_data[v_out.strip_id];

  float2 pos1, pos2, size, center, pos;
  float radius = 0.0f;
  strip_box(strip.left_handle,
            strip.right_handle,
            strip.bottom,
            strip.top,
            srt.context_data.round_radius,
            co,
            pos1,
            pos2,
            size,
            center,
            pos,
            radius);

  bool border = (strip.flags & GPU_SEQ_FLAG_BORDER) != 0;
  bool selected = (strip.flags & GPU_SEQ_FLAG_SELECTED) != 0;
  float outline_width = selected ? 2.0f : 1.0f;

  /* Distance to whole strip shape. */
  float sdf = sdf_rounded_box(pos - center, size, radius);

  /* Distance to inner part when handles are taken into account. */
  float sdf_inner = sdf;
  if ((strip.flags & GPU_SEQ_FLAG_ANY_HANDLE) != 0) {
    float handle_width = strip.handle_width;
    /* Take left/right handle from horizontal sides. */
    if ((strip.flags & GPU_SEQ_FLAG_SELECTED_LH) != 0) {
      pos1.x += handle_width;
    }
    if ((strip.flags & GPU_SEQ_FLAG_SELECTED_RH) != 0) {
      pos2.x -= handle_width;
    }
    /* Reduce vertical size by outline width. */
    pos1.y += srt.context_data.pixelsize * outline_width;
    pos2.y -= srt.context_data.pixelsize * outline_width;

    size = (pos2 - pos1) * 0.5f;
    center = (pos1 + pos2) * 0.5f;
    sdf_inner = sdf_rounded_box(pos - center, size, radius);
  }

  float4 col = float4(0.0f);

  /* Background. */
  if ((strip.flags & GPU_SEQ_FLAG_BACKGROUND) != 0) {
    col = unpackUnorm4x8(strip.col_background);
    /* Darker background for multi-image strip hold still regions. */
    if ((strip.flags & GPU_SEQ_FLAG_SINGLE_IMAGE) == 0) {
      if (co.x < strip.content_start || co.x > strip.content_end) {
        col.rgb = color_shade(col.rgb, -35.0f);
      }
    }
  }

  /* Thumbnails background. */
  if ((strip.flags & GPU_SEQ_FLAG_THUMBNAILS_BACKGROUND) != 0) {
    if (co.y < strip.strip_content_top) {
      if (co.x >= strip.content_start && co.x <= strip.content_end) {
        /* Re use the color band color here. */
        col.rgb = unpackUnorm4x8(strip.col_color_band).rgb;
      }
    }
  }
  /* Color band. */
  else if ((strip.flags & GPU_SEQ_FLAG_COLOR_BAND) != 0) {
    if (co.y < strip.strip_content_top) {
      col.rgb = unpackUnorm4x8(strip.col_color_band).rgb;
      /* Darker line to better separate the color band. */
      if (co.y > strip.strip_content_top - 1.0f) {
        col.rgb = color_shade(col.rgb, -20.0f);
      }
    }
  }

  /* Transition. */
  if ((strip.flags & GPU_SEQ_FLAG_TRANSITION) != 0) {
    if (co.x >= strip.content_start && co.x <= strip.content_end && co.y < strip.strip_content_top)
    {
      float diag_y = strip.strip_content_top - (strip.strip_content_top - strip.bottom) *
                                                   (co.x - strip.content_start) /
                                                   (strip.content_end - strip.content_start);
      uint transition_color = co.y <= diag_y ? strip.col_transition_in : strip.col_transition_out;
      col.rgb = unpackUnorm4x8(transition_color).rgb;
    }
  }

  /* Previous parts were all assigning color (not blending it),
   * make sure from now on alpha is premultiplied. */
  col.rgb *= col.a;

  /* Missing media. */
  if ((strip.flags & GPU_SEQ_FLAG_MISSING_TITLE) != 0) {
    if (co.y > strip.strip_content_top) {
      col = blend_color(col, float4(112.0f / 255.0f, 0.0f, 0.0f, 230.0f / 255.0f));
    }
  }
  if ((strip.flags & GPU_SEQ_FLAG_MISSING_CONTENT) != 0) {
    if (co.y <= strip.strip_content_top) {
      col = blend_color(col, float4(64.0f / 255.0f, 0.0f, 0.0f, 230.0f / 255.0f));
    }
  }

  /* Locked. */
  if ((strip.flags & GPU_SEQ_FLAG_LOCKED) != 0) {
    if (co.y <= strip.strip_content_top) {
      float phase = mod(frag_co.x + frag_co.y, 12.0f);
      if (phase >= 8.0f) {
        col = blend_color(col, float4(0.0f, 0.0f, 0.0f, 0.25f));
      }
    }
  }

  /* Highlight. */
  if ((strip.flags & GPU_SEQ_FLAG_HIGHLIGHT) != 0) {
    col = blend_color(col, float4(1.0f, 1.0f, 1.0f, 48.0f / 255.0f));
  }

  /* Handles. */
  float4 col_outline = unpackUnorm4x8(strip.col_outline);
  if ((strip.flags & GPU_SEQ_FLAG_ANY_HANDLE) != 0) {
    bool left_side = pos.x < center.x;
    uint handle_flag = left_side ? GPU_SEQ_FLAG_SELECTED_LH : GPU_SEQ_FLAG_SELECTED_RH;
    bool selected_handle = (strip.flags & handle_flag) != 0;
    /* Blend in handle color in between strip shape and inner handle shape. */
    if (sdf <= 0.0f && sdf_inner >= 0.0f) {
      float4 hcol = selected_handle ? col_outline : float4(0, 0, 0, 0.2f);
      hcol.a *= clamp(sdf_inner, 0.0f, 1.0f);
      col = blend_color(col, hcol);
    }
    /* For an unselected handle, no longer take it into account
     * for the "inner" distance. */
    if (!selected_handle) {
      sdf_inner = sdf;
    }
  }

  /* Outside of strip rounded rectangle? */
  if (sdf > 0.0f) {
    col = float4(0.0f);
  }

  /* Outline / border. */
  if (border) {

    if (selected) {
      /* Selection highlight + darker inset line. */
      col = srt.add_outline(sdf, 1.0f, 3.0f, col, col_outline);
      /* Inset line should be inside regular border or inside the handles. */
      float d = max(sdf_inner - 3.0f * srt.context_data.pixelsize, sdf);
      col = srt.add_outline(d, 3.0f, 4.0f, col, float4(0, 0, 0, 0.33f));
    }

    /* Active, but not selected strips get a thin inner line. */
    bool active_strip = (strip.flags & GPU_SEQ_FLAG_ACTIVE) != 0;
    if (active_strip && !selected) {
      col = srt.add_outline(sdf, 1.0f, 2.0f, col, col_outline);
    }

    /* 2px outline for all overlapping strips. */
    bool overlaps = (strip.flags & GPU_SEQ_FLAG_OVERLAP) != 0;
    bool clamped = (strip.flags & GPU_SEQ_FLAG_CLAMPED) != 0;
    if (overlaps || clamped) {
      col = srt.add_outline(sdf, 1.0f, 3.0f, col, col_outline);
    }

    /* Outer 1px outline for all strips. */
    col = srt.add_outline(sdf, 0.0f, 1.0f, col, unpackUnorm4x8(srt.context_data.col_back));
  }

  frag_out.color = col;
}
}  // namespace strip

namespace thumbnail {

struct VertOut {
  [[no_perspective]] float2 pos_interp;
  [[no_perspective]] float2 uv;
  [[flat]] uint thumb_id;
};

struct Resources {
  [[push_constant]] float4x4 ModelViewProjectionMatrix;
  [[uniform(0)]] SeqStripThumbData (&thumb_data)[GPU_SEQ_STRIP_DRAW_DATA_LEN];
  [[uniform(1)]] SeqContextDrawData &context_data;
  [[sampler(0)]] sampler2D image;
};

[[vertex]] void main_vert([[vertex_id]] const int vert_id,
                          [[instance_id]] const int inst_id,
                          [[resource_table]] Resources &srt,
                          [[out]] VertOut &v_out,
                          [[position]] float4 &out_pos)
{
  v_out.thumb_id = uint(inst_id);
  SeqStripThumbData thumb = srt.thumb_data[inst_id];
  float4 coords = float4(thumb.x1, thumb.y1, thumb.x2, thumb.y2);
  float4 uvs = float4(thumb.u1, thumb.v1, thumb.u2, thumb.v2);

  float2 co;
  float2 uv;
  if (vert_id == 0) {
    co = coords.xw;
    uv = uvs.xw;
  }
  else if (vert_id == 1) {
    co = coords.xy;
    uv = uvs.xy;
  }
  else if (vert_id == 2) {
    co = coords.zw;
    uv = uvs.zw;
  }
  else {
    co = coords.zy;
    uv = uvs.zy;
  }

  v_out.pos_interp = co;
  v_out.uv = uv;
  out_pos = srt.ModelViewProjectionMatrix * float4(co, 0.0f, 1.0f);
}

[[fragment]] void main_frag([[resource_table]] const Resources &srt,
                            [[in]] const VertOut &v_out,
                            [[out]] FragOut &frag_out)
{
  SeqStripThumbData thumb = srt.thumb_data[v_out.thumb_id];
  float2 pos1, pos2, size, center, pos;
  float radius = 0.0f;
  strip_box(thumb.left,
            thumb.right,
            thumb.bottom,
            thumb.top,
            srt.context_data.round_radius,
            v_out.pos_interp,
            pos1,
            pos2,
            size,
            center,
            pos,
            radius);

  /* Sample thumbnail texture, modulate with color. */
  float4 col = texture(srt.image, v_out.uv) * thumb.tint_color;

  /* Outside of strip rounded rectangle? */
  float sdf = sdf_rounded_box(pos - center, size, radius);
  if (sdf > 0.0f) {
    col = float4(0.0f);
  }

  frag_out.color = col;
}
}  // namespace thumbnail
}  // namespace sequencer

PipelineGraphic gpu_shader_sequencer_strips(sequencer::strip::main_vert,
                                            sequencer::strip::main_frag);
PipelineGraphic gpu_shader_sequencer_thumbs(sequencer::thumbnail::main_vert,
                                            sequencer::thumbnail::main_frag);
