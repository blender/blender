
uniform mat4 ModelViewProjectionMatrix;

#if __VERSION__ == 120
  attribute vec3 pos;
  attribute float size;
#else
  in vec3 pos;
  in float size;
#endif

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	gl_PointSize = size;
}
