/*
 * Copyright 2011-2014 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "scene/bake.h"
#include "scene/integrator.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/shader.h"
#include "scene/stats.h"
#include "session/buffers.h"

#include "util/foreach.h"

CCL_NAMESPACE_BEGIN

BakeManager::BakeManager()
{
  need_update_ = true;
}

BakeManager::~BakeManager()
{
}

bool BakeManager::get_baking() const
{
  return !object_name.empty();
}

void BakeManager::set(Scene *scene, const std::string &object_name_)
{
  object_name = object_name_;

  /* create device and update scene */
  scene->film->tag_modified();
  scene->integrator->tag_update(scene, Integrator::UPDATE_ALL);

  need_update_ = true;
}

void BakeManager::device_update(Device * /*device*/,
                                DeviceScene *dscene,
                                Scene *scene,
                                Progress & /* progress */)
{
  if (!need_update())
    return;

  KernelBake *kbake = &dscene->data.bake;
  memset(kbake, 0, sizeof(*kbake));

  if (!object_name.empty()) {
    scoped_callback_timer timer([scene](double time) {
      if (scene->update_stats) {
        scene->update_stats->bake.times.add_entry({"device_update", time});
      }
    });

    kbake->use = true;

    int object_index = 0;
    foreach (Object *object, scene->objects) {
      const Geometry *geom = object->get_geometry();
      if (object->name == object_name && geom->geometry_type == Geometry::MESH) {
        kbake->object_index = object_index;
        kbake->tri_offset = geom->prim_offset;
        break;
      }

      object_index++;
    }
  }

  need_update_ = false;
}

void BakeManager::device_free(Device * /*device*/, DeviceScene * /*dscene*/)
{
}

void BakeManager::tag_update()
{
  need_update_ = true;
}

bool BakeManager::need_update() const
{
  return need_update_;
}

CCL_NAMESPACE_END
