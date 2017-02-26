
/* Solid Wirefram implementation
 * Mike Erwin, Clément Foucault */

/* This shader follows the principles of
 * http://developer.download.nvidia.com/SDK/10/direct3d/Source/SolidWireframe/Doc/SolidWireframe.pdf */

uniform mat4 ModelViewMatrix;
uniform mat4 ModelViewProjectionMatrix;

in vec3 pos;
in ivec4 data;

out vec4 vPos;
out vec4 pPos;
out ivec4 vData;

void main()
{
	vPos = ModelViewMatrix * vec4(pos, 1.0);
	pPos = ModelViewProjectionMatrix * vec4(pos, 1.0);
	vData = data;
}
