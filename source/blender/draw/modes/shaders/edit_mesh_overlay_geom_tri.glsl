
/* Solid Wirefram implementation
 * Mike Erwin, Clément Foucault */

layout(triangles) in;

/* To fix the edge artifacts, we render
 * an outline strip around the screenspace
 * triangle. Order is important.
 * TODO diagram
 */
layout(triangle_strip, max_vertices=12) out;

uniform mat4 ProjectionMatrix;
uniform vec2 viewportSize;
uniform bool isXray = false;

in vec4 pPos[];
in ivec4 vData[];
#ifdef VERTEX_FACING
in float vFacing[];
#endif

/* these are the same for all vertices
 * and does not need interpolation */
flat out vec3 edgesCrease;
flat out vec3 edgesBweight;
flat out vec4 faceColor;
flat out ivec3 flag;

flat out vec2 ssPos[3];
#ifdef VERTEX_SELECTION
out vec3 vertexColor;
#endif
#ifdef VERTEX_FACING
out float facing;
#endif

#ifdef ANTI_ALIASING
#define Z_OFFSET -0.0005
#else
#define Z_OFFSET 0.0
#endif

/* Some bugged AMD drivers need these global variables. See T55961 */
#ifdef VERTEX_SELECTION
vec3 vertex_color[3];
#endif

#ifdef VERTEX_FACING
float v_facing[3];
#endif

/* project to screen space */
vec2 proj(vec4 pos)
{
	return (0.5 * (pos.xy / pos.w) + 0.5) * viewportSize;
}

void doVertex(int v)
{
#ifdef VERTEX_SELECTION
	vertexColor = vertex_color[v];
#endif

#ifdef VERTEX_FACING
	facing = v_facing[v];
#endif
	gl_Position = pPos[v];

	EmitVertex();
}

void doVertexOfs(int v, vec2 fixvec)
{
#ifdef VERTEX_SELECTION
	vertexColor = vertex_color[v];
#endif

#ifdef VERTEX_FACING
	facing = v_facing[v];
#endif
	float z_ofs = Z_OFFSET * ((ProjectionMatrix[3][3] == 0.0) ? 1.0 : 0.0);
	gl_Position = pPos[v] + vec4(fixvec * pPos[v].w, z_ofs, 0.0);

	EmitVertex();
}

void mask_edge_flag(int v, ivec3 eflag)
{
	int vaf = (v + 1) % 3;

	/* Only shade the edge that we are currently drawing.
	 * (fix corner bleeding) */
	flag = eflag & ~EDGE_VERTEX_EXISTS;
	flag[vaf] &= ~EDGE_EXISTS;
	flag[v]   &= ~EDGE_EXISTS;
}

vec2 compute_fixvec(int i)
{
	int i1 = (i + 1) % 3;
	int i2 = (i + 2) % 3;
	/* This fix the case when 2 vertices are perfectly aligned
	 * and corner vectors have nowhere to go.
	 * ie: length(cornervec[i]) == 0 */
	const float epsilon = 1e-2; /* in pixel so not that much */
	const vec2 bias[3] = vec2[3](
		vec2( epsilon,  epsilon),
		vec2(-epsilon,  epsilon),
		vec2(     0.0, -epsilon)
	);
	vec2 v1 = ssPos[i] + bias[i];
	vec2 v2 = ssPos[i1] + bias[i1];
	vec2 v3 = ssPos[i2] + bias[i2];
	/* Edge normalized vector */
	vec2 dir = normalize(v2 - v1);
	vec2 dir2 = normalize(v3 - v1);
	/* perpendicular to dir */
	vec2 perp = vec2(-dir.y, dir.x);
	/* Backface case */
	if (dot(perp, dir2) > 0.0) {
		perp = -perp;
	}
	/* Make it view independent */
	return perp * sizeEdgeFix / viewportSize;
}

void main()
{
	/* Edge */
	ivec3 eflag;
	for (int v = 0; v < 3; ++v) {
		eflag[v] = vData[v].y | (vData[v].x << 8);
		edgesCrease[v] = vData[v].z / 255.0;
		edgesBweight[v] = vData[v].w / 255.0;
	}

	/* Face */
	vec4 fcol;
	if ((vData[0].x & FACE_ACTIVE) != 0)
		fcol = colorFaceSelect;
	else if ((vData[0].x & FACE_SELECTED) != 0)
		fcol = colorFaceSelect;
	else if ((vData[0].x & FACE_FREESTYLE) != 0)
		fcol = colorFaceFreestyle;
	else
		fcol = colorFace;

	/* Vertex */
	ssPos[0] = proj(pPos[0]);
	ssPos[1] = proj(pPos[1]);
	ssPos[2] = proj(pPos[2]);

#ifdef VERTEX_SELECTION
	vertex_color[0] = EDIT_MESH_vertex_color(vData[0].x).rgb;
	vertex_color[1] = EDIT_MESH_vertex_color(vData[1].x).rgb;
	vertex_color[2] = EDIT_MESH_vertex_color(vData[2].x).rgb;
#endif

#ifdef VERTEX_FACING
	/* Weird but some buggy AMD drivers need this. */
	v_facing[0] = vFacing[0];
	v_facing[1] = vFacing[1];
	v_facing[2] = vFacing[2];
#endif

	/* Remember that we are assuming the last vertex
	 * of a triangle is the provoking vertex (decide what flat attribs are). */

	if ((eflag[2] & EDGE_EXISTS) != 0) {
		/* Do 0 -> 1 edge strip */
		faceColor = vec4(fcol.rgb, 0.0);
		mask_edge_flag(0, eflag);

		vec2 fixvec = compute_fixvec(0);
		doVertexOfs(0, fixvec);
		doVertexOfs(1, fixvec);
	}

	doVertex(0);
	doVertex(1);

	/* Do face triangle */
	faceColor = fcol;
	flag = (isXray) ? ivec3(0) : eflag;
	doVertex(2);
	faceColor.a = 0.0; /* to not let face color bleed */

	if ((eflag[0] & EDGE_EXISTS) != 0) {
		/* Do 1 -> 2 edge strip */
		mask_edge_flag(1, eflag);

		vec2 fixvec = compute_fixvec(1);
		doVertexOfs(1, fixvec);
		doVertexOfs(2, fixvec);
	}
	EndPrimitive();

	if ((eflag[1] & EDGE_EXISTS) != 0) {
		/* Do 2 -> 0 edge strip */
		mask_edge_flag(2, eflag);
		doVertex(2);
		doVertex(0);

		vec2 fixvec = compute_fixvec(2);
		doVertexOfs(2, fixvec);
		doVertexOfs(0, fixvec);
		EndPrimitive();
	}
}
