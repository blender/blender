/* Original implementation by Keijiro Takahashi
 * Blender integration by Clément Foucault
 *
 * Original License :
 *
 * Kino/Bloom v2 - Bloom filter for Unity
 *
 * Copyright (C) 2015, 2016 Keijiro Takahashi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

uniform sampler2D sourceBuffer; /* Buffer to filter */
uniform vec2 sourceBufferTexelSize;

/* Step Blit */
uniform vec4 curveThreshold;
uniform float clampIntensity;

/* Step Upsample */
uniform sampler2D baseBuffer; /* Previous accumulation buffer */
uniform vec2 baseBufferTexelSize;
uniform float sampleScale;

/* Step Resolve */
uniform vec3 bloomColor;
uniform bool bloomAddBase;

in vec4 uvcoordsvar;

out vec4 FragColor;

/* -------------- Utils ------------- */

vec3 safe_color(vec3 c)
{
  /* Clamp to avoid black square artifacts if a pixel goes NaN. */
  return clamp(c, vec3(0.0), vec3(1e20)); /* 1e20 arbitrary. */
}

float brightness(vec3 c)
{
  return max(max(c.r, c.g), c.b);
}

/* 3-tap median filter */
vec3 median(vec3 a, vec3 b, vec3 c)
{
  return a + b + c - min(min(a, b), c) - max(max(a, b), c);
}

/* ------------- Filters ------------ */

vec3 downsample_filter_high(sampler2D tex, vec2 uv, vec2 texelSize)
{
  /* Downsample with a 4x4 box filter + anti-flicker filter */
  vec4 d = texelSize.xyxy * vec4(-1, -1, +1, +1);

  vec3 s1 = textureLod(tex, uv + d.xy, 0.0).rgb;
  vec3 s2 = textureLod(tex, uv + d.zy, 0.0).rgb;
  vec3 s3 = textureLod(tex, uv + d.xw, 0.0).rgb;
  vec3 s4 = textureLod(tex, uv + d.zw, 0.0).rgb;

  /* Karis's luma weighted average (using brightness instead of luma) */
  float s1w = 1.0 / (brightness(s1) + 1.0);
  float s2w = 1.0 / (brightness(s2) + 1.0);
  float s3w = 1.0 / (brightness(s3) + 1.0);
  float s4w = 1.0 / (brightness(s4) + 1.0);
  float one_div_wsum = 1.0 / (s1w + s2w + s3w + s4w);

  return (s1 * s1w + s2 * s2w + s3 * s3w + s4 * s4w) * one_div_wsum;
}

vec3 downsample_filter(sampler2D tex, vec2 uv, vec2 texelSize)
{
  /* Downsample with a 4x4 box filter */
  vec4 d = texelSize.xyxy * vec4(-1, -1, +1, +1);

  vec3 s;
  s = textureLod(tex, uv + d.xy, 0.0).rgb;
  s += textureLod(tex, uv + d.zy, 0.0).rgb;
  s += textureLod(tex, uv + d.xw, 0.0).rgb;
  s += textureLod(tex, uv + d.zw, 0.0).rgb;

  return s * (1.0 / 4);
}

vec3 upsample_filter_high(sampler2D tex, vec2 uv, vec2 texelSize)
{
  /* 9-tap bilinear upsampler (tent filter) */
  vec4 d = texelSize.xyxy * vec4(1, 1, -1, 0) * sampleScale;

  vec3 s;
  s = textureLod(tex, uv - d.xy, 0.0).rgb;
  s += textureLod(tex, uv - d.wy, 0.0).rgb * 2;
  s += textureLod(tex, uv - d.zy, 0.0).rgb;

  s += textureLod(tex, uv + d.zw, 0.0).rgb * 2;
  s += textureLod(tex, uv, 0.0).rgb * 4;
  s += textureLod(tex, uv + d.xw, 0.0).rgb * 2;

  s += textureLod(tex, uv + d.zy, 0.0).rgb;
  s += textureLod(tex, uv + d.wy, 0.0).rgb * 2;
  s += textureLod(tex, uv + d.xy, 0.0).rgb;

  return s * (1.0 / 16.0);
}

