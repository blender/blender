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

#include "render/bake.h"
#include "render/buffers.h"
#include "render/integrator.h"
#include "render/mesh.h"
#include "render/object.h"
#include "render/shader.h"

#include "util/util_foreach.h"

CCL_NAMESPACE_BEGIN

static int aa_samples(Scene *scene, Object *object, ShaderEvalType type)
{
  if (type == SHADER_EVAL_UV || type == SHADER_EVAL_ROUGHNESS) {
    return 1;
  }
  else if (type == SHADER_EVAL_NORMAL) {
    /* Only antialias normal if mesh has bump mapping. */
    if (object->geometry) {
      foreach (Shader *shader, object->geometry->used_shaders) {
        if (shader->has_bump) {
          return scene->integrator->aa_samples;
        }
      }
    }

    return 1;
  }
  else {
    return scene->integrator->aa_samples;
  }
}

/* Keep it synced with kernel_bake.h logic */
static int shader_type_to_pass_filter(ShaderEvalType type, int pass_filter)
{
  const int component_flags = pass_filter &
                              (BAKE_FILTER_DIRECT | BAKE_FILTER_INDIRECT | BAKE_FILTER_COLOR);

  switch (type) {
    case SHADER_EVAL_AO:
      return BAKE_FILTER_AO;
    case SHADER_EVAL_SHADOW:
      return BAKE_FILTER_DIRECT;
    case SHADER_EVAL_DIFFUSE:
      return BAKE_FILTER_DIFFUSE | component_flags;
    case SHADER_EVAL_GLOSSY:
      return BAKE_FILTER_GLOSSY | component_flags;
    case SHADER_EVAL_TRANSMISSION:
      return BAKE_FILTER_TRANSMISSION | component_flags;
    case SHADER_EVAL_COMBINED:
      return pass_filter;
    default:
      return 0;
  }
}

BakeManager::BakeManager()
{
  type = SHADER_EVAL_BAKE;
  pass_filter = 0;

  need_update = true;
}

BakeManager::~BakeManager()
{
}

bool BakeManager::get_baking()
{
  return !object_name.empty();
}

void BakeManager::set(Scene *scene,
                      const std::string &object_name_,
                      ShaderEvalType type_,
                      int pass_filter_)
{
  object_name = object_name_;
  type = type_;
  pass_filter = shader_type_to_pass_filter(type_, pass_filter_);

  Pass::add(PASS_BAKE_PRIMITIVE, scene->passes);
  Pass::add(PASS_BAKE_DIFFERENTIAL, scene->passes);

  if (type == SHADER_EVAL_UV) {
    /* force UV to be available */
    Pass::add(PASS_UV, scene->passes);
  }

  /* force use_light_pass to be true if we bake more than just colors */
  if (pass_filter & ~BAKE_FILTER_COLOR) {
    Pass::add(PASS_LIGHT, scene->passes);
  }

  /* create device and update scene */
  scene->film->tag_update(scene);
  scene->integrator->tag_update(scene);

  need_update = true;
}

void BakeManager::device_update(Device * /*device*/,
                                DeviceScene *dscene,
                                Scene *scene,
                                Progress & /* progress */)
{
  if (!need_update)
    return;

  KernelIntegrator *kintegrator = &dscene->data.integrator;
  KernelBake *kbake = &dscene->data.bake;

  kbake->type = type;
  kbake->pass_filter = pass_filter;

  int object_index = 0;
  foreach (Object *object, scene->objects) {
    const Geometry *geom = object->geometry;
    if (object->name == object_name && geom->type == Geometry::MESH) {
      kbake->object_index = object_index;
      kbake->tri_offset = geom->prim_offset;
      kintegrator->aa_samples = aa_samples(scene, object, type);
      break;
    }

    object_index++;
  }

  need_update = false;
}

void BakeManager::device_free(Device * /*device*/, DeviceScene * /*dscene*/)
{
}

CCL_NAMESPACE_END
