/**
 * Shader that downsample depth buffer,
 * saving min and max value of each texel in the above mipmaps.
 * Adapted from http://rastergrid.com/blog/2010/10/hierarchical-z-map-based-occlusion-culling/
 **/

uniform sampler2D depthBuffer;

out vec4 FragMinMax;

vec2 sampleLowerMip(ivec2 texel)
{
#ifdef INPUT_DEPTH
	return texelFetch(depthBuffer, texel, 0).rr;
#else
	return texelFetch(depthBuffer, texel, 0).rg;
#endif
}

void minmax(inout vec2 val[2])
{
	val[0].x = min(val[0].x, val[1].x);
	val[0].y = max(val[0].y, val[1].y);
}

void main()
{
	vec2 val[2];
	ivec2 texelPos = ivec2(gl_FragCoord.xy);
	ivec2 mipsize = textureSize(depthBuffer, 0);
#ifndef COPY_DEPTH
	texelPos *= 2;
#endif

	val[0] = sampleLowerMip(texelPos);
#ifndef COPY_DEPTH
	val[1] = sampleLowerMip(texelPos + ivec2(1, 0));
	minmax(val);
	val[1] = sampleLowerMip(texelPos + ivec2(1, 1));
	minmax(val);
	val[1] = sampleLowerMip(texelPos + ivec2(0, 1));
	minmax(val);

	/* if we are reducing an odd-width texture then fetch the edge texels */
	if (((mipsize.x & 1) != 0) && (int(gl_FragCoord.x) == mipsize.x-3)) {
		/* if both edges are odd, fetch the top-left corner texel */
		if (((mipsize.y & 1) != 0) && (int(gl_FragCoord.y) == mipsize.y-3)) {
			val[1] = sampleLowerMip(texelPos + ivec2(-1, -1));
			minmax(val);
		}
		val[1] = sampleLowerMip(texelPos + ivec2(0, -1));
		minmax(val);
		val[1] = sampleLowerMip(texelPos + ivec2(1, -1));
		minmax(val);
	}
	/* if we are reducing an odd-height texture then fetch the edge texels */
	else if (((mipsize.y & 1) != 0) && (int(gl_FragCoord.y) == mipsize.y-3)) {
		val[1] = sampleLowerMip(texelPos + ivec2(0, -1));
		minmax(val);
		val[1] = sampleLowerMip(texelPos + ivec2(1, -1));
		minmax(val);
	}
#endif

	FragMinMax = vec4(val[0], 0.0, 1.0);
}