
/* This shader is only used for edge selection and OSX workaround for large wires. */

uniform float wireSize;
uniform vec2 viewportSize;
uniform vec2 viewportSizeInv;

layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

in float facing_g[];
in float edgeSharpness_g[];

#ifndef SELECT_EDGES
out float facing;
flat out float edgeSharpness;
#endif

void do_vertex(const int i, float coord, vec2 offset)
{
#ifndef SELECT_EDGES
	edgeSharpness = edgeSharpness_g[i];
	facing = facing_g[i];
#endif
	gl_Position = gl_in[i].gl_Position;
	/* Multiply offset by 2 because gl_Position range is [-1..1]. */
	gl_Position.xy += offset * 2.0 * gl_Position.w;
#ifdef USE_WORLD_CLIP_PLANES
	world_clip_planes_set_clip_distance(gl_in[i].gl_ClipDistance);
#endif
	EmitVertex();
}

void main()
{
	vec2 ss_pos[2];
	ss_pos[0] = gl_in[0].gl_Position.xy / gl_in[0].gl_Position.w;
	ss_pos[1] = gl_in[1].gl_Position.xy / gl_in[1].gl_Position.w;

	vec2 line = ss_pos[0] - ss_pos[1];
	line = abs(line) * viewportSize;

	float half_size = wireSize;

	vec3 edge_ofs = half_size * viewportSizeInv.xyy * vec3(1.0, 1.0, 0.0);

	bool horizontal = line.x > line.y;
	edge_ofs = (horizontal) ? edge_ofs.zyz : edge_ofs.xzz;

	if (edgeSharpness_g[0] < 0.0) {
		return;
	}

	do_vertex(0,  half_size,  edge_ofs.xy);
	do_vertex(0, -half_size, -edge_ofs.xy);
	do_vertex(1,  half_size,  edge_ofs.xy);
	do_vertex(1, -half_size, -edge_ofs.xy);

	EndPrimitive();
}
