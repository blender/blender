/**
 * Separable Hexagonal Bokeh Blur by Colin Barré-Brisebois
 * https://colinbarrebrisebois.com/2017/04/18/hexagonal-bokeh-blur-revisited-part-1-basic-3-pass-version/
 * Converted and adapted from HLSL to GLSL by Clément Foucault
 */

uniform vec2 invertedViewportSize;
uniform vec2 nearFar;
uniform vec3 dofParams;
uniform float noiseOffset;
uniform sampler2D inputCocTex;
uniform sampler2D maxCocTilesTex;
uniform sampler2D sceneColorTex;
uniform sampler2D sceneDepthTex;
uniform sampler2D backgroundTex;
uniform sampler2D halfResColorTex;
uniform sampler2D blurTex;
uniform sampler2D noiseTex;

#define dof_aperturesize dofParams.x
#define dof_distance dofParams.y
#define dof_invsensorsize dofParams.z

#define M_PI 3.1415926535897932 /* pi */

float max_v4(vec4 v)
{
  return max(max(v.x, v.y), max(v.z, v.w));
}

#define weighted_sum(a, b, c, d, e, e_sum) \
  ((a)*e.x + (b)*e.y + (c)*e.z + (d)*e.w) / max(1e-6, e_sum);

/* divide by sensor size to get the normalized size */
#define calculate_coc(zdepth) \
  (dof_aperturesize * (dof_distance / zdepth - 1.0) * dof_invsensorsize)

#define linear_depth(z) \
  ((ProjectionMatrix[3][3] == 0.0) ? \
       (nearFar.x * nearFar.y) / (z * (nearFar.x - nearFar.y) + nearFar.y) : \
       (z * 2.0 - 1.0) * nearFar.y)

const float MAX_COC_SIZE = 100.0;
vec2 encode_coc(float near, float far)
{
  return vec2(near, far) / MAX_COC_SIZE;
}
float decode_coc(vec2 cocs)
{
  return max(cocs.x, cocs.y) * MAX_COC_SIZE;
}
float decode_signed_coc(vec2 cocs)
{
  return ((cocs.x > cocs.y) ? cocs.x : -cocs.y) * MAX_COC_SIZE;
}

/**
 * ----------------- STEP 0 ------------------
 * Custom Coc aware downsampling. Half res pass.
 */
#ifdef PREPARE

layout(location = 0) out vec4 halfResColor;
layout(location = 1) out vec2 normalizedCoc;

void main()
{
  ivec4 texel = ivec4(gl_FragCoord.xyxy) * 2 + ivec4(0, 0, 1, 1);

  vec4 color1 = texelFetch(sceneColorTex, texel.xy, 0);
  vec4 color2 = texelFetch(sceneColorTex, texel.zw, 0);
  vec4 color3 = texelFetch(sceneColorTex, texel.zy, 0);
  vec4 color4 = texelFetch(sceneColorTex, texel.xw, 0);

  vec4 depths;
  depths.x = texelFetch(sceneDepthTex, texel.xy, 0).x;
  depths.y = texelFetch(sceneDepthTex, texel.zw, 0).x;
  depths.z = texelFetch(sceneDepthTex, texel.zy, 0).x;
  depths.w = texelFetch(sceneDepthTex, texel.xw, 0).x;

  vec4 zdepths = linear_depth(depths);
  vec4 cocs_near = calculate_coc(zdepths);
  vec4 cocs_far = -cocs_near;

  float coc_near = max(max_v4(cocs_near), 0.0);
  float coc_far = max(max_v4(cocs_far), 0.0);

  /* now we need to write the near-far fields premultiplied by the coc
   * also use bilateral weighting by each coc values to avoid bleeding. */
  vec4 near_weights = step(0.0, cocs_near) * clamp(1.0 - abs(coc_near - cocs_near), 0.0, 1.0);
  vec4 far_weights = step(0.0, cocs_far) * clamp(1.0 - abs(coc_far - cocs_far), 0.0, 1.0);

  /* now write output to weighted buffers. */
  /* Take far plane pixels in priority. */
  vec4 w = any(notEqual(far_weights, vec4(0.0))) ? far_weights : near_weights;
  float tot_weight = dot(w, vec4(1.0));
  halfResColor = weighted_sum(color1, color2, color3, color4, w, tot_weight);
  halfResColor = clamp(halfResColor, 0.0, 3.0);

  normalizedCoc = encode_coc(coc_near, coc_far);
}
#endif

