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
 *
 * Copyright 2016, Blender Foundation.
 */

/** \file
 * \ingroup draw
 */

#include "DRW_engine.h"
#include "DRW_render.h"

#include "DNA_userdef_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_force_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_particle_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_smoke_types.h"
#include "DNA_view3d_types.h"
#include "DNA_screen_types.h"
#include "DNA_world_types.h"

#include "BKE_anim.h"
#include "BKE_camera.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_editmesh.h"
#include "BKE_image.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_movieclip.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_tracking.h"

#include "BLI_ghash.h"

#include "IMB_imbuf_types.h"

#include "ED_view3d.h"

#include "GPU_batch.h"
#include "GPU_draw.h"
#include "GPU_shader.h"
#include "GPU_texture.h"

#include "MEM_guardedalloc.h"

#include "UI_resources.h"

#include "draw_mode_engines.h"
#include "draw_manager_text.h"
#include "draw_common.h"

#include "DEG_depsgraph_query.h"

extern char datatoc_object_outline_prepass_vert_glsl[];
extern char datatoc_object_outline_prepass_geom_glsl[];
extern char datatoc_object_outline_prepass_frag_glsl[];
extern char datatoc_object_outline_resolve_frag_glsl[];
extern char datatoc_object_outline_detect_frag_glsl[];
extern char datatoc_object_outline_expand_frag_glsl[];
extern char datatoc_object_grid_frag_glsl[];
extern char datatoc_object_grid_vert_glsl[];
extern char datatoc_object_camera_image_frag_glsl[];
extern char datatoc_object_camera_image_vert_glsl[];
extern char datatoc_object_empty_image_frag_glsl[];
extern char datatoc_object_empty_image_vert_glsl[];
extern char datatoc_object_lightprobe_grid_vert_glsl[];
extern char datatoc_object_loose_points_frag_glsl[];
extern char datatoc_object_particle_prim_vert_glsl[];
extern char datatoc_object_particle_dot_vert_glsl[];
extern char datatoc_object_particle_dot_frag_glsl[];
extern char datatoc_common_colormanagement_lib_glsl[];
extern char datatoc_common_globals_lib_glsl[];
extern char datatoc_common_view_lib_glsl[];
extern char datatoc_common_fxaa_lib_glsl[];
extern char datatoc_gpu_shader_flat_color_frag_glsl[];
extern char datatoc_gpu_shader_flat_id_frag_glsl[];
extern char datatoc_common_fullscreen_vert_glsl[];
extern char datatoc_gpu_shader_uniform_color_frag_glsl[];
extern char datatoc_gpu_shader_3D_vert_glsl[];

/* *********** LISTS *********** */
typedef struct OBJECT_PassList {
  struct DRWPass *non_meshes[2];
  struct DRWPass *image_empties[2];
  struct DRWPass *transp_shapes[2];
  struct DRWPass *ob_center;
  struct DRWPass *outlines;
  struct DRWPass *outlines_search;
  struct DRWPass *outlines_expand;
  struct DRWPass *outlines_bleed;
  struct DRWPass *outlines_resolve;
  struct DRWPass *grid;
  struct DRWPass *bone_solid[2];
  struct DRWPass *bone_outline[2];
  struct DRWPass *bone_wire[2];
  struct DRWPass *bone_envelope[2];
  struct DRWPass *bone_axes[2];
  struct DRWPass *particle;
  struct DRWPass *lightprobes;
  struct DRWPass *camera_images_back;
  struct DRWPass *camera_images_front;
} OBJECT_PassList;

typedef struct OBJECT_FramebufferList {
  struct GPUFrameBuffer *outlines_fb;
  struct GPUFrameBuffer *blur_fb;
  struct GPUFrameBuffer *expand_fb;
  struct GPUFrameBuffer *ghost_fb;
} OBJECT_FramebufferList;

typedef struct OBJECT_StorageList {
  struct OBJECT_PrivateData *g_data;
} OBJECT_StorageList;

typedef struct OBJECT_Data {
  void *engine_type;
  OBJECT_FramebufferList *fbl;
  DRWViewportEmptyList *txl;
  OBJECT_PassList *psl;
  OBJECT_StorageList *stl;
} OBJECT_Data;

typedef struct OBJECT_Shaders {
  /* fullscreen shaders */
  GPUShader *outline_prepass;
  GPUShader *outline_prepass_wire;
  GPUShader *outline_resolve;
  GPUShader *outline_resolve_aa;
  GPUShader *outline_detect;
  GPUShader *outline_detect_wire;
  GPUShader *outline_fade;
  GPUShader *outline_fade_large;

  /* regular shaders */
  GPUShader *object_camera_image;
  GPUShader *object_camera_image_cm;
  GPUShader *object_empty_image;
  GPUShader *object_empty_image_wire;
  GPUShader *grid;
  GPUShader *part_dot;
  GPUShader *part_prim;
  GPUShader *part_axis;
  GPUShader *lightprobe_grid;
  GPUShader *loose_points;
} OBJECT_Shaders;

/* *********** STATIC *********** */

typedef struct OBJECT_ShadingGroupList {
  /* Reference only */
  struct DRWPass *non_meshes;
  struct DRWPass *image_empties;
  struct DRWPass *transp_shapes;
  struct DRWPass *bone_solid;
  struct DRWPass *bone_outline;
  struct DRWPass *bone_wire;
  struct DRWPass *bone_envelope;
  struct DRWPass *bone_axes;

  /* Empties */
  DRWEmptiesBufferList empties;

  /* Force Field */
  DRWCallBuffer *field_wind;
  DRWCallBuffer *field_force;
  DRWCallBuffer *field_vortex;
  DRWCallBuffer *field_curve_sta;
  DRWCallBuffer *field_curve_end;
  DRWCallBuffer *field_tube_limit;
  DRWCallBuffer *field_cone_limit;

  /* Grease Pencil */
  DRWCallBuffer *gpencil_axes;

  /* Speaker */
  DRWCallBuffer *speaker;

  /* Probe */
  DRWCallBuffer *probe_cube;
  DRWCallBuffer *probe_planar;
  DRWCallBuffer *probe_grid;

  /* MetaBalls */
  DRWCallBuffer *mball_handle;

  /* Lights */
  DRWCallBuffer *light_center;
  DRWCallBuffer *light_groundpoint;
  DRWCallBuffer *light_groundline;
  DRWCallBuffer *light_circle;
  DRWCallBuffer *light_circle_shadow;
  DRWCallBuffer *light_sunrays;
  DRWCallBuffer *light_distance;
  DRWCallBuffer *light_buflimit;
  DRWCallBuffer *light_buflimit_points;
  DRWCallBuffer *light_area_sphere;
  DRWCallBuffer *light_area_square;
  DRWCallBuffer *light_area_disk;
  DRWCallBuffer *light_hemi;
  DRWCallBuffer *light_spot_cone;
  DRWCallBuffer *light_spot_blend;
  DRWCallBuffer *light_spot_pyramid;
  DRWCallBuffer *light_spot_blend_rect;
  DRWCallBuffer *light_spot_volume;
  DRWCallBuffer *light_spot_volume_rect;
  DRWCallBuffer *light_spot_volume_outside;
  DRWCallBuffer *light_spot_volume_rect_outside;

  /* Helpers */
  DRWCallBuffer *relationship_lines;
  DRWCallBuffer *constraint_lines;

  /* Camera */
  DRWCallBuffer *camera;
  DRWCallBuffer *camera_frame;
  DRWCallBuffer *camera_tria;
  DRWCallBuffer *camera_focus;
  DRWCallBuffer *camera_clip;
  DRWCallBuffer *camera_clip_points;
  DRWCallBuffer *camera_mist;
  DRWCallBuffer *camera_mist_points;
  DRWCallBuffer *camera_stereo_plane;
  DRWCallBuffer *camera_stereo_plane_wires;
  DRWCallBuffer *camera_stereo_volume;
  DRWCallBuffer *camera_stereo_volume_wires;
  ListBase camera_path;

  /* Wire */
  DRWShadingGroup *wire;
  DRWShadingGroup *wire_active;
  DRWShadingGroup *wire_select;
  DRWShadingGroup *wire_transform;
  /* Wire (duplicator) */
  DRWShadingGroup *wire_dupli;
  DRWShadingGroup *wire_dupli_select;

  /* Points */
  DRWShadingGroup *points;
  DRWShadingGroup *points_active;
  DRWShadingGroup *points_select;
  DRWShadingGroup *points_transform;
  /* Points (duplicator) */
  DRWShadingGroup *points_dupli;
  DRWShadingGroup *points_dupli_select;

  /* Texture Space */
  DRWCallBuffer *texspace;
} OBJECT_ShadingGroupList;

typedef struct OBJECT_PrivateData {
  OBJECT_ShadingGroupList sgl;
  OBJECT_ShadingGroupList sgl_ghost;

  GHash *custom_shapes;

  /* Outlines */
  DRWShadingGroup *outlines_active;
  DRWShadingGroup *outlines_select;
  DRWShadingGroup *outlines_select_dupli;
  DRWShadingGroup *outlines_transform;

  /* Lightprobes */
  DRWCallBuffer *lightprobes_cube_select;
  DRWCallBuffer *lightprobes_cube_select_dupli;
  DRWCallBuffer *lightprobes_cube_active;
  DRWCallBuffer *lightprobes_cube_transform;

  DRWCallBuffer *lightprobes_planar_select;
  DRWCallBuffer *lightprobes_planar_select_dupli;
  DRWCallBuffer *lightprobes_planar_active;
  DRWCallBuffer *lightprobes_planar_transform;

  /* Objects Centers */
  DRWCallBuffer *center_active;
  DRWCallBuffer *center_selected;
  DRWCallBuffer *center_deselected;
  DRWCallBuffer *center_selected_lib;
  DRWCallBuffer *center_deselected_lib;

  /* Outlines id offset (accessed as an array) */
  int id_ofs_active;
  int id_ofs_select;
  int id_ofs_select_dupli;
  int id_ofs_transform;

  int id_ofs_prb_active;
  int id_ofs_prb_select;
  int id_ofs_prb_select_dupli;
  int id_ofs_prb_transform;

  bool xray_enabled;
  bool xray_enabled_and_not_wire;
} OBJECT_PrivateData; /* Transient data */

typedef struct OBJECT_DupliData {
  DRWShadingGroup *outline_shgrp;
  GPUBatch *outline_geom;
  DRWShadingGroup *extra_shgrp;
  GPUBatch *extra_geom;
  short base_flag;
} OBJECT_DupliData;

static struct {
  /* Instance Data format */
  struct GPUVertFormat *empty_image_format;
  struct GPUVertFormat *empty_image_wire_format;

  OBJECT_Shaders sh_data[GPU_SHADER_CFG_LEN];

  float grid_settings[5];
  float grid_mesh_size;
  int grid_flag;
  float grid_axes[3];
  int zpos_flag;
  int zneg_flag;
  float zplane_axes[3];
  float inv_viewport_size[2];
  bool draw_grid;
  /* Temp buffer textures */
  struct GPUTexture *outlines_depth_tx;
  struct GPUTexture *outlines_id_tx;
  struct GPUTexture *outlines_color_tx;
  struct GPUTexture *outlines_blur_tx;

  ListBase smoke_domains;
  ListBase movie_clips;
} e_data = {NULL}; /* Engine data */

enum {
  SHOW_AXIS_X = (1 << 0),
  SHOW_AXIS_Y = (1 << 1),
  SHOW_AXIS_Z = (1 << 2),
  SHOW_GRID = (1 << 3),
  PLANE_XY = (1 << 4),
  PLANE_XZ = (1 << 5),
  PLANE_YZ = (1 << 6),
  CLIP_ZPOS = (1 << 7),
  CLIP_ZNEG = (1 << 8),
  GRID_BACK = (1 << 9),
};

/* Prototypes. */
static void DRW_shgroup_empty_ex(OBJECT_ShadingGroupList *sgl,
                                 const float mat[4][4],
                                 const float *draw_size,
                                 char draw_type,
                                 const float color[4]);

/* *********** FUNCTIONS *********** */

static void OBJECT_engine_init(void *vedata)
{
  OBJECT_FramebufferList *fbl = ((OBJECT_Data *)vedata)->fbl;

  const float *viewport_size = DRW_viewport_size_get();
  const int size[2] = {(int)viewport_size[0], (int)viewport_size[1]};

  if (DRW_state_is_fbo()) {
    e_data.outlines_depth_tx = DRW_texture_pool_query_2d(
        size[0], size[1], GPU_DEPTH_COMPONENT24, &draw_engine_object_type);
    /* XXX TODO GPU_R16UI can overflow, it would cause no harm
     * (only bad colored or missing outlines) but we should
     * use 32bits only if the scene have that many objects */
    e_data.outlines_id_tx = DRW_texture_pool_query_2d(
        size[0], size[1], GPU_R16UI, &draw_engine_object_type);

    GPU_framebuffer_ensure_config(&fbl->outlines_fb,
                                  {GPU_ATTACHMENT_TEXTURE(e_data.outlines_depth_tx),
                                   GPU_ATTACHMENT_TEXTURE(e_data.outlines_id_tx)});

    e_data.outlines_color_tx = DRW_texture_pool_query_2d(
        size[0], size[1], GPU_RGBA8, &draw_engine_object_type);

    GPU_framebuffer_ensure_config(
        &fbl->expand_fb, {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(e_data.outlines_color_tx)});

    e_data.outlines_blur_tx = DRW_texture_pool_query_2d(
        size[0], size[1], GPU_RGBA8, &draw_engine_object_type);

    GPU_framebuffer_ensure_config(
        &fbl->blur_fb, {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(e_data.outlines_blur_tx)});
  }

  /* Shaders */
  const DRWContextState *draw_ctx = DRW_context_state_get();
  OBJECT_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];

  const GPUShaderConfigData *sh_cfg_data = &GPU_shader_cfg_data[draw_ctx->sh_cfg];

  if (!sh_data->outline_resolve) {
    /* Outline */
    sh_data->outline_prepass = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib, datatoc_gpu_shader_3D_vert_glsl, NULL},
        .frag = (const char *[]){datatoc_object_outline_prepass_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });
    sh_data->outline_prepass_wire = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_object_outline_prepass_vert_glsl,
                                 NULL},
        .geom = (const char *[]){sh_cfg_data->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_object_outline_prepass_geom_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_object_outline_prepass_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });

    sh_data->outline_resolve = DRW_shader_create_fullscreen(
        datatoc_object_outline_resolve_frag_glsl, NULL);

    sh_data->outline_resolve_aa = DRW_shader_create_with_lib(
        datatoc_common_fullscreen_vert_glsl,
        NULL,
        datatoc_object_outline_resolve_frag_glsl,
        datatoc_common_fxaa_lib_glsl,
        "#define FXAA_ALPHA\n"
        "#define USE_FXAA\n");

    sh_data->outline_detect = DRW_shader_create_with_lib(datatoc_common_fullscreen_vert_glsl,
                                                         NULL,
                                                         datatoc_object_outline_detect_frag_glsl,
                                                         datatoc_common_globals_lib_glsl,
                                                         NULL);

    sh_data->outline_detect_wire = DRW_shader_create_with_lib(
        datatoc_common_fullscreen_vert_glsl,
        NULL,
        datatoc_object_outline_detect_frag_glsl,
        datatoc_common_globals_lib_glsl,
        "#define WIRE\n");

    sh_data->outline_fade = DRW_shader_create_fullscreen(datatoc_object_outline_expand_frag_glsl,
                                                         NULL);
    sh_data->outline_fade_large = DRW_shader_create_fullscreen(
        datatoc_object_outline_expand_frag_glsl, "#define LARGE_OUTLINE\n");

    /* Empty images */
    {
      const char *empty_image_defs = (
              "#define DEPTH_UNCHANGED " STRINGIFY(OB_EMPTY_IMAGE_DEPTH_DEFAULT) "\n"
              "#define DEPTH_FRONT " STRINGIFY(OB_EMPTY_IMAGE_DEPTH_FRONT) "\n"
              "#define DEPTH_BACK " STRINGIFY(OB_EMPTY_IMAGE_DEPTH_BACK) "\n");

      sh_data->object_empty_image = GPU_shader_create_from_arrays({
          .vert = (const char *[]){sh_cfg_data->lib,
                                   datatoc_common_view_lib_glsl,
                                   datatoc_object_empty_image_vert_glsl,
                                   NULL},
          .frag = (const char *[]){datatoc_common_colormanagement_lib_glsl,
                                   datatoc_object_empty_image_frag_glsl,
                                   NULL},
          .defs = (const char *[]){sh_cfg_data->def, empty_image_defs, NULL},
      });
      sh_data->object_empty_image_wire = GPU_shader_create_from_arrays({
          .vert = (const char *[]){sh_cfg_data->lib,
                                   datatoc_common_view_lib_glsl,
                                   datatoc_object_empty_image_vert_glsl,
                                   NULL},
          .frag = (const char *[]){datatoc_object_empty_image_frag_glsl, NULL},
          .defs = (const char *[]){sh_cfg_data->def, "#define USE_WIRE\n", empty_image_defs, NULL},
      });

      sh_data->object_camera_image_cm = GPU_shader_create_from_arrays({
          .vert = (const char *[]){sh_cfg_data->lib, datatoc_object_camera_image_vert_glsl, NULL},
          .frag = (const char *[]){datatoc_common_colormanagement_lib_glsl,
                                   datatoc_object_camera_image_frag_glsl,
                                   NULL},
          .defs =
              (const char *[]){sh_cfg_data->def, "#define DRW_STATE_DO_COLOR_MANAGEMENT\n", NULL},
      });
      sh_data->object_camera_image = GPU_shader_create_from_arrays({
          .vert = (const char *[]){sh_cfg_data->lib, datatoc_object_camera_image_vert_glsl, NULL},
          .frag = (const char *[]){datatoc_common_colormanagement_lib_glsl,
                                   datatoc_object_camera_image_frag_glsl,
                                   NULL},
      });
    }

    /* Grid */
    sh_data->grid = GPU_shader_create_from_arrays({
        .vert = (const char *[]){datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_object_grid_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_object_grid_frag_glsl,
                                 NULL},
    });

    /* Particles */
    sh_data->part_prim = DRW_shader_create_with_lib(datatoc_object_particle_prim_vert_glsl,
                                                    NULL,
                                                    datatoc_gpu_shader_flat_color_frag_glsl,
                                                    datatoc_common_view_lib_glsl,
                                                    NULL);

    sh_data->part_axis = DRW_shader_create_with_lib(datatoc_object_particle_prim_vert_glsl,
                                                    NULL,
                                                    datatoc_gpu_shader_flat_color_frag_glsl,
                                                    datatoc_common_view_lib_glsl,
                                                    "#define USE_AXIS\n");

    sh_data->part_dot = DRW_shader_create_with_lib(datatoc_object_particle_dot_vert_glsl,
                                                   NULL,
                                                   datatoc_object_particle_dot_frag_glsl,
                                                   datatoc_common_view_lib_glsl,
                                                   NULL);

    /* Lightprobes */
    sh_data->lightprobe_grid = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_object_lightprobe_grid_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_flat_id_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });

    /* Loose Points */
    sh_data->loose_points = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib, datatoc_gpu_shader_3D_vert_glsl, NULL},
        .frag = (const char *[]){datatoc_object_loose_points_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });
  }

  {
    /* Grid precompute */
    float viewinv[4][4], wininv[4][4];
    float viewmat[4][4], winmat[4][4];
    View3D *v3d = draw_ctx->v3d;
    Scene *scene = draw_ctx->scene;
    RegionView3D *rv3d = draw_ctx->rv3d;
    float grid_scale = ED_view3d_grid_scale(scene, v3d, NULL);
    float grid_res;

    const bool show_axis_x = (v3d->gridflag & V3D_SHOW_X) != 0;
    const bool show_axis_y = (v3d->gridflag & V3D_SHOW_Y) != 0;
    const bool show_axis_z = (v3d->gridflag & V3D_SHOW_Z) != 0;
    const bool show_floor = (v3d->gridflag & V3D_SHOW_FLOOR) != 0;
    const bool show_ortho_grid = (v3d->gridflag & V3D_SHOW_ORTHO_GRID) != 0;
    e_data.draw_grid = show_axis_x || show_axis_y || show_axis_z || show_floor;

    DRW_view_winmat_get(NULL, winmat, false);
    DRW_view_winmat_get(NULL, wininv, true);
    DRW_view_viewmat_get(NULL, viewmat, false);
    DRW_view_viewmat_get(NULL, viewinv, true);

    /* if perps */
    if (winmat[3][3] == 0.0f) {
      float fov;
      float viewvecs[2][4] = {
          {1.0f, -1.0f, -1.0f, 1.0f},
          {-1.0f, 1.0f, -1.0f, 1.0f},
      };

      /* convert the view vectors to view space */
      for (int i = 0; i < 2; i++) {
        mul_m4_v4(wininv, viewvecs[i]);
        mul_v3_fl(viewvecs[i], 1.0f / viewvecs[i][2]); /* perspective divide */
      }

      fov = angle_v3v3(viewvecs[0], viewvecs[1]) / 2.0f;
      grid_res = fabsf(tanf(fov)) / grid_scale;

      e_data.grid_flag = (1 << 4); /* XY plane */
      if (show_axis_x) {
        e_data.grid_flag |= SHOW_AXIS_X;
      }
      if (show_axis_y) {
        e_data.grid_flag |= SHOW_AXIS_Y;
      }
      if (show_floor) {
        e_data.grid_flag |= SHOW_GRID;
      }
    }
    else {
      if (rv3d->view != RV3D_VIEW_USER) {
        /* Allow 3 more subdivisions. */
        grid_scale /= powf(v3d->gridsubdiv, 3);
      }

      float viewdist = 1.0f / max_ff(fabsf(winmat[0][0]), fabsf(winmat[1][1]));
      grid_res = viewdist / grid_scale;

      if (ELEM(rv3d->view, RV3D_VIEW_RIGHT, RV3D_VIEW_LEFT)) {
        e_data.draw_grid = show_ortho_grid;
        e_data.grid_flag = PLANE_YZ | SHOW_AXIS_Y | SHOW_AXIS_Z | SHOW_GRID | GRID_BACK;
      }
      else if (ELEM(rv3d->view, RV3D_VIEW_TOP, RV3D_VIEW_BOTTOM)) {
        e_data.draw_grid = show_ortho_grid;
        e_data.grid_flag = PLANE_XY | SHOW_AXIS_X | SHOW_AXIS_Y | SHOW_GRID | GRID_BACK;
      }
      else if (ELEM(rv3d->view, RV3D_VIEW_FRONT, RV3D_VIEW_BACK)) {
        e_data.draw_grid = show_ortho_grid;
        e_data.grid_flag = PLANE_XZ | SHOW_AXIS_X | SHOW_AXIS_Z | SHOW_GRID | GRID_BACK;
      }
      else { /* RV3D_VIEW_USER */
        e_data.grid_flag = PLANE_XY;
        if (show_axis_x) {
          e_data.grid_flag |= SHOW_AXIS_X;
        }
        if (show_axis_y) {
          e_data.grid_flag |= SHOW_AXIS_Y;
        }
        if (show_floor) {
          e_data.grid_flag |= SHOW_GRID;
        }
      }
    }

    e_data.grid_axes[0] = (float)((e_data.grid_flag & (PLANE_XZ | PLANE_XY)) != 0);
    e_data.grid_axes[1] = (float)((e_data.grid_flag & (PLANE_YZ | PLANE_XY)) != 0);
    e_data.grid_axes[2] = (float)((e_data.grid_flag & (PLANE_YZ | PLANE_XZ)) != 0);

    /* Z axis if needed */
    if (((rv3d->view == RV3D_VIEW_USER) || (rv3d->persp != RV3D_ORTHO)) && show_axis_z) {
      e_data.zpos_flag = SHOW_AXIS_Z;

      float zvec[3], campos[3];
      negate_v3_v3(zvec, viewinv[2]);
      copy_v3_v3(campos, viewinv[3]);

      /* z axis : chose the most facing plane */
      if (fabsf(zvec[0]) < fabsf(zvec[1])) {
        e_data.zpos_flag |= PLANE_XZ;
      }
      else {
        e_data.zpos_flag |= PLANE_YZ;
      }

      e_data.zneg_flag = e_data.zpos_flag;

      /* Persp : If camera is below floor plane, we switch clipping
       * Ortho : If eye vector is looking up, we switch clipping */
      if (((winmat[3][3] == 0.0f) && (campos[2] > 0.0f)) ||
          ((winmat[3][3] != 0.0f) && (zvec[2] < 0.0f))) {
        e_data.zpos_flag |= CLIP_ZPOS;
        e_data.zneg_flag |= CLIP_ZNEG;
      }
      else {
        e_data.zpos_flag |= CLIP_ZNEG;
        e_data.zneg_flag |= CLIP_ZPOS;
      }

      e_data.zplane_axes[0] = (float)((e_data.zpos_flag & (PLANE_XZ | PLANE_XY)) != 0);
      e_data.zplane_axes[1] = (float)((e_data.zpos_flag & (PLANE_YZ | PLANE_XY)) != 0);
      e_data.zplane_axes[2] = (float)((e_data.zpos_flag & (PLANE_YZ | PLANE_XZ)) != 0);
    }
    else {
      e_data.zneg_flag = e_data.zpos_flag = CLIP_ZNEG | CLIP_ZPOS;
    }

    float dist;
    if (rv3d->persp == RV3D_CAMOB && v3d->camera && v3d->camera->type == OB_CAMERA) {
      Object *camera_object = DEG_get_evaluated_object(draw_ctx->depsgraph, v3d->camera);
      dist = ((Camera *)(camera_object->data))->clip_end;
    }
    else {
      dist = v3d->clip_end;
    }

    e_data.grid_settings[0] = dist / 2.0f;     /* gridDistance */
    e_data.grid_settings[1] = grid_res;        /* gridResolution */
    e_data.grid_settings[2] = grid_scale;      /* gridScale */
    e_data.grid_settings[3] = v3d->gridsubdiv; /* gridSubdiv */
    e_data.grid_settings[4] = (v3d->gridsubdiv > 1) ? 1.0f / logf(v3d->gridsubdiv) :
                                                      0.0f; /* 1/log(gridSubdiv) */

    if (winmat[3][3] == 0.0f) {
      e_data.grid_mesh_size = dist;
    }
    else {
      float viewdist = 1.0f / min_ff(fabsf(winmat[0][0]), fabsf(winmat[1][1]));
      e_data.grid_mesh_size = viewdist * dist;
    }
  }

  copy_v2_v2(e_data.inv_viewport_size, DRW_viewport_size_get());
  invert_v2(e_data.inv_viewport_size);
}

