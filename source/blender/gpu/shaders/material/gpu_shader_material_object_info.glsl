void decompose_rotation(out mat3 mat){

  vec3 colx = mat[0];
  vec3 coly = mat[1];
  vec3 colz = mat[2];

  /* extract scale and shear first */
  vec3 scale, shear;
  scale.x = length(mat[0]);
  mat[0] /= scale.x;
  shear.z = dot(mat[0], mat[1]);
  mat[1] -= shear.z * mat[0];
  scale.y = length(coly);
  mat[1] /= scale.y;
  shear.y = dot(mat[0], mat[2]);
  mat[2] -= shear.y * mat[0];
  shear.x = dot(mat[1], mat[2]);
  mat[2] -= shear.x * mat[1];
  scale.z = length(mat[2]);
  mat[2] /= scale.z;

  if (dot(cross(mat[0], mat[1]), mat[2]) < 0.0) {
    mat *= -1.0;
  }
}

vec3 decompose_scale(mat3 mat){

  vec3 colx = mat[0];
  vec3 coly = mat[1];
  vec3 colz = mat[2];

  /* extract scale and shear first */
  vec3 scale, shear;
  scale.x = length(mat[0]);
  mat[0] /= scale.x;
  shear.z = dot(mat[0], mat[1]);
  mat[1] -= shear.z * mat[0];
  scale.y = length(coly);
  mat[1] /= scale.y;
  shear.y = dot(mat[0], mat[2]);
  mat[2] -= shear.y * mat[0];
  shear.x = dot(mat[1], mat[2]);
  mat[2] -= shear.x * mat[1];
  scale.z = length(mat[2]);
  mat[2] /= scale.z;

  if (dot(cross(mat[0], mat[1]), mat[2]) < 0.0) {
    scale *= -1.0f;
  }

  return scale;
}

vec3 mat3_to_euler(mat3 mat)
{
  vec3 euler1, euler2;
  float cy = length(mat[0].xy);

  if (abs(cy) > 0.005) {
    euler1.x = atan(mat[1].z, mat[2].z);
    euler1.y = atan(-mat[0].z, cy);
    euler1.z = atan(mat[0].y, mat[0].x);

    euler2.x = atan(-mat[1].z, -mat[2].z);
    euler2.y = atan(-mat[0].z, -cy);
    euler2.z = atan(-mat[0].y, -mat[0].x);
  }
  else {
    euler1.x = atan(-mat[2].y, mat[1].y);
    euler1.y = atan(-mat[0].z, cy);
    euler1.z = 0.0;
    euler2 = euler1;
  }

  if (abs(euler1.x) + abs(euler1.y) + abs(euler1.z) >
      abs(euler2.x) + abs(euler2.y) + abs(euler2.z)) {
    return euler2;
  }
  else {
    return euler1;
  }
}

vec3 mat4_to_euler(mat4 mat)
{
  mat3 m = mat3(mat);
  decompose_rotation(m);
  m[0] = normalize(m[0]);
  m[1] = normalize(m[1]);
  m[2] = normalize(m[2]);
  return mat3_to_euler(m);
}

vec3 mat4_to_scale(mat4 mat)
{
  return decompose_scale(mat3(mat));
}


void node_object_info(mat4 obmat,
                      vec4 obcolor,
                      vec4 info,
                      float mat_index,
                      out vec3 location,
                      out vec3 rotation,
                      out vec3 scale,
                      out vec4 color,
                      out float object_index,
                      out float material_index,
                      out float random)
{
  location = obmat[3].xyz;
  rotation = mat4_to_euler(obmat);
  scale = mat4_to_scale(obmat);
  color = obcolor;
  object_index = info.x;
  material_index = mat_index;
  random = info.z;
}
