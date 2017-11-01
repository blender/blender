
/* Based on Frosbite Unified Volumetric.
 * https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite */

uniform float volume_light_clamp;

uniform vec3 volume_param; /* Parameters to the volume Z equation */

uniform vec2 volume_uv_ratio; /* To convert volume uvs to screen uvs */

/* Volume slice to view space depth. */
float volume_z_to_view_z(float z)
{
	if (ProjectionMatrix[3][3] == 0.0) {
		/* Exponential distribution */
		return (exp2(z / volume_param.z) - volume_param.x) / volume_param.y;
	}
	else {
		/* Linear distribution */
		return mix(volume_param.x, volume_param.y, z);
	}
}

float view_z_to_volume_z(float depth)
{
	if (ProjectionMatrix[3][3] == 0.0) {
		/* Exponential distribution */
		return volume_param.z * log2(depth * volume_param.y + volume_param.x);
	}
	else {
		/* Linear distribution */
		return (depth - volume_param.x) * volume_param.z;
	}
}

/* Volume texture normalized coordinates to NDC (special range [0, 1]). */
vec3 volume_to_ndc(vec3 cos)
{
	cos.z = volume_z_to_view_z(cos.z);
	cos.z = get_depth_from_view_z(cos.z);
	cos.xy /= volume_uv_ratio;
	return cos;
}

vec3 ndc_to_volume(vec3 cos)
{
	cos.z = get_view_z_from_depth(cos.z);
	cos.z = view_z_to_volume_z(cos.z);
	cos.xy *= volume_uv_ratio;
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
	return (1- sqr_g) / max(1e-8, 4.0 * M_PI * pow(1 + sqr_g - 2 * g * cos_theta, 3.0 / 2.0));
}

#ifdef LAMPS_LIB
vec3 light_volume(LightData ld, vec4 l_vector)
{
	float power;
	/* TODO : Area lighting ? */
	/* XXX : Removing Area Power. */
	/* TODO : put this out of the shader. */
	if (ld.l_type == AREA) {
		power = 0.0962 * (ld.l_sizex * ld.l_sizey * 4.0 * M_PI);
	}
	else if (ld.l_type == SUN) {
		power = 1.0;
	}
	else {
		power = 0.0248 * (4.0 * ld.l_radius * ld.l_radius * M_PI * M_PI);
	}

	/* OPTI: find a better way than calculating this on the fly */
	float lum = dot(ld.l_color, vec3(0.3, 0.6, 0.1)); /* luminance approx. */
	vec3 tint = (lum > 0.0) ? ld.l_color / lum : vec3(1.0); /* normalize lum. to isolate hue+sat */

	power /= (l_vector.w * l_vector.w);

	lum = min(lum * power, volume_light_clamp);

	return tint * lum;
}

#define VOLUMETRIC_SHADOW_MAX_STEP 32.0

uniform float volume_shadows_steps;

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
	/* Heterogeneous volume shadows */
	float dd = l_vector.w / volume_shadows_steps;
	vec3 L = l_vector.xyz * l_vector.w;
	vec3 shadow = vec3(1.0);
	for (float s = 0.5; s < VOLUMETRIC_SHADOW_MAX_STEP && s < (volume_shadows_steps - 0.1); s += 1.0) {
		vec3 pos = ray_wpos + L * (s / volume_shadows_steps);
		vec3 s_extinction = participating_media_extinction(pos, volume_extinction);
		shadow *= exp(-s_extinction * dd);
	}
	return shadow;
#else
	return vec3(1.0);
#endif /* VOLUME_SHADOW */
}
#endif

#ifdef IRRADIANCE_LIB
vec3 irradiance_volumetric(vec3 wpos)
{
	IrradianceData ir_data = load_irradiance_cell(0, vec3(1.0));
	vec3 irradiance = ir_data.cubesides[0] + ir_data.cubesides[1] + ir_data.cubesides[2];
	ir_data = load_irradiance_cell(0, vec3(-1.0));
	irradiance += ir_data.cubesides[0] + ir_data.cubesides[1] + ir_data.cubesides[2];
	irradiance *= 0.16666666; /* 1/6 */
	return irradiance;
}
#endif

uniform sampler3D inScattering;
uniform sampler3D inTransmittance;

vec4 volumetric_resolve(vec4 scene_color, vec2 frag_uvs, float frag_depth)
{
	vec3 volume_cos = ndc_to_volume(vec3(frag_uvs, frag_depth));

	vec3 scattering = texture(inScattering, volume_cos).rgb;
	vec3 transmittance = texture(inTransmittance, volume_cos).rgb;

	return vec4(scene_color.rgb * transmittance + scattering, scene_color.a);
}
