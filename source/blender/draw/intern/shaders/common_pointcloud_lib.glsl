
/* NOTE: To be used with UNIFORM_RESOURCE_ID and INSTANCED_ATTR as define. */
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#ifdef POINTCLOUD_SHADER
#  define COMMON_POINTCLOUD_LIB

#  ifndef USE_GPU_SHADER_CREATE_INFO
#    ifndef DRW_SHADER_SHARED_H

in vec4 pos; /* Position and radius. */

/* ---- Instanced attribs ---- */

in vec3 pos_inst;
in vec3 nor;

#    endif
#  else
#    ifndef DRW_POINTCLOUD_INFO
#      error Ensure createInfo includes `draw_pointcloud`.
#    endif
#  endif /* !USE_GPU_SHADER_CREATE_INFO */

int pointcloud_get_point_id()
{
  return gl_VertexID / 32;
}

mat3 pointcloud_get_facing_matrix(vec3 p)
{
  mat3 facing_mat;
  facing_mat[2] = cameraVec(p);
  facing_mat[1] = normalize(cross(ViewMatrixInverse[0].xyz, facing_mat[2]));
  facing_mat[0] = cross(facing_mat[1], facing_mat[2]);
  return facing_mat;
}

/* Returns world center position and radius. */
void pointcloud_get_pos_and_radius(out vec3 outpos, out float outradius)
{
  int id = pointcloud_get_point_id();
  vec4 pos_rad = texelFetch(ptcloud_pos_rad_tx, id);
  outpos = point_object_to_world(pos_rad.xyz);
  outradius = dot(abs(mat3(ModelMatrix) * pos_rad.www), vec3(1.0 / 3.0));
}

/* Return world position and normal. */
void pointcloud_get_pos_nor_radius(out vec3 outpos, out vec3 outnor, out float outradius)
{
  vec3 p;
  float radius;
  pointcloud_get_pos_and_radius(p, radius);

  mat3 facing_mat = pointcloud_get_facing_matrix(p);

  /* NOTE: Avoid modulo by non-power-of-two in shader. See Index buffer setup. */
  int vert_id = gl_VertexID % 32;
  vec3 pos_inst = vec3(0.0);

  switch (vert_id) {
    case 0:
      pos_inst.z = 1.0;
      break;
    case 1:
      pos_inst.x = 1.0;
      break;
    case 2:
      pos_inst.y = 1.0;
      break;
    case 3:
      pos_inst.x = -1.0;
      break;
    case 4:
      pos_inst.y = -1.0;
      break;
  }

  /* TODO(fclem): remove multiplication here. Here only for keeping the size correct for now. */
  radius *= 0.01;
  outnor = facing_mat * pos_inst;
  outpos = p + outnor * radius;
  outradius = radius;
}

/* Return world position and normal. */
void pointcloud_get_pos_and_nor(out vec3 outpos, out vec3 outnor)
{
  vec3 nor, pos;
  float radius;
  pointcloud_get_pos_nor_radius(pos, nor, radius);
  outpos = pos;
  outnor = nor;
}

vec3 pointcloud_get_pos()
{
  vec3 outpos, outnor;
  pointcloud_get_pos_and_nor(outpos, outnor);
  return outpos;
}

float pointcloud_get_customdata_float(const samplerBuffer cd_buf)
{
  int id = pointcloud_get_point_id();
  return texelFetch(cd_buf, id).r;
}

vec2 pointcloud_get_customdata_vec2(const samplerBuffer cd_buf)
{
  int id = pointcloud_get_point_id();
  return texelFetch(cd_buf, id).rg;
}

vec3 pointcloud_get_customdata_vec3(const samplerBuffer cd_buf)
{
  int id = pointcloud_get_point_id();
  return texelFetch(cd_buf, id).rgb;
}

vec4 pointcloud_get_customdata_vec4(const samplerBuffer cd_buf)
{
  int id = pointcloud_get_point_id();
  return texelFetch(cd_buf, id).rgba;
}

vec2 pointcloud_get_barycentric(void)
{
  /* TODO: To be implemented. */
  return vec2(0.0);
}
#endif
