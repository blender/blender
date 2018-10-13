
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

in vec4 vPos[];
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
#define Z_OFFSET 0.008
#else
#define Z_OFFSET 0.0
#endif

/* project to screen space */
vec2 proj(vec4 pos)
{
	return (0.5 * (pos.xy / pos.w) + 0.5) * viewportSize;
}

void doVertex(int v)
{
#ifdef VERTEX_SELECTION
	vertexColor = EDIT_MESH_vertex_color(vData[v].x).rgb;
#endif

#ifdef VERTEX_FACING
	facing = vFacing[v];
#endif
	gl_Position = pPos[v];

	EmitVertex();
}

void doVertexOfs(int v, vec2 fixvec)
{
#ifdef VERTEX_SELECTION
	vertexColor = EDIT_MESH_vertex_color(vData[v].x).rgb;
#endif

#ifdef VERTEX_FACING
	facing = vFacing[v];
#endif
	gl_Position = pPos[v];

	gl_Position.xyz += vec3(fixvec, Z_OFFSET);

	EmitVertex();
}

void mask_edge_flag(int v, ivec3 eflag)
{
	int vbe = (v + 2) % 3;
	int vaf = (v + 1) % 3;

	/* Only shade the edge that we are currently drawing.
	 * (fix corner bleeding) */
	flag[vbe] |= (EDGE_EXISTS & eflag[vbe]);
	flag[vaf] &= ~EDGE_EXISTS;
	flag[v]   &= ~EDGE_EXISTS;
}

void main()
{
	/* Edge */
	ivec3 eflag;
	for (int v = 0; v < 3; ++v) {
		flag[v] = eflag[v] = vData[v].y | (vData[v].x << 8);
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

	vec2 fixvec[3];
	vec2 fixvecaf[3];

	/* This fix the case when 2 vertices are perfectly aligned
	 * and corner vectors have nowhere to go.
	 * ie: length(cornervec[i]) == 0 */
	const float epsilon = 1e-2; /* in pixel so not that much */
	const vec2 bias[3] = vec2[3](
		vec2( epsilon,  epsilon),
		vec2(-epsilon,  epsilon),
		vec2(     0.0, -epsilon)
	);

	for (int i = 0; i < 3; ++i) {
		int i1 = (i + 1) % 3;
		int i2 = (i + 2) % 3;

		vec2 v1 = ssPos[i] + bias[i];
		vec2 v2 = ssPos[i1] + bias[i1];
		vec2 v3 = ssPos[i2] + bias[i2];

		/* Edge normalized vector */
		vec2 dir = normalize(v2 - v1);
		vec2 dir2 = normalize(v3 - v1);

		/* perpendicular to dir */
		vec2 perp = vec2(-dir.y, dir.x);

		/* Backface case */
		if (dot(perp, dir2) > 0) {
			perp = -perp;
		}

		/* Make it view independent */
		perp *= sizeEdgeFix / viewportSize;
		fixvec[i] = fixvecaf[i] = perp;

		/* Perspective */
		if (ProjectionMatrix[3][3] == 0.0) {
			/* vPos[i].z is negative and we don't want
			 * our fixvec to be flipped */
			fixvec[i] *= -vPos[i].z;
			fixvecaf[i] *= -vPos[i1].z;
		}
	}

	/* Remember that we are assuming the last vertex
	 * of a triangle is the provoking vertex (decide what flat attribs are). */

	/* Do 0 -> 1 edge strip */
	faceColor = vec4(fcol.rgb, 0.0);
	mask_edge_flag(0, eflag);
	doVertexOfs(0, fixvec[0]);
	doVertexOfs(1, fixvecaf[0]);
	doVertex(0);
	doVertex(1);
	/* Do face triangle */
	faceColor = fcol;
	flag = eflag;
	doVertex(2);
	faceColor.a = 0.0; /* to not let face color bleed */
	/* Do 1 -> 2 edge strip */
	mask_edge_flag(1, eflag);
	doVertexOfs(1, fixvec[1]);
	doVertexOfs(2, fixvecaf[1]);
	EndPrimitive();
	/* Do 2 -> 0 edge strip */
	mask_edge_flag(2, eflag);
	doVertex(2);
	doVertex(0);
	doVertexOfs(2, fixvec[2]);
	doVertexOfs(0, fixvecaf[2]);
	EndPrimitive();
}
