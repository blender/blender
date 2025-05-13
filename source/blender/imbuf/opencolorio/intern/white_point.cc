/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "white_point.hh"

#include "BLI_math_color.hh"
#include "BLI_math_matrix.hh"

#include "OCIO_config.hh"

namespace blender::ocio {

float3x3 calculate_white_point_matrix(const Config &config,
                                      const float temperature,
                                      const float tint)
{
  /* Compute white point of the scene space in XYZ. */
  const float3x3 xyz_to_scene = config.get_xyz_to_scene_linear_matrix();
  const float3x3 scene_to_xyz = math::invert(xyz_to_scene);
  const float3 target = scene_to_xyz * float3(1.0f);

  /* Add operations to the matrix.
   * Note: Since we're multiplying from the right, the operations here will be performed in
   * reverse list order (scene-to-XYZ, then adaption, then XYZ-to-scene, then exposure). */
  float3x3 matrix = xyz_to_scene;
  matrix *= math::chromatic_adaption_matrix(math::whitepoint_from_temp_tint(temperature, tint),
                                            target);
  matrix *= scene_to_xyz;

  return matrix;
}

}  // namespace blender::ocio
