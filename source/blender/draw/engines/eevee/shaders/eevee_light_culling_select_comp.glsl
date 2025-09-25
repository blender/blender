/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Select the visible items inside the active view and put them inside the sorting buffer.
 */

#include "infos/eevee_light_culling_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_light_culling_select)

#include "draw_intersect_lib.glsl"
#include "draw_view_lib.glsl"

void main()
{
  uint l_idx = gl_GlobalInvocationID.x;
  if (l_idx >= light_cull_buf.items_count) {
    return;
  }

  LightData light = in_light_buf[l_idx];

  /* Sun lights are packed at the end of the array. Perform early copy. */
  if (is_sun_light(light.type)) {
    /* First sun-light is reserved for world light. Perform copy from dedicated buffer. */
    bool is_world_sun_light = light.color.r < 0.0f;
    if (is_world_sun_light) {
      light.color = sunlight_buf.color;
      light.object_to_world = sunlight_buf.object_to_world;

      LightSunData sun_data = light_sun_data_get(light);
      sun_data.direction = transform_z_axis(sunlight_buf.object_to_world);
      light = light_sun_data_set(light, sun_data);
      /* NOTE: Use the radius from UI instead of auto sun size for now. */
    }
    /* NOTE: We know the index because sun lights are packed at the start of the input buffer. */
    out_light_buf[light_cull_buf.local_lights_len + l_idx] = light;
    return;
  }

  /* Do not select 0 power lights. */
  if (light_local_data_get(light).influence_radius_max < 1e-8f) {
    return;
  }

  Sphere sphere;
  switch (light.type) {
    case LIGHT_SPOT_SPHERE:
    case LIGHT_SPOT_DISK: {
      LightSpotData spot = light_spot_data_get(light);
      /* Only for < ~170 degree Cone due to plane extraction precision. */
      if (spot.spot_tan < 10.0f) {
        float3 x_axis = light_x_axis(light);
        float3 y_axis = light_y_axis(light);
        float3 z_axis = light_z_axis(light);
        Pyramid pyramid = shape_pyramid_non_oblique(
            light_position_get(light),
            light_position_get(light) - z_axis * spot.influence_radius_max,
            x_axis * spot.influence_radius_max * spot.spot_tan / spot.spot_size_inv.x,
            y_axis * spot.influence_radius_max * spot.spot_tan / spot.spot_size_inv.y);
        if (!intersect_view(pyramid)) {
          return;
        }
      }
      ATTR_FALLTHROUGH;
    }
    case LIGHT_RECT:
    case LIGHT_ELLIPSE:
    case LIGHT_OMNI_SPHERE:
    case LIGHT_OMNI_DISK:
      sphere.center = light_position_get(light);
      sphere.radius = light_local_data_get(light).influence_radius_max;
      break;
    default:
      break;
  }

  /* TODO(fclem): HiZ culling? Could be quite beneficial given the nature of the 2.5D culling. */

  /* TODO(fclem): Small light culling / fading? */

  if (intersect_view(sphere)) {
    uint index = atomicAdd(light_cull_buf.visible_count, 1u);

    float z_dist = dot(drw_view_forward(), light_position_get(light)) -
                   dot(drw_view_forward(), drw_view_position());
    out_zdist_buf[index] = z_dist;
    out_key_buf[index] = l_idx;
  }
}
