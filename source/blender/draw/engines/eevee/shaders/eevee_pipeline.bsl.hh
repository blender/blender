/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/* A bit out of place but needs to be included before . */
namespace eevee {

struct PipelineConstants {
  [[compilation_constant]] bool use_velocity;
  [[compilation_constant]] bool use_transparency;
  [[compilation_constant]] bool use_clip_plane;
  [[compilation_constant]] bool use_sss;
  [[compilation_constant]] bool is_shadow_pipe;
  [[compilation_constant]] int closure_bin_count;
};

}  // namespace eevee
