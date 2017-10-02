uniform sampler2D gridTexture;

out vec4 FragColor;

void main()
{
#if defined(IRRADIANCE_SH_L2)
	const ivec2 data_size = ivec2(3, 3);
#elif defined(IRRADIANCE_CUBEMAP)
	const ivec2 data_size = ivec2(8, 8);
#elif defined(IRRADIANCE_HL2)
	const ivec2 data_size = ivec2(3, 2);
#endif
	ivec2 coord = ivec2(gl_FragCoord.xy) % data_size;
	FragColor = texelFetch(gridTexture, coord, 0);

	if (any(greaterThanEqual(ivec2(gl_FragCoord.xy), data_size))) {
		FragColor = vec4(0.0, 0.0, 0.0, 1.0);
	}
}