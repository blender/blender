/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/bake.h"
#include "scene/integrator.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/shader.h"
#include "scene/stats.h"
#include "session/buffers.h"

CCL_NAMESPACE_BEGIN

bool BakeManager::get_baking() const
{
  return use_baking_;
}

void BakeManager::set_baking(Scene *scene, const bool use)
{
  if (use_baking_ == use) {
    return;
  }

  use_baking_ = use;

  /* create device and update scene */
  scene->film->tag_modified();
  scene->integrator->tag_update(scene, Integrator::UPDATE_ALL);

  need_update_ = true;
}

void BakeManager::set_use_camera(const bool use_camera)
{
  if (use_camera_ != use_camera) {
    use_camera_ = use_camera;
    need_update_ = true;
  }
}

void BakeManager::set_use_seed(const bool use_seed)
{
  use_seed_ = use_seed;
}

bool BakeManager::get_use_seed() const
{
  return use_seed_;
}

void BakeManager::device_update(Device * /*device*/,
                                DeviceScene *dscene,
                                Scene *scene,
                                Progress & /* progress */)
{
  if (!need_update()) {
    return;
  }

  KernelBake *kbake = &dscene->data.bake;
  memset(kbake, 0, sizeof(*kbake));

  kbake->use_camera = use_camera_;

  if (use_baking_) {
    const scoped_callback_timer timer([scene](double time) {
      if (scene->update_stats) {
        scene->update_stats->bake.times.add_entry({"device_update", time});
      }
    });

    kbake->use = true;

    int object_index = 0;
    for (Object *object : scene->objects) {
      const Geometry *geom = object->get_geometry();
      if (object->get_is_bake_target() && geom->is_mesh()) {
        kbake->object_index = object_index;
        kbake->tri_offset = geom->prim_offset;
        break;
      }

      object_index++;
    }
  }

  need_update_ = false;
}

void BakeManager::device_free(Device * /*device*/, DeviceScene * /*dscene*/) {}

void BakeManager::tag_update()
{
  need_update_ = true;
}

bool BakeManager::need_update() const
{
  return need_update_;
}

CCL_NAMESPACE_END
