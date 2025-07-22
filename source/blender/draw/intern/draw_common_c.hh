/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

struct FluidModifierData;
struct GPUMaterial;
struct GPUUniformBuf;

namespace blender::gpu {
class Texture;
class VertBuf;
}  // namespace blender::gpu
struct ModifierData;
struct Object;
struct ParticleSystem;
struct RegionView3D;
struct ViewLayer;
struct Scene;
struct DRWData;
namespace blender::draw {
class Manager;
struct CurvesModule;
struct PointCloudModule;
struct VolumeModule;
class ObjectRef;
}  // namespace blender::draw

/* draw_hair.cc */

/**
 * \note Only valid after #DRW_curves_update().
 */
blender::gpu::VertBuf *DRW_hair_pos_buffer_get(Object *object,
                                               ParticleSystem *psys,
                                               ModifierData *md);

/* draw_curves.cc */

namespace blender::draw {

/**
 * \note Only valid after #DRW_curves_update().
 */
gpu::VertBuf *DRW_curves_pos_buffer_get(Object *object);

/* If drw_data is nullptr, DST global is accessed to get it. */
void DRW_curves_init(DRWData *drw_data = nullptr);
void DRW_curves_begin_sync(DRWData *drw_data);
void DRW_curves_module_free(draw::CurvesModule *module);
void DRW_curves_update(draw::Manager &manager);

/* draw_pointcloud.cc */

/* If drw_data is nullptr, DST global is accessed to get it. */
void DRW_pointcloud_init(DRWData *drw_data = nullptr);
void DRW_pointcloud_module_free(draw::PointCloudModule *module);

/* draw_volume.cc */

/* If drw_data is nullptr, DST global is accessed to get it. */
void DRW_volume_init(DRWData *drw_data = nullptr);
void DRW_volume_module_free(draw::VolumeModule *module);

}  // namespace blender::draw

/* `draw_fluid.cc` */

/* Fluid simulation. */
void DRW_smoke_ensure(FluidModifierData *fmd, int highres);
void DRW_smoke_ensure_coba_field(FluidModifierData *fmd);
void DRW_smoke_ensure_velocity(FluidModifierData *fmd);
void DRW_fluid_ensure_flags(FluidModifierData *fmd);
void DRW_fluid_ensure_range_field(FluidModifierData *fmd);

void DRW_smoke_free(FluidModifierData *fmd);

void DRW_smoke_begin_sync(DRWData *drw_data);
void DRW_smoke_exit(DRWData *drw_data);
