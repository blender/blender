
/* Solid Wirefram implementation
 * Mike Erwin, Clément Foucault */

/* This shader follows the principles of
 * http://developer.download.nvidia.com/SDK/10/direct3d/Source/SolidWireframe/Doc/SolidWireframe.pdf */

/* keep in sync with GlobalsUboStorage */
layout(std140) uniform globalsBlock {
	vec4 colorWire;
	vec4 colorWireEdit;
	vec4 colorActive;
	vec4 colorSelect;
	vec4 colorTransform;
	vec4 colorGroupActive;
	vec4 colorGroup;
	vec4 colorLamp;
	vec4 colorSpeaker;
	vec4 colorCamera;
	vec4 colorEmpty;
	vec4 colorVertex;
	vec4 colorVertexSelect;
	vec4 colorEditMeshActive;
	vec4 colorEdgeSelect;
	vec4 colorEdgeSeam;
	vec4 colorEdgeSharp;
	vec4 colorEdgeCrease;
	vec4 colorEdgeBWeight;
	vec4 colorEdgeFaceSelect;
	vec4 colorFace;
	vec4 colorFaceSelect;
	vec4 colorNormal;
	vec4 colorVNormal;
	vec4 colorLNormal;
	vec4 colorFaceDot;

	vec4 colorDeselect;
	vec4 colorOutline;
	vec4 colorLampNoAlpha;

	float sizeLampCenter;
	float sizeLampCircle;
	float sizeLampCircleShadow;
	float sizeVertex;
	float sizeEdge;
	float sizeEdgeFix;
	float sizeFaceDot;
};

uniform float faceAlphaMod;

flat in vec3 edgesCrease;
flat in vec3 edgesBweight;
flat in ivec3 flag;
flat in vec4 faceColor;
flat in int clipCase;
#ifdef VERTEX_SELECTION
smooth in vec3 vertexColor;
#endif

/* We use a vec4[2] interface to pass edge data
 * (without fragmenting memory accesses)
 *
 * There is 2 cases :
 *
 * - Simple case : geometry shader return edge distances
 *   in the first 2 components of the first vec4.
 *   This needs noperspective interpolation.
 *   The rest is filled with vertex screen positions.
 *   eData1.zw actually contain v2
 *   eData2.xy actually contain v1
 *   eData2.zw actually contain v0
 *
 * - Hard case : two 2d edge corner are described by each
 *   vec4 as origin and direction. This is constant over
 *   the triangle and use to detect the correct case. */

noperspective in vec4 eData1;
flat in vec4 eData2;

out vec4 FragColor;

#define EDGE_EXISTS		(1 << 0)
#define EDGE_ACTIVE		(1 << 1)
#define EDGE_SELECTED	(1 << 2)
#define EDGE_SEAM		(1 << 3)
#define EDGE_SHARP		(1 << 4)
/* Vertex flag is shifted and combined with the edge flag */
#define VERTEX_ACTIVE	(1 << (0 + 8))
#define VERTEX_SELECTED	(1 << (1 + 8))
#define FACE_ACTIVE		(1 << (2 + 8))

/* Style Parameters in pixel */

/* Array to retreive vert/edge indices */
const ivec3 clipEdgeIdx[6] = ivec3[6](
	ivec3(1, 0, 2),
	ivec3(2, 0, 1),
	ivec3(2, 1, 0),
	ivec3(2, 1, 0),
	ivec3(2, 0, 1),
	ivec3(1, 0, 2)
);

const ivec3 clipPointIdx[6] = ivec3[6](
	ivec3(0, 1, 2),
	ivec3(0, 2, 1),
	ivec3(0, 2, 1),
	ivec3(1, 2, 0),
	ivec3(1, 2, 0),
	ivec3(2, 1, 0)
);

const mat4 stipple_matrix = mat4(vec4(1.0, 0.0, 0.0, 0.0),
                                 vec4(0.0, 0.0, 0.0, 0.0),
                                 vec4(0.0, 0.0, 1.0, 0.0),
                                 vec4(0.0, 0.0, 0.0, 0.0));

