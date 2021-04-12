
/* NOTE: To be used with UNIFORM_RESOURCE_ID and INSTANCED_ATTR as define. */
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

in vec4 pos; /* Position and radius. */

/* ---- Instanced attribs ---- */

in vec3 pos_inst;
in vec3 nor;

mat3 pointcloud_get_facing_matrix(vec3 p)
{
  mat3 facing_mat;
  facing_mat[2] = cameraVec(p);
  facing_mat[1] = normalize(cross(ViewMatrixInverse[0].xyz, facing_mat[2]));
  facing_mat[0] = cross(facing_mat[1], facing_mat[2]);
  return facing_mat;
}

/* Return world position and normal. */
void pointcloud_get_pos_and_nor(out vec3 outpos, out vec3 outnor)
{
  vec3 p = point_object_to_world(pos.xyz);
  mat3 facing_mat = pointcloud_get_facing_matrix(p);

  float radius = dot(abs(mat3(ModelMatrix) * pos.www), vec3(1.0 / 3.0));
  /* TODO(fclem): remove multiplication here. Here only for keeping the size correct for now. */
  radius *= 0.01;
  outpos = p + (facing_mat * pos_inst) * radius;
  outnor = facing_mat * nor;
}

vec3 pointcloud_get_pos(void)
{
  vec3 outpos, outnor;
  pointcloud_get_pos_and_nor(outpos, outnor);
  return outpos;
}
