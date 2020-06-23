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
 * Copyright 2019, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.h"

#include "GPU_shader.h"

#include "UI_resources.h"

#include "overlay_private.h"

extern char datatoc_antialiasing_frag_glsl[];
extern char datatoc_antialiasing_vert_glsl[];
extern char datatoc_armature_dof_vert_glsl[];
extern char datatoc_armature_dof_solid_frag_glsl[];
extern char datatoc_armature_envelope_distance_frag_glsl[];
extern char datatoc_armature_envelope_outline_vert_glsl[];
extern char datatoc_armature_envelope_solid_frag_glsl[];
extern char datatoc_armature_envelope_solid_vert_glsl[];
extern char datatoc_armature_shape_outline_geom_glsl[];
extern char datatoc_armature_shape_outline_vert_glsl[];
extern char datatoc_armature_shape_solid_frag_glsl[];
extern char datatoc_armature_shape_solid_vert_glsl[];
extern char datatoc_armature_shape_wire_vert_glsl[];
extern char datatoc_armature_sphere_outline_vert_glsl[];
extern char datatoc_armature_sphere_solid_frag_glsl[];
extern char datatoc_armature_sphere_solid_vert_glsl[];
extern char datatoc_armature_stick_frag_glsl[];
extern char datatoc_armature_stick_vert_glsl[];
extern char datatoc_armature_wire_frag_glsl[];
extern char datatoc_armature_wire_vert_glsl[];
extern char datatoc_background_frag_glsl[];
extern char datatoc_clipbound_vert_glsl[];
extern char datatoc_depth_only_vert_glsl[];
extern char datatoc_edit_curve_handle_geom_glsl[];
extern char datatoc_edit_curve_handle_vert_glsl[];
extern char datatoc_edit_curve_point_vert_glsl[];
extern char datatoc_edit_curve_wire_vert_glsl[];
extern char datatoc_edit_gpencil_canvas_vert_glsl[];
extern char datatoc_edit_gpencil_guide_vert_glsl[];
extern char datatoc_edit_gpencil_vert_glsl[];
extern char datatoc_edit_lattice_point_vert_glsl[];
extern char datatoc_edit_lattice_wire_vert_glsl[];
extern char datatoc_edit_mesh_common_lib_glsl[];
extern char datatoc_edit_mesh_frag_glsl[];
extern char datatoc_edit_mesh_geom_glsl[];
extern char datatoc_edit_mesh_vert_glsl[];
extern char datatoc_edit_mesh_normal_vert_glsl[];
extern char datatoc_edit_mesh_skin_root_vert_glsl[];
extern char datatoc_edit_mesh_analysis_vert_glsl[];
extern char datatoc_edit_mesh_analysis_frag_glsl[];
extern char datatoc_edit_particle_strand_vert_glsl[];
extern char datatoc_edit_particle_point_vert_glsl[];
extern char datatoc_extra_frag_glsl[];
extern char datatoc_extra_vert_glsl[];
extern char datatoc_extra_groundline_vert_glsl[];
extern char datatoc_extra_lightprobe_grid_vert_glsl[];
extern char datatoc_extra_loose_point_vert_glsl[];
extern char datatoc_extra_loose_point_frag_glsl[];
extern char datatoc_extra_point_vert_glsl[];
extern char datatoc_extra_wire_frag_glsl[];
extern char datatoc_extra_wire_vert_glsl[];
extern char datatoc_facing_frag_glsl[];
extern char datatoc_facing_vert_glsl[];
extern char datatoc_grid_frag_glsl[];
extern char datatoc_grid_vert_glsl[];
extern char datatoc_image_frag_glsl[];
extern char datatoc_image_vert_glsl[];
extern char datatoc_motion_path_line_vert_glsl[];
extern char datatoc_motion_path_line_geom_glsl[];
extern char datatoc_motion_path_point_vert_glsl[];
extern char datatoc_outline_detect_frag_glsl[];
extern char datatoc_outline_prepass_frag_glsl[];
extern char datatoc_outline_prepass_geom_glsl[];
extern char datatoc_outline_prepass_vert_glsl[];
extern char datatoc_paint_face_vert_glsl[];
extern char datatoc_paint_point_vert_glsl[];
extern char datatoc_paint_texture_frag_glsl[];
extern char datatoc_paint_texture_vert_glsl[];
extern char datatoc_paint_vertcol_frag_glsl[];
extern char datatoc_paint_vertcol_vert_glsl[];
extern char datatoc_paint_weight_frag_glsl[];
extern char datatoc_paint_weight_vert_glsl[];
extern char datatoc_paint_wire_vert_glsl[];
extern char datatoc_particle_vert_glsl[];
extern char datatoc_particle_frag_glsl[];
extern char datatoc_pointcloud_vert_glsl[];
extern char datatoc_pointcloud_frag_glsl[];
extern char datatoc_sculpt_mask_vert_glsl[];
extern char datatoc_sculpt_mask_frag_glsl[];
extern char datatoc_volume_velocity_vert_glsl[];
extern char datatoc_wireframe_vert_glsl[];
extern char datatoc_wireframe_frag_glsl[];
extern char datatoc_xray_fade_frag_glsl[];

