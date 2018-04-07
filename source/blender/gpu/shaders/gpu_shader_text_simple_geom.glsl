
layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

in vec4 pos_rect[];
in vec4 tex_rect[];
in vec4 color[];

flat out vec4 color_flat;
flat out vec4 texCoord_rect;
noperspective out vec2 texCoord_interp;

void main()
{
	color_flat = color[0];
	texCoord_rect = tex_rect[0];
	gl_Position.zw = vec2(0.0, 1.0);

	gl_Position.xy  = pos_rect[0].xy;
	texCoord_interp = vec2(0.0, 0.0);
	EmitVertex();

	gl_Position.xy  = pos_rect[0].zy;
	texCoord_interp = vec2(1.0, 0.0);
	EmitVertex();

	gl_Position.xy  = pos_rect[0].xw;
	texCoord_interp = vec2(0.0, 1.0);
	EmitVertex();

	gl_Position.xy  = pos_rect[0].zw;
	texCoord_interp = vec2(1.0, 1.0);
	EmitVertex();

	EndPrimitive();
}
