// color buffer
uniform sampler2D colorbuffer;

// jitter texture for ssao
uniform sampler2D jitter_tex;

// concentric sample texture for ssao
uniform sampler1D ssao_concentric_tex;

// depth buffer
uniform sampler2D depthbuffer;
// coordinates on framebuffer in normalized (0.0-1.0) uv space
varying vec4 uvcoordsvar;

/* ssao_params.x : pixel scale for the ssao radious */
/* ssao_params.y : factor for the ssao darkening */
uniform vec4 ssao_params;
uniform vec3 ssao_sample_params;
uniform vec4 ssao_color;

/* store the view space vectors for the corners of the view frustum here.
 * It helps to quickly reconstruct view space vectors by using uv coordinates,
 * see http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer */
uniform vec4 viewvecs[3];

vec3 calculate_view_space_normal(in vec3 viewposition)
{
	vec3 normal = cross(normalize(dFdx(viewposition)),
	                    ssao_params.w * normalize(dFdy(viewposition)));
	return normalize(normal);
}

float calculate_ssao_factor(float depth)
{
	/* take the normalized ray direction here */
	vec2 rotX = texture2D(jitter_tex, uvcoordsvar.xy * ssao_sample_params.yz).rg;
	vec2 rotY = vec2(-rotX.y, rotX.x);

	/* occlusion is zero in full depth */
	if (depth == 1.0)
		return 0.0;

	vec3 position = get_view_space_from_depth(uvcoordsvar.xy, viewvecs[0].xyz, viewvecs[1].xyz, depth);
	vec3 normal = calculate_view_space_normal(position);

	/* find the offset in screen space by multiplying a point
	 * in camera space at the depth of the point by the projection matrix. */
	vec2 offset;
	float homcoord = gl_ProjectionMatrix[2][3] * position.z + gl_ProjectionMatrix[3][3];
	offset.x = gl_ProjectionMatrix[0][0] * ssao_params.x / homcoord;
	offset.y = gl_ProjectionMatrix[1][1] * ssao_params.x / homcoord;
	/* convert from -1.0...1.0 range to 0.0..1.0 for easy use with texture coordinates */
	offset *= 0.5;

	float factor = 0.0;
	int x;
	int num_samples = int(ssao_sample_params.x);

	for (x = 0; x < num_samples; x++) {
		vec2 dir_sample = texture1D(ssao_concentric_tex, (float(x) + 0.5) / ssao_sample_params.x).rg;

		/* rotate with random direction to get jittered result */
		vec2 dir_jittered = vec2(dot(dir_sample, rotX), dot(dir_sample, rotY));

		vec2 uvcoords = uvcoordsvar.xy + dir_jittered * offset;

		if (uvcoords.x > 1.0 || uvcoords.x < 0.0 || uvcoords.y > 1.0 || uvcoords.y < 0.0)
			continue;

		float depth_new = texture2D(depthbuffer, uvcoords).r;
		if (depth_new != 1.0) {
			vec3 pos_new = get_view_space_from_depth(uvcoords, viewvecs[0].xyz, viewvecs[1].xyz, depth_new);
			vec3 dir = pos_new - position;
			float len = length(dir);
			float f = dot(dir, normal);

			/* use minor bias here to avoid self shadowing */
			if (f > 0.05 * len)
				factor += f * 1.0 / (len * (1.0 + len * len * ssao_params.z));
		}
	}

	factor /= ssao_sample_params.x;

	return clamp(factor * ssao_params.y, 0.0, 1.0);
}

void main()
{
	float depth = texture2D(depthbuffer, uvcoordsvar.xy).r;
	vec4 scene_col = texture2D(colorbuffer, uvcoordsvar.xy);
	vec3 final_color = mix(scene_col.rgb, ssao_color.rgb, calculate_ssao_factor(depth));
	gl_FragColor = vec4(final_color.rgb, scene_col.a);
}
