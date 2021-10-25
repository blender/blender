/* amount of offset to move one pixel left-right.
 * In second pass some dimensions are zero to control verical/horizontal convolution */
uniform vec2 invrendertargetdim;
// color buffer
uniform sampler2D colorbuffer;
//blurred color buffer for DOF effect
uniform sampler2D blurredcolorbuffer;
// slightly blurred buffer
uniform sampler2D mblurredcolorbuffer;
// depth buffer
uniform sampler2D depthbuffer;

// this includes focal distance in x and aperture size in y
uniform vec4 dof_params;

// viewvectors for reconstruction of world space
uniform vec4 viewvecs[3];

// coordinates on framebuffer in normalized (0.0-1.0) uv space
varying vec4 uvcoordsvar;

/* color texture coordinates, offset by a small amount */
varying vec2 color_uv1;
varying vec2 color_uv2;

varying vec2 depth_uv1;
varying vec2 depth_uv2;
varying vec2 depth_uv3;
varying vec2 depth_uv4;


float calculate_far_coc(in float zdepth)
{
	float coc = dof_params.x * max(1.0 - dof_params.y / zdepth, 0.0);

	/* multiply by 1.0 / sensor size to get the normalized size */
	return coc * dof_params.z;
}

/* near coc only! when distance is nearer than focus plane first term is bigger than one */
vec4 calculate_near_coc(in vec4 zdepth)
{
	vec4 coc = dof_params.x * max(vec4(dof_params.y) / zdepth - vec4(1.0), vec4(0.0));

	/* multiply by 1.0 / sensor size to get the normalized size */
	return coc * dof_params.z;
}

/* first pass blurs the color buffer heavily and gets the near coc only.
 * There are many texture accesses here but they are done on a
 * lower resolution image so overall bandwidth is not a concern */
void first_pass()
{
	vec4 depth;
	vec4 zdepth;
	vec4 coc;
	float final_coc;

	/* amount to add to uvs so that they move one row further */
	vec2 offset_row[3];
	offset_row[0] = vec2(0.0, invrendertargetdim.y);
	offset_row[1] = 2.0 * offset_row[0];
	offset_row[2] = 3.0 * offset_row[0];

	/* heavily blur the image */
	vec4 color = texture2D(colorbuffer, color_uv1);
	color += texture2D(colorbuffer, color_uv1 + offset_row[1]);
	color += texture2D(colorbuffer, color_uv2);
	color += texture2D(colorbuffer, color_uv2 + offset_row[1]);
	color /= 4.0;

	depth.r = texture2D(depthbuffer, depth_uv1).r;
	depth.g = texture2D(depthbuffer, depth_uv2).r;
	depth.b = texture2D(depthbuffer, depth_uv3).r;
	depth.a = texture2D(depthbuffer, depth_uv4).r;

	zdepth = get_view_space_z_from_depth(vec4(viewvecs[0].z), vec4(viewvecs[1].z), depth);
	coc = calculate_near_coc(zdepth);

	depth.r = texture2D(depthbuffer, depth_uv1 + offset_row[0]).r;
	depth.g = texture2D(depthbuffer, depth_uv2 + offset_row[0]).r;
	depth.b = texture2D(depthbuffer, depth_uv3 + offset_row[0]).r;
	depth.a = texture2D(depthbuffer, depth_uv4 + offset_row[0]).r;

	zdepth = get_view_space_z_from_depth(vec4(viewvecs[0].z), vec4(viewvecs[1].z), depth);
	coc = max(calculate_near_coc(zdepth), coc);

	depth.r = texture2D(depthbuffer, depth_uv1 + offset_row[1]).r;
	depth.g = texture2D(depthbuffer, depth_uv2 + offset_row[1]).r;
	depth.b = texture2D(depthbuffer, depth_uv3 + offset_row[1]).r;
	depth.a = texture2D(depthbuffer, depth_uv4 + offset_row[1]).r;

	zdepth = get_view_space_z_from_depth(vec4(viewvecs[0].z), vec4(viewvecs[1].z), depth);
	coc = max(calculate_near_coc(zdepth), coc);

	depth.r = texture2D(depthbuffer, depth_uv1 + offset_row[2]).r;
	depth.g = texture2D(depthbuffer, depth_uv2 + offset_row[2]).r;
	depth.b = texture2D(depthbuffer, depth_uv3 + offset_row[2]).r;
	depth.a = texture2D(depthbuffer, depth_uv4 + offset_row[2]).r;

	zdepth = get_view_space_z_from_depth(vec4(viewvecs[0].z), vec4(viewvecs[1].z), depth);
	coc = max(calculate_near_coc(zdepth), coc);

	final_coc = max(max(coc.x, coc.y), max(coc.z, coc.w));
	gl_FragColor = vec4(color.rgb, final_coc);
}

