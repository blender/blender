/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 */

#include "BKE_lib_id.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "DEG_depsgraph_query.hh"
#include "NOD_shader.h"

#include "eevee_instance.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name World
 *
 * \{ */

World::~World()
{
  if (default_world_ != nullptr) {
    BKE_id_free(nullptr, default_world_);
  }
}

::World *World::default_world_get()
{
  if (default_world_ == nullptr) {
    default_world_ = BKE_id_new_nomain<::World>("EEVEE default world");
    default_world_->nodetree = bke::node_tree_add_tree_embedded(
        nullptr, &default_world_->id, "World Nodetree", ntreeType_Shader->idname);

    BLI_listbase_clear(&default_world_->gpumaterial);
  }
  return default_world_;
}

::World *World::scene_world_get()
{
  return (inst_.scene->world != nullptr) ? inst_.scene->world : default_world_get();
}

float World::sun_threshold()
{
  /* No sun extraction during baking. */
  if (inst_.is_baking()) {
    return 0.0;
  }

  float sun_threshold = scene_world_get()->sun_threshold;
  if (inst_.use_studio_light()) {
    /* Do not call `lookdev_world_.intensity_get()` as it might not be initialized yet. */
    sun_threshold *= inst_.v3d->shading.studiolight_intensity;
  }
  return sun_threshold;
}

void World::sync()
{
  bool has_update = false;

  WorldHandle wo_handle = {0};
  if (inst_.scene->world != nullptr) {
    /* Detect world update before overriding it. */
    wo_handle = inst_.sync.sync_world(*inst_.scene->world);
    has_update = wo_handle.recalc != 0;
  }

  bool wait_ready = true;  // TODO !inst_.is_image_render;

  /* Sync volume first since its result can override the surface world. */
  sync_volume(wo_handle, wait_ready);

  ::World *bl_world;
  if (inst_.use_studio_light()) {
    has_update |= lookdev_world_.sync(LookdevParameters(inst_.v3d));
    bl_world = lookdev_world_.world_get();
  }
  else if ((inst_.view_layer->layflag & SCE_LAY_SKY) == 0) {
    bl_world = default_world_get();
  }
  else if (has_volume_absorption_) {
    bl_world = default_world_get();
  }
  else {
    bl_world = scene_world_get();
  }

  ::World *world_override = DEG_get_evaluated(inst_.depsgraph, inst_.view_layer->world_override);
  if (world_override) {
    bl_world = world_override;
  }

  bNodeTree *ntree = (bl_world->nodetree) ? bl_world->nodetree : default_world_get()->nodetree;

  {
    if (has_volume_absorption_) {
      /* Replace world by black world. */
      bl_world = default_world_get();
    }
  }

  /* We have to manually test here because we have overrides. */
  ::World *orig_world = DEG_get_original(bl_world);
  if (assign_if_different(prev_original_world, orig_world)) {
    has_update = true;
  }

  inst_.light_probes.sync_world(bl_world, has_update);

  if (inst_.is_viewport() && has_update) {
    /* Catch lookdev viewport properties updates. */
    inst_.sampling.reset();
  }

  GPUMaterial *gpumat = inst_.shaders.world_shader_get(
      bl_world, ntree, MAT_PIPE_DEFERRED, !wait_ready);

  if (GPU_material_status(gpumat) == GPU_MAT_FAILED) {
    bl_world = default_world_get();
    ntree = bl_world->nodetree;
    gpumat = inst_.shaders.world_shader_get(bl_world, ntree, MAT_PIPE_DEFERRED, !wait_ready);
  }
  if (GPU_material_status(gpumat) == GPU_MAT_QUEUED) {
    is_ready_ = false;
    return;
  }
  is_ready_ = true;

  inst_.manager->register_layer_attributes(gpumat);

  float opacity = inst_.use_studio_light() ? lookdev_world_.background_opacity_get() :
                                             inst_.film.background_opacity_get();
  float background_blur = inst_.use_studio_light() ? lookdev_world_.background_blur_get() : 0.0;

  inst_.pipelines.background.sync(gpumat, opacity, background_blur);
  inst_.pipelines.world.sync(gpumat);
}

void World::sync_volume(const WorldHandle &world_handle, bool wait_ready)
{
  /* Studio lights have no volume shader. */
  ::World *world = inst_.use_studio_light() ? nullptr : inst_.scene->world;

  GPUMaterial *gpumat = nullptr;

  /* Only the scene world nodetree can have volume shader. */
  if (world && world->nodetree) {
    gpumat = inst_.shaders.world_shader_get(
        world, world->nodetree, MAT_PIPE_VOLUME_MATERIAL, !wait_ready);
  }

  bool had_volume = has_volume_;

  if (gpumat && (GPU_material_status(gpumat) == GPU_MAT_SUCCESS)) {
    has_volume_ = GPU_material_has_volume_output(gpumat);
    has_volume_scatter_ = GPU_material_flag_get(gpumat, GPU_MATFLAG_VOLUME_SCATTER);
    has_volume_absorption_ = GPU_material_flag_get(gpumat, GPU_MATFLAG_VOLUME_ABSORPTION);
  }
  else {
    has_volume_ = has_volume_absorption_ = has_volume_scatter_ = false;
  }

  /* World volume needs to be always synced for correct clearing of parameter buffers. */
  inst_.pipelines.world_volume.sync(gpumat);

  if (has_volume_ || had_volume) {
    inst_.volume.world_sync(world_handle);
  }
}

/** \} */

}  // namespace blender::eevee
