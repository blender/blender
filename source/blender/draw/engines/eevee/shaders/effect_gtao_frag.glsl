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
	vec2 texel_size = 1.0 / vec2(textureSize(depthBuffer, 0)).xy;
	vec2 uvs = saturate(gl_FragCoord.xy * texel_size);

	float depth = textureLod(depthBuffer, uvs, 0.0).r;

	vec3 viewPosition = get_view_space_from_depth(uvs, depth);
	vec3 V = viewCameraVec;
	vec3 normal = normal_decode(texture(normalBuffer, uvs).rg, V);

	vec3 bent_normal;
	float visibility;

	vec4 noise = texelfetch_noise_tex(gl_FragCoord.xy);

	gtao_deferred(normal, viewPosition, noise, depth, visibility, bent_normal);

	/* Handle Background case. Prevent artifact due to uncleared Horizon Render Target. */
	FragColor = vec4((depth == 1.0) ? 0.0 : visibility);
}

#else

#ifdef LAYERED_DEPTH
uniform sampler2DArray depthBufferLayered;
uniform int layer;
# define gtao_depthBuffer depthBufferLayered
# define gtao_textureLod(a, b, c) textureLod(a, vec3(b, layer), c)

#else
# define gtao_depthBuffer depthBuffer
# define gtao_textureLod(a, b, c) textureLod(a, b, c)

#endif

void main()
{
	vec2 uvs = saturate(gl_FragCoord.xy / vec2(textureSize(gtao_depthBuffer, 0).xy));
	float depth = gtao_textureLod(gtao_depthBuffer, uvs, 0.0).r;

	if (depth == 1.0) {
		/* Do not trace for background */
		FragColor = vec4(0.0);
		return;
	}

	/* Avoid self shadowing. */
	depth = saturate(depth - 3e-6); /* Tweaked for 24bit depth buffer. */

	vec3 viewPosition = get_view_space_from_depth(uvs, depth);
	vec4 noise = texelfetch_noise_tex(gl_FragCoord.xy);
	vec2 max_dir = get_max_dir(viewPosition.z);
	vec4 dirs;
	dirs.xy = get_ao_dir(noise.x * 0.5);
	dirs.zw = get_ao_dir(noise.x * 0.5 + 0.5);

	/* Search in 4 directions. */
	FragColor.xy = search_horizon_sweep(dirs.xy, viewPosition, uvs, noise.y, max_dir);
	FragColor.zw = search_horizon_sweep(dirs.zw, viewPosition, uvs, noise.y, max_dir);

	/* Resize output for integer texture. */
	FragColor = pack_horizons(FragColor);
}
#endif
