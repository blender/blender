uniform sampler2D depthBuffer;

void main(void)
{
	float depth = texelFetch(depthBuffer, ivec2(gl_FragCoord.xy), 0).r;

	/* background, discard */
	if (depth >= 1.0) {
		discard;
	}

	gl_FragDepth = depth;
}
