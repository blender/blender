/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Must match eGPLayerBlendModes */
#define MODE_REGULAR 0
#define MODE_HARDLIGHT 1
#define MODE_ADD 2
#define MODE_SUB 3
#define MODE_MULTIPLY 4
#define MODE_DIVIDE 5
#define MODE_HARDLIGHT_SECOND_PASS 999

void blend_mode_output(
    int blend_mode, vec4 color, float opacity, out vec4 frag_color, out vec4 frag_revealage)
{
  switch (blend_mode) {
    case MODE_REGULAR:
      /* Reminder: Blending func is pre-multiply alpha blend
       * `(dst.rgba * (1 - src.a) + src.rgb)`. */
      color *= opacity;
      frag_color = color;
      frag_revealage = vec4(0.0, 0.0, 0.0, color.a);
      break;
    case MODE_MULTIPLY:
      /* Reminder: Blending func is multiply blend `(dst.rgba * src.rgba)`. */
      color.a *= opacity;
      frag_revealage = frag_color = (1.0 - color.a) + color.a * color;
      break;
    case MODE_DIVIDE:
      /* Reminder: Blending func is multiply blend `(dst.rgba * src.rgba)`. */
      color.a *= opacity;
      frag_revealage = frag_color = clamp(1.0 / max(vec4(1e-6), 1.0 - color * color.a), 0.0, 1e18);
      break;
    case MODE_HARDLIGHT: {
      /* Reminder: Blending func is multiply blend `(dst.rgba * src.rgba)`. */
      /**
       * We need to separate the overlay equation into 2 term (one multiply and one add).
       * This is the standard overlay equation (per channel):
       * `rtn = (src < 0.5) ? (2.0 * src * dst) : (1.0 - 2.0 * (1.0 - src) * (1.0 - dst));`
       * We rewrite the second branch like this:
       * `rtn = 1 - 2 * (1 - src) * (1 - dst);`
       * `rtn = 1 - 2 (1 - dst + src * dst - src);`
       * `rtn = 1 - 2 (1 - dst * (1 - src) - src);`
       * `rtn = 1 - 2 + dst * (2 - 2 * src) + 2 * src;`
       * `rtn = (- 1 + 2 * src) + dst * (2 - 2 * src);`
       */
      color = mix(vec4(0.5), color, color.a * opacity);
      vec4 s = step(-0.5, -color);
      frag_revealage = frag_color = 2.0 * s + 2.0 * color * (1.0 - s * 2.0);
      frag_revealage = max(vec4(0.0), frag_revealage);
      break;
    }
    case MODE_HARDLIGHT_SECOND_PASS:
      /* Reminder: Blending func is additive blend `(dst.rgba + src.rgba)`. */
      color = mix(vec4(0.5), color, color.a * opacity);
      frag_revealage = frag_color = (-1.0 + 2.0 * color) * step(-0.5, -color);
      frag_revealage = max(vec4(0.0), frag_revealage);
      break;
    case MODE_SUB:
    case MODE_ADD:
      /* Reminder: Blending func is additive / subtractive blend `(dst.rgba +/- src.rgba)`. */
      frag_color = color * color.a * opacity;
      frag_revealage = vec4(0.0);
      break;
  }
}
