#ifndef USE_GPU_SHADER_CREATE_INFO
in vec2 texCoord_interp;
out vec4 fragColor;

uniform sampler2D image;
uniform vec4 color;
uniform vec4 shuffle;
#endif

void main()
{
  vec4 sampled_color = texture(image, texCoord_interp);
  fragColor = vec4(sampled_color.r * shuffle.r + sampled_color.g * shuffle.g +
                   sampled_color.b * shuffle.b + sampled_color.a * shuffle.a) *
              color;
}
