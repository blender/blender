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

#include "DRW_render.h"

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "UI_resources.h"

#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_colorband.h"

#include "draw_common.h"

#if 0
#  define UI_COLOR_RGB_FROM_U8(r, g, b, v4) \
    ARRAY_SET_ITEMS(v4, (float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, 1.0)
#endif
#define UI_COLOR_RGBA_FROM_U8(r, g, b, a, v4) \
  ARRAY_SET_ITEMS(v4, (float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, (float)a / 255.0f)

/* Colors & Constant */
struct DRW_Global G_draw = {{{0}}};

static bool weight_ramp_custom = false;
static ColorBand weight_ramp_copy;

static struct GPUTexture *DRW_create_weight_colorramp_texture(void);

void DRW_globals_update(void)
{
  GlobalsUboStorage *gb = &G_draw.block;

  UI_GetThemeColor4fv(TH_WIRE, gb->colorWire);
  UI_GetThemeColor4fv(TH_WIRE_EDIT, gb->colorWireEdit);
  UI_GetThemeColor4fv(TH_ACTIVE, gb->colorActive);
  UI_GetThemeColor4fv(TH_SELECT, gb->colorSelect);
  UI_COLOR_RGBA_FROM_U8(0x88, 0xFF, 0xFF, 155, gb->colorLibrarySelect);
  UI_COLOR_RGBA_FROM_U8(0x55, 0xCC, 0xCC, 155, gb->colorLibrary);
  UI_GetThemeColor4fv(TH_TRANSFORM, gb->colorTransform);
  UI_GetThemeColor4fv(TH_LIGHT, gb->colorLight);
  UI_GetThemeColor4fv(TH_SPEAKER, gb->colorSpeaker);
  UI_GetThemeColor4fv(TH_CAMERA, gb->colorCamera);
  UI_GetThemeColor4fv(TH_EMPTY, gb->colorEmpty);
  UI_GetThemeColor4fv(TH_VERTEX, gb->colorVertex);
  UI_GetThemeColor4fv(TH_VERTEX_SELECT, gb->colorVertexSelect);
  UI_GetThemeColor4fv(TH_VERTEX_UNREFERENCED, gb->colorVertexUnreferenced);
  UI_COLOR_RGBA_FROM_U8(0xB0, 0x00, 0xB0, 0xFF, gb->colorVertexMissingData);
  UI_GetThemeColor4fv(TH_EDITMESH_ACTIVE, gb->colorEditMeshActive);
  UI_GetThemeColor4fv(TH_EDGE_SELECT, gb->colorEdgeSelect);

  UI_GetThemeColor4fv(TH_EDGE_SEAM, gb->colorEdgeSeam);
  UI_GetThemeColor4fv(TH_EDGE_SHARP, gb->colorEdgeSharp);
  UI_GetThemeColor4fv(TH_EDGE_CREASE, gb->colorEdgeCrease);
  UI_GetThemeColor4fv(TH_EDGE_BEVEL, gb->colorEdgeBWeight);
  UI_GetThemeColor4fv(TH_EDGE_FACESEL, gb->colorEdgeFaceSelect);
  UI_GetThemeColor4fv(TH_FACE, gb->colorFace);
  UI_GetThemeColor4fv(TH_FACE_SELECT, gb->colorFaceSelect);
  UI_GetThemeColor4fv(TH_NORMAL, gb->colorNormal);
  UI_GetThemeColor4fv(TH_VNORMAL, gb->colorVNormal);
  UI_GetThemeColor4fv(TH_LNORMAL, gb->colorLNormal);
  UI_GetThemeColor4fv(TH_FACE_DOT, gb->colorFaceDot);
  UI_GetThemeColor4fv(TH_BACK, gb->colorBackground);

  /* Custom median color to slightly affect the edit mesh colors. */
  interp_v4_v4v4(gb->colorEditMeshMiddle, gb->colorVertexSelect, gb->colorWireEdit, 0.35f);
  copy_v3_fl(
      gb->colorEditMeshMiddle,
      dot_v3v3(gb->colorEditMeshMiddle, (float[3]){0.3333f, 0.3333f, 0.3333f})); /* Desaturate */

  interp_v4_v4v4(gb->colorDupliSelect, gb->colorBackground, gb->colorSelect, 0.5f);
  /* Was 50% in 2.7x since the background was lighter making it easier to tell the color from
   * black, with a darker background we need a more faded color. */
  interp_v4_v4v4(gb->colorDupli, gb->colorBackground, gb->colorWire, 0.3f);

#ifdef WITH_FREESTYLE
  UI_GetThemeColor4fv(TH_FREESTYLE_EDGE_MARK, gb->colorEdgeFreestyle);
  UI_GetThemeColor4fv(TH_FREESTYLE_FACE_MARK, gb->colorFaceFreestyle);
#else
  zero_v4(gb->colorEdgeFreestyle);
  zero_v4(gb->colorFaceFreestyle);
#endif

  /* Curve */
  UI_GetThemeColor4fv(TH_HANDLE_FREE, gb->colorHandleFree);
  UI_GetThemeColor4fv(TH_HANDLE_AUTO, gb->colorHandleAuto);
  UI_GetThemeColor4fv(TH_HANDLE_VECT, gb->colorHandleVect);
  UI_GetThemeColor4fv(TH_HANDLE_ALIGN, gb->colorHandleAlign);
  UI_GetThemeColor4fv(TH_HANDLE_AUTOCLAMP, gb->colorHandleAutoclamp);
  UI_GetThemeColor4fv(TH_HANDLE_SEL_FREE, gb->colorHandleSelFree);
  UI_GetThemeColor4fv(TH_HANDLE_SEL_AUTO, gb->colorHandleSelAuto);
  UI_GetThemeColor4fv(TH_HANDLE_SEL_VECT, gb->colorHandleSelVect);
  UI_GetThemeColor4fv(TH_HANDLE_SEL_ALIGN, gb->colorHandleSelAlign);
  UI_GetThemeColor4fv(TH_HANDLE_SEL_AUTOCLAMP, gb->colorHandleSelAutoclamp);
  UI_GetThemeColor4fv(TH_NURB_ULINE, gb->colorNurbUline);
  UI_GetThemeColor4fv(TH_NURB_VLINE, gb->colorNurbVline);
  UI_GetThemeColor4fv(TH_NURB_SEL_ULINE, gb->colorNurbSelUline);
  UI_GetThemeColor4fv(TH_NURB_SEL_VLINE, gb->colorNurbSelVline);
  UI_GetThemeColor4fv(TH_ACTIVE_SPLINE, gb->colorActiveSpline);

  UI_GetThemeColor4fv(TH_BONE_POSE, gb->colorBonePose);

  UI_GetThemeColor4fv(TH_CFRAME, gb->colorCurrentFrame);

  /* Grid */
  UI_GetThemeColorShade4fv(TH_GRID, 10, gb->colorGrid);
  /* emphasise division lines lighter instead of darker, if background is darker than grid */
  UI_GetThemeColorShade4fv(
      TH_GRID,
      (gb->colorGrid[0] + gb->colorGrid[1] + gb->colorGrid[2] + 0.12f >
       gb->colorBackground[0] + gb->colorBackground[1] + gb->colorBackground[2]) ?
          20 :
          -10,
      gb->colorGridEmphasise);
  /* Grid Axis */
  UI_GetThemeColorBlendShade4fv(TH_GRID, TH_AXIS_X, 0.5f, -10, gb->colorGridAxisX);
  UI_GetThemeColorBlendShade4fv(TH_GRID, TH_AXIS_Y, 0.5f, -10, gb->colorGridAxisY);
  UI_GetThemeColorBlendShade4fv(TH_GRID, TH_AXIS_Z, 0.5f, -10, gb->colorGridAxisZ);

  UI_GetThemeColorShadeAlpha4fv(TH_TRANSFORM, 0, -80, gb->colorDeselect);
  UI_GetThemeColorShadeAlpha4fv(TH_WIRE, 0, -30, gb->colorOutline);
  UI_GetThemeColorShadeAlpha4fv(TH_LIGHT, 0, 255, gb->colorLightNoAlpha);

  gb->sizeLightCenter = (UI_GetThemeValuef(TH_OBCENTER_DIA) + 1.5f) * U.pixelsize;
  gb->sizeLightCircle = U.pixelsize * 9.0f;
  gb->sizeLightCircleShadow = gb->sizeLightCircle + U.pixelsize * 3.0f;

  /* M_SQRT2 to be at least the same size of the old square */
  gb->sizeVertex = U.pixelsize *
                   (max_ff(1.0f, UI_GetThemeValuef(TH_VERTEX_SIZE) * (float)M_SQRT2 / 2.0f));
  gb->sizeFaceDot = U.pixelsize * UI_GetThemeValuef(TH_FACEDOT_SIZE);
  gb->sizeEdge = U.pixelsize * (1.0f / 2.0f); /* TODO Theme */
  gb->sizeEdgeFix = U.pixelsize * (0.5f + 2.0f * (2.0f * (gb->sizeEdge * (float)M_SQRT1_2)));

  /* Color management. */
  if (DRW_state_is_image_render()) {
    float *color = gb->UBO_FIRST_COLOR;
    do {
      /* TODO more accurate transform. */
      srgb_to_linearrgb_v4(color, color);
      color += 4;
    } while (color != gb->UBO_LAST_COLOR);
  }

  if (G_draw.block_ubo == NULL) {
    G_draw.block_ubo = DRW_uniformbuffer_create(sizeof(GlobalsUboStorage), gb);
  }

  DRW_uniformbuffer_update(G_draw.block_ubo, gb);

  if (!G_draw.ramp) {
    ColorBand ramp = {0};
    float *colors;
    int col_size;

    ramp.tot = 3;
    ramp.data[0].a = 1.0f;
    ramp.data[0].b = 1.0f;
    ramp.data[0].pos = 0.0f;
    ramp.data[1].a = 1.0f;
    ramp.data[1].g = 1.0f;
    ramp.data[1].pos = 0.5f;
    ramp.data[2].a = 1.0f;
    ramp.data[2].r = 1.0f;
    ramp.data[2].pos = 1.0f;

    BKE_colorband_evaluate_table_rgba(&ramp, &colors, &col_size);

    G_draw.ramp = GPU_texture_create_1d(col_size, GPU_RGBA8, colors, NULL);

    MEM_freeN(colors);
  }

  /* Weight Painting color ramp texture */
  bool user_weight_ramp = (U.flag & USER_CUSTOM_RANGE) != 0;

  if (weight_ramp_custom != user_weight_ramp ||
      (user_weight_ramp && memcmp(&weight_ramp_copy, &U.coba_weight, sizeof(ColorBand)) != 0)) {
    DRW_TEXTURE_FREE_SAFE(G_draw.weight_ramp);
  }

  if (G_draw.weight_ramp == NULL) {
    weight_ramp_custom = user_weight_ramp;
    memcpy(&weight_ramp_copy, &U.coba_weight, sizeof(ColorBand));

    G_draw.weight_ramp = DRW_create_weight_colorramp_texture();
  }
}

/* ********************************* SHGROUP ************************************* */

extern char datatoc_animviz_mpath_lines_vert_glsl[];
extern char datatoc_animviz_mpath_lines_geom_glsl[];
extern char datatoc_animviz_mpath_points_vert_glsl[];

extern char datatoc_volume_velocity_vert_glsl[];

extern char datatoc_armature_axes_vert_glsl[];
extern char datatoc_armature_sphere_solid_vert_glsl[];
extern char datatoc_armature_sphere_solid_frag_glsl[];
extern char datatoc_armature_sphere_outline_vert_glsl[];
extern char datatoc_armature_envelope_solid_vert_glsl[];
extern char datatoc_armature_envelope_solid_frag_glsl[];
extern char datatoc_armature_envelope_outline_vert_glsl[];
extern char datatoc_armature_envelope_distance_frag_glsl[];
extern char datatoc_armature_shape_solid_vert_glsl[];
extern char datatoc_armature_shape_solid_frag_glsl[];
extern char datatoc_armature_shape_outline_vert_glsl[];
extern char datatoc_armature_shape_outline_geom_glsl[];
extern char datatoc_armature_stick_vert_glsl[];
extern char datatoc_armature_stick_frag_glsl[];
extern char datatoc_armature_dof_vert_glsl[];

extern char datatoc_common_globals_lib_glsl[];
extern char datatoc_common_view_lib_glsl[];

extern char datatoc_gpu_shader_flat_color_frag_glsl[];
extern char datatoc_gpu_shader_3D_smooth_color_frag_glsl[];
extern char datatoc_gpu_shader_point_varying_color_frag_glsl[];

extern char datatoc_object_mball_handles_vert_glsl[];
extern char datatoc_object_empty_axes_vert_glsl[];

typedef struct COMMON_Shaders {
  struct GPUShader *shape_outline;
  struct GPUShader *shape_solid;
  struct GPUShader *bone_axes;
  struct GPUShader *bone_envelope;
  struct GPUShader *bone_envelope_distance;
  struct GPUShader *bone_envelope_outline;
  struct GPUShader *bone_sphere;
  struct GPUShader *bone_sphere_outline;
  struct GPUShader *bone_stick;
  struct GPUShader *bone_dofs;

  struct GPUShader *mpath_line_sh;
  struct GPUShader *mpath_points_sh;

  struct GPUShader *volume_velocity_needle_sh;
  struct GPUShader *volume_velocity_sh;
  struct GPUShader *empty_axes_sh;

  struct GPUShader *mball_handles;
} COMMON_Shaders;

static COMMON_Shaders g_shaders[GPU_SHADER_CFG_LEN] = {{NULL}};

static struct {
  struct GPUVertFormat *instance_screenspace;
  struct GPUVertFormat *instance_color;
  struct GPUVertFormat *instance_screen_aligned;
  struct GPUVertFormat *instance_scaled;
  struct GPUVertFormat *instance_sized;
  struct GPUVertFormat *instance_outline;
  struct GPUVertFormat *instance;
  struct GPUVertFormat *instance_camera;
  struct GPUVertFormat *instance_distance_lines;
  struct GPUVertFormat *instance_spot;
  struct GPUVertFormat *instance_bone;
  struct GPUVertFormat *instance_bone_dof;
  struct GPUVertFormat *instance_bone_stick;
  struct GPUVertFormat *instance_bone_outline;
  struct GPUVertFormat *instance_bone_envelope;
  struct GPUVertFormat *instance_bone_envelope_distance;
  struct GPUVertFormat *instance_bone_envelope_outline;
  struct GPUVertFormat *instance_mball_handles;
  struct GPUVertFormat *pos_color;
  struct GPUVertFormat *pos;
} g_formats = {NULL};

void DRW_globals_free(void)
{
  struct GPUVertFormat **format = &g_formats.instance_screenspace;
  for (int i = 0; i < sizeof(g_formats) / sizeof(void *); ++i, ++format) {
    MEM_SAFE_FREE(*format);
  }

  for (int j = 0; j < GPU_SHADER_CFG_LEN; j++) {
    struct GPUShader **shader = &g_shaders[j].shape_outline;
    for (int i = 0; i < sizeof(g_shaders[j]) / sizeof(void *); ++i, ++shader) {
      DRW_SHADER_FREE_SAFE(*shader);
    }
  }
}

void DRW_shgroup_world_clip_planes_from_rv3d(DRWShadingGroup *shgrp, const RegionView3D *rv3d)
{
  int world_clip_planes_len = (rv3d->viewlock & RV3D_BOXCLIP) ? 4 : 6;
  DRW_shgroup_uniform_vec4(shgrp, "WorldClipPlanes", rv3d->clip[0], world_clip_planes_len);
  DRW_shgroup_state_enable(shgrp, DRW_STATE_CLIP_PLANES);
}

struct DRWCallBuffer *buffer_dynlines_flat_color(DRWPass *pass, eGPUShaderConfig sh_cfg)
{
  GPUShader *sh = GPU_shader_get_builtin_shader_with_config(GPU_SHADER_3D_FLAT_COLOR, sh_cfg);

  DRW_shgroup_instance_format(g_formats.pos_color,
                              {
                                  {"pos", DRW_ATTR_FLOAT, 3},
                                  {"color", DRW_ATTR_FLOAT, 4},
                              });

  DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);
  if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
    DRW_shgroup_world_clip_planes_from_rv3d(grp, DRW_context_state_get()->rv3d);
  }
  return DRW_shgroup_call_buffer(grp, g_formats.pos_color, GPU_PRIM_LINES);
}

