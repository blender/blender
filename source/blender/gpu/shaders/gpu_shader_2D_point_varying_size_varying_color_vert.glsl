
uniform mat4 ModelViewProjectionMatrix;

#if __VERSION__ == 120
  attribute vec2 pos;
  attribute float size;
  attribute vec4 color;
  varying vec4 finalColor;
#else
  in vec2 pos;
  in float size;
  in vec4 color;
  out vec4 finalColor;
#endif

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 0.0, 1.0);
	gl_PointSize = size;
	finalColor = color;
}
