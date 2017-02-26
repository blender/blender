
/* Solid Wirefram implementation
 * Mike Erwin, Clément Foucault */

/* This shader follows the principles of
 * http://developer.download.nvidia.com/SDK/10/direct3d/Source/SolidWireframe/Doc/SolidWireframe.pdf */

layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;

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
// #ifdef VERTEX_SELECTION
smooth out vec3 vertexColor;
// #endif

/* See fragment shader */
noperspective out vec4 eData1;
flat out vec4 eData2;


#define VERTEX_ACTIVE	(1 << 0)
#define VERTEX_SELECTED	(1 << 1)

#define FACE_ACTIVE		(1 << 2)
#define FACE_SELECTED	(1 << 3)

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
vec2 proj(int v)
{
	vec4 pos = pPos[v];
	return (0.5 * (pos.xy / pos.w) + 0.5) * viewportSize;
}

float dist(vec2 pos[3], int v)
{
	/* current vertex position */
	vec2 vpos = pos[v];
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
	if ((vData[v].x & VERTEX_ACTIVE) != 0)
		return vec3(0.0, 1.0, 0.0);
	else if ((vData[v].x & VERTEX_SELECTED) != 0)
		return vec3(1.0, 0.0, 0.0);
	else
		return vec3(0.0, 0.0, 0.0);
}

vec4 getClipData(vec2 pos[3], ivec2 vidx)
{
	vec2 A = pos[vidx.x];
	vec2 Adir = normalize(A - pos[vidx.y]);

	return vec4(A, Adir);
}

void doVertex(int v)
{
#ifdef VERTEX_SELECTION
	vertexColor = getVertexColor(v);
#endif

	gl_Position = pPos[v];

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
	for (int v = 0; v < 3; ++v) {
		flag[v] = vData[v].y;// | (vData[v].x << 8);
		edgesCrease[v] = vData[v].z / 255.0;
		edgesSharp[v] = vData[v].w / 255.0;
	}

	/* Face */
	if ((vData[0].x & FACE_ACTIVE) != 0) {
		faceColor = vec4(0.1, 1.0, 0.0, 0.2);
	}
	else if ((vData[0].x & FACE_SELECTED) != 0) {
		faceColor = vec4(1.0, 0.2, 0.0, 0.2);
	}
	else {
		faceColor = vec4(0.0, 0.0, 0.0, 0.2);
	}

	/* Vertex */
	vec2 pos[3] = vec2[3](proj(0), proj(1), proj(2));

	/* Simple case : compute edge distances in geometry shader */
	if (clipCase == 0) {

		/* Packing screen positions and 2 distances */
		eData1 = vec4(0.0, 0.0, pos[0]);
		eData2 = vec4(pos[1], pos[2]);

		/* Only pass the first 2 distances */
		for (int v = 0; v < 2; ++v) {
			eData1[v] = dist(pos, v);
			doVertex(v);
			eData1[v] = 0.0;
		}

		/* and the last vertex
		doVertex(2);
	}
	/* Harder case : compute visible edges vectors */
	else {
		ivec4 vindices = clipPointsIdx[clipCase - 1];

		eData1 = getClipData(pos, vindices.xz);
		eData2 = getClipData(pos, vindices.yw);

		for (int v = 0; v < 3; ++v)
			doVertex(v);
	}

	EndPrimitive();
}