static void OBJECT_engine_free(void)
{
  MEM_SAFE_FREE(e_data.empty_image_format);
  MEM_SAFE_FREE(e_data.empty_image_wire_format);

  for (int sh_data_index = 0; sh_data_index < ARRAY_SIZE(e_data.sh_data); sh_data_index++) {
    OBJECT_Shaders *sh_data = &e_data.sh_data[sh_data_index];
    GPUShader **sh_data_as_array = (GPUShader **)sh_data;
    for (int i = 0; i < (sizeof(OBJECT_Shaders) / sizeof(GPUShader *)); i++) {
      DRW_SHADER_FREE_SAFE(sh_data_as_array[i]);
    }
  }
}

static DRWShadingGroup *shgroup_outline(DRWPass *pass,
                                        const int *ofs,
                                        GPUShader *sh,
                                        eGPUShaderConfig sh_cfg)
{
  DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);
  DRW_shgroup_uniform_int(grp, "baseId", ofs, 1);

  if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
    DRW_shgroup_state_enable(grp, DRW_STATE_CLIP_PLANES);
  }
  return grp;
}

/* currently same as 'shgroup_outline', new function to avoid confustion */
static DRWShadingGroup *shgroup_wire(DRWPass *pass,
                                     const float col[4],
                                     GPUShader *sh,
                                     eGPUShaderConfig sh_cfg)
{
  DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);
  DRW_shgroup_uniform_vec4(grp, "color", col, 1);

  if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
    DRW_shgroup_state_enable(grp, DRW_STATE_CLIP_PLANES);
  }
  return grp;
}

/* currently same as 'shgroup_outline', new function to avoid confustion */
static DRWShadingGroup *shgroup_points(DRWPass *pass,
                                       const float col[4],
                                       GPUShader *sh,
                                       eGPUShaderConfig sh_cfg)
{
  DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);
  DRW_shgroup_uniform_vec4(grp, "color", col, 1);
  DRW_shgroup_uniform_vec4(grp, "innerColor", G_draw.block.colorEditMeshMiddle, 1);

  if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
    DRW_shgroup_state_enable(grp, DRW_STATE_CLIP_PLANES);
  }
  return grp;
}

static int *shgroup_theme_id_to_probe_outline_counter(OBJECT_StorageList *stl,
                                                      int theme_id,
                                                      const int base_flag)
{
  if (UNLIKELY(base_flag & BASE_FROM_DUPLI)) {
    switch (theme_id) {
      case TH_ACTIVE:
      case TH_SELECT:
        return &stl->g_data->id_ofs_prb_select_dupli;
      case TH_TRANSFORM:
      default:
        return &stl->g_data->id_ofs_prb_transform;
    }
  }

  switch (theme_id) {
    case TH_ACTIVE:
      return &stl->g_data->id_ofs_prb_active;
    case TH_SELECT:
      return &stl->g_data->id_ofs_prb_select;
    case TH_TRANSFORM:
    default:
      return &stl->g_data->id_ofs_prb_transform;
  }
}

static int *shgroup_theme_id_to_outline_counter(OBJECT_StorageList *stl,
                                                int theme_id,
                                                const int base_flag)
{
  if (UNLIKELY(base_flag & BASE_FROM_DUPLI)) {
    switch (theme_id) {
      case TH_ACTIVE:
      case TH_SELECT:
        return &stl->g_data->id_ofs_select_dupli;
      case TH_TRANSFORM:
      default:
        return &stl->g_data->id_ofs_transform;
    }
  }

  switch (theme_id) {
    case TH_ACTIVE:
      return &stl->g_data->id_ofs_active;
    case TH_SELECT:
      return &stl->g_data->id_ofs_select;
    case TH_TRANSFORM:
    default:
      return &stl->g_data->id_ofs_transform;
  }
}

static DRWCallBuffer *buffer_theme_id_to_probe_planar_outline_shgrp(OBJECT_StorageList *stl,
                                                                    int theme_id)
{
  /* does not increment counter */
  switch (theme_id) {
    case TH_ACTIVE:
      return stl->g_data->lightprobes_planar_active;
    case TH_SELECT:
      return stl->g_data->lightprobes_planar_select;
    case TH_TRANSFORM:
    default:
      return stl->g_data->lightprobes_planar_transform;
  }
}

static DRWCallBuffer *buffer_theme_id_to_probe_cube_outline_shgrp(OBJECT_StorageList *stl,
                                                                  int theme_id,
                                                                  const int base_flag)
{
  /* does not increment counter */
  if (UNLIKELY(base_flag & BASE_FROM_DUPLI)) {
    switch (theme_id) {
      case TH_ACTIVE:
      case TH_SELECT:
        return stl->g_data->lightprobes_cube_select_dupli;
      case TH_TRANSFORM:
      default:
        return stl->g_data->lightprobes_cube_transform;
    }
  }

  switch (theme_id) {
    case TH_ACTIVE:
      return stl->g_data->lightprobes_cube_active;
    case TH_SELECT:
      return stl->g_data->lightprobes_cube_select;
    case TH_TRANSFORM:
    default:
      return stl->g_data->lightprobes_cube_transform;
  }
}

static DRWShadingGroup *shgroup_theme_id_to_outline_or_null(OBJECT_StorageList *stl,
                                                            int theme_id,
                                                            const int base_flag)
{
  int *counter = shgroup_theme_id_to_outline_counter(stl, theme_id, base_flag);
  *counter += 1;

  if (UNLIKELY(base_flag & BASE_FROM_DUPLI)) {
    switch (theme_id) {
      case TH_ACTIVE:
      case TH_SELECT:
        return stl->g_data->outlines_select_dupli;
      case TH_TRANSFORM:
        return stl->g_data->outlines_transform;
      default:
        return NULL;
    }
  }

  switch (theme_id) {
    case TH_ACTIVE:
      return stl->g_data->outlines_active;
    case TH_SELECT:
      return stl->g_data->outlines_select;
    case TH_TRANSFORM:
      return stl->g_data->outlines_transform;
    default:
      return NULL;
  }
}

static DRWShadingGroup *shgroup_theme_id_to_wire(OBJECT_ShadingGroupList *sgl,
                                                 int theme_id,
                                                 const short base_flag)
{
  if (UNLIKELY(base_flag & BASE_FROM_SET)) {
    return sgl->wire_dupli;
  }
  else if (UNLIKELY(base_flag & BASE_FROM_DUPLI)) {
    switch (theme_id) {
      case TH_ACTIVE:
      case TH_SELECT:
        return sgl->wire_dupli_select;
      case TH_TRANSFORM:
        return sgl->wire_transform;
      default:
        return sgl->wire_dupli;
    }
  }

  switch (theme_id) {
    case TH_ACTIVE:
      return sgl->wire_active;
    case TH_SELECT:
      return sgl->wire_select;
    case TH_TRANSFORM:
      return sgl->wire_transform;
    default:
      return sgl->wire;
  }
}

static DRWShadingGroup *shgroup_theme_id_to_point(OBJECT_ShadingGroupList *sgl,
                                                  int theme_id,
                                                  const short base_flag)
{
  if (UNLIKELY(base_flag & BASE_FROM_SET)) {
    return sgl->points_dupli;
  }
  else if (UNLIKELY(base_flag & BASE_FROM_DUPLI)) {
    switch (theme_id) {
      case TH_ACTIVE:
      case TH_SELECT:
        return sgl->points_dupli_select;
      case TH_TRANSFORM:
        return sgl->points_transform;
      default:
        return sgl->points_dupli;
    }
  }

  switch (theme_id) {
    case TH_ACTIVE:
      return sgl->points_active;
    case TH_SELECT:
      return sgl->points_select;
    case TH_TRANSFORM:
      return sgl->points_transform;
    default:
      return sgl->points;
  }
}

static void image_calc_aspect(Image *ima, const int size[2], float r_image_aspect[2])
{
  float ima_x, ima_y;
  if (ima) {
    ima_x = size[0];
    ima_y = size[1];
  }
  else {
    /* if no image, make it a 1x1 empty square, honor scale & offset */
    ima_x = ima_y = 1.0f;
  }
  /* Get the image aspect even if the buffer is invalid */
  float sca_x = 1.0f, sca_y = 1.0f;
  if (ima) {
    if (ima->aspx > ima->aspy) {
      sca_y = ima->aspy / ima->aspx;
    }
    else if (ima->aspx < ima->aspy) {
      sca_x = ima->aspx / ima->aspy;
    }
  }

  const float scale_x_inv = ima_x * sca_x;
  const float scale_y_inv = ima_y * sca_y;
  if (scale_x_inv > scale_y_inv) {
    r_image_aspect[0] = 1.0f;
    r_image_aspect[1] = scale_y_inv / scale_x_inv;
  }
  else {
    r_image_aspect[0] = scale_x_inv / scale_y_inv;
    r_image_aspect[1] = 1.0f;
  }
}

static void DRW_shgroup_empty_image(OBJECT_Shaders *sh_data,
                                    OBJECT_ShadingGroupList *sgl,
                                    Object *ob,
                                    const float color[3],
                                    RegionView3D *rv3d,
                                    eGPUShaderConfig sh_cfg)
{
  /* TODO: 'StereoViews', see draw_empty_image. */

  if (!BKE_object_empty_image_frame_is_visible_in_view3d(ob, rv3d)) {
    return;
  }

  /* Calling 'BKE_image_get_size' may free the texture. Get the size from 'tex' instead,
   * see: T59347 */
  int size[2] = {0};

  const bool use_alpha_blend = (ob->empty_image_flag & OB_EMPTY_IMAGE_USE_ALPHA_BLEND) != 0;
  GPUTexture *tex = NULL;
  Image *ima = ob->data;

  if (ima != NULL) {
    tex = GPU_texture_from_blender(ima, ob->iuser, GL_TEXTURE_2D);
    if (tex) {
      size[0] = GPU_texture_orig_width(tex);
      size[1] = GPU_texture_orig_height(tex);
    }
  }

  CLAMP_MIN(size[0], 1);
  CLAMP_MIN(size[1], 1);

  float image_aspect[2];
  image_calc_aspect(ob->data, size, image_aspect);

  char depth_mode;
  if (DRW_state_is_depth()) {
    /* Use the actual depth if we are doing depth tests to determine the distance to the object */
    depth_mode = OB_EMPTY_IMAGE_DEPTH_DEFAULT;
  }
  else {
    depth_mode = ob->empty_image_depth;
  }

  {
    DRWShadingGroup *grp = DRW_shgroup_create(sh_data->object_empty_image_wire, sgl->non_meshes);
    DRW_shgroup_uniform_vec2_copy(grp, "aspect", image_aspect);
    DRW_shgroup_uniform_int_copy(grp, "depthMode", depth_mode);
    DRW_shgroup_uniform_float(grp, "size", &ob->empty_drawsize, 1);
    DRW_shgroup_uniform_vec2(grp, "offset", ob->ima_ofs, 1);
    DRW_shgroup_uniform_vec3(grp, "color", color, 1);
    if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
      DRW_shgroup_state_enable(grp, DRW_STATE_CLIP_PLANES);
    }
    DRW_shgroup_call_no_cull(grp, DRW_cache_image_plane_wire_get(), ob);
  }

  if (!BKE_object_empty_image_data_is_visible_in_view3d(ob, rv3d)) {
    return;
  }

  if (tex && ((ob->color[3] > 0.0f) || !use_alpha_blend)) {
    DRWShadingGroup *grp = DRW_shgroup_create(
        sh_data->object_empty_image, (use_alpha_blend) ? sgl->image_empties : sgl->non_meshes);
    DRW_shgroup_uniform_vec2_copy(grp, "aspect", image_aspect);
    DRW_shgroup_uniform_int_copy(grp, "depthMode", depth_mode);
    DRW_shgroup_uniform_float(grp, "size", &ob->empty_drawsize, 1);
    DRW_shgroup_uniform_vec2(grp, "offset", ob->ima_ofs, 1);
    DRW_shgroup_uniform_texture(grp, "image", tex);
    DRW_shgroup_uniform_bool_copy(
        grp, "imagePremultiplied", (ima->alpha_mode == IMA_ALPHA_PREMUL));
    DRW_shgroup_uniform_vec4(grp, "objectColor", ob->color, 1);
    DRW_shgroup_uniform_bool_copy(grp, "useAlphaTest", !use_alpha_blend);
    if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
      DRW_shgroup_state_enable(grp, DRW_STATE_CLIP_PLANES);
    }
    DRW_shgroup_call_no_cull(grp, DRW_cache_image_plane_get(), ob);
  }
}

/* Draw Camera Background Images */
typedef struct CameraEngineData {
  DrawData dd;
  ListBase bg_data;
} CameraEngineData;
typedef struct CameraEngineBGData {
  float transform_mat[4][4];
} CameraEngineBGData;

static void camera_engine_data_free(DrawData *dd)
{
  CameraEngineData *data = (CameraEngineData *)dd;
  for (LinkData *link = data->bg_data.first; link; link = link->next) {
    CameraEngineBGData *bg_data = (CameraEngineBGData *)link->data;
    MEM_freeN(bg_data);
  }
  BLI_freelistN(&data->bg_data);
}

static void camera_background_images_stereo_setup(Scene *scene,
                                                  View3D *v3d,
                                                  Image *ima,
                                                  ImageUser *iuser)
{
  if (BKE_image_is_stereo(ima)) {
    iuser->flag |= IMA_SHOW_STEREO;

    if ((scene->r.scemode & R_MULTIVIEW) == 0) {
      iuser->multiview_eye = STEREO_LEFT_ID;
    }
    else if (v3d->stereo3d_camera != STEREO_3D_ID) {
      /* show only left or right camera */
      iuser->multiview_eye = v3d->stereo3d_camera;
    }

    BKE_image_multiview_index(ima, iuser);
  }
  else {
    iuser->flag &= ~IMA_SHOW_STEREO;
  }
}

