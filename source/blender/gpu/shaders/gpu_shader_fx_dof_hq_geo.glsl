uniform ivec2 rendertargetdim;
uniform sampler2D colorbuffer;

uniform vec2 layerselection;

uniform sampler2D cocbuffer;

/* initial uv coordinate */
varying in vec2 uvcoord[1];
varying out vec2 particlecoord;
varying out vec4 color;


#define M_PI 3.1415926535897932384626433832795

void main(void)
{
	vec4 coc = texture2DLod(cocbuffer, uvcoord[0], 0.0);

	float offset_val = dot(coc.rg, layerselection);
	if (offset_val < 1.0)
		return;

	vec4 colortex = texture2DLod(colorbuffer, uvcoord[0], 0.0);

	/* find the area the pixel will cover and divide the color by it */
	float alpha = 1.0 / (offset_val * offset_val * M_PI);
	colortex *= alpha;
	colortex.a = alpha;

	vec2 offset_far = vec2(offset_val * 0.5) / vec2(rendertargetdim.x, rendertargetdim.y);

	gl_Position = gl_PositionIn[0] + vec4(-offset_far.x, -offset_far.y, 0.0, 0.0);
	color = colortex;
	particlecoord = vec2(-1.0, -1.0);
	EmitVertex();
	gl_Position = gl_PositionIn[0] + vec4(-offset_far.x, offset_far.y, 0.0, 0.0);
	particlecoord = vec2(-1.0, 1.0);
	color = colortex;
	EmitVertex();
	gl_Position = gl_PositionIn[0] + vec4(offset_far.x, -offset_far.y, 0.0, 0.0);
	particlecoord = vec2(1.0, -1.0);
	color = colortex;
	EmitVertex();
	gl_Position = gl_PositionIn[0] + vec4(offset_far.x, offset_far.y, 0.0, 0.0);
	particlecoord = vec2(1.0, 1.0);
	color = colortex;
	EmitVertex();
	EndPrimitive();
}
