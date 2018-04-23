
uniform sampler2DMS depthMulti;
uniform sampler2DMS colorMulti;

out vec4 fragColor;

#if SAMPLES > 16
#error "Too many samples"
#endif

void main()
{
	ivec2 texel = ivec2(gl_FragCoord.xy);

	float depth = 1.0;
	depth = min(depth, texelFetch(depthMulti, texel, 0).r);
	depth = min(depth, texelFetch(depthMulti, texel, 1).r);
#if SAMPLES > 2
	depth = min(depth, texelFetch(depthMulti, texel, 2).r);
	depth = min(depth, texelFetch(depthMulti, texel, 3).r);
#endif
#if SAMPLES > 4
	depth = min(depth, texelFetch(depthMulti, texel, 4).r);
	depth = min(depth, texelFetch(depthMulti, texel, 5).r);
	depth = min(depth, texelFetch(depthMulti, texel, 6).r);
	depth = min(depth, texelFetch(depthMulti, texel, 7).r);
#endif
#if SAMPLES > 8
	depth = min(depth, texelFetch(depthMulti, texel, 8).r);
	depth = min(depth, texelFetch(depthMulti, texel, 9).r);
	depth = min(depth, texelFetch(depthMulti, texel, 10).r);
	depth = min(depth, texelFetch(depthMulti, texel, 11).r);
	depth = min(depth, texelFetch(depthMulti, texel, 12).r);
	depth = min(depth, texelFetch(depthMulti, texel, 13).r);
	depth = min(depth, texelFetch(depthMulti, texel, 14).r);
	depth = min(depth, texelFetch(depthMulti, texel, 15).r);
#endif

	vec4 color = vec4(0.0);
	color += texelFetch(colorMulti, texel, 0);
	color += texelFetch(colorMulti, texel, 1);
#if SAMPLES > 2
	color += texelFetch(colorMulti, texel, 2);
	color += texelFetch(colorMulti, texel, 3);
#endif
#if SAMPLES > 4
	color += texelFetch(colorMulti, texel, 4);
	color += texelFetch(colorMulti, texel, 5);
	color += texelFetch(colorMulti, texel, 6);
	color += texelFetch(colorMulti, texel, 7);
#endif
#if SAMPLES > 8
	color += texelFetch(colorMulti, texel, 8);
	color += texelFetch(colorMulti, texel, 9);
	color += texelFetch(colorMulti, texel, 10);
	color += texelFetch(colorMulti, texel, 11);
	color += texelFetch(colorMulti, texel, 12);
	color += texelFetch(colorMulti, texel, 13);
	color += texelFetch(colorMulti, texel, 14);
	color += texelFetch(colorMulti, texel, 15);
#endif

	const float inv_samples = 1.0 / float(SAMPLES);

	fragColor = color * inv_samples;
	gl_FragDepth = depth;
}
