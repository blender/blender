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

#ifndef __DRAW_COMMON_H__
#define __DRAW_COMMON_H__

struct DRWPass;
struct DRWShadingGroup;
struct GPUBatch;
struct GPUMaterial;
struct ModifierData;
struct Object;
struct PTCacheEdit;
struct ParticleSystem;
struct ViewLayer;

#define UBO_FIRST_COLOR colorWire
#define UBO_LAST_COLOR colorGridAxisZ

/* Used as ubo but colors can be directly referenced as well */
/* Keep in sync with: common_globals_lib.glsl (globalsBlock) */
/* NOTE! Also keep all color as vec4 and between UBO_FIRST_COLOR and UBO_LAST_COLOR */
typedef struct GlobalsUboStorage {
  /* UBOs data needs to be 16 byte aligned (size of vec4) */
  float colorWire[4];
  float colorWireEdit[4];
  float colorActive[4];
  float colorSelect[4];
  float colorDupliSelect[4];
  float colorDupli[4];
  float colorLibrarySelect[4];
  float colorLibrary[4];
  float colorTransform[4];
  float colorLight[4];
  float colorSpeaker[4];
  float colorCamera[4];
  float colorEmpty[4];
  float colorVertex[4];
  float colorVertexSelect[4];
  float colorVertexUnreferenced[4];
  float colorVertexMissingData[4];
  float colorEditMeshActive[4];
  float colorEdgeSelect[4];
  float colorEdgeSeam[4];
  float colorEdgeSharp[4];
  float colorEdgeCrease[4];
  float colorEdgeBWeight[4];
  float colorEdgeFaceSelect[4];
  float colorEdgeFreestyle[4];
  float colorFace[4];
  float colorFaceSelect[4];
  float colorFaceFreestyle[4];
  float colorNormal[4];
  float colorVNormal[4];
  float colorLNormal[4];
  float colorFaceDot[4];

  float colorDeselect[4];
  float colorOutline[4];
  float colorLightNoAlpha[4];

  float colorBackground[4];
  float colorEditMeshMiddle[4];

  float colorHandleFree[4];
  float colorHandleAuto[4];
  float colorHandleVect[4];
  float colorHandleAlign[4];
  float colorHandleAutoclamp[4];
  float colorHandleSelFree[4];
  float colorHandleSelAuto[4];
  float colorHandleSelVect[4];
  float colorHandleSelAlign[4];
  float colorHandleSelAutoclamp[4];
  float colorNurbUline[4];
  float colorNurbVline[4];
  float colorNurbSelUline[4];
  float colorNurbSelVline[4];
  float colorActiveSpline[4];

  float colorBonePose[4];

  float colorCurrentFrame[4];

  float colorGrid[4];
  float colorGridEmphasise[4];
  float colorGridAxisX[4];
  float colorGridAxisY[4];
  float colorGridAxisZ[4];

  /* NOTE! Put all color before UBO_LAST_COLOR */

  /* Pack individual float at the end of the buffer to avoid alignment errors */
  float sizeLightCenter, sizeLightCircle, sizeLightCircleShadow;
  float sizeVertex, sizeEdge, sizeEdgeFix, sizeFaceDot;
  float gridDistance, gridResolution, gridSubdivisions, gridScale;

  float pad_globalsBlock;
} GlobalsUboStorage;
/* Keep in sync with globalsBlock in shaders */
BLI_STATIC_ASSERT_ALIGN(GlobalsUboStorage, 16)

void DRW_globals_update(void);
void DRW_globals_free(void);

void DRW_shgroup_world_clip_planes_from_rv3d(struct DRWShadingGroup *shgrp,
                                             const RegionView3D *rv3d);

struct DRWShadingGroup *shgroup_dynlines_flat_color(struct DRWPass *pass, eGPUShaderConfig sh_cfg);
struct DRWShadingGroup *shgroup_dynlines_dashed_uniform_color(struct DRWPass *pass,
                                                              const float color[4],
                                                              eGPUShaderConfig sh_cfg);
struct DRWShadingGroup *shgroup_dynpoints_uniform_color(struct DRWPass *pass,
                                                        const float color[4],
                                                        const float *size,
                                                        eGPUShaderConfig sh_cfg);
struct DRWShadingGroup *shgroup_groundlines_uniform_color(struct DRWPass *pass,
                                                          const float color[4],
                                                          eGPUShaderConfig sh_cfg);
struct DRWShadingGroup *shgroup_groundpoints_uniform_color(struct DRWPass *pass,
                                                           const float color[4],
                                                           eGPUShaderConfig sh_cfg);
struct DRWShadingGroup *shgroup_instance_screenspace(struct DRWPass *pass,
                                                     struct GPUBatch *geom,
                                                     const float *size,
                                                     eGPUShaderConfig sh_cfg);
struct DRWShadingGroup *shgroup_instance_solid(struct DRWPass *pass, struct GPUBatch *geom);
struct DRWShadingGroup *shgroup_instance_wire(struct DRWPass *pass, struct GPUBatch *geom);
struct DRWShadingGroup *shgroup_instance_screen_aligned(struct DRWPass *pass,
                                                        struct GPUBatch *geom,
                                                        eGPUShaderConfig sh_cfg);
struct DRWShadingGroup *shgroup_instance_empty_axes(struct DRWPass *pass,
                                                    struct GPUBatch *geom,
                                                    eGPUShaderConfig sh_cfg);
struct DRWShadingGroup *shgroup_instance_scaled(struct DRWPass *pass,
                                                struct GPUBatch *geom,
                                                eGPUShaderConfig sh_cfg);
