
/* [Drobot2014a] Low Level Optimizations for GCN */
vec4 fast_rcp(vec4 v)
{
	return intBitsToFloat(0x7eef370b - floatBitsToInt(v));
}

vec3 brdf_approx(vec3 spec_color, float roughness, float NV)
{
	/* Treat anything below 2% as shadowing.
	 * (in other words, makes it possible to completely disable
	 * specular on a material by setting specular color to black). */
	float shadowing = clamp(50.0 * spec_color.g, 0.0, 1.0);
	/* Very rough own approx. We don't need it to be correct, just fast.
	 * Just simulate fresnel effect with roughness attenuation. */
	float fresnel = exp2(-8.35 * NV) * (1.0 - roughness);
	return mix(spec_color, vec3(1.0), fresnel) * shadowing;
}

void prep_specular(
        vec3 L, vec3 I, vec3 N, vec3 R,
        out float NL, out float wrapped_NL, out float spec_angle)
{
	wrapped_NL = dot(L, R);
	vec3 half_dir = normalize(L + I);
	spec_angle = clamp(dot(half_dir, N), 0.0, 1.0);
	NL = clamp(dot(L, N), 0.0, 1.0);
}

/* Normalized Blinn shading */
vec4 blinn_specular(vec4 shininess, vec4 spec_angle, vec4 NL)
{
	/* Pi is already divided in the lamp power.
	 * normalization_factor = (shininess + 8.0) / (8.0 * M_PI) */
	vec4 normalization_factor = shininess * 0.125 + 1.0;
	vec4 spec_light = pow(spec_angle, shininess) * NL * normalization_factor;

	return spec_light;
}

/* NL need to be unclamped. w in [0..1] range. */
vec4 wrapped_lighting(vec4 NL, vec4 w)
{
	vec4 w_1 = w + 1.0;
	vec4 denom = fast_rcp(w_1 * w_1);
	return clamp((NL + w) * denom, 0.0, 1.0);
}

vec3 get_world_lighting(
        WorldData world_data,
        vec3 diffuse_color, vec3 specular_color, float roughness,
        vec3 N, vec3 I)
{
	vec3 specular_light = world_data.ambient_color.rgb;
	vec3 diffuse_light = world_data.ambient_color.rgb;
	vec4 wrap = vec4(
	    world_data.lights[0].diffuse_color_wrap.a,
	    world_data.lights[1].diffuse_color_wrap.a,
	    world_data.lights[2].diffuse_color_wrap.a,
	    world_data.lights[3].diffuse_color_wrap.a
	);

#ifdef V3D_SHADING_SPECULAR_HIGHLIGHT
	/* Prepare Specular computation. Eval 4 lights at once. */
	vec3 R = -reflect(I, N);
	vec4 spec_angle, spec_NL, wrap_NL;
	prep_specular(world_data.lights[0].direction.xyz, I, N, R, spec_NL.x, wrap_NL.x, spec_angle.x);
	prep_specular(world_data.lights[1].direction.xyz, I, N, R, spec_NL.y, wrap_NL.y, spec_angle.y);
	prep_specular(world_data.lights[2].direction.xyz, I, N, R, spec_NL.z, wrap_NL.z, spec_angle.z);
	prep_specular(world_data.lights[3].direction.xyz, I, N, R, spec_NL.w, wrap_NL.w, spec_angle.w);

	vec4 gloss = vec4(1.0 - roughness);
	/* Reduce gloss for smooth light. (simulate bigger light) */
	gloss *= 1.0 - wrap;
	vec4 shininess = exp2(10.0 * gloss + 1.0);

	vec4 spec_light = blinn_specular(shininess, spec_angle, spec_NL);

	/* Simulate Env. light. */
	vec4 w = mix(wrap, vec4(1.0), roughness);
	vec4 spec_env = wrapped_lighting(wrap_NL, w);

	spec_light = mix(spec_light, spec_env, wrap * wrap);

	/* Multiply result by lights specular colors. */
	specular_light += spec_light.x * world_data.lights[0].specular_color.rgb;
	specular_light += spec_light.y * world_data.lights[1].specular_color.rgb;
	specular_light += spec_light.z * world_data.lights[2].specular_color.rgb;
	specular_light += spec_light.w * world_data.lights[3].specular_color.rgb;

	float NV = clamp(dot(N, I), 0.0, 1.0);
	specular_color = brdf_approx(specular_color, roughness, NV);
#endif
	specular_light *= specular_color;

	/* Prepare diffuse computation. Eval 4 lights at once. */
	vec4 diff_NL;
	diff_NL.x = dot(world_data.lights[0].direction.xyz, N);
	diff_NL.y = dot(world_data.lights[1].direction.xyz, N);
	diff_NL.z = dot(world_data.lights[2].direction.xyz, N);
	diff_NL.w = dot(world_data.lights[3].direction.xyz, N);

	vec4 diff_light = wrapped_lighting(diff_NL, wrap);

	/* Multiply result by lights diffuse colors. */
	diffuse_light += diff_light.x * world_data.lights[0].diffuse_color_wrap.rgb;
	diffuse_light += diff_light.y * world_data.lights[1].diffuse_color_wrap.rgb;
	diffuse_light += diff_light.z * world_data.lights[2].diffuse_color_wrap.rgb;
	diffuse_light += diff_light.w * world_data.lights[3].diffuse_color_wrap.rgb;

	/* Energy conservation with colored specular look strange.
	 * Limit this strangeness by using mono-chromatic specular intensity. */
	float spec_energy = dot(specular_color, vec3(0.33333));

	diffuse_light *= diffuse_color * (1.0 - spec_energy);

	return diffuse_light + specular_light;
}
