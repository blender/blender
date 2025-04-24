/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#if !defined(GPU_SHADER) && !defined(GLSL_CPP_STUBS)
#  pragma once

#  include "GPU_shader_shared_utils.hh"

#  include "DNA_action_types.h"
#  include "DNA_view3d_types.h"
#endif

enum OVERLAY_BackgroundType : uint32_t {
  BG_SOLID = 0u,
  BG_GRADIENT = 1u,
  BG_CHECKER = 2u,
  BG_RADIAL = 3u,
  BG_SOLID_CHECKER = 4u,
  BG_MASK = 5u,
};

enum OVERLAY_UVLineStyle : uint32_t {
  OVERLAY_UV_LINE_STYLE_OUTLINE = 0u,
  OVERLAY_UV_LINE_STYLE_DASH = 1u,
  OVERLAY_UV_LINE_STYLE_BLACK = 2u,
  OVERLAY_UV_LINE_STYLE_WHITE = 3u,
  OVERLAY_UV_LINE_STYLE_SHADOW = 4u,
};

enum OVERLAY_GridBits : uint32_t {
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

enum VertexClass : uint32_t {
  VCLASS_NONE = 0,

  VCLASS_LIGHT_AREA_SHAPE = 1 << 0,
  VCLASS_LIGHT_SPOT_SHAPE = 1 << 1,
  VCLASS_LIGHT_SPOT_BLEND = 1 << 2,
  VCLASS_LIGHT_SPOT_CONE = 1 << 3,
  VCLASS_LIGHT_DIST = 1 << 4,

  VCLASS_CAMERA_FRAME = 1 << 5,
  VCLASS_CAMERA_DIST = 1 << 6,
  VCLASS_CAMERA_VOLUME = 1 << 7,

  VCLASS_SCREENSPACE = 1 << 8,
  VCLASS_SCREENALIGNED = 1 << 9,

  VCLASS_EMPTY_SCALED = 1 << 10,
  VCLASS_EMPTY_AXES = 1 << 11,
  VCLASS_EMPTY_AXES_NAME = 1 << 12,
  VCLASS_EMPTY_AXES_SHADOW = 1 << 13,
  VCLASS_EMPTY_SIZE = 1 << 14,
};
#ifndef GPU_SHADER
ENUM_OPERATORS(VertexClass, VCLASS_EMPTY_SIZE)
#endif

enum StickBoneFlag {
  COL_WIRE = (1u << 0u),
  COL_HEAD = (1u << 1u),
  COL_TAIL = (1u << 2u),
  COL_BONE = (1u << 3u),
  POS_HEAD = (1u << 4u),
  POS_TAIL = (1u << 5u),
  POS_BONE = (1u << 6u),
};
#ifndef GPU_SHADER
ENUM_OPERATORS(StickBoneFlag, POS_BONE)
#endif

static inline uint outline_id_pack(uint outline_id, uint object_id)
{
  /* Replace top 2 bits (of the 16bit output) by outline_id.
   * This leaves 16K different IDs to create outlines between objects.
   * 18 = (32 - (16 - 2)) */
  return (outline_id << 14u) | ((object_id << 18u) >> 18u);
}

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

#if !defined(GPU_SHADER)
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

enum OVERLAY_ParticleShape : uint32_t {
  PART_SHAPE_AXIS = 1,
  PART_SHAPE_CIRCLE = 2,
  PART_SHAPE_CROSS = 3,
};

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

/**
 * We want to know how much of a pixel is covered by a line.
 * Here, we imagine the square pixel is a circle with the same area and try to find the
 * intersection area. The overlap area is a circular segment.
 * https://en.wikipedia.org/wiki/Circular_segment The formula for the area uses inverse trig
 * function and is quite complex. Instead, we approximate it by using the smoothstep function and
 * a 1.05f factor to the disc radius.
 *
 * For an alternate approach, see:
 * https://developer.nvidia.com/gpugems/gpugems2/part-iii-high-quality-rendering/chapter-22-fast-prefiltered-lines
 */
#define M_1_SQRTPI 0.5641895835477563f /* `1/sqrt(pi)`. */
#define DISC_RADIUS (M_1_SQRTPI * 1.05f)
#define LINE_SMOOTH_START (0.5f - DISC_RADIUS)
#define LINE_SMOOTH_END (0.5f + DISC_RADIUS)
/* Returns 0 before LINE_SMOOTH_START and 1 after LINE_SMOOTH_END. */
#define LINE_STEP(dist) smoothstep(LINE_SMOOTH_START, LINE_SMOOTH_END, dist)
