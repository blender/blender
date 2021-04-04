/*
 * Copyright 2011-2013 Blender Foundation
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

#include "render/background.h"
#include "device/device.h"
#include "render/graph.h"
#include "render/integrator.h"
#include "render/nodes.h"
#include "render/scene.h"
#include "render/shader.h"
#include "render/stats.h"

#include "util/util_foreach.h"
#include "util/util_math.h"
#include "util/util_time.h"
#include "util/util_types.h"

CCL_NAMESPACE_BEGIN

NODE_DEFINE(Background)
{
  NodeType *type = NodeType::add("background", create);

  SOCKET_FLOAT(ao_factor, "AO Factor", 0.0f);
  SOCKET_FLOAT(ao_distance, "AO Distance", FLT_MAX);

  SOCKET_BOOLEAN(use_shader, "Use Shader", true);
  SOCKET_BOOLEAN(use_ao, "Use AO", false);
  SOCKET_UINT(visibility, "Visibility", PATH_RAY_ALL_VISIBILITY);

  SOCKET_BOOLEAN(transparent, "Transparent", false);
  SOCKET_BOOLEAN(transparent_glass, "Transparent Glass", false);
  SOCKET_FLOAT(transparent_roughness_threshold, "Transparent Roughness Threshold", 0.0f);

  SOCKET_FLOAT(volume_step_size, "Volume Step Size", 0.1f);

  SOCKET_NODE(shader, "Shader", Shader::get_node_type());

  return type;
}

Background::Background() : Node(get_node_type())
{
  shader = NULL;
}

Background::~Background()
{
}

void Background::device_update(Device *device, DeviceScene *dscene, Scene *scene)
{
  if (!is_modified())
    return;

  scoped_callback_timer timer([scene](double time) {
    if (scene->update_stats) {
      scene->update_stats->background.times.add_entry({"device_update", time});
    }
  });

  device_free(device, dscene);

  Shader *bg_shader = get_shader(scene);

  /* set shader index and transparent option */
  KernelBackground *kbackground = &dscene->data.background;

  kbackground->ao_factor = (use_ao) ? ao_factor : 0.0f;
  kbackground->ao_bounces_factor = ao_factor;
  kbackground->ao_distance = ao_distance;

  kbackground->transparent = transparent;
  kbackground->surface_shader = scene->shader_manager->get_shader_id(bg_shader);

  if (transparent && transparent_glass) {
    /* Square twice, once for principled BSDF convention, and once for
     * faster comparison in kernel with anisotropic roughness. */
    kbackground->transparent_roughness_squared_threshold = sqr(
        sqr(transparent_roughness_threshold));
  }
  else {
    kbackground->transparent_roughness_squared_threshold = -1.0f;
  }

  if (bg_shader->has_volume)
    kbackground->volume_shader = kbackground->surface_shader;
  else
    kbackground->volume_shader = SHADER_NONE;

  kbackground->volume_step_size = volume_step_size * scene->integrator->get_volume_step_rate();

  /* No background node, make world shader invisible to all rays, to skip evaluation in kernel. */
  if (bg_shader->graph->nodes.size() <= 1) {
    kbackground->surface_shader |= SHADER_EXCLUDE_ANY;
  }
  /* Background present, check visibilities */
  else {
    if (!(visibility & PATH_RAY_DIFFUSE))
      kbackground->surface_shader |= SHADER_EXCLUDE_DIFFUSE;
    if (!(visibility & PATH_RAY_GLOSSY))
      kbackground->surface_shader |= SHADER_EXCLUDE_GLOSSY;
    if (!(visibility & PATH_RAY_TRANSMIT))
      kbackground->surface_shader |= SHADER_EXCLUDE_TRANSMIT;
    if (!(visibility & PATH_RAY_VOLUME_SCATTER))
      kbackground->surface_shader |= SHADER_EXCLUDE_SCATTER;
    if (!(visibility & PATH_RAY_CAMERA))
      kbackground->surface_shader |= SHADER_EXCLUDE_CAMERA;
  }

  clear_modified();
}

void Background::device_free(Device * /*device*/, DeviceScene * /*dscene*/)
{
}

void Background::tag_update(Scene *scene)
{
  Shader *bg_shader = get_shader(scene);
  if (bg_shader && bg_shader->is_modified()) {
    /* Tag as modified to update the KernelBackground visibility information.
     * We only tag the use_shader socket as modified as it is related to the shader
     * and to avoid doing unnecessary updates anywhere else. */
    tag_use_shader_modified();
  }

  if (ao_factor_is_modified() || use_ao_is_modified()) {
    scene->integrator->tag_update(scene, Integrator::BACKGROUND_AO_MODIFIED);
  }
}

Shader *Background::get_shader(const Scene *scene)
{
  return (use_shader) ? ((shader) ? shader : scene->default_background) : scene->default_empty;
}

CCL_NAMESPACE_END
