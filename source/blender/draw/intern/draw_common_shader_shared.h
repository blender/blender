/* SPDX-FileCopyrightText: 2022 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#ifndef GPU_SHADER
#  include "GPU_shader_shared_utils.h"

typedef struct GlobalsUboStorage GlobalsUboStorage;
#endif

/* Future Plan: These globals were once shared between multiple overlay engines. But now that they
 * have been merged into one engine, there is no reasons to keep these globals out of the overlay
 * engine. */

#define UBO_FIRST_COLOR color_wire
#define UBO_LAST_COLOR color_uv_shadow

/* Used as ubo but colors can be directly referenced as well */
/* \note Also keep all color as vec4 and between #UBO_FIRST_COLOR and #UBO_LAST_COLOR. */
struct GlobalsUboStorage {
  /* UBOs data needs to be 16 byte aligned (size of vec4) */
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
  float4 color_edge_select;
  float4 color_edge_seam;
  float4 color_edge_sharp;
  float4 color_edge_crease;
  float4 color_edge_bweight;
  float4 color_edge_face_select;
  float4 color_edge_freestyle;
  float4 color_face;
  float4 color_face_select;
  float4 color_face_retopology;
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
  float4 color_bone_pose_target;
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

  /* NOTE: Put all color before #UBO_LAST_COLOR. */
  float4 size_viewport; /* Packed as vec4. */

  /* Pack individual float at the end of the buffer to avoid alignment errors */
  float size_pixel, pixel_fac;
  float size_object_center, size_light_center, size_light_circle, size_light_circle_shadow;
  float size_vertex, size_edge, size_edge_fix, size_face_dot;
  float size_checker;
  float size_vertex_gpencil;
};
BLI_STATIC_ASSERT_ALIGN(GlobalsUboStorage, 16)

