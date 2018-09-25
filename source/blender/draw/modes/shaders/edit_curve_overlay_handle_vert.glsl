
/* Draw Curve Handles */

uniform mat4 ModelViewProjectionMatrix;

in vec3 pos;
in int data;

flat out int vertFlag;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	vertFlag = data;
}
