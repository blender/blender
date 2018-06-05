
uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelViewMatrix;
uniform mat4 ProjectionMatrix;
uniform mat3 NormalMatrix;

uniform vec2 wireStepParam;
uniform vec2 viewportSize;
uniform float nearDist;

uniform samplerBuffer vertData;
uniform isamplerBuffer faceIds;

#ifdef USE_GEOM_SHADER
out vec2 ssPos;
out float facingOut; /* abs(facing) > 1.0 if we do edge */
#else
flat out vec3 ssVec0;
flat out vec3 ssVec1;
flat out vec3 ssVec2;
out float facing;
#endif

#ifdef LIGHT_EDGES
#ifdef USE_GEOM_SHADER
out vec3 obPos;
out vec3 edgeAdj;
#else
flat out vec3 edgeSharpness;
#endif
#endif

/* project to screen space */
vec2 proj(vec4 pos)
{
	return (0.5 * (pos.xy / pos.w) + 0.5) * viewportSize;
}

vec3 compute_vec(vec2 v0, vec2 v1)
{
	vec2 v = normalize(v1 - v0);
	v = vec2(-v.y, v.x);
	return vec3(v, -dot(v, v0));
}

float short_to_unit_float(uint s)
{
	int value = int(s) & 0x7FFF;
	if ((s & 0x8000u) != 0u) {
		value |= ~0x7FFF;
	}
	return float(value) / float(0x7FFF);
}

vec3 get_vertex_nor(int v_id)
{
	v_id *= 5; /* See vertex format for explanation. */
	/* Fetch compressed normal as float and unpack them. */
	vec2 data;
	data.x = texelFetch(vertData, v_id + 3).r;
	data.y = texelFetch(vertData, v_id + 4).r;

	uvec2 udata = floatBitsToUint(data);

	vec3 nor;
	nor.x = short_to_unit_float(udata.x & 0xFFFFu);
	nor.y = short_to_unit_float(udata.x >> 16u);
	nor.z = short_to_unit_float(udata.y & 0xFFFFu);
	return nor;
}

vec3 get_vertex_pos(int v_id)
{
	v_id *= 5; /* See vertex format for explanation. */
	vec3 pos;
	pos.x = texelFetch(vertData, v_id).r;
	pos.y = texelFetch(vertData, v_id + 1).r;
	pos.z = texelFetch(vertData, v_id + 2).r;
	return pos;
}

float get_edge_sharpness(vec3 e0, vec3 e1, vec3 e2)
{
	vec3 n0 = normalize(cross(e0, e1));
	vec3 n1 = normalize(cross(e1, e2));
	return dot(n0, n1);
}

#define NO_EDGE vec3(10000.0);

void main()
{
#ifdef USE_GEOM_SHADER

#  ifdef LIGHT_EDGES
	int v_id = texelFetch(faceIds, gl_VertexID * 2).r;
#  else
	int v_id = texelFetch(faceIds, gl_VertexID).r;
#  endif

	bool do_edge = v_id < 0;
	v_id = abs(v_id) - 1;

	vec3 pos = get_vertex_pos(v_id);
	vec3 nor = get_vertex_nor(v_id);
	facingOut = normalize(NormalMatrix * nor).z;
	facingOut += (do_edge) ? ((facingOut > 0.0) ? 2.0 : -2.0) : 0.0;

	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	ssPos = proj(gl_Position);

#  ifdef LIGHT_EDGES
	int adj_id = texelFetch(faceIds, gl_VertexID * 2 + 1).r;
	obPos = pos;
	edgeAdj = get_vertex_pos(adj_id);
#  endif

#else

#  ifdef LIGHT_EDGES
	int v_0 = (gl_VertexID / 3) * 6;
	ivec2 ofs = ivec2(2, 4); /* GL_TRIANGLE_ADJACENCY */
#  else
	int v_0 = (gl_VertexID / 3) * 3;
	ivec2 ofs = ivec2(1, 2); /* GL_TRIANGLES */
#  endif
	int v_n = gl_VertexID % 3;

	/* Getting the same positions for each of the 3 verts. */
	ivec3 v_id;
	v_id.x = texelFetch(faceIds, v_0).r;
	v_id.y = texelFetch(faceIds, v_0 + ofs.x).r;
	v_id.z = texelFetch(faceIds, v_0 + ofs.y).r;

	bvec3 do_edge = lessThan(v_id, ivec3(0));
	v_id = abs(v_id) - 1;

	vec3 pos[3];
	pos[0] = get_vertex_pos(v_id.x);
	pos[1] = get_vertex_pos(v_id.y);
	pos[2] = get_vertex_pos(v_id.z);

	vec4 p_pos[3];
	p_pos[0] = ModelViewProjectionMatrix * vec4(pos[0], 1.0);
	p_pos[1] = ModelViewProjectionMatrix * vec4(pos[1], 1.0);
	p_pos[2] = ModelViewProjectionMatrix * vec4(pos[2], 1.0);

	vec2 ss_pos[3];
	ss_pos[0] = proj(p_pos[0]);
	ss_pos[1] = proj(p_pos[1]);
	ss_pos[2] = proj(p_pos[2]);

	/* Compute the edges screen vectors */
	ssVec0 = do_edge.x ? compute_vec(ss_pos[0], ss_pos[1]) : NO_EDGE;
	ssVec1 = do_edge.y ? compute_vec(ss_pos[1], ss_pos[2]) : NO_EDGE;
	ssVec2 = do_edge.z ? compute_vec(ss_pos[2], ss_pos[0]) : NO_EDGE;

	gl_Position = p_pos[v_n];

	vec3 nor = get_vertex_nor(v_id[v_n]);
	facing = normalize(NormalMatrix * nor).z;

#  ifdef LIGHT_EDGES
	ivec3 adj_id;
	adj_id.x = texelFetch(faceIds, v_0 + 1).r;
	adj_id.y = texelFetch(faceIds, v_0 + 3).r;
	adj_id.z = texelFetch(faceIds, v_0 + 5).r;

	vec3 adj_pos[3];
	adj_pos[0] = get_vertex_pos(adj_id.x);
	adj_pos[1] = get_vertex_pos(adj_id.y);
	adj_pos[2] = get_vertex_pos(adj_id.z);

	vec3 edges[3];
	edges[0] = pos[1] - pos[0];
	edges[1] = pos[2] - pos[1];
	edges[2] = pos[0] - pos[2];

	edgeSharpness.x = get_edge_sharpness(adj_pos[0] - pos[0], edges[0], -edges[2]);
	edgeSharpness.y = get_edge_sharpness(adj_pos[1] - pos[1], edges[1], -edges[0]);
	edgeSharpness.z = get_edge_sharpness(adj_pos[2] - pos[2], edges[2], -edges[1]);

	/* Easy to adjust parameters. */
	edgeSharpness = smoothstep(wireStepParam.xxx, wireStepParam.yyy, edgeSharpness);
#  endif

#endif
}
