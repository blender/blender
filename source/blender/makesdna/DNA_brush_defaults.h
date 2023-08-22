/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_texture_defaults.h"

/* Struct members on own line. */
/* clang-format off */

/* -------------------------------------------------------------------- */
/** \name Brush Struct
 * \{ */

#define _DNA_DEFAULT_DynTopoSettings \
{\
  .detail_percent = 25.0f,\
  .detail_size = 8.0f,\
  .constant_detail = 16.0f,\
  .flag = DYNTOPO_COLLAPSE|DYNTOPO_SUBDIVIDE|DYNTOPO_CLEANUP,\
  .mode = DYNTOPO_DETAIL_RELATIVE,\
  .inherit = DYNTOPO_INHERIT_BITMASK,\
  .spacing = 35,\
  .radius_scale = 1.0f,\
  .quality = 1.0f,\
}

#define _DNA_DEFAULT_Brush \
  { \
    .blend = 0, \
    .flag = (BRUSH_ALPHA_PRESSURE | BRUSH_SPACE | BRUSH_SPACE_ATTEN), \
    .sampling_flag = (BRUSH_PAINT_ANTIALIASING), \
 \
    .ob_mode = OB_MODE_ALL_PAINT, \
 \
    /* BRUSH SCULPT TOOL SETTINGS */ \
    .weight = 1.0f, /* weight of brush 0 - 1.0 */ \
    .size = 35,     /* radius of the brush in pixels */ \
    .alpha = 1.0f,  /* brush strength/intensity probably variable should be renamed? */ \
    .autosmooth_factor = 0.0f, \
    .autosmooth_projection = 0.0f,\
    .autosmooth_radius_factor = 1.0f,\
    .autosmooth_spacing = 12,\
    .topology_rake_factor = 0.0f, \
    .topology_rake_projection = 1.0f,\
    .topology_rake_radius_factor = 1.0f,\
    .topology_rake_spacing = 12,\
    .crease_pinch_factor = 0.5f, \
    .normal_radius_factor = 0.5f, \
    .wet_paint_radius_factor = 0.5f, \
    .area_radius_factor = 0.5f, \
    .disconnected_distance_max = 0.1f, \
    .sculpt_plane = SCULPT_DISP_DIR_AREA, \
    .cloth_damping = 0.01, \
    .cloth_mass = 1, \
    .cloth_sim_limit = 2.5f, \
    .cloth_sim_falloff = 0.75f, \
    /* How far above or below the plane that is found by averaging the faces. */ \
    .plane_offset = 0.0f, \
    .plane_trim = 0.5f, \
    .clone.alpha = 0.5f, \
    .normal_weight = 0.0f, \
    .fill_threshold = 0.2f, \
 \
    /* BRUSH PAINT TOOL SETTINGS */ \
    /* Default rgb color of the brush when painting - white. */ \
    .rgb = {1.0f, 1.0f, 1.0f}, \
 \
    .secondary_rgb = {0, 0, 0}, \
 \
    /* BRUSH STROKE SETTINGS */ \
    /* How far each brush dot should be spaced as a percentage of brush diameter. */ \
    .spacing = 10, \
 \
    .smooth_stroke_radius = 75, \
    .smooth_stroke_factor = 0.9f, \
 \
    .smear_deform_blend = 0.5f,\
    /* Time delay between dots of paint or sculpting when doing airbrush mode. */ \
    .rate = 0.1f, \
 \
    .jitter = 0.0f, \
 \
    /* Dash */ \
    .dash_ratio = 1.0f, \
    .dash_samples = 20, \
 \
    .texture_sample_bias = 0, /* value to added to texture samples */ \
    .texture_overlay_alpha = 33, \
    .mask_overlay_alpha = 33, \
    .cursor_overlay_alpha = 33, \
    .overlay_flags = 0, \
 \
    /* Brush appearance. */ \
 \
    /* add mode color is light red */ \
    .add_col = {1.0, 0.39, 0.39, 0.9}, \
 \
    /* subtract mode color is light blue */ \
    .sub_col = {0.39, 0.39, 1.0, 0.9}, \
 \
    .stencil_pos = {256, 256}, \
    .stencil_dimension = {256, 256}, \
 \
    /* sculpting defaults to the draw tool for new brushes */ \
    .sculpt_tool = SCULPT_TOOL_DRAW, \
    .pose_smooth_iterations = 4, \
    .pose_ik_segments = 1, \
    .hardness = 0.0f, \
    .automasking_boundary_edges_propagation_steps = 1, \
    .automasking_cavity_blur_steps = 0,\
    .automasking_cavity_factor = 1.0f,\
 \
    /* A kernel radius of 1 has almost no effect (#63233). */ \
    .blur_kernel_radius = 2, \
 \
    .mtex = _DNA_DEFAULT_MTex, \
    .mask_mtex = _DNA_DEFAULT_MTex, \
    .dyntopo = _DNA_DEFAULT_DynTopoSettings,\
    .concave_mask_factor = 0.75f,\
    .falloff_shape = 0,\
    .hard_corner_pin = 1.0f,\
    .sharp_angle_limit = 0.38f,\
    .tip_scale_x = 1.0f,\
    .tip_roundness = 1.0f,\
  }
/** \} */

/* clang-format on */
