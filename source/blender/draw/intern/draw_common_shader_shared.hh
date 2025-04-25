/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#ifndef GPU_SHADER
#  include "GPU_shader_shared_utils.hh"
#endif

/* Future Plan: These globals were once shared between multiple overlay engines. But now that they
 * have been merged into one engine, there is no reasons to keep these globals out of the overlay
 * engine. */

/* All colors in this struct are converted to display linear RGB colorspace. */
struct ThemeColors {
  /* UBOs data needs to be 16 byte aligned (size of float4) */
  float4 wire;
  float4 wire_edit;
  float4 active;
  float4 select;
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
  float4 active_spline;

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
BLI_STATIC_ASSERT_ALIGN(ThemeColors, 16)

/* All values in this struct are premultiplied by U.pixelsize. */
struct ThemeSizes {
  float pixel;
  float object_center;

  float light_center;
  float light_circle;
  float light_circle_shadow;

  float vert; /* "vertex" is reserved keyword in MSL. */
  float edge;
  float face_dot;

  float checker;
  float vertex_gpencil;
  float _pad1, _pad2;
};
BLI_STATIC_ASSERT_ALIGN(ThemeSizes, 16)

struct GlobalsUboStorage {
  ThemeColors colors;
  ThemeSizes sizes;

  /** Other global states. */

  float4 size_viewport; /* Packed as float4. */

