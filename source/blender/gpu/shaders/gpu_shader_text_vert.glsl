
uniform mat4 ModelViewProjectionMatrix;

#if __VERSION__ == 120
  attribute vec2 pos;
  attribute vec2 texCoord;
  attribute vec4 color;
  flat varying vec4 color_flat;
  noperspective varying vec2 texCoord_interp;
#else
  in vec2 pos;
  in vec2 texCoord;
  in vec4 color;
  flat out vec4 color_flat;
  noperspective out vec2 texCoord_interp;
#endif

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 0.0, 1.0);

	color_flat = color;
	texCoord_interp = texCoord;
}
