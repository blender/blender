uniform int object_id = 0;
layout(location=0) out uint objectId;
uniform float ImageTransparencyCutoff = 0.1;
#ifdef V3D_SHADING_TEXTURE_COLOR
uniform sampler2D image;

in vec2 uv_interp;
#endif

void main()
{
#ifdef V3D_SHADING_TEXTURE_COLOR
	vec4 diffuse_color = texture(image, uv_interp);
	if (diffuse_color.a < ImageTransparencyCutoff) {
		discard;
	}
#endif

	objectId = uint(object_id);
}
