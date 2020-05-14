
flat in vec3 faceset_color;
in float mask_color;

out vec4 fragColor;

void main()
{
  fragColor = vec4(faceset_color * vec3(mask_color), 1.0);
}
