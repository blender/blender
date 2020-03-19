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

#include "render/light.h"
#include "device/device.h"
#include "render/background.h"
#include "render/film.h"
#include "render/graph.h"
#include "render/integrator.h"
#include "render/mesh.h"
#include "render/nodes.h"
#include "render/object.h"
#include "render/scene.h"
#include "render/shader.h"

#include "util/util_foreach.h"
#include "util/util_hash.h"
#include "util/util_logging.h"
#include "util/util_path.h"
#include "util/util_progress.h"

CCL_NAMESPACE_BEGIN

static void shade_background_pixels(Device *device,
                                    DeviceScene *dscene,
                                    int width,
                                    int height,
                                    vector<float3> &pixels,
                                    Progress &progress)
{
  /* create input */
  device_vector<uint4> d_input(device, "background_input", MEM_READ_ONLY);
  device_vector<float4> d_output(device, "background_output", MEM_READ_WRITE);

  uint4 *d_input_data = d_input.alloc(width * height);

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      float u = (x + 0.5f) / width;
      float v = (y + 0.5f) / height;

      uint4 in = make_uint4(__float_as_int(u), __float_as_int(v), 0, 0);
      d_input_data[x + y * width] = in;
    }
  }

  /* compute on device */
  d_output.alloc(width * height);
  d_output.zero_to_device();
  d_input.copy_to_device();

  device->const_copy_to("__data", &dscene->data, sizeof(dscene->data));

  DeviceTask main_task(DeviceTask::SHADER);
  main_task.shader_input = d_input.device_pointer;
  main_task.shader_output = d_output.device_pointer;
  main_task.shader_eval_type = SHADER_EVAL_BACKGROUND;
  main_task.shader_x = 0;
  main_task.shader_w = width * height;
  main_task.num_samples = 1;
  main_task.get_cancel = function_bind(&Progress::get_cancel, &progress);

  /* disabled splitting for now, there's an issue with multi-GPU mem_copy_from */
  list<DeviceTask> split_tasks;
  main_task.split(split_tasks, 1, 128 * 128);

  foreach (DeviceTask &task, split_tasks) {
    device->task_add(task);
    device->task_wait();
    d_output.copy_from_device(task.shader_x, 1, task.shader_w);
  }

  d_input.free();

  float4 *d_output_data = d_output.data();

  pixels.resize(width * height);

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      pixels[y * width + x].x = d_output_data[y * width + x].x;
      pixels[y * width + x].y = d_output_data[y * width + x].y;
      pixels[y * width + x].z = d_output_data[y * width + x].z;
    }
  }

  d_output.free();
}

/* Light */

NODE_DEFINE(Light)
{
  NodeType *type = NodeType::add("light", create);

  static NodeEnum type_enum;
  type_enum.insert("point", LIGHT_POINT);
  type_enum.insert("distant", LIGHT_DISTANT);
  type_enum.insert("background", LIGHT_BACKGROUND);
  type_enum.insert("area", LIGHT_AREA);
  type_enum.insert("spot", LIGHT_SPOT);
  SOCKET_ENUM(type, "Type", type_enum, LIGHT_POINT);

  SOCKET_COLOR(strength, "Strength", make_float3(1.0f, 1.0f, 1.0f));

  SOCKET_POINT(co, "Co", make_float3(0.0f, 0.0f, 0.0f));

  SOCKET_VECTOR(dir, "Dir", make_float3(0.0f, 0.0f, 0.0f));
  SOCKET_FLOAT(size, "Size", 0.0f);

  SOCKET_VECTOR(axisu, "Axis U", make_float3(0.0f, 0.0f, 0.0f));
  SOCKET_FLOAT(sizeu, "Size U", 1.0f);
  SOCKET_VECTOR(axisv, "Axis V", make_float3(0.0f, 0.0f, 0.0f));
  SOCKET_FLOAT(sizev, "Size V", 1.0f);
  SOCKET_BOOLEAN(round, "Round", false);

  SOCKET_INT(map_resolution, "Map Resolution", 0);

  SOCKET_FLOAT(spot_angle, "Spot Angle", M_PI_4_F);
  SOCKET_FLOAT(spot_smooth, "Spot Smooth", 0.0f);

  SOCKET_TRANSFORM(tfm, "Transform", transform_identity());

  SOCKET_BOOLEAN(cast_shadow, "Cast Shadow", true);
  SOCKET_BOOLEAN(use_mis, "Use Mis", false);
  SOCKET_BOOLEAN(use_diffuse, "Use Diffuse", true);
  SOCKET_BOOLEAN(use_glossy, "Use Glossy", true);
  SOCKET_BOOLEAN(use_transmission, "Use Transmission", true);
  SOCKET_BOOLEAN(use_scatter, "Use Scatter", true);

  SOCKET_INT(samples, "Samples", 1);
  SOCKET_INT(max_bounces, "Max Bounces", 1024);
  SOCKET_UINT(random_id, "Random ID", 0);

  SOCKET_BOOLEAN(is_portal, "Is Portal", false);
  SOCKET_BOOLEAN(is_enabled, "Is Enabled", true);

  SOCKET_NODE(shader, "Shader", &Shader::node_type);

  return type;
}

