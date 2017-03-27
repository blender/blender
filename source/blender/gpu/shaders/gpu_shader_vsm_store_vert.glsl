
uniform mat4 ModelViewProjectionMatrix;
varying vec4 v_position;

void main()
{
	gl_Position = ModelViewProjectionMatrix * v_position;
	v_position = gl_Position;
}
