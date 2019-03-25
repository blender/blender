
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
noperspective out float edgeCoord_f;

void do_vertex(const int i, vec4 pos, float coord, vec2 offset)
{
	finalColor_f = (selectOveride[0] == 0) ? finalColor[i] : finalColor[0];
	edgeCoord_f = coord;
	gl_Position = pos;
	/* Multiply offset by 2 because gl_Position range is [-1..1]. */
	gl_Position.xy += offset * 2.0 * pos.w;
#ifdef USE_WORLD_CLIP_PLANES
	world_clip_planes_set_clip_distance(gl_in[i].gl_ClipDistance);
#endif
	EmitVertex();
}

void main()
{
	vec2 ss_pos[2];

	/* Clip line against near plane to avoid deformed lines. */
	vec4 pos0 = gl_in[0].gl_Position;
	vec4 pos1 = gl_in[1].gl_Position;
	vec2 pz_ndc = vec2(pos0.z / pos0.w, pos1.z / pos1.w);
	bvec2 clipped = lessThan(pz_ndc, vec2(-1.0));
	if (all(clipped)) {
		/* Totally clipped. */
		return;
	}

	vec4 pos01 = pos0 - pos1;
	float ofs = abs((pz_ndc.y + 1.0) / (pz_ndc.x - pz_ndc.y));
	if (clipped.y) {
		pos1 += pos01 * ofs;
	}
	else if (clipped.x) {
		pos0 -= pos01 * (1.0 - ofs);
	}

	ss_pos[0] = pos0.xy / pos0.w;
	ss_pos[1] = pos1.xy / pos1.w;

	vec2 line = ss_pos[0] - ss_pos[1];
	line = abs(line) * viewportSize;

	finalColorOuter_f = finalColorOuter[0];
	float half_size = sizeEdge * edgeScale;
	/* Enlarge edge for flag display. */
	half_size += (finalColorOuter_f.a > 0.0) ? max(sizeEdge * edgeScale, 1.0) : 0.0;

#ifdef USE_SMOOTH_WIRE
	/* Add 1 px for AA */
	half_size += 0.5;
#endif

	vec3 edge_ofs = half_size * viewportSizeInv.xyy * vec3(1.0, 1.0, 0.0);

	bool horizontal = line.x > line.y;
	edge_ofs = (horizontal) ? edge_ofs.zyz : edge_ofs.xzz;

	do_vertex(0, pos0,  half_size,  edge_ofs.xy);
	do_vertex(0, pos0, -half_size, -edge_ofs.xy);
	do_vertex(1, pos1,  half_size,  edge_ofs.xy);
	do_vertex(1, pos1, -half_size, -edge_ofs.xy);

	EndPrimitive();
}
