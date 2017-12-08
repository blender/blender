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

#ifdef LAYERED
#define sampleLowerMip(t) texelFetch(depthBuffer, ivec3(t, depthLayer), 0).r
#else
#define sampleLowerMip(t) texelFetch(depthBuffer, t, 0).r
#endif

#ifdef MIN_PASS
#define minmax(a, b) min(a, b)
#else /* MAX_PASS */
#define minmax(a, b) max(a, b)
#endif

#ifdef GPU_INTEL
out vec4 fragColor;
#endif

void main()
{
	ivec2 texelPos = ivec2(gl_FragCoord.xy);
	ivec2 mipsize = textureSize(depthBuffer, 0).xy;

#ifndef COPY_DEPTH
	texelPos *= 2;
#endif

	float val = sampleLowerMip(texelPos);
#ifndef COPY_DEPTH
	float val2 = sampleLowerMip(texelPos + ivec2(1, 0));
	float val3 = sampleLowerMip(texelPos + ivec2(1, 1));
	float val4 = sampleLowerMip(texelPos + ivec2(0, 1));
	val = minmax(val, val2);
	val = minmax(val, val3);
	val = minmax(val, val4);

	/* if we are reducing an odd-width texture then fetch the edge texels */
	if (((mipsize.x & 1) != 0) && (texelPos.x == mipsize.x - 3)) {
		/* if both edges are odd, fetch the top-left corner texel */
		if (((mipsize.y & 1) != 0) && (texelPos.y == mipsize.y - 3)) {
			val = minmax(val, sampleLowerMip(texelPos + ivec2(2, 2)));
		}
		float val2 = sampleLowerMip(texelPos + ivec2(2, 0));
		float val3 = sampleLowerMip(texelPos + ivec2(2, 1));
		val = minmax(val, val2);
		val = minmax(val, val3);
	}
	/* if we are reducing an odd-height texture then fetch the edge texels */
	if (((mipsize.y & 1) != 0) && (texelPos.y == mipsize.y - 3)) {
		float val2 = sampleLowerMip(texelPos + ivec2(0, 2));
		float val3 = sampleLowerMip(texelPos + ivec2(1, 2));
		val = minmax(val, val2);
		val = minmax(val, val3);
	}
#endif

#ifdef GPU_INTEL
	/* Use color format instead of 24bit depth texture */
	fragColor = vec4(val);
#else
	gl_FragDepth = val;
#endif
}