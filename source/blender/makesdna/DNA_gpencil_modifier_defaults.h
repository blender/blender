/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

/* Note that some struct members for color-mapping and color-bands are not initialized here. */

/* Struct members on own line. */
/* clang-format off */

#define _DNA_DEFAULT_ArmatureGpencilModifierData \
  { \
    .deformflag = ARM_DEF_VGROUP, \
    .multi = 0, \
    .object = NULL, \
    .vert_coords_prev = NULL, \
    .vgname = "", \
  }

#define _DNA_DEFAULT_ArrayGpencilModifierData \
  { \
    .object = NULL, \
    .material = NULL, \
    .count = 2, \
    .flag = GP_ARRAY_USE_RELATIVE, \
    .offset = {0.0f, 0.0f, 0.0f}, \
    .shift = {1.0f, 0.0f, 0.0f}, \
    .rnd_offset = {0.0f, 0.0f, 0.0f}, \
    .rnd_rot = {0.0f, 0.0f, 0.0f}, \
    .rnd_scale = {0.0f, 0.0f, 0.0f}, \
    .seed = 1, \
    .pass_index = 0, \
    .layername = "", \
    .mat_rpl = 0, \
    .layer_pass = 0, \
  }

/* Deliberately set this range to the half the default frame-range
 * to have an immediate effect to suggest use-cases. */
#define _DNA_DEFAULT_BuildGpencilModifierData \
  { \
    .material = NULL, \
    .layername = "", \
    .pass_index = 0, \
    .layer_pass = 0, \
    .start_frame = 1, \
    .end_frame = 125, \
    .start_delay = 0.0f, \
    .length = 100.0f, \
    .flag = 0, \
    .mode = 0, \
    .transition = 0, \
    .time_alignment = 0, \
    .time_mode = 0, \
    .speed_fac = 1.2f, \
    .speed_maxgap = 0.5f, \
    .percentage_fac = 0.0f, \
  }

#define _DNA_DEFAULT_ColorGpencilModifierData \
  { \
    .material = NULL, \
    .layername = "", \
    .pass_index = 0, \
    .flag = 0, \
    .hsv = {0.5f, 1.0f, 1.0f}, \
    .modify_color = GP_MODIFY_COLOR_BOTH, \
    .layer_pass = 0, \
    .curve_intensity = NULL, \
  }

#define _DNA_DEFAULT_HookGpencilModifierData \
  { \
    .object = NULL, \
    .material = NULL, \
    .subtarget = "", \
    .layername = "", \
    .vgname = "", \
    .pass_index = 0, \
    .layer_pass = 0, \
    .flag = 0, \
    .falloff_type = eGPHook_Falloff_Smooth, \
    .parentinv = _DNA_DEFAULT_UNIT_M4, \
    .cent = {0.0f, 0.0f, 0.0f}, \
    .falloff = 0.0f, \
    .force = 0.5f, \
    .curfalloff = NULL, \
  }

#define _DNA_DEFAULT_LatticeGpencilModifierData \
  { \
    .object = NULL, \
    .material = NULL, \
    .layername = "", \
    .vgname = "", \
    .pass_index = 0, \
    .flag = 0, \
    .strength = 1.0f, \
    .layer_pass = 0, \
    .cache_data = NULL, \
  }

#define _DNA_DEFAULT_MirrorGpencilModifierData \
  { \
    .object = NULL, \
    .material = NULL, \
    .layername = "", \
    .pass_index = 0, \
    .flag = GP_MIRROR_AXIS_X, \
    .layer_pass = 0, \
  }

#define _DNA_DEFAULT_MultiplyGpencilModifierData \
  { \
    .material = NULL, \
    .layername = "", \
    .pass_index = 0, \
    .flag = 0, \
    .layer_pass = 0, \
    .flags = 0, \
    .duplications = 3, \
    .distance = 0.1f, \
    .offset = 0.0f, \
    .fading_center = 0.5f, \
    .fading_thickness = 0.5f, \
    .fading_opacity = 0.5f, \
  }

#define _DNA_DEFAULT_NoiseGpencilModifierData \
  { \
    .material = NULL, \
    .layername = "", \
    .vgname = "", \
    .pass_index = 0, \
    .flag = GP_NOISE_FULL_STROKE | GP_NOISE_USE_RANDOM, \
    .factor = 0.5f, \
    .factor_strength = 0.0f, \
    .factor_thickness = 0.0f, \
    .factor_uvs = 0.0f, \
    .noise_scale = 0.0f, \
    .noise_offset = 0.0f, \
    .step = 4, \
    .layer_pass = 0, \
    .seed = 1, \
    .curve_intensity = NULL, \
  }

