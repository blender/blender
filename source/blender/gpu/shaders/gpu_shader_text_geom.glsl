
uniform mat4 ModelViewProjectionMatrix;

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

	gl_Position = (ModelViewProjectionMatrix * vec4(pos_rect[0].xy, 0.0, 1.0));
	texCoord_interp = vec2(0.0, 0.0);
	EmitVertex();

	gl_Position = (ModelViewProjectionMatrix * vec4(pos_rect[0].zy, 0.0, 1.0));
	texCoord_interp = vec2(1.0, 0.0);
	EmitVertex();

	gl_Position = (ModelViewProjectionMatrix * vec4(pos_rect[0].xw, 0.0, 1.0));
	texCoord_interp = vec2(0.0, 1.0);
	EmitVertex();

	gl_Position = (ModelViewProjectionMatrix * vec4(pos_rect[0].zw, 0.0, 1.0));
	texCoord_interp = vec2(1.0, 1.0);
	EmitVertex();

	EndPrimitive();
}