static void DRW_shgroup_camera_background_images(OBJECT_Shaders *sh_data,
                                                 OBJECT_PassList *psl,
                                                 Object *ob,
                                                 RegionView3D *rv3d)
{
  if (!BKE_object_empty_image_frame_is_visible_in_view3d(ob, rv3d)) {
    return;
  }

  const DRWContextState *draw_ctx = DRW_context_state_get();
  struct ARegion *ar = draw_ctx->ar;
  View3D *v3d = draw_ctx->v3d;
  Scene *scene = draw_ctx->scene;
  Depsgraph *depsgraph = draw_ctx->depsgraph;
  Camera *cam = ob->data;
  const Object *camera_object = DEG_get_evaluated_object(depsgraph, v3d->camera);
  const bool is_active = (ob == camera_object);
  const bool look_through = (is_active && (rv3d->persp == RV3D_CAMOB));

  if (look_through && (cam->flag & CAM_SHOW_BG_IMAGE)) {
    GPUBatch *batch = DRW_cache_image_plane_get();

    /* load camera engine data */
    CameraEngineData *camera_engine_data = (CameraEngineData *)DRW_drawdata_ensure(
        &ob->id,
        &draw_engine_object_type,
        sizeof(CameraEngineData),
        NULL,
        camera_engine_data_free);
    LinkData *list_node = camera_engine_data->bg_data.first;

    for (CameraBGImage *bgpic = cam->bg_images.first; bgpic; bgpic = bgpic->next) {
      if ((bgpic->flag & CAM_BGIMG_FLAG_DISABLED)) {
        continue;
      }

      /* retrieve the image we want to show, continue to next when no image could be found */
      ImBuf *ibuf = NULL;
      GPUTexture *tex = NULL;
      float image_aspect_x, image_aspect_y;
      float image_aspect = 1.0;
      int image_width, image_height;
      bool premultiplied = false;

      if (bgpic->source == CAM_BGIMG_SOURCE_IMAGE) {
        Image *image = bgpic->ima;
        if (image == NULL) {
          continue;
        }
        premultiplied = (image->alpha_mode == IMA_ALPHA_PREMUL);
        ImageUser *iuser = &bgpic->iuser;
        BKE_image_user_frame_calc(image, iuser, (int)DEG_get_ctime(depsgraph));
        if (image->source == IMA_SRC_SEQUENCE && !(iuser->flag & IMA_USER_FRAME_IN_RANGE)) {
          /* frame is out of range, dont show */
          continue;
        }
        else {
          camera_background_images_stereo_setup(scene, v3d, image, iuser);
        }
        tex = GPU_texture_from_blender(image, iuser, GL_TEXTURE_2D);
        if (tex == NULL) {
          continue;
        }
        ibuf = BKE_image_acquire_ibuf(image, iuser, NULL);
        if (ibuf == NULL) {
          continue;
        }

        image_aspect_x = bgpic->ima->aspx;
        image_aspect_y = bgpic->ima->aspy;

        image_width = ibuf->x;
        image_height = ibuf->y;
        BKE_image_release_ibuf(image, ibuf, NULL);
        image_aspect = (image_width * image_aspect_x) / (image_height * image_aspect_y);
      }
      else if (bgpic->source == CAM_BGIMG_SOURCE_MOVIE) {
        MovieClip *clip = NULL;
        if (bgpic->flag & CAM_BGIMG_FLAG_CAMERACLIP) {
          if (scene->camera) {
            clip = BKE_object_movieclip_get(scene, scene->camera, true);
          }
        }
        else {
          clip = bgpic->clip;
        }

        if (clip == NULL) {
          continue;
        }

        image_aspect_x = clip->aspx;
        image_aspect_y = clip->aspy;

        BKE_movieclip_user_set_frame(&bgpic->cuser, (int)DEG_get_ctime(depsgraph));
        tex = GPU_texture_from_movieclip(clip, &bgpic->cuser, GL_TEXTURE_2D);
        if (tex == NULL) {
          continue;
        }
        BLI_addtail(&e_data.movie_clips, BLI_genericNodeN(clip));
        BKE_movieclip_get_size(clip, &bgpic->cuser, &image_width, &image_height);
        image_aspect = (image_width * image_aspect_x) / (image_height * image_aspect_y);
      }
      else {
        continue;
      }

      /* ensure link_data is allocated to store matrice */
      CameraEngineBGData *bg_data;
      if (list_node != NULL) {
        bg_data = (CameraEngineBGData *)list_node->data;
        list_node = list_node->next;
      }
      else {
        bg_data = MEM_mallocN(sizeof(CameraEngineBGData), __func__);
        BLI_addtail(&camera_engine_data->bg_data, BLI_genericNodeN(bg_data));
      }

      /* calculate the transformation matric for the current bg image */
      float uv2img_space[4][4];
      float img2cam_space[4][4];
      float rot_m4[4][4];
      float scale_m4[4][4];
      float translate_m4[4][4];
      float win_m4_scale[4][4];
      float win_m4_translate[4][4];

      unit_m4(uv2img_space);
      unit_m4(img2cam_space);
      unit_m4(win_m4_scale);
      unit_m4(win_m4_translate);
      unit_m4(scale_m4);
      axis_angle_to_mat4_single(rot_m4, 'Z', bgpic->rotation);
      unit_m4(translate_m4);

      const float *size = DRW_viewport_size_get();
      float camera_aspect_x = 1.0;
      float camera_aspect_y = 1.0;
      float camera_offset_x = 0.0;
      float camera_offset_y = 0.0;
      float camera_aspect = 1.0;
      float camera_width = size[0];
      float camera_height = size[1];

      if (!DRW_state_is_image_render()) {
        rctf render_border;
        ED_view3d_calc_camera_border(scene, depsgraph, ar, v3d, rv3d, &render_border, true);
        camera_width = render_border.xmax - render_border.xmin;
        camera_height = render_border.ymax - render_border.ymin;
        camera_aspect = camera_width / camera_height;
        const float camera_aspect_center_x = (render_border.xmax + render_border.xmin) / 2.0;
        const float camera_aspect_center_y = (render_border.ymax + render_border.ymin) / 2.0;

        camera_aspect_x = camera_width / size[0];
        camera_aspect_y = camera_height / size[1];
        win_m4_scale[0][0] = camera_aspect_x;
        win_m4_scale[1][1] = camera_aspect_y;

        camera_offset_x = (camera_aspect_center_x - (ar->winx / 2.0)) /
                          (0.5 * camera_width / camera_aspect_x);
        camera_offset_y = (camera_aspect_center_y - (ar->winy / 2.0)) /
                          (0.5 * camera_height / camera_aspect_y);
        win_m4_translate[3][0] = camera_offset_x;
        win_m4_translate[3][1] = camera_offset_y;
      }

      /* Convert from uv space to image space -0.5..-.5 */
      uv2img_space[0][0] = image_width;
      uv2img_space[1][1] = image_height;

      img2cam_space[0][0] = (1.0 / image_width);
      img2cam_space[1][1] = (1.0 / image_height);

      /* Update scaling based on image and camera framing */
      float scale_x = bgpic->scale;
      float scale_y = bgpic->scale;

      if (bgpic->flag & CAM_BGIMG_FLAG_CAMERA_ASPECT) {
        float fit_scale = image_aspect / camera_aspect;
        if (bgpic->flag & CAM_BGIMG_FLAG_CAMERA_CROP) {
          if (image_aspect > camera_aspect) {
            scale_x *= fit_scale;
          }
          else {
            scale_y /= fit_scale;
          }
        }
        else {
          if (image_aspect > camera_aspect) {
            scale_y /= fit_scale;
          }
          else {
            scale_x *= fit_scale;
          }
        }
      }

      // scale image to match the desired aspect ratio
      scale_m4[0][0] = scale_x;
      scale_m4[1][1] = scale_y;

      // translate
      translate_m4[3][0] = bgpic->offset[0];
      translate_m4[3][1] = bgpic->offset[1];

      mul_m4_series(bg_data->transform_mat,
                    win_m4_translate,
                    win_m4_scale,
                    translate_m4,
                    img2cam_space,
                    scale_m4,
                    rot_m4,
                    uv2img_space);

      DRWPass *pass = (bgpic->flag & CAM_BGIMG_FLAG_FOREGROUND) ? psl->camera_images_front :
                                                                  psl->camera_images_back;
      GPUShader *shader = DRW_state_do_color_management() ? sh_data->object_camera_image_cm :
                                                            sh_data->object_camera_image;
      DRWShadingGroup *grp = DRW_shgroup_create(shader, pass);

      DRW_shgroup_uniform_float_copy(
          grp, "depth", (bgpic->flag & CAM_BGIMG_FLAG_FOREGROUND) ? 0.000001 : 0.999999);
      DRW_shgroup_uniform_float_copy(grp, "alpha", bgpic->alpha);
      DRW_shgroup_uniform_texture(grp, "image", tex);
      DRW_shgroup_uniform_bool_copy(grp, "imagePremultiplied", premultiplied);

      DRW_shgroup_uniform_float_copy(
          grp, "flipX", (bgpic->flag & CAM_BGIMG_FLAG_FLIP_X) ? -1.0 : 1.0);
      DRW_shgroup_uniform_float_copy(
          grp, "flipY", (bgpic->flag & CAM_BGIMG_FLAG_FLIP_Y) ? -1.0 : 1.0);
      DRW_shgroup_uniform_mat4(grp, "TransformMat", bg_data->transform_mat);

      DRW_shgroup_call(grp, batch, NULL);
    }
  }
}

static void camera_background_images_free_textures(void)
{
  for (LinkData *link = e_data.movie_clips.first; link; link = link->next) {
    MovieClip *clip = (MovieClip *)link->data;
    GPU_free_texture_movieclip(clip);
  }
  BLI_freelistN(&e_data.movie_clips);
}

