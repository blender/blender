
#pragma BLENDER_REQUIRE(lights_lib.glsl)
#pragma BLENDER_REQUIRE(lightprobe_lib.glsl)
#pragma BLENDER_REQUIRE(irradiance_lib.glsl)

/* Based on Frosbite Unified Volumetric.
 * https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite */

/* Volume slice to view space depth. */
float volume_z_to_view_z(float z)
{
  if (ProjectionMatrix[3][3] == 0.0) {
    /* Exponential distribution */
    return (exp2(z / volDepthParameters.z) - volDepthParameters.x) / volDepthParameters.y;
  }
  else {
    /* Linear distribution */
    return mix(volDepthParameters.x, volDepthParameters.y, z);
  }
}

float view_z_to_volume_z(float depth)
{
  if (ProjectionMatrix[3][3] == 0.0) {
    /* Exponential distribution */
    return volDepthParameters.z * log2(depth * volDepthParameters.y + volDepthParameters.x);
  }
  else {
    /* Linear distribution */
    return (depth - volDepthParameters.x) * volDepthParameters.z;
  }
}

/* Volume texture normalized coordinates to NDC (special range [0, 1]). */
vec3 volume_to_ndc(vec3 cos)
{
  cos.z = volume_z_to_view_z(cos.z);
  cos.z = get_depth_from_view_z(cos.z);
  cos.xy /= volCoordScale.xy;
  return cos;
}

vec3 ndc_to_volume(vec3 cos)
{
  cos.z = get_view_z_from_depth(cos.z);
  cos.z = view_z_to_volume_z(cos.z);
  cos.xy *= volCoordScale.xy;
  return cos;
}

float phase_function_isotropic()
{
  return 1.0 / (4.0 * M_PI);
}

float phase_function(vec3 v, vec3 l, float g)
{
  /* Henyey-Greenstein */
  float cos_theta = dot(v, l);
  g = clamp(g, -1.0 + 1e-3, 1.0 - 1e-3);
  float sqr_g = g * g;
  return (1 - sqr_g) / max(1e-8, 4.0 * M_PI * pow(1 + sqr_g - 2 * g * cos_theta, 3.0 / 2.0));
}

vec3 light_volume(LightData ld, vec4 l_vector)
{
  float power = 1.0;
  if (ld.l_type != SUN) {
    /**
     * Using "Point Light Attenuation Without Singularity" from Cem Yuksel
     * http://www.cemyuksel.com/research/pointlightattenuation/pointlightattenuation.pdf
     * http://www.cemyuksel.com/research/pointlightattenuation/
     **/
    float d = l_vector.w;
    float d_sqr = sqr(d);
    float r_sqr = ld.l_volume_radius;
    /* Using reformulation that has better numerical percision. */
    power = 2.0 / (d_sqr + r_sqr + d * sqrt(d_sqr + r_sqr));

    if (ld.l_type == AREA_RECT || ld.l_type == AREA_ELLIPSE) {
      /* Modulate by light plane orientation / solid angle. */
      power *= saturate(dot(-ld.l_forward, l_vector.xyz / l_vector.w));
    }
  }
  return ld.l_color * ld.l_volume * power;
}

vec3 light_volume_light_vector(LightData ld, vec3 P)
{
  if (ld.l_type == SUN) {
    return -ld.l_forward;
  }
  else if (ld.l_type == AREA_RECT || ld.l_type == AREA_ELLIPSE) {
    vec3 L = P - ld.l_position;
    vec2 closest_point = vec2(dot(ld.l_right, L), dot(ld.l_up, L));
    vec2 max_pos = vec2(ld.l_sizex, ld.l_sizey);
    closest_point /= max_pos;

    if (ld.l_type == AREA_ELLIPSE) {
      closest_point /= max(1.0, length(closest_point));
    }
    else {
      closest_point = clamp(closest_point, -1.0, 1.0);
    }
    closest_point *= max_pos;

    vec3 L_prime = ld.l_right * closest_point.x + ld.l_up * closest_point.y;
    return L_prime - L;
  }
  else {
    return ld.l_position - P;
  }
}

#define VOLUMETRIC_SHADOW_MAX_STEP 128.0

vec3 participating_media_extinction(vec3 wpos, sampler3D volume_extinction)
{
  /* Waiting for proper volume shadowmaps and out of frustum shadow map. */
  vec3 ndc = project_point(ViewProjectionMatrix, wpos);
  vec3 volume_co = ndc_to_volume(ndc * 0.5 + 0.5);

  /* Let the texture be clamped to edge. This reduce visual glitches. */
  return texture(volume_extinction, volume_co).rgb;
}

vec3 light_volume_shadow(LightData ld, vec3 ray_wpos, vec4 l_vector, sampler3D volume_extinction)
{
#if defined(VOLUME_SHADOW)
  /* If light is shadowed, use the shadow vector, if not, reuse the light vector. */
  if (volUseSoftShadows && ld.l_shadowid >= 0.0) {
    ShadowData sd = shadows_data[int(ld.l_shadowid)];

    if (ld.l_type == SUN) {
      l_vector.xyz = shadows_cascade_data[int(sd.sh_data_index)].sh_shadow_vec;
      /* No need for length, it is recomputed later. */
    }
    else {
      l_vector.xyz = shadows_cube_data[int(sd.sh_data_index)].position.xyz - ray_wpos;
      l_vector.w = length(l_vector.xyz);
    }
  }

  /* Heterogeneous volume shadows */
  float dd = l_vector.w / volShadowSteps;
  vec3 L = l_vector.xyz / volShadowSteps;

  if (ld.l_type == SUN) {
    /* For sun light we scan the whole frustum. So we need to get the correct endpoints. */
    vec3 ndcP = project_point(ViewProjectionMatrix, ray_wpos);
    vec3 ndcL = project_point(ViewProjectionMatrix, ray_wpos + l_vector.xyz) - ndcP;

    vec3 frustum_isect = ndcP + ndcL * line_unit_box_intersect_dist_safe(ndcP, ndcL);

    L = project_point(ViewProjectionMatrixInverse, frustum_isect) - ray_wpos;
    L /= volShadowSteps;
    dd = length(L);
  }

  vec3 shadow = vec3(1.0);
  for (float s = 1.0; s < VOLUMETRIC_SHADOW_MAX_STEP && s <= volShadowSteps; s += 1.0) {
    vec3 pos = ray_wpos + L * s;
    vec3 s_extinction = participating_media_extinction(pos, volume_extinction);
    shadow *= exp(-s_extinction * dd);
  }
  return shadow;
#else
  return vec3(1.0);
#endif /* VOLUME_SHADOW */
}

vec3 irradiance_volumetric(vec3 wpos)
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

uniform sampler3D inScattering;
uniform sampler3D inTransmittance;

void volumetric_resolve(vec2 frag_uvs,
                        float frag_depth,
                        out vec3 transmittance,
                        out vec3 scattering)
{
  vec3 volume_cos = ndc_to_volume(vec3(frag_uvs, frag_depth));

  scattering = texture(inScattering, volume_cos).rgb;
  transmittance = texture(inTransmittance, volume_cos).rgb;
}