/**
 * ----------------- STEP 0.5 ------------------
 * Custom Coc aware downsampling. Quater res pass.
 */
#ifdef DOWNSAMPLE

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outCocs;

void main()
{
  ivec4 texel = ivec4(gl_FragCoord.xyxy) * 2 + ivec4(0, 0, 1, 1);

  vec4 color1 = texelFetch(sceneColorTex, texel.xy, 0);
  vec4 color2 = texelFetch(sceneColorTex, texel.zw, 0);
  vec4 color3 = texelFetch(sceneColorTex, texel.zy, 0);
  vec4 color4 = texelFetch(sceneColorTex, texel.xw, 0);

  vec4 depths;
  vec2 cocs1 = texelFetch(inputCocTex, texel.xy, 0).rg;
  vec2 cocs2 = texelFetch(inputCocTex, texel.zw, 0).rg;
  vec2 cocs3 = texelFetch(inputCocTex, texel.zy, 0).rg;
  vec2 cocs4 = texelFetch(inputCocTex, texel.xw, 0).rg;

  vec4 cocs_near = vec4(cocs1.r, cocs2.r, cocs3.r, cocs4.r) * MAX_COC_SIZE;
  vec4 cocs_far = vec4(cocs1.g, cocs2.g, cocs3.g, cocs4.g) * MAX_COC_SIZE;

  float coc_near = max_v4(cocs_near);
  float coc_far = max_v4(cocs_far);

  /* now we need to write the near-far fields premultiplied by the coc
   * also use bilateral weighting by each coc values to avoid bleeding. */
  vec4 near_weights = step(0.0, cocs_near) * clamp(1.0 - abs(coc_near - cocs_near), 0.0, 1.0);
  vec4 far_weights = step(0.0, cocs_far) * clamp(1.0 - abs(coc_far - cocs_far), 0.0, 1.0);

  /* now write output to weighted buffers. */
  vec4 w = any(notEqual(far_weights, vec4(0.0))) ? far_weights : near_weights;
  float tot_weight = dot(w, vec4(1.0));
  outColor = weighted_sum(color1, color2, color3, color4, w, tot_weight);

  outCocs = encode_coc(coc_near, coc_far);
}
#endif

/**
 * ----------------- STEP 1 ------------------
 * Flatten COC buffer using max filter.
 */
#if defined(FLATTEN_VERTICAL) || defined(FLATTEN_HORIZONTAL)

layout(location = 0) out vec2 flattenedCoc;

void main()
{
#  ifdef FLATTEN_HORIZONTAL
  ivec2 texel = ivec2(gl_FragCoord.xy) * ivec2(8, 1);
  vec2 cocs1 = texelFetchOffset(inputCocTex, texel, 0, ivec2(0, 0)).rg;
  vec2 cocs2 = texelFetchOffset(inputCocTex, texel, 0, ivec2(1, 0)).rg;
  vec2 cocs3 = texelFetchOffset(inputCocTex, texel, 0, ivec2(2, 0)).rg;
  vec2 cocs4 = texelFetchOffset(inputCocTex, texel, 0, ivec2(3, 0)).rg;
  vec2 cocs5 = texelFetchOffset(inputCocTex, texel, 0, ivec2(4, 0)).rg;
  vec2 cocs6 = texelFetchOffset(inputCocTex, texel, 0, ivec2(5, 0)).rg;
  vec2 cocs7 = texelFetchOffset(inputCocTex, texel, 0, ivec2(6, 0)).rg;
  vec2 cocs8 = texelFetchOffset(inputCocTex, texel, 0, ivec2(7, 0)).rg;
#  else /* FLATTEN_VERTICAL */
  ivec2 texel = ivec2(gl_FragCoord.xy) * ivec2(1, 8);
  vec2 cocs1 = texelFetchOffset(inputCocTex, texel, 0, ivec2(0, 0)).rg;
  vec2 cocs2 = texelFetchOffset(inputCocTex, texel, 0, ivec2(0, 1)).rg;
  vec2 cocs3 = texelFetchOffset(inputCocTex, texel, 0, ivec2(0, 2)).rg;
  vec2 cocs4 = texelFetchOffset(inputCocTex, texel, 0, ivec2(0, 3)).rg;
  vec2 cocs5 = texelFetchOffset(inputCocTex, texel, 0, ivec2(0, 4)).rg;
  vec2 cocs6 = texelFetchOffset(inputCocTex, texel, 0, ivec2(0, 5)).rg;
  vec2 cocs7 = texelFetchOffset(inputCocTex, texel, 0, ivec2(0, 6)).rg;
  vec2 cocs8 = texelFetchOffset(inputCocTex, texel, 0, ivec2(0, 7)).rg;
#  endif
  flattenedCoc = max(max(max(cocs1, cocs2), max(cocs3, cocs4)),
                     max(max(cocs5, cocs6), max(cocs7, cocs8)));
}
#endif

