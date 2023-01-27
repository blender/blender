#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  vec3 world_pos = point_object_to_world(vec3(au, 0.0));
  gl_Position = point_world_to_ndc(world_pos);

  bool is_selected = (flag & FACE_UV_SELECT) != 0u;
  bool is_active = (flag & FACE_UV_ACTIVE) != 0u;

  finalColor = (is_selected) ? colorFaceSelect : colorFace;
  finalColor = (is_active) ? colorEditMeshActive : finalColor;
  finalColor.a *= uvOpacity;
}