extern char datatoc_gpu_shader_depth_only_frag_glsl[];
extern char datatoc_gpu_shader_point_varying_color_frag_glsl[];
extern char datatoc_gpu_shader_3D_smooth_color_frag_glsl[];
extern char datatoc_gpu_shader_uniform_color_frag_glsl[];
extern char datatoc_gpu_shader_flat_color_frag_glsl[];
extern char datatoc_gpu_shader_point_varying_color_varying_outline_aa_frag_glsl[];
extern char datatoc_gpu_shader_common_obinfos_lib_glsl[];

extern char datatoc_gpencil_common_lib_glsl[];

extern char datatoc_common_colormanagement_lib_glsl[];
extern char datatoc_common_fullscreen_vert_glsl[];
extern char datatoc_common_fxaa_lib_glsl[];
extern char datatoc_common_smaa_lib_glsl[];
extern char datatoc_common_globals_lib_glsl[];
extern char datatoc_common_view_lib_glsl[];

typedef struct OVERLAY_Shaders {
  GPUShader *antialiasing;
  GPUShader *armature_dof_wire;
  GPUShader *armature_dof_solid;
  GPUShader *armature_envelope_outline;
  GPUShader *armature_envelope_solid;
  GPUShader *armature_shape_outline;
  GPUShader *armature_shape_solid;
  GPUShader *armature_shape_wire;
  GPUShader *armature_sphere_outline;
  GPUShader *armature_sphere_solid;
  GPUShader *armature_stick;
  GPUShader *armature_wire;
  GPUShader *background;
  GPUShader *clipbound;
  GPUShader *depth_only;
  GPUShader *edit_curve_handle;
  GPUShader *edit_curve_point;
  GPUShader *edit_curve_wire;
  GPUShader *edit_gpencil_guide_point;
  GPUShader *edit_gpencil_point;
  GPUShader *edit_gpencil_wire;
  GPUShader *edit_lattice_point;
  GPUShader *edit_lattice_wire;
  GPUShader *edit_mesh_vert;
  GPUShader *edit_mesh_edge;
  GPUShader *edit_mesh_edge_flat;
  GPUShader *edit_mesh_face;
  GPUShader *edit_mesh_facedot;
  GPUShader *edit_mesh_skin_root;
  GPUShader *edit_mesh_vnormals;
  GPUShader *edit_mesh_normals;
  GPUShader *edit_mesh_fnormals;
  GPUShader *edit_mesh_analysis;
  GPUShader *edit_particle_strand;
  GPUShader *edit_particle_point;
  GPUShader *extra;
  GPUShader *extra_select;
  GPUShader *extra_groundline;
  GPUShader *extra_wire[2];
  GPUShader *extra_wire_select;
  GPUShader *extra_point;
  GPUShader *extra_lightprobe_grid;
  GPUShader *extra_loose_point;
  GPUShader *facing;
  GPUShader *gpencil_canvas;
  GPUShader *grid;
  GPUShader *image;
  GPUShader *motion_path_line;
  GPUShader *motion_path_vert;
  GPUShader *outline_prepass;
  GPUShader *outline_prepass_gpencil;
  GPUShader *outline_prepass_wire;
  GPUShader *outline_detect;
  GPUShader *paint_face;
  GPUShader *paint_point;
  GPUShader *paint_texture;
  GPUShader *paint_vertcol;
  GPUShader *paint_weight;
  GPUShader *paint_wire;
  GPUShader *particle_dot;
  GPUShader *particle_shape;
  GPUShader *pointcloud_dot;
  GPUShader *sculpt_mask;
  GPUShader *uniform_color;
  GPUShader *volume_velocity_needle_sh;
  GPUShader *volume_velocity_sh;
  GPUShader *wireframe_select;
  GPUShader *wireframe[2];
  GPUShader *xray_fade;
} OVERLAY_Shaders;

static struct {
  OVERLAY_Shaders sh_data[GPU_SHADER_CFG_LEN];
} e_data = {{{NULL}}};

