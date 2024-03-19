/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef GPU_SHADER
#  pragma once

#  include "GPU_shader_shared_utils.h"

#  include "DNA_action_types.h"
#  include "DNA_view3d_types.h"

#  ifdef __cplusplus
extern "C" {
#  else
typedef enum OVERLAY_GridBits OVERLAY_GridBits;
#  endif
typedef struct OVERLAY_GridData OVERLAY_GridData;
typedef struct ThemeColorData ThemeColorData;
typedef struct ExtraInstanceData ExtraInstanceData;
#endif

/* TODO(fclem): Should eventually become OVERLAY_BackgroundType.
 * But there is no uint push constant functions at the moment. */
#define BG_SOLID 0
#define BG_GRADIENT 1
#define BG_CHECKER 2
#define BG_RADIAL 3
#define BG_SOLID_CHECKER 4
#define BG_MASK 5

enum OVERLAY_GridBits {
  SHOW_AXIS_X = (1u << 0u),
  SHOW_AXIS_Y = (1u << 1u),
  SHOW_AXIS_Z = (1u << 2u),
  SHOW_GRID = (1u << 3u),
  PLANE_XY = (1u << 4u),
  PLANE_XZ = (1u << 5u),
  PLANE_YZ = (1u << 6u),
  CLIP_ZPOS = (1u << 7u),
  CLIP_ZNEG = (1u << 8u),
  GRID_BACK = (1u << 9u),
  GRID_CAMERA = (1u << 10u),
  PLANE_IMAGE = (1u << 11u),
  CUSTOM_GRID = (1u << 12u),
};
#ifndef GPU_SHADER
ENUM_OPERATORS(OVERLAY_GridBits, CUSTOM_GRID)
#endif

/* Match: #SI_GRID_STEPS_LEN */
#define OVERLAY_GRID_STEPS_LEN 8

struct OVERLAY_GridData {
  float4 steps[OVERLAY_GRID_STEPS_LEN]; /* float arrays are padded to float4 in std130. */
  float4 size;                          /* float3 padded to float4. */
  float distance;
  float line_size;
  float zoom_factor; /* Only for UV editor */
  float _pad0;
};
BLI_STATIC_ASSERT_ALIGN(OVERLAY_GridData, 16)

#ifdef GPU_SHADER
/* Keep the same values as in `draw_cache_impl_curves.cc` */
#  define EDIT_CURVES_NURBS_CONTROL_POINT (1u)
#  define EDIT_CURVES_BEZIER_HANDLE (1u << 1)
#  define EDIT_CURVES_LEFT_HANDLE_TYPES_SHIFT (6u)
#  define EDIT_CURVES_RIGHT_HANDLE_TYPES_SHIFT (4u)
/* Keep the same values as in `draw_cache_imp_curve.c` */
#  define ACTIVE_NURB (1u << 2)
#  define BEZIER_HANDLE (1u << 3)
#  define EVEN_U_BIT (1u << 4)
#  define COLOR_SHIFT 5u

/* Keep the same value in `handle_display` in `DNA_view3d_types.h` */
#  define CURVE_HANDLE_SELECTED 0u
#  define CURVE_HANDLE_ALL 1u

#  define GP_EDIT_POINT_SELECTED 1u  /* 1 << 0 */
#  define GP_EDIT_STROKE_SELECTED 2u /* 1 << 1 */
#  define GP_EDIT_MULTIFRAME 4u      /* 1 << 2 */
#  define GP_EDIT_STROKE_START 8u    /* 1 << 3 */
#  define GP_EDIT_STROKE_END 16u     /* 1 << 4 */
#  define GP_EDIT_POINT_DIMMED 32u   /* 1 << 5 */

#  define MOTIONPATH_VERT_SEL (1u << 0)
#  define MOTIONPATH_VERT_KEY (1u << 1)

#else
/* TODO(fclem): Find a better way to share enums/defines from DNA files with GLSL. */
BLI_STATIC_ASSERT(CURVE_HANDLE_SELECTED == 0u, "Ensure value is sync");
BLI_STATIC_ASSERT(CURVE_HANDLE_ALL == 1u, "Ensure value is sync");
BLI_STATIC_ASSERT(MOTIONPATH_VERT_SEL == (1u << 0), "Ensure value is sync");
BLI_STATIC_ASSERT(MOTIONPATH_VERT_KEY == (1u << 1), "Ensure value is sync");
#endif

struct ThemeColorData {
  float4 color_wire;
  float4 color_wire_edit;
  float4 color_active;
  float4 color_select;
  float4 color_library_select;
  float4 color_library;
  float4 color_transform;
  float4 color_light;
  float4 color_speaker;
  float4 color_camera;
  float4 color_camera_path;
  float4 color_empty;
  float4 color_vertex;
  float4 color_vertex_select;
  float4 color_vertex_unreferenced;
  float4 color_vertex_missing_data;
  float4 color_edit_mesh_active;
  /** For edge selection, not edge select mode. */
  float4 color_edge_select;
  /** For edge mode selection. */
  float4 color_edge_mode_select;
  float4 color_edge_seam;
  float4 color_edge_sharp;
  float4 color_edge_crease;
  float4 color_edge_bweight;
  float4 color_edge_face_select;
  float4 color_edge_freestyle;
  float4 color_face;
  /** For face selection, not face select mode. */
  float4 color_face_select;
  /** For face mode selection. */
  float4 color_face_mode_select;
  float4 color_face_freestyle;
  float4 color_gpencil_vertex;
  float4 color_gpencil_vertex_select;
  float4 color_normal;
  float4 color_vnormal;
  float4 color_lnormal;
  float4 color_facedot;
  float4 color_skinroot;

  float4 color_deselect;
  float4 color_outline;
  float4 color_light_no_alpha;

  float4 color_background;
  float4 color_background_gradient;
  float4 color_checker_primary;
  float4 color_checker_secondary;
  float4 color_clipping_border;
  float4 color_edit_mesh_middle;

  float4 color_handle_free;
  float4 color_handle_auto;
  float4 color_handle_vect;
  float4 color_handle_align;
  float4 color_handle_autoclamp;
  float4 color_handle_sel_free;
  float4 color_handle_sel_auto;
  float4 color_handle_sel_vect;
  float4 color_handle_sel_align;
  float4 color_handle_sel_autoclamp;
  float4 color_nurb_uline;
  float4 color_nurb_vline;
  float4 color_nurb_sel_uline;
  float4 color_nurb_sel_vline;
  float4 color_active_spline;

  float4 color_bone_pose;
  float4 color_bone_pose_active;
  float4 color_bone_pose_active_unsel;
  float4 color_bone_pose_constraint;
  float4 color_bone_pose_ik;
  float4 color_bone_pose_spline_ik;
  float4 color_bone_pose_no_target;
  float4 color_bone_solid;
  float4 color_bone_locked;
  float4 color_bone_active;
  float4 color_bone_active_unsel;
  float4 color_bone_select;
  float4 color_bone_ik_line;
  float4 color_bone_ik_line_no_target;
  float4 color_bone_ik_line_spline;

  float4 color_text;
  float4 color_text_hi;

  float4 color_bundle_solid;

  float4 color_mball_radius;
  float4 color_mball_radius_select;
  float4 color_mball_stiffness;
  float4 color_mball_stiffness_select;

  float4 color_current_frame;

  float4 color_grid;
  float4 color_grid_emphasis;
  float4 color_grid_axis_x;
  float4 color_grid_axis_y;
  float4 color_grid_axis_z;

  float4 color_face_back;
  float4 color_face_front;

  float4 color_uv_shadow;
};
BLI_STATIC_ASSERT_ALIGN(ThemeColorData, 16)

struct ExtraInstanceData {
  float4 color_;
  float4x4 object_to_world_;

#if !defined(GPU_SHADER) && defined(__cplusplus)
  ExtraInstanceData(const float4x4 &object_to_world, float4 &color, float draw_size)
  {
    this->color_ = color;
    this->object_to_world_ = object_to_world;
    this->object_to_world_[3][3] = draw_size;
  };
#endif
};
BLI_STATIC_ASSERT_ALIGN(ExtraInstanceData, 16)

#ifndef GPU_SHADER
#  ifdef __cplusplus
}
#  endif
#endif
