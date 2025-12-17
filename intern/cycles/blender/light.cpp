/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/light.h"

#include "DNA_light_types.h"

#include "DNA_world_types.h"
#include "blender/sync.h"
#include "blender/util.h"
#include "scene/object.h"

CCL_NAMESPACE_BEGIN

void BlenderSync::sync_light(BObjectInfo &b_ob_info, Light *light)
{
  ::Light &b_light = *static_cast<::Light *>(b_ob_info.object_data.ptr.data);

  light->name = b_light.id.name + 2;

  /* type */
  switch (b_light.type) {
    case LA_LOCAL: {
      light->set_size(b_light.radius);
      light->set_light_type(LIGHT_POINT);
      light->set_is_sphere(!(b_light.mode & LA_USE_SOFT_FALLOFF));
      break;
    }
    case LA_SPOT: {
      light->set_size(b_light.radius);
      light->set_light_type(LIGHT_SPOT);
      light->set_spot_angle(b_light.spotsize);
      light->set_spot_smooth(b_light.spotblend);
      light->set_is_sphere(!(b_light.mode & LA_USE_SOFT_FALLOFF));
      break;
    }
    /* Hemi were removed from 2.8 */
    // case BL::Light::type_HEMI: {
    //  light->type = LIGHT_DISTANT;
    //  light->size = 0.0f;
    //  break;
    // }
    case LA_SUN: {
      light->set_angle(b_light.sun_angle);
      light->set_light_type(LIGHT_DISTANT);
      break;
    }
    case LA_AREA: {
      light->set_size(1.0f);
      light->set_sizeu(b_light.area_size);
      light->set_spread(b_light.area_spread);
      switch (b_light.area_shape) {
        case LA_AREA_SQUARE:
          light->set_sizev(light->get_sizeu());
          light->set_ellipse(false);
          break;
        case LA_AREA_RECT:
          light->set_sizev(b_light.area_sizey);
          light->set_ellipse(false);
          break;
        case LA_AREA_DISK:
          light->set_sizev(light->get_sizeu());
          light->set_ellipse(true);
          break;
        case LA_AREA_ELLIPSE:
          light->set_sizev(b_light.area_sizey);
          light->set_ellipse(true);
          break;
      }
      light->set_light_type(LIGHT_AREA);
      break;
    }
  }

  PointerRNA light_rna_ptr = RNA_id_pointer_create(&b_light.id);

  /* Color and strength. */
  float3 light_color = make_float3(b_light.r, b_light.g, b_light.b);
  if (b_light.mode & LA_USE_TEMPERATURE) {
    float color[3];
    RNA_float_get_array(&light_rna_ptr, "temperature_color", color);
    light_color *= make_float3(color[0], color[1], color[2]);
  }

  const float3 strength = light_color * b_light.energy * exp2f(b_light.exposure);
  light->set_strength(strength);

  /* normalize */
  light->set_normalize(!(b_light.mode & LA_UNNORMALIZED));

  /* shadow */
  PointerRNA clight = RNA_pointer_get(&light_rna_ptr, "cycles");
  light->set_cast_shadow(b_light.mode & LA_SHADOW);
  light->set_use_mis(get_boolean(clight, "use_multiple_importance_sampling"));

  /* caustics light */
  light->set_use_caustics(get_boolean(clight, "is_caustics_light"));

  light->set_max_bounces(get_int(clight, "max_bounces"));

  if (light->get_light_type() == LIGHT_AREA) {
    light->set_is_portal(get_boolean(clight, "is_portal"));
  }
  else {
    light->set_is_portal(false);
  }

  /* tag */
  light->tag_update(scene);
}

void BlenderSync::sync_background_light(::bScreen *b_screen, ::View3D *b_v3d)
{
  ::World *b_world = view_layer.world_override ? view_layer.world_override.ptr.data_as<::World>() :
                                                 b_scene.world().ptr.data_as<::World>();

  if (b_world) {
    PointerRNA world_rna_ptr = RNA_id_pointer_create(&b_world->id);
    PointerRNA cworld = RNA_pointer_get(&world_rna_ptr, "cycles");

    enum SamplingMethod { SAMPLING_NONE = 0, SAMPLING_AUTOMATIC, SAMPLING_MANUAL, SAMPLING_NUM };
    const int sampling_method = get_enum(
        cworld, "sampling_method", SAMPLING_NUM, SAMPLING_AUTOMATIC);
    const bool sample_as_light = (sampling_method != SAMPLING_NONE);

    /* Create object. */
    Object *object;
    const ObjectKey object_key(b_world, nullptr, b_world, false);
    bool update = object_map.add_or_update(&object, &b_world->id, &b_world->id, object_key);
    if (update) {
      /* Lights should be shadow catchers by default. */
      object->set_is_shadow_catcher(true);
      object->set_lightgroup(ustring(b_world ? b_world->lightgroup->name : ""));
    }

    object->set_asset_name(ustring(b_world->id.name + 2));

    /* Create geometry. */
    const GeometryKey geom_key{b_world, Geometry::LIGHT};
    Geometry *geom = geometry_map.find(geom_key);
    if (geom) {
      update |= geometry_map.update(geom, &b_world->id);
    }
    else {
      geom = scene->create_node<Light>();
      geometry_map.add(geom_key, geom);
      object->set_geometry(geom);
      update = true;
    }

    if (update || world_recalc || b_world != world_map) {
      /* Initialize light geometry. */
      Light *light = static_cast<Light *>(geom);

      array<Node *> used_shaders;
      used_shaders.push_back_slow(scene->default_background);
      light->set_used_shaders(used_shaders);

      light->set_light_type(LIGHT_BACKGROUND);

      if (sampling_method == SAMPLING_MANUAL) {
        light->set_map_resolution(get_int(cworld, "sample_map_resolution"));
      }
      else {
        light->set_map_resolution(0);
      }

      light->set_use_mis(sample_as_light);
      light->set_max_bounces(get_int(cworld, "max_bounces"));

      /* Caustic light. */
      light->set_use_caustics(get_boolean(cworld, "is_caustics_light"));

      light->tag_update(scene);

      geometry_map.set_recalc(b_world);
    }
  }

  world_map = b_world;
  world_recalc = false;
  viewport_parameters = BlenderViewportParameters(b_screen, b_v3d, use_developer_ui);
}

CCL_NAMESPACE_END
