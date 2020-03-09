
in vec4 uvcoordsvar;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 fragRevealage;

void main()
{
  /* Blend mode does the inversion. */
  fragRevealage = fragColor = vec4(1.0);
}
