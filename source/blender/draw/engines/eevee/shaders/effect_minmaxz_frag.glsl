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
#  define sampleLowerMip(t) texelFetch(depthBuffer, ivec3(t, depthLayer), 0).r
#  define gatherLowerMip(t) textureGather(depthBuffer, vec3(t, depthLayer))
#else
#  define sampleLowerMip(t) texelFetch(depthBuffer, t, 0).r
#  define gatherLowerMip(t) textureGather(depthBuffer, t)
#endif

#ifdef MIN_PASS
#define minmax2(a, b) min(a, b)
#define minmax3(a, b, c) min(min(a, b), c)
#define minmax4(a, b, c, d) min(min(min(a, b), c), d)
#else /* MAX_PASS */
#define minmax2(a, b) max(a, b)
#define minmax3(a, b, c) max(max(a, b), c)
#define minmax4(a, b, c, d) max(max(max(a, b), c), d)
#endif

/* On some AMD card / driver conbination, it is needed otherwise,
 * the shader does not write anything. */
#if defined(GPU_INTEL) || defined(GPU_ATI)
out vec4 fragColor;
#endif

void main()
{
	ivec2 texelPos = ivec2(gl_FragCoord.xy);
	ivec2 mipsize = textureSize(depthBuffer, 0).xy;

#ifndef COPY_DEPTH
	texelPos *= 2;
#endif

#ifdef COPY_DEPTH
	float val = sampleLowerMip(texelPos);
#else
	vec4 samp;
#  ifdef GL_ARB_texture_gather
	samp = gatherLowerMip(vec2(texelPos) / vec2(mipsize));
#  else
	samp.x = sampleLowerMip(texelPos);
	samp.y = sampleLowerMip(texelPos + ivec2(1, 0));
	samp.z = sampleLowerMip(texelPos + ivec2(1, 1));
	samp.w = sampleLowerMip(texelPos + ivec2(0, 1));
#  endif

	float val = minmax4(samp.x, samp.y, samp.z, samp.w);

	/* if we are reducing an odd-width texture then fetch the edge texels */
	if (((mipsize.x & 1) != 0) && (texelPos.x == mipsize.x - 3)) {
		/* if both edges are odd, fetch the top-left corner texel */
		if (((mipsize.y & 1) != 0) && (texelPos.y == mipsize.y - 3)) {
			samp.x = sampleLowerMip(texelPos + ivec2(2, 2));
			val = minmax2(val, samp.x);
		}
#  ifdef GL_ARB_texture_gather
		samp = gatherLowerMip((vec2(texelPos) + vec2(1.0, 0.0)) / vec2(mipsize));
#  else
		samp.y = sampleLowerMip(texelPos + ivec2(2, 0));
		samp.z = sampleLowerMip(texelPos + ivec2(2, 1));
#  endif
		val = minmax3(val, samp.y, samp.z);
	}
	/* if we are reducing an odd-height texture then fetch the edge texels */
	if (((mipsize.y & 1) != 0) && (texelPos.y == mipsize.y - 3)) {
#  ifdef GL_ARB_texture_gather
		samp = gatherLowerMip((vec2(texelPos) + vec2(0.0, 1.0)) / vec2(mipsize));
#  else
		samp.x = sampleLowerMip(texelPos + ivec2(0, 2));
		samp.y = sampleLowerMip(texelPos + ivec2(1, 2));
#  endif
		val = minmax3(val, samp.x, samp.y);
	}
#endif

#if defined(GPU_INTEL) || defined(GPU_ATI)
	/* Use color format instead of 24bit depth texture */
	fragColor = vec4(val);
#endif
	gl_FragDepth = val;
}
