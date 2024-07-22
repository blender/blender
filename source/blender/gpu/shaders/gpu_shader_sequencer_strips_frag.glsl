/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Signed distance to rounded box, centered at origin.
 * Reference: https://iquilezles.org/articles/distfunctions2d/ */
float sdf_rounded_box(vec2 pos, vec2 size, float radius)
{
  vec2 q = abs(pos) - size + radius;
  return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
}

vec3 color_shade(vec3 rgb, float shade)
{
  rgb += vec3(shade / 255.0);
  rgb = clamp(rgb, vec3(0.0), vec3(1.0));
  return rgb;
}

/* Blends in a straight alpha `color` into premultiplied `cur` and returns premultiplied result. */
vec4 blend_color(vec4 cur, vec4 color)
{
  float t = color.a;
  return cur * (1.0 - t) + vec4(color.rgb * t, t);
}

/* Given signed distance `d` to a shape and current premultiplied color `cur`, blends
 * in an outline at distance between `edge1` and `edge2`.
 * Outline color `outline_color` is in straight alpha. */
vec4 add_outline(float d, float edge1, float edge2, vec4 cur, vec4 outline_color)
{
  d -= 0.5;
  edge1 *= context_data.pixelsize;
  edge2 *= context_data.pixelsize;
  float f = abs(d + (edge1 + edge2) * 0.5) - abs(edge2 - edge1) * 0.5 + 0.5;
  float a = clamp(1.0 - f, 0.0, 1.0);
  outline_color.a *= a;
  return blend_color(cur, outline_color);
}

