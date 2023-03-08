/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation. */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DRW_render.h"

#include "overlay_shader_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __APPLE__
#  define USE_GEOM_SHADER_WORKAROUND 1
#else
#  define USE_GEOM_SHADER_WORKAROUND 0
#endif

/* Needed for eSpaceImage_UVDT_Stretch and eMaskOverlayMode */
#include "DNA_mask_types.h"
#include "DNA_space_types.h"
/* Forward declarations */
struct ImBuf;

typedef struct OVERLAY_FramebufferList {
  struct GPUFrameBuffer *overlay_default_fb;
  struct GPUFrameBuffer *overlay_line_fb;
  struct GPUFrameBuffer *overlay_color_only_fb;
  struct GPUFrameBuffer *overlay_in_front_fb;
  struct GPUFrameBuffer *overlay_line_in_front_fb;
  struct GPUFrameBuffer *outlines_prepass_fb;
  struct GPUFrameBuffer *outlines_resolve_fb;
} OVERLAY_FramebufferList;

typedef struct OVERLAY_TextureList {
  struct GPUTexture *temp_depth_tx;
  struct GPUTexture *dummy_depth_tx;
  struct GPUTexture *outlines_id_tx;
  struct GPUTexture *overlay_color_tx;
  struct GPUTexture *overlay_line_tx;
} OVERLAY_TextureList;

#define NOT_IN_FRONT 0
#define IN_FRONT 1

typedef enum OVERLAY_UVLineStyle {
  OVERLAY_UV_LINE_STYLE_OUTLINE = 0,
  OVERLAY_UV_LINE_STYLE_DASH = 1,
  OVERLAY_UV_LINE_STYLE_BLACK = 2,
  OVERLAY_UV_LINE_STYLE_WHITE = 3,
  OVERLAY_UV_LINE_STYLE_SHADOW = 4,
} OVERLAY_UVLineStyle;

typedef struct OVERLAY_PassList {
  DRWPass *antialiasing_ps;
  DRWPass *armature_ps[2];
  DRWPass *armature_bone_select_ps;
  DRWPass *armature_transp_ps[2];
  DRWPass *attribute_ps;
  DRWPass *background_ps;
  DRWPass *clipping_frustum_ps;
  DRWPass *edit_curve_wire_ps[2];
  DRWPass *edit_curve_handle_ps;
  DRWPass *edit_gpencil_ps;
  DRWPass *edit_gpencil_gizmos_ps;
  DRWPass *edit_gpencil_curve_ps;
  DRWPass *edit_lattice_ps;
  DRWPass *edit_mesh_depth_ps[2];
  DRWPass *edit_mesh_verts_ps[2];
  DRWPass *edit_mesh_edges_ps[2];
  DRWPass *edit_mesh_faces_ps[2];
  DRWPass *edit_mesh_faces_cage_ps[2];
  DRWPass *edit_curves_points_ps[2];
  DRWPass *edit_curves_lines_ps[2];
  DRWPass *edit_mesh_analysis_ps;
  DRWPass *edit_mesh_normals_ps;
  DRWPass *edit_particle_ps;
  DRWPass *edit_text_cursor_ps;
  DRWPass *edit_text_selection_ps;
  DRWPass *edit_text_highlight_ps;
  DRWPass *edit_text_wire_ps[2];
  DRWPass *edit_uv_edges_ps;
  DRWPass *edit_uv_verts_ps;
  DRWPass *edit_uv_faces_ps;
  DRWPass *edit_uv_stretching_ps;
  DRWPass *edit_uv_tiled_image_borders_ps;
  DRWPass *edit_uv_stencil_ps;
  DRWPass *edit_uv_mask_ps;
  DRWPass *extra_ps[2];
  DRWPass *extra_blend_ps;
  DRWPass *extra_centers_ps;
  DRWPass *extra_grid_ps;
  DRWPass *gpencil_canvas_ps;
  DRWPass *facing_ps[2];
  DRWPass *fade_ps[2];
  DRWPass *mode_transfer_ps[2];
  DRWPass *grid_ps;
  DRWPass *image_background_ps;
  DRWPass *image_background_scene_ps;
  DRWPass *image_empties_ps;
  DRWPass *image_empties_back_ps;
  DRWPass *image_empties_blend_ps;
  DRWPass *image_empties_front_ps;
  DRWPass *image_foreground_ps;
  DRWPass *image_foreground_scene_ps;
  DRWPass *metaball_ps[2];
  DRWPass *motion_paths_ps;
  DRWPass *outlines_prepass_ps;
  DRWPass *outlines_detect_ps;
  DRWPass *outlines_resolve_ps;
  DRWPass *paint_color_ps;
  DRWPass *paint_depth_ps;
  DRWPass *paint_overlay_ps;
  DRWPass *particle_ps;
  DRWPass *pointcloud_ps;
  DRWPass *sculpt_mask_ps;
  DRWPass *sculpt_curves_selection_ps;
  DRWPass *sculpt_curves_cage_ps;
  DRWPass *volume_ps;
  DRWPass *wireframe_ps;
  DRWPass *wireframe_xray_ps;
  DRWPass *xray_fade_ps;
} OVERLAY_PassList;

