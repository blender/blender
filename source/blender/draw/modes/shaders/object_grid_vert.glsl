
/* Infinite grid
 * Clément Foucault */

uniform mat4 ViewProjectionMatrix;
uniform mat4 ViewMatrix;

in vec3 pos;

out vec3 wPos;
out float viewDist;

void main()
{
	vec3 realPos = pos * 1e3;
	gl_Position = ViewProjectionMatrix * vec4(realPos, 1.0);
	viewDist = -(ViewMatrix * vec4(realPos, 1.0)).z;
	wPos = realPos;
}
