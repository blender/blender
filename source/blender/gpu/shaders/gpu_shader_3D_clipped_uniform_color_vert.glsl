
uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelMatrix;
uniform vec4 ClipPlane;

#if __VERSION__ == 120
  attribute vec3 pos;
#else
  in vec3 pos;
#endif

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	gl_ClipDistance[0] = dot(ModelMatrix * vec4(pos, 1.0), ClipPlane);
}
