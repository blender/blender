

void main()
{
  vec4 tex_color = textureLod(imageTexture, uvcoordsvar.xy, mip);
  fragColor = tex_color;
}
