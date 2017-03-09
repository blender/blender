
layout(points) in;
layout(line_strip, max_vertices=2) out;

flat in vec4 v1[1];
flat in vec4 v2[1];

void main()
{
	gl_Position = v1[0];
	EmitVertex();
	gl_Position = v2[0];
	EmitVertex();
	EndPrimitive();
}
