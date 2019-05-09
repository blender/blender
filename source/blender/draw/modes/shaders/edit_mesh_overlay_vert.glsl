
uniform mat4 ModelViewMatrix;
uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelMatrix;
uniform mat4 ModelMatrixInverse;

uniform float faceAlphaMod;
uniform ivec4 dataMask = ivec4(0xFF);
uniform float ofs;

in ivec4 data;
in vec3 pos;
#ifndef FACEDOT
in vec3 vnor;
#else
in vec4 norAndFlag;
#  define vnor norAndFlag.xyz
#endif

out vec4 finalColor;
out vec4 finalColorOuter;
#ifdef USE_GEOM_SHADER
out int selectOveride;
#endif

void main()
{
#if !defined(FACE)
  mat4 projmat = ProjectionMatrix;
  projmat[3][2] -= ofs;

  gl_Position = projmat * (ModelViewMatrix * vec4(pos, 1.0));
#else

  gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
#endif

  ivec4 m_data = data & dataMask;

#if defined(VERT)
  finalColor = EDIT_MESH_vertex_color(m_data.y);
  gl_PointSize = sizeVertex * 2.0;
  gl_Position.z -= 3e-5 * ((ProjectionMatrix[3][3] == 0.0) ? 1.0 : 0.0);
  /* Make selected and active vertex always on top. */
  if ((data.x & VERT_SELECTED) != 0) {
    gl_Position.z -= 1e-7;
  }
  if ((data.x & VERT_ACTIVE) != 0) {
    gl_Position.z -= 1e-7;
  }

#elif defined(EDGE)
#  ifdef FLAT
  finalColor = EDIT_MESH_edge_color_inner(m_data.y);
  selectOveride = 1;
#  else
  finalColor = EDIT_MESH_edge_vertex_color(m_data.y);
  selectOveride = (m_data.y & EDGE_SELECTED);
#  endif

  float crease = float(m_data.z) / 255.0;
  float bweight = float(m_data.w) / 255.0;
  finalColorOuter = EDIT_MESH_edge_color_outer(m_data.y, m_data.x, crease, bweight);

#elif defined(FACE)
  finalColor = EDIT_MESH_face_color(m_data.x);
  finalColor.a *= faceAlphaMod;

#elif defined(FACEDOT)
  finalColor = EDIT_MESH_facedot_color(norAndFlag.w);
  /* Bias Facedot Z position in clipspace. */
  gl_Position.z -= 0.00035;
  gl_PointSize = sizeFaceDot;

#endif

#if !defined(FACE)
  /* Facing based color blend */
  vec4 vpos = ModelViewMatrix * vec4(pos, 1.0);
  vec3 view_normal = normalize(normal_object_to_view(vnor) + 1e-4);
  vec3 view_vec = (ProjectionMatrix[3][3] == 0.0) ? normalize(vpos.xyz) : vec3(0.0, 0.0, 1.0);
  float facing = dot(view_vec, view_normal);
  facing = 1.0 - abs(facing) * 0.2;

  finalColor.rgb = mix(colorEditMeshMiddle.rgb, finalColor.rgb, facing);

#endif

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance((ModelMatrix * vec4(pos, 1.0)).xyz);
#endif
}
