/**
 * Shader that downsample depth buffer,
 * saving min and max value of each texel in the above mipmaps.
 * Adapted from http://rastergrid.com/blog/2010/10/hierarchical-z-map-based-occlusion-culling/
 **/

#ifdef LAYERED
uniform sampler2DArray depthBuffer;
uniform int depthLayer;
#else
uniform sampler2D depthBuffer;
#endif

float sampleLowerMip(ivec2 texel)
{
#ifdef LAYERED
	return texelFetch(depthBuffer, ivec3(texel, depthLayer), 0).r;
#else
	return texelFetch(depthBuffer, texel, 0).r;
#endif
}

void minmax(inout float out_val, float in_val)
{
#ifdef MIN_PASS
	out_val = min(out_val, in_val);
#else /* MAX_PASS */
	out_val = max(out_val, in_val);
#endif
}

void main()
{
	ivec2 texelPos = ivec2(gl_FragCoord.xy);
	ivec2 mipsize = textureSize(depthBuffer, 0).xy;

#ifndef COPY_DEPTH
	texelPos *= 2;
#endif

	float val = sampleLowerMip(texelPos);
#ifndef COPY_DEPTH
	minmax(val, sampleLowerMip(texelPos + ivec2(1, 0)));
	minmax(val, sampleLowerMip(texelPos + ivec2(1, 1)));
	minmax(val, sampleLowerMip(texelPos + ivec2(0, 1)));

	/* if we are reducing an odd-width texture then fetch the edge texels */
	if (((mipsize.x & 1) != 0) && (int(gl_FragCoord.x) == mipsize.x-3)) {
		/* if both edges are odd, fetch the top-left corner texel */
		if (((mipsize.y & 1) != 0) && (int(gl_FragCoord.y) == mipsize.y-3)) {
			minmax(val, sampleLowerMip(texelPos + ivec2(-1, -1)));
		}
		minmax(val, sampleLowerMip(texelPos + ivec2(0, -1)));
		minmax(val, sampleLowerMip(texelPos + ivec2(1, -1)));
	}
	/* if we are reducing an odd-height texture then fetch the edge texels */
	else if (((mipsize.y & 1) != 0) && (int(gl_FragCoord.y) == mipsize.y-3)) {
		minmax(val, sampleLowerMip(texelPos + ivec2(0, -1)));
		minmax(val, sampleLowerMip(texelPos + ivec2(1, -1)));
	}
#endif

	gl_FragDepth = val;
}