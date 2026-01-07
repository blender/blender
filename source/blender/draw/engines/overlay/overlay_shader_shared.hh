/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GPU_shader_shared_utils.hh"

#ifndef GPU_SHADER
#  include "DNA_action_types.h"
#  include "DNA_view3d_types.h"
#endif

enum [[host_shared]] OVERLAY_BackgroundType : uint32_t {
  BG_SOLID = 0u,
  BG_GRADIENT = 1u,
  BG_CHECKER = 2u,
  BG_RADIAL = 3u,
  BG_SOLID_CHECKER = 4u,
  BG_MASK = 5u,
};

enum [[host_shared]] OVERLAY_UVLineStyle : uint32_t {
  OVERLAY_UV_LINE_STYLE_OUTLINE,
  OVERLAY_UV_LINE_STYLE_DASH,
  OVERLAY_UV_LINE_STYLE_BLACK,
  OVERLAY_UV_LINE_STYLE_WHITE,
  OVERLAY_UV_LINE_STYLE_SHADOW,
};

enum [[host_shared]] OVERLAY_GridBits : uint32_t {
  SHOW_GRID = (1u << 0u),
  SHOW_AXES = (1u << 1u),

  /* Axis * is shown if `SHOW_AXES` is set. */
  AXIS_X = (1u << 2u),
  AXIS_Y = (1u << 3u),
  AXIS_Z = (1u << 4u),

  /* Grid is placed on * plane if `SHOW_GRID` is set. */
  PLANE_XY = (1u << 5u),
  PLANE_XZ = (1u << 6u),
  PLANE_YZ = (1u << 7u),

  GRID_SIMA = (1u << 8u),       /* Grid is in SpaceImage view. */
  GRID_OVER_IMAGE = (1u << 9u), /* Grid is shown in front of SpaceImage, not behind. */
  GRID_CAMERA = (1u << 10u)     /* Grid is shown in selected camera. */
};
#ifndef GPU_SHADER
ENUM_OPERATORS(OVERLAY_GridBits)
#endif

