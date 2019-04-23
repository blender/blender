uniform int object_id = 0;

uniform vec3 materialDiffuseColor;
uniform float materialMetallic;
uniform float materialRoughness;

uniform sampler2D image;
uniform float ImageTransparencyCutoff = 0.1;
uniform bool imageNearest;

#ifdef NORMAL_VIEWPORT_PASS_ENABLED
in vec3 normal_viewport;
#endif

#ifdef V3D_SHADING_TEXTURE_COLOR
in vec2 uv_interp;
#endif
#ifdef V3D_SHADING_VERTEX_COLOR
in vec3 vertexColor;
#endif

#ifdef HAIR_SHADER
flat in float hair_rand;
#endif

#ifdef MATDATA_PASS_ENABLED
layout(location = 0) out vec4 materialData;
#endif
#ifdef OBJECT_ID_PASS_ENABLED
layout(location = 1) out uint objectId;
#endif
#ifdef NORMAL_VIEWPORT_PASS_ENABLED
layout(location = 2) out WB_Normal normalViewport;
#endif

void main()
{
#ifdef MATDATA_PASS_ENABLED
  float metallic, roughness;
  vec4 color;

#  if defined(V3D_SHADING_TEXTURE_COLOR)
  color = workbench_sample_texture(image, uv_interp, imageNearest);
  if (color.a < ImageTransparencyCutoff) {
    discard;
  }
#  elif defined(V3D_SHADING_VERTEX_COLOR)
  color.rgb = vertexColor;
#  else
  color.rgb = materialDiffuseColor;
#  endif

#  ifdef V3D_LIGHTING_MATCAP
  /* Encode front facing in metallic channel. */
  metallic = float(gl_FrontFacing);
  roughness = 0.0;
#  else
  metallic = materialMetallic;
  roughness = materialRoughness;
#  endif

#  ifdef HAIR_SHADER
  /* Add some variation to the hairs to avoid uniform look. */
  float hair_variation = hair_rand * 0.1;
  color = clamp(color - hair_variation, 0.0, 1.0);
  metallic = clamp(materialMetallic - hair_variation, 0.0, 1.0);
  roughness = clamp(materialRoughness - hair_variation, 0.0, 1.0);
#  endif

  materialData.rgb = color.rgb;
  materialData.a = workbench_float_pair_encode(roughness, metallic);
#endif /* MATDATA_PASS_ENABLED */

#ifdef OBJECT_ID_PASS_ENABLED
  objectId = uint(object_id);
#endif

#ifdef NORMAL_VIEWPORT_PASS_ENABLED
  vec3 n = (gl_FrontFacing) ? normal_viewport : -normal_viewport;
  n = normalize(n);
  normalViewport = workbench_normal_encode(n);
#endif
}