struct DRWCallBuffer *buffer_dynlines_dashed_uniform_color(DRWPass *pass,
                                                           const float color[4],
                                                           eGPUShaderConfig sh_cfg)
{
  GPUShader *sh = GPU_shader_get_builtin_shader_with_config(
      GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR, sh_cfg);

  static float dash_width = 6.0f;
  static float dash_factor = 0.5f;

  DRW_shgroup_instance_format(g_formats.pos, {{"pos", DRW_ATTR_FLOAT, 3}});

  DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);
  DRW_shgroup_uniform_vec4(grp, "color", color, 1);
  DRW_shgroup_uniform_vec2(grp, "viewport_size", DRW_viewport_size_get(), 1);
  DRW_shgroup_uniform_float(grp, "dash_width", &dash_width, 1);
  DRW_shgroup_uniform_float(grp, "dash_factor", &dash_factor, 1);
  DRW_shgroup_uniform_int_copy(grp, "colors_len", 0); /* "simple" mode */
  if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
    DRW_shgroup_world_clip_planes_from_rv3d(grp, DRW_context_state_get()->rv3d);
  }
  return DRW_shgroup_call_buffer(grp, g_formats.pos, GPU_PRIM_LINES);
}

struct DRWCallBuffer *buffer_dynpoints_uniform_color(DRWShadingGroup *grp)
{
  DRW_shgroup_instance_format(g_formats.pos, {{"pos", DRW_ATTR_FLOAT, 3}});