static void OBJECT_cache_init(void *vedata)
{
  const GlobalsUboStorage *gb = &G_draw.block;
  OBJECT_PassList *psl = ((OBJECT_Data *)vedata)->psl;
  OBJECT_StorageList *stl = ((OBJECT_Data *)vedata)->stl;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  OBJECT_PrivateData *g_data;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  OBJECT_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];

  const float outline_width = UI_GetThemeValuef(TH_OUTLINE_WIDTH);
  const bool do_outline_expand = (U.pixelsize > 1.0) || (outline_width > 2.0f);
  const bool do_large_expand = ((U.pixelsize > 1.0) && (outline_width > 2.0f)) ||
                               (outline_width > 4.0f);

  if (!stl->g_data) {
    /* Alloc transient pointers */
    stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
  }

  g_data = stl->g_data;
  g_data->xray_enabled = XRAY_ACTIVE(draw_ctx->v3d);
  g_data->xray_enabled_and_not_wire = g_data->xray_enabled &&
                                      draw_ctx->v3d->shading.type > OB_WIRE;

  g_data->custom_shapes = BLI_ghash_ptr_new(__func__);

  {
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;
    psl->outlines = DRW_pass_create("Outlines Depth Pass", state);

    GPUShader *sh = sh_data->outline_prepass;

    if (g_data->xray_enabled_and_not_wire) {
      sh = sh_data->outline_prepass_wire;
    }

    g_data->outlines_select = shgroup_outline(
        psl->outlines, &g_data->id_ofs_select, sh, draw_ctx->sh_cfg);
    g_data->outlines_select_dupli = shgroup_outline(
        psl->outlines, &g_data->id_ofs_select_dupli, sh, draw_ctx->sh_cfg);
    g_data->outlines_transform = shgroup_outline(
        psl->outlines, &g_data->id_ofs_transform, sh, draw_ctx->sh_cfg);
    g_data->outlines_active = shgroup_outline(
        psl->outlines, &g_data->id_ofs_active, sh, draw_ctx->sh_cfg);

    g_data->id_ofs_select = 0;
    g_data->id_ofs_select_dupli = 0;
    g_data->id_ofs_active = 0;
    g_data->id_ofs_transform = 0;
  }

  {
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;
    DRWPass *pass = psl->lightprobes = DRW_pass_create("Object Probe Pass", state);
    struct GPUBatch *sphere = DRW_cache_sphere_get();
    struct GPUBatch *quad = DRW_cache_quad_get();

    /* Cubemap */
    g_data->lightprobes_cube_select = buffer_instance_outline(
        pass, sphere, &g_data->id_ofs_prb_select, draw_ctx->sh_cfg);
    g_data->lightprobes_cube_select_dupli = buffer_instance_outline(
        pass, sphere, &g_data->id_ofs_prb_select_dupli, draw_ctx->sh_cfg);
    g_data->lightprobes_cube_active = buffer_instance_outline(
        pass, sphere, &g_data->id_ofs_prb_active, draw_ctx->sh_cfg);
    g_data->lightprobes_cube_transform = buffer_instance_outline(
        pass, sphere, &g_data->id_ofs_prb_transform, draw_ctx->sh_cfg);

    /* Planar */
    g_data->lightprobes_planar_select = buffer_instance_outline(
        pass, quad, &g_data->id_ofs_prb_select, draw_ctx->sh_cfg);
    g_data->lightprobes_planar_select_dupli = buffer_instance_outline(
        pass, quad, &g_data->id_ofs_prb_select_dupli, draw_ctx->sh_cfg);
    g_data->lightprobes_planar_active = buffer_instance_outline(
        pass, quad, &g_data->id_ofs_prb_active, draw_ctx->sh_cfg);
    g_data->lightprobes_planar_transform = buffer_instance_outline(
        pass, quad, &g_data->id_ofs_prb_transform, draw_ctx->sh_cfg);

    g_data->id_ofs_prb_select = 0;
    g_data->id_ofs_prb_select_dupli = 0;
    g_data->id_ofs_prb_active = 0;
    g_data->id_ofs_prb_transform = 0;
  }

  {
    DRWState state = DRW_STATE_WRITE_COLOR;
    struct GPUBatch *quad = DRW_cache_fullscreen_quad_get();
    /* Don't occlude the "outline" detection pass if in xray mode (too much flickering). */
    float alphaOcclu = (g_data->xray_enabled) ? 1.0f : 0.35f;

    psl->outlines_search = DRW_pass_create("Outlines Detect Pass", state);

    GPUShader *sh = (g_data->xray_enabled_and_not_wire) ? sh_data->outline_detect_wire :
                                                          sh_data->outline_detect;
    DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->outlines_search);
    DRW_shgroup_uniform_texture_ref(grp, "outlineId", &e_data.outlines_id_tx);
    DRW_shgroup_uniform_texture_ref(grp, "outlineDepth", &e_data.outlines_depth_tx);
    DRW_shgroup_uniform_texture_ref(grp, "sceneDepth", &dtxl->depth);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
    DRW_shgroup_uniform_float_copy(grp, "alphaOcclu", alphaOcclu);
    DRW_shgroup_uniform_int(grp, "idOffsets", &stl->g_data->id_ofs_active, 4);
    DRW_shgroup_call(grp, quad, NULL);

    /* This is the bleed pass if do_outline_expand is false. */
    GPUShader *fade_sh = (do_large_expand) ? sh_data->outline_fade_large : sh_data->outline_fade;
    psl->outlines_expand = DRW_pass_create("Outlines Expand Pass", state);

    grp = DRW_shgroup_create(fade_sh, psl->outlines_expand);
    DRW_shgroup_uniform_texture_ref(grp, "outlineColor", &e_data.outlines_blur_tx);
    DRW_shgroup_uniform_bool_copy(grp, "doExpand", do_outline_expand);
    DRW_shgroup_call(grp, quad, NULL);

    psl->outlines_bleed = DRW_pass_create("Outlines Bleed Pass", state);

    if (do_outline_expand) {
      grp = DRW_shgroup_create(sh_data->outline_fade, psl->outlines_bleed);
      DRW_shgroup_uniform_texture_ref(grp, "outlineColor", &e_data.outlines_color_tx);
      DRW_shgroup_uniform_bool_copy(grp, "doExpand", false);
      DRW_shgroup_call(grp, quad, NULL);
    }
  }

  {
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA;
    psl->outlines_resolve = DRW_pass_create("Outlines Resolve Pass", state);

    struct GPUBatch *quad = DRW_cache_fullscreen_quad_get();
    GPUTexture **outline_tx = (do_outline_expand) ? &e_data.outlines_blur_tx :
                                                    &e_data.outlines_color_tx;

    DRWShadingGroup *grp = DRW_shgroup_create(sh_data->outline_resolve_aa, psl->outlines_resolve);
    DRW_shgroup_uniform_texture_ref(grp, "outlineBluredColor", outline_tx);
    DRW_shgroup_uniform_vec2(grp, "rcpDimensions", e_data.inv_viewport_size, 1);
    DRW_shgroup_call(grp, quad, NULL);
  }

  {
    /* Grid pass */
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA;
    psl->grid = DRW_pass_create("Infinite Grid Pass", state);

    struct GPUBatch *geom = DRW_cache_grid_get();
    float grid_line_size = max_ff(0.0f, U.pixelsize - 1.0f) * 0.5f;

    /* Create 3 quads to render ordered transparency Z axis */
    DRWShadingGroup *grp = DRW_shgroup_create(sh_data->grid, psl->grid);
    DRW_shgroup_uniform_int(grp, "gridFlag", &e_data.zneg_flag, 1);
    DRW_shgroup_uniform_vec3(grp, "planeAxes", e_data.zplane_axes, 1);
    DRW_shgroup_uniform_vec4(grp, "gridSettings", e_data.grid_settings, 1);
    DRW_shgroup_uniform_float_copy(grp, "lineKernel", grid_line_size);
    DRW_shgroup_uniform_float_copy(grp, "meshSize", e_data.grid_mesh_size);
    DRW_shgroup_uniform_float(grp, "gridOneOverLogSubdiv", &e_data.grid_settings[4], 1);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
    DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &dtxl->depth);
    DRW_shgroup_call(grp, geom, NULL);

    grp = DRW_shgroup_create(sh_data->grid, psl->grid);
    DRW_shgroup_uniform_int(grp, "gridFlag", &e_data.grid_flag, 1);
    DRW_shgroup_uniform_vec3(grp, "planeAxes", e_data.grid_axes, 1);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
    DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &dtxl->depth);
    DRW_shgroup_call(grp, geom, NULL);

    grp = DRW_shgroup_create(sh_data->grid, psl->grid);
    DRW_shgroup_uniform_int(grp, "gridFlag", &e_data.zpos_flag, 1);
    DRW_shgroup_uniform_vec3(grp, "planeAxes", e_data.zplane_axes, 1);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
    DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &dtxl->depth);
    DRW_shgroup_call(grp, geom, NULL);
  }

  /* Camera background images */
  {
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA;
    psl->camera_images_back = DRW_pass_create("Camera Images Back", state);
    psl->camera_images_front = DRW_pass_create("Camera Images Front", state);
  }

  for (int i = 0; i < 2; ++i) {
    OBJECT_ShadingGroupList *sgl = (i == 1) ? &stl->g_data->sgl_ghost : &stl->g_data->sgl;

    /* Solid bones */
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;
    sgl->bone_solid = psl->bone_solid[i] = DRW_pass_create("Bone Solid Pass", state);
    sgl->bone_outline = psl->bone_outline[i] = DRW_pass_create("Bone Outline Pass", state);

    /* Wire bones */
    state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
            DRW_STATE_BLEND_ALPHA;
    sgl->bone_wire = psl->bone_wire[i] = DRW_pass_create("Bone Wire Pass", state);

    /* distance outline around envelope bones */
    state = DRW_STATE_BLEND_ADD | DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL |
            DRW_STATE_CULL_FRONT;
    sgl->bone_envelope = psl->bone_envelope[i] = DRW_pass_create("Bone Envelope Outline Pass",
                                                                 state);

    state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;
    sgl->bone_axes = psl->bone_axes[i] = DRW_pass_create("Bone Axes Pass", state);
  }

  for (int i = 0; i < 2; ++i) {
    OBJECT_ShadingGroupList *sgl = (i == 1) ? &stl->g_data->sgl_ghost : &stl->g_data->sgl;

    /* Non Meshes Pass (Camera, empties, lights ...) */
    struct GPUBatch *geom;
    struct GPUShader *sh;

    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
                     DRW_STATE_BLEND_ALPHA;
    sgl->non_meshes = psl->non_meshes[i] = DRW_pass_create("Non Meshes Pass", state);

    state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA;
    sgl->image_empties = psl->image_empties[i] = DRW_pass_create("Image Empties", state);

    /* Empties */
    empties_callbuffers_create(sgl->non_meshes, &sgl->empties, draw_ctx->sh_cfg);

    /* Force Field */
    geom = DRW_cache_field_wind_get();
    sgl->field_wind = buffer_instance_scaled(sgl->non_meshes, geom, draw_ctx->sh_cfg);

    geom = DRW_cache_field_force_get();
    sgl->field_force = buffer_instance_screen_aligned(sgl->non_meshes, geom, draw_ctx->sh_cfg);

    geom = DRW_cache_field_vortex_get();
    sgl->field_vortex = buffer_instance_scaled(sgl->non_meshes, geom, draw_ctx->sh_cfg);

    geom = DRW_cache_screenspace_circle_get();
    sgl->field_curve_sta = buffer_instance_screen_aligned(sgl->non_meshes, geom, draw_ctx->sh_cfg);

    /* Grease Pencil */
    geom = DRW_cache_gpencil_axes_get();
    sgl->gpencil_axes = buffer_instance(sgl->non_meshes, geom, draw_ctx->sh_cfg);

    /* Speaker */
    geom = DRW_cache_speaker_get();
    sgl->speaker = buffer_instance(sgl->non_meshes, geom, draw_ctx->sh_cfg);

    /* Probe */
    static float probeSize = 14.0f;
    geom = DRW_cache_lightprobe_cube_get();
    sgl->probe_cube = buffer_instance_screenspace(
        sgl->non_meshes, geom, &probeSize, draw_ctx->sh_cfg);

    geom = DRW_cache_lightprobe_grid_get();
    sgl->probe_grid = buffer_instance_screenspace(
        sgl->non_meshes, geom, &probeSize, draw_ctx->sh_cfg);

    static float probePlanarSize = 20.0f;
    geom = DRW_cache_lightprobe_planar_get();
    sgl->probe_planar = buffer_instance_screenspace(
        sgl->non_meshes, geom, &probePlanarSize, draw_ctx->sh_cfg);

    /* Camera */
    geom = DRW_cache_camera_get();
    sgl->camera = buffer_camera_instance(sgl->non_meshes, geom, draw_ctx->sh_cfg);

    geom = DRW_cache_camera_frame_get();
    sgl->camera_frame = buffer_camera_instance(sgl->non_meshes, geom, draw_ctx->sh_cfg);

    geom = DRW_cache_camera_tria_get();
    sgl->camera_tria = buffer_camera_instance(sgl->non_meshes, geom, draw_ctx->sh_cfg);

    geom = DRW_cache_plain_axes_get();
    sgl->camera_focus = buffer_instance(sgl->non_meshes, geom, draw_ctx->sh_cfg);

    geom = DRW_cache_single_line_get();
    sgl->camera_clip = buffer_distance_lines_instance(sgl->non_meshes, geom, draw_ctx->sh_cfg);
    sgl->camera_mist = buffer_distance_lines_instance(sgl->non_meshes, geom, draw_ctx->sh_cfg);

    geom = DRW_cache_single_line_endpoints_get();
    sgl->camera_clip_points = buffer_distance_lines_instance(
        sgl->non_meshes, geom, draw_ctx->sh_cfg);
    sgl->camera_mist_points = buffer_distance_lines_instance(
        sgl->non_meshes, geom, draw_ctx->sh_cfg);

    geom = DRW_cache_quad_wires_get();
    sgl->camera_stereo_plane_wires = buffer_instance(sgl->non_meshes, geom, draw_ctx->sh_cfg);

    geom = DRW_cache_empty_cube_get();
    sgl->camera_stereo_volume_wires = buffer_instance(sgl->non_meshes, geom, draw_ctx->sh_cfg);

    BLI_listbase_clear(&sgl->camera_path);

    /* Texture Space */
    geom = DRW_cache_empty_cube_get();
    sgl->texspace = buffer_instance(sgl->non_meshes, geom, draw_ctx->sh_cfg);

    /* Wires (for loose edges) */
    sh = GPU_shader_get_builtin_shader_with_config(GPU_SHADER_3D_UNIFORM_COLOR, draw_ctx->sh_cfg);
    sgl->wire = shgroup_wire(sgl->non_meshes, gb->colorWire, sh, draw_ctx->sh_cfg);
    sgl->wire_select = shgroup_wire(sgl->non_meshes, gb->colorSelect, sh, draw_ctx->sh_cfg);
    sgl->wire_transform = shgroup_wire(sgl->non_meshes, gb->colorTransform, sh, draw_ctx->sh_cfg);
    sgl->wire_active = shgroup_wire(sgl->non_meshes, gb->colorActive, sh, draw_ctx->sh_cfg);
    /* Wire (duplicator) */
    sgl->wire_dupli = shgroup_wire(sgl->non_meshes, gb->colorDupli, sh, draw_ctx->sh_cfg);
    sgl->wire_dupli_select = shgroup_wire(
        sgl->non_meshes, gb->colorDupliSelect, sh, draw_ctx->sh_cfg);

    /* Points (loose points) */
    sh = sh_data->loose_points;
    sgl->points = shgroup_points(sgl->non_meshes, gb->colorWire, sh, draw_ctx->sh_cfg);
    sgl->points_select = shgroup_points(sgl->non_meshes, gb->colorSelect, sh, draw_ctx->sh_cfg);
    sgl->points_transform = shgroup_points(
        sgl->non_meshes, gb->colorTransform, sh, draw_ctx->sh_cfg);
    sgl->points_active = shgroup_points(sgl->non_meshes, gb->colorActive, sh, draw_ctx->sh_cfg);
    /* Points (duplicator) */
    sgl->points_dupli = shgroup_points(sgl->non_meshes, gb->colorDupli, sh, draw_ctx->sh_cfg);
    sgl->points_dupli_select = shgroup_points(
        sgl->non_meshes, gb->colorDupliSelect, sh, draw_ctx->sh_cfg);
    DRW_shgroup_state_disable(sgl->points, DRW_STATE_BLEND_ALPHA);
    DRW_shgroup_state_disable(sgl->points_select, DRW_STATE_BLEND_ALPHA);
    DRW_shgroup_state_disable(sgl->points_transform, DRW_STATE_BLEND_ALPHA);
    DRW_shgroup_state_disable(sgl->points_active, DRW_STATE_BLEND_ALPHA);
    DRW_shgroup_state_disable(sgl->points_dupli, DRW_STATE_BLEND_ALPHA);
    DRW_shgroup_state_disable(sgl->points_dupli_select, DRW_STATE_BLEND_ALPHA);

    /* Metaballs Handles */
    sgl->mball_handle = buffer_instance_mball_handles(sgl->non_meshes, draw_ctx->sh_cfg);

    /* Lights */
    /* TODO
     * for now we create multiple times the same VBO with only light center coordinates
     * but ideally we would only create it once */

    sh = GPU_shader_get_builtin_shader_with_config(
        GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA, draw_ctx->sh_cfg);

    DRWShadingGroup *grp = DRW_shgroup_create(sh, sgl->non_meshes);
    DRW_shgroup_uniform_vec4(grp, "color", gb->colorLightNoAlpha, 1);
    DRW_shgroup_uniform_float(grp, "size", &gb->sizeLightCenter, 1);
    if (draw_ctx->sh_cfg == GPU_SHADER_CFG_CLIPPED) {
      DRW_shgroup_state_enable(grp, DRW_STATE_CLIP_PLANES);
    }

    sgl->light_center = buffer_dynpoints_uniform_color(grp);

    geom = DRW_cache_single_line_get();
    sgl->light_buflimit = buffer_distance_lines_instance(sgl->non_meshes, geom, draw_ctx->sh_cfg);

    geom = DRW_cache_light_get();
    sgl->light_circle = buffer_instance_screenspace(
        sgl->non_meshes, geom, &gb->sizeLightCircle, draw_ctx->sh_cfg);
    geom = DRW_cache_light_shadows_get();
    sgl->light_circle_shadow = buffer_instance_screenspace(
        sgl->non_meshes, geom, &gb->sizeLightCircleShadow, draw_ctx->sh_cfg);

    geom = DRW_cache_light_sunrays_get();
    sgl->light_sunrays = buffer_instance_screenspace(
        sgl->non_meshes, geom, &gb->sizeLightCircle, draw_ctx->sh_cfg);

    sgl->light_groundline = buffer_groundlines_uniform_color(
        sgl->non_meshes, gb->colorLight, draw_ctx->sh_cfg);
    sgl->light_groundpoint = buffer_groundpoints_uniform_color(
        sgl->non_meshes, gb->colorLight, draw_ctx->sh_cfg);

    geom = DRW_cache_screenspace_circle_get();
    sgl->light_area_sphere = buffer_instance_screen_aligned(
        sgl->non_meshes, geom, draw_ctx->sh_cfg);

    geom = DRW_cache_light_area_square_get();
    sgl->light_area_square = buffer_instance(sgl->non_meshes, geom, draw_ctx->sh_cfg);

    geom = DRW_cache_light_area_disk_get();
    sgl->light_area_disk = buffer_instance(sgl->non_meshes, geom, draw_ctx->sh_cfg);

    geom = DRW_cache_light_hemi_get();
    sgl->light_hemi = buffer_instance(sgl->non_meshes, geom, draw_ctx->sh_cfg);

    geom = DRW_cache_single_line_get();
    sgl->light_distance = buffer_distance_lines_instance(sgl->non_meshes, geom, draw_ctx->sh_cfg);

    geom = DRW_cache_single_line_endpoints_get();
    sgl->light_buflimit_points = buffer_distance_lines_instance(
        sgl->non_meshes, geom, draw_ctx->sh_cfg);

    geom = DRW_cache_light_spot_get();
    sgl->light_spot_cone = buffer_spot_instance(sgl->non_meshes, geom, draw_ctx->sh_cfg);

    geom = DRW_cache_circle_get();
    sgl->light_spot_blend = buffer_instance(sgl->non_meshes, geom, draw_ctx->sh_cfg);

    geom = DRW_cache_light_spot_square_get();
    sgl->light_spot_pyramid = buffer_instance(sgl->non_meshes, geom, draw_ctx->sh_cfg);

    geom = DRW_cache_square_get();
    sgl->light_spot_blend_rect = buffer_instance(sgl->non_meshes, geom, draw_ctx->sh_cfg);

    /* -------- STIPPLES ------- */

    /* Relationship Lines */
    sgl->relationship_lines = buffer_dynlines_dashed_uniform_color(
        sgl->non_meshes, gb->colorWire, draw_ctx->sh_cfg);
    sgl->constraint_lines = buffer_dynlines_dashed_uniform_color(
        sgl->non_meshes, gb->colorGridAxisZ, draw_ctx->sh_cfg);

    /* Force Field Curve Guide End (here because of stipple) */
    /* TODO port to shader stipple */
    geom = DRW_cache_screenspace_circle_get();
    sgl->field_curve_end = buffer_instance_screen_aligned(sgl->non_meshes, geom, draw_ctx->sh_cfg);

    /* Force Field Limits */
    /* TODO port to shader stipple */
    geom = DRW_cache_field_tube_limit_get();
    sgl->field_tube_limit = buffer_instance_scaled(sgl->non_meshes, geom, draw_ctx->sh_cfg);

    /* TODO port to shader stipple */
    geom = DRW_cache_field_cone_limit_get();
    sgl->field_cone_limit = buffer_instance_scaled(sgl->non_meshes, geom, draw_ctx->sh_cfg);

    /* Transparent Shapes */
    state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA |
            DRW_STATE_CULL_FRONT;
    sgl->transp_shapes = psl->transp_shapes[i] = DRW_pass_create("Transparent Shapes", state);

    sh = GPU_shader_get_builtin_shader_with_config(
        GPU_SHADER_INSTANCE_VARIYING_COLOR_VARIYING_SIZE, draw_ctx->sh_cfg);

    DRWShadingGroup *grp_transp = DRW_shgroup_create(sh, sgl->transp_shapes);
    if (draw_ctx->sh_cfg == GPU_SHADER_CFG_CLIPPED) {
      DRW_shgroup_state_enable(grp_transp, DRW_STATE_CLIP_PLANES);
    }

    DRWShadingGroup *grp_cull_back = DRW_shgroup_create_sub(grp_transp);
    DRW_shgroup_state_disable(grp_cull_back, DRW_STATE_CULL_FRONT);
    DRW_shgroup_state_enable(grp_cull_back, DRW_STATE_CULL_BACK);

    DRWShadingGroup *grp_cull_none = DRW_shgroup_create_sub(grp_transp);
    DRW_shgroup_state_disable(grp_cull_none, DRW_STATE_CULL_FRONT);

    /* Spot cones */
    geom = DRW_cache_light_spot_volume_get();
    sgl->light_spot_volume = buffer_instance_alpha(grp_transp, geom);

    geom = DRW_cache_light_spot_square_volume_get();
    sgl->light_spot_volume_rect = buffer_instance_alpha(grp_transp, geom);

    geom = DRW_cache_light_spot_volume_get();
    sgl->light_spot_volume_outside = buffer_instance_alpha(grp_cull_back, geom);

    geom = DRW_cache_light_spot_square_volume_get();
    sgl->light_spot_volume_rect_outside = buffer_instance_alpha(grp_cull_back, geom);

    /* Camera stereo volumes */
    geom = DRW_cache_cube_get();
    sgl->camera_stereo_volume = buffer_instance_alpha(grp_transp, geom);

    geom = DRW_cache_quad_get();
    sgl->camera_stereo_plane = buffer_instance_alpha(grp_cull_none, geom);
  }

  {
    /* Object Center pass grouped by State */
    DRWShadingGroup *grp;
    static float outlineWidth, size;

    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA;
    psl->ob_center = DRW_pass_create("Obj Center Pass", state);

    outlineWidth = 1.0f * U.pixelsize;
    size = UI_GetThemeValuef(TH_OBCENTER_DIA) * U.pixelsize + outlineWidth;

    GPUShader *sh = GPU_shader_get_builtin_shader_with_config(
        GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_OUTLINE_AA, draw_ctx->sh_cfg);

    /* Active */
    grp = DRW_shgroup_create(sh, psl->ob_center);
    DRW_shgroup_uniform_float(grp, "size", &size, 1);
    DRW_shgroup_uniform_float(grp, "outlineWidth", &outlineWidth, 1);
    DRW_shgroup_uniform_vec4(grp, "color", gb->colorActive, 1);
    DRW_shgroup_uniform_vec4(grp, "outlineColor", gb->colorOutline, 1);
    if (draw_ctx->sh_cfg == GPU_SHADER_CFG_CLIPPED) {
      DRW_shgroup_state_enable(grp, DRW_STATE_CLIP_PLANES);
    }
    /* TODO find better name. */
    stl->g_data->center_active = buffer_dynpoints_uniform_color(grp);

    /* Select */
    grp = DRW_shgroup_create_sub(grp);
    DRW_shgroup_uniform_vec4(grp, "color", gb->colorSelect, 1);
    stl->g_data->center_selected = buffer_dynpoints_uniform_color(grp);

    /* Deselect */
    grp = DRW_shgroup_create_sub(grp);
    DRW_shgroup_uniform_vec4(grp, "color", gb->colorDeselect, 1);
    stl->g_data->center_deselected = buffer_dynpoints_uniform_color(grp);

    /* Select (library) */
    grp = DRW_shgroup_create_sub(grp);
    DRW_shgroup_uniform_vec4(grp, "color", gb->colorLibrarySelect, 1);
    stl->g_data->center_selected_lib = buffer_dynpoints_uniform_color(grp);

    /* Deselect (library) */
    grp = DRW_shgroup_create_sub(grp);
    DRW_shgroup_uniform_vec4(grp, "color", gb->colorLibrary, 1);
    stl->g_data->center_deselected_lib = buffer_dynpoints_uniform_color(grp);
  }

  {
    /* Particle Pass */
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
                     DRW_STATE_BLEND_ALPHA;
    psl->particle = DRW_pass_create("Particle Pass", state);
  }
}

static void DRW_shgroup_mball_handles(OBJECT_ShadingGroupList *sgl,
                                      Object *ob,
                                      ViewLayer *view_layer)
{
  MetaBall *mb = ob->data;

  float *color;
  DRW_object_wire_theme_get(ob, view_layer, &color);

  float draw_scale_xform[3][4]; /* Matrix of Scale and Translation */
  {
    float scamat[3][3];
    copy_m3_m4(scamat, ob->obmat);
    /* Get the normalized inverse matrix to extract only
     * the scale of Scamat */
    float iscamat[3][3];
    invert_m3_m3(iscamat, scamat);
    normalize_m3(iscamat);
    mul_m3_m3_post(scamat, iscamat);

    copy_v3_v3(draw_scale_xform[0], scamat[0]);
    copy_v3_v3(draw_scale_xform[1], scamat[1]);
    copy_v3_v3(draw_scale_xform[2], scamat[2]);
  }

  for (MetaElem *ml = mb->elems.first; ml != NULL; ml = ml->next) {
    /* draw radius */
    float world_pos[3];
    mul_v3_m4v3(world_pos, ob->obmat, &ml->x);
    draw_scale_xform[0][3] = world_pos[0];
    draw_scale_xform[1][3] = world_pos[1];
    draw_scale_xform[2][3] = world_pos[2];

    DRW_buffer_add_entry(sgl->mball_handle, draw_scale_xform, &ml->rad, color);
  }
}

static void DRW_shgroup_light(OBJECT_ShadingGroupList *sgl, Object *ob, ViewLayer *view_layer)
{
  Light *la = ob->data;
  float *color;
  int theme_id = DRW_object_wire_theme_get(ob, view_layer, &color);
  static float zero = 0.0f;

  typedef struct LightEngineData {
    DrawData dd;
    float shape_mat[4][4];
    float spot_blend_mat[4][4];
  } LightEngineData;

  LightEngineData *light_engine_data = (LightEngineData *)DRW_drawdata_ensure(
      &ob->id, &draw_engine_object_type, sizeof(LightEngineData), NULL, NULL);

  float(*shapemat)[4] = light_engine_data->shape_mat;
  float(*spotblendmat)[4] = light_engine_data->spot_blend_mat;

  if ((ob->base_flag & (BASE_FROM_SET | BASE_FROM_DUPLI)) == 0) {
    /* Don't draw the center if it's selected or active */
    if (theme_id == TH_LIGHT) {
      DRW_buffer_add_entry(sgl->light_center, ob->obmat[3]);
    }
  }

  /* First circle */
  DRW_buffer_add_entry(sgl->light_circle, ob->obmat[3], color);

  /* draw dashed outer circle for shadow */
  DRW_buffer_add_entry(sgl->light_circle_shadow, ob->obmat[3], color);

  /* Distance */
  if (ELEM(la->type, LA_SUN, LA_AREA)) {
    DRW_buffer_add_entry(sgl->light_distance, color, &zero, &la->dist, ob->obmat);
  }

  copy_m4_m4(shapemat, ob->obmat);

  if (la->type == LA_SUN) {
    DRW_buffer_add_entry(sgl->light_sunrays, ob->obmat[3], color);
  }
  else if (la->type == LA_SPOT) {
    float size[3], sizemat[4][4];
    static float one = 1.0f;
    float cone_inside[4] = {0.0f, 0.0f, 0.0f, 0.5f};
    float cone_outside[4] = {1.0f, 1.0f, 1.0f, 0.3f};
    float blend = 1.0f - pow2f(la->spotblend);
    size[0] = size[1] = sinf(la->spotsize * 0.5f) * la->dist;
    size[2] = cosf(la->spotsize * 0.5f) * la->dist;

    size_to_mat4(sizemat, size);
    mul_m4_m4m4(shapemat, ob->obmat, sizemat);

    size[0] = size[1] = blend;
    size[2] = 1.0f;
    size_to_mat4(sizemat, size);
    translate_m4(sizemat, 0.0f, 0.0f, -1.0f);
    rotate_m4(sizemat, 'X', (float)(M_PI / 2));
    mul_m4_m4m4(spotblendmat, shapemat, sizemat);

    if (la->mode & LA_SQUARE) {
      DRW_buffer_add_entry(sgl->light_spot_pyramid, color, &one, shapemat);

      /* hide line if it is zero size or overlaps with outer border,
       * previously it adjusted to always to show it but that seems
       * confusing because it doesn't show the actual blend size */
      if (blend != 0.0f && blend != 1.0f) {
        DRW_buffer_add_entry(sgl->light_spot_blend_rect, color, &one, spotblendmat);
      }

      if (la->mode & LA_SHOW_CONE) {

        DRW_buffer_add_entry(sgl->light_spot_volume_rect, cone_inside, &one, shapemat);
        DRW_buffer_add_entry(sgl->light_spot_volume_rect_outside, cone_outside, &one, shapemat);
      }
    }
    else {
      DRW_buffer_add_entry(sgl->light_spot_cone, color, shapemat);

      /* hide line if it is zero size or overlaps with outer border,
       * previously it adjusted to always to show it but that seems
       * confusing because it doesn't show the actual blend size */
      if (blend != 0.0f && blend != 1.0f) {
        DRW_buffer_add_entry(sgl->light_spot_blend, color, &one, spotblendmat);
      }

      if (la->mode & LA_SHOW_CONE) {
        DRW_buffer_add_entry(sgl->light_spot_volume, cone_inside, &one, shapemat);
        DRW_buffer_add_entry(sgl->light_spot_volume_outside, cone_outside, &one, shapemat);
      }
    }

    DRW_buffer_add_entry(sgl->light_buflimit, color, &la->clipsta, &la->clipend, ob->obmat);
    DRW_buffer_add_entry(sgl->light_buflimit_points, color, &la->clipsta, &la->clipend, ob->obmat);
  }
  else if (la->type == LA_AREA) {
    float size[3] = {1.0f, 1.0f, 1.0f}, sizemat[4][4];

    if (ELEM(la->area_shape, LA_AREA_RECT, LA_AREA_ELLIPSE)) {
      size[1] = la->area_sizey / la->area_size;
      size_to_mat4(sizemat, size);
      mul_m4_m4m4(shapemat, shapemat, sizemat);
    }

    if (ELEM(la->area_shape, LA_AREA_DISK, LA_AREA_ELLIPSE)) {
      DRW_buffer_add_entry(sgl->light_area_disk, color, &la->area_size, shapemat);
    }
    else {
      DRW_buffer_add_entry(sgl->light_area_square, color, &la->area_size, shapemat);
    }
  }

  if (ELEM(la->type, LA_LOCAL, LA_SPOT)) {
    /* We only want position not scale. */
    shapemat[0][0] = shapemat[1][1] = shapemat[2][2] = 1.0f;
    shapemat[0][1] = shapemat[0][2] = 0.0f;
    shapemat[1][0] = shapemat[1][2] = 0.0f;
    shapemat[2][0] = shapemat[2][1] = 0.0f;
    DRW_buffer_add_entry(sgl->light_area_sphere, color, &la->area_size, shapemat);
  }

  /* Line and point going to the ground */
  DRW_buffer_add_entry(sgl->light_groundline, ob->obmat[3]);
  DRW_buffer_add_entry(sgl->light_groundpoint, ob->obmat[3]);
}

