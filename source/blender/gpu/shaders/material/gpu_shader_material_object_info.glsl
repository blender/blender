void node_object_info(mat4 obmat,
                      vec4 obcolor,
                      vec4 info,
                      float mat_index,
                      out vec3 location,
                      out vec4 color,
                      out float object_index,
                      out float material_index,
                      out float random)
{
  location = obmat[3].xyz;
  color = obcolor;
  object_index = info.x;
  material_index = mat_index;
  random = info.z;
}
