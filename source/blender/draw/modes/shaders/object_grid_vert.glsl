
/* Infinite grid
 * Clément Foucault */

uniform mat4 ViewProjectionOffsetMatrix;

in vec3 pos;

out vec3 wPos;

void main()
{
	vec3 realPos = pos * 1e3;
	gl_Position = ViewProjectionOffsetMatrix * vec4(realPos, 1.0);
	wPos = realPos;
}
