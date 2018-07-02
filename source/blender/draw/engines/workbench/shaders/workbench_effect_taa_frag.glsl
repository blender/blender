uniform sampler2D historyBuffer;
uniform sampler2D colorBuffer;

out vec4 colorOutput;

uniform float mixFactor;

void main()
{
	ivec2 texel = ivec2(gl_FragCoord.xy);
	vec4 color_buffer = texelFetch(colorBuffer, texel, 0);
	vec4 history_buffer = texelFetch(historyBuffer, texel, 0);
	colorOutput = mix(history_buffer, color_buffer, mixFactor);
}
