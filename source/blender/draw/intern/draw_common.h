/* SPDX-FileCopyrightText: 2016 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "draw_common_shader_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

struct CurvesUniformBufPool;
struct DRWShadingGroup;
struct FluidModifierData;
struct GPUMaterial;
struct ModifierData;
struct Object;
struct ParticleSystem;
struct RegionView3D;
struct ViewLayer;
struct Scene;
struct DRWData;

/* Keep in sync with globalsBlock in shaders */
BLI_STATIC_ASSERT_ALIGN(GlobalsUboStorage, 16)

void DRW_globals_update(void);
void DRW_globals_free(void);

struct DRWView *DRW_view_create_with_zoffset(const struct DRWView *parent_view,
                                             const struct RegionView3D *rv3d,
                                             float offset);

/**
 * Get the wire color theme_id of an object based on its state
 * \a r_color is a way to get a pointer to the static color var associated
 */
int DRW_object_wire_theme_get(struct Object *ob, struct ViewLayer *view_layer, float **r_color);
float *DRW_color_background_blend_get(int theme_id);

bool DRW_object_is_flat(struct Object *ob, int *r_axis);
bool DRW_object_axis_orthogonal_to_view(struct Object *ob, int axis);

/* draw_hair.cc */

/**
 * This creates a shading group with display hairs.
 * The draw call is already added by this function, just add additional uniforms.
 */
struct DRWShadingGroup *DRW_shgroup_hair_create_sub(struct Object *object,
                                                    struct ParticleSystem *psys,
                                                    struct ModifierData *md,
                                                    struct DRWShadingGroup *shgrp,
                                                    struct GPUMaterial *gpu_material);

/**
 * \note Only valid after #DRW_hair_update().
 */
struct GPUVertBuf *DRW_hair_pos_buffer_get(struct Object *object,
                                           struct ParticleSystem *psys,
                                           struct ModifierData *md);
void DRW_hair_duplimat_get(struct Object *object,
                           struct ParticleSystem *psys,
                           struct ModifierData *md,
                           float (*dupli_mat)[4]);

void DRW_hair_init(void);
void DRW_hair_update(void);
void DRW_hair_free(void);

/* draw_curves.cc */

/**
 * \note Only valid after #DRW_curves_update().
 */
struct GPUVertBuf *DRW_curves_pos_buffer_get(struct Object *object);

struct DRWShadingGroup *DRW_shgroup_curves_create_sub(struct Object *object,
                                                      struct DRWShadingGroup *shgrp,
                                                      struct GPUMaterial *gpu_material);

void DRW_curves_init(struct DRWData *drw_data);
void DRW_curves_ubos_pool_free(struct CurvesUniformBufPool *pool);
void DRW_curves_update(void);
void DRW_curves_free(void);

/* draw_pointcloud.cc */

struct DRWShadingGroup *DRW_shgroup_pointcloud_create_sub(struct Object *object,
                                                          struct DRWShadingGroup *shgrp_parent,
                                                          struct GPUMaterial *gpu_material);
void DRW_pointcloud_init(void);
void DRW_pointcloud_free(void);

/* draw_volume.cc */

/**
 * Add attributes bindings of volume grids to an existing shading group.
 * No draw call is added so the caller can decide how to use the data.
 * \return nullptr if there is nothing to draw.
 */
struct DRWShadingGroup *DRW_shgroup_volume_create_sub(struct Scene *scene,
                                                      struct Object *ob,
                                                      struct DRWShadingGroup *shgrp,
                                                      struct GPUMaterial *gpu_material);

void DRW_volume_init(struct DRWData *drw_data);
void DRW_volume_ubos_pool_free(void *pool);
void DRW_volume_free(void);

/* draw_fluid.c */

/* Fluid simulation. */
void DRW_smoke_ensure(struct FluidModifierData *fmd, int highres);
void DRW_smoke_ensure_coba_field(struct FluidModifierData *fmd);
void DRW_smoke_ensure_velocity(struct FluidModifierData *fmd);
void DRW_fluid_ensure_flags(struct FluidModifierData *fmd);
void DRW_fluid_ensure_range_field(struct FluidModifierData *fmd);

void DRW_smoke_free(struct FluidModifierData *fmd);

void DRW_smoke_init(struct DRWData *drw_data);
void DRW_smoke_exit(struct DRWData *drw_data);

/* draw_common.c */

struct DRW_Global {
  /** If needed, contains all global/Theme colors
   * Add needed theme colors / values to DRW_globals_update() and update UBO
   * Not needed for constant color. */
  GlobalsUboStorage block;
  /** Define "globalsBlock" uniform for 'block'. */
  struct GPUUniformBuf *block_ubo;

  struct GPUTexture *ramp;
  struct GPUTexture *weight_ramp;

  struct GPUUniformBuf *view_ubo;
  struct GPUUniformBuf *clipping_ubo;
};
extern struct DRW_Global G_draw;

#ifdef __cplusplus
}
#endif
