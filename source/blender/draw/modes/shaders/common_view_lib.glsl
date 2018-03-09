/* keep in sync with DRWManager.view_data */
layout(std140) uniform viewBlock {
	/* Same order as DRWViewportMatrixType */
	mat4 ViewProjectionMatrix;
	mat4 ViewProjectionMatrixInverse;
	mat4 ViewMatrix;
	mat4 ViewMatrixInverse;
	mat4 ProjectionMatrix;
	mat4 ProjectionMatrixInverse;

	vec4 CameraTexCoFactors;

	vec4 clipPlanes[2];
};
