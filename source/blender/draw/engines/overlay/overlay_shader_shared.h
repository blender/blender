/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#if !defined(GPU_SHADER) && !defined(GLSL_CPP_STUBS)
#  pragma once

#  include "GPU_shader_shared_utils.hh"

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
typedef struct VertexData VertexData;
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

/* Due to the encoding clamping the passed in floats, the wire width needs to be scaled down. */
#define WIRE_WIDTH_COMPRESSION 16.0

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
#  define EDIT_CURVES_ACTIVE_HANDLE (1u << 2)
#  define EDIT_CURVES_BEZIER_KNOT (1u << 3)
#  define EDIT_CURVES_HANDLE_TYPES_SHIFT (4u)
/* Keep the same values as in `draw_cache_imp_curve.c` */
#  define ACTIVE_NURB (1u << 2)
#  define BEZIER_HANDLE (1u << 3)
#  define EVEN_U_BIT (1u << 4)
#  define COLOR_SHIFT 5u

/* Keep the same value in `handle_display` in `DNA_view3d_types.h` */
#  define CURVE_HANDLE_SELECTED 0u
#  define CURVE_HANDLE_ALL 1u
#  define CURVE_HANDLE_NONE 2u

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
  float4x4 object_to_world;

#if !defined(GPU_SHADER) && defined(__cplusplus)
  ExtraInstanceData(const float4x4 &object_to_world, const float4 &color, float draw_size)
  {
    this->color_ = color;
    this->object_to_world = object_to_world;
    this->object_to_world[3][3] = draw_size;
  };

  ExtraInstanceData with_color(const float4 &color) const
  {
    ExtraInstanceData copy = *this;
    copy.color_ = color;
    return copy;
  }

  /* For degrees of freedom. */
  ExtraInstanceData(const float4x4 &object_to_world,
                    const float4 &color,
                    float angle_min_x,
                    float angle_min_z,
                    float angle_max_x,
                    float angle_max_z)
  {
    this->color_ = color;
    this->object_to_world = object_to_world;
    this->object_to_world[0][3] = angle_min_x;
    this->object_to_world[1][3] = angle_min_z;
    this->object_to_world[2][3] = angle_max_x;
    this->object_to_world[3][3] = angle_max_z;
  };
#endif
};
BLI_STATIC_ASSERT_ALIGN(ExtraInstanceData, 16)

struct VertexData {
  float4 pos_;
  /* TODO: change to color_id. Idea expressed in #125894. */
  float4 color_;
};
BLI_STATIC_ASSERT_ALIGN(VertexData, 16)

/* Limited by expand_prim_len bit count. */
#define PARTICLE_SHAPE_CIRCLE_RESOLUTION 7

/* TODO(fclem): This should be a enum, but it breaks compilation on Metal for some reason. */
#define PART_SHAPE_AXIS 1
#define PART_SHAPE_CIRCLE 2
#define PART_SHAPE_CROSS 3

struct ParticlePointData {
  packed_float3 position;
  /* Can either be velocity or acceleration. */
  float value;
  /* Rotation encoded as quaternion. */
  float4 rotation;
};
BLI_STATIC_ASSERT_ALIGN(ParticlePointData, 16)

struct BoneEnvelopeData {
  float4 head_sphere;
  float4 tail_sphere;
  /* TODO(pragma37): wire width is never used in the shader. */
  float4 bone_color_and_wire_width;
  float4 state_color;
  float4 x_axis;

#ifndef GPU_SHADER
  BoneEnvelopeData() = default;

  /* For bone fills. */
  BoneEnvelopeData(float4 &head_sphere,
                   float4 &tail_sphere,
                   float3 &bone_color,
                   float3 &state_color,
                   float3 &x_axis)
      : head_sphere(head_sphere),
        tail_sphere(tail_sphere),
        bone_color_and_wire_width(bone_color, 0.0f),
        state_color(state_color, 0.0f),
        x_axis(x_axis, 0.0f){};

  /* For bone outlines. */
  BoneEnvelopeData(float4 &head_sphere,
                   float4 &tail_sphere,
                   float4 &color_and_wire_width,
                   float3 &x_axis)
      : head_sphere(head_sphere),
        tail_sphere(tail_sphere),
        bone_color_and_wire_width(color_and_wire_width),
        x_axis(x_axis, 0.0f){};

  /* For bone distance volumes. */
  BoneEnvelopeData(float4 &head_sphere, float4 &tail_sphere, float3 &x_axis)
      : head_sphere(head_sphere), tail_sphere(tail_sphere), x_axis(x_axis, 0.0f){};
#endif
};
BLI_STATIC_ASSERT_ALIGN(BoneEnvelopeData, 16)

/* Keep in sync with armature_stick_vert.glsl. */
enum StickBoneFlag {
  COL_WIRE = (1u << 0u),
  COL_HEAD = (1u << 1u),
  COL_TAIL = (1u << 2u),
  COL_BONE = (1u << 3u),
  POS_HEAD = (1u << 4u),
  POS_TAIL = (1u << 5u),
  POS_BONE = (1u << 6u),
};

struct BoneStickData {
  float4 bone_start;
  float4 bone_end;
  float4 wire_color;
  float4 bone_color;
  float4 head_color;
  float4 tail_color;

#ifndef GPU_SHADER
  BoneStickData() = default;

  /* For bone fills. */
  BoneStickData(const float3 &bone_start,
                const float3 &bone_end,
                const float4 &wire_color,
                const float4 &bone_color,
                const float4 &head_color,
                const float4 &tail_color)
      : bone_start(float3(bone_start), 0.0f),
        bone_end(float3(bone_end), 0.0f),
        wire_color(wire_color),
        bone_color(bone_color),
        head_color(head_color),
        tail_color(tail_color){};
#endif
};
BLI_STATIC_ASSERT_ALIGN(BoneStickData, 16)

#ifndef GPU_SHADER
#  ifdef __cplusplus
}
#  endif
#endif
