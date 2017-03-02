
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

uniform mat4 ModelViewProjectionMatrix;
uniform vec2 viewportSize;

in vec3 pos;
in ivec4 data;

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

void main()
{
	clipCase = 0;

	vec4 pPos = ModelViewProjectionMatrix * vec4(pos, 1.0);

	/* there is no face */
	faceColor = vec4(0.0);

	/* only verterx position 0 is used */
	eData1 = eData2 = vec4(1e10);
	eData2.zw = proj(pPos);

	flag = ivec3(0);
	flag[0] = (data.x << 8);

	gl_PointSize = sizeEdgeFix;
	gl_Position = pPos;
}