/* Data used by GLSL shader. */
typedef struct OVERLAY_ShadingData {
  /** Wireframe */
  float wire_step_param;
  float wire_opacity;
  /** Edit Curve */
  float edit_curve_normal_length;
  /** Edit Mesh */
  int data_mask[4];
} OVERLAY_ShadingData;

typedef struct OVERLAY_ExtraCallBuffers {
  DRWCallBuffer *camera_frame;
  DRWCallBuffer *camera_tria[2];
  DRWCallBuffer *camera_distances;
  DRWCallBuffer *camera_volume;
  DRWCallBuffer *camera_volume_frame;

  DRWCallBuffer *center_active;
  DRWCallBuffer *center_selected;
  DRWCallBuffer *center_deselected;
  DRWCallBuffer *center_selected_lib;
  DRWCallBuffer *center_deselected_lib;

  DRWCallBuffer *empty_axes;
  DRWCallBuffer *empty_capsule_body;
  DRWCallBuffer *empty_capsule_cap;
  DRWCallBuffer *empty_circle;
  DRWCallBuffer *empty_cone;
  DRWCallBuffer *empty_cube;
  DRWCallBuffer *empty_cylinder;
  DRWCallBuffer *empty_image_frame;
  DRWCallBuffer *empty_plain_axes;
  DRWCallBuffer *empty_single_arrow;
  DRWCallBuffer *empty_sphere;
  DRWCallBuffer *empty_sphere_solid;

  DRWCallBuffer *extra_dashed_lines;
  DRWCallBuffer *extra_lines;
  DRWCallBuffer *extra_points;

  DRWCallBuffer *field_curve;
  DRWCallBuffer *field_force;
  DRWCallBuffer *field_vortex;
  DRWCallBuffer *field_wind;
  DRWCallBuffer *field_cone_limit;
  DRWCallBuffer *field_sphere_limit;
  DRWCallBuffer *field_tube_limit;

  DRWCallBuffer *groundline;

  DRWCallBuffer *light_icon_inner;
  DRWCallBuffer *light_icon_outer;
  DRWCallBuffer *light_icon_sun_rays;
  DRWCallBuffer *light_point;
  DRWCallBuffer *light_sun;
  DRWCallBuffer *light_spot;
  DRWCallBuffer *light_spot_cone_back;
  DRWCallBuffer *light_spot_cone_front;
  DRWCallBuffer *light_area[2];

  DRWCallBuffer *origin_xform;

  DRWCallBuffer *probe_planar;
  DRWCallBuffer *probe_cube;
  DRWCallBuffer *probe_grid;

  DRWCallBuffer *solid_quad;

  DRWCallBuffer *speaker;

  DRWShadingGroup *extra_wire;
  DRWShadingGroup *extra_loose_points;
} OVERLAY_ExtraCallBuffers;

typedef struct OVERLAY_ArmatureCallBuffersInner {
  DRWCallBuffer *box_outline;
  DRWCallBuffer *box_fill;

  DRWCallBuffer *dof_lines;
  DRWCallBuffer *dof_sphere;

  DRWCallBuffer *envelope_distance;
  DRWCallBuffer *envelope_outline;
  DRWCallBuffer *envelope_fill;

  DRWCallBuffer *octa_outline;
  DRWCallBuffer *octa_fill;

  DRWCallBuffer *point_outline;
  DRWCallBuffer *point_fill;

  DRWCallBuffer *stick;

  DRWCallBuffer *wire;

  DRWShadingGroup *custom_outline;
  DRWShadingGroup *custom_fill;
  DRWShadingGroup *custom_wire;

  GHash *custom_shapes_ghash;
} OVERLAY_ArmatureCallBuffersInner;

