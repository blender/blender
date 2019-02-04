
layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

uniform vec2 viewportSize;
uniform vec2 viewportSizeInv;

in VertexData {
	vec4 finalColor;
#if defined(EDGE) && !defined(FLAT)
	int selectOveride;
#endif
} v[];

#ifdef FLAT
#  define interp_col flat
#else
#  define interp_col
#endif

interp_col out vec4 finalColor;
#if defined(EDGE) && !defined(FLAT)
flat out int selectOveride;
#endif

void do_vertex(const int i, vec2 offset)
{
	finalColor = v[i].finalColor;
#if defined(EDGE) && !defined(FLAT)
	selectOveride = v[0].selectOveride;
#endif
	gl_Position = gl_in[i].gl_Position;
	gl_Position.xy += offset * gl_Position.w;
	EmitVertex();
}

void main()
{
	vec2 ss_pos[2];
	ss_pos[0] = gl_in[0].gl_Position.xy / gl_in[0].gl_Position.w;
	ss_pos[1] = gl_in[1].gl_Position.xy / gl_in[1].gl_Position.w;

	vec2 line = ss_pos[0] - ss_pos[1];

	vec3 edge_ofs = sizeEdge * 2.0 * viewportSizeInv.xyy * vec3(1.0, 1.0, 0.0);

#ifdef EDGE_DECORATION
	edge_ofs *= 3.0;

	if (finalColor.a == 0.0) {
		return;
	}
#endif

	bool horizontal = abs(line.x) > abs(line.y);
	edge_ofs = (horizontal) ? edge_ofs.zyz : edge_ofs.xzz;

	do_vertex(0,  edge_ofs.xy);
	do_vertex(0, -edge_ofs.xy);
	do_vertex(1,  edge_ofs.xy);
	do_vertex(1, -edge_ofs.xy);

	EndPrimitive();
}