Light::Light() : Node(node_type)
{
}

void Light::tag_update(Scene *scene)
{
  scene->light_manager->need_update = true;
}

bool Light::has_contribution(Scene *scene)
{
  if (strength == make_float3(0.0f, 0.0f, 0.0f)) {
    return false;
  }
  if (is_portal) {
    return false;
  }
  if (type == LIGHT_BACKGROUND) {
    return true;
  }
  return (shader) ? shader->has_surface_emission : scene->default_light->has_surface_emission;
}

/* Light Manager */

LightManager::LightManager()
{
  need_update = true;
  use_light_visibility = false;
}

LightManager::~LightManager()
{
  foreach (IESSlot *slot, ies_slots) {
    delete slot;
  }
}

bool LightManager::has_background_light(Scene *scene)
{
  foreach (Light *light, scene->lights) {
    if (light->type == LIGHT_BACKGROUND && light->is_enabled) {
      return true;
    }
  }
  return false;
}

void LightManager::disable_ineffective_light(Scene *scene)
{
  /* Make all lights enabled by default, and perform some preliminary checks
   * needed for finer-tuning of settings (for example, check whether we've
   * got portals or not).
   */
  bool has_portal = false, has_background = false;
  foreach (Light *light, scene->lights) {
    light->is_enabled = light->has_contribution(scene);
    has_portal |= light->is_portal;
    has_background |= light->type == LIGHT_BACKGROUND;
  }

  if (has_background) {
    /* Ignore background light if:
     * - If unsupported on a device
     * - If we don't need it (no HDRs etc.)
     */
    Shader *shader = scene->background->get_shader(scene);
    const bool disable_mis = !(has_portal || shader->has_surface_spatial_varying);
    VLOG_IF(1, disable_mis) << "Background MIS has been disabled.\n";
    foreach (Light *light, scene->lights) {
      if (light->type == LIGHT_BACKGROUND) {
        light->is_enabled = !disable_mis;
      }
    }
  }
}

bool LightManager::object_usable_as_light(Object *object)
{
  Geometry *geom = object->geometry;
  if (geom->type != Geometry::MESH) {
    return false;
  }
  /* Skip objects with NaNs */
  if (!object->bounds.valid()) {
    return false;
  }
  /* Skip if we are not visible for BSDFs. */
  if (!(object->visibility & (PATH_RAY_DIFFUSE | PATH_RAY_GLOSSY | PATH_RAY_TRANSMIT))) {
    return false;
  }
  /* Skip if we have no emission shaders. */
  /* TODO(sergey): Ideally we want to avoid such duplicated loop, since it'll
   * iterate all geometry shaders twice (when counting and when calculating
   * triangle area.
   */
  foreach (const Shader *shader, geom->used_shaders) {
    if (shader->use_mis && shader->has_surface_emission) {
      return true;
    }
  }
  return false;
}

