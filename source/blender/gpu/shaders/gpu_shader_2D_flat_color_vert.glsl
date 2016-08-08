
#if __VERSION__ == 120
  attribute vec2 pos;
  attribute vec4 color;

  flat varying vec4 finalColor;
#else
  in vec2 pos;
  in vec4 color;

  flat out vec4 finalColor;
#endif

void main()
{
	gl_Position = gl_ModelViewProjectionMatrix * vec4(pos, 0.0, 1.0);
	finalColor = color;
}
