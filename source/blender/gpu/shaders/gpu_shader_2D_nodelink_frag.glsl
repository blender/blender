/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#define ANTIALIAS 1.5
#define MINIMUM_ALPHA 0.5

void main()
{
  fragColor = finalColor;

  if ((isMainLine != 0) && (dashFactor < 1.0)) {
    float distance_along_line = lineLength * lineU;
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

    fragColor.a *= alpha;
  }

  fragColor.a *= smoothstep(lineThickness, lineThickness - ANTIALIAS, abs(colorGradient));
}
