uniform mat4 ProjectionMatrix;
uniform mat4 ViewMatrix;

uniform sampler2D strokeColor;
uniform sampler2D strokeDepth;

uniform int blur[2];

uniform vec3 loc;
uniform float pixsize;   /* rv3d->pixsize */
uniform float pixelsize; /* U.pixelsize */
uniform float pixfactor;

float defaultpixsize = pixsize * pixelsize * (1000.0 / pixfactor);
vec2 noffset = vec2(blur[0], blur[1]);

out vec4 FragColor;

void main()
{
	ivec2 uv = ivec2(gl_FragCoord.xy);

	vec4 nloc = ProjectionMatrix * ViewMatrix * vec4(loc.xyz, 1.0);
	
	float dx = (ProjectionMatrix[3][3] == 0.0) ? (noffset[0] / (nloc.z * defaultpixsize)) : (noffset[0] / defaultpixsize);
	float dy = (ProjectionMatrix[3][3] == 0.0) ? (noffset[1] / (nloc.z * defaultpixsize)) : (noffset[1] / defaultpixsize);
	
	/* apply blurring, using a 9-tap filter with predefined gaussian weights */
	/* depth */
	float outdepth = 0;
    outdepth += texelFetch(strokeDepth, ivec2(uv.x - 1.0 * dx, uv.y + 1.0 * dy), 0).r * 0.0947416;
    outdepth += texelFetch(strokeDepth, ivec2(uv.x - 0.0 * dx, uv.y + 1.0 * dy), 0).r * 0.118318;
    outdepth += texelFetch(strokeDepth, ivec2(uv.x + 1.0 * dx, uv.y + 1.0 * dy), 0).r * 0.0947416;
    outdepth += texelFetch(strokeDepth, ivec2(uv.x - 1.0 * dx, uv.y + 0.0 * dy), 0).r * 0.118318;

    outdepth += texelFetch(strokeDepth, ivec2(uv.x, uv.y), 0).r * 0.147761;

    outdepth += texelFetch(strokeDepth, ivec2(uv.x + 1.0 * dx, uv.y + 0.0 * dy), 0).r * 0.118318;
    outdepth += texelFetch(strokeDepth, ivec2(uv.x - 1.0 * dx, uv.y - 1.0 * dy), 0).r * 0.0947416;
    outdepth += texelFetch(strokeDepth, ivec2(uv.x + 0.0 * dx, uv.y - 1.0 * dy), 0).r * 0.118318;
    outdepth += texelFetch(strokeDepth, ivec2(uv.x + 1.0 * dx, uv.y - 1.0 * dy), 0).r * 0.0947416;

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
}