typedef struct OVERLAY_ArmatureCallBuffers {
  OVERLAY_ArmatureCallBuffersInner solid;
  OVERLAY_ArmatureCallBuffersInner transp;
} OVERLAY_ArmatureCallBuffers;

typedef struct OVERLAY_PrivateData {
  DRWShadingGroup *armature_bone_select_act_grp;
  DRWShadingGroup *armature_bone_select_grp;
  DRWShadingGroup *edit_curve_normal_grp[2];
  DRWShadingGroup *edit_curve_wire_grp[2];
  DRWShadingGroup *edit_curve_handle_grp;
  DRWShadingGroup *edit_curve_points_grp;
  DRWShadingGroup *edit_lattice_points_grp;
  DRWShadingGroup *edit_lattice_wires_grp;
  DRWShadingGroup *edit_gpencil_points_grp;
  DRWShadingGroup *edit_gpencil_wires_grp;
  DRWShadingGroup *edit_gpencil_curve_handle_grp;
  DRWShadingGroup *edit_gpencil_curve_points_grp;
  DRWShadingGroup *edit_mesh_depth_grp[2];
  DRWShadingGroup *edit_mesh_faces_grp[2];
  DRWShadingGroup *edit_mesh_faces_cage_grp[2];
  DRWShadingGroup *edit_mesh_verts_grp[2];
  DRWShadingGroup *edit_mesh_edges_grp[2];
  DRWShadingGroup *edit_mesh_facedots_grp[2];
  DRWShadingGroup *edit_mesh_skin_roots_grp[2];
  DRWShadingGroup *edit_mesh_normals_grp;
  DRWShadingGroup *edit_mesh_analysis_grp;
  DRWShadingGroup *edit_particle_strand_grp;
  DRWShadingGroup *edit_particle_point_grp;
  DRWShadingGroup *edit_text_cursor_grp;
  DRWShadingGroup *edit_text_selection_grp;
  DRWShadingGroup *edit_text_wire_grp[2];
  DRWShadingGroup *edit_uv_verts_grp;
  DRWShadingGroup *edit_uv_edges_grp;
  DRWShadingGroup *edit_uv_shadow_edges_grp;
  DRWShadingGroup *edit_uv_faces_grp;
  DRWShadingGroup *edit_uv_face_dots_grp;
  DRWShadingGroup *edit_uv_stretching_grp;
  DRWShadingGroup *edit_curves_points_grp[2];
  DRWShadingGroup *edit_curves_lines_grp[2];
  DRWShadingGroup *extra_grid_grp;
  DRWShadingGroup *facing_grp[2];
  DRWShadingGroup *fade_grp[2];
  DRWShadingGroup *flash_grp[2];
  DRWShadingGroup *motion_path_lines_grp;
  DRWShadingGroup *motion_path_points_grp;
  DRWShadingGroup *outlines_grp;
  DRWShadingGroup *outlines_curves_grp;
  DRWShadingGroup *outlines_ptcloud_grp;
  DRWShadingGroup *outlines_gpencil_grp;
  DRWShadingGroup *paint_depth_grp;
  DRWShadingGroup *paint_surf_grp;
  DRWShadingGroup *paint_wire_grp;
  DRWShadingGroup *paint_wire_selected_grp;
  DRWShadingGroup *paint_point_grp;
  DRWShadingGroup *paint_face_grp;
  DRWShadingGroup *particle_dots_grp;
  DRWShadingGroup *particle_shapes_grp;
  DRWShadingGroup *pointcloud_dots_grp;
  DRWShadingGroup *sculpt_mask_grp;
  DRWShadingGroup *sculpt_curves_selection_grp;
  DRWShadingGroup *sculpt_curves_cage_lines_grp;
  DRWShadingGroup *viewer_attribute_curve_grp;
  DRWShadingGroup *viewer_attribute_curves_grp;
  DRWShadingGroup *viewer_attribute_mesh_grp;
  DRWShadingGroup *viewer_attribute_pointcloud_grp;
  DRWShadingGroup *viewer_attribute_instance_grp;
  DRWShadingGroup *viewer_attribute_instance_pointcloud_grp;
  DRWShadingGroup *volume_selection_surface_grp;
  DRWShadingGroup *wires_grp[2][2];      /* With and without coloring. */
  DRWShadingGroup *wires_all_grp[2][2];  /* With and without coloring. */
  DRWShadingGroup *wires_hair_grp[2][2]; /* With and without coloring. */
  DRWShadingGroup *wires_sculpt_grp[2];

  DRWView *view_default;
  DRWView *view_wires;
  DRWView *view_edit_faces;
  DRWView *view_edit_faces_cage;
  DRWView *view_edit_edges;
  DRWView *view_edit_verts;
  DRWView *view_edit_text;
  DRWView *view_reference_images;
  DRWView *view_edit_curves;

  /** TODO: get rid of this. */
  ListBase bg_movie_clips;

  /** Two instances for in_front option and without. */
  OVERLAY_ExtraCallBuffers extra_call_buffers[2];

  OVERLAY_ArmatureCallBuffers armature_call_buffers[2];

  View3DOverlay overlay;
  enum eContextObjectMode ctx_mode;
  char space_type;
  bool clear_in_front;
  bool use_in_front;
  bool wireframe_mode;
  bool hide_overlays;
  bool xray_enabled;
  bool xray_enabled_and_not_wire;
  float xray_opacity;
  short v3d_flag;     /* TODO: move to #View3DOverlay. */
  short v3d_gridflag; /* TODO: move to #View3DOverlay. */
  int cfra;
  DRWState clipping_state;
  OVERLAY_ShadingData shdata;
  OVERLAY_GridData grid_data;

  struct {
    float grid_axes[3];
    float zplane_axes[3];
    OVERLAY_GridBits zneg_flag, zpos_flag, grid_flag;
  } grid;
  struct {
    bool enabled;
    bool do_depth_copy;
    bool do_depth_infront_copy;
  } antialiasing;
  struct {
    bool show_handles;
    int handle_display;
  } edit_curve;
  struct {
    float cursor_color[4];
    float selection_color[4];
  } edit_text;
  struct {
    bool do_zbufclip;
    bool do_faces;
    bool do_edges;
    bool select_vert;
    bool select_face;
    bool select_edge;
    int flag; /** Copy of #v3d->overlay.edit_flag. */
  } edit_mesh;
  struct {
    bool do_points;
    bool do_zbufclip;
  } edit_curves;
  struct {
    bool use_weight;
    int select_mode;
  } edit_particle;
  struct {
    bool do_uv_overlay;
    bool do_uv_shadow_overlay;
    bool do_uv_stretching_overlay;
    bool do_tiled_image_overlay;
    bool do_tiled_image_border_overlay;
    bool do_stencil_overlay;
    bool do_mask_overlay;

    bool do_verts;
    bool do_faces;
    bool do_face_dots;

    float uv_opacity;

    int image_size[2];
    float image_aspect[2];

    /* edge drawing */
    OVERLAY_UVLineStyle line_style;
    float dash_length;
    int do_smooth_wire;

    /* stretching overlay */
    float uv_aspect[2];
    eSpaceImage_UVDT_Stretch draw_type;
    ListBase totals;
    float total_area_ratio;

    /* stencil overlay */
    struct Image *stencil_image;
    struct ImBuf *stencil_ibuf;
    void *stencil_lock;

    /* mask overlay */
    Mask *mask;
    eMaskOverlayMode mask_overlay_mode;
    GPUTexture *mask_texture;
  } edit_uv;
  struct {
    bool transparent;
    bool show_relations;
    bool do_pose_xray;
    bool do_pose_fade_geom;
  } armature;
  struct {
    bool in_front;
    bool alpha_blending;
  } painting;
  struct {
    DRWCallBuffer *handle[2];
  } mball;
  struct {
    double time;
    bool any_animated;
  } mode_transfer;
} OVERLAY_PrivateData; /* Transient data */

