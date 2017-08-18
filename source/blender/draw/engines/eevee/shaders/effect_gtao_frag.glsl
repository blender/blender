/**
 * This shader only compute maximum horizon angles for each directions.
 * The final integration is done at the resolve stage with the shading normal.
 **/

uniform float rotationOffset;

out vec4 FragColor;

#ifdef DEBUG_AO
uniform sampler2D normalBuffer;

void main()
{
	vec4 texel_size = 1.0 / vec2(textureSize(depthBuffer, 0)).xyxy;
	vec2 uvs = saturate(gl_FragCoord.xy * texel_size.xy);

	float depth = textureLod(depthBuffer, uvs, 0.0).r;

	vec3 viewPosition = get_view_space_from_depth(uvs, depth);
	vec3 V = viewCameraVec;
	vec3 normal = normal_decode(texture(normalBuffer, uvs).rg, V);

	vec3 bent_normal;
	float visibility;
#if 1
	gtao_deferred(normal, viewPosition, depth, visibility, bent_normal);
#else
	vec2 rand = vec2((1.0 / 4.0) * float((int(gl_FragCoord.y) & 0x1) * 2 + (int(gl_FragCoord.x) & 0x1)), 0.5);
	rand = fract(rand.x + texture(utilTex, vec3(floor(gl_FragCoord.xy * 0.5) / LUT_SIZE, 2.0)).rg);
	gtao(normal, viewPosition, rand, visibility, bent_normal);
#endif
	denoise_ao(normal, depth, visibility, bent_normal);

	FragColor = vec4(visibility);
}

#else
uniform float sampleNbr;

void main()
{
	ivec2 hr_co = ivec2(gl_FragCoord.xy);
	ivec2 fs_co = get_fs_co(hr_co);

	vec2 uvs = saturate((vec2(fs_co) + 0.5) / vec2(textureSize(depthBuffer, 0)));
	float depth = textureLod(depthBuffer, uvs, 0.0).r;

	if (depth == 1.0) {
		/* Do not trace for background */
		FragColor = vec4(0.0);
		return;
	}

	/* Avoid self shadowing. */
	depth = saturate(depth - 3e-6); /* Tweaked for 24bit depth buffer. */

	vec3 viewPosition = get_view_space_from_depth(uvs, depth);

	float phi = get_phi(hr_co, fs_co, sampleNbr);
	float offset = get_offset(fs_co, sampleNbr);
	vec2 max_dir = get_max_dir(viewPosition.z);

	FragColor.xy = search_horizon_sweep(phi, viewPosition, uvs, offset, max_dir);

	/* Resize output for integer texture. */
	FragColor = pack_horizons(FragColor.xy).xyxy;
}
#endif
