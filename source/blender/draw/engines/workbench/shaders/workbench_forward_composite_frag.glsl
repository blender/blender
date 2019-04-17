out vec4 fragColor;

uniform usampler2D objectId;
uniform sampler2D transparentAccum;
uniform sampler2D transparentRevealage;
uniform vec2 invertedViewportSize;

#ifndef ALPHA_COMPOSITE
layout(std140) uniform world_block
{
  WorldData world_data;
};
#endif

/* TODO: Bypass the whole shader if there is no xray pass and no outline pass. */
void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);
  vec2 uv_viewport = gl_FragCoord.xy * invertedViewportSize;

  /* Listing 4 */
  vec4 trans_accum = texelFetch(transparentAccum, texel, 0);
  float trans_revealage = trans_accum.a;
  trans_accum.a = texelFetch(transparentRevealage, texel, 0).r;

  vec3 trans_color = trans_accum.rgb / clamp(trans_accum.a, 1e-4, 5e4);

#ifndef ALPHA_COMPOSITE
  vec3 bg_color = background_color(world_data, uv_viewport.y);

  bg_color = (world_data.background_alpha == 0.0) ? trans_color : bg_color;
  vec4 color = mix(
      vec4(trans_color, 1.0), vec4(bg_color, world_data.background_alpha), trans_revealage);

#  ifdef V3D_SHADING_OBJECT_OUTLINE
  uint object_id = texelFetch(objectId, texel, 0).r;
  float outline = calculate_object_outline(objectId, texel, object_id);
  color = mix(vec4(world_data.object_outline_color.rgb, 1.0), color, outline);
#  endif

  fragColor = color;
#else
  fragColor = vec4(trans_color, 1.0 - trans_revealage);
#endif
}
