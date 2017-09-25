
uniform sampler2D colorBuffer;
uniform float alpha;

out vec4 FragColor;

void main()
{
	/* TODO History buffer Reprojection */
	FragColor = vec4(texelFetch(colorBuffer, ivec2(gl_FragCoord.xy), 0).rgb, alpha);
}