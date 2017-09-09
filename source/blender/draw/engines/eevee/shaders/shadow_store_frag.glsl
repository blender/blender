
layout(std140) uniform shadow_render_block {
	mat4 ShadowMatrix[6];
	mat4 FaceViewMatrix[6];
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
#endif
uniform float shadowFilterSize;

out vec4 FragColor;

vec3 octahedral_to_cubemap_proj(vec2 co)
{
	co = co * 2.0 - 1.0;

	vec2 abs_co = abs(co);
	vec3 v = vec3(co, 1.0 - (abs_co.x + abs_co.y));

	if ( abs_co.x + abs_co.y > 1.0 ) {
		v.xy = (abs(co.yx) - 1.0) * -sign(co.xy);
	}

	return v;
}

void make_orthonormal_basis(vec3 N, out vec3 T, out vec3 B)
{
	vec3 UpVector = (abs(N.z) < 0.999) ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	T = normalize(cross(UpVector, N));
	B = cross(N, T);
}

/* Marco Salvi's GDC 2008 presentation about shadow maps pre-filtering techniques slide 24 */
float ln_space_prefilter(float w0, float x, float w1, float y)
{
    return x + log(w0 + w1 * exp(y - x));
}

vec4 ln_space_prefilter(float w0, vec4 x, float w1, vec4 y)
{
    return x + log(w0 + w1 * exp(y - x));
}

#ifdef CSM
vec3 get_texco(vec3 cos, vec2 ofs)
{
	cos.xy += ofs * shadowFilterSize;
	return cos;
}
#else /* CUBEMAP */
vec3 T, B; /* global vars */
vec3 get_texco(vec3 cos, vec2 ofs)
{
	return cos + ofs.x * T + ofs.y * B;
}
#endif

void main() {
	vec3 cos;

	cos.xy = gl_FragCoord.xy * storedTexelSize;

#ifdef CSM
	cos.z = float(cascadeId);
#else /* CUBEMAP */
	/* add a 2 pixel border to ensure filtering is correct */
	cos.xy *= 1.0 + storedTexelSize * 2.0;
	cos.xy -= storedTexelSize;

	float pattern = 1.0;

	/* edge mirroring : only mirror if directly adjacent
	 * (not diagonally adjacent) */
	vec2 m = abs(cos.xy - 0.5) + 0.5;
	vec2 f = floor(m);
	if (f.x - f.y != 0.0) {
		cos.xy = 1.0 - cos.xy;
	}

	/* clamp to [0-1] */
	cos.xy = fract(cos.xy);

	/* get cubemap vector */
	cos = normalize(octahedral_to_cubemap_proj(cos.xy));
	make_orthonormal_basis(cos, T, B);

	T *= shadowFilterSize;
	B *= shadowFilterSize;
#endif

#ifdef ESM
	vec4 accum = vec4(0.0);

	/* disc blur in log space. */
	vec4 depths;
	depths.x = texture(shadowTexture, get_texco(cos, concentric[0])).r;
	depths.y = texture(shadowTexture, get_texco(cos, concentric[1])).r;
	depths.z = texture(shadowTexture, get_texco(cos, concentric[2])).r;
	depths.w = texture(shadowTexture, get_texco(cos, concentric[3])).r;
	accum = ln_space_prefilter(0.0, accum, shadowInvSampleCount, depths);

	for (int i = 4; i < shadowSampleCount && i < CONCENTRIC_SAMPLE_NUM; i += 4) {
		depths.x = texture(shadowTexture, get_texco(cos, concentric[i+0])).r;
		depths.y = texture(shadowTexture, get_texco(cos, concentric[i+1])).r;
		depths.z = texture(shadowTexture, get_texco(cos, concentric[i+2])).r;
		depths.w = texture(shadowTexture, get_texco(cos, concentric[i+3])).r;
		accum = ln_space_prefilter(1.0, accum, shadowInvSampleCount, depths);
	}

	accum.x = ln_space_prefilter(1.0, accum.x, 1.0, accum.y);
	accum.x = ln_space_prefilter(1.0, accum.x, 1.0, accum.z);
	accum.x = ln_space_prefilter(1.0, accum.x, 1.0, accum.w);
	FragColor = accum.xxxx;

#else /* VSM */
	vec2 accum = vec2(0.0);

	/* disc blur. */
	vec4 depths1, depths2;
	for (int i = 0; i < shadowSampleCount && i < CONCENTRIC_SAMPLE_NUM; i += 4) {
		depths1.xy = texture(shadowTexture, get_texco(cos, concentric[i+0])).rg;
		depths1.zw = texture(shadowTexture, get_texco(cos, concentric[i+1])).rg;
		depths2.xy = texture(shadowTexture, get_texco(cos, concentric[i+2])).rg;
		depths2.zw = texture(shadowTexture, get_texco(cos, concentric[i+3])).rg;
		accum += depths1.xy + depths1.zw + depths2.xy + depths2.zw;
	}

	FragColor = accum.xyxy * shadowInvSampleCount;
#endif
}