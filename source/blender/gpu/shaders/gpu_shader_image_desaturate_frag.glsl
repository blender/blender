#ifndef USE_GPU_SHADER_CREATE_INFO
uniform float factor;
in vec2 texCoord_interp;
out vec4 fragColor;

uniform vec4 color;
uniform sampler2D image;
#endif

void main()
{
  vec4 tex = texture(image, texCoord_interp);
  tex.rgb = ((0.3333333 * factor) * vec3(tex.r + tex.g + tex.b)) + (tex.rgb * (1.0 - factor));
  fragColor = tex * color;
}
