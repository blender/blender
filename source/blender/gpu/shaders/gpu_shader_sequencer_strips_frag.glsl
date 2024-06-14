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
 * in an outline of at least 1px width (plus `extra_half_width` on each side), inset
 * by `inset` pixels. Outline color `outline_color` is in straight alpha. */
vec4 add_outline(float d, float extra_half_width, float inset, vec4 cur, vec4 outline_color)
{
  float f = abs(d + inset) - extra_half_width;
  float a = clamp(1.0 - f, 0.0, 1.0);
  outline_color.a *= a;
  return blend_color(cur, outline_color);
}

void main()
{
  vec2 co = co_interp;

  SeqStripDrawData strip = strip_data[strip_id];

  /* Transform strip rectangle into pixel coordinates, so that
   * rounded corners have proper aspect ratio and can be expressed in pixels.
   * Make sure strip right side does not include the last pixel.
   * Also snap to pixel grid coordinates, so that outline/border is clear
   * non-fractional pixel sizes. */
  vec2 view_to_pixel = vec2(context_data.inv_pixelx, context_data.inv_pixely);
  vec2 pos1 = round(vec2(strip.left_handle, strip.bottom) * view_to_pixel);
  vec2 pos2 = round(vec2(strip.right_handle, strip.top) * view_to_pixel);
  pos2.x -= 1.0;
  /* Make sure strip is at least 1px wide. */
  pos2.x = max(pos2.x, pos1.x + 1.0);
  vec2 size = (pos2 - pos1) * 0.5;
  vec2 center = (pos1 + pos2) * 0.5;
  vec2 pos = round(co * view_to_pixel);

  float radius = context_data.round_radius;
  if (radius > size.x) {
    radius = 0.0;
  }

  float sdf = sdf_rounded_box(pos - center, size, radius);

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
      if (co.y > strip.strip_content_top - context_data.pixely) {
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
  if ((strip.flags & GPU_SEQ_FLAG_HANDLES) != 0) {
    float handle_width = strip.handle_width * view_to_pixel.x;
    if (pos.x >= pos1.x && pos.x < pos1.x + handle_width) {
      col = blend_color(col, unpackUnorm4x8(strip.col_handle_left));
    }
    if (pos.x > pos2.x - handle_width && pos.x <= pos2.x) {
      col = blend_color(col, unpackUnorm4x8(strip.col_handle_right));
    }
  }

  /* Outside of strip rounded rectangle? */
  if (sdf > 0.0) {
    col = vec4(0.0);
  }

  /* Outline / border. */
  if ((strip.flags & GPU_SEQ_FLAG_BORDER) != 0) {
    bool selected = (strip.flags & GPU_SEQ_FLAG_SELECTED) != 0;
    vec4 col_outline = unpackUnorm4x8(strip.col_outline);
    if (selected) {
      /* Inset 1px line with background color. */
      col = add_outline(sdf, 0.0, 2.0, col, unpackUnorm4x8(context_data.col_back));
      /* 2x wide outline. */
      col = add_outline(sdf, 0.5, 0.5, col, col_outline);
    }
    else {
      col = add_outline(sdf, 0.0, 0.0, col, col_outline);
    }
  }

  fragColor = col;
}
