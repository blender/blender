
#pragma BLENDER_REQUIRE(effect_dof_lib.glsl)

uniform vec2 targetTexelSize;
uniform int spritePerRow;
uniform vec2 bokehAnisotropy;

uniform sampler2D colorBuffer;
uniform sampler2D cocBuffer;

/* Scatter pass, calculate a triangle covering the CoC.
 * We render to a half resolution target with double width so we can
 * separate near and far fields. We also generate only one triangle per group of 4 pixels
 * to limit overdraw. */

flat out vec4 color1;
flat out vec4 color2;
flat out vec4 color3;
flat out vec4 color4;
flat out vec4 weights;
flat out vec4 cocs;
flat out vec2 spritepos;
flat out float spritesize;

/* Load 4 Circle of confusion values. texel_co is centered around the 4 taps. */
vec4 fetch_cocs(vec2 texel_co)
{
  /* TODO(fclem) The textureGather(sampler, co, comp) variant isn't here on some implementations.*/
#if 0  // GPU_ARB_texture_gather
  vec2 uvs = texel_co / vec2(textureSize(cocBuffer, 0));
  /* Reminder: Samples order is CW starting from top left. */
  cocs = textureGather(cocBuffer, uvs, isForegroundPass ? 0 : 1);
#else
  ivec2 texel = ivec2(texel_co - 0.5);
  vec4 cocs;
  cocs.x = texelFetchOffset(cocBuffer, texel, 0, ivec2(0, 1)).r;
  cocs.y = texelFetchOffset(cocBuffer, texel, 0, ivec2(1, 1)).r;
  cocs.z = texelFetchOffset(cocBuffer, texel, 0, ivec2(1, 0)).r;
  cocs.w = texelFetchOffset(cocBuffer, texel, 0, ivec2(0, 0)).r;
#endif

#ifdef DOF_FOREGROUND_PASS
  cocs *= -1.0;
#endif

  cocs = max(vec4(0.0), cocs);
  /* We are scattering at half resolution, so divide CoC by 2. */
  return cocs * 0.5;
}

void vertex_discard()
{
  /* Don't produce any fragments */
  gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}

void main()
{
  ivec2 tex_size = textureSize(cocBuffer, 0);

  int t_id = gl_VertexID / 3; /* Triangle Id */

  /* Some math to get the target pixel. */
  ivec2 texelco = ivec2(t_id % spritePerRow, t_id / spritePerRow) * 2;

  /* Center sprite around the 4 texture taps. */
  spritepos = vec2(texelco) + 1.0;

  cocs = fetch_cocs(spritepos);

  /* Early out from local CoC radius. */
  if (all(lessThan(cocs, vec4(0.5)))) {
    vertex_discard();
    return;
  }

  vec2 input_texel_size = 1.0 / vec2(tex_size);
  vec2 quad_center = spritepos * input_texel_size;
  vec4 colors[4];
  bool no_color = true;
  for (int i = 0; i < 4; i++) {
    vec2 sample_uv = quad_center + quad_offsets[i] * input_texel_size;

    colors[i] = dof_load_scatter_color(colorBuffer, sample_uv, 0.0);
    no_color = no_color && all(equal(colors[i].rgb, vec3(0.0)));
  }

  /* Early out from no color to scatter. */
  if (no_color) {
    vertex_discard();
    return;
  }

  weights = dof_layer_weight(cocs) * dof_sample_weight(cocs);
  /* Filter NaNs. */
  weights = mix(weights, vec4(0.0), equal(cocs, vec4(0.0)));

  color1 = colors[0] * weights[0];
  color2 = colors[1] * weights[1];
  color3 = colors[2] * weights[2];
  color4 = colors[3] * weights[3];

  /* Extend to cover at least the unit circle */
  const float extend = (cos(M_PI / 4.0) + 1.0) * 2.0;
  /* Crappy diagram
   * ex 1
   *    | \
   *    |   \
   *  1 |     \
   *    |       \
   *    |         \
   *  0 |     x     \
   *    |   Circle    \
   *    |   Origin      \
   * -1 0 --------------- 2
   *   -1     0     1     ex
   */

  /* Generate Triangle : less memory fetches from a VBO */
  int v_id = gl_VertexID % 3;                     /* Vertex Id */
  gl_Position.x = float(v_id / 2) * extend - 1.0; /* int divisor round down */
  gl_Position.y = float(v_id % 2) * extend - 1.0;
  gl_Position.z = 0.0;
  gl_Position.w = 1.0;

  spritesize = max_v4(cocs);

  /* Add 2.5 to max_coc because the max_coc may not be centered on the sprite origin
   * and because we smooth the bokeh shape a bit in the pixel shader. */
  gl_Position.xy *= spritesize * bokehAnisotropy + 2.5;
  /* Position the sprite. */
  gl_Position.xy += spritepos;
  /* NDC range [-1..1]. */
  gl_Position.xy = gl_Position.xy * targetTexelSize * 2.0 - 1.0;

  /* Add 2.5 for the same reason but without the ratio. */
  spritesize += 2.5;
}
