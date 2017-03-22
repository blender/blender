
/* Solid Wirefram implementation
 * Mike Erwin, Clément Foucault */

/* This shader follows the principles of
 * http://developer.download.nvidia.com/SDK/10/direct3d/Source/SolidWireframe/Doc/SolidWireframe.pdf */

layout(triangles) in;

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

/* these are the same for all vertices
 * and does not need interpolation */
flat out vec3 edgesCrease;
flat out vec3 edgesSharp;
flat out ivec3 flag;
flat out vec4 faceColor;
flat out int clipCase;
#ifdef VERTEX_SELECTION
smooth out vec3 vertexColor;
#endif

/* See fragment shader */
noperspective out vec4 eData1;
flat out vec4 eData2;


#define VERTEX_ACTIVE   (1 << 0)
#define VERTEX_SELECTED (1 << 1)

#define FACE_ACTIVE     (1 << 2)
#define FACE_SELECTED   (1 << 3)

/* Table 1. Triangle Projection Cases */
const ivec4 clipPointsIdx[6] = ivec4[6](
	ivec4(0, 1, 2, 2),
	ivec4(0, 2, 1, 1),
	ivec4(0, 0, 1, 2),
	ivec4(1, 2, 0, 0),
	ivec4(1, 1, 0, 2),
	ivec4(2, 2, 0, 1)
);

/* project to screen space */
vec2 proj(vec4 pos)
{
	return (0.5 * (pos.xy / pos.w) + 0.5) * viewportSize;
}

float dist(vec2 pos[3], vec2 vpos, int v)
{
	/* endpoints of opposite edge */
	vec2 e1 = pos[(v + 1) % 3];
	vec2 e2 = pos[(v + 2) % 3];
	/* Edge normalized vector */
	vec2 dir = normalize(e2 - e1);
	/* perpendicular to dir */
	vec2 orthogonal = vec2(-dir.y, dir.x);

	return abs(dot(vpos - e1, orthogonal));
}

vec3 getVertexColor(int v)
{
	if ((vData[v].x & (VERTEX_ACTIVE | VERTEX_SELECTED)) != 0)
		return colorEdgeSelect.rgb;
	else
		return colorWireEdit.rgb;
}

vec4 getClipData(vec2 pos[3], ivec2 vidx)
{
	vec2 A = pos[vidx.x];
	vec2 Adir = normalize(A - pos[vidx.y]);

	return vec4(A, Adir);
}

void doVertex(int v, vec4 pos)
{
#ifdef VERTEX_SELECTION
	vertexColor = getVertexColor(v);
#endif

	gl_Position = pos;

	EmitVertex();
}

void main()
{
	/* First we detect which case we are in */
	clipCase = 0;

	/* if perspective */
	if (ProjectionMatrix[3][3] == 0.0) {
		/* See Table 1. Triangle Projection Cases  */
		clipCase += int(pPos[0].z / pPos[0].w < -1 || vPos[0].z > 0.0) * 4;
		clipCase += int(pPos[1].z / pPos[1].w < -1 || vPos[1].z > 0.0) * 2;
		clipCase += int(pPos[2].z / pPos[2].w < -1 || vPos[2].z > 0.0) * 1;
	}

	/* If triangle is behind nearplane, early out */
	if (clipCase == 7)
		return;

	/* Edge */
	ivec3 eflag; vec3 ecrease, esharp;
	for (int v = 0; v < 3; ++v) {
		flag[v] = eflag[v] = vData[v].y | (vData[v].x << 8);
		edgesCrease[v] = ecrease[v] = vData[v].z / 255.0;
		edgesSharp[v] = esharp[v] = vData[v].w / 255.0;
	}

	/* Face */
	if ((vData[0].x & FACE_ACTIVE) != 0)
		faceColor = colorEditMeshActive;
	else if ((vData[0].x & FACE_SELECTED) != 0)
		faceColor = colorFaceSelect;
	else
		faceColor = colorFace;

	/* Vertex */
	vec2 pos[3] = vec2[3](proj(pPos[0]), proj(pPos[1]), proj(pPos[2]));

	/* Simple case : compute edge distances in geometry shader */
	if (clipCase == 0) {

		/* Packing screen positions and 2 distances */
		eData1 = vec4(0.0, 0.0, pos[2]);
		eData2 = vec4(pos[1], pos[0]);

		/* Only pass the first 2 distances */
		for (int v = 0; v < 2; ++v) {
			eData1[v] = dist(pos, pos[v], v);
			doVertex(v, pPos[v]);
			eData1[v] = 0.0;
		}

		/* and the last vertex */
		doVertex(2, pPos[2]);

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

			vec2 v1 = pos[i] + bias[i];
			vec2 v2 = pos[i1] + bias[i1];
			vec2 v3 = pos[i2] + bias[i2];

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

			/* Make it view independant */
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
		faceColor = vec4(0.0, 0.0, 0.0, 0.0);

		/* we don't want other edges : make them far*/
		eData1 = vec4(1e10);

		/* Start with the same last vertex to create a
		 * degenerate triangle in order to "create"
		 * a new triangle strip */
		for (int i = 2; i < 5; ++i) {
			int vbe = (i - 1) % 3;
			int vaf = (i + 1) % 3;
			int v = i % 3;

			/* Position of the "hidden" thrid vertex */
			eData1.zw = pos[vbe];

			doVertex(v, pPos[v]);
			doVertex(v, pPos[v] + vec4(fixvec[v], 0.0, 0.0));

			/* Now one triangle only shade one edge
			 * so we use the edge distance calculated
			 * in the fragment shader, the third edge;
			 * we do this because we need flat interp to
			 * draw a continuous triangle strip */
			eData2.xy = pos[vaf];
			eData2.zw = pos[v];
			flag[0] = (vData[v].x << 8);
			flag[1] = (vData[vaf].x << 8);
			flag[2] = eflag[vbe];
			edgesCrease[2] = ecrease[vbe];
			edgesSharp[2] = esharp[vbe];

			doVertex(vaf, pPos[vaf]);
			doVertex(vaf, pPos[vaf] + vec4(fixvecaf[v], 0.0, 0.0));

			/* corner vertices should not drax edges but draw point only */
			flag[2] = (vData[vbe].x << 8);
#ifdef VERTEX_SELECTION
			doVertex(vaf, pPos[vaf]);
			doVertex(vaf, pPos[vaf] + vec4(cornervec[vaf], 0.0, 0.0));
#endif
		}

		/* finish the loop strip */
		doVertex(2, pPos[2]);
		doVertex(2, pPos[2] + vec4(fixvec[2], 0.0, 0.0));
#endif
	}
	/* Harder case : compute visible edges vectors */
	else {
		ivec4 vindices = clipPointsIdx[clipCase - 1];

		eData1 = getClipData(pos, vindices.xz);
		eData2 = getClipData(pos, vindices.yw);

		for (int v = 0; v < 3; ++v)
			doVertex(v, pPos[v]);
	}

	EndPrimitive();
}
