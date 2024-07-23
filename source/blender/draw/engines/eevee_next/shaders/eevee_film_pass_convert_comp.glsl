/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Used by the Viewport Compositor to copy EEVEE passes to the compositor DRW passes textures. The
 * output passes covert the entire display extent even when border rendering because that's what
 * the compositor expects, so areas outside of the border are zeroed. */

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  if (any(greaterThan(texel, imageSize(output_img) - ivec2(1)))) {
    return;
  }

  /* In case of border rendering, clear areas outside of the border. The offset is the lower left
   * corner of the border. */
  ivec2 input_bounds = textureSize(input_tx, 0).xy - ivec2(1);
  if (any(lessThan(texel, offset)) || any(greaterThan(texel, offset + input_bounds))) {
    imageStoreFast(output_img, texel, vec4(0.0));
    return;
  }

/* The input can be a single slice of an array texture, in which case just sample the 0th layer of
 * the array texture. Subtract the offset because in case of border rendering, the inputs only
 * contain the data inside the border. */
#if defined(IS_ARRAY_INPUT)
  imageStoreFast(output_img, texel, texelFetch(input_tx, ivec3(texel - offset, 0), 0));
#else
  imageStoreFast(output_img, texel, texelFetch(input_tx, texel - offset, 0));
#endif
}
