
uniform usampler2D objectId;

uniform vec2 invertedViewportSize;

out vec4 fragColor;

layout(std140) uniform world_block
{
  WorldData world_data;
};

void main()
{
  vec2 uv_viewport = gl_FragCoord.xy * invertedViewportSize;

#ifndef V3D_SHADING_OBJECT_OUTLINE

  fragColor = vec4(0.0);

#else /* !V3D_SHADING_OBJECT_OUTLINE */

  ivec2 texel = ivec2(gl_FragCoord.xy);
  uint object_id = texelFetch(objectId, texel, 0).r;
  float object_outline = calculate_object_outline(objectId, texel, object_id);

  fragColor = vec4(world_data.object_outline_color.rgb, 1.0) * (1.0 - object_outline);

#endif /* !V3D_SHADING_OBJECT_OUTLINE */
}