void LightManager::device_update_distribution(Device *,
                                              DeviceScene *dscene,
                                              Scene *scene,
                                              Progress &progress)
{
  progress.set_status("Updating Lights", "Computing distribution");

  /* count */
  size_t num_lights = 0;
  size_t num_portals = 0;
  size_t num_background_lights = 0;
  size_t num_triangles = 0;

  bool background_mis = false;

  foreach (Light *light, scene->lights) {
    if (light->is_enabled) {
      num_lights++;
    }
    if (light->is_portal) {
      num_portals++;
    }
  }

  foreach (Object *object, scene->objects) {
    if (progress.get_cancel())
      return;

    if (!object_usable_as_light(object)) {
      continue;
    }

    /* Count triangles. */
    Mesh *mesh = static_cast<Mesh *>(object->geometry);
    size_t mesh_num_triangles = mesh->num_triangles();
    for (size_t i = 0; i < mesh_num_triangles; i++) {
      int shader_index = mesh->shader[i];
      Shader *shader = (shader_index < mesh->used_shaders.size()) ?
                           mesh->used_shaders[shader_index] :
                           scene->default_surface;

      if (shader->use_mis && shader->has_surface_emission) {
        num_triangles++;
      }
    }
  }

  size_t num_distribution = num_triangles + num_lights;
  VLOG(1) << "Total " << num_distribution << " of light distribution primitives.";

  /* emission area */
  KernelLightDistribution *distribution = dscene->light_distribution.alloc(num_distribution + 1);
  float totarea = 0.0f;

  /* triangles */
  size_t offset = 0;
  int j = 0;

  foreach (Object *object, scene->objects) {
    if (progress.get_cancel())
      return;

    if (!object_usable_as_light(object)) {
      j++;
      continue;
    }
    /* Sum area. */
    Mesh *mesh = static_cast<Mesh *>(object->geometry);
    bool transform_applied = mesh->transform_applied;
    Transform tfm = object->tfm;
    int object_id = j;
    int shader_flag = 0;

    if (!(object->visibility & PATH_RAY_DIFFUSE)) {
      shader_flag |= SHADER_EXCLUDE_DIFFUSE;
      use_light_visibility = true;
    }
    if (!(object->visibility & PATH_RAY_GLOSSY)) {
      shader_flag |= SHADER_EXCLUDE_GLOSSY;
      use_light_visibility = true;
    }
    if (!(object->visibility & PATH_RAY_TRANSMIT)) {
      shader_flag |= SHADER_EXCLUDE_TRANSMIT;
      use_light_visibility = true;
    }
    if (!(object->visibility & PATH_RAY_VOLUME_SCATTER)) {
      shader_flag |= SHADER_EXCLUDE_SCATTER;
      use_light_visibility = true;
    }

    size_t mesh_num_triangles = mesh->num_triangles();
    for (size_t i = 0; i < mesh_num_triangles; i++) {
      int shader_index = mesh->shader[i];
      Shader *shader = (shader_index < mesh->used_shaders.size()) ?
                           mesh->used_shaders[shader_index] :
                           scene->default_surface;

      if (shader->use_mis && shader->has_surface_emission) {
        distribution[offset].totarea = totarea;
        distribution[offset].prim = i + mesh->prim_offset;
        distribution[offset].mesh_light.shader_flag = shader_flag;
        distribution[offset].mesh_light.object_id = object_id;
        offset++;

        Mesh::Triangle t = mesh->get_triangle(i);
        if (!t.valid(&mesh->verts[0])) {
          continue;
        }
        float3 p1 = mesh->verts[t.v[0]];
        float3 p2 = mesh->verts[t.v[1]];
        float3 p3 = mesh->verts[t.v[2]];

        if (!transform_applied) {
          p1 = transform_point(&tfm, p1);
          p2 = transform_point(&tfm, p2);
          p3 = transform_point(&tfm, p3);
        }

        totarea += triangle_area(p1, p2, p3);
      }
    }

    j++;
  }

  float trianglearea = totarea;

  /* point lights */
  float lightarea = (totarea > 0.0f) ? totarea / num_lights : 1.0f;
  bool use_lamp_mis = false;

  int light_index = 0;
  foreach (Light *light, scene->lights) {
    if (!light->is_enabled)
      continue;

    distribution[offset].totarea = totarea;
    distribution[offset].prim = ~light_index;
    distribution[offset].lamp.pad = 1.0f;
    distribution[offset].lamp.size = light->size;
    totarea += lightarea;

    if (light->type == LIGHT_DISTANT) {
      use_lamp_mis |= (light->angle > 0.0f && light->use_mis);
    }
    else if (light->type == LIGHT_POINT || light->type == LIGHT_SPOT) {
      use_lamp_mis |= (light->size > 0.0f && light->use_mis);
    }
    else if (light->type == LIGHT_AREA) {
      use_lamp_mis |= light->use_mis;
    }
    else if (light->type == LIGHT_BACKGROUND) {
      num_background_lights++;
      background_mis |= light->use_mis;
    }

    light_index++;
    offset++;
  }

  /* normalize cumulative distribution functions */
  distribution[num_distribution].totarea = totarea;
  distribution[num_distribution].prim = 0.0f;
  distribution[num_distribution].lamp.pad = 0.0f;
  distribution[num_distribution].lamp.size = 0.0f;

  if (totarea > 0.0f) {
    for (size_t i = 0; i < num_distribution; i++)
      distribution[i].totarea /= totarea;
    distribution[num_distribution].totarea = 1.0f;
  }

  if (progress.get_cancel())
    return;

  /* update device */
  KernelIntegrator *kintegrator = &dscene->data.integrator;
  KernelFilm *kfilm = &dscene->data.film;
  kintegrator->use_direct_light = (totarea > 0.0f);

  if (kintegrator->use_direct_light) {
    /* number of emissives */
    kintegrator->num_distribution = num_distribution;

    /* precompute pdfs */
    kintegrator->pdf_triangles = 0.0f;
    kintegrator->pdf_lights = 0.0f;

    /* sample one, with 0.5 probability of light or triangle */
    kintegrator->num_all_lights = num_lights;

    if (trianglearea > 0.0f) {
      kintegrator->pdf_triangles = 1.0f / trianglearea;
      if (num_lights)
        kintegrator->pdf_triangles *= 0.5f;
    }

    if (num_lights) {
      kintegrator->pdf_lights = 1.0f / num_lights;
      if (trianglearea > 0.0f)
        kintegrator->pdf_lights *= 0.5f;
    }

    kintegrator->use_lamp_mis = use_lamp_mis;

    /* bit of an ugly hack to compensate for emitting triangles influencing
     * amount of samples we get for this pass */
    kfilm->pass_shadow_scale = 1.0f;

    if (kintegrator->pdf_triangles != 0.0f)
      kfilm->pass_shadow_scale *= 0.5f;

    if (num_background_lights < num_lights)
      kfilm->pass_shadow_scale *= (float)(num_lights - num_background_lights) / (float)num_lights;

    /* CDF */
    dscene->light_distribution.copy_to_device();

    /* Portals */
    if (num_portals > 0) {
      kintegrator->portal_offset = light_index;
      kintegrator->num_portals = num_portals;
      kintegrator->portal_pdf = background_mis ? 0.5f : 1.0f;
    }
    else {
      kintegrator->num_portals = 0;
      kintegrator->portal_offset = 0;
      kintegrator->portal_pdf = 0.0f;
    }
  }
  else {
    dscene->light_distribution.free();

    kintegrator->num_distribution = 0;
    kintegrator->num_all_lights = 0;
    kintegrator->pdf_triangles = 0.0f;
    kintegrator->pdf_lights = 0.0f;
    kintegrator->use_lamp_mis = false;
    kintegrator->num_portals = 0;
    kintegrator->portal_offset = 0;
    kintegrator->portal_pdf = 0.0f;

    kfilm->pass_shadow_scale = 1.0f;
  }
}

