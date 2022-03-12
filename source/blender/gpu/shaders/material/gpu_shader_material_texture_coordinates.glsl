vec3 mtex_2d_mapping(vec3 vec)
{
  return vec3(vec.xy * 0.5 + vec2(0.5), vec.z);
}

void generated_from_orco(vec3 orco, out vec3 generated)
{
#ifdef VOLUMETRICS
#  ifdef MESH_SHADER
  generated = volumeObjectLocalCoord;
#  else
  generated = worldPosition;
#  endif
#else
  generated = orco;
#endif
}

void generated_texco(vec3 I, vec3 attr_orco, out vec3 generated)
{
  vec4 v = (ProjectionMatrix[3][3] == 0.0) ? vec4(I, 1.0) : vec4(0.0, 0.0, 1.0, 1.0);
  vec4 co_homogeneous = (ProjectionMatrixInverse * v);
  vec4 co = vec4(co_homogeneous.xyz / co_homogeneous.w, 0.0);
  co.xyz = normalize(co.xyz);
#if defined(WORLD_BACKGROUND) || defined(PROBE_CAPTURE)
  generated = (ViewMatrixInverse * co).xyz;
#else
  generated_from_orco(attr_orco, generated);
#endif
}

void node_tex_coord(vec3 I,
                    vec3 wN,
                    mat4 obmatinv,
                    vec4 camerafac,
                    vec3 attr_orco,
                    vec3 attr_uv,
                    out vec3 generated,
                    out vec3 normal,
                    out vec3 uv,
                    out vec3 object,
                    out vec3 camera,
                    out vec3 window,
                    out vec3 reflection)
{
  generated = attr_orco;
  normal = normalize(normal_world_to_object(wN));
  uv = attr_uv;
  object = (obmatinv * (ViewMatrixInverse * vec4(I, 1.0))).xyz;
  camera = vec3(I.xy, -I.z);
  vec4 projvec = ProjectionMatrix * vec4(I, 1.0);
  window = vec3(mtex_2d_mapping(projvec.xyz / projvec.w).xy * camerafac.xy + camerafac.zw, 0.0);
  reflection = -reflect(cameraVec(worldPosition), normalize(wN));
}

void node_tex_coord_background(vec3 I,
                               vec3 N,
                               mat4 obmatinv,
                               vec4 camerafac,
                               vec3 attr_orco,
                               vec3 attr_uv,
                               out vec3 generated,
                               out vec3 normal,
                               out vec3 uv,
                               out vec3 object,
                               out vec3 camera,
                               out vec3 window,
                               out vec3 reflection)
{
  vec4 v = (ProjectionMatrix[3][3] == 0.0) ? vec4(I, 1.0) : vec4(0.0, 0.0, 1.0, 1.0);
  vec4 co_homogeneous = (ProjectionMatrixInverse * v);

  vec4 co = vec4(co_homogeneous.xyz / co_homogeneous.w, 0.0);

  co = normalize(co);

  vec3 coords = (ViewMatrixInverse * co).xyz;

  generated = coords;
  normal = -coords;
  uv = vec3(attr_uv.xy, 0.0);
  object = (obmatinv * vec4(coords, 1.0)).xyz;

  camera = vec3(co.xy, -co.z);
  window = vec3(mtex_2d_mapping(I).xy * camerafac.xy + camerafac.zw, 0.0);

  reflection = -coords;
}

#if defined(WORLD_BACKGROUND) || (defined(PROBE_CAPTURE) && !defined(MESH_SHADER))
#  define node_tex_coord node_tex_coord_background
#endif
