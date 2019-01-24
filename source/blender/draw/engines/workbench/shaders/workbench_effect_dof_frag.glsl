/**
 * Separable Hexagonal Bokeh Blur by Colin Barré-Brisebois
 * https://colinbarrebrisebois.com/2017/04/18/hexagonal-bokeh-blur-revisited-part-1-basic-3-pass-version/
 * Converted and adapted from HLSL to GLSL by Clément Foucault
 **/

uniform mat4 ProjectionMatrix;
uniform vec2 invertedViewportSize;
uniform vec2 nearFar;
uniform vec3 dofParams;
uniform sampler2D inputCocTex;
uniform sampler2D maxCocTilesTex;
uniform sampler2D sceneColorTex;
uniform sampler2D sceneDepthTex;
uniform sampler2D backgroundTex;
uniform sampler2D halfResColorTex;
uniform sampler2D blurTex;

#define dof_aperturesize    dofParams.x
#define dof_distance        dofParams.y
#define dof_invsensorsize   dofParams.z

#define M_PI       3.1415926535897932        /* pi */

float max_v4(vec4 v) { return max(max(v.x, v.y), max(v.z, v.w)); }

#define weighted_sum(a, b, c, d, e, e_sum) ((a) * e.x + (b) * e.y + (c) * e.z + (d) * e.w) / max(1e-6, e_sum);

/* divide by sensor size to get the normalized size */
#define calculate_coc(zdepth) (dof_aperturesize * (dof_distance / zdepth - 1.0) * dof_invsensorsize)

#define linear_depth(z) ((ProjectionMatrix[3][3] == 0.0) \
		? (nearFar.x  * nearFar.y) / (z * (nearFar.x - nearFar.y) + nearFar.y) \
		: (z * 2.0 - 1.0) * nearFar.y)


const float MAX_COC_SIZE = 100.0;
vec2 encode_coc(float near, float far) { return vec2(near, far) / MAX_COC_SIZE; }
float decode_coc(vec2 cocs) { return max(cocs.x, cocs.y) * MAX_COC_SIZE; }
float decode_signed_coc(vec2 cocs) { return ((cocs.x > cocs.y) ? cocs.x : -cocs.y) * MAX_COC_SIZE; }

/**
 * ----------------- STEP 0 ------------------
 * Coc aware downsample.
 **/
#ifdef PREPARE

layout(location = 0) out vec4 backgroundColorCoc;
layout(location = 1) out vec2 normalizedCoc;

void main()
{
	/* Half Res pass */
	vec2 uv = (floor(gl_FragCoord.xy) * 2.0 + 0.5) * invertedViewportSize;

	ivec4 texel = ivec4(gl_FragCoord.xyxy) * 2 + ivec4(0, 0, 1, 1);

	/* custom downsampling */
	vec4 color1 = texelFetch(sceneColorTex, texel.xy, 0);
	vec4 color2 = texelFetch(sceneColorTex, texel.zw, 0);
	vec4 color3 = texelFetch(sceneColorTex, texel.zy, 0);
	vec4 color4 = texelFetch(sceneColorTex, texel.xw, 0);

	vec3 ofs = vec3(invertedViewportSize.xy, 0.0);
	vec4 depths;
	depths.x = texelFetch(sceneDepthTex, texel.xy, 0).x;
	depths.y = texelFetch(sceneDepthTex, texel.zw, 0).x;
	depths.z = texelFetch(sceneDepthTex, texel.zy, 0).x;
	depths.w = texelFetch(sceneDepthTex, texel.xw, 0).x;

	vec4 zdepths = linear_depth(depths);
	vec4 cocs_near = calculate_coc(zdepths);
	vec4 cocs_far = -cocs_near;

	float coc_near = max(max_v4(cocs_near), 0.0);
	float coc_far  = max(max_v4(cocs_far), 0.0);

	/* now we need to write the near-far fields premultiplied by the coc
	 * also use bilateral weighting by each coc values to avoid bleeding. */
	vec4 near_weights = step(0.0, cocs_near) * clamp(1.0 - abs(coc_near - cocs_near), 0.0, 1.0);
	vec4 far_weights  = step(0.0, cocs_far)  * clamp(1.0 - abs(coc_far  - cocs_far),  0.0, 1.0);

	/* now write output to weighted buffers. */
	// backgroundColorCoc   = weighted_sum(color1, color2, color3, color4, cocs_far, coc_far);
	float tot_weight_near = dot(near_weights, vec4(1.0));
	float tot_weight_far  = dot(far_weights, vec4(1.0));
	backgroundColorCoc    = weighted_sum(color1, color2, color3, color4, near_weights, tot_weight_near);
	backgroundColorCoc   += weighted_sum(color1, color2, color3, color4, far_weights, tot_weight_far);
	if (tot_weight_near > 0.0 && tot_weight_far > 0.0) {
		backgroundColorCoc   *= 0.5;
	}

	normalizedCoc = encode_coc(cocs_near.x, cocs_far.x);
}
#endif

