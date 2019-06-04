
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

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

in int layerID_g[];

flat out int layerID;

void main()
{
  gl_Layer = layerID_g[0];
  layerID = gl_Layer - baseId;

  gl_Position = gl_in[0].gl_Position;
  EmitVertex();
  gl_Position = gl_in[1].gl_Position;
  EmitVertex();
  gl_Position = gl_in[2].gl_Position;
  EmitVertex();
  EndPrimitive();
}
