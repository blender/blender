
layout(std140) uniform shadow_render_block
{
  /* Use vectors to avoid alignement padding. */
  ivec4 shadowSampleCount;
  vec4 shadowInvSampleCount;
  vec4 filterSize;
  int viewCount;
  int baseId;
  float cubeTexelSize;
  float storedTexelSize;
  float nearClip;
  float farClip;
  float exponent;
};

out int layerID_g;

void main()
{
  int v = gl_VertexID % 3;
  layerID_g = gl_VertexID / 3;
  float x = -1.0 + float((v & 1) << 2);
  float y = -1.0 + float((v & 2) << 1);
  gl_Position = vec4(x, y, 1.0, 1.0);

  /* HACK avoid changing drawcall parameters. */
  if (layerID_g >= viewCount) {
    gl_Position = vec4(0.0);
  }
  layerID_g += baseId;
}
