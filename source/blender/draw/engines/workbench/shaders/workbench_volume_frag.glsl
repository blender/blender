
uniform mat4 ProjectionMatrix;
uniform mat4 ModelMatrixInverse;
uniform mat4 ModelViewMatrixInverse;
uniform mat4 ModelMatrix;

uniform sampler2D depthBuffer;
uniform sampler3D densityTexture;

uniform int samplesLen = 256;
uniform float stepLength; /* Step length in local space. */
uniform float densityScale; /* Simple Opacity multiplicator. */
uniform vec4 viewvecs[3];

uniform float slicePosition;
uniform int sliceAxis; /* -1 is no slice, 0 is X, 1 is Y, 2 is Z. */

#ifdef VOLUME_SLICE
in vec3 localPos;
#endif

out vec4 fragColor;

#define M_PI  3.1415926535897932        /* pi */

float phase_function_isotropic()
{
	return 1.0 / (4.0 * M_PI);
}

float get_view_z_from_depth(float depth)
{
	if (ProjectionMatrix[3][3] == 0.0) {
		float d = 2.0 * depth - 1.0;
		return -ProjectionMatrix[3][2] / (d + ProjectionMatrix[2][2]);
	}
	else {
		return viewvecs[0].z + depth * viewvecs[1].z;
	}
}

vec3 get_view_space_from_depth(vec2 uvcoords, float depth)
{
	if (ProjectionMatrix[3][3] == 0.0) {
		return vec3(viewvecs[0].xy + uvcoords * viewvecs[1].xy, 1.0) * get_view_z_from_depth(depth);
	}
	else {
		return viewvecs[0].xyz + vec3(uvcoords, depth) * viewvecs[1].xyz;
	}
}

float max_v3(vec3 v) { return max(v.x, max(v.y, v.z)); }

float line_unit_box_intersect_dist(vec3 lineorigin, vec3 linedirection)
{
	/* https://seblagarde.wordpress.com/2012/09/29/image-based-lighting-approaches-and-parallax-corrected-cubemap/ */
	vec3 firstplane  = (vec3( 1.0) - lineorigin) / linedirection;
	vec3 secondplane = (vec3(-1.0) - lineorigin) / linedirection;
	vec3 furthestplane = min(firstplane, secondplane);
	return max_v3(furthestplane);
}

void volume_properties(vec3 ls_pos, out vec3 scattering, out float extinction)
{
	scattering = vec3(0.0);
	extinction = 1e-8;

	vec4 density = texture(densityTexture, ls_pos * 0.5 + 0.5);
	density.rgb /= density.a;
	density *= densityScale;

	scattering = density.rgb;
	extinction = max(1e-8, density.a);
}

#define P(x) ((x + 0.5) * (1.0 / 16.0))
const vec4 dither_mat[4] = vec4[4](
	vec4( P(0.0),  P(8.0),  P(2.0), P(10.0)),
	vec4(P(12.0),  P(4.0), P(14.0),  P(6.0)),
	vec4( P(3.0), P(11.0),  P(1.0),  P(9.0)),
	vec4(P(15.0),  P(7.0), P(13.0),  P(5.0))
);

vec4 volume_integration(
        vec3 ray_ori, vec3 ray_dir, float ray_inc, float ray_max, float step_len)
{
	/* Start with full transmittance and no scattered light. */
	vec3 final_scattering = vec3(0.0);
	float final_transmittance = 1.0;

	ivec2 tx = ivec2(gl_FragCoord.xy) % 4;
	float noise = dither_mat[tx.x][tx.y];

	float ray_len = noise * ray_inc;
	for (int i = 0; i < samplesLen && ray_len < ray_max; ++i, ray_len += ray_inc) {
		vec3 ls_pos = ray_ori + ray_dir * ray_len;

		vec3 Lscat;
		float s_extinction;
		volume_properties(ls_pos, Lscat, s_extinction);
		/* Evaluate Scattering */
		float Tr = exp(-s_extinction * step_len);
		/* integrate along the current step segment */
		Lscat = (Lscat - Lscat * Tr) / s_extinction;
		/* accumulate and also take into account the transmittance from previous steps */
		final_scattering += final_transmittance * Lscat;
		final_transmittance *= Tr;
	}

	return vec4(final_scattering, 1.0 - final_transmittance);
}

void main()
{
#ifdef VOLUME_SLICE
	/* Manual depth test. TODO remove. */
	float depth = texelFetch(depthBuffer, ivec2(gl_FragCoord.xy), 0).r;
	if (gl_FragCoord.z >= depth) {
		discard;
	}

	ivec3 volume_size = textureSize(densityTexture, 0);
	float step_len;
	if (sliceAxis == 0) {
		step_len = float(volume_size.x);
	}
	else if (sliceAxis == 1) {
		step_len = float(volume_size.y);
	}
	else {
		step_len = float(volume_size.z);
	}
	/* FIXME Should be in world space but is in local space. */
	step_len = 1.0 / step_len;

	vec3 Lscat;
	float s_extinction;
	volume_properties(localPos, Lscat, s_extinction);
	/* Evaluate Scattering */
	float Tr = exp(-s_extinction * step_len);
	/* integrate along the current step segment */
	Lscat = (Lscat - Lscat * Tr) / s_extinction;

	fragColor = vec4(Lscat, 1.0 - Tr);

#else
	vec2 screen_uv = gl_FragCoord.xy / vec2(textureSize(depthBuffer, 0).xy);
	bool is_persp = ProjectionMatrix[3][3] == 0.0;

	vec3 volume_center = ModelMatrix[3].xyz;

	float depth = texelFetch(depthBuffer, ivec2(gl_FragCoord.xy), 0).r;
	float depth_end = min(depth, gl_FragCoord.z);
	vec3 vs_ray_end = get_view_space_from_depth(screen_uv, depth_end);
	vec3 vs_ray_ori = get_view_space_from_depth(screen_uv, 0.0);
	vec3 vs_ray_dir = (is_persp) ? (vs_ray_end - vs_ray_ori) : vec3(0.0, 0.0, -1.0);
	vs_ray_dir /= abs(vs_ray_dir.z);

	vec3 ls_ray_dir = mat3(ModelViewMatrixInverse) * vs_ray_dir;
	vec3 ls_ray_ori = (ModelViewMatrixInverse * vec4(vs_ray_ori, 1.0)).xyz;
	vec3 ls_ray_end = (ModelViewMatrixInverse * vec4(vs_ray_end, 1.0)).xyz;

	/* TODO: Align rays to volume center so that it mimics old behaviour of slicing the volume. */

	float dist = line_unit_box_intersect_dist(ls_ray_ori, ls_ray_dir);
	if (dist > 0.0) {
		ls_ray_ori = ls_ray_dir * dist + ls_ray_ori;
	}

	vec3 ls_vol_isect = ls_ray_end - ls_ray_ori;
	if (dot(ls_ray_dir, ls_vol_isect) < 0.0) {
		/* Start is further away than the end.
		 * That means no volume is intersected. */
		discard;
	}

	fragColor = volume_integration(ls_ray_ori, ls_ray_dir, stepLength,
	                               length(ls_vol_isect) / length(ls_ray_dir),
	                               length(vs_ray_dir) * stepLength);
#endif
}
