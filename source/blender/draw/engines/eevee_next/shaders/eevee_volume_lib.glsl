/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * The resources expected to be defined are:
 * - volumes_info_buf
 */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_shadow_lib.glsl)

/* Based on Frosbite Unified Volumetric.
 * https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite */

/* Volume slice to view space depth. */
float volume_z_to_view_z(float z)
{
  float near = volumes_info_buf.depth_near;
  float far = volumes_info_buf.depth_far;
  float distribution = volumes_info_buf.depth_distribution;
  bool is_persp = ProjectionMatrix[3][3] == 0.0;
  /* Implemented in eevee_shader_shared.cc */
  return volume_z_to_view_z(near, far, distribution, is_persp, z);
}

float view_z_to_volume_z(float depth)
{
  float near = volumes_info_buf.depth_near;
  float far = volumes_info_buf.depth_far;
  float distribution = volumes_info_buf.depth_distribution;
  bool is_persp = ProjectionMatrix[3][3] == 0.0;
  /* Implemented in eevee_shader_shared.cc */
  return view_z_to_volume_z(near, far, distribution, is_persp, depth);
}

/* Volume texture normalized coordinates to NDC (special range [0, 1]). */
vec3 volume_to_ndc(vec3 coord)
{
  coord.z = volume_z_to_view_z(coord.z);
  coord.z = get_depth_from_view_z(coord.z);
  coord.xy /= volumes_info_buf.coord_scale;
  return coord;
}

vec3 ndc_to_volume(vec3 coord)
{
  float near = volumes_info_buf.depth_near;
  float far = volumes_info_buf.depth_far;
  float distribution = volumes_info_buf.depth_distribution;
  vec2 coord_scale = volumes_info_buf.coord_scale;
  /* Implemented in eevee_shader_shared.cc */
  return ndc_to_volume(ProjectionMatrix, near, far, distribution, coord_scale, coord);
}

float volume_phase_function_isotropic()
{
  return 1.0 / (4.0 * M_PI);
}

float volume_phase_function(vec3 v, vec3 l, float g)
{
  /* Henyey-Greenstein. */
  float cos_theta = dot(v, l);
  g = clamp(g, -1.0 + 1e-3, 1.0 - 1e-3);
  float sqr_g = g * g;
  return (1 - sqr_g) / max(1e-8, 4.0 * M_PI * pow(1 + sqr_g - 2 * g * cos_theta, 3.0 / 2.0));
}

vec3 volume_light(LightData ld, vec3 L, float l_dist)
{
  float power = 1.0;
  if (ld.type != LIGHT_SUN) {

    float volume_radius_squared = ld.radius_squared;
    float light_clamp = volumes_info_buf.light_clamp;
    if (light_clamp != 0.0) {
      /* 0.0 light clamp means it's disabled. */
      float max_power = max_v3(ld.color) * ld.volume_power;
      if (max_power > 0.0) {
        /* The limit of the power attenuation function when the distance to the light goes to 0 is
         * `2 / r^2` where r is the light radius. We need to find the right radius that emits at
         * most the volume light upper bound. Inverting the function we get: */
        float min_radius_squared = 1.0f / (0.5f * light_clamp / max_power);
        /* Square it here to avoid a multiplication inside the shader. */
        volume_radius_squared = max(volume_radius_squared, min_radius_squared);
      }
    }

    /**
     * Using "Point Light Attenuation Without Singularity" from Cem Yuksel
     * http://www.cemyuksel.com/research/pointlightattenuation/pointlightattenuation.pdf
     * http://www.cemyuksel.com/research/pointlightattenuation/
     */
    float d = l_dist;
    float d_sqr = sqr(d);
    float r_sqr = volume_radius_squared;

    /* Using reformulation that has better numerical precision. */
    power = 2.0 / (d_sqr + r_sqr + d * sqrt(d_sqr + r_sqr));

    if (ld.type == LIGHT_RECT || ld.type == LIGHT_ELLIPSE) {
      /* Modulate by light plane orientation / solid angle. */
      power *= saturate(dot(ld._back, L));
    }
  }
  return ld.color * ld.volume_power * power;
}

