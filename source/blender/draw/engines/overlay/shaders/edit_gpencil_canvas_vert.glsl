
uniform vec4 color;
uniform vec3 xAxis;
uniform vec3 yAxis;
uniform vec3 origin;
uniform int halfLineCount;

flat out vec4 finalColor;
flat out vec2 edgeStart;
noperspective out vec2 edgePos;

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  vec2 pos;
  pos.x = float(gl_VertexID % 2);
  pos.y = float(gl_VertexID / 2) / float(halfLineCount - 1);

  if (pos.y > 1.0) {
    pos.xy = pos.yx;
    pos.x -= 1.0 + 1.0 / float(halfLineCount - 1);
  }

  pos -= 0.5;

  vec3 world_pos = xAxis * pos.x + yAxis * pos.y + origin;

  gl_Position = point_world_to_ndc(world_pos);

  finalColor = color;

  /* Convert to screen position [0..sizeVp]. */
  edgePos = edgeStart = ((gl_Position.xy / gl_Position.w) * 0.5 + 0.5) * sizeViewport.xy;
}
