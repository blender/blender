
uniform mat4 currPersinv;
uniform mat4 pastPersmat;

out vec2 outData;

void main()
{
  /* Extract pixel motion vector from camera movement. */
  ivec2 texel = ivec2(gl_FragCoord.xy);
  vec2 uv = gl_FragCoord.xy / vec2(textureSize(depthBuffer, 0).xy);

  float depth = texelFetch(depthBuffer, texel, 0).r;

  vec3 world_position = project_point(currPersinv, vec3(uv, depth) * 2.0 - 1.0);
  vec2 uv_history = project_point(pastPersmat, world_position).xy * 0.5 + 0.5;

  outData = uv - uv_history;

  /* HACK: Reject lookdev spheres from TAA reprojection. */
  outData = (depth > 0.0) ? outData : vec2(0.0);

  /* Encode to unsigned normalized 16bit texture. */
  outData = outData * 0.5 + 0.5;
}
