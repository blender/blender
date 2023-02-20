
const float diagonal_scale = sqrt(0.5);

const float minmax_bias = 0.7;
const float minmax_scale = sqrt(1.0 / (1.0 + 1.0 / minmax_bias));

bool test(uint bit)
{
  return (finalFlags & bit) != 0u;
}

void main()
{
  vec2 pos = gl_PointCoord - vec2(0.5);
  vec2 absPos = abs(pos);
  float radius = (absPos.x + absPos.y) * diagonal_scale;

  float outline_dist = -1.0;

  /* Diamond outline */
  if (test(GPU_KEYFRAME_SHAPE_DIAMOND)) {
    outline_dist = max(outline_dist, radius - radii[0]);
  }

  /* Circle outline */
  if (test(GPU_KEYFRAME_SHAPE_CIRCLE)) {
    radius = length(absPos);

    outline_dist = max(outline_dist, radius - radii[1]);
  }

  /* Top & Bottom clamp */
  if (test(GPU_KEYFRAME_SHAPE_CLIPPED_VERTICAL)) {
    outline_dist = max(outline_dist, absPos.y - radii[2]);
  }

  /* Left & Right clamp */
  if (test(GPU_KEYFRAME_SHAPE_CLIPPED_HORIZONTAL)) {
    outline_dist = max(outline_dist, absPos.x - radii[2]);
  }

  float alpha = 1 - smoothstep(thresholds[0], thresholds[1], abs(outline_dist));

  /* Inside the outline. */
  if (outline_dist < 0) {
    /* Middle dot */
    if (test(GPU_KEYFRAME_SHAPE_INNER_DOT)) {
      alpha = max(alpha, 1 - smoothstep(thresholds[2], thresholds[3], length(absPos)));
    }

    /* Up and down arrow-like shading. */
    if (test(GPU_KEYFRAME_SHAPE_ARROW_END_MAX | GPU_KEYFRAME_SHAPE_ARROW_END_MIN)) {
      float ypos = -1.0;

      /* Up arrow (maximum) */
      if (test(GPU_KEYFRAME_SHAPE_ARROW_END_MAX)) {
        ypos = max(ypos, pos.y);
      }
      /* Down arrow (minimum) */
      if (test(GPU_KEYFRAME_SHAPE_ARROW_END_MIN)) {
        ypos = max(ypos, -pos.y);
      }

      /* Arrow shape threshold. */
      float minmax_dist = (ypos - radii[3]) - absPos.x * minmax_bias;
      float minmax_step = smoothstep(thresholds[0], thresholds[1], minmax_dist * minmax_scale);

      /* Reduced alpha for uncertain extremes. */
      float minmax_alpha = test(GPU_KEYFRAME_SHAPE_ARROW_END_MIXED) ? 0.55 : 0.85;

      alpha = max(alpha, minmax_step * minmax_alpha);
    }

    fragColor = mix(finalColor, finalOutlineColor, alpha);
  }
  /* Outside the outline. */
  else {
    fragColor = vec4(finalOutlineColor.rgb, finalOutlineColor.a * alpha);
  }
}