/**
 * ----------------- STEP 1 ------------------
 * Flatten COC buffer using max filter.
 **/
#if defined(FLATTEN_VERTICAL) || defined(FLATTEN_HORIZONTAL)

layout(location = 0) out vec2 flattenedCoc;

void main()
{
#ifdef FLATTEN_HORIZONTAL
	ivec2 texel = ivec2(gl_FragCoord.xy) * ivec2(8, 1);
	vec2 cocs1 = texelFetchOffset(inputCocTex, texel, 0, ivec2(0, 0)).rg;
	vec2 cocs2 = texelFetchOffset(inputCocTex, texel, 0, ivec2(1, 0)).rg;
	vec2 cocs3 = texelFetchOffset(inputCocTex, texel, 0, ivec2(2, 0)).rg;
	vec2 cocs4 = texelFetchOffset(inputCocTex, texel, 0, ivec2(3, 0)).rg;
	vec2 cocs5 = texelFetchOffset(inputCocTex, texel, 0, ivec2(4, 0)).rg;
	vec2 cocs6 = texelFetchOffset(inputCocTex, texel, 0, ivec2(5, 0)).rg;
	vec2 cocs7 = texelFetchOffset(inputCocTex, texel, 0, ivec2(6, 0)).rg;
	vec2 cocs8 = texelFetchOffset(inputCocTex, texel, 0, ivec2(7, 0)).rg;
#else /* FLATTEN_VERTICAL */
	ivec2 texel = ivec2(gl_FragCoord.xy) * ivec2(1, 8);
	vec2 cocs1 = texelFetchOffset(inputCocTex, texel, 0, ivec2(0, 0)).rg;
	vec2 cocs2 = texelFetchOffset(inputCocTex, texel, 0, ivec2(0, 1)).rg;
	vec2 cocs3 = texelFetchOffset(inputCocTex, texel, 0, ivec2(0, 2)).rg;
	vec2 cocs4 = texelFetchOffset(inputCocTex, texel, 0, ivec2(0, 3)).rg;
	vec2 cocs5 = texelFetchOffset(inputCocTex, texel, 0, ivec2(0, 4)).rg;
	vec2 cocs6 = texelFetchOffset(inputCocTex, texel, 0, ivec2(0, 5)).rg;
	vec2 cocs7 = texelFetchOffset(inputCocTex, texel, 0, ivec2(0, 6)).rg;
	vec2 cocs8 = texelFetchOffset(inputCocTex, texel, 0, ivec2(0, 7)).rg;
#endif
	flattenedCoc = max(max(max(cocs1, cocs2), max(cocs3, cocs4)), max(max(cocs5, cocs6), max(cocs7, cocs8)));
}
#endif

/**
 * ----------------- STEP 1.ax------------------
 * Dilate COC buffer using min filter.
 **/
