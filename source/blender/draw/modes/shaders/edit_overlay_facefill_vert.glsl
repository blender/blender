
uniform mat4 ModelViewProjectionMatrix;

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

in vec3 pos;
in ivec4 data;

flat out vec4 faceColor;
flat out int faceActive;

#define FACE_ACTIVE     (1 << 2)
#define FACE_SELECTED   (1 << 3)

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);

	if ((data.x & FACE_ACTIVE) != 0) {
		faceColor = colorEditMeshActive;
		faceActive = 1;
	}
	else if ((data.x & FACE_SELECTED) != 0) {
		faceColor = colorFaceSelect;
		faceActive = 0;
	}
	else {
		faceColor = colorFace;
		faceActive = 0;
	}
}