#ifdef GPU_SHADER
/* Keep compatibility_with old global scope syntax. */
/* TODO(@fclem) Mass rename and remove the camel case. */
#  define colorWire globalsBlock.color_wire
#  define colorWireEdit globalsBlock.color_wire_edit
#  define colorActive globalsBlock.color_active
#  define colorSelect globalsBlock.color_select
#  define colorLibrarySelect globalsBlock.color_library_select
#  define colorLibrary globalsBlock.color_library
#  define colorTransform globalsBlock.color_transform
#  define colorLight globalsBlock.color_light
#  define colorSpeaker globalsBlock.color_speaker
#  define colorCamera globalsBlock.color_camera
#  define colorCameraPath globalsBlock.color_camera_path
#  define colorEmpty globalsBlock.color_empty
#  define colorVertex globalsBlock.color_vertex
#  define colorVertexSelect globalsBlock.color_vertex_select
#  define colorVertexUnreferenced globalsBlock.color_vertex_unreferenced
#  define colorVertexMissingData globalsBlock.color_vertex_missing_data
#  define colorEditMeshActive globalsBlock.color_edit_mesh_active
#  define colorEdgeSelect globalsBlock.color_edge_select
#  define colorEdgeSeam globalsBlock.color_edge_seam
#  define colorEdgeSharp globalsBlock.color_edge_sharp
#  define colorEdgeCrease globalsBlock.color_edge_crease
#  define colorEdgeBWeight globalsBlock.color_edge_bweight
#  define colorEdgeFaceSelect globalsBlock.color_edge_face_select
#  define colorEdgeFreestyle globalsBlock.color_edge_freestyle
#  define colorFace globalsBlock.color_face
#  define colorFaceSelect globalsBlock.color_face_select
#  define colorFaceRetopology globalsBlock.color_face_retopology
#  define colorFaceFreestyle globalsBlock.color_face_freestyle
#  define colorGpencilVertex globalsBlock.color_gpencil_vertex
#  define colorGpencilVertexSelect globalsBlock.color_gpencil_vertex_select
#  define colorNormal globalsBlock.color_normal
#  define colorVNormal globalsBlock.color_vnormal
#  define colorLNormal globalsBlock.color_lnormal
#  define colorFaceDot globalsBlock.color_facedot
#  define colorSkinRoot globalsBlock.color_skinroot
#  define colorDeselect globalsBlock.color_deselect
#  define colorOutline globalsBlock.color_outline
#  define colorLightNoAlpha globalsBlock.color_light_no_alpha
#  define colorBackground globalsBlock.color_background
#  define colorBackgroundGradient globalsBlock.color_background_gradient
#  define colorCheckerPrimary globalsBlock.color_checker_primary
#  define colorCheckerSecondary globalsBlock.color_checker_secondary
#  define colorClippingBorder globalsBlock.color_clipping_border
#  define colorEditMeshMiddle globalsBlock.color_edit_mesh_middle
#  define colorHandleFree globalsBlock.color_handle_free
#  define colorHandleAuto globalsBlock.color_handle_auto
#  define colorHandleVect globalsBlock.color_handle_vect
#  define colorHandleAlign globalsBlock.color_handle_align
#  define colorHandleAutoclamp globalsBlock.color_handle_autoclamp
#  define colorHandleSelFree globalsBlock.color_handle_sel_free
#  define colorHandleSelAuto globalsBlock.color_handle_sel_auto
#  define colorHandleSelVect globalsBlock.color_handle_sel_vect
#  define colorHandleSelAlign globalsBlock.color_handle_sel_align
#  define colorHandleSelAutoclamp globalsBlock.color_handle_sel_autoclamp
#  define colorNurbUline globalsBlock.color_nurb_uline
#  define colorNurbVline globalsBlock.color_nurb_vline
#  define colorNurbSelUline globalsBlock.color_nurb_sel_uline
#  define colorNurbSelVline globalsBlock.color_nurb_sel_vline
#  define colorActiveSpline globalsBlock.color_active_spline
#  define colorBonePose globalsBlock.color_bone_pose
#  define colorBonePoseActive globalsBlock.color_bone_pose_active
#  define colorBonePoseActiveUnsel globalsBlock.color_bone_pose_active_unsel
#  define colorBonePoseConstraint globalsBlock.color_bone_pose_constraint
#  define colorBonePoseIK globalsBlock.color_bone_pose_ik
#  define colorBonePoseSplineIK globalsBlock.color_bone_pose_spline_ik
#  define colorBonePoseTarget globalsBlock.color_bone_pose_target
#  define colorBoneSolid globalsBlock.color_bone_solid
#  define colorBoneLocked globalsBlock.color_bone_locked
#  define colorBoneActive globalsBlock.color_bone_active
#  define colorBoneActiveUnsel globalsBlock.color_bone_active_unsel
#  define colorBoneSelect globalsBlock.color_bone_select
#  define colorBoneIKLine globalsBlock.color_bone_ik_line
#  define colorBoneIKLineNoTarget globalsBlock.color_bone_ik_line_no_target
#  define colorBoneIKLineSpline globalsBlock.color_bone_ik_line_spline
#  define colorText globalsBlock.color_text
#  define colorTextHi globalsBlock.color_text_hi
#  define colorBundleSolid globalsBlock.color_bundle_solid
#  define colorMballRadius globalsBlock.color_mball_radius
#  define colorMballRadiusSelect globalsBlock.color_mball_radius_select
#  define colorMballStiffness globalsBlock.color_mball_stiffness
#  define colorMballStiffnessSelect globalsBlock.color_mball_stiffness_select
#  define colorCurrentFrame globalsBlock.color_current_frame
#  define colorGrid globalsBlock.color_grid
#  define colorGridEmphasis globalsBlock.color_grid_emphasis
#  define colorGridAxisX globalsBlock.color_grid_axis_x
#  define colorGridAxisY globalsBlock.color_grid_axis_y
#  define colorGridAxisZ globalsBlock.color_grid_axis_z
#  define colorFaceBack globalsBlock.color_face_back
#  define colorFaceFront globalsBlock.color_face_front
#  define colorUVShadow globalsBlock.color_uv_shadow
#  define sizeViewport globalsBlock.size_viewport.xy
#  define sizeViewportInv globalsBlock.size_viewport.zw
#  define sizePixel globalsBlock.size_pixel
#  define pixelFac globalsBlock.pixel_fac
#  define sizeObjectCenter globalsBlock.size_object_center
#  define sizeLightCenter globalsBlock.size_light_center
#  define sizeLightCircle globalsBlock.size_light_circle
#  define sizeLightCircleShadow globalsBlock.size_light_circle_shadow
#  define sizeVertex globalsBlock.size_vertex
#  define sizeEdge globalsBlock.size_edge
#  define sizeEdgeFix globalsBlock.size_edge_fix
#  define sizeFaceDot globalsBlock.size_face_dot
#  define sizeChecker globalsBlock.size_checker
#  define sizeVertexGpencil globalsBlock.size_vertex_gpencil
#endif

/* See: 'draw_cache_impl.h' for matching includes. */
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
