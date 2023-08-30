/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(eevee_shadow_tilemap_lib.glsl)

/** \a unormalized_uv is the uv coordinates for the whole tile-map [0..SHADOW_TILEMAP_RES]. */
vec2 shadow_page_uv_transform(
    vec2 atlas_size, uvec3 page, uint lod, vec2 unormalized_uv, ivec2 tile_lod0_coord)
{
  /* Bias uv sample for LODs since custom raster aligns LOD pixels instead of centering them. */
  if (lod != 0) {
    unormalized_uv += 0.5 / float(SHADOW_PAGE_RES * SHADOW_TILEMAP_RES);
  }
  float lod_scaling = exp2(-float(lod));
  vec2 target_tile = vec2(tile_lod0_coord >> lod);
  vec2 page_uv = unormalized_uv * lod_scaling - target_tile;
  /* Assumes atlas is squared. */
  vec2 atlas_uv = (vec2(page.xy) + min(page_uv, 0.99999)) * float(SHADOW_PAGE_RES) / atlas_size;
  return atlas_uv;
}

/* Rotate vector to light's local space . Used for directional shadows. */
vec3 shadow_world_to_local(LightData ld, vec3 L)
{
  /* Avoid relying on compiler to optimize this.
   * vec3 lL = transpose(mat3(ld.object_mat)) * L; */
  vec3 lL;
  lL.x = dot(ld.object_mat[0].xyz, L);
  lL.y = dot(ld.object_mat[1].xyz, L);
  lL.z = dot(ld.object_mat[2].xyz, L);
  return lL;
}

/* TODO(fclem) use utildef version. */
float shadow_orderedIntBitsToFloat(int int_value)
{
  return intBitsToFloat((int_value < 0) ? (int_value ^ 0x7FFFFFFF) : int_value);
}

/* ---------------------------------------------------------------------- */
/** \name Shadow Sampling Functions
 * \{ */

/* Turns local light coordinate into shadow region index. Matches eCubeFace order.
 * \note lL does not need to be normalized. */
int shadow_punctual_face_index_get(vec3 lL)
{
  vec3 aP = abs(lL);
  if (all(greaterThan(aP.xx, aP.yz))) {
    return (lL.x > 0.0) ? 1 : 2;
  }
  else if (all(greaterThan(aP.yy, aP.xz))) {
    return (lL.y > 0.0) ? 3 : 4;
  }
  else {
    return (lL.z > 0.0) ? 5 : 0;
  }
}

mat4x4 shadow_load_normal_matrix(LightData light)
{
  if (!is_sun_light(light.type)) {
    /* FIXME: Why? */
    float scale = 0.5;
    return mat4x4(vec4(scale, 0.0, 0.0, 0.0),
                  vec4(0.0, scale, 0.0, 0.0),
                  vec4(0.0, 0.0, 0.0, -1.0),
                  vec4(0.0, 0.0, light.normal_mat_packed.x, light.normal_mat_packed.y));
  }
  else {
    float near = shadow_orderedIntBitsToFloat(light.clip_near);
    float far = shadow_orderedIntBitsToFloat(light.clip_far);
    /* Could be store precomputed inside the light struct. Just have to find a how to update it. */
    float z_scale = (far - near) * 0.5;
    return mat4x4(vec4(light.normal_mat_packed.x, 0.0, 0.0, 0.0),
                  vec4(0.0, light.normal_mat_packed.x, 0.0, 0.0),
                  vec4(0.0, 0.0, z_scale, 0.0),
                  vec4(0.0, 0.0, 0.0, 1.0));
  }
}

/* Returns minimum bias (in world space unit) needed for a given geometry normal and a shadow-map
 * page to avoid self shadowing artifacts. Note that this can return a negative bias to better
 * match the surface. */
float shadow_slope_bias_get(vec2 atlas_size, LightData light, vec3 lNg, vec3 lP, vec2 uv, uint lod)
{
  /* Compute coordinate inside the pixel we are sampling. */
  vec2 uv_subpixel_coord = fract(uv * atlas_size);
  /* Compute delta to the texel center (where the sample is). */
  vec2 ndc_texel_center_delta = uv_subpixel_coord * 2.0 - 1.0;
  /* Create a normal plane equation and go through the normal projection matrix. */
  vec4 lNg_plane = vec4(lNg, -dot(lNg, lP));
  vec4 ndc_Ng = shadow_load_normal_matrix(light) * lNg_plane;
  /* Get slope from normal vector. Note that this is signed. */
  vec2 ndc_slope = ndc_Ng.xy / abs(ndc_Ng.z);
  /* Clamp out to avoid the bias going to infinity. Remember this is in NDC space. */
  ndc_slope = clamp(ndc_slope, -100.0, 100.0);
  /* Compute slope to where the receiver should be by extending the plane to the texel center. */
  float bias = dot(ndc_slope, ndc_texel_center_delta);
  /* Bias for 1 pixel of the sampled LOD. */
  bias /= float((SHADOW_TILEMAP_RES * SHADOW_PAGE_RES) >> lod);
  return bias;
}

