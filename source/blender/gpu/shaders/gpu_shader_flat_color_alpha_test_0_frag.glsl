
void main()
{
  if (finalColor.a > 0.0) {
    fragColor = finalColor;
  }
  else {
    discard;
  }
}