GPUShader *OVERLAY_shader_antialiasing(void)
{
  OVERLAY_Shaders *sh_data = &e_data.sh_data[0];
  if (!sh_data->antialiasing) {
    sh_data->antialiasing = GPU_shader_create_from_arrays({
        .vert = (const char *[]){datatoc_common_globals_lib_glsl,
                                 datatoc_antialiasing_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_common_globals_lib_glsl,
                                 datatoc_antialiasing_frag_glsl,
                                 NULL},
        .defs = (const char *[]){NULL},
    });
  }
  return sh_data->antialiasing;
}

GPUShader *OVERLAY_shader_background(void)
{
  OVERLAY_Shaders *sh_data = &e_data.sh_data[0];
  if (!sh_data->background) {
    sh_data->background = GPU_shader_create_from_arrays({
        .vert = (const char *[]){datatoc_common_fullscreen_vert_glsl, NULL},
        .frag =
            (const char *[]){datatoc_common_globals_lib_glsl, datatoc_background_frag_glsl, NULL},
    });
  }
  return sh_data->background;
}

GPUShader *OVERLAY_shader_clipbound(void)
{
  OVERLAY_Shaders *sh_data = &e_data.sh_data[0];
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[0];
  if (!sh_data->clipbound) {
    sh_data->clipbound = GPU_shader_create_from_arrays({
        .vert = (const char *[]){datatoc_common_view_lib_glsl, datatoc_clipbound_vert_glsl, NULL},
        .frag = (const char *[]){datatoc_gpu_shader_uniform_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->clipbound;
}

GPUShader *OVERLAY_shader_depth_only(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->depth_only) {
    sh_data->depth_only = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_depth_only_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_depth_only_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->depth_only;
}

GPUShader *OVERLAY_shader_edit_mesh_vert(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->edit_mesh_vert) {
    sh_data->edit_mesh_vert = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_edit_mesh_common_lib_glsl,
                                 datatoc_edit_mesh_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_point_varying_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, "#define VERT\n", NULL},
    });
  }
  return sh_data->edit_mesh_vert;
}

GPUShader *OVERLAY_shader_edit_mesh_edge(bool use_flat_interp)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  GPUShader **sh = use_flat_interp ? &sh_data->edit_mesh_edge_flat : &sh_data->edit_mesh_edge;
  if (*sh == NULL) {
    /* Use geometry shader to draw edge wire-frame. This ensure us
     * the same result across platforms and more flexibility.
     * But we pay the cost of running a geometry shader.
     * In the future we might consider using only the vertex shader
     * and loading data manually with buffer textures. */
    const bool use_geom_shader = true;
    const bool use_smooth_wires = (U.gpu_flag & USER_GPU_FLAG_NO_EDIT_MODE_SMOOTH_WIRE) == 0;
    const char *geom_sh_code[] = {use_geom_shader ? sh_cfg->lib : NULL,
                                  datatoc_common_globals_lib_glsl,
                                  datatoc_common_view_lib_glsl,
                                  datatoc_edit_mesh_geom_glsl,
                                  NULL};
    const char *vert_sh_code[] = {sh_cfg->lib,
                                  datatoc_common_globals_lib_glsl,
                                  datatoc_common_view_lib_glsl,
                                  datatoc_edit_mesh_common_lib_glsl,
                                  datatoc_edit_mesh_vert_glsl,
                                  NULL};
    const char *frag_sh_code[] = {sh_cfg->lib,
                                  datatoc_common_globals_lib_glsl,
                                  datatoc_common_view_lib_glsl,
                                  datatoc_edit_mesh_common_lib_glsl,
                                  datatoc_edit_mesh_frag_glsl,
                                  NULL};
    const char *defs[] = {sh_cfg->def,
                          use_geom_shader ? "#define USE_GEOM_SHADER\n" : "",
                          use_smooth_wires ? "#define USE_SMOOTH_WIRE\n" : "",
                          use_flat_interp ? "#define FLAT\n" : "",
                          "#define EDGE\n",
                          NULL};

    *sh = GPU_shader_create_from_arrays({
        .vert = vert_sh_code,
        .frag = frag_sh_code,
        .geom = geom_sh_code,
        .defs = defs,
    });
  }
  return *sh;
}

