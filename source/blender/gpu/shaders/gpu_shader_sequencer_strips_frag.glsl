/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_sequencer_infos.hh"

#include "gpu_shader_sequencer_lib.glsl"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_sequencer_strips)

float3 color_shade(float3 rgb, float shade)
{
  rgb += float3(shade / 255.0f);
  rgb = clamp(rgb, float3(0.0f), float3(1.0f));
  return rgb;
}

/* Blends in a straight alpha `color` into premultiplied `cur` and returns premultiplied result. */
float4 blend_color(float4 cur, float4 color)
{
  float t = color.a;
  return cur * (1.0f - t) + float4(color.rgb * t, t);
}

/* Given signed distance `d` to a shape and current premultiplied color `cur`, blends
 * in an outline at distance between `edge1` and `edge2`.
 * Outline color `outline_color` is in straight alpha. */
float4 add_outline(float d, float edge1, float edge2, float4 cur, float4 outline_color)
{
  d -= 0.5f;
  edge1 *= context_data.pixelsize;
  edge2 *= context_data.pixelsize;
  float f = abs(d + (edge1 + edge2) * 0.5f) - abs(edge2 - edge1) * 0.5f + 0.5f;
  float a = clamp(1.0f - f, 0.0f, 1.0f);
  outline_color.a *= a;
  return blend_color(cur, outline_color);
}

void main()
{
  float2 co = co_interp;

  SeqStripDrawData strip = strip_data[strip_id];

  float2 pos1, pos2, size, center, pos;
  float radius = 0.0f;
  strip_box(strip.left_handle,
            strip.right_handle,
            strip.bottom,
            strip.top,
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
    pos1.y += context_data.pixelsize * outline_width;
    pos2.y -= context_data.pixelsize * outline_width;

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

  /* Color band. */
  if ((strip.flags & GPU_SEQ_FLAG_COLOR_BAND) != 0) {
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
      float phase = mod(gl_FragCoord.x + gl_FragCoord.y, 12.0f);
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
      col = add_outline(sdf, 1.0f, 3.0f, col, col_outline);
      /* Inset line should be inside regular border or inside the handles. */
      float d = max(sdf_inner - 3.0f * context_data.pixelsize, sdf);
      col = add_outline(d, 3.0f, 4.0f, col, float4(0, 0, 0, 0.33f));
    }

    /* Active, but not selected strips get a thin inner line. */
    bool active_strip = (strip.flags & GPU_SEQ_FLAG_ACTIVE) != 0;
    if (active_strip && !selected) {
      col = add_outline(sdf, 1.0f, 2.0f, col, col_outline);
    }

    /* 2px outline for all overlapping strips. */
    bool overlaps = (strip.flags & GPU_SEQ_FLAG_OVERLAP) != 0;
    bool clamped = (strip.flags & GPU_SEQ_FLAG_CLAMPED) != 0;
    if (overlaps || clamped) {
      col = add_outline(sdf, 1.0f, 3.0f, col, col_outline);
    }

    /* Outer 1px outline for all strips. */
    col = add_outline(sdf, 0.0f, 1.0f, col, unpackUnorm4x8(context_data.col_back));
  }

  fragColor = col;
}
