in vec4 uvcoordsvar;

out vec4 FragColor;

uniform sampler2D strokeColor;
uniform sampler2D strokeDepth;
void main()
{
	ivec2 uv = ivec2(gl_FragCoord.xy);
	float stroke_depth = texelFetch(strokeDepth, uv, 0).r;
	vec4 stroke_color =  texelFetch(strokeColor, uv, 0).rgba;

	FragColor = stroke_color;
	gl_FragDepth = stroke_depth;
}