/**
 * ----------------- STEP 1.ax------------------
 * Dilate COC buffer using min filter.
 */
#if defined(DILATE_VERTICAL) || defined(DILATE_HORIZONTAL)

layout(location = 0) out vec2 dilatedCoc;

void main()
{
  vec2 texel_size = 1.0 / vec2(textureSize(inputCocTex, 0));
  vec2 uv = gl_FragCoord.xy * texel_size;
#  ifdef DILATE_VERTICAL
  vec2 cocs1 = texture(inputCocTex, uv + texel_size * vec2(-3, 0)).rg;
  vec2 cocs2 = texture(inputCocTex, uv + texel_size * vec2(-2, 0)).rg;
  vec2 cocs3 = texture(inputCocTex, uv + texel_size * vec2(-1, 0)).rg;
  vec2 cocs4 = texture(inputCocTex, uv + texel_size * vec2(0, 0)).rg;
  vec2 cocs5 = texture(inputCocTex, uv + texel_size * vec2(1, 0)).rg;
  vec2 cocs6 = texture(inputCocTex, uv + texel_size * vec2(2, 0)).rg;
  vec2 cocs7 = texture(inputCocTex, uv + texel_size * vec2(3, 0)).rg;
#  else /* DILATE_HORIZONTAL */
  vec2 cocs1 = texture(inputCocTex, uv + texel_size * vec2(0, -3)).rg;
  vec2 cocs2 = texture(inputCocTex, uv + texel_size * vec2(0, -2)).rg;
  vec2 cocs3 = texture(inputCocTex, uv + texel_size * vec2(0, -1)).rg;
  vec2 cocs4 = texture(inputCocTex, uv + texel_size * vec2(0, 0)).rg;
  vec2 cocs5 = texture(inputCocTex, uv + texel_size * vec2(0, 1)).rg;
  vec2 cocs6 = texture(inputCocTex, uv + texel_size * vec2(0, 2)).rg;
  vec2 cocs7 = texture(inputCocTex, uv + texel_size * vec2(0, 3)).rg;
#  endif
  // dilatedCoc = max(max(cocs3, cocs4), max(max(cocs5, cocs6), cocs2));
  dilatedCoc = max(max(max(cocs1, cocs2), max(cocs3, cocs4)), max(max(cocs5, cocs6), cocs7));
}
#endif

/**
 * ----------------- STEP 2 ------------------
 * Blur vertically and diagonally.
 * Outputs vertical blur and combined blur in MRT
 */
#ifdef BLUR1
layout(location = 0) out vec4 blurColor;

#  define NUM_SAMPLES 49

layout(std140) uniform dofSamplesBlock
{
  vec4 samples[NUM_SAMPLES];
};

vec2 get_random_vector(float offset)
{
  /* Interlieved gradient noise by Jorge Jimenez
   * http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare */
  float ign = fract(offset +
                    52.9829189 * fract(0.06711056 * gl_FragCoord.x + 0.00583715 * gl_FragCoord.y));
  float bn = texelFetch(noiseTex, ivec2(gl_FragCoord.xy) % 64, 0).a;
  float ang = M_PI * 2.0 * fract(bn + offset);
  return vec2(cos(ang), sin(ang)) * sqrt(ign);
  // return noise.rg * sqrt(ign);
}