enum [[host_shared]] VertexClass : uint32_t {
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
ENUM_OPERATORS(VertexClass)
#endif

enum [[host_shared]] StickBoneFlag : uint32_t {
  COL_WIRE = (1u << 0u),
  COL_HEAD = (1u << 1u),
  COL_TAIL = (1u << 2u),
  COL_BONE = (1u << 3u),
  POS_HEAD = (1u << 4u),
  POS_TAIL = (1u << 5u),
  POS_BONE = (1u << 6u),
};
#ifndef GPU_SHADER
ENUM_OPERATORS(StickBoneFlag)
#endif

/* TODO(fclem): Convert into enum. */
/* See: 'draw_cache_impl.hh' for matching includes. */
#define VERT_GPENCIL_BEZT_HANDLE (1u << 30)
/* data[0] (1st byte flags) */
#define FACE_ACTIVE (1u << 0)
#define FACE_SELECTED (1u << 1)
#define FACE_FREESTYLE (1u << 2)
#define VERT_UV_SELECT (1u << 3)
#define VERT_UV_PINNED (1u << 4)
#define EDGE_UV_SELECT (1u << 5)
#define FACE_UV_ACTIVE (1u << 6)
#define FACE_UV_SELECT (1u << 7)
/* data[1] (2nd byte flags) */
#define VERT_ACTIVE (1u << 0)
#define VERT_SELECTED (1u << 1)
#define VERT_SELECTED_BEZT_HANDLE (1u << 2)
#define EDGE_ACTIVE (1u << 3)
#define EDGE_SELECTED (1u << 4)
#define EDGE_SEAM (1u << 5)
#define EDGE_SHARP (1u << 6)
#define EDGE_FREESTYLE (1u << 7)

static inline uint outline_id_pack(uint outline_id, uint object_id)
{
  /* Replace top 2 bits (of the 16bit output) by outline_id.
   * This leaves 16K different IDs to create outlines between objects.
   * 18 = (32 - (16 - 2)) */
  return (outline_id << 14u) | ((object_id << 18u) >> 18u);
}

/** Keep in sync with `SI_GRID_STEPS_LEN` in `DNA_space_types.h`. */
#define OVERLAY_GRID_STEPS_LEN 8
/** Hardcoded grid steps drawn at a time. */
#define OVERLAY_GRID_STEPS_DRAW 3
/** Hardcoded max iterations of grid draw for alpha fade. */
#define OVERLAY_GRID_ITER_LEN 4

/* Due to the encoding clamping the passed in floats, the wire width needs to be scaled down. */
#define WIRE_WIDTH_COMPRESSION 16.0f

struct [[host_shared]] OVERLAY_GridData {
  /* Per level step size, based on selected units/subdivision. */
  float4 steps[OVERLAY_GRID_STEPS_LEN]; /* float3 array padded to float4 (std140). */
  /* XY/YZ/XZ camera offset of grid. */
  float2 offset;
  /* Clipping rectangle for UV/Image editor. */
  float2 clip_rect;
  /* Fractional grid-level, dependent on current camera position/distance/zoom. */
  float level;
  /* Per-level line count. */
  uint num_lines;
  uint _pad0;
  uint _pad1;
};

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
#  define CURVE_HANDLE_SELECTED blender::CURVE_HANDLE_SELECTED
#  define CURVE_HANDLE_ALL blender::CURVE_HANDLE_ALL
#  define MOTIONPATH_VERT_SEL blender::MOTIONPATH_VERT_SEL
#  define MOTIONPATH_VERT_KEY blender::MOTIONPATH_VERT_KEY
/* TODO(fclem): Find a better way to share enums/defines from DNA files with GLSL. */
BLI_STATIC_ASSERT(CURVE_HANDLE_SELECTED == 0u, "Ensure value is sync");
BLI_STATIC_ASSERT(CURVE_HANDLE_ALL == 1u, "Ensure value is sync");
BLI_STATIC_ASSERT(MOTIONPATH_VERT_SEL == (1u << 0), "Ensure value is sync");
BLI_STATIC_ASSERT(MOTIONPATH_VERT_KEY == (1u << 1), "Ensure value is sync");
#endif

/* All colors in this struct are converted to display linear RGB color-space. */
struct [[host_shared]] ThemeColors {
  /* UBOs data needs to be 16 byte aligned (size of float4) */
  float4 wire;
  float4 wire_edit;
  float4 active_object; /* "active" is reserved keyword in GLSL. */
  float4 object_select; /* "select" is defined as a macro for GLSL. */
  float4 library_select;
  float4 library;
  float4 transform;
  float4 light;
  float4 speaker;
  float4 camera;
  float4 camera_path;
  float4 empty;
  float4 vert; /* "vertex" is reserved keyword in MSL. */
  float4 vert_select;
  float4 vert_unreferenced;
  float4 vert_missing_data;
  float4 edit_mesh_active;
  float4 edge_select;      /* Stands for edge selection, not edge select mode. */
  float4 edge_mode_select; /* Stands for edge mode selection. */
  float4 edge_seam;
  float4 edge_sharp;
  float4 edge_crease;
  float4 edge_bweight;
  float4 edge_face_select;
  float4 edge_freestyle;
  float4 face;
  float4 face_select;      /* Stands for face selection, not face select mode. */
  float4 face_mode_select; /* Stands for face mode selection. */
  float4 face_retopology;
  float4 face_freestyle;
  float4 gpencil_wire_edit;
  float4 gpencil_vertex;
  float4 gpencil_vertex_select;
  float4 normal;
  float4 vnormal;
  float4 lnormal;
  float4 facedot;
  float4 skinroot;

  float4 deselect;
  float4 outline;
  float4 light_no_alpha;

  float4 background;
  float4 background_gradient;
  float4 checker_primary;
  float4 checker_secondary;
  float4 clipping_border;
  float4 edit_mesh_middle;

