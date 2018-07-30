
uniform sampler2DMS depthMulti;
uniform sampler2DMS colorMulti;

out vec4 fragColor;

#if SAMPLES > 16
#error "Too many samples"
#endif

void main()
{
	ivec2 texel = ivec2(gl_FragCoord.xy);

	bvec4 b1, b2, b3, b4;
	vec4 w1, w2, w3, w4;
	vec4 d1, d2, d3, d4;
	vec4 c1, c2, c3, c4, c5, c6, c7, c8;
	vec4 c9, c10, c11, c12, c13, c14, c15, c16;
	d1 = d2 = d3 = d4 = vec4(0.5);
	w1 = w2 = w3 = w4 = vec4(0.0);
	c1 = c2 = c3 = c4 = c5 = c6 = c7 = c8 = vec4(0.0);
	c9 = c10 = c11 = c12 = c13 = c14 = c15 = c16 = vec4(0.0);

#ifdef USE_DEPTH
	/* Depth */
	d1.x = texelFetch(depthMulti, texel, 0).r;
	d1.y = texelFetch(depthMulti, texel, 1).r;
#  if SAMPLES > 2
	d1.z = texelFetch(depthMulti, texel, 2).r;
	d1.w = texelFetch(depthMulti, texel, 3).r;
#  endif
#  if SAMPLES > 4
	d2.x = texelFetch(depthMulti, texel, 4).r;
	d2.y = texelFetch(depthMulti, texel, 5).r;
	d2.z = texelFetch(depthMulti, texel, 6).r;
	d2.w = texelFetch(depthMulti, texel, 7).r;
#  endif
#  if SAMPLES > 8
	d3.x = texelFetch(depthMulti, texel, 8).r;
	d3.y = texelFetch(depthMulti, texel, 9).r;
	d3.z = texelFetch(depthMulti, texel, 10).r;
	d3.w = texelFetch(depthMulti, texel, 11).r;
	d4.x = texelFetch(depthMulti, texel, 12).r;
	d4.y = texelFetch(depthMulti, texel, 13).r;
	d4.z = texelFetch(depthMulti, texel, 14).r;
	d4.w = texelFetch(depthMulti, texel, 15).r;
#  endif
#endif

	/* COLOR */
	b1 = notEqual(d1, vec4(1.0));
	if (any(b1)) {
		c1 = texelFetch(colorMulti, texel, 0);
		c2 = texelFetch(colorMulti, texel, 1);
#if SAMPLES > 2
		c3 = texelFetch(colorMulti, texel, 2);
		c4 = texelFetch(colorMulti, texel, 3);
#endif
		w1 = vec4(b1);
	}
#if SAMPLES > 4
	b2 = notEqual(d2, vec4(1.0));
	if (any(b2)) {
		c5 = texelFetch(colorMulti, texel, 4);
		c6 = texelFetch(colorMulti, texel, 5);
		c7 = texelFetch(colorMulti, texel, 6);
		c8 = texelFetch(colorMulti, texel, 7);
		w2 = vec4(b2);
	}
#endif
#if SAMPLES > 8
	b3 = notEqual(d3, vec4(1.0));
	if (any(b3)) {
		c9 = texelFetch(colorMulti, texel, 8);
		c10 = texelFetch(colorMulti, texel, 9);
		c11 = texelFetch(colorMulti, texel, 10);
		c12 = texelFetch(colorMulti, texel, 11);
		w3 = vec4(b3);
	}
	b4 = notEqual(d4, vec4(1.0));
	if (any(b4)) {
		c13 = texelFetch(colorMulti, texel, 12);
		c14 = texelFetch(colorMulti, texel, 13);
		c15 = texelFetch(colorMulti, texel, 14);
		c16 = texelFetch(colorMulti, texel, 15);
		w4 = vec4(b4);
	}
#endif

#ifdef USE_DEPTH
	d1 *= 1.0 - step(1.0, d1); /* make far plane depth = 0 */
#  if SAMPLES > 8
	d4 *= 1.0 - step(1.0, d4);
	d3 *= 1.0 - step(1.0, d3);
	d1 = max(d1, max(d3, d4));
#  endif
#  if SAMPLES > 4
	d2 *= 1.0 - step(1.0, d2);
	d1 = max(d1, d2);
	d1 = max(d1, d2);
#  endif
#  if SAMPLES > 2
	d1.xy = max(d1.xy, d1.zw);
#  endif
	gl_FragDepth = max(d1.x, d1.y);
	/* Don't let the 0.0 farplane occlude other things */
	if (gl_FragDepth == 0.0) {
		gl_FragDepth = 1.0;
	}
#endif

	c1 =  c1 + c2;
#if SAMPLES > 2
	c1 += c3 + c4;
#endif
#if SAMPLES > 4
	c1 += c5 + c6 + c7 + c8;
#endif
#if SAMPLES > 8
	c1 += c9 + c10 + c11 + c12 + c13 + c14 + c15 + c16;
#endif

	const float inv_samples = 1.0 / float(SAMPLES);

	fragColor = c1 * inv_samples;
}
