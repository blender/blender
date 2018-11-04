
/* Solid Wirefram implementation
 * Mike Erwin, Clément Foucault */

uniform float faceAlphaMod;
uniform float edgeScale;
uniform bool isXray = false;

flat in vec3 edgesCrease;
flat in vec3 edgesBweight;
flat in vec4 faceColor;
flat in ivec3 flag;
#ifdef VERTEX_SELECTION
in vec3 vertexColor;
#endif

#ifdef EDGE_FIX
flat in vec2 ssPos[3];
#else
in vec3 barycentric;
#endif

#ifdef VERTEX_FACING
in float facing;
#endif

out vec4 FragColor;

/* Vertex flag is shifted and combined with the edge flag */
#define FACE_ACTIVE_   (FACE_ACTIVE << 8)

#define LARGE_EDGE_SIZE 2.15

/* Enough to visually fill gaps and not enough to mess the AA gradient too much. */
#define EDGE_FIX_ALPHA 0.67

void distToEdgesAndPoints(out vec3 edges, out vec3 points)
{
#ifdef EDGE_FIX
	vec2 e0 = normalize(ssPos[1] - ssPos[0] + 1e-8);
	vec2 e1 = normalize(ssPos[2] - ssPos[1] + 1e-8);
	vec2 e2 = normalize(ssPos[0] - ssPos[2] + 1e-8);
	e0 = vec2(-e0.y, e0.x);
	e1 = vec2(-e1.y, e1.x);
	e2 = vec2(-e2.y, e2.x);
	vec2 p0 = gl_FragCoord.xy - ssPos[0];
	vec2 p1 = gl_FragCoord.xy - ssPos[1];
	vec2 p2 = gl_FragCoord.xy - ssPos[2];
	edges.z = abs(dot(e0, p0));
	edges.x = abs(dot(e1, p1));
	edges.y = abs(dot(e2, p2));
#else
	vec3 dx = dFdx(barycentric);
	vec3 dy = dFdy(barycentric);
	/* per component derivative */
	vec2 d0 = vec2(dx.x, dy.x);
	vec2 d1 = vec2(dx.y, dy.y);
	vec2 d2 = vec2(dx.z, dy.z);
	vec3 d = vec3(length(d0), length(d1), length(d2));

	edges = abs(vec3(barycentric / d));
#endif

#if defined(VERTEX_SELECTION) && defined(EDGE_FIX)
	points.x = dot(p0, p0);
	points.y = dot(p1, p1);
	points.z = dot(p2, p2);
	points = sqrt(points);
#else
	points = vec3(1e10);
#endif
}

void colorDist(vec4 color, float dist)
{
	FragColor = (dist < 0) ? color : FragColor;
}

#ifdef ANTI_ALIASING
void colorDistEdge(vec4 color, float dist)
{
	FragColor.rgb *= FragColor.a;
	FragColor = mix(color, FragColor, clamp(dist, 0.0, 1.0));
	FragColor.rgb /= max(1e-8, FragColor.a);
}
#else
#define colorDistEdge colorDist
#endif

void main()
{
	vec3 e, p;
	distToEdgesAndPoints(e, p);

	/* Face */
	FragColor = faceColor;
	FragColor.a *= faceAlphaMod;

	/* Edges */
	float sizeEdgeFinal = sizeEdge * edgeScale;

	for (int v = 0; v < 3; ++v) {
		if ((flag[v] & EDGE_EXISTS) != 0) {
			/* Outer large edge */
			float largeEdge = e[v] - sizeEdgeFinal * LARGE_EDGE_SIZE;

			vec4 large_edge_color = EDIT_MESH_edge_color_outer(flag[v], (flag[0] & FACE_ACTIVE_) != 0, edgesCrease[v], edgesBweight[v]);
#ifdef EDGE_FIX
			large_edge_color *= isXray ? 1.0 : EDGE_FIX_ALPHA;
#endif
			if (large_edge_color.a != 0.0) {
				colorDistEdge(large_edge_color, largeEdge);
			}

			/* Inner thin edge */
			float innerEdge = e[v] - sizeEdgeFinal;
#ifdef ANTI_ALIASING
			innerEdge += 0.4;
#endif

#ifdef VERTEX_SELECTION
			vec4 inner_edge_color = vec4(vertexColor, 1.0);
#  ifdef EDGE_FIX
			inner_edge_color *= isXray ? 1.0 : EDGE_FIX_ALPHA;
#  endif
			colorDistEdge(vec4(vertexColor, 1.0), innerEdge);
#else
			vec4 inner_edge_color = EDIT_MESH_edge_color_inner(flag[v], (flag[0] & FACE_ACTIVE_) != 0);
#  ifdef EDGE_FIX
			inner_edge_color *= isXray ? 1.0 : EDGE_FIX_ALPHA;
#  endif
			colorDistEdge(inner_edge_color, innerEdge);
#endif
		}
	}

#if defined(VERTEX_SELECTION) && defined(EDGE_FIX)
	/* Points */
	for (int v = 0; v < 3; ++v) {
		if ((flag[v] & EDGE_VERTEX_EXISTS) == 0) {
			/* Leave as-is, no vertex. */
		}
		else {
			float size = p[v] - sizeVertex;

			vec4 point_color = colorVertex;
			point_color = ((flag[v] & EDGE_VERTEX_SELECTED) != 0) ? colorVertexSelect : point_color;
			point_color = ((flag[v] & EDGE_VERTEX_ACTIVE) != 0) ? vec4(colorEditMeshActive.xyz, 1.0) : point_color;
#  ifdef EDGE_FIX
			point_color *= isXray ? 1.0 : EDGE_FIX_ALPHA;
#  endif
			colorDist(point_color, size);
		}
	}
#endif

#ifdef VERTEX_FACING
	FragColor.a *= 1.0 - abs(facing) * 0.4;
#endif

	/* don't write depth if not opaque */
	if (FragColor.a == 0.0) discard;
}