  return DRW_shgroup_call_buffer(grp, g_formats.pos, GPU_PRIM_POINTS);
}

struct DRWCallBuffer *buffer_groundlines_uniform_color(DRWPass *pass,
                                                       const float color[4],
                                                       eGPUShaderConfig sh_cfg)
{
  GPUShader *sh = GPU_shader_get_builtin_shader_with_config(GPU_SHADER_3D_GROUNDLINE, sh_cfg);

  DRW_shgroup_instance_format(g_formats.pos, {{"pos", DRW_ATTR_FLOAT, 3}});

  DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);
  DRW_shgroup_uniform_vec4(grp, "color", color, 1);
  if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
    DRW_shgroup_world_clip_planes_from_rv3d(grp, DRW_context_state_get()->rv3d);
  }
  return DRW_shgroup_call_buffer(grp, g_formats.pos, GPU_PRIM_POINTS);
}

struct DRWCallBuffer *buffer_groundpoints_uniform_color(DRWPass *pass,
                                                        const float color[4],
                                                        eGPUShaderConfig sh_cfg)
{
  GPUShader *sh = GPU_shader_get_builtin_shader_with_config(GPU_SHADER_3D_GROUNDPOINT, sh_cfg);

  DRW_shgroup_instance_format(g_formats.pos, {{"pos", DRW_ATTR_FLOAT, 3}});

  DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);
  DRW_shgroup_uniform_vec4(grp, "color", color, 1);
  if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
    DRW_shgroup_world_clip_planes_from_rv3d(grp, DRW_context_state_get()->rv3d);
  }
  return DRW_shgroup_call_buffer(grp, g_formats.pos, GPU_PRIM_POINTS);
}

struct DRWCallBuffer *buffer_instance_screenspace(DRWPass *pass,
                                                  struct GPUBatch *geom,
                                                  const float *size,
                                                  eGPUShaderConfig sh_cfg)
{
  GPUShader *sh = GPU_shader_get_builtin_shader_with_config(
      GPU_SHADER_3D_SCREENSPACE_VARIYING_COLOR, sh_cfg);

  DRW_shgroup_instance_format(g_formats.instance_screenspace,
                              {
                                  {"world_pos", DRW_ATTR_FLOAT, 3},
                                  {"color", DRW_ATTR_FLOAT, 3},
                              });

  DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);
  DRW_shgroup_uniform_float(grp, "size", size, 1);
  DRW_shgroup_uniform_float(grp, "pixel_size", DRW_viewport_pixelsize_get(), 1);
  DRW_shgroup_uniform_vec3(grp, "screen_vecs[0]", DRW_viewport_screenvecs_get(), 2);
  if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
    DRW_shgroup_world_clip_planes_from_rv3d(grp, DRW_context_state_get()->rv3d);
  }
  return DRW_shgroup_call_buffer_instance(grp, g_formats.instance_screenspace, geom);
}

