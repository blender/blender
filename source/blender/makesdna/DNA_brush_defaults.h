/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

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
    .topology_rake_factor = 0.0f, \
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
 \
    /* A kernel radius of 1 has almost no effect (T63233). */ \
    .blur_kernel_radius = 2, \
 \
    .mtex = _DNA_DEFAULT_MTex, \
    .mask_mtex = _DNA_DEFAULT_MTex, \
  }

/** \} */

/* clang-format on */
