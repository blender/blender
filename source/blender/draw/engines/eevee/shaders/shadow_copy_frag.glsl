/* Copy the depth only shadowmap into another texture while converting
 * to linear depth (or other storage method) and doing a 3x3 box filter. */

layout(std140) uniform shadow_render_block {
	vec4 lampPosition;
	float cubeTexelSize;
	float storedTexelSize;
	float nearClip;
	float farClip;
	int shadowSampleCount;
	float shadowInvSampleCount;
};

#ifdef CSM
uniform sampler2DArray shadowTexture;
uniform int cascadeId;
#else
uniform samplerCube shadowTexture;
uniform int faceId;
#endif
uniform float shadowFilterSize;

out vec4 FragColor;

float linear_depth(float z)
{
	return (nearClip  * farClip) / (z * (nearClip - farClip) + farClip);
}

vec4 linear_depth(vec4 z)
{
	return (nearClip  * farClip) / (z * (nearClip - farClip) + farClip);
}

#ifdef CSM
vec4 get_world_distance(vec4 depths, vec3 cos[4])
{
	/* Background case */
	vec4 is_background = step(vec4(0.99999), depths);
	depths *= abs(farClip - nearClip); /* Same factor as in shadow_cascade(). */
	depths += 1e1 * is_background;
	return depths;
}

float get_world_distance(float depth, vec3 cos)
{
	/* Background case */
	float is_background = step(0.9999, depth);
	depth *= abs(farClip - nearClip); /* Same factor as in shadow_cascade(). */
	depth += 1e1 * is_background;
	return depth;
}
#else /* CUBEMAP */
vec4 get_world_distance(vec4 depths, vec3 cos[4])
{
	vec4 is_background = step(vec4(1.0), depths);
	depths = linear_depth(depths);
	depths += vec4(1e1) * is_background;
	cos[0] = normalize(abs(cos[0]));
	cos[1] = normalize(abs(cos[1]));
	cos[2] = normalize(abs(cos[2]));
	cos[3] = normalize(abs(cos[3]));
	vec4 cos_vec;
	cos_vec.x = max(cos[0].x, max(cos[0].y, cos[0].z));
	cos_vec.y = max(cos[1].x, max(cos[1].y, cos[1].z));
	cos_vec.z = max(cos[2].x, max(cos[2].y, cos[2].z));
	cos_vec.w = max(cos[3].x, max(cos[3].y, cos[3].z));
	return depths / cos_vec;
}

float get_world_distance(float depth, vec3 cos)
{
	float is_background = step(1.0, depth);
	depth = linear_depth(depth);
	depth += 1e1 * is_background;
	cos = normalize(abs(cos));
	float cos_vec = max(cos.x, max(cos.y, cos.z));
	return depth / cos_vec;
}
#endif

/* Marco Salvi's GDC 2008 presentation about shadow maps pre-filtering techniques slide 24 */
float ln_space_prefilter(float w0, float x, float w1, float y)
{
    return x + log(w0 + w1 * exp(y - x));
}

#define SAMPLE_WEIGHT 0.11111

#ifdef ESM
void prefilter(vec4 depths, inout float accum)
{
	accum = ln_space_prefilter(1.0, accum, SAMPLE_WEIGHT, depths.x);
	accum = ln_space_prefilter(1.0, accum, SAMPLE_WEIGHT, depths.y);
	accum = ln_space_prefilter(1.0, accum, SAMPLE_WEIGHT, depths.z);
	accum = ln_space_prefilter(1.0, accum, SAMPLE_WEIGHT, depths.w);
}
#else /* VSM */
void prefilter(vec4 depths, inout vec2 accum)
{
	vec4 depths_sqr = depths * depths;
	accum += vec2(dot(vec4(1.0), depths), dot(vec4(1.0), depths_sqr)) * SAMPLE_WEIGHT;
}
#endif

