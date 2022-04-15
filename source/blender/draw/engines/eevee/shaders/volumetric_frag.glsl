
#pragma BLENDER_REQUIRE(volumetric_lib.glsl)

/* Based on Frosbite Unified Volumetric.
 * https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite */

#ifdef MESH_SHADER
uniform vec3 volumeOrcoLoc;
uniform vec3 volumeOrcoSize;
uniform mat4 volumeObjectToTexture;
uniform float volumeDensityScale = 1.0;
#endif

flat in int slice;

/* Warning: these are not attributes, these are global vars. */
vec3 worldPosition = vec3(0.0);
vec3 viewPosition = vec3(0.0);
vec3 viewNormal = vec3(0.0);
vec3 volumeOrco = vec3(0.0);

layout(location = 0) out vec4 volumeScattering;
layout(location = 1) out vec4 volumeExtinction;
layout(location = 2) out vec4 volumeEmissive;
layout(location = 3) out vec4 volumePhase;

/* Store volumetric properties into the froxel textures. */

#ifdef MESH_SHADER
GlobalData init_globals(void)
{
  GlobalData surf;
  surf.P = worldPosition;
  surf.N = vec3(0.0);
  surf.Ng = vec3(0.0);
  surf.is_strand = false;
  surf.hair_time = 0.0;
  surf.hair_thickness = 0.0;
  surf.hair_strand_id = 0;
  surf.barycentric_coords = vec2(0.0);
  surf.barycentric_dists = vec3(0.0);
  surf.ray_type = RAY_TYPE_CAMERA;
  surf.ray_depth = 0.0;
  surf.ray_length = distance(surf.P, cameraPos);
  return surf;
}

vec3 coordinate_camera(vec3 P)
{
  vec3 vP;
  vP = transform_point(ViewMatrix, P);
  vP.z = -vP.z;
  return vP;
}

vec3 coordinate_screen(vec3 P)
{
  vec3 window = vec3(0.0);
  window.xy = project_point(ViewProjectionMatrix, P).xy * 0.5 + 0.5;
  window.xy = window.xy * CameraTexCoFactors.xy + CameraTexCoFactors.zw;
  return window;
}

vec3 coordinate_reflect(vec3 P, vec3 N)
{
  return vec3(0.0);
}

vec3 coordinate_incoming(vec3 P)
{
  return cameraVec(P);
}
#endif

void main()
{
  ivec3 volume_cell = ivec3(ivec2(gl_FragCoord.xy), slice);
  vec3 ndc_cell = volume_to_ndc((vec3(volume_cell) + volJitter.xyz) * volInvTexSize.xyz);

  viewPosition = get_view_space_from_depth(ndc_cell.xy, ndc_cell.z);
  worldPosition = point_view_to_world(viewPosition);
#ifdef MESH_SHADER
  volumeOrco = point_world_to_object(worldPosition);
  /* TODO: redundant transform */
  volumeOrco = (volumeOrco - volumeOrcoLoc + volumeOrcoSize) / (volumeOrcoSize * 2.0);
  volumeOrco = (volumeObjectToTexture * vec4(volumeOrco, 1.0)).xyz;

  if (any(lessThan(volumeOrco, vec3(0.0))) || any(greaterThan(volumeOrco, vec3(1.0)))) {
    /* Note: Discard is not an explicit return in Metal prior to versions 2.3.
     * adding return after discard ensures consistent behaviour and avoids GPU
     * side-effects where control flow continues with undefined values. */
    discard;
    return;
  }
#endif

#ifdef CLEAR
  volumeScattering = vec4(0.0, 0.0, 0.0, 1.0);
  volumeExtinction = vec4(0.0, 0.0, 0.0, 1.0);
  volumeEmissive = vec4(0.0, 0.0, 0.0, 1.0);
  volumePhase = vec4(0.0, 0.0, 0.0, 0.0);
#else
#  ifdef MESH_SHADER
  g_data = init_globals();
  attrib_load();
#  endif
  Closure cl = nodetree_exec();
#  ifdef MESH_SHADER
  cl.scatter *= volumeDensityScale;
  cl.absorption *= volumeDensityScale;
  cl.emission *= volumeDensityScale;
#  endif

  volumeScattering = vec4(cl.scatter, 1.0);
  volumeExtinction = vec4(cl.absorption + cl.scatter, 1.0);
  volumeEmissive = vec4(cl.emission, 1.0);
  /* Do not add phase weight if no scattering. */
  if (all(equal(cl.scatter, vec3(0.0)))) {
    volumePhase = vec4(0.0);
  }
  else {
    volumePhase = vec4(cl.anisotropy, vec3(1.0));
  }
#endif
}

vec3 attr_load_orco(vec4 orco)
{
  return volumeOrco;
}
vec4 attr_load_tangent(vec4 tangent)
{
  return vec4(0);
}
vec4 attr_load_vec4(vec4 attr)
{
  return vec4(0);
}
vec3 attr_load_vec3(vec3 attr)
{
  return vec3(0);
}
vec2 attr_load_vec2(vec2 attr)
{
  return vec2(0);
}
float attr_load_float(float attr)
{
  return 0.0;
}
vec4 attr_load_color(vec4 attr)
{
  return vec4(0);
}
vec3 attr_load_uv(vec3 attr)
{
  return vec3(0);
}
