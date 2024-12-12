/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "draw_common_shader_shared.hh"

struct FluidModifierData;
struct GPUMaterial;
struct GPUTexture;
struct GPUUniformBuf;

namespace blender::gpu {
class VertBuf;
}
struct ModifierData;
struct Object;
struct ParticleSystem;
struct RegionView3D;
struct ViewLayer;
struct Scene;
struct DRWData;
namespace blender::draw {
struct CurvesUniformBufPool;
class Manager;
class CurveRefinePass;
}  // namespace blender::draw

/* Keep in sync with globalsBlock in shaders */
BLI_STATIC_ASSERT_ALIGN(GlobalsUboStorage, 16)

void DRW_globals_update();
void DRW_globals_free();

/* draw_hair.cc */

/**
 * \note Only valid after #DRW_curves_update().
 */
blender::gpu::VertBuf *DRW_hair_pos_buffer_get(Object *object,
                                               ParticleSystem *psys,
                                               ModifierData *md);
void DRW_hair_duplimat_get(Object *object,
                           ParticleSystem *psys,
                           ModifierData *md,
                           float (*dupli_mat)[4]);

void DRW_hair_init();
void DRW_hair_free();

/* draw_curves.cc */

namespace blender::draw {

/**
 * \note Only valid after #DRW_curves_update().
 */
gpu::VertBuf *DRW_curves_pos_buffer_get(Object *object);

void DRW_curves_init(DRWData *drw_data);
void DRW_curves_ubos_pool_free(CurvesUniformBufPool *pool);
void DRW_curves_refine_pass_free(CurveRefinePass *pass);
void DRW_curves_update(draw::Manager &manager);
void DRW_curves_free();

/* draw_pointcloud.cc */

void DRW_pointcloud_init();
void DRW_pointcloud_free();

}  // namespace blender::draw

/* draw_volume.cc */

void DRW_volume_init(DRWData *drw_data);
void DRW_volume_ubos_pool_free(void *pool);
void DRW_volume_free();

/* `draw_fluid.cc` */

/* Fluid simulation. */
void DRW_smoke_ensure(FluidModifierData *fmd, int highres);
void DRW_smoke_ensure_coba_field(FluidModifierData *fmd);
void DRW_smoke_ensure_velocity(FluidModifierData *fmd);
void DRW_fluid_ensure_flags(FluidModifierData *fmd);
void DRW_fluid_ensure_range_field(FluidModifierData *fmd);

void DRW_smoke_free(FluidModifierData *fmd);

void DRW_smoke_init(DRWData *drw_data);
void DRW_smoke_exit(DRWData *drw_data);

/* `draw_common.cc` */

struct DRW_Global {
  /** If needed, contains all global/Theme colors
   * Add needed theme colors / values to DRW_globals_update() and update UBO
   * Not needed for constant color. */
  GlobalsUboStorage block;
  /** Define "globalsBlock" uniform for 'block'. */
  GPUUniformBuf *block_ubo;

  GPUTexture *ramp;
  GPUTexture *weight_ramp;
};
extern DRW_Global G_draw;