struct DRWCallBuffer *buffer_instance_solid(DRWPass *pass, struct GPUBatch *geom)
{
  static float light[3] = {0.0f, 0.0f, 1.0f};
  GPUShader *sh = GPU_shader_get_builtin_shader(
      GPU_SHADER_3D_OBJECTSPACE_SIMPLE_LIGHTING_VARIYING_COLOR);

  DRW_shgroup_instance_format(g_formats.instance_color,
                              {
                                  {"InstanceModelMatrix", DRW_ATTR_FLOAT, 16},
                                  {"color", DRW_ATTR_FLOAT, 4},
                              });

  DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);
  DRW_shgroup_uniform_vec3(grp, "light", light, 1);

  return DRW_shgroup_call_buffer_instance(grp, g_formats.instance_color, geom);
}

struct DRWCallBuffer *buffer_instance_wire(DRWPass *pass, struct GPUBatch *geom)
{
  GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_OBJECTSPACE_VARIYING_COLOR);

  DRW_shgroup_instance_format(g_formats.instance_color,
                              {
                                  {"InstanceModelMatrix", DRW_ATTR_FLOAT, 16},
                                  {"color", DRW_ATTR_FLOAT, 4},
                              });

  DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);

  return DRW_shgroup_call_buffer_instance(grp, g_formats.instance_color, geom);
}

struct DRWCallBuffer *buffer_instance_screen_aligned(DRWPass *pass,
                                                     struct GPUBatch *geom,
                                                     eGPUShaderConfig sh_cfg)
{
  GPUShader *sh = GPU_shader_get_builtin_shader_with_config(GPU_SHADER_3D_INSTANCE_SCREEN_ALIGNED,
                                                            sh_cfg);

  DRW_shgroup_instance_format(g_formats.instance_screen_aligned,
                              {
                                  {"color", DRW_ATTR_FLOAT, 3},
                                  {"size", DRW_ATTR_FLOAT, 1},
                                  {"InstanceModelMatrix", DRW_ATTR_FLOAT, 16},
                              });

  DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);
  DRW_shgroup_uniform_vec3(grp, "screen_vecs[0]", DRW_viewport_screenvecs_get(), 2);
  if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
    DRW_shgroup_world_clip_planes_from_rv3d(grp, DRW_context_state_get()->rv3d);
  }
  return DRW_shgroup_call_buffer_instance(grp, g_formats.instance_screen_aligned, geom);
}

struct DRWCallBuffer *buffer_instance_scaled(DRWPass *pass,
                                             struct GPUBatch *geom,
                                             eGPUShaderConfig sh_cfg)
{
  GPUShader *sh_inst = GPU_shader_get_builtin_shader_with_config(
      GPU_SHADER_INSTANCE_VARIYING_COLOR_VARIYING_SCALE, sh_cfg);

  DRW_shgroup_instance_format(g_formats.instance_scaled,
                              {
                                  {"color", DRW_ATTR_FLOAT, 3},
                                  {"size", DRW_ATTR_FLOAT, 3},
                                  {"InstanceModelMatrix", DRW_ATTR_FLOAT, 16},
                              });

  DRWShadingGroup *grp = DRW_shgroup_create(sh_inst, pass);
  if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
    DRW_shgroup_world_clip_planes_from_rv3d(grp, DRW_context_state_get()->rv3d);
  }
  return DRW_shgroup_call_buffer_instance(grp, g_formats.instance_scaled, geom);
}

struct DRWCallBuffer *buffer_instance(DRWPass *pass,
                                      struct GPUBatch *geom,
                                      eGPUShaderConfig sh_cfg)
{
  GPUShader *sh_inst = GPU_shader_get_builtin_shader_with_config(
      GPU_SHADER_INSTANCE_VARIYING_COLOR_VARIYING_SIZE, sh_cfg);

  DRW_shgroup_instance_format(g_formats.instance_sized,
                              {
                                  {"color", DRW_ATTR_FLOAT, 4},
                                  {"size", DRW_ATTR_FLOAT, 1},
                                  {"InstanceModelMatrix", DRW_ATTR_FLOAT, 16},
                              });

  DRWShadingGroup *grp = DRW_shgroup_create(sh_inst, pass);
  DRW_shgroup_state_disable(grp, DRW_STATE_BLEND);
  if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
    DRW_shgroup_world_clip_planes_from_rv3d(grp, DRW_context_state_get()->rv3d);
  }
  return DRW_shgroup_call_buffer_instance(grp, g_formats.instance_sized, geom);
}

struct DRWCallBuffer *buffer_instance_alpha(DRWShadingGroup *grp, struct GPUBatch *geom)
{
  DRW_shgroup_instance_format(g_formats.instance_sized,
                              {
                                  {"color", DRW_ATTR_FLOAT, 4},
                                  {"size", DRW_ATTR_FLOAT, 1},
                                  {"InstanceModelMatrix", DRW_ATTR_FLOAT, 16},
                              });

  return DRW_shgroup_call_buffer_instance(grp, g_formats.instance_sized, geom);
}

struct DRWCallBuffer *buffer_instance_empty_axes(DRWPass *pass,
                                                 struct GPUBatch *geom,
                                                 eGPUShaderConfig sh_cfg)
{
  COMMON_Shaders *sh_data = &g_shaders[sh_cfg];
  if (sh_data->empty_axes_sh == NULL) {
    const GPUShaderConfigData *sh_cfg_data = &GPU_shader_cfg_data[sh_cfg];
    sh_data->empty_axes_sh = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib, datatoc_object_empty_axes_vert_glsl, NULL},
        .frag = (const char *[]){datatoc_gpu_shader_flat_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });
  }

  DRW_shgroup_instance_format(g_formats.instance_sized,
                              {
                                  {"color", DRW_ATTR_FLOAT, 3},
                                  {"size", DRW_ATTR_FLOAT, 1},
                                  {"InstanceModelMatrix", DRW_ATTR_FLOAT, 16},
                              });

  DRWShadingGroup *grp = DRW_shgroup_create(sh_data->empty_axes_sh, pass);
  DRW_shgroup_uniform_vec3(grp, "screenVecs[0]", DRW_viewport_screenvecs_get(), 2);
  if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
    DRW_shgroup_world_clip_planes_from_rv3d(grp, DRW_context_state_get()->rv3d);
  }
  return DRW_shgroup_call_buffer_instance(grp, g_formats.instance_sized, geom);
}

struct DRWCallBuffer *buffer_instance_outline(DRWPass *pass, struct GPUBatch *geom, int *baseid)
{
  GPUShader *sh_inst = GPU_shader_get_builtin_shader(
      GPU_SHADER_INSTANCE_VARIYING_ID_VARIYING_SIZE);

  DRW_shgroup_instance_format(g_formats.instance_outline,
                              {
                                  {"callId", DRW_ATTR_INT, 1},
                                  {"size", DRW_ATTR_FLOAT, 1},
                                  {"InstanceModelMatrix", DRW_ATTR_FLOAT, 16},
                              });

  DRWShadingGroup *grp = DRW_shgroup_create(sh_inst, pass);
  DRW_shgroup_uniform_int(grp, "baseId", baseid, 1);

  return DRW_shgroup_call_buffer_instance(grp, g_formats.instance_outline, geom);
}

