
#pragma BLENDER_REQUIRE(common_math_geom_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_ltc_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_iter_lib.glsl)

/* ---------------------------------------------------------------------- */
/** \name Light Functions
 * \{ */

void light_vector_get(LightData ld, vec3 P, out vec3 L, out float dist)
{
  /* TODO(fclem): Static branching. */
  if (is_sun_light(ld.type)) {
    L = ld._back;
    dist = 1.0;
  }
  else {
    L = ld._position - P;
    dist = inversesqrt(len_squared(L));
    L *= dist;
    dist = 1.0 / dist;
  }
}

/* Rotate vector to light's local space. Does not translate. */
vec3 light_world_to_local(LightData ld, vec3 L)
{
  /* Avoid relying on compiler to optimize this.
   * vec3 lL = transpose(mat3(ld.object_mat)) * L; */
  vec3 lL;
  lL.x = dot(ld.object_mat[0].xyz, L);
  lL.y = dot(ld.object_mat[1].xyz, L);
  lL.z = dot(ld.object_mat[2].xyz, L);
  return lL;
}

/* From Frostbite PBR Course
 * Distance based attenuation
 * http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr.pdf */
float light_influence_attenuation(float dist, float inv_sqr_influence)
{
  float factor = sqr(dist) * inv_sqr_influence;
  float fac = saturate(1.0 - sqr(factor));
  return sqr(fac);
}

float light_spot_attenuation(LightData ld, vec3 L)
{
  vec3 lL = light_world_to_local(ld, L);
  float ellipse = inversesqrt(1.0 + len_squared(lL.xy * ld.spot_size_inv / lL.z));
  float spotmask = smoothstep(0.0, 1.0, ellipse * ld._spot_mul + ld._spot_bias);
  return spotmask;
}

float light_attenuation(LightData ld, vec3 L, float dist)
{
  float vis = 1.0;
  if (ld.type == LIGHT_SPOT) {
    vis *= light_spot_attenuation(ld, L);
  }

  if (ld.type >= LIGHT_SPOT) {
    vis *= step(0.0, -dot(L, -ld._back));
  }

  /* TODO(fclem): Static branching. */
  if (!is_sun_light(ld.type)) {
#ifdef VOLUME_LIGHTING
    vis *= light_influence_attenuation(dist, ld.influence_radius_invsqr_volume);
#else
    vis *= light_influence_attenuation(dist, ld.influence_radius_invsqr_surface);
#endif
  }
  return vis;
}

/* Cheaper alternative than evaluating the LTC.
 * The result needs to be multiplied by BSDF or Phase Function. */
float light_point_light(LightData ld, const bool is_directional, vec3 L, float dist)
{
  if (is_directional) {
    return 1.0;
  }
  /**
   * Using "Point Light Attenuation Without Singularity" from Cem Yuksel
   * http://www.cemyuksel.com/research/pointlightattenuation/pointlightattenuation.pdf
   * http://www.cemyuksel.com/research/pointlightattenuation/
   **/
  float d_sqr = sqr(dist);
  float r_sqr = ld.radius_squared;
  /* Using reformulation that has better numerical precision. */
  float power = 2.0 / (d_sqr + r_sqr + dist * sqrt(d_sqr + r_sqr));

  if (is_area_light(ld.type)) {
    /* Modulate by light plane orientation / solid angle. */
    power *= saturate(dot(ld._back, L));
  }
  return power;
}

