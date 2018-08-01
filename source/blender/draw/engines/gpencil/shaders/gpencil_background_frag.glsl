out vec4 FragColor;

uniform sampler2D strokeColor;
uniform sampler2D strokeDepth;

void main()
{
	ivec2 uv = ivec2(gl_FragCoord.xy);

	gl_FragDepth = texelFetch(strokeDepth, uv, 0).r;
	FragColor = texelFetch(strokeColor, uv, 0);
}
