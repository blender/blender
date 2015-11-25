uniform ivec2 rendertargetdim;
uniform sampler2D colorbuffer;

uniform vec2 layerselection;

uniform sampler2D cocbuffer;

#if __VERSION__ >= 150
  layout(points) in;
  layout(triangle_strip, max_vertices = 4) out;

  #define POS gl_in[0].gl_Position
#else
  /* use the EXT_geometry_shader4 way */
  #define POS gl_PositionIn[0]
#endif

/* initial uv coordinate */
#if __VERSION__ < 130
  varying in vec2 uvcoord[];
  varying out vec2 particlecoord;
  varying out vec4 color;
  #define textureLod texture2DLod
#else
  in vec2 uvcoord[];
  out vec2 particlecoord;
  out vec4 color;
#endif

#define M_PI 3.1415926535897932384626433832795

void main()
{
	vec4 coc = textureLod(cocbuffer, uvcoord[0], 0.0);

	float offset_val = dot(coc.rg, layerselection);
	if (offset_val < 1.0)
		return;

	vec4 colortex = textureLod(colorbuffer, uvcoord[0], 0.0);

	/* find the area the pixel will cover and divide the color by it */
	float alpha = 1.0 / (offset_val * offset_val * M_PI);
	colortex *= alpha;
	colortex.a = alpha;

	vec2 offset_far = vec2(offset_val * 0.5) / vec2(rendertargetdim.x, rendertargetdim.y);

	gl_Position = POS + vec4(-offset_far.x, -offset_far.y, 0.0, 0.0);
	color = colortex;
	particlecoord = vec2(-1.0, -1.0);
	EmitVertex();
	gl_Position = POS + vec4(-offset_far.x, offset_far.y, 0.0, 0.0);
	particlecoord = vec2(-1.0, 1.0);
	color = colortex;
	EmitVertex();
	gl_Position = POS + vec4(offset_far.x, -offset_far.y, 0.0, 0.0);
	particlecoord = vec2(1.0, -1.0);
	color = colortex;
	EmitVertex();
	gl_Position = POS + vec4(offset_far.x, offset_far.y, 0.0, 0.0);
	particlecoord = vec2(1.0, 1.0);
	color = colortex;
	EmitVertex();
	EndPrimitive();
}
