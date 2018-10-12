
/* Solid Wirefram implementation
 * Mike Erwin, Clément Foucault */

layout(triangles) in;

/* To fix the edge artifacts, we render
 * an outline strip around the screenspace
 * triangle. Order is important.
 * TODO diagram
 */
layout(triangle_strip, max_vertices=15) out;

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

out vec3 barycentric;
#ifdef VERTEX_SELECTION
out vec3 vertexColor;
#endif
#ifdef VERTEX_FACING
out float facing;
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
	barycentric = vec3(0.0);
	barycentric[v % 3] = 1.0;

	EmitVertex();
}

void doLoopStrip(int v, vec3 offset)
{
	doVertex(v);

	gl_Position.xyz += offset;
	barycentric = vec3(1.0);

	EmitVertex();
}

#ifdef ANTI_ALIASING
#define Z_OFFSET 0.008
#else
#define Z_OFFSET 0.0
#endif

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
	if ((vData[0].x & FACE_ACTIVE) != 0)
		faceColor = colorFaceSelect;
	else if ((vData[0].x & FACE_SELECTED) != 0)
		faceColor = colorFaceSelect;
	else if ((vData[0].x & FACE_FREESTYLE) != 0)
		faceColor = colorFaceFreestyle;
	else
		faceColor = colorFace;

	/* Vertex */
	vec2 ssPos[3];
	ssPos[0] = proj(pPos[0]);
	ssPos[1] = proj(pPos[1]);
	ssPos[2] = proj(pPos[2]);

	doVertex(0);
	doVertex(1);
	doVertex(2);

	vec2 fixvec[6];
	vec2 fixvecaf[6];

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

	/* to not let face color bleed */
	faceColor.a = 0.0;

	/* Start with the same last vertex to create a
	 * degenerate triangle in order to "create"
	 * a new triangle strip */
	for (int i = 2; i < 5; ++i) {
		int vbe = (i - 1) % 3;
		int vaf = (i + 1) % 3;
		int v = i % 3;

		doLoopStrip(v, vec3(fixvec[v], Z_OFFSET));

		/* Only shade the edge that we are currently drawing.
		 * (fix corner bleeding) */
		flag[vbe] |= (EDGE_EXISTS & eflag[vbe]);
		flag[vaf] &= ~EDGE_EXISTS;
		flag[v]   &= ~EDGE_EXISTS;
		doLoopStrip(vaf, vec3(fixvecaf[v], Z_OFFSET));

		EndPrimitive();
	}
}