static GPUBatch *batch_camera_path_get(ListBase *camera_paths,
                                       const MovieTrackingReconstruction *reconstruction)
{
  GPUBatch *geom;
  static GPUVertFormat format = {0};
  static struct {
    uint pos;
  } attr_id;
  if (format.attr_len == 0) {
    attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  }
  GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
  GPU_vertbuf_data_alloc(vbo, reconstruction->camnr);

  MovieReconstructedCamera *camera = reconstruction->cameras;
  for (int a = 0; a < reconstruction->camnr; a++, camera++) {
    GPU_vertbuf_attr_set(vbo, attr_id.pos, a, camera->mat[3]);
  }

  geom = GPU_batch_create_ex(GPU_PRIM_LINE_STRIP, vbo, NULL, GPU_BATCH_OWNS_VBO);

  /* Store the batch to do cleanup after drawing. */
  BLI_addtail(camera_paths, BLI_genericNodeN(geom));
  return geom;
}

static void batch_camera_path_free(ListBase *camera_paths)
{
  LinkData *link;
  while ((link = BLI_pophead(camera_paths))) {
    GPUBatch *camera_path = link->data;
    GPU_batch_discard(camera_path);
    MEM_freeN(link);
  }
}

/**
 * Draw the stereo 3d support elements (cameras, plane, volume).
 * They are only visible when not looking through the camera:
 */
static void camera_view3d_stereoscopy_display_extra(OBJECT_ShadingGroupList *sgl,
                                                    Scene *scene,
                                                    ViewLayer *view_layer,
                                                    View3D *v3d,
                                                    Object *ob,
                                                    Camera *cam,
                                                    const float vec[4][3],
                                                    float drawsize,
                                                    const float scale[3])
{
  const bool is_select = DRW_state_is_select();
  static float drw_tria_dummy[2][2][2] = {{{0}}};
  const float fac = (cam->stereo.pivot == CAM_S3D_PIVOT_CENTER) ? 2.0f : 1.0f;
  float origin[2][3] = {{0}};
  const char *viewnames[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};

  const bool is_stereo3d_cameras = (v3d->stereo3d_flag & V3D_S3D_DISPCAMERAS) &&
                                   (scene->r.views_format == SCE_VIEWS_FORMAT_STEREO_3D);
  const bool is_stereo3d_plane = (v3d->stereo3d_flag & V3D_S3D_DISPPLANE) &&
                                 (scene->r.views_format == SCE_VIEWS_FORMAT_STEREO_3D);
  const bool is_stereo3d_volume = (v3d->stereo3d_flag & V3D_S3D_DISPVOLUME);

  float *color;
  DRW_object_wire_theme_get(ob, view_layer, &color);

  for (int eye = 0; eye < 2; eye++) {
    float obmat[4][4];
    ob = BKE_camera_multiview_render(scene, ob, viewnames[eye]);

    BKE_camera_multiview_model_matrix_scaled(&scene->r, ob, viewnames[eye], obmat);

    copy_v2_v2(cam->runtime.drw_corners[eye][0], vec[0]);
    copy_v2_v2(cam->runtime.drw_corners[eye][1], vec[1]);
    copy_v2_v2(cam->runtime.drw_corners[eye][2], vec[2]);
    copy_v2_v2(cam->runtime.drw_corners[eye][3], vec[3]);

    cam->runtime.drw_depth[eye] = vec[0][2];

    if (cam->stereo.convergence_mode == CAM_S3D_OFFAXIS) {
      const float shift_x = ((BKE_camera_multiview_shift_x(&scene->r, ob, viewnames[eye]) -
                              cam->shiftx) *
                             (drawsize * scale[0] * fac));

      for (int i = 0; i < 4; i++) {
        cam->runtime.drw_corners[eye][i][0] += shift_x;
      }
    }

    /* Dummy triangle, draw on top of existent lines so it is invisible. */
    copy_v2_v2(drw_tria_dummy[eye][0], cam->runtime.drw_corners[eye][0]);
    copy_v2_v2(drw_tria_dummy[eye][1], cam->runtime.drw_corners[eye][0]);

    if (is_stereo3d_cameras) {
      DRW_buffer_add_entry(sgl->camera_frame,
                           color,
                           cam->runtime.drw_corners[eye],
                           &cam->runtime.drw_depth[eye],
                           cam->runtime.drw_tria,
                           obmat);

      DRW_buffer_add_entry(sgl->camera,
                           color,
                           cam->runtime.drw_corners[eye],
                           &cam->runtime.drw_depth[eye],
                           drw_tria_dummy[eye],
                           obmat);
    }

    /* Connecting line. */
    mul_m4_v3(obmat, origin[eye]);
  }

  /* Draw connecting lines. */
  if (is_stereo3d_cameras) {
    DRW_buffer_add_entry(sgl->relationship_lines, origin[0]);
    DRW_buffer_add_entry(sgl->relationship_lines, origin[1]);
  }

  /* Draw convergence plane. */
  if (is_stereo3d_plane && !is_select) {
    float convergence_plane[4][2];
    const float offset = cam->stereo.convergence_distance / cam->runtime.drw_depth[0];

    for (int i = 0; i < 4; i++) {
      mid_v2_v2v2(
          convergence_plane[i], cam->runtime.drw_corners[0][i], cam->runtime.drw_corners[1][i]);
      mul_v2_fl(convergence_plane[i], offset);
    }

    /* We are using a -1,1 quad for this shading group, so we need to
     * scale and transform it to match the convergence plane border. */
    static float one = 1.0f;
    float plane_mat[4][4], scale_mat[4][4];
    float scale_factor[3] = {1.0f, 1.0f, 1.0f};
    float color_plane[2][4] = {
        {0.0f, 0.0f, 0.0f, v3d->stereo3d_convergence_alpha},
        {0.0f, 0.0f, 0.0f, 1.0f},
    };

    const float height = convergence_plane[1][1] - convergence_plane[0][1];
    const float width = convergence_plane[2][0] - convergence_plane[0][0];

    scale_factor[0] = width * 0.5f;
    scale_factor[1] = height * 0.5f;

    copy_m4_m4(plane_mat, cam->runtime.drw_normalmat);
    translate_m4(plane_mat, 0.0f, 0.0f, -cam->stereo.convergence_distance);
    size_to_mat4(scale_mat, scale_factor);
    mul_m4_m4_post(plane_mat, scale_mat);
    translate_m4(plane_mat, 2.0f * cam->shiftx, (width / height) * 2.0f * cam->shifty, 0.0f);

    if (v3d->stereo3d_convergence_alpha > 0.0f) {
      DRW_buffer_add_entry(sgl->camera_stereo_plane, color_plane[0], &one, plane_mat);
    }
    DRW_buffer_add_entry(sgl->camera_stereo_plane_wires, color_plane[1], &one, plane_mat);
  }

  /* Draw convergence volume. */
  if (is_stereo3d_volume && !is_select) {
    static float one = 1.0f;
    float color_volume[3][4] = {
        {0.0f, 1.0f, 1.0f, v3d->stereo3d_volume_alpha},
        {1.0f, 0.0f, 0.0f, v3d->stereo3d_volume_alpha},
        {0.0f, 0.0f, 0.0f, 1.0f},
    };

    for (int eye = 0; eye < 2; eye++) {
      float winmat[4][4], viewinv[4][4], viewmat[4][4], persmat[4][4], persinv[4][4];
      ob = BKE_camera_multiview_render(scene, ob, viewnames[eye]);

      BKE_camera_multiview_window_matrix(&scene->r, ob, viewnames[eye], winmat);
      BKE_camera_multiview_model_matrix(&scene->r, ob, viewnames[eye], viewinv);

      invert_m4_m4(viewmat, viewinv);
      mul_m4_m4m4(persmat, winmat, viewmat);
      invert_m4_m4(persinv, persmat);

      if (v3d->stereo3d_volume_alpha > 0.0f) {
        DRW_buffer_add_entry(sgl->camera_stereo_volume, color_volume[eye], &one, persinv);
      }
      DRW_buffer_add_entry(sgl->camera_stereo_volume_wires, color_volume[2], &one, persinv);
    }
  }
}

static void camera_view3d_reconstruction(OBJECT_ShadingGroupList *sgl,
                                         Scene *scene,
                                         View3D *v3d,
                                         Object *camera_object,
                                         Object *ob,
                                         const float color[4],
                                         const bool is_select)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Camera *cam = ob->data;
  const Object *orig_camera_object = DEG_get_original_object(camera_object);

  if ((v3d->flag2 & V3D_SHOW_RECONSTRUCTION) == 0) {
    return;
  }

  MovieClip *clip = BKE_object_movieclip_get(scene, ob, false);
  if (clip == NULL) {
    return;
  }

  BLI_assert(BLI_listbase_is_empty(&sgl->camera_path));
  const bool is_solid_bundle = (v3d->bundle_drawtype == OB_EMPTY_SPHERE) &&
                               ((v3d->shading.type != OB_SOLID) || !XRAY_FLAG_ENABLED(v3d));

  MovieTracking *tracking = &clip->tracking;
  /* Index must start in 1, to mimic BKE_tracking_track_get_indexed. */
  int track_index = 1;

  uchar text_color_selected[4], text_color_unselected[4];
  float bundle_color_unselected[4], bundle_color_solid[4];

  UI_GetThemeColor4ubv(TH_SELECT, text_color_selected);
  UI_GetThemeColor4ubv(TH_TEXT, text_color_unselected);
  UI_GetThemeColor4fv(TH_WIRE, bundle_color_unselected);
  UI_GetThemeColor4fv(TH_BUNDLE_SOLID, bundle_color_solid);

  float camera_mat[4][4];
  BKE_tracking_get_camera_object_matrix(scene, ob, camera_mat);

  float bundle_scale_mat[4][4];
  if (is_solid_bundle) {
    scale_m4_fl(bundle_scale_mat, v3d->bundle_size);
  }

  for (MovieTrackingObject *tracking_object = tracking->objects.first; tracking_object != NULL;
       tracking_object = tracking_object->next) {
    float tracking_object_mat[4][4];

    if (tracking_object->flag & TRACKING_OBJECT_CAMERA) {
      copy_m4_m4(tracking_object_mat, camera_mat);
    }
    else {
      const int framenr = BKE_movieclip_remap_scene_to_clip_frame(
          clip, DEG_get_ctime(draw_ctx->depsgraph));
      float object_mat[4][4];
      BKE_tracking_camera_get_reconstructed_interpolate(
          tracking, tracking_object, framenr, object_mat);

      invert_m4(object_mat);
      mul_m4_m4m4(tracking_object_mat, cam->runtime.drw_normalmat, object_mat);
    }

    ListBase *tracksbase = BKE_tracking_object_get_tracks(tracking, tracking_object);
    for (MovieTrackingTrack *track = tracksbase->first; track; track = track->next) {

      if ((track->flag & TRACK_HAS_BUNDLE) == 0) {
        continue;
      }

      bool is_selected = TRACK_SELECTED(track);

      float bundle_mat[4][4];
      copy_m4_m4(bundle_mat, tracking_object_mat);
      translate_m4(bundle_mat, track->bundle_pos[0], track->bundle_pos[1], track->bundle_pos[2]);

      const float *bundle_color;
      if (track->flag & TRACK_CUSTOMCOLOR) {
        bundle_color = track->color;
      }
      else if (is_solid_bundle) {
        bundle_color = bundle_color_solid;
      }
      else if (is_selected) {
        bundle_color = color;
      }
      else {
        bundle_color = bundle_color_unselected;
      }

      if (is_select) {
        DRW_select_load_id(orig_camera_object->runtime.select_id | (track_index << 16));
        track_index++;
      }

      if (is_solid_bundle) {

        if (is_selected) {
          DRW_shgroup_empty_ex(sgl, bundle_mat, &v3d->bundle_size, v3d->bundle_drawtype, color);
        }

        float bundle_color_v4[4] = {
            bundle_color[0],
            bundle_color[1],
            bundle_color[2],
            1.0f,
        };

        mul_m4_m4m4(bundle_mat, bundle_mat, bundle_scale_mat);
        DRW_buffer_add_entry(sgl->empties.sphere_solid, bundle_mat, bundle_color_v4);
      }
      else {
        DRW_shgroup_empty_ex(
            sgl, bundle_mat, &v3d->bundle_size, v3d->bundle_drawtype, bundle_color);
      }

      if ((v3d->flag2 & V3D_SHOW_BUNDLENAME) && !is_select) {
        struct DRWTextStore *dt = DRW_text_cache_ensure();

        DRW_text_cache_add(dt,
                           bundle_mat[3],
                           track->name,
                           strlen(track->name),
                           10,
                           0,
                           DRW_TEXT_CACHE_GLOBALSPACE | DRW_TEXT_CACHE_STRING_PTR,
                           is_selected ? text_color_selected : text_color_unselected);
      }
    }

    if ((v3d->flag2 & V3D_SHOW_CAMERAPATH) && (tracking_object->flag & TRACKING_OBJECT_CAMERA) &&
        !is_select) {
      MovieTrackingReconstruction *reconstruction;
      reconstruction = BKE_tracking_object_get_reconstruction(tracking, tracking_object);

      if (reconstruction->camnr) {
        static float camera_path_color[4];
        UI_GetThemeColor4fv(TH_CAMERA_PATH, camera_path_color);

        GPUBatch *geom = batch_camera_path_get(&sgl->camera_path, reconstruction);
        GPUShader *shader = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);
        DRWShadingGroup *shading_group = DRW_shgroup_create(shader, sgl->non_meshes);
        DRW_shgroup_uniform_vec4(shading_group, "color", camera_path_color, 1);
        DRW_shgroup_call_obmat(shading_group, geom, camera_mat);
      }
    }
  }
}

static void DRW_shgroup_camera(OBJECT_ShadingGroupList *sgl, Object *ob, ViewLayer *view_layer)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  View3D *v3d = draw_ctx->v3d;
  Scene *scene = draw_ctx->scene;
  RegionView3D *rv3d = draw_ctx->rv3d;

  Camera *cam = ob->data;
  Object *camera_object = DEG_get_evaluated_object(draw_ctx->depsgraph, v3d->camera);
  const bool is_select = DRW_state_is_select();
  const bool is_active = (ob == camera_object);
  const bool look_through = (is_active && (rv3d->persp == RV3D_CAMOB));

  const bool is_multiview = (scene->r.scemode & R_MULTIVIEW) != 0;
  const bool is_stereo3d_view = (scene->r.views_format == SCE_VIEWS_FORMAT_STEREO_3D);
  const bool is_stereo3d_display_extra = is_active && is_multiview && (!look_through) &&
                                         ((v3d->stereo3d_flag) != 0);
  const bool is_stereo3d_cameras = (ob == scene->camera) && is_multiview && is_stereo3d_view &&
                                   (v3d->stereo3d_flag & V3D_S3D_DISPCAMERAS);
  const bool is_selection_camera_stereo = is_select && look_through && is_multiview &&
                                          is_stereo3d_view;

  float *color;
  DRW_object_wire_theme_get(ob, view_layer, &color);

  float vec[4][3], asp[2], shift[2], scale[3], drawsize;

  /* BKE_camera_multiview_model_matrix already accounts for scale, don't do it here. */
  if (is_selection_camera_stereo) {
    scale[0] = 1.0f;
    scale[1] = 1.0f;
    scale[2] = 1.0f;
  }
  else {
    scale[0] = 1.0f / len_v3(ob->obmat[0]);
    scale[1] = 1.0f / len_v3(ob->obmat[1]);
    scale[2] = 1.0f / len_v3(ob->obmat[2]);
  }

  BKE_camera_view_frame_ex(
      scene, cam, cam->drawsize, look_through, scale, asp, shift, &drawsize, vec);

  /* Frame coords */
  copy_v2_v2(cam->runtime.drw_corners[0][0], vec[0]);
  copy_v2_v2(cam->runtime.drw_corners[0][1], vec[1]);
  copy_v2_v2(cam->runtime.drw_corners[0][2], vec[2]);
  copy_v2_v2(cam->runtime.drw_corners[0][3], vec[3]);

  /* depth */
  cam->runtime.drw_depth[0] = vec[0][2];

  /* tria */
  cam->runtime.drw_tria[0][0] = shift[0] + ((0.7f * drawsize) * scale[0]);
  cam->runtime.drw_tria[0][1] = shift[1] + ((drawsize * (asp[1] + 0.1f)) * scale[1]);
  cam->runtime.drw_tria[1][0] = shift[0];
  cam->runtime.drw_tria[1][1] = shift[1] + ((1.1f * drawsize * (asp[1] + 0.7f)) * scale[1]);

  if (look_through) {
    if (!DRW_state_is_image_render()) {
      /* Only draw the frame. */
      float mat[4][4];
      if (is_multiview) {
        const bool is_left = v3d->multiview_eye == STEREO_LEFT_ID;
        const char *view_name = is_left ? STEREO_LEFT_NAME : STEREO_RIGHT_NAME;
        BKE_camera_multiview_model_matrix(&scene->r, ob, view_name, mat);
        const float shiftx = BKE_camera_multiview_shift_x(&scene->r, ob, view_name);
        const float delta_shiftx = shiftx - cam->shiftx;
        const float width = cam->runtime.drw_corners[0][2][0] - cam->runtime.drw_corners[0][0][0];
        for (int i = 0; i < 4; i++) {
          cam->runtime.drw_corners[0][i][0] -= delta_shiftx * width;
        }
      }
      else {
        copy_m4_m4(mat, ob->obmat);
      }

      DRW_buffer_add_entry(sgl->camera_frame,
                           color,
                           cam->runtime.drw_corners[0],
                           &cam->runtime.drw_depth[0],
                           cam->runtime.drw_tria,
                           mat);
    }
  }
  else {
    if (!is_stereo3d_cameras) {
      DRW_buffer_add_entry(sgl->camera,
                           color,
                           cam->runtime.drw_corners[0],
                           &cam->runtime.drw_depth[0],
                           cam->runtime.drw_tria,
                           ob->obmat);
    }

    /* Active cam */
    if (is_active) {
      DRW_buffer_add_entry(sgl->camera_tria,
                           color,
                           cam->runtime.drw_corners[0],
                           &cam->runtime.drw_depth[0],
                           cam->runtime.drw_tria,
                           ob->obmat);
    }
  }

  /* draw the rest in normalize object space */
  normalize_m4_m4(cam->runtime.drw_normalmat, ob->obmat);

  if (cam->flag & CAM_SHOWLIMITS) {
    static float col[4] = {0.5f, 0.5f, 0.25f, 1.0f}, col_hi[4] = {1.0f, 1.0f, 0.5f, 1.0f};
    float sizemat[4][4], size[3] = {1.0f, 1.0f, 0.0f};
    float focusdist = BKE_camera_object_dof_distance(ob);

    copy_m4_m4(cam->runtime.drw_focusmat, cam->runtime.drw_normalmat);
    translate_m4(cam->runtime.drw_focusmat, 0.0f, 0.0f, -focusdist);
    size_to_mat4(sizemat, size);
    mul_m4_m4m4(cam->runtime.drw_focusmat, cam->runtime.drw_focusmat, sizemat);

    DRW_buffer_add_entry(
        sgl->camera_focus, (is_active ? col_hi : col), &cam->drawsize, cam->runtime.drw_focusmat);

    DRW_buffer_add_entry(
        sgl->camera_clip, color, &cam->clip_start, &cam->clip_end, cam->runtime.drw_normalmat);
    DRW_buffer_add_entry(sgl->camera_clip_points,
                         (is_active ? col_hi : col),
                         &cam->clip_start,
                         &cam->clip_end,
                         cam->runtime.drw_normalmat);
  }

  if (cam->flag & CAM_SHOWMIST) {
    World *world = scene->world;

    if (world) {
      static float col[4] = {0.5f, 0.5f, 0.5f, 1.0f}, col_hi[4] = {1.0f, 1.0f, 1.0f, 1.0f};
      world->mistend = world->miststa + world->mistdist;
      DRW_buffer_add_entry(
          sgl->camera_mist, color, &world->miststa, &world->mistend, cam->runtime.drw_normalmat);
      DRW_buffer_add_entry(sgl->camera_mist_points,
                           (is_active ? col_hi : col),
                           &world->miststa,
                           &world->mistend,
                           cam->runtime.drw_normalmat);
    }
  }

  /* Stereo cameras, volumes, plane drawing. */
  if (is_stereo3d_display_extra) {
    camera_view3d_stereoscopy_display_extra(
        sgl, scene, view_layer, v3d, ob, cam, vec, drawsize, scale);
  }

  /* Motion Tracking. */
  camera_view3d_reconstruction(sgl, scene, v3d, camera_object, ob, color, is_select);
}

