/**
 * Shader that down-sample depth buffer,
 * saving min and max value of each texel in the above mipmaps.
 * Adapted from http://rastergrid.com/blog/2010/10/hierarchical-z-map-based-occlusion-culling/
 *
 * Major simplification has been made since we pad the buffer to always be bigger than input to
 * avoid mipmapping misalignement.
 */

#ifdef LAYERED
uniform sampler2DArray depthBuffer;
uniform int depthLayer;
#else
uniform sampler2D depthBuffer;
#endif

#ifndef COPY_DEPTH
uniform vec2 texelSize;
#endif

#ifdef LAYERED
#  define sampleLowerMip(t) texture(depthBuffer, vec3(t, depthLayer)).r
#  define gatherLowerMip(t) textureGather(depthBuffer, vec3(t, depthLayer))
#else
#  define sampleLowerMip(t) texture(depthBuffer, t).r
#  define gatherLowerMip(t) textureGather(depthBuffer, t)
#endif

#ifdef MIN_PASS
#  define minmax2(a, b) min(a, b)
#  define minmax3(a, b, c) min(min(a, b), c)
#  define minmax4(a, b, c, d) min(min(min(a, b), c), d)
#else /* MAX_PASS */
#  define minmax2(a, b) max(a, b)
#  define minmax3(a, b, c) max(max(a, b), c)
#  define minmax4(a, b, c, d) max(max(max(a, b), c), d)
#endif

/* On some AMD card / driver combination, it is needed otherwise,
 * the shader does not write anything. */
#if defined(GPU_INTEL) || defined(GPU_ATI)
out vec4 fragColor;
#endif

void main()
{
  vec2 texel = gl_FragCoord.xy;

#ifdef COPY_DEPTH
  vec2 uv = texel / vec2(textureSize(depthBuffer, 0).xy);

  float val = sampleLowerMip(uv);
#else
  /* NOTE(@fclem): textureSize() does not work the same on all implementations
   * when changing the min and max texture levels. Use uniform instead (see T87801). */
  vec2 uv = texel * 2.0 * texelSize;

  vec4 samp;
#  ifdef GPU_ARB_texture_gather
  samp = gatherLowerMip(uv);
#  else
  samp.x = sampleLowerMip(uv + vec2(-0.5, -0.5) * texelSize);
  samp.y = sampleLowerMip(uv + vec2(-0.5, 0.5) * texelSize);
  samp.z = sampleLowerMip(uv + vec2(0.5, -0.5) * texelSize);
  samp.w = sampleLowerMip(uv + vec2(0.5, 0.5) * texelSize);
#  endif

  float val = minmax4(samp.x, samp.y, samp.z, samp.w);
#endif

#if defined(GPU_INTEL) || defined(GPU_ATI)
  /* Use color format instead of 24bit depth texture */
  fragColor = vec4(val);
#endif
  gl_FragDepth = val;
}