#define VOLUMETRIC_SHADOW_MAX_STEP 128.0

vec3 volume_participating_media_extinction(vec3 wpos, sampler3D volume_extinction)
{
  /* Waiting for proper volume shadowmaps and out of frustum shadow map. */
  vec3 ndc = project_point(ProjectionMatrix, transform_point(ViewMatrix, wpos));
  vec3 volume_co = ndc_to_volume(ndc * 0.5 + 0.5);

  /* Let the texture be clamped to edge. This reduce visual glitches. */
  return texture(volume_extinction, volume_co).rgb;
}

vec3 volume_shadow(LightData ld, vec3 ray_wpos, vec3 L, float l_dist)
{
  /* TODO (Miguel Pozo) */
#if 0 && defined(VOLUME_SHADOW)
  vec4 l_vector = vec4(L * l_dist, l_dist);

  /* If light is shadowed, use the shadow vector, if not, reuse the light vector. */
  if (volumes_info_buf.use_soft_shadows && ld.shadowid >= 0.0) {
    ShadowData sd = shadows_data[int(ld.shadowid)];

    if (ld.type == LIGHT_SUN) {
      l_vector.xyz = shadows_cascade_data[int(sd.sh_data_index)].sh_shadow_vec;
      /* No need for length, it is recomputed later. */
    }
    else {
      l_vector.xyz = shadows_cube_data[int(sd.sh_data_index)].position.xyz - ray_wpos;
      l_vector.w = length(l_vector.xyz);
    }
  }

  /* Heterogeneous volume shadows. */
  float dd = l_vector.w / volumes_info_buf.shadow_steps;
  vec3 L = l_vector.xyz / volumes_info_buf.shadow_steps;

  if (ld.type == LIGHT_SUN) {
    /* For sun light we scan the whole frustum. So we need to get the correct endpoints. */
    vec3 ndcP = project_point(ProjectionMatrix, transform_point(ViewMatrix, ray_wpos));
    vec3 ndcL = project_point(ProjectionMatrix,
                              transform_point(ViewMatrix, ray_wpos + l_vector.xyz)) -
                ndcP;

    vec3 frustum_isect = ndcP + ndcL * line_unit_box_intersect_dist_safe(ndcP, ndcL);

    vec4 L_hom = ViewMatrixInverse * (ProjectionMatrixInverse * vec4(frustum_isect, 1.0));
    L = (L_hom.xyz / L_hom.w) - ray_wpos;
    L /= volumes_info_buf.shadow_steps;
    dd = length(L);
  }

#  if 0 /* TODO use shadow maps instead. */
  vec3 shadow = vec3(1.0);
  for (float s = 1.0; s < VOLUMETRIC_SHADOW_MAX_STEP && s <= volumes_info_buf.shadow_steps;
       s += 1.0) {
    vec3 pos = ray_wpos + L * s;
    vec3 s_extinction = volume_participating_media_extinction(pos, volume_extinction);
    shadow *= exp(-s_extinction * dd);
  }
  return shadow;
#  endif
#else
  return vec3(1.0);
#endif /* VOLUME_SHADOW */
}

vec3 volume_irradiance(vec3 wpos)
{
#ifdef IRRADIANCE_HL2
  IrradianceData ir_data = load_irradiance_cell(0, vec3(1.0));
  vec3 irradiance = ir_data.cubesides[0] + ir_data.cubesides[1] + ir_data.cubesides[2];
  ir_data = load_irradiance_cell(0, vec3(-1.0));
  irradiance += ir_data.cubesides[0] + ir_data.cubesides[1] + ir_data.cubesides[2];
  irradiance *= 0.16666666; /* 1/6 */
  return irradiance;
#else
  return vec3(0.0);
#endif
}

struct VolumeResolveSample {
  vec3 transmittance;
  vec3 scattering;
};

VolumeResolveSample volume_resolve(vec3 ndc_P, sampler3D transmittance_tx, sampler3D scattering_tx)
{
  vec3 coord = ndc_to_volume(ndc_P);

  VolumeResolveSample volume;
  volume.scattering = texture(scattering_tx, coord).rgb;
  volume.transmittance = texture(transmittance_tx, coord).rgb;
  return volume;
}
