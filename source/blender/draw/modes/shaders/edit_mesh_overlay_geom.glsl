
layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

uniform vec2 viewportSize;
uniform vec2 viewportSizeInv;
uniform float edgeScale;

in vec4 finalColor[2];
in vec4 finalColorOuter[2];
in int selectOveride[2];

flat out vec4 finalColorOuter_f;
out vec4 finalColor_f;
out float edgeCoord_f;

void do_vertex(const int i, float coord, vec2 offset)
{
	finalColor_f = (selectOveride[0] == 0) ? finalColor[i] : finalColor[0];
	edgeCoord_f = coord;
	gl_Position = gl_in[i].gl_Position;
	/* Multiply offset by 2 because gl_Position range is [-1..1]. */
	gl_Position.xy += offset * 2.0 * gl_Position.w;
	EmitVertex();
}

void main()
{
	vec2 ss_pos[2];
	ss_pos[0] = gl_in[0].gl_Position.xy / gl_in[0].gl_Position.w;
	ss_pos[1] = gl_in[1].gl_Position.xy / gl_in[1].gl_Position.w;

	vec2 line = ss_pos[0] - ss_pos[1];
	line = abs(line) * viewportSize;

	finalColorOuter_f = finalColorOuter[0];
	float half_size = sizeEdge * edgeScale;
	/* Enlarge edge for flag display. */
	half_size += (finalColorOuter_f.a > 0.0) ? max(sizeEdge * edgeScale, 1.0) : 0.0;
	/* Add 1 px for AA */
	half_size += 0.5;

	vec3 edge_ofs = half_size * viewportSizeInv.xyy * vec3(1.0, 1.0, 0.0);

	bool horizontal = line.x > line.y;
	edge_ofs = (horizontal) ? edge_ofs.zyz : edge_ofs.xzz;

	do_vertex(0,  half_size,  edge_ofs.xy);
	do_vertex(0, -half_size, -edge_ofs.xy);
	do_vertex(1,  half_size,  edge_ofs.xy);
	do_vertex(1, -half_size, -edge_ofs.xy);

	EndPrimitive();
}