static void DRW_shgroup_empty_ex(OBJECT_ShadingGroupList *sgl,
                                 const float mat[4][4],
                                 const float *draw_size,
                                 char draw_type,
                                 const float color[4])
{
  DRWEmptiesBufferList *buffers = &sgl->empties;
  switch (draw_type) {
    case OB_PLAINAXES:
      DRW_buffer_add_entry(buffers->plain_axes, color, draw_size, mat);
      break;
    case OB_SINGLE_ARROW:
      DRW_buffer_add_entry(buffers->single_arrow, color, draw_size, mat);
      DRW_buffer_add_entry(buffers->single_arrow_line, color, draw_size, mat);
      break;
    case OB_CUBE:
      DRW_buffer_add_entry(buffers->cube, color, draw_size, mat);
      break;
    case OB_CIRCLE:
      DRW_buffer_add_entry(buffers->circle, color, draw_size, mat);
      break;
    case OB_EMPTY_SPHERE:
      DRW_buffer_add_entry(buffers->sphere, color, draw_size, mat);
      break;
    case OB_EMPTY_CONE:
      DRW_buffer_add_entry(buffers->cone, color, draw_size, mat);
      break;
    case OB_ARROWS:
      DRW_buffer_add_entry(buffers->empty_axes, color, draw_size, mat);
      break;
    case OB_EMPTY_IMAGE:
      BLI_assert(!"Should never happen, use DRW_shgroup_empty instead.");
      break;
  }
}

static void DRW_shgroup_empty(OBJECT_Shaders *sh_data,
                              OBJECT_ShadingGroupList *sgl,
                              Object *ob,
                              ViewLayer *view_layer,
                              RegionView3D *rv3d,
                              eGPUShaderConfig sh_cfg)
{
  float *color;
  DRW_object_wire_theme_get(ob, view_layer, &color);

  switch (ob->empty_drawtype) {
    case OB_PLAINAXES:
    case OB_SINGLE_ARROW:
    case OB_CUBE:
    case OB_CIRCLE:
    case OB_EMPTY_SPHERE:
    case OB_EMPTY_CONE:
    case OB_ARROWS:
      DRW_shgroup_empty_ex(sgl, ob->obmat, &ob->empty_drawsize, ob->empty_drawtype, color);
      break;
    case OB_EMPTY_IMAGE:
      DRW_shgroup_empty_image(sh_data, sgl, ob, color, rv3d, sh_cfg);
      break;
  }
}

static void DRW_shgroup_forcefield(OBJECT_ShadingGroupList *sgl, Object *ob, ViewLayer *view_layer)
{
  int theme_id = DRW_object_wire_theme_get(ob, view_layer, NULL);
  float *color = DRW_color_background_blend_get(theme_id);
  PartDeflect *pd = ob->pd;
  Curve *cu = (ob->type == OB_CURVE) ? ob->data : NULL;

  /* TODO Move this to depsgraph */
  float tmp[3];
  copy_v3_fl(pd->drawvec1, ob->empty_drawsize);

  switch (pd->forcefield) {
    case PFIELD_WIND:
      pd->drawvec1[2] = pd->f_strength;
      break;
    case PFIELD_VORTEX:
      if (pd->f_strength < 0.0f) {
        pd->drawvec1[1] = -pd->drawvec1[1];
      }
      break;
    case PFIELD_GUIDE:
      if (cu && (cu->flag & CU_PATH) && ob->runtime.curve_cache->path &&
          ob->runtime.curve_cache->path->data) {
        where_on_path(ob, 0.0f, pd->drawvec1, tmp, NULL, NULL, NULL);
        where_on_path(ob, 1.0f, pd->drawvec2, tmp, NULL, NULL, NULL);
      }
      break;
  }

  if (pd->falloff == PFIELD_FALL_TUBE) {
    pd->drawvec_falloff_max[0] = pd->drawvec_falloff_max[1] = (pd->flag & PFIELD_USEMAXR) ?
                                                                  pd->maxrad :
                                                                  1.0f;
    pd->drawvec_falloff_max[2] = (pd->flag & PFIELD_USEMAX) ? pd->maxdist : 0.0f;

    pd->drawvec_falloff_min[0] = pd->drawvec_falloff_min[1] = (pd->flag & PFIELD_USEMINR) ?
                                                                  pd->minrad :
                                                                  1.0f;
    pd->drawvec_falloff_min[2] = (pd->flag & PFIELD_USEMIN) ? pd->mindist : 0.0f;
  }
  else if (pd->falloff == PFIELD_FALL_CONE) {
    float radius, distance;

    radius = DEG2RADF((pd->flag & PFIELD_USEMAXR) ? pd->maxrad : 1.0f);
    distance = (pd->flag & PFIELD_USEMAX) ? pd->maxdist : 0.0f;
    pd->drawvec_falloff_max[0] = pd->drawvec_falloff_max[1] = distance * sinf(radius);
    pd->drawvec_falloff_max[2] = distance * cosf(radius);

    radius = DEG2RADF((pd->flag & PFIELD_USEMINR) ? pd->minrad : 1.0f);
    distance = (pd->flag & PFIELD_USEMIN) ? pd->mindist : 0.0f;

    pd->drawvec_falloff_min[0] = pd->drawvec_falloff_min[1] = distance * sinf(radius);
    pd->drawvec_falloff_min[2] = distance * cosf(radius);
  }
  /* End of things that should go to depthgraph */

  switch (pd->forcefield) {
    case PFIELD_WIND:
      DRW_buffer_add_entry(sgl->field_wind, color, &pd->drawvec1, ob->obmat);
      break;
    case PFIELD_FORCE:
      DRW_buffer_add_entry(sgl->field_force, color, &pd->drawvec1, ob->obmat);
      break;
    case PFIELD_VORTEX:
      DRW_buffer_add_entry(sgl->field_vortex, color, &pd->drawvec1, ob->obmat);
      break;
    case PFIELD_GUIDE:
      if (cu && (cu->flag & CU_PATH) && ob->runtime.curve_cache->path &&
          ob->runtime.curve_cache->path->data) {
        DRW_buffer_add_entry(sgl->field_curve_sta, color, &pd->f_strength, ob->obmat);
        DRW_buffer_add_entry(sgl->field_curve_end, color, &pd->f_strength, ob->obmat);
      }
      break;
  }

  if (pd->falloff == PFIELD_FALL_SPHERE) {
    /* as last, guide curve alters it */
    if ((pd->flag & PFIELD_USEMAX) != 0) {
      DRW_buffer_add_entry(sgl->field_curve_end, color, &pd->maxdist, ob->obmat);
    }

    if ((pd->flag & PFIELD_USEMIN) != 0) {
      DRW_buffer_add_entry(sgl->field_curve_end, color, &pd->mindist, ob->obmat);
    }
  }
  else if (pd->falloff == PFIELD_FALL_TUBE) {
    if (pd->flag & (PFIELD_USEMAX | PFIELD_USEMAXR)) {
      DRW_buffer_add_entry(sgl->field_tube_limit, color, &pd->drawvec_falloff_max, ob->obmat);
    }

    if (pd->flag & (PFIELD_USEMIN | PFIELD_USEMINR)) {
      DRW_buffer_add_entry(sgl->field_tube_limit, color, &pd->drawvec_falloff_min, ob->obmat);
    }
  }
  else if (pd->falloff == PFIELD_FALL_CONE) {
    if (pd->flag & (PFIELD_USEMAX | PFIELD_USEMAXR)) {
      DRW_buffer_add_entry(sgl->field_cone_limit, color, &pd->drawvec_falloff_max, ob->obmat);
    }

    if (pd->flag & (PFIELD_USEMIN | PFIELD_USEMINR)) {
      DRW_buffer_add_entry(sgl->field_cone_limit, color, &pd->drawvec_falloff_min, ob->obmat);
    }
  }
}

static void DRW_shgroup_volume_extra(OBJECT_ShadingGroupList *sgl,
                                     Object *ob,
                                     ViewLayer *view_layer,
                                     Scene *scene,
                                     ModifierData *md)
{
  SmokeModifierData *smd = (SmokeModifierData *)md;
  SmokeDomainSettings *sds = smd->domain;
  float *color;
  float one = 1.0f;

  if (sds == NULL) {
    return;
  }

  DRW_object_wire_theme_get(ob, view_layer, &color);

  /* Small cube showing voxel size. */
  float voxel_cubemat[4][4] = {{0.0f}};
  voxel_cubemat[0][0] = 1.0f / (float)sds->res[0];
  voxel_cubemat[1][1] = 1.0f / (float)sds->res[1];
  voxel_cubemat[2][2] = 1.0f / (float)sds->res[2];
  voxel_cubemat[3][0] = voxel_cubemat[3][1] = voxel_cubemat[3][2] = -1.0f;
  voxel_cubemat[3][3] = 1.0f;
  translate_m4(voxel_cubemat, 1.0f, 1.0f, 1.0f);
  mul_m4_m4m4(voxel_cubemat, ob->obmat, voxel_cubemat);

  DRW_buffer_add_entry(sgl->empties.cube, color, &one, voxel_cubemat);

  /* Don't show smoke before simulation starts, this could be made an option in the future. */
  if (!sds->draw_velocity || !sds->fluid || CFRA < sds->point_cache[0]->startframe) {
    return;
  }

  const bool use_needle = (sds->vector_draw_type == VECTOR_DRAW_NEEDLE);
  int line_count = (use_needle) ? 6 : 1;
  int slice_axis = -1;
  line_count *= sds->res[0] * sds->res[1] * sds->res[2];

  if (sds->slice_method == MOD_SMOKE_SLICE_AXIS_ALIGNED &&
      sds->axis_slice_method == AXIS_SLICE_SINGLE) {
    float viewinv[4][4];
    DRW_view_viewmat_get(NULL, viewinv, true);

    const int axis = (sds->slice_axis == SLICE_AXIS_AUTO) ? axis_dominant_v3_single(viewinv[2]) :
                                                            sds->slice_axis - 1;
    slice_axis = axis;
    line_count /= sds->res[axis];
  }

  GPU_create_smoke_velocity(smd);

  DRWShadingGroup *grp = DRW_shgroup_create(volume_velocity_shader_get(use_needle),
                                            sgl->non_meshes);
  DRW_shgroup_uniform_texture(grp, "velocityX", sds->tex_velocity_x);
  DRW_shgroup_uniform_texture(grp, "velocityY", sds->tex_velocity_y);
  DRW_shgroup_uniform_texture(grp, "velocityZ", sds->tex_velocity_z);
  DRW_shgroup_uniform_float_copy(grp, "displaySize", sds->vector_scale);
  DRW_shgroup_uniform_float_copy(grp, "slicePosition", sds->slice_depth);
  DRW_shgroup_uniform_int_copy(grp, "sliceAxis", slice_axis);
  DRW_shgroup_call_procedural_lines(grp, ob, line_count);

  BLI_addtail(&e_data.smoke_domains, BLI_genericNodeN(smd));
}

static void volumes_free_smoke_textures(void)
{
  /* Free Smoke Textures after rendering */
  /* XXX This is a waste of processing and GPU bandwidth if nothing
   * is updated. But the problem is since Textures are stored in the
   * modifier we don't want them to take precious VRAM if the
   * modifier is not used for display. We should share them for
   * all viewport in a redraw at least. */
  for (LinkData *link = e_data.smoke_domains.first; link; link = link->next) {
    SmokeModifierData *smd = (SmokeModifierData *)link->data;
    GPU_free_smoke_velocity(smd);
  }
  BLI_freelistN(&e_data.smoke_domains);
}

static void DRW_shgroup_speaker(OBJECT_ShadingGroupList *sgl, Object *ob, ViewLayer *view_layer)
{
  float *color;
  static float one = 1.0f;
  DRW_object_wire_theme_get(ob, view_layer, &color);

  DRW_buffer_add_entry(sgl->speaker, color, &one, ob->obmat);
}

typedef struct OBJECT_LightProbeEngineData {
  DrawData dd;

  float increment_x[3];
  float increment_y[3];
  float increment_z[3];
  float corner[3];
} OBJECT_LightProbeEngineData;

static void DRW_shgroup_lightprobe(OBJECT_Shaders *sh_data,
                                   OBJECT_StorageList *stl,
                                   OBJECT_PassList *psl,
                                   Object *ob,
                                   ViewLayer *view_layer,
                                   const eGPUShaderConfig sh_cfg)
{
  float *color;
  static float one = 1.0f;
  LightProbe *prb = (LightProbe *)ob->data;
  bool do_outlines = ((ob->base_flag & BASE_SELECTED) != 0);
  int theme_id = DRW_object_wire_theme_get(ob, view_layer, &color);

  OBJECT_ShadingGroupList *sgl = (ob->dtx & OB_DRAWXRAY) ? &stl->g_data->sgl_ghost :
                                                           &stl->g_data->sgl;

  OBJECT_LightProbeEngineData *prb_data = (OBJECT_LightProbeEngineData *)DRW_drawdata_ensure(
      &ob->id, &draw_engine_object_type, sizeof(OBJECT_LightProbeEngineData), NULL, NULL);

  if (DRW_state_is_select() || do_outlines) {
    int *call_id = shgroup_theme_id_to_probe_outline_counter(stl, theme_id, ob->base_flag);

    if (prb->type == LIGHTPROBE_TYPE_GRID) {
      /* Update transforms */
      float cell_dim[3], half_cell_dim[3];
      cell_dim[0] = 2.0f / (float)(prb->grid_resolution_x);
      cell_dim[1] = 2.0f / (float)(prb->grid_resolution_y);
      cell_dim[2] = 2.0f / (float)(prb->grid_resolution_z);

      mul_v3_v3fl(half_cell_dim, cell_dim, 0.5f);

      /* First cell. */
      copy_v3_fl(prb_data->corner, -1.0f);
      add_v3_v3(prb_data->corner, half_cell_dim);
      mul_m4_v3(ob->obmat, prb_data->corner);

      /* Opposite neighbor cell. */
      copy_v3_fl3(prb_data->increment_x, cell_dim[0], 0.0f, 0.0f);
      add_v3_v3(prb_data->increment_x, half_cell_dim);
      add_v3_fl(prb_data->increment_x, -1.0f);
      mul_m4_v3(ob->obmat, prb_data->increment_x);
      sub_v3_v3(prb_data->increment_x, prb_data->corner);

      copy_v3_fl3(prb_data->increment_y, 0.0f, cell_dim[1], 0.0f);
      add_v3_v3(prb_data->increment_y, half_cell_dim);
      add_v3_fl(prb_data->increment_y, -1.0f);
      mul_m4_v3(ob->obmat, prb_data->increment_y);
      sub_v3_v3(prb_data->increment_y, prb_data->corner);

      copy_v3_fl3(prb_data->increment_z, 0.0f, 0.0f, cell_dim[2]);
      add_v3_v3(prb_data->increment_z, half_cell_dim);
      add_v3_fl(prb_data->increment_z, -1.0f);
      mul_m4_v3(ob->obmat, prb_data->increment_z);
      sub_v3_v3(prb_data->increment_z, prb_data->corner);

      uint cell_count = prb->grid_resolution_x * prb->grid_resolution_y * prb->grid_resolution_z;
      DRWShadingGroup *grp = DRW_shgroup_create(sh_data->lightprobe_grid, psl->lightprobes);
      DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
      DRW_shgroup_uniform_int_copy(grp, "call_id", *call_id);
      DRW_shgroup_uniform_int(grp, "baseId", call_id, 1); /* that's correct */
      DRW_shgroup_uniform_vec3(grp, "corner", prb_data->corner, 1);
      DRW_shgroup_uniform_vec3(grp, "increment_x", prb_data->increment_x, 1);
      DRW_shgroup_uniform_vec3(grp, "increment_y", prb_data->increment_y, 1);
      DRW_shgroup_uniform_vec3(grp, "increment_z", prb_data->increment_z, 1);
      DRW_shgroup_uniform_ivec3(grp, "grid_resolution", &prb->grid_resolution_x, 1);
      if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
        DRW_shgroup_state_enable(grp, DRW_STATE_CLIP_PLANES);
      }
      DRW_shgroup_call_procedural_points(grp, NULL, cell_count);
      *call_id += 1;
    }
    else if (prb->type == LIGHTPROBE_TYPE_CUBE) {
      float draw_size = 1.0f;
      float probe_cube_mat[4][4];
      // prb_data->draw_size = prb->data_draw_size * 0.1f;
      // unit_m4(prb_data->probe_cube_mat);
      // copy_v3_v3(prb_data->probe_cube_mat[3], ob->obmat[3]);

      DRWCallBuffer *buf = buffer_theme_id_to_probe_cube_outline_shgrp(
          stl, theme_id, ob->base_flag);
      /* TODO remove or change the drawing of the cube probes. Theses line draws nothing on purpose
       * to keep the call ids correct. */
      zero_m4(probe_cube_mat);
      DRW_buffer_add_entry(buf, call_id, &draw_size, probe_cube_mat);
      *call_id += 1;
    }
    else if (prb->flag & LIGHTPROBE_FLAG_SHOW_DATA) {
      float draw_size = 1.0f;
      DRWCallBuffer *buf = buffer_theme_id_to_probe_planar_outline_shgrp(stl, theme_id);
      DRW_buffer_add_entry(buf, call_id, &draw_size, ob->obmat);
      *call_id += 1;
    }
  }

  switch (prb->type) {
    case LIGHTPROBE_TYPE_PLANAR:
      DRW_buffer_add_entry(sgl->probe_planar, ob->obmat[3], color);
      break;
    case LIGHTPROBE_TYPE_GRID:
      DRW_buffer_add_entry(sgl->probe_grid, ob->obmat[3], color);
      break;
    case LIGHTPROBE_TYPE_CUBE:
    default:
      DRW_buffer_add_entry(sgl->probe_cube, ob->obmat[3], color);
      break;
  }

  if (prb->type == LIGHTPROBE_TYPE_PLANAR) {
    float mat[4][4];
    copy_m4_m4(mat, ob->obmat);
    normalize_m4(mat);

    DRW_buffer_add_entry(sgl->empties.single_arrow, color, &ob->empty_drawsize, mat);
    DRW_buffer_add_entry(sgl->empties.single_arrow_line, color, &ob->empty_drawsize, mat);

    copy_m4_m4(mat, ob->obmat);
    zero_v3(mat[2]);

    DRW_buffer_add_entry(sgl->empties.cube, color, &one, mat);
  }

  if ((prb->flag & LIGHTPROBE_FLAG_SHOW_INFLUENCE) != 0) {

    prb->distfalloff = (1.0f - prb->falloff) * prb->distinf;
    prb->distgridinf = prb->distinf;

    if (prb->type == LIGHTPROBE_TYPE_GRID) {
      prb->distfalloff += 1.0f;
      prb->distgridinf += 1.0f;
    }

    if (prb->type == LIGHTPROBE_TYPE_GRID || prb->attenuation_type == LIGHTPROBE_SHAPE_BOX) {
      DRW_buffer_add_entry(sgl->empties.cube, color, &prb->distgridinf, ob->obmat);
      DRW_buffer_add_entry(sgl->empties.cube, color, &prb->distfalloff, ob->obmat);
    }
    else if (prb->type == LIGHTPROBE_TYPE_PLANAR) {
      float rangemat[4][4];
      copy_m4_m4(rangemat, ob->obmat);
      normalize_v3(rangemat[2]);
      mul_v3_fl(rangemat[2], prb->distinf);

      DRW_buffer_add_entry(sgl->empties.cube, color, &one, rangemat);

      copy_m4_m4(rangemat, ob->obmat);
      normalize_v3(rangemat[2]);
      mul_v3_fl(rangemat[2], prb->distfalloff);

      DRW_buffer_add_entry(sgl->empties.cube, color, &one, rangemat);
    }
    else {
      DRW_buffer_add_entry(sgl->empties.sphere, color, &prb->distgridinf, ob->obmat);
      DRW_buffer_add_entry(sgl->empties.sphere, color, &prb->distfalloff, ob->obmat);
    }
  }

  if ((prb->flag & LIGHTPROBE_FLAG_SHOW_PARALLAX) != 0) {
    if (prb->type != LIGHTPROBE_TYPE_PLANAR) {
      float(*obmat)[4], *dist;

      if ((prb->flag & LIGHTPROBE_FLAG_CUSTOM_PARALLAX) != 0) {
        dist = &prb->distpar;
        /* TODO object parallax */
        obmat = ob->obmat;
      }
      else {
        dist = &prb->distinf;
        obmat = ob->obmat;
      }

      if (prb->parallax_type == LIGHTPROBE_SHAPE_BOX) {
        DRW_buffer_add_entry(sgl->empties.cube, color, dist, obmat);
      }
      else {
        DRW_buffer_add_entry(sgl->empties.sphere, color, dist, obmat);
      }
    }
  }

  if ((prb->flag & LIGHTPROBE_FLAG_SHOW_CLIP_DIST) != 0) {
    if (prb->type != LIGHTPROBE_TYPE_PLANAR) {
      static const float cubefacemat[6][4][4] = {
          {{0.0, 0.0, -1.0, 0.0},
           {0.0, -1.0, 0.0, 0.0},
           {-1.0, 0.0, 0.0, 0.0},
           {0.0, 0.0, 0.0, 1.0}},
          {{0.0, 0.0, 1.0, 0.0},
           {0.0, -1.0, 0.0, 0.0},
           {1.0, 0.0, 0.0, 0.0},
           {0.0, 0.0, 0.0, 1.0}},
          {{1.0, 0.0, 0.0, 0.0},
           {0.0, 0.0, -1.0, 0.0},
           {0.0, 1.0, 0.0, 0.0},
           {0.0, 0.0, 0.0, 1.0}},
          {{1.0, 0.0, 0.0, 0.0},
           {0.0, 0.0, 1.0, 0.0},
           {0.0, -1.0, 0.0, 0.0},
           {0.0, 0.0, 0.0, 1.0}},
          {{1.0, 0.0, 0.0, 0.0},
           {0.0, -1.0, 0.0, 0.0},
           {0.0, 0.0, -1.0, 0.0},
           {0.0, 0.0, 0.0, 1.0}},
          {{-1.0, 0.0, 0.0, 0.0},
           {0.0, -1.0, 0.0, 0.0},
           {0.0, 0.0, 1.0, 0.0},
           {0.0, 0.0, 0.0, 1.0}},
      };

      for (int i = 0; i < 6; ++i) {
        float clipmat[4][4];
        normalize_m4_m4(clipmat, ob->obmat);
        mul_m4_m4m4(clipmat, clipmat, cubefacemat[i]);

        DRW_buffer_add_entry(sgl->light_buflimit, color, &prb->clipsta, &prb->clipend, clipmat);
        DRW_buffer_add_entry(
            sgl->light_buflimit_points, color, &prb->clipsta, &prb->clipend, clipmat);
      }
    }
  }

  /* Line and point going to the ground */
  if (prb->type == LIGHTPROBE_TYPE_CUBE) {
    DRW_buffer_add_entry(sgl->light_groundline, ob->obmat[3]);
    DRW_buffer_add_entry(sgl->light_groundpoint, ob->obmat[3]);
  }
}

