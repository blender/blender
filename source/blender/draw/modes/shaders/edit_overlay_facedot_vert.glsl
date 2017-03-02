
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

in vec3 pos;
in int data;

flat out int isSelected;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	gl_PointSize = sizeFaceDot;
	isSelected = data;
}
