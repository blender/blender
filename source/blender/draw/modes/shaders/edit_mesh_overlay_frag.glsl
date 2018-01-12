
/* Solid Wirefram implementation
 * Mike Erwin, Clément Foucault */

/* This shader follows the principles of
 * http://developer.download.nvidia.com/SDK/10/direct3d/Source/SolidWireframe/Doc/SolidWireframe.pdf */

/* This is not perfect. Only a subset of intel gpus are affected.
 * This fix have some performance impact.
 * TODO Refine the range to only affect GPUs. */

uniform float faceAlphaMod;

flat in vec3 edgesCrease;
flat in vec3 edgesBweight;
flat in vec4 faceColor;
flat in ivec3 flag;
flat in int clipCase;
#ifdef VERTEX_SELECTION
in vec3 vertexColor;
#endif
#ifdef VERTEX_FACING
in float facing;
#endif

/* We use a vec4[2] interface to pass edge data
 * (without fragmenting memory accesses)
 *
 * There are 2 cases :
 *
 * - Simple case : geometry shader return edge distances
 *   in the first 2 components of the first vec4.
 *   This needs noperspective interpolation.
 *   The rest is filled with vertex screen positions.
 *   eData2[0] actually contain v2
 *   eData2[1] actually contain v1
 *   eData2[2] actually contain v0
 *
 * - Hard case : two 2d edge corner are described by each
 *   vec4 as origin and direction. This is constant over
 *   the triangle and use to detect the correct case. */

noperspective in vec2 eData1;
flat in vec2 eData2[3];

out vec4 FragColor;

#define EDGE_EXISTS     (1 << 0)
#define EDGE_ACTIVE     (1 << 1)
#define EDGE_SELECTED   (1 << 2)
#define EDGE_SEAM       (1 << 3)
#define EDGE_SHARP      (1 << 4)
/* Vertex flag is shifted and combined with the edge flag */
#define VERTEX_ACTIVE   (1 << (0 + 8))
#define VERTEX_SELECTED (1 << (1 + 8))
#define FACE_ACTIVE     (1 << (2 + 8))

/* Style Parameters in pixel */

/* Array to retrieve vert/edge indices */
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

const vec4 stipple_matrix[4] = vec4[4](
	vec4(1.0, 0.0, 0.0, 0.0),
	vec4(0.0, 0.0, 0.0, 0.0),
	vec4(0.0, 0.0, 1.0, 0.0),
	vec4(0.0, 0.0, 0.0, 0.0)
);

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

#ifdef ANTI_ALIASING
void colorDistEdge(vec4 color, float dist)
{
	FragColor = mix(color, FragColor, clamp(dist, 0.0, 1.0));
}
#else
#define colorDistEdge colorDist
#endif

void main()
{
	vec3 e, p;

	/* Step 1 : Computing Distances */

	if (clipCase == 0) {
		e.xy = eData1;

		/* computing missing distance */
		vec2 dir = normalize(eData2[2] - eData2[1]);
		e.z = distToEdge(eData2[2], dir);

		p.x = distance(eData2[2], gl_FragCoord.xy);
		p.y = distance(eData2[1], gl_FragCoord.xy);
		p.z = distance(eData2[0], gl_FragCoord.xy);
	}
	else {
		ivec3 eidxs = clipEdgeIdx[clipCase - 1];
		ivec3 pidxs = clipPointIdx[clipCase - 1];

		e[eidxs.x] = distToEdge(eData1, eData2[0]);
		e[eidxs.y] = distToEdge(eData2[1], eData2[2]);

		/* Three edges visible cases */
		if (clipCase == 1 || clipCase == 2 || clipCase == 4) {
			e[eidxs.z] = distToEdge(eData1, normalize(eData2[1] - eData1));
			p[pidxs.y] = distance(eData2[1], gl_FragCoord.xy);
		}
		else {
			e[eidxs.z] = 1e10; /* off screen */
			p[pidxs.y] = 1e10; /* off screen */
		}

		p[pidxs.x] = distance(eData1, gl_FragCoord.xy);
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
			/* Outer large edge */
			float largeEdge = e[v] - sizeEdge * 3.0;

			vec4 large_edge_color = vec4(0.0);
			large_edge_color = ((flag[v] & EDGE_SHARP) != 0) ? colorEdgeSharp : large_edge_color;
			large_edge_color = (edgesCrease[v] > 0.0) ? vec4(colorEdgeCrease.rgb, edgesCrease[v]) : large_edge_color;
			large_edge_color = (edgesBweight[v] > 0.0) ? vec4(colorEdgeBWeight.rgb, edgesBweight[v]) : large_edge_color;
			large_edge_color = ((flag[v] & EDGE_SEAM) != 0) ? colorEdgeSeam : large_edge_color;

			if (large_edge_color.a != 0.0) {
				colorDistEdge(large_edge_color, largeEdge);
			}

			/* Inner thin edge */
			float innerEdge = e[v] - sizeEdge;
#ifdef ANTI_ALIASING
			innerEdge += 0.125;
#endif

#ifdef VERTEX_SELECTION
			colorDistEdge(vec4(vertexColor, 1.0), innerEdge);
#else
			vec4 inner_edge_color = colorWireEdit;
			inner_edge_color = ((flag[v] & EDGE_SELECTED) != 0) ? colorEdgeSelect : inner_edge_color;
			inner_edge_color = ((flag[v] & EDGE_ACTIVE) != 0) ? vec4(colorEditMeshActive.xyz, 1.0) : inner_edge_color;

			colorDistEdge(inner_edge_color, innerEdge);
#endif
		}
	}

	/* Points */
#ifdef VERTEX_SELECTION
	for (int v = 0; v < 3; ++v) {
		float size = p[v] - sizeVertex;

		vec4 point_color = colorVertex;
		point_color = ((flag[v] & VERTEX_SELECTED) != 0) ? colorVertexSelect : point_color;
		point_color = ((flag[v] & VERTEX_ACTIVE) != 0) ? vec4(colorEditMeshActive.xyz, 1.0) : point_color;

		colorDist(point_color, size);
	}
#endif

#ifdef VERTEX_FACING
	FragColor.a *= 1.0 - abs(facing) * 0.4;
#endif

	/* don't write depth if not opaque */
	if (FragColor.a == 0.0) discard;
}
