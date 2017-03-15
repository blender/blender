uniform sampler2D depthbuffer;

#if __VERSION__ == 120
	varying vec4 uvcoordsvar;
#else
	in vec4 uvcoordsvar;
#endif

void main(void)
{
	float depth = texture2D(depthbuffer, uvcoordsvar.xy).r;

	/* XRay background, discard */
	if (depth >= 1.0) {
		discard;
	}

	gl_FragDepth = depth;
}
