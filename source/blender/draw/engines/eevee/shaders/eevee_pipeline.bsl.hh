/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

/* A bit out of place but needs to be included before . */
namespace eevee {

struct PipelineConstants {
  [[compilation_constant]] bool is_mesh;
  [[compilation_constant]] bool is_curves;
  [[compilation_constant]] bool is_pointcloud;
  [[compilation_constant]] bool is_gsplat;
  [[compilation_constant]] bool is_world;
  [[compilation_constant]] bool use_lighting_nodes;
  [[compilation_constant]] bool use_ambient_occlusion;
  [[compilation_constant]] bool use_velocity;
  [[compilation_constant]] bool use_transparency;
  [[compilation_constant]] bool use_clip_plane;
  [[compilation_constant]] bool use_sss;
  [[compilation_constant]] bool use_aov_output;
  [[compilation_constant]] bool use_raycast;
  [[compilation_constant]] bool use_additional_data;
  [[compilation_constant]] bool is_volume_pipe;
  [[compilation_constant]] bool is_shadow_pipe;
  [[compilation_constant]] bool is_occupancy_pipe;
  [[compilation_constant]] bool use_forward_lighting;
  [[compilation_constant]] bool use_multi_viewport;
  [[compilation_constant]] int closure_bin_count;
};

}  // namespace eevee
