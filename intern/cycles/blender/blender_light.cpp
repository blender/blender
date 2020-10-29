

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

#include "blender/blender_sync.h"
#include "blender/blender_util.h"

#include "util/util_hash.h"

CCL_NAMESPACE_BEGIN

void BlenderSync::sync_light(BL::Object &b_parent,
                             int persistent_id[OBJECT_PERSISTENT_ID_SIZE],
                             BL::Object &b_ob,
                             BL::Object &b_ob_instance,
                             int random_id,
                             Transform &tfm,
                             bool *use_portal)
{
  /* test if we need to sync */
  Light *light;
  ObjectKey key(b_parent, persistent_id, b_ob_instance, false);
  BL::Light b_light(b_ob.data());

  /* Update if either object or light data changed. */
  if (!light_map.add_or_update(&light, b_ob, b_parent, key)) {
    Shader *shader;
    if (!shader_map.add_or_update(&shader, b_light)) {
      if (light->is_portal)
        *use_portal = true;
      return;
    }
  }

  /* type */
  switch (b_light.type()) {
    case BL::Light::type_POINT: {
      BL::PointLight b_point_light(b_light);
      light->size = b_point_light.shadow_soft_size();
      light->type = LIGHT_POINT;
      break;
    }
    case BL::Light::type_SPOT: {
      BL::SpotLight b_spot_light(b_light);
      light->size = b_spot_light.shadow_soft_size();
      light->type = LIGHT_SPOT;
      light->spot_angle = b_spot_light.spot_size();
      light->spot_smooth = b_spot_light.spot_blend();
      break;
    }
    /* Hemi were removed from 2.8 */
    // case BL::Light::type_HEMI: {
    //  light->type = LIGHT_DISTANT;
    //  light->size = 0.0f;
    //  break;
    // }
    case BL::Light::type_SUN: {
      BL::SunLight b_sun_light(b_light);
      light->angle = b_sun_light.angle();
      light->type = LIGHT_DISTANT;
      break;
    }
    case BL::Light::type_AREA: {
      BL::AreaLight b_area_light(b_light);
      light->size = 1.0f;
      light->axisu = transform_get_column(&tfm, 0);
      light->axisv = transform_get_column(&tfm, 1);
      light->sizeu = b_area_light.size();
      switch (b_area_light.shape()) {
        case BL::AreaLight::shape_SQUARE:
          light->sizev = light->sizeu;
          light->round = false;
          break;
        case BL::AreaLight::shape_RECTANGLE:
          light->sizev = b_area_light.size_y();
          light->round = false;
          break;
        case BL::AreaLight::shape_DISK:
          light->sizev = light->sizeu;
          light->round = true;
          break;
        case BL::AreaLight::shape_ELLIPSE:
          light->sizev = b_area_light.size_y();
          light->round = true;
          break;
      }
      light->type = LIGHT_AREA;
      break;
    }
  }

  /* strength */
  light->strength = get_float3(b_light.color());
  light->strength *= BL::PointLight(b_light).energy();

  /* location and (inverted!) direction */
  light->co = transform_get_column(&tfm, 3);
  light->dir = -transform_get_column(&tfm, 2);
  light->tfm = tfm;

  /* shader */
  vector<Shader *> used_shaders;
  find_shader(b_light, used_shaders, scene->default_light);
  light->shader = used_shaders[0];

  /* shadow */
  PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
  PointerRNA clight = RNA_pointer_get(&b_light.ptr, "cycles");
  light->cast_shadow = get_boolean(clight, "cast_shadow");
  light->use_mis = get_boolean(clight, "use_multiple_importance_sampling");

  int samples = get_int(clight, "samples");
  if (get_boolean(cscene, "use_square_samples"))
    light->samples = samples * samples;
  else
    light->samples = samples;

  light->max_bounces = get_int(clight, "max_bounces");

  if (b_ob != b_ob_instance) {
    light->random_id = random_id;
  }
  else {
    light->random_id = hash_uint2(hash_string(b_ob.name().c_str()), 0);
  }

  if (light->type == LIGHT_AREA)
    light->is_portal = get_boolean(clight, "is_portal");
  else
    light->is_portal = false;

  if (light->is_portal)
    *use_portal = true;

  /* visibility */
  uint visibility = object_ray_visibility(b_ob);
  light->use_diffuse = (visibility & PATH_RAY_DIFFUSE) != 0;
  light->use_glossy = (visibility & PATH_RAY_GLOSSY) != 0;
  light->use_transmission = (visibility & PATH_RAY_TRANSMIT) != 0;
  light->use_scatter = (visibility & PATH_RAY_VOLUME_SCATTER) != 0;

  /* tag */
  light->tag_update(scene);
}

void BlenderSync::sync_background_light(BL::SpaceView3D &b_v3d, bool use_portal)
{
  BL::World b_world = b_scene.world();

  if (b_world) {
    PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
    PointerRNA cworld = RNA_pointer_get(&b_world.ptr, "cycles");

    enum SamplingMethod { SAMPLING_NONE = 0, SAMPLING_AUTOMATIC, SAMPLING_MANUAL, SAMPLING_NUM };
    int sampling_method = get_enum(cworld, "sampling_method", SAMPLING_NUM, SAMPLING_AUTOMATIC);
    bool sample_as_light = (sampling_method != SAMPLING_NONE);

    if (sample_as_light || use_portal) {
      /* test if we need to sync */
      Light *light;
      ObjectKey key(b_world, 0, b_world, false);

      if (light_map.add_or_update(&light, b_world, b_world, key) || world_recalc ||
          b_world.ptr.data != world_map) {
        light->type = LIGHT_BACKGROUND;
        if (sampling_method == SAMPLING_MANUAL) {
          light->map_resolution = get_int(cworld, "sample_map_resolution");
        }
        else {
          light->map_resolution = 0;
        }
        light->shader = scene->default_background;
        light->use_mis = sample_as_light;
        light->max_bounces = get_int(cworld, "max_bounces");

        /* force enable light again when world is resynced */
        light->is_enabled = true;

        int samples = get_int(cworld, "samples");
        if (get_boolean(cscene, "use_square_samples"))
          light->samples = samples * samples;
        else
          light->samples = samples;

        light->tag_update(scene);
        light_map.set_recalc(b_world);
      }
    }
  }

  world_map = b_world.ptr.data;
  world_recalc = false;
  viewport_parameters = BlenderViewportParameters(b_v3d);
}

CCL_NAMESPACE_END
