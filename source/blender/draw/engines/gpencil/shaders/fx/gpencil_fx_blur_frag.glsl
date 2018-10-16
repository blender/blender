uniform mat4 ProjectionMatrix;
uniform mat4 ViewMatrix;

uniform sampler2D strokeColor;
uniform sampler2D strokeDepth;
uniform vec2 Viewport;

uniform int blur[2];

uniform vec3 loc;
uniform float pixsize;   /* rv3d->pixsize */
uniform float pixfactor;

float defaultpixsize = pixsize * (1000.0 / pixfactor);
vec2 noffset = vec2(blur[0], blur[1]);

out vec4 FragColor;

float get_zdepth(ivec2 poxy)
{
	/* if outside viewport set as infinite depth */
	if ((poxy.x < 0) || (poxy.x > Viewport.x)) {
		return 1.0f;
	}
	if ((poxy.y < 0) || (poxy.y > Viewport.y)) {
		return 1.0f;
	}

	float zdepth = texelFetch(strokeDepth, poxy, 0).r;
	return zdepth;
}

void main()
{
	ivec2 uv = ivec2(gl_FragCoord.xy);

	vec4 nloc = ProjectionMatrix * ViewMatrix * vec4(loc.xyz, 1.0);

	float dx = (ProjectionMatrix[3][3] == 0.0) ? (noffset[0] / (nloc.z * defaultpixsize)) : (noffset[0] / defaultpixsize);
	float dy = (ProjectionMatrix[3][3] == 0.0) ? (noffset[1] / (nloc.z * defaultpixsize)) : (noffset[1] / defaultpixsize);

	/* round to avoid shift when add more samples */
	dx = floor(dx) + 1.0;
	dy = floor(dy) + 1.0;
	
	/* apply blurring, using a 9-tap filter with predefined gaussian weights */
	/* depth (get the value of the surrounding pixels) */
    float outdepth = 0.0;
	
    outdepth += get_zdepth(ivec2(uv.x - 1.0 * dx, uv.y + 1.0 * dy)) * 0.0947416;
    outdepth += get_zdepth(ivec2(uv.x - 0.0 * dx, uv.y + 1.0 * dy)) * 0.118318;
    outdepth += get_zdepth(ivec2(uv.x + 1.0 * dx, uv.y + 1.0 * dy)) * 0.0947416;
    outdepth += get_zdepth(ivec2(uv.x - 1.0 * dx, uv.y + 0.0 * dy)) * 0.118318;

    outdepth += get_zdepth(ivec2(uv.x, uv.y)) * 0.147761;

    outdepth += get_zdepth(ivec2(uv.x + 1.0 * dx, uv.y + 0.0 * dy)) * 0.118318;
    outdepth += get_zdepth(ivec2(uv.x - 1.0 * dx, uv.y - 1.0 * dy)) * 0.0947416;
    outdepth += get_zdepth(ivec2(uv.x + 0.0 * dx, uv.y - 1.0 * dy)) * 0.118318;
    outdepth += get_zdepth(ivec2(uv.x + 1.0 * dx, uv.y - 1.0 * dy)) * 0.0947416;

    gl_FragDepth = outdepth;

	/* color */
	vec4 outcolor = vec4(0.0);
    outcolor += texelFetch(strokeColor, ivec2(uv.x - 1.0 * dx, uv.y + 1.0 * dy), 0) * 0.0947416;
    outcolor += texelFetch(strokeColor, ivec2(uv.x - 0.0 * dx, uv.y + 1.0 * dy), 0) * 0.118318;
    outcolor += texelFetch(strokeColor, ivec2(uv.x + 1.0 * dx, uv.y + 1.0 * dy), 0) * 0.0947416;
    outcolor += texelFetch(strokeColor, ivec2(uv.x - 1.0 * dx, uv.y + 0.0 * dy), 0) * 0.118318;

    outcolor += texelFetch(strokeColor, ivec2(uv.x, uv.y), 0) * 0.147761;

    outcolor += texelFetch(strokeColor, ivec2(uv.x + 1.0 * dx, uv.y + 0.0 * dy), 0) * 0.118318;
    outcolor += texelFetch(strokeColor, ivec2(uv.x - 1.0 * dx, uv.y - 1.0 * dy), 0) * 0.0947416;
    outcolor += texelFetch(strokeColor, ivec2(uv.x + 0.0 * dx, uv.y - 1.0 * dy), 0) * 0.118318;
    outcolor += texelFetch(strokeColor, ivec2(uv.x + 1.0 * dx, uv.y - 1.0 * dy), 0) * 0.0947416;

	FragColor = clamp(outcolor, 0, 1.0);
	
	/* discar extreme values */
	if (outcolor.a < 0.02f) {
		discard;
	}
	if ((outdepth <= 0.000001) || (outdepth  >= 0.999999)){
		discard;
	}
}