static void background_cdf(
    int start, int end, int res_x, int res_y, const vector<float3> *pixels, float2 *cond_cdf)
{
  int cdf_width = res_x + 1;
  /* Conditional CDFs (rows, U direction). */
  for (int i = start; i < end; i++) {
    float sin_theta = sinf(M_PI_F * (i + 0.5f) / res_y);
    float3 env_color = (*pixels)[i * res_x];
    float ave_luminance = average(env_color);

    cond_cdf[i * cdf_width].x = ave_luminance * sin_theta;
    cond_cdf[i * cdf_width].y = 0.0f;

    for (int j = 1; j < res_x; j++) {
      env_color = (*pixels)[i * res_x + j];
      ave_luminance = average(env_color);

      cond_cdf[i * cdf_width + j].x = ave_luminance * sin_theta;
      cond_cdf[i * cdf_width + j].y = cond_cdf[i * cdf_width + j - 1].y +
                                      cond_cdf[i * cdf_width + j - 1].x / res_x;
    }

    float cdf_total = cond_cdf[i * cdf_width + res_x - 1].y +
                      cond_cdf[i * cdf_width + res_x - 1].x / res_x;
    float cdf_total_inv = 1.0f / cdf_total;

    /* stuff the total into the brightness value for the last entry, because
     * we are going to normalize the CDFs to 0.0 to 1.0 afterwards */
    cond_cdf[i * cdf_width + res_x].x = cdf_total;

    if (cdf_total > 0.0f)
      for (int j = 1; j < res_x; j++)
        cond_cdf[i * cdf_width + j].y *= cdf_total_inv;

    cond_cdf[i * cdf_width + res_x].y = 1.0f;
  }
}