#define _DNA_DEFAULT_OffsetGpencilModifierData \
  { \
    .material = NULL, \
    .layername = "", \
    .vgname = "", \
    .pass_index = 0, \
    .flag = 0, \
    .stroke_step = 1, \
    .mode = GP_OFFSET_RANDOM, \
    .stroke_start_offset = 0, \
    .loc = {0.0f, 0.0f, 0.0f}, \
    .rot = {0.0f, 0.0f, 0.0f}, \
    .scale = {0.0f, 0.0f, 0.0f}, \
    .layer_pass = 0, \
  }

#define _DNA_DEFAULT_OpacityGpencilModifierData \
  { \
    .material = NULL, \
    .layername = "", \
    .vgname = "", \
    .pass_index = 0, \
    .flag = 0, \
    .factor = 1.0f, \
    .modify_color = GP_MODIFY_COLOR_BOTH, \
    .layer_pass = 0, \
    .hardness = 1.0f, \
    .curve_intensity = NULL, \
  }

#define _DNA_DEFAULT_OutlineGpencilModifierData \
  { \
    .material = NULL, \
    .layername = "", \
    .pass_index = 0, \
    .flag = GP_OUTLINE_KEEP_SHAPE, \
    .thickness = 1, \
    .sample_length = 0.0f, \
    .subdiv = 3, \
    .layer_pass = 0, \
    .outline_material = NULL, \
  }

#define _DNA_DEFAULT_SimplifyGpencilModifierData \
  { \
    .material = NULL, \
    .layername = "", \
    .pass_index = 0, \
    .flag = 0, \
    .factor = 0.0f, \
    .mode = 0, \
    .step = 1, \
    .layer_pass = 0, \
    .length = 0.1f, \
    .distance = 0.1f, \
  }

#define _DNA_DEFAULT_SmoothGpencilModifierData \
  { \
    .material = NULL, \
    .layername = "", \
    .vgname = "", \
    .pass_index = 0, \
    .flag = GP_SMOOTH_MOD_LOCATION, \
    .factor = 1.0f, \
    .step = 1, \
    .layer_pass = 0, \
    .curve_intensity = NULL, \
  }

#define _DNA_DEFAULT_SubdivGpencilModifierData \
  { \
    .material = NULL, \
    .layername = "", \
    .pass_index = 0, \
    .flag = 0, \
    .level = 1, \
    .layer_pass = 0, \
    .type = 0, \
  }

#define _DNA_DEFAULT_TextureGpencilModifierData \
  { \
    .material = NULL, \
    .layername = "", \
    .vgname = "", \
    .pass_index = 0, \
    .flag = 0, \
    .uv_offset = 0.0f, \
    .uv_scale = 1.0f, \
    .fill_rotation = 0.0f, \
    .fill_offset = {0.0f, 0.0f}, \
    .fill_scale = 1.0f, \
    .layer_pass = 0, \
    .fit_method = GP_TEX_CONSTANT_LENGTH, \
    .mode = 0, \
  }

#define _DNA_DEFAULT_ThickGpencilModifierData \
  { \
    .material = NULL, \
    .layername = "", \
    .vgname = "", \
    .pass_index = 0, \
    .flag = 0, \
    .thickness_fac = 1.0f, \
    .thickness = 30, \
    .layer_pass = 0, \
  }

#define _DNA_DEFAULT_TimeGpencilModifierData \
  { \
    .layername = "", \
    .layer_pass = 0, \
    .flag = GP_TIME_KEEP_LOOP, \
    .offset = 1, \
    .frame_scale = 1.0f, \
    .mode = 0, \
    .sfra = 1, \
    .efra = 250, \
    .segments = NULL, \
    .segments_len = 1, \
    .segment_active_index = 0, \
  }

  #define _DNA_DEFAULT_TimeGpencilModifierSegment \
  { \
    .name = "", \
    .seg_start = 1, \
    .seg_end = 2, \
    .seg_mode = 0, \
    .seg_repeat = 1, \
  }