#if defined(DILATE_VERTICAL) || defined(DILATE_HORIZONTAL)

layout(location = 0) out vec2 dilatedCoc;

void main()
{
	vec2 texel_size = 1.0 / vec2(textureSize(inputCocTex, 0));
	vec2 uv = gl_FragCoord.xy * texel_size;
#ifdef DILATE_VERTICAL
	vec2 cocs1 = texture(inputCocTex, uv + texel_size * vec2(-3, 0)).rg;
	vec2 cocs2 = texture(inputCocTex, uv + texel_size * vec2(-2, 0)).rg;
	vec2 cocs3 = texture(inputCocTex, uv + texel_size * vec2(-1, 0)).rg;
	vec2 cocs4 = texture(inputCocTex, uv + texel_size * vec2( 0, 0)).rg;
	vec2 cocs5 = texture(inputCocTex, uv + texel_size * vec2( 1, 0)).rg;
	vec2 cocs6 = texture(inputCocTex, uv + texel_size * vec2( 2, 0)).rg;
	vec2 cocs7 = texture(inputCocTex, uv + texel_size * vec2( 3, 0)).rg;
#else /* DILATE_HORIZONTAL */
	vec2 cocs1 = texture(inputCocTex, uv + texel_size * vec2(0, -3)).rg;
	vec2 cocs2 = texture(inputCocTex, uv + texel_size * vec2(0, -2)).rg;
	vec2 cocs3 = texture(inputCocTex, uv + texel_size * vec2(0, -1)).rg;
	vec2 cocs4 = texture(inputCocTex, uv + texel_size * vec2(0,  0)).rg;
	vec2 cocs5 = texture(inputCocTex, uv + texel_size * vec2(0,  1)).rg;
	vec2 cocs6 = texture(inputCocTex, uv + texel_size * vec2(0,  2)).rg;
	vec2 cocs7 = texture(inputCocTex, uv + texel_size * vec2(0,  3)).rg;
#endif
	// dilatedCoc = max(max(cocs3, cocs4), max(max(cocs5, cocs6), cocs2));
	dilatedCoc = max(max(max(cocs1, cocs2), max(cocs3, cocs4)), max(max(cocs5, cocs6), cocs7));
}
#endif

/**
 * ----------------- STEP 2 ------------------
 * Blur vertically and diagonally.
 * Outputs vertical blur and combined blur in MRT
 **/
#ifdef BLUR1
layout(location = 0) out vec4 blurColor;

#define NUM_SAMPLES 49

/* keep in sync with GlobalsUboStorage */
layout(std140) uniform dofSamplesBlock {
	vec4 samples[NUM_SAMPLES];
};

