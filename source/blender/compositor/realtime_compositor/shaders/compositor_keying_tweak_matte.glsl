#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  float matte = texture_load(input_matte_tx, texel).x;

  /* Search the neighbourhood around the current matte value and identify if it lies along the
   * edges of the matte. This is needs to be computed only when we need to compute the edges output
   * or tweak the levels of the matte. */
  bool is_edge = false;
  if (compute_edges || black_level != 0.0 || white_level != 1.0) {
    /* Count the number of neighbours whose matte is sufficiently similar to the current matte,
     * as controlled by the edge_tolerance factor. */
    int count = 0;
    for (int j = -edge_search_radius; j <= edge_search_radius; j++) {
      for (int i = -edge_search_radius; i <= edge_search_radius; i++) {
        float neighbour_matte = texture_load(input_matte_tx, texel + ivec2(i, j)).x;
        count += int(distance(matte, neighbour_matte) < edge_tolerance);
      }
    }

    /* If the number of neighbours that are sufficiently similar to the center matte is less that
     * 90% of the total number of neighbours, then that means the variance is high in that areas
     * and it is considered an edge. */
    is_edge = count < ((edge_search_radius * 2 + 1) * (edge_search_radius * 2 + 1)) * 0.9;
  }

  float tweaked_matte = matte;

  /* Remap the matte using the black and white levels, but only for areas that are not on the edge
   * of the matte to preserve details. Also check for equality between levels to avoid zero
   * division. */
  if (!is_edge && white_level != black_level) {
    tweaked_matte = clamp((matte - black_level) / (white_level - black_level), 0.0, 1.0);
  }

  /* Exclude unwanted areas using the provided garbage matte, 1 means unwanted, so invert the
   * garbage matte and take the minimum. */
  if (apply_garbage_matte) {
    float garbage_matte = texture_load(garbage_matte_tx, texel).x;
    tweaked_matte = min(tweaked_matte, 1.0 - garbage_matte);
  }

  /* Include wanted areas that were incorrectly keyed using the provided core matte. */
  if (apply_core_matte) {
    float core_matte = texture_load(core_matte_tx, texel).x;
    tweaked_matte = max(tweaked_matte, core_matte);
  }

  imageStore(output_matte_img, texel, vec4(tweaked_matte));
  imageStore(output_edges_img, texel, vec4(is_edge ? 1.0 : 0.0));
}
