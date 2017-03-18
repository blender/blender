
out vec4 FragColor;

uniform vec4 color;

void main()
{
	/* Checkerboard pattern */
	/* 0 | 1
	 * 1 | 0 */
	if ((((int(gl_FragCoord.x) & 0x1) + (int(gl_FragCoord.y) & 0x1)) & 0x1) != 0)
		discard;

	FragColor = vec4(color.rgb * 0.5, 1.0);
}