void LightManager::device_update_background(Device *device,
                                            DeviceScene *dscene,
                                            Scene *scene,
                                            Progress &progress)
{
  KernelIntegrator *kintegrator = &dscene->data.integrator;
  Light *background_light = NULL;

  /* find background light */
  foreach (Light *light, scene->lights) {
    if (light->type == LIGHT_BACKGROUND) {
      background_light = light;
      break;
    }
  }

  /* no background light found, signal renderer to skip sampling */
  if (!background_light || !background_light->is_enabled) {
    kintegrator->pdf_background_res_x = 0;
    kintegrator->pdf_background_res_y = 0;
    return;
  }

  progress.set_status("Updating Lights", "Importance map");

  assert(kintegrator->use_direct_light);

  /* get the resolution from the light's size (we stuff it in there) */
  int2 res = make_int2(background_light->map_resolution, background_light->map_resolution / 2);
  /* If the resolution isn't set manually, try to find an environment texture. */
  if (res.x == 0) {
    Shader *shader = scene->background->get_shader(scene);
    foreach (ShaderNode *node, shader->graph->nodes) {
      if (node->type == EnvironmentTextureNode::node_type) {
        EnvironmentTextureNode *env = (EnvironmentTextureNode *)node;
        ImageMetaData metadata;
        if (!env->handle.empty()) {
          ImageMetaData metadata = env->handle.metadata();
          res.x = max(res.x, metadata.width);
          res.y = max(res.y, metadata.height);
        }
      }
    }
    if (res.x > 0 && res.y > 0) {
      VLOG(2) << "Automatically set World MIS resolution to " << res.x << " by " << res.y << "\n";
    }
  }
  /* If it's still unknown, just use the default. */
  if (res.x == 0 || res.y == 0) {
    res = make_int2(1024, 512);
    VLOG(2) << "Setting World MIS resolution to default\n";
  }
  kintegrator->pdf_background_res_x = res.x;
  kintegrator->pdf_background_res_y = res.y;

  vector<float3> pixels;
  shade_background_pixels(device, dscene, res.x, res.y, pixels, progress);

  if (progress.get_cancel())
    return;

  /* build row distributions and column distribution for the infinite area environment light */
  int cdf_width = res.x + 1;
  float2 *marg_cdf = dscene->light_background_marginal_cdf.alloc(res.y + 1);
  float2 *cond_cdf = dscene->light_background_conditional_cdf.alloc(cdf_width * res.y);

  double time_start = time_dt();
  if (max(res.x, res.y) < 512) {
    /* Small enough resolution, faster to do single-threaded. */
    background_cdf(0, res.y, res.x, res.y, &pixels, cond_cdf);
  }
  else {
    /* Threaded evaluation for large resolution. */
    const int num_blocks = TaskScheduler::num_threads();
    const int chunk_size = res.y / num_blocks;
    int start_row = 0;
    TaskPool pool;
    for (int i = 0; i < num_blocks; ++i) {
      const int current_chunk_size = (i != num_blocks - 1) ? chunk_size : (res.y - i * chunk_size);
      pool.push(function_bind(&background_cdf,
                              start_row,
                              start_row + current_chunk_size,
                              res.x,
                              res.y,
                              &pixels,
                              cond_cdf));
      start_row += current_chunk_size;
    }
    pool.wait_work();
  }

  /* marginal CDFs (column, V direction, sum of rows) */
  marg_cdf[0].x = cond_cdf[res.x].x;
  marg_cdf[0].y = 0.0f;

  for (int i = 1; i < res.y; i++) {
    marg_cdf[i].x = cond_cdf[i * cdf_width + res.x].x;
    marg_cdf[i].y = marg_cdf[i - 1].y + marg_cdf[i - 1].x / res.y;
  }

  float cdf_total = marg_cdf[res.y - 1].y + marg_cdf[res.y - 1].x / res.y;
  marg_cdf[res.y].x = cdf_total;

  if (cdf_total > 0.0f)
    for (int i = 1; i < res.y; i++)
      marg_cdf[i].y /= cdf_total;

  marg_cdf[res.y].y = 1.0f;

  VLOG(2) << "Background MIS build time " << time_dt() - time_start << "\n";

  /* update device */
  dscene->light_background_marginal_cdf.copy_to_device();
  dscene->light_background_conditional_cdf.copy_to_device();
}

