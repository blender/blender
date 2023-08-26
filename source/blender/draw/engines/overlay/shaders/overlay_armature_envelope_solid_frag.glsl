/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  float n = normalize(normalView).z;
  if (isDistance) {
    n = 1.0 - clamp(-n, 0.0, 1.0);
    fragColor = vec4(1.0, 1.0, 1.0, 0.33 * alpha) * n;
  }
  else {
    /* Smooth lighting factor. */
    const float s = 0.2; /* [0.0-0.5] range */
    float fac = clamp((n * (1.0 - s)) + s, 0.0, 1.0);
    fragColor.rgb = mix(finalStateColor, finalBoneColor, fac * fac);
    fragColor.a = alpha;
  }
  lineOutput = vec4(0.0);
}