GPUShader *OVERLAY_shader_armature_sphere(bool use_outline)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  const char extensions[] = "#extension GL_ARB_conservative_depth : enable\n";
  if (use_outline && !sh_data->armature_sphere_outline) {
    sh_data->armature_sphere_outline = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_armature_sphere_outline_vert_glsl,
                                 NULL},
        .frag = (const char *[]){extensions,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_armature_wire_frag_glsl,
                                 NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  else if (!sh_data->armature_sphere_solid) {
    sh_data->armature_sphere_solid = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_armature_sphere_solid_vert_glsl,
                                 NULL},
        .frag = (const char *[]){extensions,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_armature_sphere_solid_frag_glsl,
                                 NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return use_outline ? sh_data->armature_sphere_outline : sh_data->armature_sphere_solid;
}

GPUShader *OVERLAY_shader_armature_shape(bool use_outline)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (use_outline && !sh_data->armature_shape_outline) {
    sh_data->armature_shape_outline = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_armature_shape_outline_vert_glsl,
                                 NULL},
        .geom = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_armature_shape_outline_geom_glsl,
                                 NULL},
        .frag =
            (const char *[]){datatoc_common_view_lib_glsl, datatoc_armature_wire_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  else if (!sh_data->armature_shape_solid) {
    sh_data->armature_shape_solid = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_armature_shape_solid_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_armature_shape_solid_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return use_outline ? sh_data->armature_shape_outline : sh_data->armature_shape_solid;
}

GPUShader *OVERLAY_shader_armature_shape_wire(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->armature_shape_wire) {
    sh_data->armature_shape_wire = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_armature_shape_wire_vert_glsl,
                                 NULL},
        .frag =
            (const char *[]){datatoc_common_view_lib_glsl, datatoc_armature_wire_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->armature_shape_wire;
}

GPUShader *OVERLAY_shader_armature_envelope(bool use_outline)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (use_outline && !sh_data->armature_envelope_outline) {
    sh_data->armature_envelope_outline = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_armature_envelope_outline_vert_glsl,
                                 NULL},
        .frag =
            (const char *[]){datatoc_common_view_lib_glsl, datatoc_armature_wire_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  else if (!sh_data->armature_envelope_solid) {
    sh_data->armature_envelope_solid = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_armature_envelope_solid_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_armature_envelope_solid_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return use_outline ? sh_data->armature_envelope_outline : sh_data->armature_envelope_solid;
}

GPUShader *OVERLAY_shader_armature_stick(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->armature_stick) {
    sh_data->armature_stick = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_armature_stick_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_armature_stick_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->armature_stick;
}

GPUShader *OVERLAY_shader_armature_degrees_of_freedom_wire(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->armature_dof_wire) {
    sh_data->armature_dof_wire = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_armature_dof_vert_glsl,
                                 NULL},
        .frag =
            (const char *[]){datatoc_common_view_lib_glsl, datatoc_armature_wire_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, "#define EDGE\n", NULL},
    });
  }
  return sh_data->armature_dof_wire;
}

GPUShader *OVERLAY_shader_armature_degrees_of_freedom_solid(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->armature_dof_solid) {
    sh_data->armature_dof_solid = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_armature_dof_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_common_view_lib_glsl,
                                 datatoc_armature_dof_solid_frag_glsl,
                                 NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->armature_dof_solid;
}

GPUShader *OVERLAY_shader_armature_wire(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->armature_wire) {
    sh_data->armature_wire = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_armature_wire_vert_glsl,
                                 NULL},
        .frag =
            (const char *[]){datatoc_common_view_lib_glsl, datatoc_armature_wire_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->armature_wire;
}

GPUShader *OVERLAY_shader_edit_curve_handle(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->edit_curve_handle) {
    sh_data->edit_curve_handle = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_edit_curve_handle_vert_glsl,
                                 NULL},
        .geom = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_edit_curve_handle_geom_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_3D_smooth_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->edit_curve_handle;
}

GPUShader *OVERLAY_shader_edit_curve_point(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->edit_curve_point) {
    sh_data->edit_curve_point = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_edit_curve_point_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_point_varying_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->edit_curve_point;
}

GPUShader *OVERLAY_shader_edit_curve_wire(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->edit_curve_wire) {
    sh_data->edit_curve_wire = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_edit_curve_wire_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_flat_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, "#define IN_PLACE_INSTANCES\n", NULL},
    });
  }
  return sh_data->edit_curve_wire;
}

GPUShader *OVERLAY_shader_edit_gpencil_guide_point(void)
{
  OVERLAY_Shaders *sh_data = &e_data.sh_data[0];
  if (!sh_data->edit_gpencil_guide_point) {
    sh_data->edit_gpencil_guide_point = GPU_shader_create_from_arrays({
        .vert = (const char *[]){datatoc_common_view_lib_glsl,
                                 datatoc_edit_gpencil_guide_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_point_varying_color_frag_glsl, NULL},
    });
  }
  return sh_data->edit_gpencil_guide_point;
}