void LightManager::device_update_points(Device *, DeviceScene *dscene, Scene *scene)
{
  int num_scene_lights = scene->lights.size();

  int num_lights = 0;
  foreach (Light *light, scene->lights) {
    if (light->is_enabled || light->is_portal) {
      num_lights++;
    }
  }

  KernelLight *klights = dscene->lights.alloc(num_lights);

  if (num_lights == 0) {
    VLOG(1) << "No effective light, ignoring points update.";
    return;
  }

  int light_index = 0;

  foreach (Light *light, scene->lights) {
    if (!light->is_enabled) {
      continue;
    }

    float3 co = light->co;
    Shader *shader = (light->shader) ? light->shader : scene->default_light;
    int shader_id = scene->shader_manager->get_shader_id(shader);
    int max_bounces = light->max_bounces;
    float random = (float)light->random_id * (1.0f / (float)0xFFFFFFFF);

    if (!light->cast_shadow)
      shader_id &= ~SHADER_CAST_SHADOW;

    if (!light->use_diffuse) {
      shader_id |= SHADER_EXCLUDE_DIFFUSE;
      use_light_visibility = true;
    }
    if (!light->use_glossy) {
      shader_id |= SHADER_EXCLUDE_GLOSSY;
      use_light_visibility = true;
    }
    if (!light->use_transmission) {
      shader_id |= SHADER_EXCLUDE_TRANSMIT;
      use_light_visibility = true;
    }
    if (!light->use_scatter) {
      shader_id |= SHADER_EXCLUDE_SCATTER;
      use_light_visibility = true;
    }

    klights[light_index].type = light->type;
    klights[light_index].samples = light->samples;
    klights[light_index].strength[0] = light->strength.x;
    klights[light_index].strength[1] = light->strength.y;
    klights[light_index].strength[2] = light->strength.z;

    if (light->type == LIGHT_POINT) {
      shader_id &= ~SHADER_AREA_LIGHT;

      float radius = light->size;
      float invarea = (radius > 0.0f) ? 1.0f / (M_PI_F * radius * radius) : 1.0f;

      if (light->use_mis && radius > 0.0f)
        shader_id |= SHADER_USE_MIS;

      klights[light_index].co[0] = co.x;
      klights[light_index].co[1] = co.y;
      klights[light_index].co[2] = co.z;

      klights[light_index].spot.radius = radius;
      klights[light_index].spot.invarea = invarea;
    }
    else if (light->type == LIGHT_DISTANT) {
      shader_id &= ~SHADER_AREA_LIGHT;

      float angle = light->angle / 2.0f;
      float radius = tanf(angle);
      float cosangle = cosf(angle);
      float area = M_PI_F * radius * radius;
      float invarea = (area > 0.0f) ? 1.0f / area : 1.0f;
      float3 dir = light->dir;

      dir = safe_normalize(dir);

      if (light->use_mis && area > 0.0f)
        shader_id |= SHADER_USE_MIS;

      klights[light_index].co[0] = dir.x;
      klights[light_index].co[1] = dir.y;
      klights[light_index].co[2] = dir.z;

      klights[light_index].distant.invarea = invarea;
      klights[light_index].distant.radius = radius;
      klights[light_index].distant.cosangle = cosangle;
    }
    else if (light->type == LIGHT_BACKGROUND) {
      uint visibility = scene->background->visibility;

      shader_id &= ~SHADER_AREA_LIGHT;
      shader_id |= SHADER_USE_MIS;

      if (!(visibility & PATH_RAY_DIFFUSE)) {
        shader_id |= SHADER_EXCLUDE_DIFFUSE;
        use_light_visibility = true;
      }
      if (!(visibility & PATH_RAY_GLOSSY)) {
        shader_id |= SHADER_EXCLUDE_GLOSSY;
        use_light_visibility = true;
      }
      if (!(visibility & PATH_RAY_TRANSMIT)) {
        shader_id |= SHADER_EXCLUDE_TRANSMIT;
        use_light_visibility = true;
      }
      if (!(visibility & PATH_RAY_VOLUME_SCATTER)) {
        shader_id |= SHADER_EXCLUDE_SCATTER;
        use_light_visibility = true;
      }
    }
    else if (light->type == LIGHT_AREA) {
      float3 axisu = light->axisu * (light->sizeu * light->size);
      float3 axisv = light->axisv * (light->sizev * light->size);
      float area = len(axisu) * len(axisv);
      if (light->round) {
        area *= -M_PI_4_F;
      }
      float invarea = (area != 0.0f) ? 1.0f / area : 1.0f;
      float3 dir = light->dir;

      dir = safe_normalize(dir);

      if (light->use_mis && area != 0.0f)
        shader_id |= SHADER_USE_MIS;

      klights[light_index].co[0] = co.x;
      klights[light_index].co[1] = co.y;
      klights[light_index].co[2] = co.z;

      klights[light_index].area.axisu[0] = axisu.x;
      klights[light_index].area.axisu[1] = axisu.y;
      klights[light_index].area.axisu[2] = axisu.z;
      klights[light_index].area.axisv[0] = axisv.x;
      klights[light_index].area.axisv[1] = axisv.y;
      klights[light_index].area.axisv[2] = axisv.z;
      klights[light_index].area.invarea = invarea;
      klights[light_index].area.dir[0] = dir.x;
      klights[light_index].area.dir[1] = dir.y;
      klights[light_index].area.dir[2] = dir.z;
    }
    else if (light->type == LIGHT_SPOT) {
      shader_id &= ~SHADER_AREA_LIGHT;

      float radius = light->size;
      float invarea = (radius > 0.0f) ? 1.0f / (M_PI_F * radius * radius) : 1.0f;
      float spot_angle = cosf(light->spot_angle * 0.5f);
      float spot_smooth = (1.0f - spot_angle) * light->spot_smooth;
      float3 dir = light->dir;

      dir = safe_normalize(dir);

      if (light->use_mis && radius > 0.0f)
        shader_id |= SHADER_USE_MIS;

      klights[light_index].co[0] = co.x;
      klights[light_index].co[1] = co.y;
      klights[light_index].co[2] = co.z;

      klights[light_index].spot.radius = radius;
      klights[light_index].spot.invarea = invarea;
      klights[light_index].spot.spot_angle = spot_angle;
      klights[light_index].spot.spot_smooth = spot_smooth;
      klights[light_index].spot.dir[0] = dir.x;
      klights[light_index].spot.dir[1] = dir.y;
      klights[light_index].spot.dir[2] = dir.z;
    }

    klights[light_index].shader_id = shader_id;

    klights[light_index].max_bounces = max_bounces;
    klights[light_index].random = random;

    klights[light_index].tfm = light->tfm;
    klights[light_index].itfm = transform_inverse(light->tfm);

    light_index++;
  }

  /* TODO(sergey): Consider moving portals update to their own function
   * keeping this one more manageable.
   */
  foreach (Light *light, scene->lights) {
    if (!light->is_portal)
      continue;
    assert(light->type == LIGHT_AREA);

    float3 co = light->co;
    float3 axisu = light->axisu * (light->sizeu * light->size);
    float3 axisv = light->axisv * (light->sizev * light->size);
    float area = len(axisu) * len(axisv);
    if (light->round) {
      area *= -M_PI_4_F;
    }
    float invarea = (area != 0.0f) ? 1.0f / area : 1.0f;
    float3 dir = light->dir;

    dir = safe_normalize(dir);

    klights[light_index].co[0] = co.x;
    klights[light_index].co[1] = co.y;
    klights[light_index].co[2] = co.z;

    klights[light_index].area.axisu[0] = axisu.x;
    klights[light_index].area.axisu[1] = axisu.y;
    klights[light_index].area.axisu[2] = axisu.z;
    klights[light_index].area.axisv[0] = axisv.x;
    klights[light_index].area.axisv[1] = axisv.y;
    klights[light_index].area.axisv[2] = axisv.z;
    klights[light_index].area.invarea = invarea;
    klights[light_index].area.dir[0] = dir.x;
    klights[light_index].area.dir[1] = dir.y;
    klights[light_index].area.dir[2] = dir.z;
    klights[light_index].tfm = light->tfm;
    klights[light_index].itfm = transform_inverse(light->tfm);

    light_index++;
  }

  VLOG(1) << "Number of lights sent to the device: " << light_index;

  VLOG(1) << "Number of lights without contribution: " << num_scene_lights - light_index;

  dscene->lights.copy_to_device();
}

