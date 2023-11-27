/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  fragColor = vec4(0.0);
  /* Manual curve fit of the falloff curve of previous drawing method. */
  float shadow_alpha = alpha * (shadowFalloff * shadowFalloff * 0.722 + shadowFalloff * 0.277);
  float inner_alpha = smoothstep(0.0, 0.05, innerMask);

  fragColor.a = inner_alpha * shadow_alpha;
}