struct ShadowSample {
  /* Signed delta in world units from the shading point to the occluder. Negative if occluded. */
  float occluder_delta;
  /* Tile coordinate inside the tile-map [0..SHADOW_TILEMAP_RES). */
  ivec2 tile_coord;
  /* UV coordinate inside the tile-map [0..SHADOW_TILEMAP_RES). */
  vec2 uv;
  /* Minimum slope bias to apply during comparison. */
  float bias;
  /* Distance from near clip plane in world space units. */
  float occluder_dist;
  /* Tile used loaded for page indirection. */
  ShadowTileData tile;
};

float shadow_tile_depth_get(usampler2DArray atlas_tx, ShadowTileData tile, vec2 atlas_uv)
{
  if (!tile.is_allocated) {
    /* Far plane distance but with a bias to make sure there will be no shadowing.
     * But also not FLT_MAX since it can cause issue with projection. */
    return 1.1;
  }
  uint raw_bits = texture(atlas_tx, vec3(atlas_uv, float(tile.page.z))).r;
  float depth = uintBitsToFloat(raw_bits);
  return depth;
}

vec2 shadow_punctual_linear_depth(vec2 z, float near, float far)
{
  vec2 d = z * 2.0 - 1.0;
  float z_delta = far - near;
  /* Can we simplify? */
  return ((-2.0 * near * far) / z_delta) / (d + (-(far + near) / z_delta));
}

float shadow_directional_linear_depth(float z, float near, float far)
{
  return z * (far - near) + near;
}

ShadowSample shadow_punctual_sample_get(
    usampler2DArray atlas_tx, usampler2D tilemaps_tx, LightData light, vec3 lP, vec3 lNg)
{
  int face_id = shadow_punctual_face_index_get(lP);
  lNg = shadow_punctual_local_position_to_face_local(face_id, lNg);
  lP = shadow_punctual_local_position_to_face_local(face_id, lP);

  ShadowCoordinates coord = shadow_punctual_coordinates(light, lP, face_id);

  vec2 atlas_size = vec2(textureSize(atlas_tx, 0).xy);

  ShadowSample samp;
  samp.tile = shadow_tile_load(tilemaps_tx, coord.tile_coord, coord.tilemap_index);
  samp.uv = shadow_page_uv_transform(
      atlas_size, samp.tile.page, samp.tile.lod, coord.uv, coord.tile_coord);
  samp.bias = shadow_slope_bias_get(atlas_size, light, lNg, lP, samp.uv, samp.tile.lod);

  float occluder_ndc = shadow_tile_depth_get(atlas_tx, samp.tile, samp.uv);
  /* Depth is cleared to FLT_MAX, clamp it to 1 to avoid issues when converting to linear. */
  occluder_ndc = saturate(occluder_ndc);

  /* NOTE: Given to be both positive, so can use intBitsToFloat instead of orderedInt version. */
  float near = intBitsToFloat(light.clip_near);
  float far = intBitsToFloat(light.clip_far);
  /* Shadow is stored as gl_FragCoord.z. Convert to radial distance along with the bias. */
  vec2 occluder = vec2(occluder_ndc, saturate(occluder_ndc + samp.bias));
  vec2 occluder_z = shadow_punctual_linear_depth(occluder, near, far);
  float receiver_dist = length(lP);
  float radius_divisor = receiver_dist / abs(lP.z);
  samp.occluder_dist = occluder_z.x * radius_divisor;
  samp.bias = (occluder_z.y - occluder_z.x) * radius_divisor;
  samp.occluder_delta = samp.occluder_dist - receiver_dist;
  return samp;
}

ShadowSample shadow_directional_sample_get(
    usampler2DArray atlas_tx, usampler2D tilemaps_tx, LightData light, vec3 P, vec3 lNg)
{
  vec3 lP = shadow_world_to_local(light, P);
  ShadowCoordinates coord = shadow_directional_coordinates(light, lP);

  vec2 atlas_size = vec2(textureSize(atlas_tx, 0).xy);

  ShadowSample samp;
  samp.tile = shadow_tile_load(tilemaps_tx, coord.tile_coord, coord.tilemap_index);
  samp.uv = shadow_page_uv_transform(
      atlas_size, samp.tile.page, samp.tile.lod, coord.uv, coord.tile_coord);
  samp.bias = shadow_slope_bias_get(atlas_size, light, lNg, lP, samp.uv, samp.tile.lod);
  samp.bias *= exp2(float(coord.lod_relative));

  float occluder_ndc = shadow_tile_depth_get(atlas_tx, samp.tile, samp.uv);

  float near = shadow_orderedIntBitsToFloat(light.clip_near);
  float far = shadow_orderedIntBitsToFloat(light.clip_far);
  samp.occluder_dist = shadow_directional_linear_depth(occluder_ndc, near, far);
  /* Receiver distance needs to also be increasing.
   * Negate since Z distance follows blender camera convention of -Z as forward. */
  float receiver_dist = -lP.z;
  samp.bias *= far - near;
  samp.occluder_delta = samp.occluder_dist - receiver_dist;
  return samp;
}

ShadowSample shadow_sample(const bool is_directional,
                           usampler2DArray atlas_tx,
                           usampler2D tilemaps_tx,
                           LightData light,
                           vec3 lL,
                           vec3 lNg,
                           vec3 P)
{
  if (is_directional) {
    return shadow_directional_sample_get(atlas_tx, tilemaps_tx, light, P, lNg);
  }
  else {
    return shadow_punctual_sample_get(atlas_tx, tilemaps_tx, light, lL, lNg);
  }
}

/** \} */