GPUShader *OVERLAY_shader_edit_gpencil_point(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->edit_gpencil_point) {
    sh_data->edit_gpencil_point = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_edit_gpencil_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_point_varying_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, "#define USE_POINTS\n", NULL},
    });
  }
  return sh_data->edit_gpencil_point;
}

GPUShader *OVERLAY_shader_edit_gpencil_wire(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->edit_gpencil_wire) {
    sh_data->edit_gpencil_wire = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_edit_gpencil_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_3D_smooth_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->edit_gpencil_wire;
}

GPUShader *OVERLAY_shader_edit_lattice_point(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->edit_lattice_point) {
    sh_data->edit_lattice_point = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_edit_lattice_point_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_point_varying_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->edit_lattice_point;
}

GPUShader *OVERLAY_shader_edit_lattice_wire(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->edit_lattice_wire) {
    sh_data->edit_lattice_wire = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_edit_lattice_wire_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_3D_smooth_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->edit_lattice_wire;
}

GPUShader *OVERLAY_shader_edit_mesh_face(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->edit_mesh_face) {
    sh_data->edit_mesh_face = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_edit_mesh_common_lib_glsl,
                                 datatoc_edit_mesh_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_3D_smooth_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, "#define FACE\n", NULL},
    });
  }
  return sh_data->edit_mesh_face;
}

GPUShader *OVERLAY_shader_edit_mesh_facedot(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->edit_mesh_facedot) {
    sh_data->edit_mesh_facedot = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_edit_mesh_common_lib_glsl,
                                 datatoc_edit_mesh_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_point_varying_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, "#define FACEDOT\n", NULL},
    });
  }
  return sh_data->edit_mesh_facedot;
}

GPUShader *OVERLAY_shader_edit_mesh_normal(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->edit_mesh_normals) {
    sh_data->edit_mesh_normals = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_edit_mesh_normal_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_flat_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, "#define INSTANCED_ATTR\n", NULL},
    });
  }
  return sh_data->edit_mesh_normals;
}

GPUShader *OVERLAY_shader_edit_mesh_analysis(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->edit_mesh_analysis) {
    sh_data->edit_mesh_analysis = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_edit_mesh_analysis_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_edit_mesh_analysis_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->edit_mesh_analysis;
}

GPUShader *OVERLAY_shader_edit_mesh_skin_root(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->edit_mesh_skin_root) {
    sh_data->edit_mesh_skin_root = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_edit_mesh_skin_root_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_flat_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, "#define INSTANCED_ATTR\n", NULL},
    });
  }
  return sh_data->edit_mesh_skin_root;
}

GPUShader *OVERLAY_shader_edit_particle_strand(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->edit_particle_strand) {
    sh_data->edit_particle_strand = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_edit_particle_strand_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_3D_smooth_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->edit_particle_strand;
}

GPUShader *OVERLAY_shader_edit_particle_point(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->edit_particle_point) {
    sh_data->edit_particle_point = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_edit_particle_point_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_point_varying_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->edit_particle_point;
}

GPUShader *OVERLAY_shader_extra(bool is_select)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  GPUShader **sh = (is_select) ? &sh_data->extra_select : &sh_data->extra;
  if (!*sh) {
    *sh = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_extra_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_common_view_lib_glsl, datatoc_extra_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, (is_select) ? "#define SELECT_EDGES\n" : NULL, NULL},
    });
  }
  return *sh;
}

GPUShader *OVERLAY_shader_extra_grid(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->extra_lightprobe_grid) {
    sh_data->extra_lightprobe_grid = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_gpu_shader_common_obinfos_lib_glsl,
                                 datatoc_extra_lightprobe_grid_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_point_varying_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->extra_lightprobe_grid;
}

GPUShader *OVERLAY_shader_extra_groundline(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->extra_groundline) {
    sh_data->extra_groundline = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_extra_groundline_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_common_view_lib_glsl, datatoc_extra_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->extra_groundline;
}

GPUShader *OVERLAY_shader_extra_wire(bool use_object, bool is_select)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  GPUShader **sh = (is_select) ? &sh_data->extra_wire_select : &sh_data->extra_wire[use_object];
  if (!*sh) {
    char colorids[1024];
    /* NOTE: define all ids we need here. */
    BLI_snprintf(colorids,
                 sizeof(colorids),
                 "#define TH_ACTIVE %d\n"
                 "#define TH_SELECT %d\n"
                 "#define TH_TRANSFORM %d\n"
                 "#define TH_WIRE %d\n"
                 "#define TH_CAMERA_PATH %d\n",
                 TH_ACTIVE,
                 TH_SELECT,
                 TH_TRANSFORM,
                 TH_WIRE,
                 TH_CAMERA_PATH);
    *sh = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_extra_wire_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_common_view_lib_glsl, datatoc_extra_wire_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def,
                                 colorids,
                                 (is_select) ? "#define SELECT_EDGES\n" : "",
                                 (use_object) ? "#define OBJECT_WIRE \n" : NULL,
                                 NULL},
    });
  }
  return *sh;
}

