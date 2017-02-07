
/* Make to be used with dynamic batching so no Model Matrix needed */
uniform mat4 ViewProjectionMatrix;

layout(points) in;
layout(line_strip, max_vertices = 2) out;

void main()
{
	vec3 vert = gl_in[0].gl_Position.xyz;
	gl_Position = ViewProjectionMatrix * vec4(vert.xyz, 1.0);
	EmitVertex();
	gl_Position = ViewProjectionMatrix * vec4(vert.xy, 0.0, 1.0);
	EmitVertex();
	EndPrimitive();
}
