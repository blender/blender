
uniform sampler2D colorBuffer;

in vec4 uvcoordsvar;

out vec4 fragColor;

void main()
{
  fragColor = texture(colorBuffer, uvcoordsvar.st);
}
