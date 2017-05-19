uniform sampler2D depthbuffer;

in vec4 uvcoordsvar;

void main(void)
{
	float depth = texture(depthbuffer, uvcoordsvar.xy).r;

	/* XRay background, discard */
	if (depth >= 1.0) {
		discard;
	}

	gl_FragDepth = depth;
}
