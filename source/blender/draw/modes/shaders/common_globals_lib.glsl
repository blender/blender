
/* keep in sync with GlobalsUboStorage */
layout(std140) uniform globalsBlock {
	vec4 colorWire;
	vec4 colorWireEdit;
	vec4 colorActive;
	vec4 colorSelect;
	vec4 colorTransform;
	vec4 colorLibrarySelect;
	vec4 colorLibrary;
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
	vec4 colorEdgeFreestyle;
	vec4 colorFace;
	vec4 colorFaceSelect;
	vec4 colorFaceFreestyle;
	vec4 colorNormal;
	vec4 colorVNormal;
	vec4 colorLNormal;
	vec4 colorFaceDot;
	vec4 colorDeselect;
	vec4 colorOutline;
	vec4 colorLampNoAlpha;

	vec4 colorBackground;

	vec4 colorHandleFree;
	vec4 colorHandleAuto;
	vec4 colorHandleVect;
	vec4 colorHandleAlign;
	vec4 colorHandleAutoclamp;
	vec4 colorHandleSelFree;
	vec4 colorHandleSelAuto;
	vec4 colorHandleSelVect;
	vec4 colorHandleSelAlign;
	vec4 colorHandleSelAutoclamp;
	vec4 colorNurbUline;
	vec4 colorNurbSelUline;
	vec4 colorActiveSpline;

	vec4 colorBonePose;

	vec4 colorCurrentFrame;

	vec4 colorGrid;
	vec4 colorGridEmphasise;
	vec4 colorGridAxisX;
	vec4 colorGridAxisY;
	vec4 colorGridAxisZ;

	float sizeLampCenter;
	float sizeLampCircle;
	float sizeLampCircleShadow;
	float sizeVertex;
	float sizeEdge;
	float sizeEdgeFix;
	float sizeFaceDot;

	float gridDistance;
	float gridResolution;
	float gridSubdivisions;
	float gridScale;
};
