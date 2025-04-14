/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Loops over all identifiers selected by the user, and accumulate the coverage of ranks whose
 * identifiers match that of the user selected identifiers.
 *
 * This is described in section "Matte Extraction: Implementation Details" in the original
 * Cryptomatte publication:
 *
 *   Friedman, Jonah, and Andrew C. Jones. "Fully automatic id mattes with support for motion blur
 *   and transparency." ACM SIGGRAPH 2015 Posters. 2015. 1-1.
 */

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 layer = texture_load(layer_tx, texel + lower_bound);

  /* Each Cryptomatte layer stores two ranks. */
  float2 first_rank = layer.xy;
  float2 second_rank = layer.zw;

  /* Each Cryptomatte rank stores a pair of an identifier and the coverage of the entity identified
   * by that identifier. */
  float identifier_of_first_rank = first_rank.x;
  float coverage_of_first_rank = first_rank.y;
  float identifier_of_second_rank = second_rank.x;
  float coverage_of_second_rank = second_rank.y;

  /* Loop over all identifiers selected by the user, if the identifier of either of the ranks match
   * it, accumulate its coverage. */
  float total_coverage = 0.0f;
  for (int i = 0; i < identifiers_count; i++) {
    float identifier = identifiers[i];
    if (identifier_of_first_rank == identifier) {
      total_coverage += coverage_of_first_rank;
    }
    if (identifier_of_second_rank == identifier) {
      total_coverage += coverage_of_second_rank;
    }
  }

  /* Add the total coverage to the coverage accumulated by previous layers. */
  imageStore(matte_img, texel, imageLoad(matte_img, texel) + float4(total_coverage));
}
