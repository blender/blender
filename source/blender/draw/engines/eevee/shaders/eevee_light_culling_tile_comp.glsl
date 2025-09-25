/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * 2D Culling pass for lights.
 * We iterate over all items and check if they intersect with the tile frustum.
 * Dispatch one thread per word.
 */

#include "infos/eevee_light_culling_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_light_culling_tile)

#include "draw_intersect_lib.glsl"
#include "draw_view_lib.glsl"
#include "gpu_shader_math_matrix_transform_lib.glsl"

/* ---------------------------------------------------------------------- */
/** \name Culling shapes extraction
 * \{ */

struct CullingTile {
  IsectFrustum frustum;
  float4 bounds;
};

/* Corners are expected to be in view-space so that the cone is starting from the origin.
 * Corner order does not matter. */
float4 tile_bound_cone(float3 v00, float3 v01, float3 v10, float3 v11)
{
  v00 = normalize(v00);
  v01 = normalize(v01);
  v10 = normalize(v10);
  v11 = normalize(v11);
  float3 center = normalize(v00 + v01 + v10 + v11);
  float angle_cosine = dot(center, v00);
  angle_cosine = max(angle_cosine, dot(center, v01));
  angle_cosine = max(angle_cosine, dot(center, v10));
  angle_cosine = max(angle_cosine, dot(center, v11));
  return float4(center, angle_cosine);
}

/* Corners are expected to be in view-space. Returns Z-aligned bounding cylinder.
 * Corner order does not matter. */
float4 tile_bound_cylinder(float3 v00, float3 v01, float3 v10, float3 v11)
{
  float3 center = (v00 + v01 + v10 + v11) * 0.25f;
  float dist_sqr = distance_squared(center, v00);
  dist_sqr = max(dist_sqr, distance_squared(center, v01));
  dist_sqr = max(dist_sqr, distance_squared(center, v10));
  dist_sqr = max(dist_sqr, distance_squared(center, v11));
  /* Return a cone. Later converted to cylinder. */
  return float4(center, sqrt(dist_sqr));
}

float2 tile_to_ndc(float2 tile_co, float2 offset)
{
  /* Add a margin to prevent culling too much if the frustum becomes too much unstable. */
  constexpr float margin = 0.02f;
  tile_co += margin * (offset * 2.0f - 1.0f);

  tile_co += offset;
  return tile_co * light_cull_buf.tile_to_uv_fac * 2.0f - 1.0f;
}

