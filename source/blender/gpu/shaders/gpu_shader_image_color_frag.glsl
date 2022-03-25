#ifndef USE_GPU_SHADER_CREATE_INFO
in vec2 texCoord_interp;
out vec4 fragColor;

uniform vec4 color;
uniform sampler2D image;
#endif

void main()
{
  fragColor = texture(image, texCoord_interp) * color;
}
