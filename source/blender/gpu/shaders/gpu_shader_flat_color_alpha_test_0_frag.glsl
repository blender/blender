
flat in vec4 finalColor;
out vec4 fragColor;

void main()
{
  if (finalColor.a > 0.0) {
    fragColor = finalColor;
  }
  else {
    discard;
  }
}