  float4 handle_free;
  float4 handle_auto;
  float4 handle_vect;
  float4 handle_align;
  float4 handle_autoclamp;
  float4 handle_sel_free;
  float4 handle_sel_auto;
  float4 handle_sel_vect;
  float4 handle_sel_align;
  float4 handle_sel_autoclamp;
  float4 nurb_uline;
  float4 nurb_vline;
  float4 nurb_sel_uline;
  float4 nurb_sel_vline;

  float4 bone_pose;
  float4 bone_pose_active;
  float4 bone_pose_active_unsel;
  float4 bone_pose_constraint;
  float4 bone_pose_ik;
  float4 bone_pose_spline_ik;
  float4 bone_pose_no_target;
  float4 bone_solid;
  float4 bone_locked;
  float4 bone_active;
  float4 bone_active_unsel;
  float4 bone_select;
  float4 bone_ik_line;
  float4 bone_ik_line_no_target;
  float4 bone_ik_line_spline;

  float4 text;
  float4 text_hi;

  float4 bundle_solid;

  float4 mball_radius;
  float4 mball_radius_select;
  float4 mball_stiffness;
  float4 mball_stiffness_select;

  float4 current_frame;
  float4 before_frame;
  float4 after_frame;

  float4 grid;
  float4 grid_emphasis;
  float4 grid_axis_x;
  float4 grid_axis_y;
  float4 grid_axis_z;

  float4 face_back;
  float4 face_front;

  float4 uv_shadow;
};

/* All values in this struct are premultiplied by U.pixelsize. */
struct [[host_shared]] ThemeSizes {
  float pixel; /* Equivalent to U.pixelsize. */

  float object_center;

  float light_center;
  float light_circle;
  float light_circle_shadow;

  float vert; /* "vertex" is reserved keyword in MSL. */
  float edge;
  float face_dot;

  float checker;
  float vertex_gpencil;
  float _pad1;
  float _pad2;
};

struct [[host_shared]] UniformData {
  struct ThemeColors colors;
  struct ThemeSizes sizes;

  /** Other global states. */

  float2 size_viewport;
  float2 size_viewport_inv;

  float fresnel_mix_edit;
  float pixel_fac;
  bool32_t backface_culling;
  float _pad1;
};

#ifdef GPU_SHADER
/* The uniform_buf mostly contains theme properties.
 * This alias has better semantic and shorter syntax. */
#  define theme uniform_buf
#endif

struct [[host_shared]] ExtraInstanceData {
  float4 color_;
  float4x4 object_to_world;

#ifndef GPU_SHADER
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

struct [[host_shared]] VertexData {
  float4 pos_;
  /* TODO: change to color_id. Idea expressed in #125894. */
  float4 color_;
};

/* Limited by expand_prim_len bit count. */
#define PARTICLE_SHAPE_CIRCLE_RESOLUTION 7

enum [[host_shared]] OVERLAY_ParticleShape : uint32_t {
  PART_SHAPE_AXIS = 1,
  PART_SHAPE_CIRCLE = 2,
  PART_SHAPE_CROSS = 3,
};

struct [[host_shared]] ParticlePointData {
  packed_float3 position;
  /* Can either be velocity or acceleration. */
  float value;
  /* Rotation encoded as quaternion. */
  float4 rotation;
};

struct [[host_shared]] BoneEnvelopeData {
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
        x_axis(x_axis, 0.0f) {};

  /* For bone outlines. */
  BoneEnvelopeData(float4 &head_sphere,
                   float4 &tail_sphere,
                   float4 &color_and_wire_width,
                   float3 &x_axis)
      : head_sphere(head_sphere),
        tail_sphere(tail_sphere),
        bone_color_and_wire_width(color_and_wire_width),
        x_axis(x_axis, 0.0f) {};

  /* For bone distance volumes. */
  BoneEnvelopeData(float4 &head_sphere, float4 &tail_sphere, float3 &x_axis)
      : head_sphere(head_sphere), tail_sphere(tail_sphere), x_axis(x_axis, 0.0f) {};
#endif
};

struct [[host_shared]] BoneStickData {
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
        tail_color(tail_color) {};
#endif
};

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
