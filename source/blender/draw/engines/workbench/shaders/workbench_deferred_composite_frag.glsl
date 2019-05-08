out vec4 fragColor;

uniform usampler2D objectId;
uniform sampler2D materialBuffer;
uniform sampler2D normalBuffer;
/* normalBuffer contains viewport normals */
uniform sampler2D cavityBuffer;
uniform sampler2D matcapImage;

uniform vec2 invertedViewportSize;
uniform vec4 viewvecs[3];
uniform float shadowMultiplier;
uniform float lightMultiplier;
uniform float shadowShift = 0.1;
uniform float shadowFocus = 1.0;

uniform vec3 materialSingleColor;

layout(std140) uniform world_block
{
  WorldData world_data;
};

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);
  vec2 uv_viewport = gl_FragCoord.xy * invertedViewportSize;

  float roughness, metallic;
  vec3 base_color;

#ifndef MATDATA_PASS_ENABLED
  base_color = materialSingleColor;
  metallic = 0.0;
  roughness = 0.5;
#else
  vec4 material_data = texelFetch(materialBuffer, texel, 0);
  base_color = material_data.rgb;
  workbench_float_pair_decode(material_data.a, roughness, metallic);
#endif

/* Do we need normals */
#ifdef NORMAL_VIEWPORT_PASS_ENABLED
  vec3 normal_viewport = workbench_normal_decode(texelFetch(normalBuffer, texel, 0).rg);
#endif

  vec3 I_vs = view_vector_from_screen_uv(uv_viewport, viewvecs, ProjectionMatrix);

  /* -------- SHADING --------- */
#ifdef V3D_LIGHTING_FLAT
  vec3 shaded_color = base_color;

#elif defined(V3D_LIGHTING_MATCAP)
  /* When using matcaps, the metallic is the backface sign. */
  normal_viewport = (metallic > 0.0) ? normal_viewport : -normal_viewport;
  bool flipped = world_data.matcap_orientation != 0;
  vec2 matcap_uv = matcap_uv_compute(I_vs, normal_viewport, flipped);
  vec3 matcap = textureLod(matcapImage, matcap_uv, 0.0).rgb;
  vec3 shaded_color = matcap * base_color;

#elif defined(V3D_LIGHTING_STUDIO)

#  ifdef V3D_SHADING_SPECULAR_HIGHLIGHT
  vec3 specular_color = mix(vec3(0.05), base_color, metallic);
  vec3 diffuse_color = mix(base_color, vec3(0.0), metallic);
#  else
  roughness = 0.0;
  vec3 specular_color = vec3(0.0);
  vec3 diffuse_color = base_color;
#  endif

  vec3 shaded_color = get_world_lighting(
      world_data, diffuse_color, specular_color, roughness, normal_viewport, I_vs);
#endif

  /* -------- POST EFFECTS --------- */
#ifdef WB_CAVITY
  /* Using UNORM texture so decompress the range */
  shaded_color *= texelFetch(cavityBuffer, texel, 0).r * CAVITY_BUFFER_RANGE;
#endif

#ifdef V3D_SHADING_SHADOW
  float light_factor = -dot(normal_viewport, world_data.shadow_direction_vs.xyz);
  float shadow_mix = smoothstep(shadowFocus, shadowShift, light_factor);
  shaded_color *= mix(lightMultiplier, shadowMultiplier, shadow_mix);
#endif

#ifdef V3D_SHADING_OBJECT_OUTLINE
  uint object_id = texelFetch(objectId, texel, 0).r;
  float object_outline = calculate_object_outline(objectId, texel, object_id);
  shaded_color = mix(world_data.object_outline_color.rgb, shaded_color, object_outline);
#endif

  fragColor = vec4(shaded_color, 1.0);
}