#define _DNA_DEFAULT_TintGpencilModifierData \
  { \
    .object = NULL, \
    .material = NULL, \
    .layername = "", \
    .vgname = "", \
    .pass_index = 0, \
    .layer_pass = 0, \
    .flag = 0, \
    .mode = GPPAINT_MODE_BOTH, \
    .factor = 0.5f, \
    .radius = 1.0f, \
    .rgb = {1.0f, 1.0f, 1.0f}, \
    .type = 0, \
    .curve_intensity = NULL, \
    .colorband = NULL, \
  }

#define _DNA_DEFAULT_WeightProxGpencilModifierData \
  { \
    .target_vgname = "", \
    .material = NULL, \
    .layername = "", \
    .vgname = "", \
    .pass_index = 0, \
    .flag = 0, \
    .layer_pass = 0, \
    .dist_start = 0.0f, \
    .dist_end = 20.0f, \
  }

#define _DNA_DEFAULT_WeightAngleGpencilModifierData \
  { \
    .target_vgname = "", \
    .material = NULL, \
    .layername = "", \
    .vgname = "", \
    .pass_index = 0, \
    .flag = 0, \
    .axis = 1, \
    .layer_pass = 0, \
  }

#define _DNA_DEFAULT_LineartGpencilModifierData \
  { \
    .edge_types = LRT_EDGE_FLAG_INIT_TYPE, \
    .thickness = 25, \
    .opacity = 1.0f, \
    .crease_threshold = DEG2RAD(140.0f), \
    .calculation_flags = LRT_ALLOW_DUPLI_OBJECTS | LRT_ALLOW_CLIPPING_BOUNDARIES | \
                         LRT_USE_CREASE_ON_SHARP_EDGES | LRT_FILTER_FACE_MARK_KEEP_CONTOUR | \
                         LRT_GPENCIL_MATCH_OUTPUT_VGROUP, \
    /* Do not split by default, this is for better chaining quality. */ \
    .angle_splitting_threshold = 0.0f, \
    .chaining_image_threshold = 0.001f, \
    .stroke_depth_offset = 0.05,\
    .chain_smooth_tolerance = 0.0f,\
    .overscan = 0.1f,\
    .shadow_camera_near = 0.1f, \
    .shadow_camera_far = 200.0f, \
    .shadow_camera_size = 200.0f, \
  }

#define _DNA_DEFAULT_LengthGpencilModifierData \
  { \
    .start_fac = 0.1f,\
    .end_fac = 0.1f,\
    .overshoot_fac = 0.1f,\
    .pass_index = 0,\
    .material = NULL,\
    .flag = GP_LENGTH_USE_CURVATURE,\
    .point_density = 30.0f,\
    .segment_influence = 0.0f,\
    .max_angle = DEG2RAD(170.0f),\
    .rand_start_fac = 0.0f,\
    .rand_end_fac = 0.0f,\
    .rand_offset = 0.0f,\
    .seed = 0,\
    .step = 4,\
  }

#define _DNA_DEFAULT_DashGpencilModifierData \
  { \
    .dash_offset = 0, \
    .segments = NULL, \
    .segments_len = 1, \
    .segment_active_index = 0, \
  }

#define _DNA_DEFAULT_DashGpencilModifierSegment \
  { \
    .name = "", \
    .dash = 2, \
    .gap = 1, \
    .radius = 1.0f, \
    .opacity = 1.0f, \
    .mat_nr = -1, \
  }

#define _DNA_DEFAULT_ShrinkwrapGpencilModifierData \
  { \
    .target = NULL, \
    .aux_target = NULL, \
    .keep_dist = 0.05f, \
    .shrink_type = MOD_SHRINKWRAP_NEAREST_SURFACE, \
    .shrink_opts = MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR, \
    .shrink_mode = 0, \
    .proj_limit = 0.0f, \
    .proj_axis = 0, \
    .subsurf_levels = 0, \
    .material = NULL, \
    .layername = "", \
    .vgname = "", \
    .pass_index = 0, \
    .flag = 0, \
    .layer_pass = 0, \
    .smooth_factor = 0.05f, \
    .smooth_step = 1, \
  }

#define _DNA_DEFAULT_EnvelopeGpencilModifierData \
  { \
    .spread = 10, \
    .mode = GP_ENVELOPE_SEGMENTS, \
    .mat_nr = -1, \
    .thickness = 1.0f, \
    .strength = 1.0f, \
    .skip = 0, \
  }


/* clang-format off */
