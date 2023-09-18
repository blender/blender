/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 */

#include "BKE_lib_id.h"
#include "BKE_node.hh"
#include "BKE_world.h"
#include "DEG_depsgraph_query.h"
#include "NOD_shader.h"

#include "eevee_instance.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name Default Material
 *
 * \{ */

DefaultWorldNodeTree::DefaultWorldNodeTree()
{
  bNodeTree *ntree = ntreeAddTree(nullptr, "World Nodetree", ntreeType_Shader->idname);
  bNode *background = nodeAddStaticNode(nullptr, ntree, SH_NODE_BACKGROUND);
  bNode *output = nodeAddStaticNode(nullptr, ntree, SH_NODE_OUTPUT_WORLD);
  bNodeSocket *background_out = nodeFindSocket(background, SOCK_OUT, "Background");
  bNodeSocket *output_in = nodeFindSocket(output, SOCK_IN, "Surface");
  nodeAddLink(ntree, background, background_out, output, output_in);
  nodeSetActive(ntree, output);

  color_socket_ =
      (bNodeSocketValueRGBA *)nodeFindSocket(background, SOCK_IN, "Color")->default_value;
  ntree_ = ntree;
}

DefaultWorldNodeTree::~DefaultWorldNodeTree()
{
  ntreeFreeEmbeddedTree(ntree_);
  MEM_SAFE_FREE(ntree_);
}

/* Configure a default node-tree with the given world. */
bNodeTree *DefaultWorldNodeTree::nodetree_get(::World *wo)
{
  /* WARNING: This function is not thread-safe. Which is not a problem for the moment. */
  copy_v3_fl3(color_socket_->value, wo->horr, wo->horg, wo->horb);
  return ntree_;
}

/** \} */

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
    default_world_ = static_cast<::World *>(BKE_id_new_nomain(ID_WO, "EEVEEE default world"));
    copy_v3_fl(&default_world_->horr, 0.0f);
    default_world_->use_nodes = 0;
    default_world_->nodetree = nullptr;
    BLI_listbase_clear(&default_world_->gpumaterial);
  }
  return default_world_;
}

void World::world_and_ntree_get(::World *&world, bNodeTree *&ntree)
{
  world = inst_.scene->world;
  if (world == nullptr) {
    world = default_world_get();
  }

  ntree = (world->nodetree && world->use_nodes && !inst_.use_studio_light()) ?
              world->nodetree :
              default_tree.nodetree_get(world);
}

void World::sync()
{
  ::World *bl_world;
  bNodeTree *ntree;
  world_and_ntree_get(bl_world, ntree);

  GPUMaterial *volume_gpumat = inst_.shaders.world_shader_get(bl_world, ntree, MAT_PIPE_VOLUME);
  inst_.pipelines.world_volume.sync(volume_gpumat);

  if (inst_.lookdev.sync_world()) {
    return;
  }

  WorldHandle &wo_handle = inst_.sync.sync_world(bl_world);
  inst_.reflection_probes.sync_world(bl_world, wo_handle);
  if (wo_handle.recalc != 0) {
    inst_.reflection_probes.do_world_update_set(true);
  }
  wo_handle.reset_recalc_flag();

  /* TODO(fclem) This should be detected to scene level. */
  ::World *orig_world = (::World *)DEG_get_original_id(&bl_world->id);
  if (assign_if_different(prev_original_world, orig_world)) {
    inst_.sampling.reset();
  }

  GPUMaterial *gpumat = inst_.shaders.world_shader_get(bl_world, ntree, MAT_PIPE_DEFERRED);

  inst_.manager->register_layer_attributes(gpumat);

  inst_.pipelines.background.sync(gpumat, inst_.film.background_opacity_get());
  inst_.pipelines.world.sync(gpumat);
}

bool World::has_volume()
{
  ::World *bl_world;
  bNodeTree *ntree;
  world_and_ntree_get(bl_world, ntree);

  GPUMaterial *gpumat = inst_.shaders.world_shader_get(bl_world, ntree, MAT_PIPE_VOLUME);
  return GPU_material_has_volume_output(gpumat);
}

/** \} */

}  // namespace blender::eevee