typedef struct OVERLAY_StorageList {
  struct OVERLAY_PrivateData *pd;
} OVERLAY_StorageList;

typedef struct OVERLAY_Instance {
  GPUUniformBuf *grid_ubo;
} OVERLAY_Instance;

typedef struct OVERLAY_Data {
  void *engine_type;
  OVERLAY_FramebufferList *fbl;
  OVERLAY_TextureList *txl;
  OVERLAY_PassList *psl;
  OVERLAY_StorageList *stl;

  OVERLAY_Instance *instance;
} OVERLAY_Data;

typedef struct OVERLAY_DupliData {
  DRWShadingGroup *wire_shgrp;
  DRWShadingGroup *outline_shgrp;
  DRWShadingGroup *extra_shgrp;
  struct GPUBatch *wire_geom;
  struct GPUBatch *outline_geom;
  struct GPUBatch *extra_geom;
  short base_flag;
} OVERLAY_DupliData;

typedef struct BoneInstanceData {
  /* Keep sync with bone instance vertex format (OVERLAY_InstanceFormats) */
  union {
    float mat[4][4];
    struct {
      float _pad0[3], color_hint_a;
      float _pad1[3], color_hint_b;
      float _pad2[3], color_a;
      float _pad3[3], color_b;
    };
    struct {
      float _pad00[3], amin_a;
      float _pad01[3], amin_b;
      float _pad02[3], amax_a;
      float _pad03[3], amax_b;
    };
  };
} BoneInstanceData;