float light_diffuse(sampler2DArray utility_tx,
                    const bool is_directional,
                    LightData ld,
                    vec3 N,
                    vec3 V,
                    vec3 L,
                    float dist)
{
  if (is_directional || !is_area_light(ld.type)) {
    float radius = ld._radius / dist;
    return ltc_evaluate_disk_simple(utility_tx, radius, dot(N, L));
  }
  else if (ld.type == LIGHT_RECT) {
    vec3 corners[4];
    corners[0] = ld._right * ld._area_size_x + ld._up * -ld._area_size_y;
    corners[1] = ld._right * ld._area_size_x + ld._up * ld._area_size_y;
    corners[2] = -corners[0];
    corners[3] = -corners[1];

    corners[0] = normalize(L * dist + corners[0]);
    corners[1] = normalize(L * dist + corners[1]);
    corners[2] = normalize(L * dist + corners[2]);
    corners[3] = normalize(L * dist + corners[3]);

    return ltc_evaluate_quad(utility_tx, corners, N);
  }
  else /* (ld.type == LIGHT_ELLIPSE) */ {
    vec3 points[3];
    points[0] = ld._right * -ld._area_size_x + ld._up * -ld._area_size_y;
    points[1] = ld._right * ld._area_size_x + ld._up * -ld._area_size_y;
    points[2] = -points[0];

    points[0] += L * dist;
    points[1] += L * dist;
    points[2] += L * dist;

    return ltc_evaluate_disk(utility_tx, N, V, mat3(1.0), points);
  }
}

float light_ltc(sampler2DArray utility_tx,
                const bool is_directional,
                LightData ld,
                vec3 N,
                vec3 V,
                vec3 L,
                float dist,
                vec4 ltc_mat)
{
  if (is_directional || ld.type != LIGHT_RECT) {
    vec3 Px = ld._right;
    vec3 Py = ld._up;

    if (is_directional || !is_area_light(ld.type)) {
      make_orthonormal_basis(L, Px, Py);
    }

    vec3 points[3];
    points[0] = Px * -ld._area_size_x + Py * -ld._area_size_y;
    points[1] = Px * ld._area_size_x + Py * -ld._area_size_y;
    points[2] = -points[0];

    points[0] += L * dist;
    points[1] += L * dist;
    points[2] += L * dist;

    return ltc_evaluate_disk(utility_tx, N, V, ltc_matrix(ltc_mat), points);
  }
  else {
    vec3 corners[4];
    corners[0] = ld._right * ld._area_size_x + ld._up * -ld._area_size_y;
    corners[1] = ld._right * ld._area_size_x + ld._up * ld._area_size_y;
    corners[2] = -corners[0];
    corners[3] = -corners[1];

    corners[0] += L * dist;
    corners[1] += L * dist;
    corners[2] += L * dist;
    corners[3] += L * dist;

    ltc_transform_quad(N, V, ltc_matrix(ltc_mat), corners);

    return ltc_evaluate_quad(utility_tx, corners, vec3(0.0, 0.0, 1.0));
  }
}

vec3 light_translucent(sampler1D transmittance_tx,
                       const bool is_directional,
                       LightData ld,
                       vec3 N,
                       vec3 L,
                       float dist,
                       vec3 sss_radius,
                       float delta)
{
  /* TODO(fclem): We should compute the power at the entry point. */
  /* NOTE(fclem): we compute the light attenuation using the light vector but the transmittance
   * using the shadow depth delta. */
  float power = light_point_light(ld, is_directional, L, dist);
  /* Do not add more energy on front faces. Also apply lambertian BSDF. */
  power *= max(0.0, dot(-N, L)) * M_1_PI;

  sss_radius *= SSS_TRANSMIT_LUT_RADIUS;
  vec3 channels_co = saturate(delta / sss_radius) * SSS_TRANSMIT_LUT_SCALE + SSS_TRANSMIT_LUT_BIAS;

  vec3 translucency;
  translucency.x = (sss_radius.x > 0.0) ? texture(transmittance_tx, channels_co.x).r : 0.0;
  translucency.y = (sss_radius.y > 0.0) ? texture(transmittance_tx, channels_co.y).r : 0.0;
  translucency.z = (sss_radius.z > 0.0) ? texture(transmittance_tx, channels_co.z).r : 0.0;
  return translucency * power;
}

/** \} */
