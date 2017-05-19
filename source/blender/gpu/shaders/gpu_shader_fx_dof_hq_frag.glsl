/* amount of offset to move one pixel left-right.
 * In second pass some dimensions are zero to control verical/horizontal convolution */
uniform vec2 invrendertargetdim;

uniform ivec2 rendertargetdim;

/* color buffer */
uniform sampler2D colorbuffer;
uniform sampler2D farbuffer;
uniform sampler2D nearbuffer;

/* depth buffer */
uniform sampler2D depthbuffer;

uniform sampler2D cocbuffer;

/* this includes aperture size in x and focal distance in y */
uniform vec4 dof_params;

/* viewvectors for reconstruction of world space */
uniform vec4 viewvecs[3];

/* initial uv coordinate */
in vec2 uvcoord;

/* coordinate used for calculating radius et al set in geometry shader */
in vec2 particlecoord;
flat in vec4 color;

/* downsampling coordinates */
in vec2 downsample1;
in vec2 downsample2;
in vec2 downsample3;
in vec2 downsample4;

layout(location = 0) out vec4 fragData0;
layout(location = 1) out vec4 fragData1;
layout(location = 2) out vec4 fragData2;

#define M_PI 3.1415926535897932384626433832795

/* calculate 4 samples at once */
vec4 calculate_coc(in vec4 zdepth)
{
	vec4 coc = dof_params.x * (vec4(dof_params.y) / zdepth - vec4(1.0));

	/* multiply by 1.0 / sensor size to get the normalized size */
	return coc * dof_params.z;
}

#define THRESHOLD 0.0

/* downsample the color buffer to half resolution */
void downsample_pass()
{
	vec4 depth;
	vec4 zdepth;
	vec4 coc;
	float far_coc, near_coc;

	/* custom downsampling. We need to be careful to sample nearest here to avoid leaks */
	vec4 color1 = texture(colorbuffer, downsample1);
	vec4 color2 = texture(colorbuffer, downsample2);
	vec4 color3 = texture(colorbuffer, downsample3);
	vec4 color4 = texture(colorbuffer, downsample4);

	depth.r = texture(depthbuffer, downsample1).r;
	depth.g = texture(depthbuffer, downsample2).r;
	depth.b = texture(depthbuffer, downsample3).r;
	depth.a = texture(depthbuffer, downsample4).r;

	zdepth = get_view_space_z_from_depth(vec4(viewvecs[0].z), vec4(viewvecs[1].z), depth);
	coc = calculate_coc(zdepth);
	vec4 coc_far = -coc;

	/* now we need to write the near-far fields premultiplied by the coc */
	vec4 near_weights = vec4((coc.x >= THRESHOLD) ? 1.0 : 0.0, (coc.y >= THRESHOLD) ? 1.0 : 0.0,
	                         (coc.z >= THRESHOLD) ? 1.0 : 0.0, (coc.w >= THRESHOLD) ? 1.0 : 0.0);
	vec4 far_weights =  vec4((coc_far.x >= THRESHOLD) ? 1.0 : 0.0, (coc_far.y >= THRESHOLD) ? 1.0 : 0.0,
	                         (coc_far.z >= THRESHOLD) ? 1.0 : 0.0, (coc_far.w >= THRESHOLD) ? 1.0 : 0.0);

	near_coc = max(max(max(coc.x, coc.y), max(coc.z, coc.w)), 0.0);
	far_coc = max(max(max(coc_far.x, coc_far.y), max(coc_far.z, coc_far.w)), 0.0);

	float norm_near = dot(near_weights, vec4(1.0));
	float norm_far = dot(far_weights, vec4(1.0));

	/* now write output to weighted buffers. */
	fragData0 = color1 * near_weights.x + color2 * near_weights.y + color3 * near_weights.z +
	                 color4 * near_weights.w;
	fragData1 = color1 * far_weights.x + color2 * far_weights.y + color3 * far_weights.z +
	                 color4 * far_weights.w;

	if (norm_near > 0.0)
		fragData0 /= norm_near;
	if (norm_far > 0.0)
		fragData1 /= norm_far;
	fragData2 = vec4(near_coc, far_coc, 0.0, 1.0);
}

/* accumulate color in the near/far blur buffers */
void accumulate_pass(void) {
	float theta = atan(particlecoord.y, particlecoord.x);
	float r;

	if (dof_params.w == 0.0)
		r = 1.0;
	else
	 	r = cos(M_PI / dof_params.w) /
	 	    (cos(theta - (2.0 * M_PI / dof_params.w) * floor((dof_params.w * theta + M_PI) / (2.0 * M_PI))));

	if (dot(particlecoord, particlecoord) > r * r)
		discard;

	fragData0 = color;
}

#define MERGE_THRESHOLD 4.0
/* combine the passes, */
void final_pass(void) {
	vec4 finalcolor;
	float totalweight;
	float depth = texture(depthbuffer, uvcoord).r;

	vec4 zdepth = get_view_space_z_from_depth(vec4(viewvecs[0].z), vec4(viewvecs[1].z), vec4(depth));
	float coc_near = calculate_coc(zdepth).r;
	float coc_far = max(-coc_near, 0.0);
	coc_near = max(coc_near, 0.0);

	vec4 farcolor = texture(farbuffer, uvcoord);
	float farweight = farcolor.a;
	if (farweight > 0.0)
		farcolor /= farweight;
	vec4 nearcolor = texture(nearbuffer, uvcoord);

	vec4 srccolor = texture(colorbuffer, uvcoord);

	vec4 coc = texture(cocbuffer, uvcoord);

	float mixfac = smoothstep(1.0, MERGE_THRESHOLD, coc_far);
	finalcolor =  mix(srccolor, farcolor, mixfac);

	farweight = mix(1.0, farweight, mixfac);

	float nearweight = nearcolor.a;
	if (nearweight > 0.0) {
		nearcolor /= nearweight;
	}

	if (coc_near > 1.0) {
		nearweight = 1.0;
		finalcolor = nearcolor;
	}
	else {
		totalweight = nearweight + farweight;
		finalcolor = mix(finalcolor, nearcolor, nearweight / totalweight);
	}

	fragData0 = finalcolor;
	// fragData0 = vec4(nearweight, farweight, 0.0, 1.0);
	// fragData0 = vec4(nearcolor.rgb, 1.0);
}

void main()
{
#ifdef FIRST_PASS
	downsample_pass();
#elif defined(SECOND_PASS)
	accumulate_pass();
#elif defined(THIRD_PASS)
	final_pass();
#endif
}
