
layout(location = 0) out uint objectId;

uniform float ImageTransparencyCutoff = 0.1;
#ifdef V3D_SHADING_TEXTURE_COLOR
uniform sampler2D image;

in vec2 uv_interp;
#endif

void main()
{
#ifdef V3D_SHADING_TEXTURE_COLOR
  if (texture(image, uv_interp).a < ImageTransparencyCutoff) {
    discard;
  }
#endif

  objectId = uint(resource_id + 1) & 0xFFu;
}
