
void main()
{
  vec4 sampled_color = texture(image, texCoord_interp);
  fragColor = vec4(sampled_color.r * shuffle.r + sampled_color.g * shuffle.g +
                   sampled_color.b * shuffle.b + sampled_color.a * shuffle.a) *
              color;
}
