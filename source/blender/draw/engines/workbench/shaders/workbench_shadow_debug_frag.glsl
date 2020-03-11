
out vec4 fragColor;

layout(location = 0) out vec4 materialData;
layout(location = 1) out vec4 normalData;
layout(location = 2) out uint objectId;

void main()
{
  const float a = 0.25;
#ifdef SHADOW_PASS
  materialData.rgb = gl_FrontFacing ? vec3(a, -a, 0.0) : vec3(-a, a, 0.0);
#else
  materialData.rgb = gl_FrontFacing ? vec3(a, a, -a) : vec3(-a, -a, a);
#endif
  materialData.a = 0.0;
  normalData = vec4(0.0);
  objectId = 0u;
}
