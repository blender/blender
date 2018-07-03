
/* Solid Wirefram implementation
 * Mike Erwin, Clément Foucault */

/* This shader follows the principles of
 * http://developer.download.nvidia.com/SDK/10/direct3d/Source/SolidWireframe/Doc/SolidWireframe.pdf */

layout(triangles) in;

/* This is not perfect. Only a subset of intel gpus are affected.
 * This fix have some performance impact.
 * TODO Refine the range to only affect GPUs. */

#ifdef EDGE_FIX
/* To fix the edge artifacts, we render
 * an outline strip around the screenspace
 * triangle. Order is important.
 * TODO diagram
 */

#ifdef VERTEX_SELECTION
layout(triangle_strip, max_vertices=23) out;
#else
layout(triangle_strip, max_vertices=17) out;
#endif
#else
layout(triangle_strip, max_vertices=3) out;
#endif

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
flat out int clipCase;
#ifdef VERTEX_SELECTION
out vec3 vertexColor;
#endif
#ifdef VERTEX_FACING
out float facing;
#endif

/* See fragment shader */
flat out vec2 ssPos[3];

#define FACE_ACTIVE     (1 << 2)
#define FACE_SELECTED   (1 << 3)

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

void doLoopStrip(int v, vec3 offset)
{
	doVertex(v);

	gl_Position.xyz += offset;

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
	else
		faceColor = colorFace;

	/* Vertex */
	ssPos[0] = proj(pPos[0]);
	ssPos[1] = proj(pPos[1]);
	ssPos[2] = proj(pPos[2]);

	doVertex(0);
	doVertex(1);
	doVertex(2);

#ifdef EDGE_FIX
	vec2 fixvec[6];
	vec2 fixvecaf[6];
	vec2 cornervec[3];

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

		cornervec[i] = -normalize(dir + dir2);

		/* perpendicular to dir */
		vec2 perp = vec2(-dir.y, dir.x);

		/* Backface case */
		if (dot(perp, dir2) > 0) {
			perp = -perp;
		}

		/* Make it view independent */
		perp *= sizeEdgeFix / viewportSize;
		cornervec[i] *= sizeEdgeFix / viewportSize;
		fixvec[i] = fixvecaf[i] = perp;

		/* Perspective */
		if (ProjectionMatrix[3][3] == 0.0) {
			/* vPos[i].z is negative and we don't want
			 * our fixvec to be flipped */
			fixvec[i] *= -vPos[i].z;
			fixvecaf[i] *= -vPos[i1].z;
			cornervec[i] *= -vPos[i].z;
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

		/* corner vertices should not draw edges but draw point only */
		flag[vbe] &= ~EDGE_EXISTS;
#ifdef VERTEX_SELECTION
		doLoopStrip(vaf, vec3(cornervec[vaf], Z_OFFSET));
#endif
	}

	/* finish the loop strip */
	doLoopStrip(2, vec3(fixvec[2], Z_OFFSET));
#endif

	EndPrimitive();
}
