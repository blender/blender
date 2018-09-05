
/* Solid Wirefram implementation
 * Mike Erwin, Clément Foucault */

/* This shader follows the principles of
 * http://developer.download.nvidia.com/SDK/10/direct3d/Source/SolidWireframe/Doc/SolidWireframe.pdf */

uniform mat4 ModelViewMatrix;
uniform mat4 ModelViewProjectionMatrix;
uniform ivec4 dataMask = ivec4(0xFF);

in vec3 pos;
in ivec4 data;

out vec4 vPos;
out vec4 pPos;
out ivec4 vData;

#ifdef VERTEX_FACING
uniform mat4 ProjectionMatrix;
uniform mat3 NormalMatrix;

in vec3 vnor;
out float vFacing;
#endif

void main()
{
	vPos = ModelViewMatrix * vec4(pos, 1.0);
	pPos = ModelViewProjectionMatrix * vec4(pos, 1.0);
	vData = data & dataMask;
#ifdef VERTEX_FACING
	vec3 view_normal = normalize(NormalMatrix * vnor);
	vec3 view_vec = (ProjectionMatrix[3][3] == 0.0)
		? normalize(vPos.xyz)
		: vec3(0.0, 0.0, 1.0);
	vFacing = dot(view_vec, view_normal);
#endif
}
