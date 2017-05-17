uniform sampler2D depthbuffer;

in vec4 uvcoordsvar;
#define texture2D texture

void main(void)
{
	float depth = texture2D(depthbuffer, uvcoordsvar.xy).r;

	/* XRay background, discard */
	if (depth >= 1.0) {
		discard;
	}

	gl_FragDepth = depth;
}