#ifdef CSM
vec3 get_texco(vec2 uvs, vec2 ofs)
{
	return vec3(uvs + ofs, float(cascadeId));
}
#else /* CUBEMAP */
const vec3 minorAxisX[6] = vec3[6](
	vec3(0.0f, 0.0f, -1.0f),
	vec3(0.0f, 0.0f, 1.0f),
	vec3(1.0f, 0.0f, 0.0f),
	vec3(1.0f, 0.0f, 0.0f),
	vec3(1.0f, 0.0f, 0.0f),
	vec3(-1.0f, 0.0f, 0.0f)
);

const vec3 minorAxisY[6] = vec3[6](
	vec3(0.0f, -1.0f, 0.0f),
	vec3(0.0f, -1.0f, 0.0f),
	vec3(0.0f, 0.0f, 1.0f),
	vec3(0.0f, 0.0f, -1.0f),
	vec3(0.0f, -1.0f, 0.0f),
	vec3(0.0f, -1.0f, 0.0f)
);

const vec3 majorAxis[6] = vec3[6](
	vec3(1.0f, 0.0f, 0.0f),
	vec3(-1.0f, 0.0f, 0.0f),
	vec3(0.0f, 1.0f, 0.0f),
	vec3(0.0f, -1.0f, 0.0f),
	vec3(0.0f, 0.0f, 1.0f),
	vec3(0.0f, 0.0f, -1.0f)
);

vec3 get_texco(vec2 uvs, vec2 ofs)
{
	uvs += ofs;
	return majorAxis[faceId] + uvs.x * minorAxisX[faceId] + uvs.y * minorAxisY[faceId];
}
#endif

void main() {
	/* Copy the depth only shadowmap into another texture while converting
	 * to linear depth and do a 3x3 box blur. */

#ifdef CSM
	vec2 uvs = gl_FragCoord.xy * storedTexelSize;
#else /* CUBEMAP */
	vec2 uvs = gl_FragCoord.xy * cubeTexelSize * 2.0 - 1.0;
#endif

	/* Center texel */
	vec3 co = get_texco(uvs, vec2(0.0));
	float depth = texture(shadowTexture, co).r;
	depth = get_world_distance(depth, co);

	if (shadowFilterSize == 0.0) {
#ifdef ESM
		FragColor = vec4(depth);
#else /* VSM */
		FragColor = vec2(depth, depth * depth).xyxy;
#endif
		return;
	}

#ifdef ESM
	float accum = ln_space_prefilter(0.0, 0.0, SAMPLE_WEIGHT, depth);
#else /* VSM */
	vec2 accum = vec2(depth, depth * depth) * SAMPLE_WEIGHT;
#endif

#ifdef CSM
	vec3 ofs = vec3(1.0, 0.0, -1.0) * shadowFilterSize;
#else /* CUBEMAP */
	vec3 ofs = vec3(1.0, 0.0, -1.0) * shadowFilterSize;
#endif

	vec3 cos[4];
	cos[0] = get_texco(uvs, ofs.zz);
	cos[1] = get_texco(uvs, ofs.yz);
	cos[2] = get_texco(uvs, ofs.xz);
	cos[3] = get_texco(uvs, ofs.zy);

	vec4 depths;
	depths.x = texture(shadowTexture, cos[0]).r;
	depths.y = texture(shadowTexture, cos[1]).r;
	depths.z = texture(shadowTexture, cos[2]).r;
	depths.w = texture(shadowTexture, cos[3]).r;
	depths = get_world_distance(depths, cos);
	prefilter(depths, accum);

	cos[0] = get_texco(uvs, ofs.xy);
	cos[1] = get_texco(uvs, ofs.zx);
	cos[2] = get_texco(uvs, ofs.yx);
	cos[3] = get_texco(uvs, ofs.xx);
	depths.x = texture(shadowTexture, cos[0]).r;
	depths.y = texture(shadowTexture, cos[1]).r;
	depths.z = texture(shadowTexture, cos[2]).r;
	depths.w = texture(shadowTexture, cos[3]).r;
	depths = get_world_distance(depths, cos);
	prefilter(depths, accum);

	FragColor = vec2(accum).xyxy;
}
