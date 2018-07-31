
out vec4 FragColor;

uniform sampler2D strokeColor;
uniform sampler2D strokeDepth;

uniform float amplitude;
uniform float period;
uniform float phase;
uniform int orientation;
uniform vec2 wsize;

#define M_PI 3.1415926535897932384626433832795

#define HORIZONTAL 0
#define VERTICAL 1

void main()
{
	vec4 outcolor;
	ivec2 uv = ivec2(gl_FragCoord.xy);
	float stroke_depth;

	float value;
	if (orientation == HORIZONTAL) {
		float pval = (uv.x * M_PI) / wsize[0];
		value = amplitude * sin((period * pval) + phase);
		outcolor = texelFetch(strokeColor, ivec2(uv.x, uv.y + value), 0);
		stroke_depth = texelFetch(strokeDepth, ivec2(uv.x, uv.y + value), 0).r;
	}
	else {
		float pval = (uv.y * M_PI) / wsize[1];
		value = amplitude * sin((period * pval) + phase);
		outcolor = texelFetch(strokeColor, ivec2(uv.x + value, uv.y), 0);
		stroke_depth = texelFetch(strokeDepth, ivec2(uv.x + value, uv.y), 0).r;
	}

	FragColor = outcolor;
	gl_FragDepth = stroke_depth;
}