static void DRW_shgroup_relationship_lines(OBJECT_ShadingGroupList *sgl,
                                           Depsgraph *depsgraph,
                                           Scene *scene,
                                           Object *ob)
{
  if (ob->parent && (DRW_object_visibility_in_active_context(ob->parent) & OB_VISIBLE_SELF)) {
    DRW_buffer_add_entry(sgl->relationship_lines, ob->runtime.parent_display_origin);
    DRW_buffer_add_entry(sgl->relationship_lines, ob->obmat[3]);
  }

  if (ob->rigidbody_constraint) {
    Object *rbc_ob1 = ob->rigidbody_constraint->ob1;
    Object *rbc_ob2 = ob->rigidbody_constraint->ob2;
    if (rbc_ob1 && (DRW_object_visibility_in_active_context(rbc_ob1) & OB_VISIBLE_SELF)) {
      DRW_buffer_add_entry(sgl->relationship_lines, rbc_ob1->obmat[3]);
      DRW_buffer_add_entry(sgl->relationship_lines, ob->obmat[3]);
    }
    if (rbc_ob2 && (DRW_object_visibility_in_active_context(rbc_ob2) & OB_VISIBLE_SELF)) {
      DRW_buffer_add_entry(sgl->relationship_lines, rbc_ob2->obmat[3]);
      DRW_buffer_add_entry(sgl->relationship_lines, ob->obmat[3]);
    }
  }

  /* Drawing the constraint lines */
  if (!BLI_listbase_is_empty(&ob->constraints)) {
    bConstraint *curcon;
    bConstraintOb *cob;
    ListBase *list = &ob->constraints;

    cob = BKE_constraints_make_evalob(depsgraph, scene, ob, NULL, CONSTRAINT_OBTYPE_OBJECT);

    for (curcon = list->first; curcon; curcon = curcon->next) {
      if (ELEM(curcon->type, CONSTRAINT_TYPE_FOLLOWTRACK, CONSTRAINT_TYPE_OBJECTSOLVER)) {
        /* special case for object solver and follow track constraints because they don't fill
         * constraint targets properly (design limitation -- scene is needed for their target
         * but it can't be accessed from get_targets callback) */

        Object *camob = NULL;

        if (curcon->type == CONSTRAINT_TYPE_FOLLOWTRACK) {
          bFollowTrackConstraint *data = (bFollowTrackConstraint *)curcon->data;

          camob = data->camera ? data->camera : scene->camera;
        }
        else if (curcon->type == CONSTRAINT_TYPE_OBJECTSOLVER) {
          bObjectSolverConstraint *data = (bObjectSolverConstraint *)curcon->data;

          camob = data->camera ? data->camera : scene->camera;
        }

        if (camob) {
          DRW_buffer_add_entry(sgl->constraint_lines, camob->obmat[3]);
          DRW_buffer_add_entry(sgl->constraint_lines, ob->obmat[3]);
        }
      }
      else {
        const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(curcon);

        if ((cti && cti->get_constraint_targets) && (curcon->flag & CONSTRAINT_EXPAND)) {
          ListBase targets = {NULL, NULL};
          bConstraintTarget *ct;

          cti->get_constraint_targets(curcon, &targets);

          for (ct = targets.first; ct; ct = ct->next) {
            /* calculate target's matrix */
            if (cti->get_target_matrix) {
              cti->get_target_matrix(depsgraph, curcon, cob, ct, DEG_get_ctime(depsgraph));
            }
            else {
              unit_m4(ct->matrix);
            }

            DRW_buffer_add_entry(sgl->constraint_lines, ct->matrix[3]);
            DRW_buffer_add_entry(sgl->constraint_lines, ob->obmat[3]);
          }

          if (cti->flush_constraint_targets) {
            cti->flush_constraint_targets(curcon, &targets, 1);
          }
        }
      }
    }

    BKE_constraints_clear_evalob(cob);
  }
}

static void DRW_shgroup_object_center(OBJECT_StorageList *stl,
                                      Object *ob,
                                      ViewLayer *view_layer,
                                      View3D *v3d)
{
  if (v3d->overlay.flag & V3D_OVERLAY_HIDE_OBJECT_ORIGINS) {
    return;
  }
  const bool is_library = ob->id.us > 1 || ID_IS_LINKED(ob);
  DRWCallBuffer *buf;

  if (ob == OBACT(view_layer)) {
    buf = stl->g_data->center_active;
  }
  else if (ob->base_flag & BASE_SELECTED) {
    if (is_library) {
      buf = stl->g_data->center_selected_lib;
    }
    else {
      buf = stl->g_data->center_selected;
    }
  }
  else if (v3d->flag & V3D_DRAW_CENTERS) {
    if (is_library) {
      buf = stl->g_data->center_deselected_lib;
    }
    else {
      buf = stl->g_data->center_deselected;
    }
  }
  else {
    return;
  }

  DRW_buffer_add_entry(buf, ob->obmat[3]);
}

static void DRW_shgroup_texture_space(OBJECT_ShadingGroupList *sgl, Object *ob, int theme_id)
{
  if (ob->data == NULL) {
    return;
  }

  ID *ob_data = ob->data;
  float *texcoloc = NULL;
  float *texcosize = NULL;

  switch (GS(ob_data->name)) {
    case ID_ME:
      BKE_mesh_texspace_get_reference((Mesh *)ob_data, NULL, &texcoloc, NULL, &texcosize);
      break;
    case ID_CU: {
      Curve *cu = (Curve *)ob_data;
      if (cu->bb == NULL || (cu->bb->flag & BOUNDBOX_DIRTY)) {
        BKE_curve_texspace_calc(cu);
      }
      texcoloc = cu->loc;
      texcosize = cu->size;
      break;
    }
    case ID_MB: {
      MetaBall *mb = (MetaBall *)ob_data;
      texcoloc = mb->loc;
      texcosize = mb->size;
      break;
    }
    default:
      BLI_assert(0);
  }

  float tmp[4][4] = {{0.0f}}, one = 1.0f;
  tmp[0][0] = texcosize[0];
  tmp[1][1] = texcosize[1];
  tmp[2][2] = texcosize[2];
  tmp[3][0] = texcoloc[0];
  tmp[3][1] = texcoloc[1];
  tmp[3][2] = texcoloc[2];
  tmp[3][3] = 1.0f;

  mul_m4_m4m4(tmp, ob->obmat, tmp);

  float color[4];
  UI_GetThemeColor4fv(theme_id, color);

  DRW_buffer_add_entry(sgl->texspace, color, &one, tmp);
}

static void DRW_shgroup_bounds(OBJECT_ShadingGroupList *sgl, Object *ob, int theme_id)
{
  float color[4], center[3], size[3], tmp[4][4], final_mat[4][4], one = 1.0f;
  BoundBox bb_local;

  if (ob->type == OB_MBALL && !BKE_mball_is_basis(ob)) {
    return;
  }

  BoundBox *bb = BKE_object_boundbox_get(ob);

  if (!ELEM(ob->type,
            OB_MESH,
            OB_CURVE,
            OB_SURF,
            OB_FONT,
            OB_MBALL,
            OB_ARMATURE,
            OB_LATTICE,
            OB_GPENCIL)) {
    const float min[3] = {-1.0f, -1.0f, -1.0f}, max[3] = {1.0f, 1.0f, 1.0f};
    bb = &bb_local;
    BKE_boundbox_init_from_minmax(bb, min, max);
  }

  UI_GetThemeColor4fv(theme_id, color);
  BKE_boundbox_calc_center_aabb(bb, center);
  BKE_boundbox_calc_size_aabb(bb, size);

  switch (ob->boundtype) {
    case OB_BOUND_BOX:
      size_to_mat4(tmp, size);
      copy_v3_v3(tmp[3], center);
      mul_m4_m4m4(tmp, ob->obmat, tmp);
      DRW_buffer_add_entry(sgl->empties.cube, color, &one, tmp);
      break;
    case OB_BOUND_SPHERE:
      size[0] = max_fff(size[0], size[1], size[2]);
      size[1] = size[2] = size[0];
      size_to_mat4(tmp, size);
      copy_v3_v3(tmp[3], center);
      mul_m4_m4m4(tmp, ob->obmat, tmp);
      DRW_buffer_add_entry(sgl->empties.sphere, color, &one, tmp);
      break;
    case OB_BOUND_CYLINDER:
      size[0] = max_ff(size[0], size[1]);
      size[1] = size[0];
      size_to_mat4(tmp, size);
      copy_v3_v3(tmp[3], center);
      mul_m4_m4m4(tmp, ob->obmat, tmp);
      DRW_buffer_add_entry(sgl->empties.cylinder, color, &one, tmp);
      break;
    case OB_BOUND_CONE:
      size[0] = max_ff(size[0], size[1]);
      size[1] = size[0];
      size_to_mat4(tmp, size);
      copy_v3_v3(tmp[3], center);
      /* Cone batch has base at 0 and is pointing towards +Y. */
      swap_v3_v3(tmp[1], tmp[2]);
      tmp[3][2] -= size[2];
      mul_m4_m4m4(tmp, ob->obmat, tmp);
      DRW_buffer_add_entry(sgl->empties.cone, color, &one, tmp);
      break;
    case OB_BOUND_CAPSULE:
      size[0] = max_ff(size[0], size[1]);
      size[1] = size[0];
      scale_m4_fl(tmp, size[0]);
      copy_v2_v2(tmp[3], center);
      tmp[3][2] = center[2] + max_ff(0.0f, size[2] - size[0]);
      mul_m4_m4m4(final_mat, ob->obmat, tmp);
      DRW_buffer_add_entry(sgl->empties.capsule_cap, color, &one, final_mat);
      negate_v3(tmp[2]);
      tmp[3][2] = center[2] - max_ff(0.0f, size[2] - size[0]);
      mul_m4_m4m4(final_mat, ob->obmat, tmp);
      DRW_buffer_add_entry(sgl->empties.capsule_cap, color, &one, final_mat);
      tmp[2][2] = max_ff(0.0f, size[2] * 2.0f - size[0] * 2.0f);
      mul_m4_m4m4(final_mat, ob->obmat, tmp);
      DRW_buffer_add_entry(sgl->empties.capsule_body, color, &one, final_mat);
      break;
  }
}

static void OBJECT_cache_populate_particles(OBJECT_Shaders *sh_data,
                                            Object *ob,
                                            OBJECT_PassList *psl)
{
  for (ParticleSystem *psys = ob->particlesystem.first; psys; psys = psys->next) {
    if (!DRW_object_is_visible_psys_in_active_context(ob, psys)) {
      continue;
    }

    ParticleSettings *part = psys->part;
    int draw_as = (part->draw_as == PART_DRAW_REND) ? part->ren_as : part->draw_as;

    if (part->type == PART_HAIR) {
      /* Hairs should have been rendered by the render engine.*/
      continue;
    }

    if (!ELEM(draw_as, PART_DRAW_NOT, PART_DRAW_OB, PART_DRAW_GR)) {
      struct GPUBatch *geom = DRW_cache_particles_get_dots(ob, psys);
      DRWShadingGroup *shgrp = NULL;
      struct GPUBatch *shape = NULL;
      static float def_prim_col[3] = {0.5f, 0.5f, 0.5f};
      static float def_sec_col[3] = {1.0f, 1.0f, 1.0f};

      Material *ma = give_current_material(ob, part->omat);

      switch (draw_as) {
        default:
        case PART_DRAW_DOT:
          shgrp = DRW_shgroup_create(sh_data->part_dot, psl->particle);
          DRW_shgroup_uniform_vec3(shgrp, "color", ma ? &ma->r : def_prim_col, 1);
          DRW_shgroup_uniform_vec3(shgrp, "outlineColor", ma ? &ma->specr : def_sec_col, 1);
          DRW_shgroup_uniform_float(shgrp, "pixel_size", DRW_viewport_pixelsize_get(), 1);
          DRW_shgroup_uniform_float(shgrp, "size", &part->draw_size, 1);
          DRW_shgroup_uniform_texture(shgrp, "ramp", G_draw.ramp);
          DRW_shgroup_call(shgrp, geom, NULL);
          break;
        case PART_DRAW_CROSS:
          shgrp = DRW_shgroup_create(sh_data->part_prim, psl->particle);
          DRW_shgroup_uniform_texture(shgrp, "ramp", G_draw.ramp);
          DRW_shgroup_uniform_vec3(shgrp, "color", ma ? &ma->r : def_prim_col, 1);
          DRW_shgroup_uniform_float(shgrp, "draw_size", &part->draw_size, 1);
          DRW_shgroup_uniform_bool_copy(shgrp, "screen_space", false);
          shape = DRW_cache_particles_get_prim(PART_DRAW_CROSS);
          DRW_shgroup_call_instances_with_attribs(shgrp, NULL, shape, geom);
          break;
        case PART_DRAW_CIRC:
          shape = DRW_cache_particles_get_prim(PART_DRAW_CIRC);
          shgrp = DRW_shgroup_create(sh_data->part_prim, psl->particle);
          DRW_shgroup_uniform_texture(shgrp, "ramp", G_draw.ramp);
          DRW_shgroup_uniform_vec3(shgrp, "color", ma ? &ma->r : def_prim_col, 1);
          DRW_shgroup_uniform_float(shgrp, "draw_size", &part->draw_size, 1);
          DRW_shgroup_uniform_bool_copy(shgrp, "screen_space", true);
          DRW_shgroup_call_instances_with_attribs(shgrp, NULL, shape, geom);
          break;
        case PART_DRAW_AXIS:
          shape = DRW_cache_particles_get_prim(PART_DRAW_AXIS);
          shgrp = DRW_shgroup_create(sh_data->part_axis, psl->particle);
          DRW_shgroup_uniform_float(shgrp, "draw_size", &part->draw_size, 1);
          DRW_shgroup_uniform_bool_copy(shgrp, "screen_space", false);
          DRW_shgroup_call_instances_with_attribs(shgrp, NULL, shape, geom);
          break;
      }
    }
  }
}

static void OBJECT_gpencil_color_names(Object *ob, struct DRWTextStore *dt, uchar color[4])
{
  if (ob->mode != OB_MODE_EDIT_GPENCIL) {
    return;
  }

  bGPdata *gpd = (bGPdata *)ob->data;
  if (gpd == NULL) {
    return;
  }

  for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
    if (gpl->flag & GP_LAYER_HIDE) {
      continue;
    }
    bGPDframe *gpf = gpl->actframe;
    if (gpf == NULL) {
      continue;
    }
    for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {
      Material *ma = give_current_material(ob, gps->mat_nr + 1);
      if (ma == NULL) {
        continue;
      }

      MaterialGPencilStyle *gp_style = ma->gp_style;
      /* skip stroke if it doesn't have any valid data */
      if ((gps->points == NULL) || (gps->totpoints < 1) || (gp_style == NULL)) {
        continue;
      }
      /* check if the color is visible */
      if (gp_style->flag & GP_STYLE_COLOR_HIDE) {
        continue;
      }

      /* only if selected */
      if (gps->flag & GP_STROKE_SELECT) {
        float fpt[3];
        for (int i = 0; i < gps->totpoints; i++) {
          bGPDspoint *pt = &gps->points[i];
          if (pt->flag & GP_SPOINT_SELECT) {
            mul_v3_m4v3(fpt, ob->obmat, &pt->x);
            DRW_text_cache_add(dt,
                               fpt,
                               ma->id.name + 2,
                               strlen(ma->id.name + 2),
                               10,
                               0,
                               DRW_TEXT_CACHE_GLOBALSPACE | DRW_TEXT_CACHE_STRING_PTR,
                               color);
            break;
          }
        }
      }
    }
  }
}

BLI_INLINE OBJECT_DupliData *OBJECT_duplidata_get(Object *ob, void *vedata, bool *init)
{
  OBJECT_DupliData **dupli_data = (OBJECT_DupliData **)DRW_duplidata_get(vedata);
  *init = false;
  if (!ELEM(ob->type, OB_MESH, OB_SURF, OB_LATTICE, OB_CURVE, OB_FONT)) {
    return NULL;
  }

  if (dupli_data) {
    if (*dupli_data == NULL) {
      *dupli_data = MEM_callocN(sizeof(OBJECT_DupliData), "OBJECT_DupliData");
      *init = true;
    }
    else if ((*dupli_data)->base_flag != ob->base_flag) {
      /* Select state might have change, reinit. */
      *init = true;
    }
    return *dupli_data;
  }
  return NULL;
}