GPUShader *OVERLAY_shader_extra_loose_point(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->extra_loose_point) {
    sh_data->extra_loose_point = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_extra_loose_point_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_common_globals_lib_glsl,
                                 datatoc_extra_loose_point_frag_glsl,
                                 NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->extra_loose_point;
}

GPUShader *OVERLAY_shader_extra_point(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->extra_point) {
    sh_data->extra_point = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_extra_point_vert_glsl,
                                 NULL},
        .frag =
            (const char *[]){datatoc_gpu_shader_point_varying_color_varying_outline_aa_frag_glsl,
                             NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->extra_point;
}

GPUShader *OVERLAY_shader_facing(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->facing) {
    sh_data->facing = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_facing_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_common_globals_lib_glsl, datatoc_facing_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->facing;
}

GPUShader *OVERLAY_shader_gpencil_canvas(void)
{
  OVERLAY_Shaders *sh_data = &e_data.sh_data[0];
  if (!sh_data->gpencil_canvas) {
    sh_data->gpencil_canvas = GPU_shader_create_from_arrays({
        .vert = (const char *[]){datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_edit_gpencil_canvas_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_common_view_lib_glsl, datatoc_extra_frag_glsl, NULL},
    });
  }
  return sh_data->gpencil_canvas;
}

GPUShader *OVERLAY_shader_grid(void)
{
  OVERLAY_Shaders *sh_data = &e_data.sh_data[0];
  if (!sh_data->grid) {
    sh_data->grid = GPU_shader_create_from_arrays({
        .vert = (const char *[]){datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_grid_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_grid_frag_glsl,
                                 NULL},
    });
  }
  return sh_data->grid;
}

GPUShader *OVERLAY_shader_image(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->image) {
    sh_data->image = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_image_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_common_colormanagement_lib_glsl,
                                 datatoc_image_frag_glsl,
                                 NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->image;
}

GPUShader *OVERLAY_shader_motion_path_line(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->motion_path_line) {
    sh_data->motion_path_line = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_motion_path_line_vert_glsl,
                                 NULL},
        .geom = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_motion_path_line_geom_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_3D_smooth_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->motion_path_line;
}

GPUShader *OVERLAY_shader_motion_path_vert(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->motion_path_vert) {
    sh_data->motion_path_vert = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_motion_path_point_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_point_varying_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->motion_path_vert;
}

GPUShader *OVERLAY_shader_outline_prepass(bool use_wire)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (use_wire && !sh_data->outline_prepass_wire) {
    sh_data->outline_prepass_wire = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_gpu_shader_common_obinfos_lib_glsl,
                                 datatoc_outline_prepass_vert_glsl,
                                 NULL},
        .geom = (const char *[]){sh_cfg->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_outline_prepass_geom_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_outline_prepass_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, "#define USE_GEOM\n", NULL},
    });
  }
  else if (!sh_data->outline_prepass) {
    sh_data->outline_prepass = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_gpu_shader_common_obinfos_lib_glsl,
                                 datatoc_outline_prepass_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_outline_prepass_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return use_wire ? sh_data->outline_prepass_wire : sh_data->outline_prepass;
}

GPUShader *OVERLAY_shader_outline_prepass_gpencil(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->outline_prepass_gpencil) {
    sh_data->outline_prepass_gpencil = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_gpencil_common_lib_glsl,
                                 datatoc_gpu_shader_common_obinfos_lib_glsl,
                                 datatoc_outline_prepass_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_common_view_lib_glsl,
                                 datatoc_gpencil_common_lib_glsl,
                                 datatoc_outline_prepass_frag_glsl,
                                 NULL},
        .defs = (const char *[]){sh_cfg->def,
                                 "#define USE_GPENCIL\n",
                                 "#define UNIFORM_RESOURCE_ID\n",
                                 NULL},
    });
  }
  return sh_data->outline_prepass_gpencil;
}

GPUShader *OVERLAY_shader_outline_detect(void)
{
  OVERLAY_Shaders *sh_data = &e_data.sh_data[0];
  if (!sh_data->outline_detect) {
    sh_data->outline_detect = GPU_shader_create_from_arrays({
        .vert = (const char *[]){datatoc_common_fullscreen_vert_glsl, NULL},
        .frag = (const char *[]){datatoc_common_view_lib_glsl,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_outline_detect_frag_glsl,
                                 NULL},
    });
  }
  return sh_data->outline_detect;
}

