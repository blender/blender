
uniform mat4 ProjectionMatrix;

uniform sampler2D colorBuffer;
uniform sampler2D depthBuffer;

uniform vec3 dofParams;

#define dof_aperturesize    dofParams.x
#define dof_distance        dofParams.y
#define dof_invsensorsize   dofParams.z

uniform vec4 bokehParams;

#define bokeh_sides         bokehParams.x /* Polygon Bokeh shape number of sides */
#define bokeh_rotation      bokehParams.y
#define bokeh_ratio         bokehParams.z
#define bokeh_maxsize       bokehParams.w

uniform vec2 nearFar; /* Near & far view depths values */

#define M_PI 3.1415926535897932384626433832795
#define M_2PI 6.2831853071795864769252868

/* -------------- Utils ------------- */

/* calculate 4 samples at once */
float calculate_coc(in float zdepth)
{
	float coc = dof_aperturesize * (dof_distance / zdepth - 1.0);
	return coc * dof_invsensorsize; /* divide by sensor size to get the normalized size */
}

vec4 calculate_coc(in vec4 zdepth)
{
	vec4 coc = dof_aperturesize * (vec4(dof_distance) / zdepth - vec4(1.0));
	return coc * dof_invsensorsize; /* divide by sensor size to get the normalized size */
}

float max4(vec4 x) { return max(max(x.x, x.y), max(x.z, x.w)); }

float linear_depth(float z)
{
	/* if persp */
	if (ProjectionMatrix[3][3] == 0.0) {
		return (nearFar.x  * nearFar.y) / (z * (nearFar.x - nearFar.y) + nearFar.y);
	}
	else {
		return (z * 2.0 - 1.0) * nearFar.y;
	}
}

vec4 linear_depth(vec4 z)
{
	/* if persp */
	if (ProjectionMatrix[3][3] == 0.0) {
		return (nearFar.xxxx  * nearFar.yyyy) / (z * (nearFar.xxxx - nearFar.yyyy) + nearFar.yyyy);
	}
	else {
		return (z * 2.0 - 1.0) * nearFar.yyyy;
	}
}

#define THRESHOLD 0.0

#ifdef STEP_DOWNSAMPLE

layout(location = 0) out vec4 nearColor;
layout(location = 1) out vec4 farColor;
layout(location = 2) out vec2 cocData;

/* Downsample the color buffer to half resolution.
 * Weight color samples by
 * Compute maximum CoC for near and far blur. */
void main(void)
{
	ivec4 uvs = ivec4(gl_FragCoord.xyxy) * 2 + ivec4(0, 0, 1, 1);

	/* custom downsampling */
	vec4 color1 = texelFetch(colorBuffer, uvs.xy, 0);
	vec4 color2 = texelFetch(colorBuffer, uvs.zw, 0);
	vec4 color3 = texelFetch(colorBuffer, uvs.zy, 0);
	vec4 color4 = texelFetch(colorBuffer, uvs.xw, 0);

	/* Leverage SIMD by combining 4 depth samples into a vec4 */
	vec4 depth;
	depth.r = texelFetch(depthBuffer, uvs.xy, 0).r;
	depth.g = texelFetch(depthBuffer, uvs.zw, 0).r;
	depth.b = texelFetch(depthBuffer, uvs.zy, 0).r;
	depth.a = texelFetch(depthBuffer, uvs.xw, 0).r;

	vec4 zdepth = linear_depth(depth);

	/* Compute signed CoC for each depth samples */
	vec4 coc_near = calculate_coc(zdepth);
	vec4 coc_far = -coc_near;

	/* now we need to write the near-far fields premultiplied by the coc */
	vec4 near_weights = step(THRESHOLD, coc_near);
	vec4 far_weights = step(THRESHOLD, coc_far);

	/* now write output to weighted buffers. */
	nearColor = color1 * near_weights.x +
	            color2 * near_weights.y +
	            color3 * near_weights.z +
	            color4 * near_weights.w;

	farColor = color1 * far_weights.x +
	           color2 * far_weights.y +
	           color3 * far_weights.z +
	           color4 * far_weights.w;

	float norm_near = dot(near_weights, near_weights);
	float norm_far = dot(far_weights, far_weights);

	if (norm_near > 0.0) {
		nearColor /= norm_near;
	}

	if (norm_far > 0.0) {
		farColor /= norm_far;
	}

	float max_near_coc = max(max4(coc_near), 0.0);
	float max_far_coc = max(max4(coc_far), 0.0);

	cocData = vec2(max_near_coc, max_far_coc);
}