vec3 upsample_filter(sampler2D tex, vec2 uv, vec2 texelSize)
{
  /* 4-tap bilinear upsampler */
  vec4 d = texelSize.xyxy * vec4(-1, -1, +1, +1) * (sampleScale * 0.5);

  vec3 s;
  s = textureLod(tex, uv + d.xy, 0.0).rgb;
  s += textureLod(tex, uv + d.zy, 0.0).rgb;
  s += textureLod(tex, uv + d.xw, 0.0).rgb;
  s += textureLod(tex, uv + d.zw, 0.0).rgb;

  return s * (1.0 / 4.0);
}

/* ----------- Steps ----------- */

vec4 step_blit(void)
{
  vec2 uv = uvcoordsvar.xy + sourceBufferTexelSize.xy * 0.5;

#ifdef HIGH_QUALITY /* Anti flicker */
  vec3 d = sourceBufferTexelSize.xyx * vec3(1, 1, 0);
  vec3 s0 = safe_color(textureLod(sourceBuffer, uvcoordsvar.xy, 0.0).rgb);
  vec3 s1 = safe_color(textureLod(sourceBuffer, uvcoordsvar.xy - d.xz, 0.0).rgb);
  vec3 s2 = safe_color(textureLod(sourceBuffer, uvcoordsvar.xy + d.xz, 0.0).rgb);
  vec3 s3 = safe_color(textureLod(sourceBuffer, uvcoordsvar.xy - d.zy, 0.0).rgb);
  vec3 s4 = safe_color(textureLod(sourceBuffer, uvcoordsvar.xy + d.zy, 0.0).rgb);
  vec3 m = median(median(s0.rgb, s1, s2), s3, s4);
#else
  vec3 s0 = safe_color(textureLod(sourceBuffer, uvcoordsvar.xy, 0.0).rgb);
  vec3 m = s0.rgb;
#endif

  /* Pixel brightness */
  float br = brightness(m);

  /* Under-threshold part: quadratic curve */
  float rq = clamp(br - curveThreshold.x, 0, curveThreshold.y);
  rq = curveThreshold.z * rq * rq;

  /* Combine and apply the brightness response curve. */
  m *= max(rq, br - curveThreshold.w) / max(1e-5, br);

  /* Clamp pixel intensity if clamping enabled */
  if (clampIntensity > 0.0) {
    br = max(1e-5, brightness(m));
    m *= 1.0 - max(0.0, br - clampIntensity) / br;
  }

  return vec4(m, 1.0);
}

vec4 step_downsample(void)
{
#ifdef HIGH_QUALITY /* Anti flicker */
  vec3 sample = downsample_filter_high(sourceBuffer, uvcoordsvar.xy, sourceBufferTexelSize);
#else
  vec3 sample = downsample_filter(sourceBuffer, uvcoordsvar.xy, sourceBufferTexelSize);
#endif
  return vec4(sample, 1.0);
}

vec4 step_upsample(void)
{
#ifdef HIGH_QUALITY
  vec3 blur = upsample_filter_high(sourceBuffer, uvcoordsvar.xy, sourceBufferTexelSize);
#else
  vec3 blur = upsample_filter(sourceBuffer, uvcoordsvar.xy, sourceBufferTexelSize);
#endif
  vec3 base = textureLod(baseBuffer, uvcoordsvar.xy, 0.0).rgb;
  return vec4(base + blur, 1.0);
}

vec4 step_resolve(void)
{
#ifdef HIGH_QUALITY
  vec3 blur = upsample_filter_high(sourceBuffer, uvcoordsvar.xy, sourceBufferTexelSize);
#else
  vec3 blur = upsample_filter(sourceBuffer, uvcoordsvar.xy, sourceBufferTexelSize);
#endif
  vec4 base = bloomAddBase ? textureLod(baseBuffer, uvcoordsvar.xy, 0.0) : vec4(0.0);
  vec3 cout = base.rgb + blur * bloomColor;
  return vec4(cout, base.a);
}

void main(void)
{
#if defined(STEP_BLIT)
  FragColor = step_blit();
#elif defined(STEP_DOWNSAMPLE)
  FragColor = step_downsample();
#elif defined(STEP_UPSAMPLE)
  FragColor = step_upsample();
#elif defined(STEP_RESOLVE)
  FragColor = step_resolve();
#endif
}
