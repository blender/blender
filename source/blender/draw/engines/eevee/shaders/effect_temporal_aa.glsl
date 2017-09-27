
uniform sampler2D colorBuffer;
uniform sampler2D historyBuffer;
uniform float alpha;

out vec4 FragColor;

void main()
{
	/* TODO History buffer Reprojection */
	vec4 history = texelFetch(historyBuffer, ivec2(gl_FragCoord.xy), 0).rgba;
	vec4 color = texelFetch(colorBuffer, ivec2(gl_FragCoord.xy), 0).rgba;
	FragColor = mix(history, color, alpha);
}