struct DRWCallBuffer *buffer_camera_instance(DRWPass *pass,
                                             struct GPUBatch *geom,
                                             eGPUShaderConfig sh_cfg)
{
  GPUShader *sh_inst = GPU_shader_get_builtin_shader_with_config(GPU_SHADER_CAMERA, sh_cfg);

  DRW_shgroup_instance_format(g_formats.instance_camera,
                              {
                                  {"color", DRW_ATTR_FLOAT, 3},
                                  {"corners", DRW_ATTR_FLOAT, 8},
                                  {"depth", DRW_ATTR_FLOAT, 1},
                                  {"tria", DRW_ATTR_FLOAT, 4},
                                  {"InstanceModelMatrix", DRW_ATTR_FLOAT, 16},
                              });

  DRWShadingGroup *grp = DRW_shgroup_create(sh_inst, pass);
  if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
    DRW_shgroup_world_clip_planes_from_rv3d(grp, DRW_context_state_get()->rv3d);
  }
  return DRW_shgroup_call_buffer_instance(grp, g_formats.instance_camera, geom);
}

struct DRWCallBuffer *buffer_distance_lines_instance(DRWPass *pass,
                                                     struct GPUBatch *geom,
                                                     eGPUShaderConfig sh_cfg)
{
  GPUShader *sh_inst = GPU_shader_get_builtin_shader_with_config(GPU_SHADER_DISTANCE_LINES,
                                                                 sh_cfg);
  static float point_size = 4.0f;

  DRW_shgroup_instance_format(g_formats.instance_distance_lines,
                              {
                                  {"color", DRW_ATTR_FLOAT, 3},
                                  {"start", DRW_ATTR_FLOAT, 1},
                                  {"end", DRW_ATTR_FLOAT, 1},
                                  {"InstanceModelMatrix", DRW_ATTR_FLOAT, 16},
                              });

  DRWShadingGroup *grp = DRW_shgroup_create(sh_inst, pass);
  DRW_shgroup_uniform_float(grp, "size", &point_size, 1);
  if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
    DRW_shgroup_world_clip_planes_from_rv3d(grp, DRW_context_state_get()->rv3d);
  }
  return DRW_shgroup_call_buffer_instance(grp, g_formats.instance_distance_lines, geom);
}

struct DRWCallBuffer *buffer_spot_instance(DRWPass *pass,
                                           struct GPUBatch *geom,
                                           eGPUShaderConfig sh_cfg)
{
  GPUShader *sh_inst = GPU_shader_get_builtin_shader_with_config(
      GPU_SHADER_INSTANCE_EDGES_VARIYING_COLOR, sh_cfg);
  static const int True = true;
  static const int False = false;

  DRW_shgroup_instance_format(g_formats.instance_spot,
                              {
                                  {"color", DRW_ATTR_FLOAT, 3},
                                  {"InstanceModelMatrix", DRW_ATTR_FLOAT, 16},
                              });

  DRWShadingGroup *grp = DRW_shgroup_create(sh_inst, pass);
  DRW_shgroup_uniform_bool(grp, "drawFront", &False, 1);
  DRW_shgroup_uniform_bool(grp, "drawBack", &False, 1);
  DRW_shgroup_uniform_bool(grp, "drawSilhouette", &True, 1);
  if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
    DRW_shgroup_world_clip_planes_from_rv3d(grp, DRW_context_state_get()->rv3d);
  }
  return DRW_shgroup_call_buffer_instance(grp, g_formats.instance_spot, geom);
}

struct DRWCallBuffer *buffer_instance_bone_axes(DRWPass *pass, eGPUShaderConfig sh_cfg)
{
  COMMON_Shaders *sh_data = &g_shaders[sh_cfg];
  if (sh_data->bone_axes == NULL) {
    const GPUShaderConfigData *sh_cfg_data = &GPU_shader_cfg_data[sh_cfg];
    sh_data->bone_axes = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib, datatoc_armature_axes_vert_glsl, NULL},
        .frag = (const char *[]){datatoc_gpu_shader_flat_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });
  }

  DRW_shgroup_instance_format(g_formats.instance_color,
                              {
                                  {"InstanceModelMatrix", DRW_ATTR_FLOAT, 16},
                                  {"color", DRW_ATTR_FLOAT, 4},
                              });

  DRWShadingGroup *grp = DRW_shgroup_create(sh_data->bone_axes, pass);
  DRW_shgroup_uniform_vec3(grp, "screenVecs[0]", DRW_viewport_screenvecs_get(), 2);
  if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
    DRW_shgroup_world_clip_planes_from_rv3d(grp, DRW_context_state_get()->rv3d);
  }
  return DRW_shgroup_call_buffer_instance(
      grp, g_formats.instance_color, DRW_cache_bone_arrows_get());
}

struct DRWCallBuffer *buffer_instance_bone_envelope_outline(DRWPass *pass, eGPUShaderConfig sh_cfg)
{
  COMMON_Shaders *sh_data = &g_shaders[sh_cfg];
  if (sh_data->bone_envelope_outline == NULL) {
    const GPUShaderConfigData *sh_cfg_data = &GPU_shader_cfg_data[sh_cfg];
    sh_data->bone_envelope_outline = GPU_shader_create_from_arrays({
        .vert =
            (const char *[]){sh_cfg_data->lib, datatoc_armature_envelope_outline_vert_glsl, NULL},
        .frag = (const char *[]){datatoc_gpu_shader_flat_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });
  }

  DRW_shgroup_instance_format(g_formats.instance_bone_envelope_outline,
                              {
                                  {"headSphere", DRW_ATTR_FLOAT, 4},
                                  {"tailSphere", DRW_ATTR_FLOAT, 4},
                                  {"outlineColorSize", DRW_ATTR_FLOAT, 4},
                                  {"xAxis", DRW_ATTR_FLOAT, 3},
                              });

  DRWShadingGroup *grp = DRW_shgroup_create(sh_data->bone_envelope_outline, pass);
  DRW_shgroup_uniform_vec2(grp, "viewportSize", DRW_viewport_size_get(), 1);
  if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
    DRW_shgroup_world_clip_planes_from_rv3d(grp, DRW_context_state_get()->rv3d);
  }
  return DRW_shgroup_call_buffer_instance(
      grp, g_formats.instance_bone_envelope_outline, DRW_cache_bone_envelope_outline_get());
}