void main()
{
  vec2 uv = gl_FragCoord.xy * invertedViewportSize * 2.0;

  vec2 size = vec2(textureSize(halfResColorTex, 0).xy);
  ivec2 texel = ivec2(uv * size);

  vec4 color = vec4(0.0);
  float tot = 0.0;

  float coc = decode_coc(texelFetch(inputCocTex, texel, 0).rg);
  float max_radius = coc;
  vec2 noise = get_random_vector(noiseOffset) * 0.2 * clamp(max_radius * 0.2 - 4.0, 0.0, 1.0);
  for (int i = 0; i < NUM_SAMPLES; i++) {
    vec2 tc = uv + (noise + samples[i].xy) * invertedViewportSize * max_radius;

    /* decode_signed_coc return biggest coc. */
    coc = abs(decode_signed_coc(texture(inputCocTex, tc).rg));

    float lod = log2(clamp((coc + min(coc, max_radius)) * 0.5 - 21.0, 0.0, 16.0) * 0.25);
    vec4 samp = textureLod(halfResColorTex, tc, lod);

    float radius = samples[i].z * max_radius;
    float weight = abs(coc) * smoothstep(radius - 0.5, radius + 0.5, abs(coc));

    color += samp * weight;
    tot += weight;
  }

  if (tot > 0.0) {
    blurColor = color / tot;
  }
  else {
    blurColor = textureLod(halfResColorTex, uv, 0.0);
  }
}
#endif

/**
 * ----------------- STEP 3 ------------------
 * 3x3 Median Filter
 * Morgan McGuire and Kyle Whitson
 * http://graphics.cs.williams.edu
 *
 *
 * Copyright (c) Morgan McGuire and Williams College, 2006
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
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

#  define vec vec4
#  define toVec(x) x.rgba

#  define s2(a, b) \
    temp = a; \
    a = min(a, b); \
    b = max(temp, b);
#  define mn3(a, b, c) \
    s2(a, b); \
    s2(a, c);
#  define mx3(a, b, c) \
    s2(b, c); \
    s2(a, c);

#  define mnmx3(a, b, c) \
    mx3(a, b, c); \
    s2(a, b);  // 3 exchanges
#  define mnmx4(a, b, c, d) \
    s2(a, b); \
    s2(c, d); \
    s2(a, c); \
    s2(b, d);  // 4 exchanges
#  define mnmx5(a, b, c, d, e) \
    s2(a, b); \
    s2(c, d); \
    mn3(a, c, e); \
    mx3(b, d, e);  // 6 exchanges
#  define mnmx6(a, b, c, d, e, f) \
    s2(a, d); \
    s2(b, e); \
    s2(c, f); \
    mn3(a, b, c); \
    mx3(d, e, f);  // 7 exchanges

  vec v[9];

  /* Add the pixels which make up our window to the pixel array. */
  for (int dX = -1; dX <= 1; dX++) {
    for (int dY = -1; dY <= 1; dY++) {
      vec2 offset = vec2(float(dX), float(dY));
      /* If a pixel in the window is located at (x+dX, y+dY), put it at index (dX + R)(2R + 1) +
       * (dY + R) of the pixel array. This will fill the pixel array, with the top left pixel of
       * the window at pixel[0] and the bottom right pixel of the window at pixel[N-1]. */
      v[(dX + 1) * 3 + (dY + 1)] = toVec(texture(blurTex, uv + offset * pixel_size * rad));
    }
  }

  vec temp;

  /* Starting with a subset of size 6, remove the min and max each time */
  mnmx6(v[0], v[1], v[2], v[3], v[4], v[5]);
  mnmx5(v[1], v[2], v[3], v[4], v[6]);
  mnmx4(v[2], v[3], v[4], v[7]);
  mnmx3(v[3], v[4], v[8]);
  toVec(finalColor) = v[4];
}

#endif

/**
 * ----------------- STEP 4 ------------------
 */
#ifdef RESOLVE

layout(location = 0) out vec4 finalColorAdd;
layout(location = 1) out vec4 finalColorMul;

void main()
{
  /* Fullscreen pass */
  vec2 pixel_size = 0.5 / vec2(textureSize(halfResColorTex, 0).xy);
  vec2 uv = gl_FragCoord.xy * pixel_size;

  /* TODO MAKE SURE TO ALIGN SAMPLE POSITION TO AVOID OFFSET IN THE BOKEH */
  float depth = texelFetch(sceneDepthTex, ivec2(gl_FragCoord.xy), 0).r;
  float zdepth = linear_depth(depth);
  float coc = calculate_coc(zdepth);

  float blend = smoothstep(1.0, 3.0, abs(coc));
  finalColorAdd = texture(halfResColorTex, uv) * blend;
  finalColorMul = vec4(1.0 - blend);
}
#endif