struct DRWShadingGroup *shgroup_instance(struct DRWPass *pass,
                                         struct GPUBatch *geom,
                                         eGPUShaderConfig sh_cfg);
struct DRWShadingGroup *shgroup_instance_alpha(struct DRWPass *pass,
                                               struct GPUBatch *geom,
                                               eGPUShaderConfig sh_cfg);
struct DRWShadingGroup *shgroup_instance_outline(struct DRWPass *pass,
                                                 struct GPUBatch *geom,
                                                 int *baseid);
struct DRWShadingGroup *shgroup_camera_instance(struct DRWPass *pass,
                                                struct GPUBatch *geom,
                                                eGPUShaderConfig sh_cfg);
struct DRWShadingGroup *shgroup_distance_lines_instance(struct DRWPass *pass,
                                                        struct GPUBatch *geom,
                                                        eGPUShaderConfig sh_cfg);
struct DRWShadingGroup *shgroup_spot_instance(struct DRWPass *pass,
                                              struct GPUBatch *geom,
                                              eGPUShaderConfig sh_cfg);
struct DRWShadingGroup *shgroup_instance_mball_handles(struct DRWPass *pass,
                                                       eGPUShaderConfig sh_cfg);
struct DRWShadingGroup *shgroup_instance_bone_axes(struct DRWPass *pass, eGPUShaderConfig sh_cfg);
struct DRWShadingGroup *shgroup_instance_bone_envelope_distance(struct DRWPass *pass,
                                                                eGPUShaderConfig sh_cfg);
struct DRWShadingGroup *shgroup_instance_bone_envelope_outline(struct DRWPass *pass,
                                                               eGPUShaderConfig sh_cfg);
struct DRWShadingGroup *shgroup_instance_bone_envelope_solid(struct DRWPass *pass,
                                                             bool transp,
                                                             eGPUShaderConfig sh_cfg);
struct DRWShadingGroup *shgroup_instance_bone_shape_outline(struct DRWPass *pass,
                                                            struct GPUBatch *geom,
                                                            eGPUShaderConfig sh_cfg);
struct DRWShadingGroup *shgroup_instance_bone_shape_solid(struct DRWPass *pass,
                                                          struct GPUBatch *geom,
                                                          bool transp,
                                                          eGPUShaderConfig sh_cfg);
struct DRWShadingGroup *shgroup_instance_bone_sphere_outline(struct DRWPass *pass,
                                                             eGPUShaderConfig sh_cfg);
struct DRWShadingGroup *shgroup_instance_bone_sphere_solid(struct DRWPass *pass,
                                                           bool transp,
                                                           eGPUShaderConfig sh_cfg);
struct DRWShadingGroup *shgroup_instance_bone_stick(struct DRWPass *pass, eGPUShaderConfig sh_cfg);
struct DRWShadingGroup *shgroup_instance_bone_dof(struct DRWPass *pass, struct GPUBatch *geom);

struct GPUShader *mpath_line_shader_get(void);
struct GPUShader *mpath_points_shader_get(void);

struct GPUShader *volume_velocity_shader_get(bool use_needle);

int DRW_object_wire_theme_get(struct Object *ob, struct ViewLayer *view_layer, float **r_color);
float *DRW_color_background_blend_get(int theme_id);

bool DRW_object_is_flat(Object *ob, int *axis);
bool DRW_object_axis_orthogonal_to_view(Object *ob, int axis);

/* draw_armature.c */
typedef struct DRWArmaturePasses {
  struct DRWPass *bone_solid;
  struct DRWPass *bone_outline;
  struct DRWPass *bone_wire;
  struct DRWPass *bone_envelope;
  struct DRWPass *bone_axes;
  struct DRWPass *relationship_lines;
  struct GHash *custom_shapes;
} DRWArmaturePasses;

void DRW_shgroup_armature_object(struct Object *ob,
                                 struct ViewLayer *view_layer,
                                 struct DRWArmaturePasses passes,
                                 bool transp);
void DRW_shgroup_armature_pose(struct Object *ob, struct DRWArmaturePasses passes, bool transp);
void DRW_shgroup_armature_edit(struct Object *ob, struct DRWArmaturePasses passes, bool transp);

/* draw_hair.c */

/* This creates a shading group with display hairs.
 * The draw call is already added by this function, just add additional uniforms. */
struct DRWShadingGroup *DRW_shgroup_hair_create(struct Object *object,
                                                struct ParticleSystem *psys,
                                                struct ModifierData *md,
                                                struct DRWPass *hair_pass,
                                                struct GPUShader *shader);

struct DRWShadingGroup *DRW_shgroup_material_hair_create(struct Object *object,
                                                         struct ParticleSystem *psys,
                                                         struct ModifierData *md,
                                                         struct DRWPass *hair_pass,
                                                         struct GPUMaterial *material);

void DRW_hair_init(void);
void DRW_hair_update(void);
void DRW_hair_free(void);

/* pose_mode.c */
bool DRW_pose_mode_armature(struct Object *ob, struct Object *active_ob);

/* draw_common.c */
struct DRW_Global {
  /** If needed, contains all global/Theme colors
   * Add needed theme colors / values to DRW_globals_update() and update UBO
   * Not needed for constant color. */
  GlobalsUboStorage block;
  /** Define "globalsBlock" uniform for 'block'.  */
  struct GPUUniformBuffer *block_ubo;

  struct GPUTexture *ramp;
  struct GPUTexture *weight_ramp;

  struct GPUUniformBuffer *view_ubo;
};
extern struct DRW_Global G_draw;

#endif /* __DRAW_COMMON_H__ */
