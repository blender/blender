
void main()
{
  fragColor = texture(image, texCoord_interp);
  fragColor.a *= alpha;
}
