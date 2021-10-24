/*
 * Copyright 2011-2018 Blender Foundation
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

#include "scene/procedural.h"
#include "scene/scene.h"
#include "scene/stats.h"

#include "util/foreach.h"
#include "util/progress.h"

CCL_NAMESPACE_BEGIN

NODE_ABSTRACT_DEFINE(Procedural)
{
  NodeType *type = NodeType::add("procedural_base", NULL);
  return type;
}

Procedural::Procedural(const NodeType *type) : Node(type)
{
}

Procedural::~Procedural()
{
}

ProceduralManager::ProceduralManager()
{
  need_update_ = true;
}

ProceduralManager::~ProceduralManager()
{
}

void ProceduralManager::update(Scene *scene, Progress &progress)
{
  if (!need_update()) {
    return;
  }

  progress.set_status("Updating Procedurals");

  scoped_callback_timer timer([scene](double time) {
    if (scene->update_stats) {
      scene->update_stats->procedurals.times.add_entry({"update", time});
    }
  });

  foreach (Procedural *procedural, scene->procedurals) {
    if (progress.get_cancel()) {
      return;
    }

    procedural->generate(scene, progress);
  }

  if (progress.get_cancel()) {
    return;
  }

  need_update_ = false;
}

void ProceduralManager::tag_update()
{
  need_update_ = true;
}

bool ProceduralManager::need_update() const
{
  return need_update_;
}

CCL_NAMESPACE_END
