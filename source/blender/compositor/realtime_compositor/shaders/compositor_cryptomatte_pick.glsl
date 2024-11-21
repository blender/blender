/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Blender provides a Cryptomatte picker operator (UI_OT_eyedropper_color) that can pick a
 * Cryptomatte entity from an image. That image is a specially encoded image that the picker
 * operator can understand. In particular, its red channel is the identifier of the entity in the
 * first rank, while the green and blue channels are arbitrary [0, 1] compressed versions of the
 * identifier to make the image more humane-viewable, but they are actually ignored by the picker
 * operator, as can be seen in functions like eyedropper_color_sample_text_update, where only the
 * red channel is considered.
 *
 * This shader just computes this special image given the first Cryptomatte layer. The output needs
 * to be in full precision since the identifier is a 32-bit float.
 *
 * This is the same concept as the "keyable" image described in section "Matte Extraction:
 * Implementation Details" in the original Cryptomatte publication:
 *
 *   Friedman, Jonah, and Andrew C. Jones. "Fully automatic id mattes with support for motion blur
 *   and transparency." ACM SIGGRAPH 2015 Posters. 2015. 1-1.
 *
 * Except we put the identifier in the red channel by convention instead of the suggested blue
 * channel. */

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* Each layer stores two ranks, each rank contains a pair, the identifier and the coverage of
   * the entity identified by the identifier. */
  vec2 first_rank = texture_load(first_layer_tx, texel + lower_bound).xy;
  float id_of_first_rank = first_rank.x;

  /* There is no logic to this, we just compute arbitrary compressed versions of the identifier in
   * the [0, 1] range to make the image more human-viewable. */
  uint hash_value = floatBitsToUint(id_of_first_rank);
  float green = float(hash_value << 8) / float(0xFFFFFFFFu);
  float blue = float(hash_value << 16) / float(0xFFFFFFFFu);

  imageStore(output_img, texel, vec4(id_of_first_rank, green, blue, 1.0));
}
