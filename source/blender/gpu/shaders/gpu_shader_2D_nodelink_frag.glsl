/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#define ANTIALIAS 0.75

float get_line_alpha(float center, float relative_radius)
{
  float radius = relative_radius * lineThickness;
  float sdf = abs(lineThickness * (lineUV.y - center));
  return smoothstep(radius, radius - ANTIALIAS, sdf);
}

void main()
{
  if (isMainLine == 0) {
    fragColor = finalColor;
    fragColor.a *= get_line_alpha(0.5, 0.5);
    return;
  }

  float dash_frag_alpha = 1.0;
  if (dashFactor < 1.0) {
    float distance_along_line = lineLength * lineUV.x;
    float normalized_distance = fract(distance_along_line / dashLength);

    /* Checking if `normalized_distance <= dashFactor` is already enough for a basic
     * dash, however we want to handle a nice anti-alias. */

    float dash_center = dashLength * dashFactor * 0.5;
    float normalized_distance_triangle =
        1.0 - abs((fract((distance_along_line - dash_center) / dashLength)) * 2.0 - 1.0);
    float t = aspect * ANTIALIAS / dashLength;
    float slope = 1.0 / (2.0 * t);

    float unclamped_alpha = 1.0 - slope * (normalized_distance_triangle - dashFactor + t);
    float alpha = max(dashAlpha, min(unclamped_alpha, 1.0));

    dash_frag_alpha = alpha;
  }

  if (hasBackLink == 0) {
    fragColor = finalColor;
    fragColor.a *= get_line_alpha(0.5, 0.5) * dash_frag_alpha;
  }
  else {
    /* Draw two links right next to each other, the main link and the back-link. */
    vec4 main_link_color = finalColor;
    main_link_color.a *= get_line_alpha(0.75, 0.3);

    vec4 back_link_color = vec4(vec3(0.8), 1.0);
    back_link_color.a *= get_line_alpha(0.2, 0.25);

    /* Combine both links. */
    fragColor.rgb = main_link_color.rgb * main_link_color.a +
                    back_link_color.rgb * back_link_color.a;
    fragColor.a = main_link_color.a * dash_frag_alpha + back_link_color.a;
  }
}
