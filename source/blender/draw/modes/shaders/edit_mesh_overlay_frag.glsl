
/* Solid Wirefram implementation
 * Mike Erwin, Clément Foucault */

uniform float faceAlphaMod;
uniform float edgeScale;

flat in vec3 edgesCrease;
flat in vec3 edgesBweight;
flat in vec4 faceColor;
flat in ivec3 flag;
#ifdef VERTEX_SELECTION
in vec3 vertexColor;
#endif
#ifdef VERTEX_FACING
in float facing;
#endif

in vec3 barycentric;

out vec4 FragColor;

/* Vertex flag is shifted and combined with the edge flag */
#define FACE_ACTIVE_   (FACE_ACTIVE << 8)

#define LARGE_EDGE_SIZE 3.0

/* Style Parameters in pixel */

void distToEdgeAndPoint(vec2 dir, vec2 ori, out float edge, out float point)
{
	dir = normalize(dir.xy);
	dir = vec2(-dir.y, dir.x);
	vec2 of = gl_FragCoord.xy - ori;
	point = sqrt(dot(of, of));
	edge = abs(dot(dir, of));
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
	/* Step 1 : Computing Distances */
	vec3 dx = dFdx(barycentric);
	vec3 dy = dFdy(barycentric);
	vec3 d = vec3(
		length(vec2(dx.x, dy.x)),
		length(vec2(dx.y, dy.y)),
		length(vec2(dx.z, dy.z))
	);
	vec3 e = abs(vec3(barycentric / d));

	/* Step 2 : coloring (order dependent) */

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

			if (large_edge_color.a != 0.0) {
				colorDistEdge(large_edge_color, largeEdge);
			}

			/* Inner thin edge */
			float innerEdge = e[v] - sizeEdgeFinal;
#ifdef ANTI_ALIASING
			innerEdge += 0.4;
#endif

#ifdef VERTEX_SELECTION
			colorDistEdge(vec4(vertexColor, 1.0), innerEdge);
#else
			vec4 inner_edge_color = EDIT_MESH_edge_color_inner(flag[v], (flag[0] & FACE_ACTIVE_) != 0);
			colorDistEdge(inner_edge_color, innerEdge);
#endif
		}
	}

#if 0
	/* Points */
#ifdef VERTEX_SELECTION
	for (int v = 0; v < 3; ++v) {
		if ((flag[v] & EDGE_VERTEX_EXISTS) == 0) {
			/* Leave as-is, no vertex. */
		}
		else {
			float size = p[v] - sizeVertex;

			vec4 point_color = colorVertex;
			point_color = ((flag[v] & EDGE_VERTEX_SELECTED) != 0) ? colorVertexSelect : point_color;
			point_color = ((flag[v] & EDGE_VERTEX_ACTIVE) != 0) ? vec4(colorEditMeshActive.xyz, 1.0) : point_color;

			colorDist(point_color, size);
		}
	}
#endif
#endif

#ifdef VERTEX_FACING
	FragColor.a *= 1.0 - abs(facing) * 0.4;
#endif
	/* don't write depth if not opaque */
	if (FragColor.a == 0.0) discard;
}
