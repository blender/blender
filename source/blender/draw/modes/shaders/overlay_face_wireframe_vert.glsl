
uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelViewMatrix;
uniform mat4 ProjectionMatrix;
uniform mat3 NormalMatrix;

uniform vec2 wireStepParam;
uniform vec2 viewportSize;
uniform float nearDist;

uniform samplerBuffer vertData;
uniform usamplerBuffer faceIds;

#ifdef USE_SCULPT
in vec3 pos;
in vec3 nor;
#endif

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
#  ifdef USE_GEOM_SHADER
out vec3 obPos;
out vec3 vNor;
out float forceEdge;
#  else
flat out vec3 edgeSharpness;
#  endif
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

vec3 get_vertex_nor(uint id)
{
	int v_id = int(id) * 5; /* See vertex format for explanation. */
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

vec3 get_vertex_pos(uint id)
{
	int v_id = int(id) * 5; /* See vertex format for explanation. */
	vec3 pos;
	pos.x = texelFetch(vertData, v_id).r;
	pos.y = texelFetch(vertData, v_id + 1).r;
	pos.z = texelFetch(vertData, v_id + 2).r;
	return pos;
}

vec3 get_edge_normal(vec3 n1, vec3 n2, vec3 edge)
{
	edge = normalize(edge);
	vec3 n = n1 + n2;
	float p = dot(edge, n);
	return normalize(n - p * edge);
}

float get_edge_sharpness(vec3 fnor, vec3 vnor)
{
	float sharpness = abs(dot(fnor, vnor));
	return smoothstep(wireStepParam.x, wireStepParam.y, sharpness);
}

#define NO_EDGE vec3(10000.0);

void main()
{
#ifdef USE_GEOM_SHADER
#ifndef USE_SCULPT
	uint v_id = texelFetch(faceIds, gl_VertexID).r;

	bool do_edge = (v_id & (1u << 30u)) != 0u;
	bool force_edge = (v_id & (1u << 31u)) != 0u;
	v_id = (v_id << 2u) >> 2u;

	vec3 pos = get_vertex_pos(v_id);
	vec3 nor = get_vertex_nor(v_id);
#else
	const bool do_edge = true;
	const bool force_edge = false;
#endif

	facingOut = normalize(NormalMatrix * nor).z;
	facingOut += (do_edge) ? ((facingOut > 0.0) ? 2.0 : -2.0) : 0.0;

	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	ssPos = proj(gl_Position);

#  ifdef LIGHT_EDGES
	obPos = pos;
	vNor = nor;
	forceEdge = float(force_edge); /* meh, could try to also encode it in facingOut */
#  endif

#else
	int v_0 = (gl_VertexID / 3) * 3;
	int v_n = gl_VertexID % 3;

	/* Getting the same positions for each of the 3 verts. */
	uvec3 v_id;
	v_id.x = texelFetch(faceIds, v_0).r;
	v_id.y = texelFetch(faceIds, v_0 + 1).r;
	v_id.z = texelFetch(faceIds, v_0 + 2).r;

	bvec3 do_edge, force_edge;
	do_edge.x = (v_id.x & (1u << 30u)) != 0u;
	do_edge.y = (v_id.y & (1u << 30u)) != 0u;
	do_edge.z = (v_id.z & (1u << 30u)) != 0u;
	force_edge.x = (v_id.x & (1u << 31u)) != 0u;
	force_edge.y = (v_id.y & (1u << 31u)) != 0u;
	force_edge.z = (v_id.z & (1u << 31u)) != 0u;
	v_id = (v_id << 2u) >> 2u;

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

#  ifndef LIGHT_EDGES
	vec3 nor = get_vertex_nor(v_id[v_n]);
#  else
	vec3 edges[3];
	edges[0] = pos[1] - pos[0];
	edges[1] = pos[2] - pos[1];
	edges[2] = pos[0] - pos[2];
	vec3 fnor = normalize(cross(edges[0], -edges[2]));

	vec3 nors[3];
	nors[0] = get_vertex_nor(v_id.x);
	nors[1] = get_vertex_nor(v_id.y);
	nors[2] = get_vertex_nor(v_id.z);
	edgeSharpness.x = get_edge_sharpness(fnor, get_edge_normal(nors[0], nors[1], edges[0]));
	edgeSharpness.y = get_edge_sharpness(fnor, get_edge_normal(nors[1], nors[2], edges[1]));
	edgeSharpness.z = get_edge_sharpness(fnor, get_edge_normal(nors[2], nors[0], edges[2]));
	edgeSharpness.x = force_edge.x ? 1.0 : edgeSharpness.x;
	edgeSharpness.y = force_edge.y ? 1.0 : edgeSharpness.y;
	edgeSharpness.z = force_edge.z ? 1.0 : edgeSharpness.z;

	vec3 nor = nors[v_n];
#  endif

	facing = normalize(NormalMatrix * nor).z;

#endif
}
