
uniform mat4 prevViewProjMatrix;
uniform mat4 currViewProjMatrixInv;
uniform mat4 nextViewProjMatrix;

out vec4 outData;

void main()
{
  /* Extract pixel motion vector from camera movement. */
  ivec2 texel = ivec2(gl_FragCoord.xy);
  vec2 uv = gl_FragCoord.xy / vec2(textureSize(depthBuffer, 0).xy);

  float depth = texelFetch(depthBuffer, texel, 0).r;

  vec3 world_position = project_point(currViewProjMatrixInv, vec3(uv, depth) * 2.0 - 1.0);
  vec2 uv_prev = project_point(prevViewProjMatrix, world_position).xy * 0.5 + 0.5;
  vec2 uv_next = project_point(nextViewProjMatrix, world_position).xy * 0.5 + 0.5;

  outData.xy = uv_prev - uv;
  outData.zw = uv_next - uv;

  /* Encode to unsigned normalized 16bit texture. */
  outData = outData * 0.5 + 0.5;
}
