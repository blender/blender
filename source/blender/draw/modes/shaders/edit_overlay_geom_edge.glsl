
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
	float sizeNormal;
	float sizeFaceDot;
};

layout(lines) in;
layout(triangle_strip, max_vertices=6) out;

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
	clipCase = 0;

	/* Face */
	faceColor = vec4(0.0);

	/* Proj Vertex */
	vec2 pos[2] = vec2[2](proj(pPos[0]), proj(pPos[1]));

	/* little optimization use a vec4 to vectorize
	 * following operations */
	vec4 dirs1, dirs2;

	/* Edge normalized vector */
	dirs1.xy = normalize(pos[1] - pos[0]);

	/* perpendicular to dir */
	dirs1.zw = vec2(-dirs1.y, dirs1.x);

	/* Make it view independant */
	dirs1 *= sizeEdgeFix / viewportSize.xyxy;

	dirs2 = dirs1;

	/* Perspective */
	if (ProjectionMatrix[3][3] == 0.0) {
		/* vPos[i].z is negative and we don't want
		 * our fixvec to be flipped */
		dirs1 *= -vPos[0].z;
		dirs2 *= -vPos[1].z;
	}

	/* Edge / Vert data */
	eData1 = vec4(1e10);
	eData2.zw = pos[0];
	eData2.xy = pos[1];
	flag[0] = (vData[0].x << 8);
	flag[1] = (vData[1].x << 8);
	flag[2] = 0;

	doVertex(0, pPos[0] + vec4(-dirs1.xy, 0.0, 0.0));
	doVertex(0, pPos[0] + vec4( dirs1.zw, 0.0, 0.0));
	doVertex(0, pPos[0] + vec4(-dirs1.zw, 0.0, 0.0));

	flag[2] = vData[0].y | (vData[0].x << 8);
	edgesCrease[2] = vData[0].z / 255.0;
	edgesSharp[2] = vData[0].w / 255.0;

	doVertex(1, pPos[1] + vec4( dirs2.zw, 0.0, 0.0));
	doVertex(1, pPos[1] + vec4(-dirs2.zw, 0.0, 0.0));

	flag[2] = 0;
	doVertex(1, pPos[1] + vec4( dirs2.xy, 0.0, 0.0));

	EndPrimitive();
}