GPUShader *OVERLAY_shader_paint_face(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->paint_face) {
    sh_data->paint_face = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_paint_face_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_uniform_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->paint_face;
}

GPUShader *OVERLAY_shader_paint_point(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->paint_point) {
    sh_data->paint_point = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_paint_point_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_point_varying_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->paint_point;
}

GPUShader *OVERLAY_shader_paint_texture(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->paint_texture) {
    sh_data->paint_texture = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_paint_texture_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_common_colormanagement_lib_glsl,
                                 datatoc_paint_texture_frag_glsl,
                                 NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->paint_texture;
}

GPUShader *OVERLAY_shader_paint_vertcol(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->paint_vertcol) {
    sh_data->paint_vertcol = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_paint_vertcol_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_common_globals_lib_glsl,
                                 datatoc_paint_vertcol_frag_glsl,
                                 NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->paint_vertcol;
}

GPUShader *OVERLAY_shader_paint_weight(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->paint_weight) {
    sh_data->paint_weight = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_paint_weight_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_common_globals_lib_glsl,
                                 datatoc_paint_weight_frag_glsl,
                                 NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->paint_weight;
}

GPUShader *OVERLAY_shader_paint_wire(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->paint_wire) {
    sh_data->paint_wire = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_paint_wire_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_flat_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->paint_wire;
}

GPUShader *OVERLAY_shader_particle_dot(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->particle_dot) {
    sh_data->particle_dot = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_particle_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_common_view_lib_glsl, datatoc_particle_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, "#define USE_DOTS\n", NULL},
    });
  }
  return sh_data->particle_dot;
}

GPUShader *OVERLAY_shader_particle_shape(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->particle_shape) {
    sh_data->particle_shape = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_particle_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_flat_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, "#define INSTANCED_ATTR\n", NULL},
    });
  }
  return sh_data->particle_shape;
}

GPUShader *OVERLAY_shader_pointcloud_dot(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->pointcloud_dot) {
    sh_data->pointcloud_dot = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_pointcloud_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_pointcloud_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, "#define USE_DOTS\n", NULL},
    });
  }
  return sh_data->pointcloud_dot;
}

GPUShader *OVERLAY_shader_sculpt_mask(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->sculpt_mask) {
    sh_data->sculpt_mask = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_sculpt_mask_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_sculpt_mask_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->sculpt_mask;
}

struct GPUShader *OVERLAY_shader_uniform_color(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->uniform_color) {
    sh_data->uniform_color = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_depth_only_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_uniform_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }
  return sh_data->uniform_color;
}

struct GPUShader *OVERLAY_shader_volume_velocity(bool use_needle)
{
  OVERLAY_Shaders *sh_data = &e_data.sh_data[0];
  if (use_needle && !sh_data->volume_velocity_needle_sh) {
    sh_data->volume_velocity_needle_sh = DRW_shader_create_with_lib(
        datatoc_volume_velocity_vert_glsl,
        NULL,
        datatoc_gpu_shader_flat_color_frag_glsl,
        datatoc_common_view_lib_glsl,
        "#define blender_srgb_to_framebuffer_space(a) a\n"
        "#define USE_NEEDLE\n");
  }
  else if (!sh_data->volume_velocity_sh) {
    sh_data->volume_velocity_sh = DRW_shader_create_with_lib(
        datatoc_volume_velocity_vert_glsl,
        NULL,
        datatoc_gpu_shader_flat_color_frag_glsl,
        datatoc_common_view_lib_glsl,
        "#define blender_srgb_to_framebuffer_space(a) a\n");
  }
  return (use_needle) ? sh_data->volume_velocity_needle_sh : sh_data->volume_velocity_sh;
}

GPUShader *OVERLAY_shader_wireframe_select(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->wireframe_select) {
    sh_data->wireframe_select = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_gpu_shader_common_obinfos_lib_glsl,
                                 datatoc_wireframe_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_wireframe_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, "#define SELECT_EDGES\n", NULL},
    });
  }
  return sh_data->wireframe_select;
}

GPUShader *OVERLAY_shader_wireframe(bool custom_bias)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  if (!sh_data->wireframe[custom_bias]) {
    sh_data->wireframe[custom_bias] = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_gpu_shader_common_obinfos_lib_glsl,
                                 datatoc_wireframe_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_common_view_lib_glsl,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_wireframe_frag_glsl,
                                 NULL},
        .defs = (const char *[]){sh_cfg->def,
                                 custom_bias ? "#define CUSTOM_DEPTH_BIAS\n" : NULL,
                                 NULL},
    });
  }
  return sh_data->wireframe[custom_bias];
}

