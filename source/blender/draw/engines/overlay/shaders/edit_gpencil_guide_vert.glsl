uniform vec4 pColor;
uniform float pSize;
uniform vec3 pPosition;

out vec4 finalColor;

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  gl_Position = point_world_to_ndc(pPosition);
  finalColor = pColor;
  gl_PointSize = pSize;
}