  float fresnel_mix_edit;
  float pixel_fac;
  bool32_t backface_culling;
  float _pad1;
};
BLI_STATIC_ASSERT_ALIGN(GlobalsUboStorage, 16)

#ifdef GPU_SHADER
/* Keep compatibility_with old global scope syntax. */
/* TODO(@fclem) Mass rename and remove the camel case. */
#  define colorWire globalsBlock.colors.wire
#  define colorWireEdit globalsBlock.colors.wire_edit
#  define colorActive globalsBlock.colors.active
#  define colorSelect globalsBlock.colors.select
#  define colorLibrarySelect globalsBlock.colors.library_select
#  define colorLibrary globalsBlock.colors.library
#  define colorTransform globalsBlock.colors.transform
#  define colorLight globalsBlock.colors.light
#  define colorSpeaker globalsBlock.colors.speaker
#  define colorCamera globalsBlock.colors.camera
#  define colorCameraPath globalsBlock.colors.camera_path
#  define colorEmpty globalsBlock.colors.empty
#  define colorVertex globalsBlock.colors.vert
#  define colorVertexSelect globalsBlock.colors.vert_select
#  define colorVertexUnreferenced globalsBlock.colors.vert_unreferenced
#  define colorVertexMissingData globalsBlock.colors.vert_missing_data
#  define colorEditMeshActive globalsBlock.colors.edit_mesh_active
#  define colorEdgeSelect globalsBlock.colors.edge_select
#  define colorEdgeModeSelect globalsBlock.colors.edge_mode_select
#  define colorEdgeSeam globalsBlock.colors.edge_seam
#  define colorEdgeSharp globalsBlock.colors.edge_sharp
#  define colorEdgeCrease globalsBlock.colors.edge_crease
#  define colorEdgeBWeight globalsBlock.colors.edge_bweight
#  define colorEdgeFaceSelect globalsBlock.colors.edge_face_select
#  define colorEdgeFreestyle globalsBlock.colors.edge_freestyle
#  define colorFace globalsBlock.colors.face
#  define colorFaceSelect globalsBlock.colors.face_select
#  define colorFaceModeSelect globalsBlock.colors.face_mode_select
#  define colorFaceRetopology globalsBlock.colors.face_retopology
#  define colorFaceFreestyle globalsBlock.colors.face_freestyle
#  define colorGpencilVertex globalsBlock.colors.gpencil_vertex
#  define colorGpencilVertexSelect globalsBlock.colors.gpencil_vertex_select
#  define colorNormal globalsBlock.colors.normal
#  define colorVNormal globalsBlock.colors.vnormal
#  define colorLNormal globalsBlock.colors.lnormal
#  define colorFaceDot globalsBlock.colors.facedot
#  define colorSkinRoot globalsBlock.colors.skinroot
#  define colorDeselect globalsBlock.colors.deselect
#  define colorOutline globalsBlock.colors.outline
#  define colorLightNoAlpha globalsBlock.colors.light_no_alpha
#  define colorBackground globalsBlock.colors.background
#  define colorBackgroundGradient globalsBlock.colors.background_gradient
#  define colorCheckerPrimary globalsBlock.colors.checker_primary
#  define colorCheckerSecondary globalsBlock.colors.checker_secondary
#  define colorClippingBorder globalsBlock.colors.clipping_border
#  define colorEditMeshMiddle globalsBlock.colors.edit_mesh_middle
#  define colorHandleFree globalsBlock.colors.handle_free
#  define colorHandleAuto globalsBlock.colors.handle_auto
#  define colorHandleVect globalsBlock.colors.handle_vect
#  define colorHandleAlign globalsBlock.colors.handle_align
#  define colorHandleAutoclamp globalsBlock.colors.handle_autoclamp
#  define colorHandleSelFree globalsBlock.colors.handle_sel_free
#  define colorHandleSelAuto globalsBlock.colors.handle_sel_auto
#  define colorHandleSelVect globalsBlock.colors.handle_sel_vect
#  define colorHandleSelAlign globalsBlock.colors.handle_sel_align
#  define colorHandleSelAutoclamp globalsBlock.colors.handle_sel_autoclamp
#  define colorNurbUline globalsBlock.colors.nurb_uline
#  define colorNurbVline globalsBlock.colors.nurb_vline
#  define colorNurbSelUline globalsBlock.colors.nurb_sel_uline
#  define colorNurbSelVline globalsBlock.colors.nurb_sel_vline
#  define colorActiveSpline globalsBlock.colors.active_spline
#  define colorBonePose globalsBlock.colors.bone_pose
#  define colorBonePoseActive globalsBlock.colors.bone_pose_active
#  define colorBonePoseActiveUnsel globalsBlock.colors.bone_pose_active_unsel
#  define colorBonePoseConstraint globalsBlock.colors.bone_pose_constraint
#  define colorBonePoseIK globalsBlock.colors.bone_pose_ik
#  define colorBonePoseSplineIK globalsBlock.colors.bone_pose_spline_ik
#  define colorBonePoseTarget globalsBlock.colors.bone_pose_no_target
#  define colorBoneSolid globalsBlock.colors.bone_solid
#  define colorBoneLocked globalsBlock.colors.bone_locked
#  define colorBoneActive globalsBlock.colors.bone_active
#  define colorBoneActiveUnsel globalsBlock.colors.bone_active_unsel
#  define colorBoneSelect globalsBlock.colors.bone_select
#  define colorBoneIKLine globalsBlock.colors.bone_ik_line
#  define colorBoneIKLineNoTarget globalsBlock.colors.bone_ik_line_no_target
#  define colorBoneIKLineSpline globalsBlock.colors.bone_ik_line_spline
#  define colorText globalsBlock.colors.text
#  define colorTextHi globalsBlock.colors.text_hi
#  define colorBundleSolid globalsBlock.colors.bundle_solid
#  define colorMballRadius globalsBlock.colors.mball_radius
#  define colorMballRadiusSelect globalsBlock.colors.mball_radius_select
#  define colorMballStiffness globalsBlock.colors.mball_stiffness
#  define colorMballStiffnessSelect globalsBlock.colors.mball_stiffness_select
#  define colorCurrentFrame globalsBlock.colors.current_frame
#  define colorBeforeFrame globalsBlock.colors.before_frame
#  define colorAfterFrame globalsBlock.colors.after_frame
#  define colorGrid globalsBlock.colors.grid
#  define colorGridEmphasis globalsBlock.colors.grid_emphasis
#  define colorGridAxisX globalsBlock.colors.grid_axis_x
#  define colorGridAxisY globalsBlock.colors.grid_axis_y
#  define colorGridAxisZ globalsBlock.colors.grid_axis_z
#  define colorFaceBack globalsBlock.colors.face_back
#  define colorFaceFront globalsBlock.colors.face_front
#  define colorUVShadow globalsBlock.colors.uv_shadow
#  define sizeViewport float2(globalsBlock.size_viewport.xy)
#  define sizeViewportInv float2(globalsBlock.size_viewport.zw)
#  define sizePixel globalsBlock.sizes.pixel
#  define pixelFac globalsBlock.pixel_fac
#  define sizeObjectCenter globalsBlock.sizes.object_center
#  define sizeLightCenter globalsBlock.sizes.light_center
#  define sizeLightCircle globalsBlock.sizes.light_circle
#  define sizeLightCircleShadow globalsBlock.sizes.light_circle_shadow
#  define sizeVertex globalsBlock.sizes.vert
#  define sizeEdge globalsBlock.sizes.edge
#  define sizeFaceDot globalsBlock.sizes.face_dot
#  define sizeChecker globalsBlock.sizes.checker
#  define sizeVertexGpencil globalsBlock.sizes.vertex_gpencil
#  define fresnelMixEdit globalsBlock.fresnel_mix_edit
#endif

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
/* data[1] (2st byte flags) */
#define VERT_ACTIVE (1u << 0)
#define VERT_SELECTED (1u << 1)
#define VERT_SELECTED_BEZT_HANDLE (1u << 2)
#define EDGE_ACTIVE (1u << 3)
#define EDGE_SELECTED (1u << 4)
#define EDGE_SEAM (1u << 5)
#define EDGE_SHARP (1u << 6)
#define EDGE_FREESTYLE (1u << 7)

#define COMMON_GLOBALS_LIB
