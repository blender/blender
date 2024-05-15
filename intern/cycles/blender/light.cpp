/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/light.h"

#include "blender/light_linking.h"
#include "blender/sync.h"
#include "blender/util.h"

#include "util/hash.h"

CCL_NAMESPACE_BEGIN

void BlenderSync::sync_light(BL::Object &b_parent,
                             int persistent_id[OBJECT_PERSISTENT_ID_SIZE],
                             BObjectInfo &b_ob_info,
                             int random_id,
                             Transform &tfm,
                             bool *use_portal)
{
  /* test if we need to sync */
  ObjectKey key(b_parent, persistent_id, b_ob_info.real_object, false);
  BL::Light b_light(b_ob_info.object_data);

  Light *light = light_map.find(key);

  /* Check if the transform was modified, in case a linked collection is moved we do not get a
   * specific depsgraph update (#88515). This also mimics the behavior for Objects. */
  const bool tfm_updated = (light && light->get_tfm() != tfm);

  /* Update if either object or light data changed. */
  if (!light_map.add_or_update(&light, b_ob_info.real_object, b_parent, key) && !tfm_updated) {
    Shader *shader;
    if (!shader_map.add_or_update(&shader, b_light)) {
      if (light->get_is_portal()) {
        *use_portal = true;
      }
      return;
    }
  }

  light->name = b_light.name().c_str();

  /* type */
  switch (b_light.type()) {
    case BL::Light::type_POINT: {
      BL::PointLight b_point_light(b_light);
      light->set_size(b_point_light.shadow_soft_size());
      light->set_light_type(LIGHT_POINT);
      light->set_is_sphere(!b_point_light.use_soft_falloff());
      break;
    }
    case BL::Light::type_SPOT: {
      BL::SpotLight b_spot_light(b_light);
      light->set_size(b_spot_light.shadow_soft_size());
      light->set_light_type(LIGHT_SPOT);
      light->set_spot_angle(b_spot_light.spot_size());
      light->set_spot_smooth(b_spot_light.spot_blend());
      light->set_is_sphere(!b_spot_light.use_soft_falloff());
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
      light->set_angle(b_sun_light.angle());
      light->set_light_type(LIGHT_DISTANT);
      break;
    }
    case BL::Light::type_AREA: {
      BL::AreaLight b_area_light(b_light);
      light->set_size(1.0f);
      light->set_sizeu(b_area_light.size());
      light->set_spread(b_area_light.spread());
      switch (b_area_light.shape()) {
        case BL::AreaLight::shape_SQUARE:
          light->set_sizev(light->get_sizeu());
          light->set_ellipse(false);
          break;
        case BL::AreaLight::shape_RECTANGLE:
          light->set_sizev(b_area_light.size_y());
          light->set_ellipse(false);
          break;
        case BL::AreaLight::shape_DISK:
          light->set_sizev(light->get_sizeu());
          light->set_ellipse(true);
          break;
        case BL::AreaLight::shape_ELLIPSE:
          light->set_sizev(b_area_light.size_y());
          light->set_ellipse(true);
          break;
      }
      light->set_light_type(LIGHT_AREA);
      break;
    }
  }

  /* strength */
  float3 strength = get_float3(b_light.color()) * BL::PointLight(b_light).energy();
  light->set_strength(strength);

  /* location and (inverted!) direction */
  light->set_tfm(tfm);

  /* shader */
  array<Node *> used_shaders;
  find_shader(b_light, used_shaders, scene->default_light);
  light->set_shader(static_cast<Shader *>(used_shaders[0]));

  /* shadow */
  PointerRNA clight = RNA_pointer_get(&b_light.ptr, "cycles");
  light->set_cast_shadow(b_light.use_shadow());
  light->set_use_mis(get_boolean(clight, "use_multiple_importance_sampling"));

  /* caustics light */
  light->set_use_caustics(get_boolean(clight, "is_caustics_light"));

  light->set_max_bounces(get_int(clight, "max_bounces"));

  if (b_ob_info.real_object != b_ob_info.iter_object) {
    light->set_random_id(random_id);
  }
  else {
    light->set_random_id(hash_uint2(hash_string(b_ob_info.real_object.name().c_str()), 0));
  }

  if (light->get_light_type() == LIGHT_AREA) {
    light->set_is_portal(get_boolean(clight, "is_portal"));
  }
  else {
    light->set_is_portal(false);
  }

  if (light->get_is_portal()) {
    *use_portal = true;
  }

  /* visibility */
  uint visibility = object_ray_visibility(b_ob_info.real_object);
  light->set_use_camera((visibility & PATH_RAY_CAMERA) != 0);
  light->set_use_diffuse((visibility & PATH_RAY_DIFFUSE) != 0);
  light->set_use_glossy((visibility & PATH_RAY_GLOSSY) != 0);
  light->set_use_transmission((visibility & PATH_RAY_TRANSMIT) != 0);
  light->set_use_scatter((visibility & PATH_RAY_VOLUME_SCATTER) != 0);
  light->set_is_shadow_catcher(b_ob_info.real_object.is_shadow_catcher());

  /* Light group and linking. */
  string lightgroup = b_ob_info.real_object.lightgroup();
  if (lightgroup.empty()) {
    lightgroup = b_parent.lightgroup();
  }
  light->set_lightgroup(ustring(lightgroup));
  light->set_light_set_membership(
      BlenderLightLink::get_light_set_membership(PointerRNA_NULL, b_ob_info.real_object));
  light->set_shadow_set_membership(
      BlenderLightLink::get_shadow_set_membership(PointerRNA_NULL, b_ob_info.real_object));

  /* tag */
  light->tag_update(scene);
}

void BlenderSync::sync_background_light(BL::SpaceView3D &b_v3d, bool use_portal)
{
  BL::World b_world = view_layer.world_override ? view_layer.world_override : b_scene.world();

  if (b_world) {
    PointerRNA cworld = RNA_pointer_get(&b_world.ptr, "cycles");

    enum SamplingMethod { SAMPLING_NONE = 0, SAMPLING_AUTOMATIC, SAMPLING_MANUAL, SAMPLING_NUM };
    int sampling_method = get_enum(cworld, "sampling_method", SAMPLING_NUM, SAMPLING_AUTOMATIC);
    bool sample_as_light = (sampling_method != SAMPLING_NONE);

    if (sample_as_light || use_portal) {
      /* test if we need to sync */
      Light *light;
      ObjectKey key(b_world, 0, b_world, false);

      if (light_map.add_or_update(&light, b_world, b_world, key) || world_recalc ||
          b_world.ptr.data != world_map)
      {
        light->set_light_type(LIGHT_BACKGROUND);
        if (sampling_method == SAMPLING_MANUAL) {
          light->set_map_resolution(get_int(cworld, "sample_map_resolution"));
        }
        else {
          light->set_map_resolution(0);
        }
        light->set_shader(scene->default_background);
        light->set_use_mis(sample_as_light);
        light->set_max_bounces(get_int(cworld, "max_bounces"));

        /* force enable light again when world is resynced */
        light->set_is_enabled(true);

        /* caustic light */
        light->set_use_caustics(get_boolean(cworld, "is_caustics_light"));

        light->tag_update(scene);
        light_map.set_recalc(b_world);
      }
    }
  }

  world_map = b_world.ptr.data;
  world_recalc = false;
  viewport_parameters = BlenderViewportParameters(b_v3d, use_developer_ui);
}

CCL_NAMESPACE_END
