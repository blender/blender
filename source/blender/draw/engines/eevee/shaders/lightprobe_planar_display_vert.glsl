
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

in vec3 pos;

in int probe_id;
in mat4 probe_mat;

out vec3 worldPosition;
flat out int probeIdx;

void main()
{
  worldPosition = (probe_mat * vec4(-pos.x, pos.y, 0.0, 1.0)).xyz;
  gl_Position = ViewProjectionMatrix * vec4(worldPosition, 1.0);
  probeIdx = probe_id;
}
