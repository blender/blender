
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

#ifdef LAMPS_LIB
vec3 light_volume(LightData ld, vec4 l_vector)
{
  float power;
  /* TODO : Area lighting ? */
  /* XXX : Removing Area Power. */
  /* TODO : put this out of the shader. */
  /* See eevee_light_setup(). */
  if (ld.l_type == AREA_RECT || ld.l_type == AREA_ELLIPSE) {
    power = (ld.l_sizex * ld.l_sizey * 4.0 * M_PI) * (1.0 / 80.0);
    if (ld.l_type == AREA_ELLIPSE) {
      power *= M_PI * 0.25;
    }
    power *= 20.0 *
             max(0.0, dot(-ld.l_forward, l_vector.xyz / l_vector.w)); /* XXX ad hoc, empirical */
  }
  else if (ld.l_type == SUN) {
    power = ld.l_radius * ld.l_radius * M_PI; /* Removing area light power*/
    power /= 1.0f + (ld.l_radius * ld.l_radius * 0.5f);
    power *= M_PI * 0.5; /* Matching cycles. */
  }
  else {
    power = (4.0 * ld.l_radius * ld.l_radius) * (1.0 / 10.0);
    power *= M_2PI; /* Matching cycles with point light. */
  }

  power /= (l_vector.w * l_vector.w);

  /* OPTI: find a better way than calculating this on the fly */
  float lum = dot(ld.l_color, vec3(0.3, 0.6, 0.1));       /* luminance approx. */
  vec3 tint = (lum > 0.0) ? ld.l_color / lum : vec3(1.0); /* normalize lum. to isolate hue+sat */

  lum = min(lum * power, volLightClamp);

  return tint * lum;
}

#  define VOLUMETRIC_SHADOW_MAX_STEP 32.0

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
#  if defined(VOLUME_SHADOW)
  /* Heterogeneous volume shadows */
  float dd = l_vector.w / volShadowSteps;
  vec3 L = l_vector.xyz * l_vector.w;
  vec3 shadow = vec3(1.0);
  for (float s = 0.5; s < VOLUMETRIC_SHADOW_MAX_STEP && s < (volShadowSteps - 0.1); s += 1.0) {
    vec3 pos = ray_wpos + L * (s / volShadowSteps);
    vec3 s_extinction = participating_media_extinction(pos, volume_extinction);
    shadow *= exp(-s_extinction * dd);
  }
  return shadow;
#  else
  return vec3(1.0);
#  endif /* VOLUME_SHADOW */
}
#endif

#ifdef IRRADIANCE_LIB
vec3 irradiance_volumetric(vec3 wpos)
{
#  ifdef IRRADIANCE_HL2
  IrradianceData ir_data = load_irradiance_cell(0, vec3(1.0));
  vec3 irradiance = ir_data.cubesides[0] + ir_data.cubesides[1] + ir_data.cubesides[2];
  ir_data = load_irradiance_cell(0, vec3(-1.0));
  irradiance += ir_data.cubesides[0] + ir_data.cubesides[1] + ir_data.cubesides[2];
  irradiance *= 0.16666666; /* 1/6 */
  return irradiance;
#  else
  return vec3(0.0);
#  endif
}
#endif

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
