varying vec4 v_position;

void main()
{
	gl_Position = ftransform();
	v_position = gl_Position;
}