struct DRWCallBuffer *buffer_instance_bone_envelope_distance(DRWPass *pass,
                                                             eGPUShaderConfig sh_cfg)
{
  COMMON_Shaders *sh_data = &g_shaders[sh_cfg];
  if (sh_data->bone_envelope_distance == NULL) {
    const GPUShaderConfigData *sh_cfg_data = &GPU_shader_cfg_data[sh_cfg];
    sh_data->bone_envelope_distance = GPU_shader_create_from_arrays({
        .vert =
            (const char *[]){sh_cfg_data->lib, datatoc_armature_envelope_solid_vert_glsl, NULL},
        .frag = (const char *[]){datatoc_armature_envelope_distance_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });
  }

  DRW_shgroup_instance_format(g_formats.instance_bone_envelope_distance,
                              {
                                  {"headSphere", DRW_ATTR_FLOAT, 4},
                                  {"tailSphere", DRW_ATTR_FLOAT, 4},
                                  {"xAxis", DRW_ATTR_FLOAT, 3},
                              });

  DRWShadingGroup *grp = DRW_shgroup_create(sh_data->bone_envelope_distance, pass);
  if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
    DRW_shgroup_world_clip_planes_from_rv3d(grp, DRW_context_state_get()->rv3d);
  }
  return DRW_shgroup_call_buffer_instance(
      grp, g_formats.instance_bone_envelope_distance, DRW_cache_bone_envelope_solid_get());
}

struct DRWCallBuffer *buffer_instance_bone_envelope_solid(DRWPass *pass,
                                                          bool transp,
                                                          eGPUShaderConfig sh_cfg)
{
  COMMON_Shaders *sh_data = &g_shaders[sh_cfg];
  if (sh_data->bone_envelope == NULL) {
    const GPUShaderConfigData *sh_cfg_data = &GPU_shader_cfg_data[sh_cfg];
    sh_data->bone_envelope = GPU_shader_create_from_arrays({
        .vert =
            (const char *[]){sh_cfg_data->lib, datatoc_armature_envelope_solid_vert_glsl, NULL},
        .frag = (const char *[]){datatoc_armature_envelope_solid_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });
  }

  DRW_shgroup_instance_format(g_formats.instance_bone_envelope,
                              {
                                  {"headSphere", DRW_ATTR_FLOAT, 4},
                                  {"tailSphere", DRW_ATTR_FLOAT, 4},
                                  {"boneColor", DRW_ATTR_FLOAT, 3},
                                  {"stateColor", DRW_ATTR_FLOAT, 3},
                                  {"xAxis", DRW_ATTR_FLOAT, 3},
                              });

  DRWShadingGroup *grp = DRW_shgroup_create(sh_data->bone_envelope, pass);
  /* We can have a lot of overdraw if we don't do this. Also envelope are not subject to
   * inverted matrix. */
  DRW_shgroup_state_enable(grp, DRW_STATE_CULL_BACK);
  DRW_shgroup_uniform_float_copy(grp, "alpha", transp ? 0.6f : 1.0f);
  if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
    DRW_shgroup_world_clip_planes_from_rv3d(grp, DRW_context_state_get()->rv3d);
  }
  return DRW_shgroup_call_buffer_instance(
      grp, g_formats.instance_bone_envelope, DRW_cache_bone_envelope_solid_get());
}

struct DRWCallBuffer *buffer_instance_mball_handles(DRWPass *pass, eGPUShaderConfig sh_cfg)
{
  COMMON_Shaders *sh_data = &g_shaders[sh_cfg];
  if (sh_data->mball_handles == NULL) {
    const GPUShaderConfigData *sh_cfg_data = &GPU_shader_cfg_data[sh_cfg];
    sh_data->mball_handles = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib, datatoc_object_mball_handles_vert_glsl, NULL},
        .frag = (const char *[]){datatoc_gpu_shader_flat_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });
  }

  DRW_shgroup_instance_format(g_formats.instance_mball_handles,
                              {
                                  {"ScaleTranslationMatrix", DRW_ATTR_FLOAT, 12},
                                  {"radius", DRW_ATTR_FLOAT, 1},
                                  {"color", DRW_ATTR_FLOAT, 3},
                              });

  DRWShadingGroup *grp = DRW_shgroup_create(sh_data->mball_handles, pass);
  DRW_shgroup_uniform_vec3(grp, "screen_vecs[0]", DRW_viewport_screenvecs_get(), 2);
  if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
    DRW_shgroup_world_clip_planes_from_rv3d(grp, DRW_context_state_get()->rv3d);
  }
  return DRW_shgroup_call_buffer_instance(
      grp, g_formats.instance_mball_handles, DRW_cache_screenspace_circle_get());
}

/* Only works with batches with adjacency infos. */
struct DRWCallBuffer *buffer_instance_bone_shape_outline(DRWPass *pass,
                                                         struct GPUBatch *geom,
                                                         eGPUShaderConfig sh_cfg)
{
  COMMON_Shaders *sh_data = &g_shaders[sh_cfg];
  if (sh_data->shape_outline == NULL) {
    const GPUShaderConfigData *sh_cfg_data = &GPU_shader_cfg_data[sh_cfg];
    sh_data->shape_outline = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_armature_shape_outline_vert_glsl,
                                 NULL},
        .geom = (const char *[]){sh_cfg_data->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_armature_shape_outline_geom_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_flat_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });
  }

  DRW_shgroup_instance_format(g_formats.instance_bone_outline,
                              {
                                  {"InstanceModelMatrix", DRW_ATTR_FLOAT, 16},
                                  {"outlineColorSize", DRW_ATTR_FLOAT, 4},
                              });

  DRWShadingGroup *grp = DRW_shgroup_create(sh_data->shape_outline, pass);
  DRW_shgroup_uniform_vec2(grp, "viewportSize", DRW_viewport_size_get(), 1);
  if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
    DRW_shgroup_world_clip_planes_from_rv3d(grp, DRW_context_state_get()->rv3d);
  }
  return DRW_shgroup_call_buffer_instance(grp, g_formats.instance_bone_outline, geom);
}

struct DRWCallBuffer *buffer_instance_bone_shape_solid(DRWPass *pass,
                                                       struct GPUBatch *geom,
                                                       bool transp,
                                                       eGPUShaderConfig sh_cfg)
{
  COMMON_Shaders *sh_data = &g_shaders[sh_cfg];
  if (sh_data->shape_solid == NULL) {
    const GPUShaderConfigData *sh_cfg_data = &GPU_shader_cfg_data[sh_cfg];
    sh_data->shape_solid = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_armature_shape_solid_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_armature_shape_solid_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });
  }

  DRW_shgroup_instance_format(g_formats.instance_bone,
                              {
                                  {"InstanceModelMatrix", DRW_ATTR_FLOAT, 16},
                                  {"boneColor", DRW_ATTR_FLOAT, 3},
                                  {"stateColor", DRW_ATTR_FLOAT, 3},
                              });

  DRWShadingGroup *grp = DRW_shgroup_create(sh_data->shape_solid, pass);
  DRW_shgroup_uniform_float_copy(grp, "alpha", transp ? 0.6f : 1.0f);
  if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
    DRW_shgroup_world_clip_planes_from_rv3d(grp, DRW_context_state_get()->rv3d);
  }
  return DRW_shgroup_call_buffer_instance(grp, g_formats.instance_bone, geom);
}