typedef struct OVERLAY_InstanceFormats {
  struct GPUVertFormat *instance_pos;
  struct GPUVertFormat *instance_extra;
  struct GPUVertFormat *instance_bone;
  struct GPUVertFormat *instance_bone_outline;
  struct GPUVertFormat *instance_bone_envelope;
  struct GPUVertFormat *instance_bone_envelope_distance;
  struct GPUVertFormat *instance_bone_envelope_outline;
  struct GPUVertFormat *instance_bone_stick;
  struct GPUVertFormat *pos;
  struct GPUVertFormat *pos_color;
  struct GPUVertFormat *wire_extra;
  struct GPUVertFormat *point_extra;
} OVERLAY_InstanceFormats;

/* Pack data into the last row of the 4x4 matrix. It will be decoded by the vertex shader. */
BLI_INLINE void pack_data_in_mat4(
    float rmat[4][4], const float mat[4][4], float a, float b, float c, float d)
{
  copy_m4_m4(rmat, mat);
  rmat[0][3] = a;
  rmat[1][3] = b;
  rmat[2][3] = c;
  rmat[3][3] = d;
}

BLI_INLINE void pack_v4_in_mat4(float rmat[4][4], const float mat[4][4], const float v[4])
{
  pack_data_in_mat4(rmat, mat, v[0], v[1], v[2], v[3]);
}

BLI_INLINE void pack_fl_in_mat4(float rmat[4][4], const float mat[4][4], float a)
{
  copy_m4_m4(rmat, mat);
  rmat[3][3] = a;
}

void OVERLAY_antialiasing_init(OVERLAY_Data *vedata);
void OVERLAY_antialiasing_cache_init(OVERLAY_Data *vedata);
void OVERLAY_antialiasing_cache_finish(OVERLAY_Data *vedata);
void OVERLAY_antialiasing_start(OVERLAY_Data *vedata);
void OVERLAY_antialiasing_end(OVERLAY_Data *vedata);
void OVERLAY_xray_fade_draw(OVERLAY_Data *vedata);
void OVERLAY_xray_depth_copy(OVERLAY_Data *vedata);
void OVERLAY_xray_depth_infront_copy(OVERLAY_Data *vedata);

/**
 * Return true if armature should be handled by the pose mode engine.
 */
bool OVERLAY_armature_is_pose_mode(Object *ob, const struct DRWContextState *draw_ctx);
void OVERLAY_armature_cache_init(OVERLAY_Data *vedata);
void OVERLAY_armature_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_edit_armature_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_pose_armature_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_armature_cache_finish(OVERLAY_Data *vedata);
void OVERLAY_armature_draw(OVERLAY_Data *vedata);
void OVERLAY_armature_in_front_draw(OVERLAY_Data *vedata);
void OVERLAY_pose_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_pose_draw(OVERLAY_Data *vedata);

void OVERLAY_background_cache_init(OVERLAY_Data *vedata);
void OVERLAY_background_draw(OVERLAY_Data *vedata);

void OVERLAY_bone_instance_data_set_color_hint(BoneInstanceData *data, const float hint_color[4]);
void OVERLAY_bone_instance_data_set_color(BoneInstanceData *data, const float bone_color[4]);

void OVERLAY_edit_curve_cache_init(OVERLAY_Data *vedata);
void OVERLAY_edit_curve_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_edit_surf_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_edit_curve_draw(OVERLAY_Data *vedata);

void OVERLAY_edit_gpencil_cache_init(OVERLAY_Data *vedata);
void OVERLAY_gpencil_cache_init(OVERLAY_Data *vedata);
void OVERLAY_gpencil_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_gpencil_draw(OVERLAY_Data *vedata);
void OVERLAY_edit_gpencil_draw(OVERLAY_Data *vedata);

void OVERLAY_edit_lattice_cache_init(OVERLAY_Data *vedata);
void OVERLAY_edit_lattice_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_lattice_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_edit_lattice_draw(OVERLAY_Data *vedata);

void OVERLAY_edit_text_cache_init(OVERLAY_Data *vedata);
void OVERLAY_edit_text_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_edit_text_draw(OVERLAY_Data *vedata);

