
uniform mat4 ModelViewProjectionMatrix;

#if __VERSION__ == 120
  attribute vec3 pos;
  attribute vec4 color;

  flat varying vec4 finalColor;
#else
  in vec3 pos;
  in vec4 color;

  flat out vec4 finalColor;
#endif

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	finalColor = color;
}
