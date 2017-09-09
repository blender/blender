
layout(std140) uniform shadow_render_block {
	mat4 ShadowMatrix[6];
	mat4 FaceViewMatrix[6];
	vec4 lampPosition;
	float cubeTexelSize;
	float storedTexelSize;
	float nearClip;
	float farClip;
	int shadowSampleCount;
	float shadowInvSampleCount;
};

layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;

in vec4 vPos[];
flat in int face[];

#ifdef MESH_SHADER
in vec3 vNor[];
#endif

out vec3 worldPosition;
#ifdef MESH_SHADER
out vec3 viewPosition; /* Required. otherwise generate linking error. */
out vec3 worldNormal; /* Required. otherwise generate linking error. */
out vec3 viewNormal; /* Required. otherwise generate linking error. */
flat out int shFace;
#else
int shFace;
#endif

void main() {
	shFace = face[0];
	gl_Layer = shFace;

	for (int v = 0; v < 3; ++v) {
		gl_Position = ShadowMatrix[shFace] * vPos[v];
		worldPosition = vPos[v].xyz;
#ifdef MESH_SHADER
		worldNormal = vNor[v];
		viewPosition = (FaceViewMatrix[shFace] * vec4(worldPosition, 1.0)).xyz;
		viewNormal = (FaceViewMatrix[shFace] * vec4(worldNormal, 0.0)).xyz;
#ifdef ATTRIB
		pass_attrib(v);
#endif
#endif
		EmitVertex();
	}

	EndPrimitive();
}