void OVERLAY_volume_cache_init(OVERLAY_Data *vedata);
void OVERLAY_volume_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_volume_draw(OVERLAY_Data *vedata);

void OVERLAY_edit_mesh_init(OVERLAY_Data *vedata);
void OVERLAY_edit_mesh_cache_init(OVERLAY_Data *vedata);
void OVERLAY_edit_mesh_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_edit_mesh_draw(OVERLAY_Data *vedata);

void OVERLAY_edit_particle_cache_init(OVERLAY_Data *vedata);
void OVERLAY_edit_particle_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_edit_particle_draw(OVERLAY_Data *vedata);

void OVERLAY_edit_uv_init(OVERLAY_Data *vedata);
void OVERLAY_edit_uv_cache_init(OVERLAY_Data *vedata);
void OVERLAY_edit_uv_cache_finish(OVERLAY_Data *vedata);
void OVERLAY_edit_uv_draw(OVERLAY_Data *vedata);

void OVERLAY_extra_cache_init(OVERLAY_Data *vedata);
void OVERLAY_extra_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_extra_blend_draw(OVERLAY_Data *vedata);
void OVERLAY_extra_draw(OVERLAY_Data *vedata);
void OVERLAY_extra_in_front_draw(OVERLAY_Data *vedata);
void OVERLAY_extra_centers_draw(OVERLAY_Data *vedata);

void OVERLAY_camera_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_empty_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_light_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_lightprobe_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_speaker_cache_populate(OVERLAY_Data *vedata, Object *ob);

