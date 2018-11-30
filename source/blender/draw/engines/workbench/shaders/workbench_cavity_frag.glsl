out vec4 fragColor;

uniform sampler2D depthBuffer;
uniform sampler2D colorBuffer;
uniform sampler2D normalBuffer;
uniform usampler2D objectId;

uniform vec2 invertedViewportSize;
uniform mat4 WinMatrix; /* inverse WinMatrix */

uniform vec4 viewvecs[3];
uniform vec4 ssao_params;
uniform vec4 ssao_settings;
uniform vec2 curvature_settings;
uniform sampler2D ssao_jitter;

layout(std140) uniform samples_block {
	vec4 ssao_samples[500];
};

#define ssao_samples_num    ssao_params.x
#define jitter_tilling      ssao_params.yz
#define ssao_iteration      ssao_params.w

#define ssao_distance       ssao_settings.x
#define ssao_factor_cavity  ssao_settings.y
#define ssao_factor_edge    ssao_settings.z
#define ssao_attenuation    ssao_settings.w

vec3 get_view_space_from_depth(in vec2 uvcoords, in float depth)
{
	if (WinMatrix[3][3] == 0.0) {
		/* Perspective */
		float d = 2.0 * depth - 1.0;

		float zview = -WinMatrix[3][2] / (d + WinMatrix[2][2]);

		return zview * (viewvecs[0].xyz + vec3(uvcoords, 0.0) * viewvecs[1].xyz);
	}
	else {
		/* Orthographic */
		vec3 offset = vec3(uvcoords, depth);

		return viewvecs[0].xyz + offset * viewvecs[1].xyz;
	}
}

/* forward declartion */
void ssao_factors(
        in float depth, in vec3 normal, in vec3 position, in vec2 screenco,
        out float cavities, out float edges);


void main()
{
	vec2 screenco = vec2(gl_FragCoord.xy) * invertedViewportSize;
	ivec2 texel = ivec2(gl_FragCoord.xy);

	float cavity = 0.0, edges = 0.0, curvature = 0.0;

#ifdef USE_CAVITY
	float depth = texelFetch(depthBuffer, texel, 0).x;
	vec3 position = get_view_space_from_depth(screenco, depth);

	vec4 diffuse_color = texelFetch(colorBuffer, texel, 0);
	vec3 normal_viewport = normal_decode(texelFetch(normalBuffer, texel, 0).rg);
	if (diffuse_color.a == 0.0) {
		normal_viewport = -normal_viewport;
	}


	ssao_factors(depth, normal_viewport, position, screenco, cavity, edges);
#endif

#ifdef USE_CURVATURE
	curvature = calculate_curvature(objectId, normalBuffer, texel, curvature_settings.x, curvature_settings.y);
#endif

	float final_cavity_factor = clamp((1.0 - cavity) * (1.0 + edges) * (1.0 + curvature), 0.0, 4.0);

	/* Using UNORM render target so compress the range. */
	fragColor = vec4(final_cavity_factor / CAVITY_BUFFER_RANGE);
}