struct DRWCallBuffer *buffer_instance_bone_sphere_solid(DRWPass *pass,
                                                        bool transp,
                                                        eGPUShaderConfig sh_cfg)
{
  COMMON_Shaders *sh_data = &g_shaders[sh_cfg];
  if (sh_data->bone_sphere == NULL) {
    const GPUShaderConfigData *sh_cfg_data = &GPU_shader_cfg_data[sh_cfg];
    sh_data->bone_sphere = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib, datatoc_armature_sphere_solid_vert_glsl, NULL},
        .frag = (const char *[]){datatoc_armature_sphere_solid_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });
  }

  DRW_shgroup_instance_format(g_formats.instance_bone,
                              {
                                  {"InstanceModelMatrix", DRW_ATTR_FLOAT, 16},
                                  {"boneColor", DRW_ATTR_FLOAT, 3},
                                  {"stateColor", DRW_ATTR_FLOAT, 3},
                              });

  DRWShadingGroup *grp = DRW_shgroup_create(sh_data->bone_sphere, pass);
  /* More transparent than the shape to be less distractive. */
  DRW_shgroup_uniform_float_copy(grp, "alpha", transp ? 0.4f : 1.0f);
  if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
    DRW_shgroup_world_clip_planes_from_rv3d(grp, DRW_context_state_get()->rv3d);
  }
  return DRW_shgroup_call_buffer_instance(
      grp, g_formats.instance_bone, DRW_cache_bone_point_get());
}

struct DRWCallBuffer *buffer_instance_bone_sphere_outline(DRWPass *pass, eGPUShaderConfig sh_cfg)
{
  COMMON_Shaders *sh_data = &g_shaders[sh_cfg];
  if (sh_data->bone_sphere_outline == NULL) {
    const GPUShaderConfigData *sh_cfg_data = &GPU_shader_cfg_data[sh_cfg];
    sh_data->bone_sphere_outline = GPU_shader_create_from_arrays({
        .vert =
            (const char *[]){sh_cfg_data->lib, datatoc_armature_sphere_outline_vert_glsl, NULL},
        .frag = (const char *[]){datatoc_gpu_shader_flat_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });
  }

  DRW_shgroup_instance_format(g_formats.instance_bone_outline,
                              {
                                  {"InstanceModelMatrix", DRW_ATTR_FLOAT, 16},
                                  {"outlineColorSize", DRW_ATTR_FLOAT, 4},
                              });

  DRWShadingGroup *grp = DRW_shgroup_create(sh_data->bone_sphere_outline, pass);
  DRW_shgroup_uniform_vec2(grp, "viewportSize", DRW_viewport_size_get(), 1);
  if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
    DRW_shgroup_world_clip_planes_from_rv3d(grp, DRW_context_state_get()->rv3d);
  }
  return DRW_shgroup_call_buffer_instance(
      grp, g_formats.instance_bone_outline, DRW_cache_bone_point_wire_outline_get());
}

struct DRWCallBuffer *buffer_instance_bone_stick(DRWPass *pass, eGPUShaderConfig sh_cfg)
{
  COMMON_Shaders *sh_data = &g_shaders[sh_cfg];
  if (sh_data->bone_stick == NULL) {
    const GPUShaderConfigData *sh_cfg_data = &GPU_shader_cfg_data[sh_cfg];
    sh_data->bone_stick = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib, datatoc_armature_stick_vert_glsl, NULL},
        .frag = (const char *[]){datatoc_armature_stick_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });
  }

  DRW_shgroup_instance_format(
      g_formats.instance_bone_stick,
      {
          {"boneStart", DRW_ATTR_FLOAT, 3},
          {"boneEnd", DRW_ATTR_FLOAT, 3},
          {"wireColor", DRW_ATTR_FLOAT, 4}, /* TODO port theses to uchar color */
          {"boneColor", DRW_ATTR_FLOAT, 4},
          {"headColor", DRW_ATTR_FLOAT, 4},
          {"tailColor", DRW_ATTR_FLOAT, 4},
      });

  DRWShadingGroup *grp = DRW_shgroup_create(sh_data->bone_stick, pass);
  DRW_shgroup_uniform_vec2(grp, "viewportSize", DRW_viewport_size_get(), 1);
  DRW_shgroup_uniform_float_copy(grp, "stickSize", 5.0f * U.pixelsize);
  if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
    DRW_shgroup_world_clip_planes_from_rv3d(grp, DRW_context_state_get()->rv3d);
  }
  return DRW_shgroup_call_buffer_instance(
      grp, g_formats.instance_bone_stick, DRW_cache_bone_stick_get());
}

struct DRWCallBuffer *buffer_instance_bone_dof(struct DRWPass *pass,
                                               struct GPUBatch *geom,
                                               bool blend)
{
  COMMON_Shaders *sh_data = &g_shaders[GPU_SHADER_CFG_DEFAULT];
  if (sh_data->bone_dofs == NULL) {
    sh_data->bone_dofs = DRW_shader_create(
        datatoc_armature_dof_vert_glsl, NULL, datatoc_gpu_shader_flat_color_frag_glsl, NULL);
  }

  DRW_shgroup_instance_format(g_formats.instance_bone_dof,
                              {
                                  {"InstanceModelMatrix", DRW_ATTR_FLOAT, 16},
                                  {"color", DRW_ATTR_FLOAT, 4},
                                  {"amin", DRW_ATTR_FLOAT, 2},
                                  {"amax", DRW_ATTR_FLOAT, 2},
                              });

  DRWShadingGroup *grp = DRW_shgroup_create(sh_data->bone_dofs, pass);
  if (blend) {
    DRW_shgroup_state_enable(grp, DRW_STATE_BLEND);
    DRW_shgroup_state_disable(grp, DRW_STATE_CULL_FRONT);
  }
  return DRW_shgroup_call_buffer_instance(grp, g_formats.instance_bone_dof, geom);
}

struct GPUShader *mpath_line_shader_get(void)
{
  COMMON_Shaders *sh_data = &g_shaders[GPU_SHADER_CFG_DEFAULT];
  if (sh_data->mpath_line_sh == NULL) {
    sh_data->mpath_line_sh = DRW_shader_create_with_lib(
        datatoc_animviz_mpath_lines_vert_glsl,
        datatoc_animviz_mpath_lines_geom_glsl,
        datatoc_gpu_shader_3D_smooth_color_frag_glsl,
        datatoc_common_globals_lib_glsl,
        NULL);
  }
  return sh_data->mpath_line_sh;
}

struct GPUShader *mpath_points_shader_get(void)
{
  COMMON_Shaders *sh_data = &g_shaders[GPU_SHADER_CFG_DEFAULT];
  if (sh_data->mpath_points_sh == NULL) {
    sh_data->mpath_points_sh = DRW_shader_create_with_lib(
        datatoc_animviz_mpath_points_vert_glsl,
        NULL,
        datatoc_gpu_shader_point_varying_color_frag_glsl,
        datatoc_common_globals_lib_glsl,
        NULL);
  }
  return sh_data->mpath_points_sh;
}