void main()
{
  vec2 co = co_interp;

  SeqStripDrawData strip = strip_data[strip_id];

  /* Snap to pixel grid coordinates, so that outline/border is non-fractional
   * pixel sizes. */
  vec2 pos1 = round(vec2(strip.left_handle, strip.bottom));
  vec2 pos2 = round(vec2(strip.right_handle, strip.top));
  /* Make sure strip is at least 1px wide. */
  pos2.x = max(pos2.x, pos1.x + 1.0);
  vec2 size = (pos2 - pos1) * 0.5;
  vec2 center = (pos1 + pos2) * 0.5;
  vec2 pos = round(co);

  float radius = context_data.round_radius;
  if (radius > size.x) {
    radius = 0.0;
  }

  bool border = (strip.flags & GPU_SEQ_FLAG_BORDER) != 0;
  bool selected = (strip.flags & GPU_SEQ_FLAG_SELECTED) != 0;
  float outline_width = selected ? 2.0 : 1.0;

  /* Distance to whole strip shape. */
  float sdf = sdf_rounded_box(pos - center, size, radius);

  /* Distance to inner part when handles are taken into account. */
  float sdf_inner = sdf;
  if ((strip.flags & GPU_SEQ_FLAG_ANY_HANDLE) != 0) {
    float handle_width = strip.handle_width;
    /* Take left/right handle from horizontal sides. */
    if ((strip.flags & GPU_SEQ_FLAG_DRAW_LH) != 0) {
      pos1.x += handle_width;
    }
    if ((strip.flags & GPU_SEQ_FLAG_DRAW_RH) != 0) {
      pos2.x -= handle_width;
    }
    /* Reduce vertical size by outline width. */
    pos1.y += context_data.pixelsize * outline_width;
    pos2.y -= context_data.pixelsize * outline_width;

    size = (pos2 - pos1) * 0.5;
    center = (pos1 + pos2) * 0.5;
    sdf_inner = sdf_rounded_box(pos - center, size, radius);
  }

  vec4 col = vec4(0.0);

  /* Background. */
  if ((strip.flags & GPU_SEQ_FLAG_BACKGROUND) != 0) {
    col = unpackUnorm4x8(strip.col_background);
    /* Darker background for multi-image strip hold still regions. */
    if ((strip.flags & GPU_SEQ_FLAG_SINGLE_IMAGE) == 0) {
      if (co.x < strip.content_start || co.x > strip.content_end) {
        col.rgb = color_shade(col.rgb, -35.0);
      }
    }
  }

  /* Color band. */
  if ((strip.flags & GPU_SEQ_FLAG_COLOR_BAND) != 0) {
    if (co.y < strip.strip_content_top) {
      col.rgb = unpackUnorm4x8(strip.col_color_band).rgb;
      /* Darker line to better separate the color band. */
      if (co.y > strip.strip_content_top - 1.0) {
        col.rgb = color_shade(col.rgb, -20.0);
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
      col = blend_color(col, vec4(112.0 / 255.0, 0.0, 0.0, 230.0 / 255.0));
    }
  }
  if ((strip.flags & GPU_SEQ_FLAG_MISSING_CONTENT) != 0) {
    if (co.y <= strip.strip_content_top) {
      col = blend_color(col, vec4(64.0 / 255.0, 0.0, 0.0, 230.0 / 255.0));
    }
  }

  /* Locked. */
  if ((strip.flags & GPU_SEQ_FLAG_LOCKED) != 0) {
    if (co.y <= strip.strip_content_top) {
      float phase = mod(gl_FragCoord.x + gl_FragCoord.y, 12.0);
      if (phase >= 8.0) {
        col = blend_color(col, vec4(0.0, 0.0, 0.0, 0.25));
      }
    }
  }

  /* Highlight. */
  if ((strip.flags & GPU_SEQ_FLAG_HIGHLIGHT) != 0) {
    col = blend_color(col, vec4(1.0, 1.0, 1.0, 48.0 / 255.0));
  }

  /* Handles. */
  vec4 col_outline = unpackUnorm4x8(strip.col_outline);
  if ((strip.flags & GPU_SEQ_FLAG_ANY_HANDLE) != 0) {
    bool left_side = pos.x < center.x;
    uint handle_flag = left_side ? GPU_SEQ_FLAG_SELECTED_LH : GPU_SEQ_FLAG_SELECTED_RH;
    bool selected_handle = (strip.flags & handle_flag) != 0;
    /* Blend in handle color in between strip shape and inner handle shape. */
    if (sdf <= 0.0 && sdf_inner >= 0.0) {
      vec4 hcol = selected_handle ? col_outline : vec4(0, 0, 0, 0.2);
      hcol.a *= clamp(sdf_inner, 0.0, 1.0);
      col = blend_color(col, hcol);
    }
    /* For an unselected handle, no longer take it into account
     * for the "inner" distance. */
    if (!selected_handle) {
      sdf_inner = sdf;
    }
  }

  /* Outside of strip rounded rectangle? */
  if (sdf > 0.0) {
    col = vec4(0.0);
  }

  /* Outline / border. */
  if (border) {

    if (selected) {
      /* Selection highlight + darker inset line. */
      col = add_outline(sdf, 1.0, 3.0, col, col_outline);
      /* Inset line should be inside regular border or inside the handles. */
      float d = max(sdf_inner - 3.0 * context_data.pixelsize, sdf);
      col = add_outline(d, 3.0, 4.0, col, vec4(0, 0, 0, 0.33));
    }

    /* Active, but not selected strips get a thin inner line. */
    bool active_strip = (strip.flags & GPU_SEQ_FLAG_ACTIVE) != 0;
    if (active_strip && !selected) {
      col = add_outline(sdf, 1.0, 2.0, col, col_outline);
    }

    /* 2px outline for all overlapping strips. */
    bool overlaps = (strip.flags & GPU_SEQ_FLAG_OVERLAP) != 0;
    if (overlaps) {
      col = add_outline(sdf, 1.0, 3.0, col, col_outline);
    }

    /* Outer 1px outline for all strips. */
    col = add_outline(sdf, 0.0, 1.0, col, unpackUnorm4x8(context_data.col_back));
  }

  fragColor = col;
}
