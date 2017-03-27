
uniform mat4 ModelViewProjectionMatrix;

#if __VERSION__ == 120
  varying vec4 v_position;
#else
  out vec4 v_position;
#endif

void main()
{
	gl_Position = ModelViewProjectionMatrix * gl_Vertex;
	v_position = gl_Position;
}