void LightManager::device_update(Device *device,
                                 DeviceScene *dscene,
                                 Scene *scene,
                                 Progress &progress)
{
  if (!need_update)
    return;

  VLOG(1) << "Total " << scene->lights.size() << " lights.";

  device_free(device, dscene);

  use_light_visibility = false;

  disable_ineffective_light(scene);

  device_update_points(device, dscene, scene);
  if (progress.get_cancel())
    return;

  device_update_distribution(device, dscene, scene, progress);
  if (progress.get_cancel())
    return;

  device_update_background(device, dscene, scene, progress);
  if (progress.get_cancel())
    return;

  device_update_ies(dscene);
  if (progress.get_cancel())
    return;

  if (use_light_visibility != scene->film->use_light_visibility) {
    scene->film->use_light_visibility = use_light_visibility;
    scene->film->tag_update(scene);
  }

  need_update = false;
}

void LightManager::device_free(Device *, DeviceScene *dscene)
{
  dscene->light_distribution.free();
  dscene->lights.free();
  dscene->light_background_marginal_cdf.free();
  dscene->light_background_conditional_cdf.free();
  dscene->ies_lights.free();
}

void LightManager::tag_update(Scene * /*scene*/)
{
  need_update = true;
}

int LightManager::add_ies_from_file(const string &filename)
{
  string content;

  /* If the file can't be opened, call with an empty line */
  if (filename.empty() || !path_read_text(filename.c_str(), content)) {
    content = "\n";
  }

  return add_ies(content);
}

