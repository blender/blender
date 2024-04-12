/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(draw_math_geom_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_ltc_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_iter_lib.glsl)

/* Attenuation cutoff needs to be the same in the shadow loop and the light eval loop. */
#define LIGHT_ATTENUATION_THRESHOLD 1e-6

/* ---------------------------------------------------------------------- */
/** \name Light Functions
 * \{ */

struct LightVector {
  /* World space light vector. From the shading point to the light center. Normalized. */
  vec3 L;
  /* Distance from the shading point to the light center. */
  float dist;
};

LightVector light_vector_get(LightData light, const bool is_directional, vec3 P)
{
  LightVector lv;
  if (is_directional) {
    lv.L = light._back;
    lv.dist = 1.0;
  }
  else {
    lv.L = light._position - P;
    float inv_distance = inversesqrt(length_squared(lv.L));
    lv.L *= inv_distance;
    lv.dist = 1.0 / inv_distance;
  }
  return lv;
}

/* Light vector to the closest point in the light shape. */
LightVector light_shape_vector_get(LightData light, const bool is_directional, vec3 P)
{
  if (!is_directional && is_area_light(light.type)) {
    LightAreaData area = light_area_data_get(light);

    vec3 L = P - light._position;
    vec2 closest_point = vec2(dot(light._right, L), dot(light._up, L));
    closest_point /= area.size;

    if (light.type == LIGHT_ELLIPSE) {
      closest_point /= max(1.0, length(closest_point));
    }
    else {
      closest_point = clamp(closest_point, -1.0, 1.0);
    }
    closest_point *= area.size;

    vec3 L_prime = light._right * closest_point.x + light._up * closest_point.y;

    L = L_prime - L;
    float inv_distance = inversesqrt(length_squared(L));
    LightVector lv;
    lv.L = L * inv_distance;
    lv.dist = 1.0 / inv_distance;
    return lv;
  }
  /* TODO(@fclem): other light shape? */
  return light_vector_get(light, is_directional, P);
}

/* Rotate vector to light's local space. Does not translate. */
vec3 light_world_to_local(LightData light, vec3 L)
{
  /* Avoid relying on compiler to optimize this.
   * vec3 lL = transpose(mat3(light.object_mat)) * L; */
  vec3 lL;
  lL.x = dot(light.object_mat[0].xyz, L);
  lL.y = dot(light.object_mat[1].xyz, L);
  lL.z = dot(light.object_mat[2].xyz, L);
  return lL;
}

/* Transform position from light's local space to world space. Does translation. */
vec3 light_local_position_to_world(LightData light, vec3 lP)
{
  return mat3(light.object_mat) * lP + light._position;
}

/* From Frostbite PBR Course
 * Distance based attenuation
 * http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr.pdf */
float light_influence_attenuation(float dist, float inv_sqr_influence)
{
  float factor = square(dist) * inv_sqr_influence;
  float fac = saturate(1.0 - square(factor));
  return square(fac);
}

float light_spot_attenuation(LightData light, vec3 L)
{
  LightSpotData spot = light_spot_data_get(light);
  vec3 lL = light_world_to_local(light, L);
  float ellipse = inversesqrt(1.0 + length_squared(lL.xy * spot.spot_size_inv / lL.z));
  float spotmask = smoothstep(0.0, 1.0, ellipse * spot.spot_mul + spot.spot_bias);
  return spotmask * step(0.0, -dot(L, -light._back));
}

float light_attenuation_common(LightData light, const bool is_directional, vec3 L)
{
  if (is_directional) {
    return 1.0;
  }
  if (is_spot_light(light.type)) {
    return light_spot_attenuation(light, L);
  }
  if (is_area_light(light.type)) {
    return step(0.0, -dot(L, -light._back));
  }
  return 1.0;
}

/**
 * Fade light influence when surface is not facing the light.
 * This is needed because LTC leaks light at roughness not 0 or 1
 * when the light is below the horizon.
 * L is normalized vector to light shape center.
 * Ng is ideally the geometric normal.
 */
float light_attenuation_facing(LightData light,
                               vec3 L,
                               float distance_to_light,
                               vec3 Ng,
                               const bool is_transmission,
                               bool is_translucent_with_thickness)
{
  if (is_translucent_with_thickness) {
    /* No attenuation in this case since we integrate the whole sphere. */
    return 1.0;
  }

  float radius;
  if (is_sun_light(light.type)) {
    radius = light_sun_data_get(light).radius;
  }
  else if (is_area_light(light.type)) {
    radius = length(light_area_data_get(light).size);
  }
  else {
    radius = light_spot_data_get(light).radius;
  }
  /* Sine of angle between light center and light edge. */
  float sin_solid_angle = radius / distance_to_light;
  /* Sine of angle between light center and shading plane. */
  float sin_light_angle = dot(L, Ng);
  /* Do attenuation after the horizon line to avoid harsh cut
   * or biasing of surfaces without light bleeding. */
  float dist = sin_solid_angle + (is_transmission ? -sin_light_angle : sin_light_angle);
  return saturate((dist + 0.1) * 10.0);
}

