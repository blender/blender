
flat in vec4 finalColor;

#ifndef USE_WIRE
in vec2 texCoord_interp;
#endif

out vec4 fragColor;

#ifndef USE_WIRE
uniform sampler2D image;
#endif

uniform int depthMode;

void main()
{
#ifdef USE_WIRE
	fragColor = finalColor;
#else
	fragColor = finalColor * texture(image, texCoord_interp);
#endif

	if (depthMode == DEPTH_BACK) {
		gl_FragDepth = 0.999999;
	}
	else if (depthMode == DEPTH_FRONT) {
		gl_FragDepth = 0.000001;
	}
	else if (depthMode == DEPTH_UNCHANGED) {
		gl_FragDepth = gl_FragCoord.z;
	}
}
