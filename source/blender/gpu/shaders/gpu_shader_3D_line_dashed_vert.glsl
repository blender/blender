
// Draw dashed lines, perforated in screen space.

uniform mat4 ModelViewProjectionMatrix;

in vec3 pos;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
}