/* second pass, gaussian blur the downsampled image */
void second_pass()
{
	vec4 depth = vec4(texture2D(depthbuffer, uvcoordsvar.xy).r);

	/* clever sampling to sample 2 pixels at once. Of course it's not real gaussian sampling this way */
	vec4 color =  texture2D(colorbuffer, uvcoordsvar.xy) * 0.3125;
	color += texture2D(colorbuffer, uvcoordsvar.xy + invrendertargetdim) * 0.234375;
	color += texture2D(colorbuffer, uvcoordsvar.xy + 2.5 * invrendertargetdim) * 0.09375;
	color += texture2D(colorbuffer, uvcoordsvar.xy + 4.5 * invrendertargetdim) * 0.015625;
	color += texture2D(colorbuffer, uvcoordsvar.xy - invrendertargetdim) * 0.234375;
	color += texture2D(colorbuffer, uvcoordsvar.xy - 2.5 * invrendertargetdim) * 0.09375;
	color += texture2D(colorbuffer, uvcoordsvar.xy - 4.5 * invrendertargetdim) * 0.015625;

	gl_FragColor = color;
}


/* third pass, calculate the final coc from blurred and unblurred images */
void third_pass()
{
	vec4 color =  texture2D(colorbuffer, uvcoordsvar.xy);
	vec4 color_blurred =  texture2D(blurredcolorbuffer, uvcoordsvar.xy);
	float coc = 2.0 * max(color_blurred.a, color.a); -color.a;
	gl_FragColor = vec4(color.rgb, coc);
}


/* fourth pass, blur the final coc once to get rid of discontinuities */
void fourth_pass()
{
	vec4 color = texture2D(colorbuffer, uvcoordsvar.xz);
	color += texture2D(colorbuffer, uvcoordsvar.yz);
	color += texture2D(colorbuffer, uvcoordsvar.xw);
	color += texture2D(colorbuffer, uvcoordsvar.yw);

	gl_FragColor = color / 4.0;
}

vec4 small_sample_blur(in sampler2D colorbuffer, in vec2 uv, in vec4 color)
{
	float weight = 1.0 / 17.0;
	vec4 result = weight * color;
	weight *= 4.0;

	result += weight * texture2D(colorbuffer, uv + color_uv1.xy);
	result += weight * texture2D(colorbuffer, uv - color_uv1.xy);
	result += weight * texture2D(colorbuffer, uv + color_uv1.yx);
	result += weight * texture2D(colorbuffer, uv - color_uv1.yx);

	return result;
}


/* fourth pass, just visualize the third pass contents */
void fifth_pass()
{
	vec4 factors;
	vec4 color_orig = texture2D(colorbuffer, uvcoordsvar.xy);
	vec4 highblurred = texture2D(blurredcolorbuffer, uvcoordsvar.xy);
	vec4 mediumblurred = texture2D(mblurredcolorbuffer, uvcoordsvar.xy);
	vec4 smallblurred = small_sample_blur(colorbuffer, uvcoordsvar.xy, color_orig);
	float depth = texture2D(depthbuffer, uvcoordsvar.xy).r;

	float zdepth = get_view_space_z_from_depth(vec4(viewvecs[0].z), vec4(viewvecs[1].z), vec4(depth)).r;
	float coc_far = clamp(calculate_far_coc(zdepth), 0.0, 1.0);

	/* calculate final coc here */
	float coc = max(max(coc_far, mediumblurred.a), 0.0);

	float width = 2.5;
	float radius = 0.2;

	factors.x = 1.0 - clamp(width * coc, 0.0, 1.0);
	factors.y = 1.0 - clamp(abs(width * (coc - 2.0 * radius)), 0.0, 1.0);
	factors.z = 1.0 - clamp(abs(width * (coc - 3.0 * radius)), 0.0, 1.0);
	factors.w = 1.0 - clamp(abs(width * (coc - 4.0 * radius)), 0.0, 1.0);
	/* blend! */
	vec4 color = factors.x * color_orig + factors.y * smallblurred + factors.z * mediumblurred + factors.w * highblurred;

	color /= dot(factors, vec4(1.0));
	/* using original color is not correct, but use that for now because alpha of
	 * blurred buffers uses CoC instead */
	gl_FragColor = vec4(color.rgb, color_orig.a);
}


void main()
{
#ifdef FIRST_PASS
	first_pass();
#elif defined(SECOND_PASS)
	second_pass();
#elif defined(THIRD_PASS)
	third_pass();
#elif defined(FOURTH_PASS)
	fourth_pass();
#elif defined(FIFTH_PASS)
	fifth_pass();
#endif
}