int LightManager::add_ies(const string &content)
{
  uint hash = hash_string(content.c_str());

  thread_scoped_lock ies_lock(ies_mutex);

  /* Check whether this IES already has a slot. */
  size_t slot;
  for (slot = 0; slot < ies_slots.size(); slot++) {
    if (ies_slots[slot]->hash == hash) {
      ies_slots[slot]->users++;
      return slot;
    }
  }

  /* Try to find an empty slot for the new IES. */
  for (slot = 0; slot < ies_slots.size(); slot++) {
    if (ies_slots[slot]->users == 0 && ies_slots[slot]->hash == 0) {
      break;
    }
  }

  /* If there's no free slot, add one. */
  if (slot == ies_slots.size()) {
    ies_slots.push_back(new IESSlot());
  }

  ies_slots[slot]->ies.load(content);
  ies_slots[slot]->users = 1;
  ies_slots[slot]->hash = hash;

  need_update = true;

  return slot;
}

void LightManager::remove_ies(int slot)
{
  thread_scoped_lock ies_lock(ies_mutex);

  if (slot < 0 || slot >= ies_slots.size()) {
    assert(false);
    return;
  }

  assert(ies_slots[slot]->users > 0);
  ies_slots[slot]->users--;

  /* If the slot has no more users, update the device to remove it. */
  need_update |= (ies_slots[slot]->users == 0);
}

void LightManager::device_update_ies(DeviceScene *dscene)
{
  /* Clear empty slots. */
  foreach (IESSlot *slot, ies_slots) {
    if (slot->users == 0) {
      slot->hash = 0;
      slot->ies.clear();
    }
  }

  /* Shrink the slot table by removing empty slots at the end. */
  int slot_end;
  for (slot_end = ies_slots.size(); slot_end; slot_end--) {
    if (ies_slots[slot_end - 1]->users > 0) {
      /* If the preceding slot has users, we found the new end of the table. */
      break;
    }
    else {
      /* The slot will be past the new end of the table, so free it. */
      delete ies_slots[slot_end - 1];
    }
  }
  ies_slots.resize(slot_end);

  if (ies_slots.size() > 0) {
    int packed_size = 0;
    foreach (IESSlot *slot, ies_slots) {
      packed_size += slot->ies.packed_size();
    }

    /* ies_lights starts with an offset table that contains the offset of every slot,
     * or -1 if the slot is invalid.
     * Following that table, the packed valid IES lights are stored. */
    float *data = dscene->ies_lights.alloc(ies_slots.size() + packed_size);

    int offset = ies_slots.size();
    for (int i = 0; i < ies_slots.size(); i++) {
      int size = ies_slots[i]->ies.packed_size();
      if (size > 0) {
        data[i] = __int_as_float(offset);
        ies_slots[i]->ies.pack(data + offset);
        offset += size;
      }
      else {
        data[i] = __int_as_float(-1);
      }
    }

    dscene->ies_lights.copy_to_device();
  }
}

CCL_NAMESPACE_END