static void OBJECT_cache_populate(void *vedata, Object *ob)
{
  OBJECT_PassList *psl = ((OBJECT_Data *)vedata)->psl;
  OBJECT_StorageList *stl = ((OBJECT_Data *)vedata)->stl;
  OBJECT_ShadingGroupList *sgl = (ob->dtx & OB_DRAWXRAY) ? &stl->g_data->sgl_ghost :
                                                           &stl->g_data->sgl;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const bool is_edit_mode = (ob == draw_ctx->object_edit) || BKE_object_is_in_editmode(ob);
  ViewLayer *view_layer = draw_ctx->view_layer;
  Scene *scene = draw_ctx->scene;
  View3D *v3d = draw_ctx->v3d;
  RegionView3D *rv3d = draw_ctx->rv3d;
  ModifierData *md = NULL;
  int theme_id = TH_UNDEFINED;
  const int ob_visibility = DRW_object_visibility_in_active_context(ob);
  OBJECT_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];

  /* Handle particles first in case the emitter itself shouldn't be rendered. */
  if (ob_visibility & OB_VISIBLE_PARTICLES) {
    OBJECT_cache_populate_particles(sh_data, ob, psl);
  }

  if ((ob_visibility & OB_VISIBLE_SELF) == 0) {
    return;
  }

  const bool do_outlines = ((draw_ctx->v3d->flag & V3D_SELECT_OUTLINE) &&
                            ((ob->base_flag & BASE_SELECTED) != 0) &&
                            ((DRW_object_is_renderable(ob) && (ob->dt > OB_WIRE)) ||
                             (ob->dt == OB_WIRE)));
  const bool show_relations = ((draw_ctx->v3d->flag & V3D_HIDE_HELPLINES) == 0);
  const bool hide_object_extra =
      ((v3d->overlay.flag & V3D_OVERLAY_HIDE_OBJECT_XTRAS) != 0 &&
       /* Show if this is the camera we're looking through since it's useful for selecting. */
       (((rv3d->persp == RV3D_CAMOB) && ((ID *)v3d->camera == ob->id.orig_id)) == 0));

  /* Fast path for duplis. */
  bool init_duplidata;
  OBJECT_DupliData *dupli_data = OBJECT_duplidata_get(ob, vedata, &init_duplidata);

  if (do_outlines) {
    if (!BKE_object_is_in_editmode(ob) &&
        !((ob == draw_ctx->obact) && (draw_ctx->object_mode & OB_MODE_ALL_PAINT))) {
      struct GPUBatch *geom;
      DRWShadingGroup *shgroup = NULL;

      /* This fixes only the biggest case which is a plane in ortho view. */
      int flat_axis = 0;
      bool is_flat_object_viewed_from_side = ((rv3d->persp == RV3D_ORTHO) &&
                                              DRW_object_is_flat(ob, &flat_axis) &&
                                              DRW_object_axis_orthogonal_to_view(ob, flat_axis));

      if (dupli_data && !init_duplidata) {
        geom = dupli_data->outline_geom;
        shgroup = dupli_data->outline_shgrp;
        /* TODO: Remove. Only here to increment outline id counter. */
        theme_id = DRW_object_wire_theme_get(ob, view_layer, NULL);
        shgroup = shgroup_theme_id_to_outline_or_null(stl, theme_id, ob->base_flag);
      }
      else {
        if (stl->g_data->xray_enabled_and_not_wire || is_flat_object_viewed_from_side) {
          geom = DRW_cache_object_edge_detection_get(ob, NULL);
        }
        else {
          geom = DRW_cache_object_surface_get(ob);
        }

        if (geom) {
          theme_id = DRW_object_wire_theme_get(ob, view_layer, NULL);
          shgroup = shgroup_theme_id_to_outline_or_null(stl, theme_id, ob->base_flag);
        }
      }

      if (shgroup && geom) {
        DRW_shgroup_call(shgroup, geom, ob);
      }

      if (init_duplidata) {
        dupli_data->outline_shgrp = shgroup;
        dupli_data->outline_geom = geom;
      }
    }
  }

  if (dupli_data && !init_duplidata) {
    if (dupli_data->extra_shgrp && dupli_data->extra_geom) {
      DRW_shgroup_call(dupli_data->extra_shgrp, dupli_data->extra_geom, ob);
    }
  }
  else {
    struct GPUBatch *geom = NULL;
    DRWShadingGroup *shgroup = NULL;
    switch (ob->type) {
      case OB_MESH: {
        if (hide_object_extra) {
          break;
        }
        Mesh *me = ob->data;
        if (!is_edit_mode && me->totedge == 0) {
          geom = DRW_cache_mesh_all_verts_get(ob);
          if (geom) {
            if (theme_id == TH_UNDEFINED) {
              theme_id = DRW_object_wire_theme_get(ob, view_layer, NULL);
            }
            shgroup = shgroup_theme_id_to_point(sgl, theme_id, ob->base_flag);
            DRW_shgroup_call(shgroup, geom, ob);
          }
        }
        else {
          bool has_edit_mesh_cage = false;
          /* TODO: Should be its own function. */
          if (is_edit_mode) {
            BMEditMesh *embm = me->edit_mesh;
            has_edit_mesh_cage = embm->mesh_eval_cage &&
                                 (embm->mesh_eval_cage != embm->mesh_eval_final);
          }
          if ((!is_edit_mode && me->totedge > 0) || has_edit_mesh_cage) {
            geom = DRW_cache_mesh_loose_edges_get(ob);
            if (geom) {
              if (theme_id == TH_UNDEFINED) {
                theme_id = DRW_object_wire_theme_get(ob, view_layer, NULL);
              }
              shgroup = shgroup_theme_id_to_wire(sgl, theme_id, ob->base_flag);
              DRW_shgroup_call(shgroup, geom, ob);
            }
          }
        }
        break;
      }
      case OB_SURF: {
        if (hide_object_extra) {
          break;
        }
        geom = DRW_cache_surf_edge_wire_get(ob);
        if (geom == NULL) {
          break;
        }
        if (theme_id == TH_UNDEFINED) {
          theme_id = DRW_object_wire_theme_get(ob, view_layer, NULL);
        }
        shgroup = shgroup_theme_id_to_wire(sgl, theme_id, ob->base_flag);
        DRW_shgroup_call(shgroup, geom, ob);
        break;
      }
      case OB_LATTICE: {
        if (!is_edit_mode) {
          if (hide_object_extra) {
            break;
          }
          geom = DRW_cache_lattice_wire_get(ob, false);
          if (geom == NULL) {
            break;
          }
          if (theme_id == TH_UNDEFINED) {
            theme_id = DRW_object_wire_theme_get(ob, view_layer, NULL);
          }
          shgroup = shgroup_theme_id_to_wire(sgl, theme_id, ob->base_flag);
          DRW_shgroup_call(shgroup, geom, ob);
        }
        break;
      }
      case OB_CURVE: {
        if (!is_edit_mode) {
          if (hide_object_extra) {
            break;
          }
          geom = DRW_cache_curve_edge_wire_get(ob);
          if (geom == NULL) {
            break;
          }
          if (theme_id == TH_UNDEFINED) {
            theme_id = DRW_object_wire_theme_get(ob, view_layer, NULL);
          }
          shgroup = shgroup_theme_id_to_wire(sgl, theme_id, ob->base_flag);
          DRW_shgroup_call(shgroup, geom, ob);
        }
        break;
      }
      case OB_MBALL: {
        if (!is_edit_mode) {
          DRW_shgroup_mball_handles(sgl, ob, view_layer);
        }
        break;
      }
      case OB_LAMP:
        if (hide_object_extra) {
          break;
        }
        DRW_shgroup_light(sgl, ob, view_layer);
        break;
      case OB_CAMERA:
        if (hide_object_extra) {
          break;
        }
        DRW_shgroup_camera(sgl, ob, view_layer);
        DRW_shgroup_camera_background_images(sh_data, psl, ob, rv3d);
        break;
      case OB_EMPTY:
        if (hide_object_extra) {
          break;
        }
        DRW_shgroup_empty(sh_data, sgl, ob, view_layer, rv3d, draw_ctx->sh_cfg);
        break;
      case OB_SPEAKER:
        if (hide_object_extra) {
          break;
        }
        DRW_shgroup_speaker(sgl, ob, view_layer);
        break;
      case OB_LIGHTPROBE:
        if (hide_object_extra) {
          break;
        }
        DRW_shgroup_lightprobe(sh_data, stl, psl, ob, view_layer, draw_ctx->sh_cfg);
        break;
      case OB_ARMATURE: {
        if ((v3d->flag2 & V3D_HIDE_OVERLAYS) || (v3d->overlay.flag & V3D_OVERLAY_HIDE_BONES) ||
            ((ob->dt < OB_WIRE) && !DRW_state_is_select())) {
          break;
        }
        bArmature *arm = ob->data;
        if (arm->edbo == NULL) {
          if (DRW_state_is_select() || !DRW_pose_mode_armature(ob, draw_ctx->obact)) {
            bool is_wire = (v3d->shading.type == OB_WIRE) || (ob->dt <= OB_WIRE) ||
                           XRAY_FLAG_ENABLED(v3d);
            DRWArmaturePasses passes = {
                .bone_solid = (is_wire) ? NULL : sgl->bone_solid,
                .bone_outline = sgl->bone_outline,
                .bone_wire = sgl->bone_wire,
                .bone_envelope = sgl->bone_envelope,
                .bone_axes = sgl->bone_axes,
                .relationship_lines = NULL, /* Don't draw relationship lines */
                .custom_shapes = stl->g_data->custom_shapes,
            };
            DRW_shgroup_armature_object(ob, view_layer, passes, is_wire);
          }
        }
        break;
      }
      case OB_FONT: {
        if (hide_object_extra) {
          break;
        }
        Curve *cu = (Curve *)ob->data;
        bool has_surface = (cu->flag & (CU_FRONT | CU_BACK)) || cu->ext1 != 0.0f ||
                           cu->ext2 != 0.0f;
        if (!has_surface) {
          geom = DRW_cache_text_edge_wire_get(ob);
          if (geom) {
            if (theme_id == TH_UNDEFINED) {
              theme_id = DRW_object_wire_theme_get(ob, view_layer, NULL);
            }
            shgroup = shgroup_theme_id_to_wire(sgl, theme_id, ob->base_flag);
            DRW_shgroup_call(shgroup, geom, ob);
          }
        }
        break;
      }
      default:
        break;
    }

    if (init_duplidata) {
      dupli_data->extra_shgrp = shgroup;
      dupli_data->extra_geom = geom;
      dupli_data->base_flag = ob->base_flag;
    }
  }

  if (ob->pd && ob->pd->forcefield) {
    DRW_shgroup_forcefield(sgl, ob, view_layer);
  }

  if ((ob->dt == OB_BOUNDBOX) &&
      !ELEM(ob->type, OB_LAMP, OB_CAMERA, OB_EMPTY, OB_SPEAKER, OB_LIGHTPROBE)) {
    if (theme_id == TH_UNDEFINED) {
      theme_id = DRW_object_wire_theme_get(ob, view_layer, NULL);
    }
    DRW_shgroup_bounds(sgl, ob, theme_id);
  }

  /* don't show object extras in set's */
  if ((ob->base_flag & (BASE_FROM_SET | BASE_FROM_DUPLI)) == 0) {
    if ((draw_ctx->object_mode & (OB_MODE_ALL_PAINT | OB_MODE_ALL_PAINT_GPENCIL)) == 0) {
      DRW_shgroup_object_center(stl, ob, view_layer, v3d);
    }

    if (show_relations && !DRW_state_is_select()) {
      DRW_shgroup_relationship_lines(sgl, draw_ctx->depsgraph, scene, ob);
    }

    const bool draw_extra = (ob->dtx != 0);
    if (draw_extra && (theme_id == TH_UNDEFINED)) {
      theme_id = DRW_object_wire_theme_get(ob, view_layer, NULL);
    }

    if ((ob->dtx & OB_DRAWNAME) && DRW_state_show_text()) {
      struct DRWTextStore *dt = DRW_text_cache_ensure();

      uchar color[4];
      UI_GetThemeColor4ubv(theme_id, color);

      DRW_text_cache_add(dt,
                         ob->obmat[3],
                         ob->id.name + 2,
                         strlen(ob->id.name + 2),
                         10,
                         0,
                         DRW_TEXT_CACHE_GLOBALSPACE | DRW_TEXT_CACHE_STRING_PTR,
                         color);

      /* draw grease pencil stroke names */
      if (ob->type == OB_GPENCIL) {
        OBJECT_gpencil_color_names(ob, dt, color);
      }
    }

    if ((ob->dtx & OB_TEXSPACE) && ELEM(ob->type, OB_MESH, OB_CURVE, OB_MBALL)) {
      DRW_shgroup_texture_space(sgl, ob, theme_id);
    }

    /* Don't draw bounding box again if draw type is bound box. */
    if ((ob->dtx & OB_DRAWBOUNDOX) && (ob->dt != OB_BOUNDBOX) &&
        !ELEM(ob->type, OB_LAMP, OB_CAMERA, OB_EMPTY, OB_SPEAKER, OB_LIGHTPROBE)) {
      DRW_shgroup_bounds(sgl, ob, theme_id);
    }

    if (ob->dtx & OB_AXIS) {
      float *color, axes_size = 1.0f;
      DRW_object_wire_theme_get(ob, view_layer, &color);

      DRW_buffer_add_entry(sgl->empties.empty_axes, color, &axes_size, ob->obmat);
    }

    if ((md = modifiers_findByType(ob, eModifierType_Smoke)) &&
        (modifier_isEnabled(scene, md, eModifierMode_Realtime)) &&
        (((SmokeModifierData *)md)->domain != NULL)) {
      DRW_shgroup_volume_extra(sgl, ob, view_layer, scene, md);
    }
  }
}

static void OBJECT_cache_finish(void *vedata)
{
  OBJECT_StorageList *stl = ((OBJECT_Data *)vedata)->stl;

  DRW_pass_sort_shgroup_z(stl->g_data->sgl.image_empties);
  DRW_pass_sort_shgroup_z(stl->g_data->sgl_ghost.image_empties);

  if (stl->g_data->custom_shapes) {
    /* TODO(fclem): Do not free it for each frame but reuse it. Avoiding alloc cost. */
    BLI_ghash_free(stl->g_data->custom_shapes, NULL, NULL);
  }
}

static void OBJECT_draw_scene(void *vedata)
{
  OBJECT_PassList *psl = ((OBJECT_Data *)vedata)->psl;
  OBJECT_StorageList *stl = ((OBJECT_Data *)vedata)->stl;
  OBJECT_FramebufferList *fbl = ((OBJECT_Data *)vedata)->fbl;
  OBJECT_PrivateData *g_data = stl->g_data;
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  int id_len_select = g_data->id_ofs_select;
  int id_len_select_dupli = g_data->id_ofs_select_dupli;
  int id_len_active = g_data->id_ofs_active;
  int id_len_transform = g_data->id_ofs_transform;

  int id_len_prb_select = g_data->id_ofs_prb_select;
  int id_len_prb_select_dupli = g_data->id_ofs_prb_select_dupli;
  int id_len_prb_active = g_data->id_ofs_prb_active;
  int id_len_prb_transform = g_data->id_ofs_prb_transform;

  int outline_calls = id_len_select + id_len_select_dupli + id_len_active + id_len_transform;
  outline_calls += id_len_prb_select + id_len_prb_select_dupli + id_len_prb_active +
                   id_len_prb_transform;

  float clearcol[4] = {0.0f, 0.0f, 0.0f, 0.0f};

  DRW_draw_pass(psl->camera_images_back);

  /* Don't draw Transparent passes in MSAA buffer. */
  //  DRW_draw_pass(psl->bone_envelope);  /* Never drawn in Object mode currently. */
  DRW_draw_pass(stl->g_data->sgl.transp_shapes);

  MULTISAMPLE_SYNC_ENABLE(dfbl, dtxl);

  DRW_draw_pass(stl->g_data->sgl.bone_solid);
  DRW_draw_pass(stl->g_data->sgl.bone_wire);
  DRW_draw_pass(stl->g_data->sgl.bone_outline);
  DRW_draw_pass(stl->g_data->sgl.non_meshes);
  DRW_draw_pass(psl->particle);
  DRW_draw_pass(stl->g_data->sgl.bone_axes);

  MULTISAMPLE_SYNC_DISABLE(dfbl, dtxl);

  DRW_draw_pass(stl->g_data->sgl.image_empties);

  if (DRW_state_is_fbo() && outline_calls > 0) {
    DRW_stats_group_start("Outlines");

    g_data->id_ofs_active = 1;
    g_data->id_ofs_select = g_data->id_ofs_active + id_len_active + id_len_prb_active + 1;
    g_data->id_ofs_select_dupli = g_data->id_ofs_select + id_len_select + id_len_prb_select + 1;
    g_data->id_ofs_transform = g_data->id_ofs_select_dupli + id_len_select_dupli +
                               id_len_prb_select_dupli + 1;

    g_data->id_ofs_prb_active = g_data->id_ofs_active + id_len_active;
    g_data->id_ofs_prb_select = g_data->id_ofs_select + id_len_select;
    g_data->id_ofs_prb_select_dupli = g_data->id_ofs_select_dupli + id_len_select_dupli;
    g_data->id_ofs_prb_transform = g_data->id_ofs_transform + id_len_transform;

    /* Render filled polygon on a separate framebuffer */
    GPU_framebuffer_bind(fbl->outlines_fb);
    GPU_framebuffer_clear_color_depth(fbl->outlines_fb, clearcol, 1.0f);
    DRW_draw_pass(psl->outlines);
    DRW_draw_pass(psl->lightprobes);

    /* Search outline pixels */
    GPU_framebuffer_bind(fbl->blur_fb);
    DRW_draw_pass(psl->outlines_search);

    /* Expand outline to form a 3px wide line */
    GPU_framebuffer_bind(fbl->expand_fb);
    DRW_draw_pass(psl->outlines_expand);

    /* Bleed color so the AA can do it's stuff */
    GPU_framebuffer_bind(fbl->blur_fb);
    DRW_draw_pass(psl->outlines_bleed);

    /* restore main framebuffer */
    GPU_framebuffer_bind(dfbl->default_fb);
    DRW_stats_group_end();
  }
  else if (DRW_state_is_select()) {
    /* Render probes spheres/planes so we can select them. */
    DRW_draw_pass(psl->lightprobes);
  }

  if (DRW_state_is_fbo()) {
    if (e_data.draw_grid) {
      GPU_framebuffer_bind(dfbl->color_only_fb);
      DRW_draw_pass(psl->grid);
    }

    /* Combine with scene buffer last */
    if (outline_calls > 0) {
      DRW_draw_pass(psl->outlines_resolve);
    }
  }
  volumes_free_smoke_textures();
  batch_camera_path_free(&stl->g_data->sgl.camera_path);

  if (!DRW_pass_is_empty(stl->g_data->sgl_ghost.bone_solid) ||
      !DRW_pass_is_empty(stl->g_data->sgl_ghost.bone_wire) ||
      !DRW_pass_is_empty(stl->g_data->sgl_ghost.bone_outline) ||
      !DRW_pass_is_empty(stl->g_data->sgl_ghost.non_meshes) ||
      !DRW_pass_is_empty(stl->g_data->sgl_ghost.image_empties) ||
      !DRW_pass_is_empty(stl->g_data->sgl_ghost.bone_axes)) {
    if (DRW_state_is_fbo()) {
      /* meh, late init to not request a depth buffer we won't use. */
      const float *viewport_size = DRW_viewport_size_get();
      const int size[2] = {(int)viewport_size[0], (int)viewport_size[1]};

      GPUTexture *ghost_depth_tx = DRW_texture_pool_query_2d(
          size[0], size[1], GPU_DEPTH_COMPONENT24, &draw_engine_object_type);
      GPU_framebuffer_ensure_config(&fbl->ghost_fb,
                                    {
                                        GPU_ATTACHMENT_TEXTURE(ghost_depth_tx),
                                        GPU_ATTACHMENT_TEXTURE(dtxl->color),
                                    });

      GPU_framebuffer_bind(fbl->ghost_fb);
      GPU_framebuffer_clear_depth(fbl->ghost_fb, 1.0f);
    }
    else if (DRW_state_is_select()) {
      /* XXX `GPU_depth_range` is not a perfect solution
       * since very distant geometries can still be occluded.
       * Also the depth test precision of these geometries is impaired.
       * However solves the selection for the vast majority of cases. */
      GPU_depth_range(0.0f, 0.01f);
    }

    DRW_draw_pass(stl->g_data->sgl_ghost.transp_shapes);
    DRW_draw_pass(stl->g_data->sgl_ghost.bone_solid);
    DRW_draw_pass(stl->g_data->sgl_ghost.bone_wire);
    DRW_draw_pass(stl->g_data->sgl_ghost.bone_outline);
    DRW_draw_pass(stl->g_data->sgl_ghost.non_meshes);
    DRW_draw_pass(stl->g_data->sgl_ghost.image_empties);
    DRW_draw_pass(stl->g_data->sgl_ghost.bone_axes);

    if (DRW_state_is_select()) {
      GPU_depth_range(0.0f, 1.0f);
    }
  }

  batch_camera_path_free(&stl->g_data->sgl_ghost.camera_path);

  DRW_draw_pass(psl->camera_images_front);
  camera_background_images_free_textures();

  DRW_draw_pass(psl->ob_center);
}

static const DrawEngineDataSize OBJECT_data_size = DRW_VIEWPORT_DATA_SIZE(OBJECT_Data);

DrawEngineType draw_engine_object_type = {
    NULL,
    NULL,
    N_("ObjectMode"),
    &OBJECT_data_size,
    &OBJECT_engine_init,
    &OBJECT_engine_free,
    &OBJECT_cache_init,
    &OBJECT_cache_populate,
    &OBJECT_cache_finish,
    NULL,
    &OBJECT_draw_scene,
    NULL,
    NULL,
};
