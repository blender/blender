/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Takes scene surfel representation and build list of surfels aligning in a given direction.
 *
 * The lists head are allocated to fit the surfel granularity.
 *
 * Due to alignment the link and list head are split into several int arrays to avoid too much
 * memory waste.
 *
 * Dispatch 1 thread per surfel.
 */

#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_base_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_surfel_list_lib.glsl)

void main()
{
  int surfel_index = int(gl_GlobalInvocationID.x);
  if (surfel_index >= capture_info_buf.surfel_len) {
    return;
  }

  float ray_distance;
  int list_index = surfel_list_index_get(
      list_info_buf.ray_grid_size, surfel_buf[surfel_index].position, ray_distance);
  /* Do separate assignement to avoid reference to buffer in arguments which is tricky to cross
   * compile. */
  surfel_buf[surfel_index].ray_distance = ray_distance;
  /* NOTE: We only need to init the `list_start_buf` to -1 for the whole list to be valid since
   * every surfel will load its `next` value from the list head. */
  surfel_buf[surfel_index].next = atomicExchange(list_start_buf[list_index], surfel_index);
}