GPUShader *OVERLAY_shader_xray_fade(void)
{
  OVERLAY_Shaders *sh_data = &e_data.sh_data[0];
  if (!sh_data->xray_fade) {
    sh_data->xray_fade = GPU_shader_create_from_arrays({
        .vert = (const char *[]){datatoc_common_fullscreen_vert_glsl, NULL},
        .frag = (const char *[]){datatoc_xray_fade_frag_glsl, NULL},
    });
  }
  return sh_data->xray_fade;
}

static OVERLAY_InstanceFormats g_formats = {NULL};

OVERLAY_InstanceFormats *OVERLAY_shader_instance_formats_get(void)
{
  DRW_shgroup_instance_format(g_formats.pos,
                              {
                                  {"pos", DRW_ATTR_FLOAT, 3},
                              });
  DRW_shgroup_instance_format(g_formats.pos_color,
                              {
                                  {"pos", DRW_ATTR_FLOAT, 3},
                                  {"color", DRW_ATTR_FLOAT, 4},
                              });
  DRW_shgroup_instance_format(g_formats.instance_pos,
                              {
                                  {"inst_pos", DRW_ATTR_FLOAT, 3},
                              });
  DRW_shgroup_instance_format(g_formats.instance_extra,
                              {
                                  {"color", DRW_ATTR_FLOAT, 4},
                                  {"inst_obmat", DRW_ATTR_FLOAT, 16},
                              });
  DRW_shgroup_instance_format(g_formats.wire_extra,
                              {
                                  {"pos", DRW_ATTR_FLOAT, 3},
                                  {"colorid", DRW_ATTR_INT, 1},
                              });
  DRW_shgroup_instance_format(g_formats.instance_bone,
                              {
                                  {"inst_obmat", DRW_ATTR_FLOAT, 16},
                              });
  DRW_shgroup_instance_format(g_formats.instance_bone_stick,
                              {
                                  {"boneStart", DRW_ATTR_FLOAT, 3},
                                  {"boneEnd", DRW_ATTR_FLOAT, 3},
                                  {"wireColor", DRW_ATTR_FLOAT, 4}, /* TODO uchar color */
                                  {"boneColor", DRW_ATTR_FLOAT, 4},
                                  {"headColor", DRW_ATTR_FLOAT, 4},
                                  {"tailColor", DRW_ATTR_FLOAT, 4},
                              });
  DRW_shgroup_instance_format(g_formats.instance_bone_envelope_outline,
                              {
                                  {"headSphere", DRW_ATTR_FLOAT, 4},
                                  {"tailSphere", DRW_ATTR_FLOAT, 4},
                                  {"outlineColorSize", DRW_ATTR_FLOAT, 4},
                                  {"xAxis", DRW_ATTR_FLOAT, 3},
                              });
  DRW_shgroup_instance_format(g_formats.instance_bone_envelope_distance,
                              {
                                  {"headSphere", DRW_ATTR_FLOAT, 4},
                                  {"tailSphere", DRW_ATTR_FLOAT, 4},
                                  {"xAxis", DRW_ATTR_FLOAT, 3},
                              });
  DRW_shgroup_instance_format(g_formats.instance_bone_envelope,
                              {
                                  {"headSphere", DRW_ATTR_FLOAT, 4},
                                  {"tailSphere", DRW_ATTR_FLOAT, 4},
                                  {"boneColor", DRW_ATTR_FLOAT, 3},
                                  {"stateColor", DRW_ATTR_FLOAT, 3},
                                  {"xAxis", DRW_ATTR_FLOAT, 3},
                              });

  return &g_formats;
}

void OVERLAY_shader_free(void)
{
  for (int sh_data_index = 0; sh_data_index < ARRAY_SIZE(e_data.sh_data); sh_data_index++) {
    OVERLAY_Shaders *sh_data = &e_data.sh_data[sh_data_index];
    GPUShader **sh_data_as_array = (GPUShader **)sh_data;
    for (int i = 0; i < (sizeof(OVERLAY_Shaders) / sizeof(GPUShader *)); i++) {
      DRW_SHADER_FREE_SAFE(sh_data_as_array[i]);
    }
  }
  struct GPUVertFormat **format = (struct GPUVertFormat **)&g_formats;
  for (int i = 0; i < sizeof(g_formats) / sizeof(void *); i++, format++) {
    MEM_SAFE_FREE(*format);
  }
}
