
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

layout(points) in;
layout(triangle_strip, max_vertices=4) out;

uniform mat4 ProjectionMatrix;
uniform vec2 viewportSize;

in vec4 vPos[];
in vec4 pPos[];
in ivec4 vData[];

/* these are the same for all vertices
 * and does not need interpolation */
flat out ivec3 flag;
flat out vec4 faceColor;
flat out int clipCase;

/* See fragment shader */
noperspective out vec4 eData1;
flat out vec4 eData2;

/* project to screen space */
vec2 proj(vec4 pos)
{
	return (0.5 * (pos.xy / pos.w) + 0.5) * viewportSize;
}

void doVertex(vec4 pos)
{
	gl_Position = pos;
	EmitVertex();
}

void main()
{
	clipCase = 0;

	/* there is no face */
	faceColor = vec4(0.0);

	/* only verterx position 0 is used */
	eData1 = eData2 = vec4(1e10);
	flag = ivec3(0);

	vec2 dir = vec2(1.0) * sizeEdgeFix;
	/* Make it view independant */
	dir /= viewportSize;

	if (ProjectionMatrix[3][3] == 0.0) {
		dir *= -vPos[0].z;
	}

	eData2.zw = proj(pPos[0]);

	flag[0] = (vData[0].x << 8);

	/* Quad */
	doVertex(pPos[0] + vec4( dir.x,  dir.y, 0.0, 0.0));
	doVertex(pPos[0] + vec4(-dir.x,  dir.y, 0.0, 0.0));
	doVertex(pPos[0] + vec4( dir.x, -dir.y, 0.0, 0.0));
	doVertex(pPos[0] + vec4(-dir.x, -dir.y, 0.0, 0.0));

	EndPrimitive();
}