OVERLAY_ExtraCallBuffers *OVERLAY_extra_call_buffer_get(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_extra_point(OVERLAY_ExtraCallBuffers *cb, const float point[3], const float color[4]);
void OVERLAY_extra_line_dashed(OVERLAY_ExtraCallBuffers *cb,
                               const float start[3],
                               const float end[3],
                               const float color[4]);
void OVERLAY_extra_line(OVERLAY_ExtraCallBuffers *cb,
                        const float start[3],
                        const float end[3],
                        int color_id);
void OVERLAY_empty_shape(OVERLAY_ExtraCallBuffers *cb,
                         const float mat[4][4],
                         float draw_size,
                         char draw_type,
                         const float color[4]);
void OVERLAY_extra_loose_points(OVERLAY_ExtraCallBuffers *cb,
                                struct GPUBatch *geom,
                                const float mat[4][4],
                                const float color[4]);
void OVERLAY_extra_wire(OVERLAY_ExtraCallBuffers *cb,
                        struct GPUBatch *geom,
                        const float mat[4][4],
                        const float color[4]);

void OVERLAY_facing_init(OVERLAY_Data *vedata);
void OVERLAY_facing_cache_init(OVERLAY_Data *vedata);
void OVERLAY_facing_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_facing_draw(OVERLAY_Data *vedata);
void OVERLAY_facing_infront_draw(OVERLAY_Data *vedata);

void OVERLAY_fade_init(OVERLAY_Data *vedata);
void OVERLAY_fade_cache_init(OVERLAY_Data *vedata);
void OVERLAY_fade_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_fade_draw(OVERLAY_Data *vedata);
void OVERLAY_fade_infront_draw(OVERLAY_Data *vedata);

void OVERLAY_mode_transfer_cache_init(OVERLAY_Data *vedata);
void OVERLAY_mode_transfer_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_mode_transfer_draw(OVERLAY_Data *vedata);
void OVERLAY_mode_transfer_infront_draw(OVERLAY_Data *vedata);
void OVERLAY_mode_transfer_cache_finish(OVERLAY_Data *vedata);

void OVERLAY_grid_init(OVERLAY_Data *vedata);
void OVERLAY_grid_cache_init(OVERLAY_Data *vedata);
void OVERLAY_grid_draw(OVERLAY_Data *vedata);

void OVERLAY_image_init(OVERLAY_Data *vedata);
void OVERLAY_image_cache_init(OVERLAY_Data *vedata);
void OVERLAY_image_camera_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_image_empty_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_image_cache_finish(OVERLAY_Data *vedata);
void OVERLAY_image_draw(OVERLAY_Data *vedata);
void OVERLAY_image_background_draw(OVERLAY_Data *vedata);
/**
 * This function draws images that needs the view transform applied.
 * It draws these images directly into the scene color buffer.
 */
void OVERLAY_image_scene_background_draw(OVERLAY_Data *vedata);
void OVERLAY_image_in_front_draw(OVERLAY_Data *vedata);

void OVERLAY_metaball_cache_init(OVERLAY_Data *vedata);
void OVERLAY_edit_metaball_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_metaball_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_metaball_draw(OVERLAY_Data *vedata);
void OVERLAY_metaball_in_front_draw(OVERLAY_Data *vedata);

void OVERLAY_motion_path_cache_init(OVERLAY_Data *vedata);
void OVERLAY_motion_path_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_motion_path_draw(OVERLAY_Data *vedata);

void OVERLAY_outline_init(OVERLAY_Data *vedata);
void OVERLAY_outline_cache_init(OVERLAY_Data *vedata);
void OVERLAY_outline_cache_populate(OVERLAY_Data *vedata,
                                    Object *ob,
                                    OVERLAY_DupliData *dupli,
                                    bool init_dupli);
void OVERLAY_outline_draw(OVERLAY_Data *vedata);

void OVERLAY_paint_init(OVERLAY_Data *vedata);
void OVERLAY_paint_cache_init(OVERLAY_Data *vedata);
void OVERLAY_paint_texture_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_paint_vertex_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_paint_weight_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_paint_draw(OVERLAY_Data *vedata);

void OVERLAY_particle_cache_init(OVERLAY_Data *vedata);
void OVERLAY_particle_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_particle_draw(OVERLAY_Data *vedata);

void OVERLAY_sculpt_cache_init(OVERLAY_Data *vedata);
void OVERLAY_sculpt_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_sculpt_draw(OVERLAY_Data *vedata);

void OVERLAY_sculpt_curves_cache_init(OVERLAY_Data *vedata);
void OVERLAY_sculpt_curves_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_sculpt_curves_draw(OVERLAY_Data *vedata);
void OVERLAY_sculpt_curves_draw_wires(OVERLAY_Data *vedata);

void OVERLAY_viewer_attribute_cache_init(OVERLAY_Data *vedata);
void OVERLAY_viewer_attribute_cache_populate(OVERLAY_Data *vedata, Object *object);
void OVERLAY_viewer_attribute_draw(OVERLAY_Data *vedata);

void OVERLAY_wireframe_init(OVERLAY_Data *vedata);
void OVERLAY_wireframe_cache_init(OVERLAY_Data *vedata);
void OVERLAY_wireframe_cache_populate(OVERLAY_Data *vedata,
                                      Object *ob,
                                      OVERLAY_DupliData *dupli,
                                      bool init_dupli);
void OVERLAY_wireframe_draw(OVERLAY_Data *vedata);
void OVERLAY_wireframe_in_front_draw(OVERLAY_Data *vedata);

void OVERLAY_edit_curves_init(OVERLAY_Data *vedata);
void OVERLAY_edit_curves_cache_init(OVERLAY_Data *vedata);
void OVERLAY_edit_curves_cache_populate(OVERLAY_Data *vedata, Object *ob);
void OVERLAY_edit_curves_draw(OVERLAY_Data *vedata);

GPUShader *OVERLAY_shader_antialiasing(void);
GPUShader *OVERLAY_shader_armature_degrees_of_freedom_wire(void);
GPUShader *OVERLAY_shader_armature_degrees_of_freedom_solid(void);
GPUShader *OVERLAY_shader_armature_envelope(bool use_outline);
GPUShader *OVERLAY_shader_armature_shape(bool use_outline);
GPUShader *OVERLAY_shader_armature_shape_wire(void);
GPUShader *OVERLAY_shader_armature_sphere(bool use_outline);
GPUShader *OVERLAY_shader_armature_stick(void);
GPUShader *OVERLAY_shader_armature_wire(void);
GPUShader *OVERLAY_shader_background(void);
GPUShader *OVERLAY_shader_clipbound(void);
GPUShader *OVERLAY_shader_depth_only(void);
GPUShader *OVERLAY_shader_edit_curve_handle(void);
GPUShader *OVERLAY_shader_edit_curve_point(void);
GPUShader *OVERLAY_shader_edit_curve_wire(void);
GPUShader *OVERLAY_shader_edit_gpencil_guide_point(void);
GPUShader *OVERLAY_shader_edit_gpencil_point(void);
GPUShader *OVERLAY_shader_edit_gpencil_wire(void);
GPUShader *OVERLAY_shader_edit_lattice_point(void);
GPUShader *OVERLAY_shader_edit_lattice_wire(void);
GPUShader *OVERLAY_shader_edit_mesh_analysis(void);
GPUShader *OVERLAY_shader_edit_mesh_depth(void);
GPUShader *OVERLAY_shader_edit_mesh_edge(bool use_flat_interp);
GPUShader *OVERLAY_shader_edit_mesh_face(void);
GPUShader *OVERLAY_shader_edit_mesh_facedot(void);
GPUShader *OVERLAY_shader_edit_mesh_normal(void);
GPUShader *OVERLAY_shader_edit_mesh_skin_root(void);
GPUShader *OVERLAY_shader_edit_mesh_vert(void);
GPUShader *OVERLAY_shader_edit_particle_strand(void);
GPUShader *OVERLAY_shader_edit_particle_point(void);
GPUShader *OVERLAY_shader_edit_uv_edges_get(void);
GPUShader *OVERLAY_shader_edit_uv_edges_for_edge_select_get(void);
GPUShader *OVERLAY_shader_edit_uv_face_get(void);
GPUShader *OVERLAY_shader_edit_uv_face_dots_get(void);
GPUShader *OVERLAY_shader_edit_uv_verts_get(void);
GPUShader *OVERLAY_shader_edit_uv_stretching_area_get(void);
GPUShader *OVERLAY_shader_edit_uv_stretching_angle_get(void);
GPUShader *OVERLAY_shader_edit_uv_tiled_image_borders_get(void);
GPUShader *OVERLAY_shader_edit_uv_stencil_image(void);
GPUShader *OVERLAY_shader_edit_uv_mask_image(void);
GPUShader *OVERLAY_shader_extra(bool is_select);
GPUShader *OVERLAY_shader_extra_groundline(void);
GPUShader *OVERLAY_shader_extra_wire(bool use_object, bool is_select);
GPUShader *OVERLAY_shader_extra_loose_point(void);
GPUShader *OVERLAY_shader_extra_point(void);
GPUShader *OVERLAY_shader_facing(void);
GPUShader *OVERLAY_shader_gpencil_canvas(void);
GPUShader *OVERLAY_shader_grid(void);
GPUShader *OVERLAY_shader_grid_background(void);
GPUShader *OVERLAY_shader_grid_image(void);
GPUShader *OVERLAY_shader_image(void);
GPUShader *OVERLAY_shader_motion_path_line(void);
GPUShader *OVERLAY_shader_motion_path_vert(void);
GPUShader *OVERLAY_shader_uniform_color(void);
GPUShader *OVERLAY_shader_uniform_color_pointcloud(void);
GPUShader *OVERLAY_shader_outline_prepass(bool use_wire);
GPUShader *OVERLAY_shader_outline_prepass_curves(void);
GPUShader *OVERLAY_shader_outline_prepass_gpencil(void);
GPUShader *OVERLAY_shader_outline_prepass_pointcloud(void);
GPUShader *OVERLAY_shader_extra_grid(void);
GPUShader *OVERLAY_shader_outline_detect(void);
GPUShader *OVERLAY_shader_paint_face(void);
GPUShader *OVERLAY_shader_paint_point(void);
GPUShader *OVERLAY_shader_paint_texture(void);
GPUShader *OVERLAY_shader_paint_vertcol(void);
GPUShader *OVERLAY_shader_paint_weight(bool shading);
GPUShader *OVERLAY_shader_paint_wire(void);
GPUShader *OVERLAY_shader_particle_dot(void);
GPUShader *OVERLAY_shader_particle_shape(void);
GPUShader *OVERLAY_shader_sculpt_mask(void);
GPUShader *OVERLAY_shader_sculpt_curves_selection(void);
GPUShader *OVERLAY_shader_sculpt_curves_cage(void);
GPUShader *OVERLAY_shader_viewer_attribute_curve(void);
GPUShader *OVERLAY_shader_viewer_attribute_curves(void);
GPUShader *OVERLAY_shader_viewer_attribute_mesh(void);
GPUShader *OVERLAY_shader_viewer_attribute_pointcloud(void);
GPUShader *OVERLAY_shader_volume_velocity(bool use_needle, bool use_mac);
GPUShader *OVERLAY_shader_volume_gridlines(bool color_with_flags, bool color_range);
GPUShader *OVERLAY_shader_wireframe(bool custom_bias);
GPUShader *OVERLAY_shader_wireframe_select(void);
GPUShader *OVERLAY_shader_xray_fade(void);

OVERLAY_InstanceFormats *OVERLAY_shader_instance_formats_get(void);

void OVERLAY_shader_free(void);

#ifdef __cplusplus
}
#endif