struct GPUShader *volume_velocity_shader_get(bool use_needle)
{
  COMMON_Shaders *sh_data = &g_shaders[GPU_SHADER_CFG_DEFAULT];
  if (use_needle) {
    if (sh_data->volume_velocity_needle_sh == NULL) {
      sh_data->volume_velocity_needle_sh = DRW_shader_create_with_lib(
          datatoc_volume_velocity_vert_glsl,
          NULL,
          datatoc_gpu_shader_flat_color_frag_glsl,
          datatoc_common_view_lib_glsl,
          "#define USE_NEEDLE");
    }
    return sh_data->volume_velocity_needle_sh;
  }
  else {
    if (sh_data->volume_velocity_sh == NULL) {
      sh_data->volume_velocity_sh = DRW_shader_create_with_lib(
          datatoc_volume_velocity_vert_glsl,
          NULL,
          datatoc_gpu_shader_flat_color_frag_glsl,
          datatoc_common_view_lib_glsl,
          NULL);
    }
    return sh_data->volume_velocity_sh;
  }
}

/* ******************************************** COLOR UTILS ************************************ */

/* TODO FINISH */
/**
 * Get the wire color theme_id of an object based on it's state
 * \a r_color is a way to get a pointer to the static color var associated
 */
int DRW_object_wire_theme_get(Object *ob, ViewLayer *view_layer, float **r_color)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const bool is_edit = (draw_ctx->object_mode & OB_MODE_EDIT) && (ob->mode & OB_MODE_EDIT);
  const bool active = (view_layer->basact && view_layer->basact->object == ob);
  /* confusing logic here, there are 2 methods of setting the color
   * 'colortab[colindex]' and 'theme_id', colindex overrides theme_id.
   *
   * note: no theme yet for 'colindex' */
  int theme_id = is_edit ? TH_WIRE_EDIT : TH_WIRE;

  if (is_edit) {
    /* fallback to TH_WIRE */
  }
  else if (((G.moving & G_TRANSFORM_OBJ) != 0) && ((ob->base_flag & BASE_SELECTED) != 0)) {
    theme_id = TH_TRANSFORM;
  }
  else {
    /* Sets the 'theme_id' or fallback to wire */
    if ((ob->base_flag & BASE_SELECTED) != 0) {
      theme_id = (active) ? TH_ACTIVE : TH_SELECT;
    }
    else {
      switch (ob->type) {
        case OB_LAMP:
          theme_id = TH_LIGHT;
          break;
        case OB_SPEAKER:
          theme_id = TH_SPEAKER;
          break;
        case OB_CAMERA:
          theme_id = TH_CAMERA;
          break;
        case OB_EMPTY:
          theme_id = TH_EMPTY;
          break;
        case OB_LIGHTPROBE:
          /* TODO add lightprobe color */
          theme_id = TH_EMPTY;
          break;
        default:
          /* fallback to TH_WIRE */
          break;
      }
    }
  }

  if (r_color != NULL) {
    if (UNLIKELY(ob->base_flag & BASE_FROM_SET)) {
      *r_color = G_draw.block.colorDupli;
    }
    else if (UNLIKELY(ob->base_flag & BASE_FROM_DUPLI)) {
      switch (theme_id) {
        case TH_ACTIVE:
        case TH_SELECT:
          *r_color = G_draw.block.colorDupliSelect;
          break;
        case TH_TRANSFORM:
          *r_color = G_draw.block.colorTransform;
          break;
        default:
          *r_color = G_draw.block.colorDupli;
          break;
      }
    }
    else {
      switch (theme_id) {
        case TH_WIRE_EDIT:
          *r_color = G_draw.block.colorWireEdit;
          break;
        case TH_ACTIVE:
          *r_color = G_draw.block.colorActive;
          break;
        case TH_SELECT:
          *r_color = G_draw.block.colorSelect;
          break;
        case TH_TRANSFORM:
          *r_color = G_draw.block.colorTransform;
          break;
        case TH_SPEAKER:
          *r_color = G_draw.block.colorSpeaker;
          break;
        case TH_CAMERA:
          *r_color = G_draw.block.colorCamera;
          break;
        case TH_EMPTY:
          *r_color = G_draw.block.colorEmpty;
          break;
        case TH_LIGHT:
          *r_color = G_draw.block.colorLight;
          break;
        default:
          *r_color = G_draw.block.colorWire;
          break;
      }
    }
  }

  return theme_id;
}

/* XXX This is very stupid, better find something more general. */
float *DRW_color_background_blend_get(int theme_id)
{
  static float colors[11][4];
  float *ret;

  switch (theme_id) {
    case TH_WIRE_EDIT:
      ret = colors[0];
      break;
    case TH_ACTIVE:
      ret = colors[1];
      break;
    case TH_SELECT:
      ret = colors[2];
      break;
    case TH_TRANSFORM:
      ret = colors[5];
      break;
    case TH_SPEAKER:
      ret = colors[6];
      break;
    case TH_CAMERA:
      ret = colors[7];
      break;
    case TH_EMPTY:
      ret = colors[8];
      break;
    case TH_LIGHT:
      ret = colors[9];
      break;
    default:
      ret = colors[10];
      break;
  }

  UI_GetThemeColorBlendShade4fv(theme_id, TH_BACK, 0.5, 0, ret);

  return ret;
}

bool DRW_object_is_flat(Object *ob, int *axis)
{
  float dim[3];

  if (!ELEM(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_MBALL)) {
    /* Non-meshes object cannot be considered as flat. */
    return false;
  }

  BKE_object_dimensions_get(ob, dim);
  if (dim[0] == 0.0f) {
    *axis = 0;
    return true;
  }
  else if (dim[1] == 0.0f) {
    *axis = 1;
    return true;
  }
  else if (dim[2] == 0.0f) {
    *axis = 2;
    return true;
  }
  return false;
}

bool DRW_object_axis_orthogonal_to_view(Object *ob, int axis)
{
  float ob_rot[3][3], invviewmat[4][4];
  DRW_viewport_matrix_get(invviewmat, DRW_MAT_VIEWINV);
  BKE_object_rot_to_mat3(ob, ob_rot, true);
  float dot = dot_v3v3(ob_rot[axis], invviewmat[2]);
  if (fabsf(dot) < 1e-3) {
    return true;
  }

  return false;
}

static void DRW_evaluate_weight_to_color(const float weight, float result[4])
{
  if (U.flag & USER_CUSTOM_RANGE) {
    BKE_colorband_evaluate(&U.coba_weight, weight, result);
  }
  else {
    /* Use gamma correction to even out the color bands:
     * increasing widens yellow/cyan vs red/green/blue.
     * Gamma 1.0 produces the original 2.79 color ramp. */
    const float gamma = 1.5f;
    float hsv[3] = {(2.0f / 3.0f) * (1.0f - weight), 1.0f, pow(0.5f + 0.5f * weight, gamma)};

    hsv_to_rgb_v(hsv, result);

    for (int i = 0; i < 3; i++) {
      result[i] = pow(result[i], 1.0f / gamma);
    }
  }
}

static GPUTexture *DRW_create_weight_colorramp_texture(void)
{
  char error[256];
  float pixels[256][4];
  for (int i = 0; i < 256; i++) {
    DRW_evaluate_weight_to_color(i / 255.0f, pixels[i]);
    pixels[i][3] = 1.0f;
  }

  return GPU_texture_create_1d(256, GPU_RGBA8, pixels[0], error);
}
