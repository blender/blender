
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
vec3 get_texco(vec3 cos, const vec2 ofs)
{
	cos.xy += ofs * shadowFilterSize;
	return cos;
}
#else /* CUBEMAP */
/* global vars */
vec3 T = vec3(0.0);
vec3 B = vec3(0.0);

void make_orthonormal_basis(vec3 N)
{
	vec3 UpVector = (abs(N.z) < 0.999) ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	T = normalize(cross(UpVector, N));
	B = cross(N, T);
}

vec3 get_texco(vec3 cos, const vec2 ofs)
{
	return cos + ofs.x * T + ofs.y * B;
}

#endif

#ifdef ESM
void grouped_samples_accum(
		vec3 cos,
		const vec2 co1, const vec2 co2, const vec2 co3, const vec2 co4,
		inout vec4 accum)
{
	vec4 depths;
	depths.x = texture(shadowTexture, get_texco(cos, co1)).r;
	depths.y = texture(shadowTexture, get_texco(cos, co2)).r;
	depths.z = texture(shadowTexture, get_texco(cos, co3)).r;
	depths.w = texture(shadowTexture, get_texco(cos, co4)).r;

	accum = ln_space_prefilter(1.0, accum, shadowInvSampleCount, depths);
}
#else /* VSM */
void grouped_samples_accum(
		vec3 cos,
		const vec2 co1, const vec2 co2, const vec2 co3, const vec2 co4,
		inout vec2 accum)
{
	vec4 depths1, depths2;
	depths1.xy = texture(shadowTexture, get_texco(cos, co1)).rg;
	depths1.zw = texture(shadowTexture, get_texco(cos, co2)).rg;
	depths2.xy = texture(shadowTexture, get_texco(cos, co3)).rg;
	depths2.zw = texture(shadowTexture, get_texco(cos, co4)).rg;

	accum += depths1.xy + depths1.zw + depths2.xy + depths2.zw;
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
	make_orthonormal_basis(cos);

	T *= shadowFilterSize;
	B *= shadowFilterSize;
#endif

#ifdef ESM
	/* disc blur in log space. */
	vec4 accum = vec4(0.0);
	vec4 depths;
	depths.x = texture(shadowTexture, get_texco(cos, concentric[0])).r;
	depths.y = texture(shadowTexture, get_texco(cos, concentric[1])).r;
	depths.z = texture(shadowTexture, get_texco(cos, concentric[2])).r;
	depths.w = texture(shadowTexture, get_texco(cos, concentric[3])).r;
	accum = ln_space_prefilter(0.0, accum, shadowInvSampleCount, depths);

#else /* VSM */
	vec2 accum = vec2(0.0);
	grouped_samples_accum(cos, concentric[0], concentric[1], concentric[2], concentric[3], accum);
#endif

	/**
	 * Making the `grouped_samples_accum` be called within a loop would be
	 * the most conventional solution, however in some older gpus, transverse the huge
	 * `const vec2 concentric[]` array with variable indices is extremely slow.
	 * The solution is to use constant indices to access the array.
	 */
	if (shadowSampleCount > 4) {
		grouped_samples_accum(cos, concentric[4], concentric[5], concentric[6], concentric[7], accum);
		grouped_samples_accum(cos, concentric[8], concentric[9], concentric[10], concentric[11], accum);
		grouped_samples_accum(cos, concentric[12], concentric[13], concentric[14], concentric[15], accum);
	}
	if (shadowSampleCount > 16) {
		grouped_samples_accum(cos, concentric[16], concentric[17], concentric[18], concentric[19], accum);
		grouped_samples_accum(cos, concentric[20], concentric[21], concentric[22], concentric[23], accum);
		grouped_samples_accum(cos, concentric[24], concentric[25], concentric[26], concentric[27], accum);
		grouped_samples_accum(cos, concentric[28], concentric[29], concentric[30], concentric[31], accum);
		grouped_samples_accum(cos, concentric[32], concentric[33], concentric[34], concentric[35], accum);
	}
#ifdef HIGH_BLUR
	if (shadowSampleCount > 36) {
		grouped_samples_accum(cos, concentric[36], concentric[37], concentric[38], concentric[39], accum);
		grouped_samples_accum(cos, concentric[40], concentric[41], concentric[42], concentric[43], accum);
		grouped_samples_accum(cos, concentric[44], concentric[45], concentric[46], concentric[47], accum);
		grouped_samples_accum(cos, concentric[48], concentric[49], concentric[50], concentric[51], accum);
		grouped_samples_accum(cos, concentric[52], concentric[53], concentric[54], concentric[55], accum);
		grouped_samples_accum(cos, concentric[56], concentric[57], concentric[58], concentric[59], accum);
		grouped_samples_accum(cos, concentric[60], concentric[61], concentric[62], concentric[63], accum);
	}
	if (shadowSampleCount > 64) {
		grouped_samples_accum(cos, concentric[64], concentric[65], concentric[66], concentric[67], accum);
		grouped_samples_accum(cos, concentric[68], concentric[69], concentric[70], concentric[71], accum);
		grouped_samples_accum(cos, concentric[72], concentric[73], concentric[74], concentric[75], accum);
		grouped_samples_accum(cos, concentric[76], concentric[77], concentric[78], concentric[79], accum);
		grouped_samples_accum(cos, concentric[80], concentric[81], concentric[82], concentric[83], accum);
		grouped_samples_accum(cos, concentric[84], concentric[85], concentric[86], concentric[87], accum);
		grouped_samples_accum(cos, concentric[88], concentric[89], concentric[90], concentric[91], accum);
		grouped_samples_accum(cos, concentric[92], concentric[93], concentric[94], concentric[95], accum);
		grouped_samples_accum(cos, concentric[96], concentric[97], concentric[98], concentric[99], accum);
	}
	if (shadowSampleCount > 100) {
		grouped_samples_accum(cos, concentric[100], concentric[101], concentric[102], concentric[103], accum);
		grouped_samples_accum(cos, concentric[104], concentric[105], concentric[106], concentric[107], accum);
		grouped_samples_accum(cos, concentric[108], concentric[109], concentric[110], concentric[111], accum);
		grouped_samples_accum(cos, concentric[112], concentric[113], concentric[114], concentric[115], accum);
		grouped_samples_accum(cos, concentric[116], concentric[117], concentric[118], concentric[119], accum);
		grouped_samples_accum(cos, concentric[120], concentric[121], concentric[122], concentric[123], accum);
		grouped_samples_accum(cos, concentric[124], concentric[125], concentric[126], concentric[127], accum);
		grouped_samples_accum(cos, concentric[128], concentric[129], concentric[130], concentric[131], accum);
		grouped_samples_accum(cos, concentric[132], concentric[133], concentric[134], concentric[135], accum);
		grouped_samples_accum(cos, concentric[136], concentric[137], concentric[138], concentric[139], accum);
		grouped_samples_accum(cos, concentric[140], concentric[141], concentric[142], concentric[143], accum);
	}
	if (shadowSampleCount > 144) {
		grouped_samples_accum(cos, concentric[144], concentric[145], concentric[146], concentric[147], accum);
		grouped_samples_accum(cos, concentric[148], concentric[149], concentric[150], concentric[151], accum);
		grouped_samples_accum(cos, concentric[152], concentric[153], concentric[154], concentric[155], accum);
		grouped_samples_accum(cos, concentric[156], concentric[157], concentric[158], concentric[159], accum);
		grouped_samples_accum(cos, concentric[160], concentric[161], concentric[162], concentric[163], accum);
		grouped_samples_accum(cos, concentric[164], concentric[165], concentric[166], concentric[167], accum);
		grouped_samples_accum(cos, concentric[168], concentric[169], concentric[170], concentric[171], accum);
		grouped_samples_accum(cos, concentric[172], concentric[173], concentric[174], concentric[175], accum);
		grouped_samples_accum(cos, concentric[176], concentric[177], concentric[178], concentric[179], accum);
		grouped_samples_accum(cos, concentric[180], concentric[181], concentric[182], concentric[183], accum);
		grouped_samples_accum(cos, concentric[184], concentric[185], concentric[186], concentric[187], accum);
		grouped_samples_accum(cos, concentric[188], concentric[189], concentric[190], concentric[191], accum);
		grouped_samples_accum(cos, concentric[192], concentric[193], concentric[194], concentric[195], accum);
	}
	if (shadowSampleCount > 196) {
		grouped_samples_accum(cos, concentric[196], concentric[197], concentric[198], concentric[199], accum);
		grouped_samples_accum(cos, concentric[200], concentric[201], concentric[202], concentric[203], accum);
		grouped_samples_accum(cos, concentric[204], concentric[205], concentric[206], concentric[207], accum);
		grouped_samples_accum(cos, concentric[208], concentric[209], concentric[210], concentric[211], accum);
		grouped_samples_accum(cos, concentric[212], concentric[213], concentric[114], concentric[215], accum);
		grouped_samples_accum(cos, concentric[216], concentric[217], concentric[218], concentric[219], accum);
		grouped_samples_accum(cos, concentric[220], concentric[221], concentric[222], concentric[223], accum);
		grouped_samples_accum(cos, concentric[224], concentric[225], concentric[226], concentric[227], accum);
		grouped_samples_accum(cos, concentric[228], concentric[229], concentric[230], concentric[231], accum);
		grouped_samples_accum(cos, concentric[232], concentric[233], concentric[234], concentric[235], accum);
		grouped_samples_accum(cos, concentric[236], concentric[237], concentric[238], concentric[239], accum);
		grouped_samples_accum(cos, concentric[240], concentric[241], concentric[242], concentric[243], accum);
		grouped_samples_accum(cos, concentric[244], concentric[245], concentric[246], concentric[247], accum);
		grouped_samples_accum(cos, concentric[248], concentric[249], concentric[250], concentric[251], accum);
		grouped_samples_accum(cos, concentric[252], concentric[253], concentric[254], concentric[255], accum);
	}
#endif

#ifdef ESM
	accum.x = ln_space_prefilter(1.0, accum.x, 1.0, accum.y);
	accum.x = ln_space_prefilter(1.0, accum.x, 1.0, accum.z);
	accum.x = ln_space_prefilter(1.0, accum.x, 1.0, accum.w);
	FragColor = accum.xxxx;

#else /* VSM */
	FragColor = accum.xyxy * shadowInvSampleCount;
#endif
}
