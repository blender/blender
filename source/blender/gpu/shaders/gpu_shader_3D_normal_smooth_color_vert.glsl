
uniform mat4 ModelViewProjectionMatrix;
uniform mat3 NormalMatrix;

#if __VERSION__ == 120
  attribute vec3 pos;
  attribute vec3 nor;
  attribute vec4 color;

  varying vec4 finalColor;
  varying vec3 normal;
#else
  in vec3 pos;
  in vec3 nor;
  in vec4 color;

  out vec3 normal;
  out vec4 finalColor;
#endif

void main()
{
	normal = normalize(NormalMatrix * nor);
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	finalColor = color;
}
