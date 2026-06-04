/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_intersect_lib.glsl"
#include "draw_shape_lib.glsl"
#include "draw_view.bsl.hh"
#include "eevee_hiz.bsl.hh"
#include "eevee_light_iter.bsl.hh"
#include "eevee_light_lib.bsl.hh"
#include "eevee_light_shared.hh"
#include "gpu_shader_debug_gradients_lib.glsl"
#include "gpu_shader_fullscreen_lib.glsl"
#include "gpu_shader_math_matrix_transform_lib.glsl"

namespace eevee::light::culling {

/**
 * Select the visible items inside the active view and put them inside the sorting buffer.
 */
struct Cull {
  [[legacy_info]] ShaderCreateInfo draw_view_culling;

  [[uniform(0)]] const LightData (&sunlight_buf)[2];

  [[storage(0, read_write)]] LightCullingData &light_cull_buf;
  [[storage(1, read)]] const LightData (&in_light_buf)[];

  [[storage(2, write)]] LightData (&out_light_buf)[];
  [[storage(3, write)]] float (&out_zdist_buf)[];
  [[storage(4, write)]] uint (&out_key_buf)[];
};

[[compute, local_size(CULLING_SELECT_GROUP_SIZE)]]
void cull_main([[resource_table]] Cull &srt,
               [[resource_table]] const draw::View &views,
               [[global_invocation_id]] const uint3 global_id)
{
  uint l_idx = global_id.x;
  if (l_idx >= srt.light_cull_buf.items_count) {
    return;
  }

  LightData light = srt.in_light_buf[l_idx];

  /* Sun lights are packed at the end of the array. Perform early copy. */
  if (is_sun_light(light.type)) {
    /* Some sun-lights are reserved for world light. Perform copy from dedicated buffer. */
    bool is_world_sun_light = light.color.r < 0.0f;
    if (is_world_sun_light) {
      light.color = srt.sunlight_buf[l_idx].color;
      light.object_to_world = srt.sunlight_buf[l_idx].object_to_world;

      LightSunData sun_data = light.sun();
      sun_data.direction = srt.sunlight_buf[l_idx].object_to_world.z_axis();
      light.sun() = sun_data;
      /* NOTE: Use the radius from UI instead of auto sun size for now. */
    }
    /* NOTE: We know the index because sun lights are packed at the start of the input buffer. */
    srt.out_light_buf[srt.light_cull_buf.local_lights_len + l_idx] = light;
    return;
  }

  /* Do not select 0 power lights. */
  if (light.local().local.influence_radius_max < 1e-8f) {
    return;
  }

  Sphere sphere;
  switch (light.type) {
    case LIGHT_SPOT_SPHERE:
    case LIGHT_SPOT_DISK: {
      LightSpotData spot = light.spot();
      /* Only for < ~170 degree Cone due to plane extraction precision. */
      if (spot.spot_tan < 10.0f) {
        float3 x_axis = light.x_axis();
        float3 y_axis = light.y_axis();
        float3 z_axis = light.z_axis();
        Pyramid pyramid = shape_pyramid_non_oblique(
            light.position(),
            light.position() - z_axis * spot.local.influence_radius_max,
            x_axis * spot.local.influence_radius_max * spot.spot_tan / spot.spot_size_inv.x,
            y_axis * spot.local.influence_radius_max * spot.spot_tan / spot.spot_size_inv.y);
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
      sphere.center = light.position();
      sphere.radius = light.local().local.influence_radius_max;
      break;
    default:
      break;
  }

  /* TODO(fclem): HiZ culling? Could be quite beneficial given the nature of the 2.5D culling. */

  /* TODO(fclem): Small light culling / fading? */

  if (intersect_view(sphere)) {
    const ViewMatrices view = views.get(0);
    uint index = atomicAdd(srt.light_cull_buf.visible_count, 1u);

    float z_dist = dot(view.forward(), light.position()) - dot(view.forward(), view.position());
    srt.out_zdist_buf[index] = z_dist;
    srt.out_key_buf[index] = l_idx;
  }
}

/**
 * Sort the lights by their Z distance to the camera.
 * Outputs ordered light buffer.
 * One thread processes one Light entity.
 */
struct Sort {
  [[shared]] float zdists_cache[CULLING_SORT_GROUP_SIZE];

  [[storage(0, read)]] const LightCullingData &light_cull_buf;

  [[storage(1, read)]] const LightData (&in_light_buf)[];
  [[storage(2, read)]] const float (&in_zdist_buf)[];
  [[storage(3, read)]] const uint (&in_key_buf)[];

  [[storage(4, write)]] LightData (&out_light_buf)[];
};

[[compute, local_size(CULLING_SORT_GROUP_SIZE)]]
void sort_main([[resource_table]] Sort &srt,
               [[global_invocation_id]] const uint3 global_id,
               [[local_invocation_id]] const uint3 local_id)
{
  /* Early exit if no lights are present to prevent out of bounds buffer read. */
  if (srt.light_cull_buf.visible_count == 0) {
    return;
  }

  const uint group_size = CULLING_SORT_GROUP_SIZE;
  uint src_index = global_id.x;
  bool valid_thread = true;

  if (src_index >= srt.light_cull_buf.visible_count) {
    /* Do not return because we use barriers later on (which need uniform control flow).
     * Just process the same last item but avoid insertion. */
    src_index = srt.light_cull_buf.visible_count - 1;
    valid_thread = false;
  }

  float local_zdist = srt.in_zdist_buf[src_index];

  int prefix_sum = 0;
  /* Iterate over the whole key buffer. */
  uint iter = divide_ceil(srt.light_cull_buf.visible_count, group_size);
  for (uint i = 0u; i < iter; i++) {
    uint index = group_size * i + local_id.x;
    /* NOTE: This will load duplicated values, but they will be discarded. */
    index = min(index, srt.light_cull_buf.visible_count - 1);
    srt.zdists_cache[local_id.x] = srt.in_zdist_buf[index];

    barrier();

    /* Iterate over the cache line. */
    uint line_end = min(group_size, srt.light_cull_buf.visible_count - group_size * i);
    for (uint j = 0u; j < line_end; j++) {
      if (srt.zdists_cache[j] < local_zdist) {
        prefix_sum++;
      }
      else if (srt.zdists_cache[j] == local_zdist) {
        /* Same depth, use index to order and avoid same prefix for 2 different lights. */
        if ((group_size * i + j) < src_index) {
          prefix_sum++;
        }
      }
    }

    barrier();
  }

  if (valid_thread) {
    /* Copy sorted light to render light buffer. */
    uint input_index = srt.in_key_buf[src_index];
    srt.out_light_buf[prefix_sum] = srt.in_light_buf[input_index];
  }
}

/**
 * Create the Z-bins from Z-sorted lights.
 * Perform min-max operation in LDS memory for speed.
 * For this reason, we only dispatch 1 thread group.
 */

struct ZBinning {
  /* Fits the limit of 32KB. */
  [[shared]] uint zbin_max[CULLING_ZBIN_COUNT];
  [[shared]] uint zbin_min[CULLING_ZBIN_COUNT];

  [[storage(0, read)]] const LightCullingData &light_cull_buf;
  [[storage(1, read)]] const LightData (&light_buf)[];
  [[storage(2, write)]] uint (&out_zbin_buf)[];
};

[[compute, local_size(CULLING_ZBIN_GROUP_SIZE)]]
void zbin_main([[resource_table]] ZBinning &srt,
               [[resource_table]] const draw::View &views,
               [[local_invocation_id]] const uint3 local_id)
{
  constexpr uint zbin_iter = CULLING_ZBIN_COUNT / CULLING_ZBIN_GROUP_SIZE;
  const uint zbin_local = local_id.x * zbin_iter;

  for (uint i = 0u, l = zbin_local; i < zbin_iter; i++, l++) {
    srt.zbin_max[l] = 0x0u;
    srt.zbin_min[l] = ~0x0u;
  }
  barrier();

  const ViewMatrices view = views.get(0);

  uint light_iter = divide_ceil(srt.light_cull_buf.visible_count, uint(CULLING_ZBIN_GROUP_SIZE));
  for (uint i = 0u; i < light_iter; i++) {
    uint index = i * CULLING_ZBIN_GROUP_SIZE + local_id.x;
    if (index >= srt.light_cull_buf.visible_count) {
      continue;
    }
    LightData light = srt.light_buf[index];
    float3 P = light.position();
    /* TODO(fclem): Could have better bounds for spot and area lights. */
    float radius = light.local().local.influence_radius_max;
    float z_dist = dot(view.forward(), P) - dot(view.forward(), view.position());
    int z_min = culling_z_to_zbin(
        srt.light_cull_buf.zbin_scale, srt.light_cull_buf.zbin_bias, z_dist + radius);
    int z_max = culling_z_to_zbin(
        srt.light_cull_buf.zbin_scale, srt.light_cull_buf.zbin_bias, z_dist - radius);
    z_min = clamp(z_min, 0, CULLING_ZBIN_COUNT - 1);
    z_max = clamp(z_max, 0, CULLING_ZBIN_COUNT - 1);
    /* Register to Z bins. */
    for (int z = z_min; z <= z_max; z++) {
      atomicMin(srt.zbin_min[z], index);
      atomicMax(srt.zbin_max[z], index);
    }
  }
  barrier();

  /* Write result to Z-bins buffer. Pack min & max into 1 `uint`. */
  for (uint i = 0u, l = zbin_local; i < zbin_iter; i++, l++) {
    srt.out_zbin_buf[l] = (srt.zbin_max[l] << 16u) | (srt.zbin_min[l] & 0xFFFFu);
  }
}

/**
 * 2D Culling pass for lights.
 * We iterate over all items and check if they intersect with the tile frustum.
 * Dispatch one thread per word.
 */

struct CullingTile {
  IsectFrustum frustum;
  float4 bounds;

  static CullingTile from_corners(bool is_persp, float3 corners[8])
  {
    CullingTile tile;
    tile.bounds = (is_persp) ? tile_bound_cone(corners[0], corners[4], corners[7], corners[3]) :
                               tile_bound_cylinder(corners[0], corners[4], corners[7], corners[3]);

    tile.frustum = isect_frustum_setup(shape_frustum(corners));
    return tile;
  }

  bool intersect(const ViewMatrices view, Sphere sphere)
  {
    bool isect = true;
    /* Test tile intersection using bounding cone or bounding cylinder.
     * This has less false positive cases when the sphere is large. */
    if (view.is_perspective()) {
      isect = ::intersect(shape_cone(this->bounds.xyz, this->bounds.w), sphere);
    }
    else {
      /* Simplify to a 2D circle test on the view Z axis plane. */
      isect = ::intersect(shape_circle(this->bounds.xy, this->bounds.w),
                          shape_circle(sphere.center.xy, sphere.radius));
    }
    /* Refine using frustum test. If the sphere is small it avoids intersection
     * with a neighbor tile. */
    if (isect) {
      isect = ::intersect(this->frustum, sphere);
    }
    return isect;
  }

  bool intersect(Box bbox)
  {
    return ::intersect(this->frustum, bbox);
  }

  bool intersect(Pyramid pyramid)
  {
    return ::intersect(this->frustum, pyramid);
  }

 private:
  /* Corners are expected to be in view-space so that the cone is starting from the origin.
   * Corner order does not matter. */
  static float4 tile_bound_cone(float3 v00, float3 v01, float3 v10, float3 v11)
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
  static float4 tile_bound_cylinder(float3 v00, float3 v01, float3 v10, float3 v11)
  {
    float3 center = (v00 + v01 + v10 + v11) * 0.25f;
    float dist_sqr = distance_squared(center, v00);
    dist_sqr = max(dist_sqr, distance_squared(center, v01));
    dist_sqr = max(dist_sqr, distance_squared(center, v10));
    dist_sqr = max(dist_sqr, distance_squared(center, v11));
    /* Return a cone. Later converted to cylinder. */
    return float4(center, sqrt(dist_sqr));
  }
};

struct Tile {
  [[legacy_info]] ShaderCreateInfo draw_view_culling;

  [[storage(0, read)]] const LightCullingData &light_cull_buf;
  [[storage(1, read)]] const LightData (&light_buf)[];

  [[storage(2, write)]] uint (&out_light_tile_buf)[];

  float2 tile_to_ndc(float2 tile_co, float2 offset)
  {
    /* Add a margin to prevent culling too much if the frustum becomes too much unstable. */
    constexpr float margin = 0.02f;
    tile_co += margin * (offset * 2.0f - 1.0f);

    tile_co += offset;
    return tile_co * light_cull_buf.tile_to_uv_fac * 2.0f - 1.0f;
  }

  CullingTile tile_culling_get(const ViewMatrices view, uint2 tile_co)
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

    for (int i = 0; i < 8; i++) [[unroll]] {
      /* Culling in view space for precision. */
      corners[i] = project_point(view.wininv, corners[i]);
    }

    bool is_persp = view.winmat[3][3] == 0.0f;
    return CullingTile::from_corners(is_persp, corners);
  }
};

[[compute, local_size(CULLING_TILE_GROUP_SIZE)]]
void tile_main([[resource_table]] const draw::View &views,
               [[resource_table]] Tile &srt,
               [[global_invocation_id]] const uint3 global_id)
{
  uint word_idx = global_id.x % srt.light_cull_buf.tile_word_len;
  uint tile_idx = global_id.x / srt.light_cull_buf.tile_word_len;
  uint2 tile_co = uint2(tile_idx % srt.light_cull_buf.tile_x_len,
                        tile_idx / srt.light_cull_buf.tile_x_len);

  if (tile_co.y >= srt.light_cull_buf.tile_y_len) {
    return;
  }

  const ViewMatrices view = views.get(0);

  /* TODO(fclem): We could stop the tile at the HiZ depth. */
  CullingTile tile = srt.tile_culling_get(view, tile_co);

  uint l_idx = word_idx * 32u;
  uint l_end = min(l_idx + 32u, srt.light_cull_buf.visible_count);
  uint word = 0u;
  for (; l_idx < l_end; l_idx++) {
    LightData light = srt.light_buf[l_idx];

    /* Culling in view space for precision and simplicity. */
    float3 vP = view.point_world_to_view(light.position());
    float3 v_right = view.normal_world_to_view(light.x_axis());
    float3 v_up = view.normal_world_to_view(light.y_axis());
    float3 v_back = view.normal_world_to_view(light.z_axis());
    float radius = light.local().local.influence_radius_max;

    if (srt.light_cull_buf.view_is_flipped) {
      v_right = -v_right;
    }

    Sphere sphere = shape_sphere(vP, radius);
    bool intersect_tile = tile.intersect(view, sphere);

    switch (light.type) {
      case LIGHT_SPOT_SPHERE:
      case LIGHT_SPOT_DISK: {
        LightSpotData spot = light.spot();
        /* Only for < ~170 degree Cone due to plane extraction precision. */
        if (spot.spot_tan < 10.0f) {
          Pyramid pyramid = shape_pyramid_non_oblique(
              vP,
              vP - v_back * radius,
              v_right * radius * spot.spot_tan / spot.spot_size_inv.x,
              v_up * radius * spot.spot_tan / spot.spot_size_inv.y);
          intersect_tile = intersect_tile && tile.intersect(pyramid);
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
        intersect_tile = intersect_tile && tile.intersect(bbox);
        break;
      }
      default:
        break;
    }

    if (intersect_tile) {
      word |= 1u << (l_idx % 32u);
    }
  }

  srt.out_light_tile_buf[global_id.x] = word;
}

PipelineCompute cull(cull_main);
PipelineCompute sort(sort_main);
PipelineCompute zbin(zbin_main);
PipelineCompute tile(tile_main);

struct DebugVertOut {
  [[smooth]] float2 screen_uv;
};

struct DebugFragOut {
  [[frag_color(0), index(0)]] float4 out_debug_color_add;
  [[frag_color(0), index(1)]] float4 out_debug_color_mul;
};

[[vertex]]
void debug_vert([[vertex_id]] const int vert_id,
                [[position]] float4 &out_position,
                [[out]] DebugVertOut &v_out)
{
  fullscreen_vertex(vert_id, out_position, v_out.screen_uv);
}

struct NoCullCtx {
  float light_count;
  uint light_bits;

  void eval_directional([[resource_table]] LightRenderData & /*lrd*/,
                        uint /*l_idx*/,
                        LightData /*light*/)
  {
  }

  void eval_local([[resource_table]] LightRenderData & /*lrd*/, uint l_idx, LightData /*light*/)
  {
    light_bits |= 1u << l_idx;
    light_count += 1.0f;
  }
};

struct WithCullCtx {
  uint light_bits;
  float3 P;

  void eval_directional([[resource_table]] LightRenderData & /*lrd*/,
                        uint /*l_idx*/,
                        LightData /*light*/)
  {
  }

  void eval_local([[resource_table]] LightRenderData & /*lrd*/, uint l_idx, LightData light)
  {
    LightVector lv = light_vector_get(light, false, P);
    if (light_attenuation_surface(light, false, lv) > LIGHT_ATTENUATION_THRESHOLD) {
      light_bits |= 1u << l_idx;
    }
  }
};

}  // namespace eevee::light::culling

namespace eevee::light {

template void light::foreach<culling::NoCullCtx, LightRenderData>(const LightRenderData &,
                                                                  culling::NoCullCtx &,
                                                                  LightRenderData &);

template void light::foreach_visible<culling::WithCullCtx, LightRenderData>(
    const LightRenderData &, float2, float, culling::WithCullCtx &, LightRenderData &);

}  // namespace eevee::light

namespace eevee::light::culling {
[[fragment]]
void debug_frag([[resource_table]] const draw::View &views,
                [[resource_table]] LightRenderData &lrd,
                [[resource_table]] const HiZ &hiz,
                [[frag_coord]] const float4 frag_co,
                [[in]] const DebugVertOut &v_out,
                [[out]] DebugFragOut &frag_out)
{
  int2 texel = int2(frag_co.xy);

  const ViewMatrices view = views.get(0);

  float depth = texelFetch(hiz.hiz_tx, texel, 0).r;
  float vP_z = view.depth_screen_to_view(depth);
  float3 P = view.point_screen_to_world(float3(v_out.screen_uv, depth));

  NoCullCtx no_cull = {};
  light::foreach(lrd, no_cull, lrd);

  WithCullCtx with_cull = {.P = P};
  light::foreach_visible(lrd, frag_co.xy, vP_z, with_cull, lrd);

  float4 color = float4(heatmap_gradient(no_cull.light_count / 4.0f), 1.0f);

  if ((with_cull.light_bits & no_cull.light_bits) != no_cull.light_bits) {
    /* ERROR. Some lights were culled incorrectly. */
    color = float4(0.0f, 1.0f, 0.0f, 1.0f);
  }

  frag_out.out_debug_color_add = float4(color.rgb, 0.0f) * 0.2f;
  frag_out.out_debug_color_mul = color;
}

PipelineGraphic debug(debug_vert, debug_frag);

}  // namespace eevee::light::culling