float light_attenuation_surface(LightData light,
                                const bool is_directional,
                                const bool is_transmission,
                                bool is_translucency_with_thickness,
                                vec3 Ng,
                                LightVector lv)
{
  float result = light_attenuation_facing(
      light, lv.L, lv.dist, Ng, is_transmission, is_translucency_with_thickness);
  result *= light_attenuation_common(light, is_directional, lv.L);
  if (!is_directional) {
    result *= light_influence_attenuation(
        lv.dist, light_local_data_get(light).influence_radius_invsqr_surface);
  }
  return result;
}

float light_attenuation_volume(LightData light, const bool is_directional, LightVector lv)
{
  float result = light_attenuation_common(light, is_directional, lv.L);
  if (!is_directional) {
    result *= light_influence_attenuation(
        lv.dist, light_local_data_get(light).influence_radius_invsqr_volume);
  }
  return result;
}

/* Cheaper alternative than evaluating the LTC.
 * The result needs to be multiplied by BSDF or Phase Function. */
float light_point_light(LightData light, const bool is_directional, LightVector lv)
{
  if (is_directional) {
    return 1.0;
  }
  /* Using "Point Light Attenuation Without Singularity" from Cem Yuksel
   * http://www.cemyuksel.com/research/pointlightattenuation/pointlightattenuation.pdf
   * http://www.cemyuksel.com/research/pointlightattenuation/
   */
  float d_sqr = square(lv.dist);
  float r_sqr = light_local_data_get(light).radius_squared;
  /* Using reformulation that has better numerical precision. */
  float power = 2.0 / (d_sqr + r_sqr + lv.dist * sqrt(d_sqr + r_sqr));

  if (is_area_light(light.type)) {
    /* Modulate by light plane orientation / solid angle. */
    power *= saturate(dot(light._back, lv.L));
  }
  return power;
}

/**
 * Return the radius of the disk at the sphere origin spanning the same solid angle as the sphere
 * from a given distance.
 * Assumes `distance_to_sphere > sphere_radius`.
 */
float light_sphere_disk_radius(float sphere_radius, float distance_to_sphere)
{
  /* The sine of the half-angle spanned by a sphere light is equal to the tangent of the
   * half-angle spanned by a disk light with the same radius. */
  return sphere_radius * inversesqrt(1.0 - square(sphere_radius / distance_to_sphere));
}

float light_ltc(
    sampler2DArray utility_tx, LightData light, vec3 N, vec3 V, LightVector lv, vec4 ltc_mat)
{
  if (is_sphere_light(light.type) && lv.dist < light_spot_data_get(light).radius) {
    /* Inside the sphere light, integrate over the hemisphere. */
    return 1.0;
  }

  if (light.type == LIGHT_RECT) {
    LightAreaData area = light_area_data_get(light);

    vec3 corners[4];
    corners[0] = light._right * area.size.x + light._up * -area.size.y;
    corners[1] = light._right * area.size.x + light._up * area.size.y;
    corners[2] = -corners[0];
    corners[3] = -corners[1];

    vec3 L = lv.L * lv.dist;
    corners[0] += L;
    corners[1] += L;
    corners[2] += L;
    corners[3] += L;

    ltc_transform_quad(N, V, ltc_matrix(ltc_mat), corners);

    return ltc_evaluate_quad(utility_tx, corners, vec3(0.0, 0.0, 1.0));
  }
  else {
    vec3 Px = light._right;
    vec3 Py = light._up;

    if (!is_area_light(light.type)) {
      make_orthonormal_basis(lv.L, Px, Py);
    }

    vec2 size;
    if (is_sphere_light(light.type)) {
      /* Spherical omni or spot light. */
      size = vec2(light_sphere_disk_radius(light_spot_data_get(light).radius, lv.dist));
    }
    else if (is_oriented_disk_light(light.type)) {
      /* View direction-aligned disk. */
      size = vec2(light_spot_data_get(light).radius);
    }
    else if (is_sun_light(light.type)) {
      size = vec2(light_sun_data_get(light).radius);
    }
    else {
      /* Area light. */
      size = vec2(light_area_data_get(light).size);
    }

    vec3 points[3];
    points[0] = Px * -size.x + Py * -size.y;
    points[1] = Px * size.x + Py * -size.y;
    points[2] = -points[0];

    vec3 L = lv.L * lv.dist;
    points[0] += L;
    points[1] += L;
    points[2] += L;

    return ltc_evaluate_disk(utility_tx, N, V, ltc_matrix(ltc_mat), points);
  }
}

/** \} */