#if 0 /* Spilar sampling. Better but slower */
void main()
{
	/* Half Res pass */
	vec2 uv = gl_FragCoord.xy * invertedViewportSize * 2.0;

	vec2 size = vec2(textureSize(halfResColorTex, 0).xy);
	ivec2 texel = ivec2(uv * size);

	vec4 color = texelFetch(halfResColorTex, texel, 0);
	float coc = decode_signed_coc(texelFetch(inputCocTex, texel, 0).rg);

	/* TODO Ensure alignement */
	vec2 max_radii = texture(maxCocTilesTex, (0.5 + floor(gl_FragCoord.xy / 8.0)) / vec2(textureSize(maxCocTilesTex, 0))).rg;
	float max_radius = decode_coc(max_radii);

	float center_coc = coc;
	float tot = 1.0;

	for (int i = 0; i < NUM_SAMPLES; ++i) {
		vec2 tc = uv + samples[i].xy * invertedViewportSize * max_radius;

		vec4 samp = texture(halfResColorTex, tc);
		coc = decode_signed_coc(texture(inputCocTex, tc).rg);
		if (coc > center_coc) {
			coc = clamp(abs(coc), 0.0, abs(center_coc) * 2.0);
		}
		float radius = max_radius * float(i + 1) / float(NUM_SAMPLES);
		float m = smoothstep(radius - 0.5, radius + 0.5, abs(coc));
		color += mix(color / tot, samp, m);
		tot += 1.0;
	}

	blurColor = color / tot;
}
#else
void main()
{
	/* Half Res pass */
	vec2 uv = gl_FragCoord.xy * invertedViewportSize * 2.0;

	vec2 size = vec2(textureSize(halfResColorTex, 0).xy);
	ivec2 texel = ivec2(uv * size);

	float coc = decode_coc(texelFetch(inputCocTex, texel, 0).rg);
	float tot = max(0.5, coc);

	vec4 color = texelFetch(halfResColorTex, texel, 0);
	color *= tot;

	float max_radius = coc;
	for (int i = 0; i < NUM_SAMPLES; ++i) {
		vec2 tc = uv + samples[i].xy * invertedViewportSize * max_radius;

		vec4 samp = texture(halfResColorTex, tc);

		coc = decode_coc(texture(inputCocTex, tc).rg);

		float radius = samples[i].z * max_radius;
		coc *= smoothstep(radius - 0.5, radius + 0.5, coc);
		color += samp * coc;
		tot += coc;
	}

	blurColor = color / tot;
}
#endif
#endif

/**
 * ----------------- STEP 3 ------------------
 * Additional 3x3 blur
 **/
#ifdef BLUR2
out vec4 finalColor;

void main()
{
	/* Half Res pass */
	vec2 pixel_size = 1.0 / vec2(textureSize(blurTex, 0).xy);
	vec2 uv = gl_FragCoord.xy * pixel_size.xy; 
	float coc = decode_coc(texture(inputCocTex, uv).rg);
	/* Only use this filter if coc is > 9.0
	 * since this filter is not weighted by CoC
	 * and can bleed a bit. */
	float rad = clamp(coc - 9.0, 0.0, 1.0);
	rad *= 1.5; /* If not, it's a gaussian filter. */
	finalColor  = texture(blurTex, uv + pixel_size * vec2(-1.0, -1.0) * rad);
	finalColor += texture(blurTex, uv + pixel_size * vec2(-1.0,  0.0) * rad);
	finalColor += texture(blurTex, uv + pixel_size * vec2(-1.0,  1.0) * rad);
	finalColor += texture(blurTex, uv + pixel_size * vec2( 0.0, -1.0) * rad);
	finalColor += texture(blurTex, uv + pixel_size * vec2( 0.0,  0.0) * rad);
	finalColor += texture(blurTex, uv + pixel_size * vec2( 0.0,  1.0) * rad);
	finalColor += texture(blurTex, uv + pixel_size * vec2( 1.0, -1.0) * rad);
	finalColor += texture(blurTex, uv + pixel_size * vec2( 1.0,  0.0) * rad);
	finalColor += texture(blurTex, uv + pixel_size * vec2( 1.0,  1.0) * rad);
	finalColor *= 1.0 / 9.0;
}
#endif

/**
 * ----------------- STEP 4 ------------------
 **/
#ifdef RESOLVE
out vec4 finalColor;

void main()
{
	/* Fullscreen pass */
	vec2 pixel_size = 0.5 / vec2(textureSize(halfResColorTex, 0).xy);
	vec2 uv = gl_FragCoord.xy * pixel_size;

	/* TODO MAKE SURE TO ALIGN SAMPLE POSITION TO AVOID OFFSET IN THE BOKEH */
	float depth = texelFetch(sceneDepthTex, ivec2(gl_FragCoord.xy), 0).r;
	float zdepth = linear_depth(depth);
	float coc = calculate_coc(zdepth);

	finalColor = texture(halfResColorTex, uv);
	finalColor.a = smoothstep(1.0, 3.0, abs(coc));
}
#endif