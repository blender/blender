

in vec4 uvcoordsvar;
uniform sampler2D imageTexture;
uniform int mip;
out vec4 fragColor;

void main()
{
  vec4 tex_color = textureLod(imageTexture, uvcoordsvar.xy, mip);
  fragColor = tex_color;
}