CullingTile tile_culling_get(uint2 tile_co)
{
  float2 ftile = float2(tile_co);
  /* Culling frustum corners for this tile. */
  float3 corners[8];
  /* Follow same corners order as view frustum. */
  corners[1].xy = corners[0].xy = tile_to_ndc(ftile, float2(0, 0));
  corners[5].xy = corners[4].xy = tile_to_ndc(ftile, float2(1, 0));
  corners[6].xy = corners[7].xy = tile_to_ndc(ftile, float2(1, 1));
  corners[2].xy = corners[3].xy = tile_to_ndc(ftile, float2(0, 1));
  corners[1].z = corners[5].z = corners[6].z = corners[2].z = -1.0f;
  corners[0].z = corners[4].z = corners[7].z = corners[3].z = 1.0f;

  for (int i = 0; i < 8; i++) {
    /* Culling in view space for precision. */
    corners[i] = project_point(drw_view().wininv, corners[i]);
  }

  bool is_persp = drw_view().winmat[3][3] == 0.0f;
  CullingTile tile;
  tile.bounds = (is_persp) ? tile_bound_cone(corners[0], corners[4], corners[7], corners[3]) :
                             tile_bound_cylinder(corners[0], corners[4], corners[7], corners[3]);

  tile.frustum = isect_frustum_setup(shape_frustum(corners));
  return tile;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Intersection Tests
 * \{ */

bool intersect(CullingTile tile, Sphere sphere)
{
  bool isect = true;
  /* Test tile intersection using bounding cone or bounding cylinder.
   * This has less false positive cases when the sphere is large. */
  if (drw_view().winmat[3][3] == 0.0f) {
    isect = intersect(shape_cone(tile.bounds.xyz, tile.bounds.w), sphere);
  }
  else {
    /* Simplify to a 2D circle test on the view Z axis plane. */
    isect = intersect(shape_circle(tile.bounds.xy, tile.bounds.w),
                      shape_circle(sphere.center.xy, sphere.radius));
  }
  /* Refine using frustum test. If the sphere is small it avoids intersection
   * with a neighbor tile. */
  if (isect) {
    isect = intersect(tile.frustum, sphere);
  }
  return isect;
}

bool intersect(CullingTile tile, Box bbox)
{
  return intersect(tile.frustum, bbox);
}

bool intersect(CullingTile tile, Pyramid pyramid)
{
  return intersect(tile.frustum, pyramid);
}

/** \} */

void main()
{
  uint word_idx = gl_GlobalInvocationID.x % light_cull_buf.tile_word_len;
  uint tile_idx = gl_GlobalInvocationID.x / light_cull_buf.tile_word_len;
  uint2 tile_co = uint2(tile_idx % light_cull_buf.tile_x_len,
                        tile_idx / light_cull_buf.tile_x_len);

  if (tile_co.y >= light_cull_buf.tile_y_len) {
    return;
  }

  /* TODO(fclem): We could stop the tile at the HiZ depth. */
  CullingTile tile = tile_culling_get(tile_co);

  uint l_idx = word_idx * 32u;
  uint l_end = min(l_idx + 32u, light_cull_buf.visible_count);
  uint word = 0u;
  for (; l_idx < l_end; l_idx++) {
    LightData light = light_buf[l_idx];

    /* Culling in view space for precision and simplicity. */
    float3 vP = drw_point_world_to_view(light_position_get(light));
    float3 v_right = drw_normal_world_to_view(light_x_axis(light));
    float3 v_up = drw_normal_world_to_view(light_y_axis(light));
    float3 v_back = drw_normal_world_to_view(light_z_axis(light));
    float radius = light_local_data_get(light).influence_radius_max;

    if (light_cull_buf.view_is_flipped) {
      v_right = -v_right;
    }

    Sphere sphere = shape_sphere(vP, radius);
    bool intersect_tile = intersect(tile, sphere);

    switch (light.type) {
      case LIGHT_SPOT_SPHERE:
      case LIGHT_SPOT_DISK: {
        LightSpotData spot = light_spot_data_get(light);
        /* Only for < ~170 degree Cone due to plane extraction precision. */
        if (spot.spot_tan < 10.0f) {
          Pyramid pyramid = shape_pyramid_non_oblique(
              vP,
              vP - v_back * radius,
              v_right * radius * spot.spot_tan / spot.spot_size_inv.x,
              v_up * radius * spot.spot_tan / spot.spot_size_inv.y);
          intersect_tile = intersect_tile && intersect(tile, pyramid);
          break;
        }
        /* Fall-through to the hemispheric case. */
        ATTR_FALLTHROUGH;
      }
      case LIGHT_RECT:
      case LIGHT_ELLIPSE: {
        float3 v000 = vP - v_right * radius - v_up * radius;
        float3 v100 = v000 + v_right * (radius * 2.0f);
        float3 v010 = v000 + v_up * (radius * 2.0f);
        float3 v001 = v000 - v_back * radius;
        Box bbox = shape_box(v000, v100, v010, v001);
        intersect_tile = intersect_tile && intersect(tile, bbox);
        break;
      }
      default:
        break;
    }

    if (intersect_tile) {
      word |= 1u << (l_idx % 32u);
    }
  }

  out_light_tile_buf[gl_GlobalInvocationID.x] = word;
}
