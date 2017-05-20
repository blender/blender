
flat in vec4 finalColor;

#ifndef USE_WIRE
in vec2 texCoord_interp;
#endif

out vec4 fragColor;

#ifndef USE_WIRE
uniform sampler2D image;
#endif

void main()
{
#ifdef USE_WIRE
	fragColor = finalColor;
#else
	fragColor = finalColor * texture(image, texCoord_interp);
#endif
}