#elif defined(STEP_SCATTER)

flat in vec4 color;
/* coordinate used for calculating radius */
in vec2 particlecoord;

out vec4 fragColor;

/* accumulate color in the near/far blur buffers */
void main(void)
{
	/* Early out */
	float dist_sqrd = dot(particlecoord, particlecoord);

	/* Circle Dof */
	if (dist_sqrd > 1.0) {
		discard;
	}

	/* Regular Polygon Dof */
	if (bokeh_sides > 0.0) {
		/* Circle parametrization */
		float theta = atan(particlecoord.y, particlecoord.x) + bokeh_rotation;
		float r;

		r = cos(M_PI / bokeh_sides) /
		    (cos(theta - (M_2PI / bokeh_sides) * floor((bokeh_sides * theta + M_PI) / M_2PI)));

		if (dist_sqrd > r * r) {
			discard;
		}
	}

	fragColor = color;
}

#elif defined(STEP_RESOLVE)

#define MERGE_THRESHOLD 4.0

uniform sampler2D farBuffer;
uniform sampler2D nearBuffer;

in vec4 uvcoordsvar;
out vec4 fragColor;

vec4 upsample_filter(sampler2D tex, vec2 uv, vec2 texelSize)
{
#if 1 /* 9-tap bilinear upsampler (tent filter) */
	vec4 d = texelSize.xyxy * vec4(1, 1, -1, 0);

	vec4 s;
	s  = textureLod(tex, uv - d.xy, 0.0);
	s += textureLod(tex, uv - d.wy, 0.0) * 2;
	s += textureLod(tex, uv - d.zy, 0.0);

	s += textureLod(tex, uv + d.zw, 0.0) * 2;
	s += textureLod(tex, uv       , 0.0) * 4;
	s += textureLod(tex, uv + d.xw, 0.0) * 2;

	s += textureLod(tex, uv + d.zy, 0.0);
	s += textureLod(tex, uv + d.wy, 0.0) * 2;
	s += textureLod(tex, uv + d.xy, 0.0);

	return s * (1.0 / 16.0);
#else
	/* 4-tap bilinear upsampler */
	vec4 d = texelSize.xyxy * vec4(-1, -1, +1, +1) * 0.5;

	vec4 s;
	s  = textureLod(tex, uv + d.xy, 0.0);
	s += textureLod(tex, uv + d.zy, 0.0);
	s += textureLod(tex, uv + d.xw, 0.0);
	s += textureLod(tex, uv + d.zw, 0.0);

	return s * (1.0 / 4.0);
#endif
}

/* Combine the Far and Near color buffers */
void main(void)
{
	/* Recompute Near / Far CoC per pixel */
	float depth = textureLod(depthBuffer, uvcoordsvar.xy, 0.0).r;
	float zdepth = linear_depth(depth);
	float coc_signed = calculate_coc(zdepth);
	float coc_far = max(-coc_signed, 0.0);
	float coc_near = max(coc_signed, 0.0);

	vec2 texelSize = 1.0 / vec2(textureSize(farBuffer, 0));
	vec4 srccolor = textureLod(colorBuffer, uvcoordsvar.xy, 0.0);
	vec4 farcolor = upsample_filter(farBuffer, uvcoordsvar.xy, texelSize);
	vec4 nearcolor = upsample_filter(nearBuffer, uvcoordsvar.xy, texelSize);

	float farweight = farcolor.a;
	float nearweight = nearcolor.a;

	if (farcolor.a > 0.0) farcolor /= farcolor.a;
	if (nearcolor.a > 0.0) nearcolor /= nearcolor.a;

	float mixfac = smoothstep(1.0, MERGE_THRESHOLD, abs(coc_signed));

	float totalweight = nearweight + farweight;
	farcolor = mix(srccolor, farcolor, mixfac);
	nearcolor = mix(srccolor, nearcolor, mixfac);
	fragColor = mix(farcolor, nearcolor, nearweight / max(1e-6, totalweight));
}

#endif