void colorDist(vec4 color, float dist)
{
	FragColor = (dist < 0) ? color : FragColor;
}

float distToEdge(vec2 o, vec2 dir)
{
	vec2 af = gl_FragCoord.xy - o;
	float daf = dot(dir, af);
	return sqrt(abs(dot(af, af) - daf * daf));
}

void main()
{
	vec3 e, p;

	/* Step 1 : Computing Distances */

	if (clipCase == 0) {
		e.xy = eData1.xy;

		/* computing missing distance */
		vec2 dir = normalize(eData2.zw - eData2.xy);
		e.z = distToEdge(eData2.zw, dir);

		p.x = distance(eData2.zw, gl_FragCoord.xy);
		p.y = distance(eData2.xy, gl_FragCoord.xy);
		p.z = distance(eData1.zw, gl_FragCoord.xy);
	}
	else {
		ivec3 eidxs = clipEdgeIdx[clipCase - 1];
		ivec3 pidxs = clipPointIdx[clipCase - 1];

		e[eidxs.x] = distToEdge(eData1.xy, eData1.zw);
		e[eidxs.y] = distToEdge(eData2.xy, eData2.zw);

		/* Three edges visible cases */
		if (clipCase == 1 || clipCase == 2 || clipCase == 4) {
			e[eidxs.z] = distToEdge(eData1.xy, normalize(eData2.xy - eData1.xy));
			p[pidxs.y] = distance(eData2.xy, gl_FragCoord.xy);
		}
		else {
			e[eidxs.z] = 1e10; /* off screen */
			p[pidxs.y] = 1e10; /* off screen */
		}

		p[pidxs.x] = distance(eData1.xy, gl_FragCoord.xy);
		p[pidxs.z] = 1e10; /* off screen */
	}

	/* Step 2 : coloring (order dependant) */

	/* First */
	FragColor = faceColor;

	if ((flag[0] & FACE_ACTIVE) != 0) {
		int x = int(gl_FragCoord.x) & 0x3; /* mod 4 */
		int y = int(gl_FragCoord.y) & 0x3; /* mod 4 */
		FragColor *= stipple_matrix[x][y];
	}
	else {
		FragColor.a *= faceAlphaMod;
	}

	/* Edges */
	for (int v = 0; v < 3; ++v) {
		if ((flag[v] & EDGE_EXISTS) != 0) {
			float largeEdge = e[v] - sizeEdge * 2.0;
			float innerEdge = e[v] - sizeEdge;

			if ((flag[v] & EDGE_SEAM) != 0)
				colorDist(colorEdgeSeam, largeEdge);
			else if (edgesBweight[v] > 0.0)
				colorDist(vec4(colorEdgeBWeight.rgb, edgesBweight[v]), largeEdge);
			else if (edgesCrease[v] > 0.0)
				colorDist(vec4(colorEdgeCrease.rgb, edgesCrease[v]), largeEdge);
			else if ((flag[v] & EDGE_SHARP) != 0)
				colorDist(colorEdgeSharp, largeEdge);
#ifndef VERTEX_SELECTION
			else
				colorDist(colorWireEdit, innerEdge);

			if ((flag[v] & EDGE_ACTIVE) != 0)
				colorDist(vec4(colorEditMeshActive.xyz, 1.0), innerEdge);
			else if ((flag[v] & EDGE_SELECTED) != 0)
				colorDist(colorEdgeSelect, innerEdge);
#else
			colorDist(vec4(vertexColor, 1.0), innerEdge);
#endif
		}
	}

	/* Points */
#ifdef VERTEX_SELECTION
	for (int v = 0; v < 3; ++v) {
		float size = p[v] - sizeVertex;

		if ((flag[v] & VERTEX_ACTIVE) != 0)
			colorDist(vec4(colorEditMeshActive.xyz, 1.0), size);
		else if ((flag[v] & VERTEX_SELECTED) != 0)
			colorDist(colorVertexSelect, size);
		else
			colorDist(colorVertex, size);
	}
#endif

	/* don't write depth if not opaque */
	if (FragColor.a == 0.0)	discard;
}
