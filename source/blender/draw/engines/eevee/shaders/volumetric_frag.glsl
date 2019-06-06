
/* Based on Frosbite Unified Volumetric.
 * https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite */

#define NODETREE_EXEC

#ifdef MESH_SHADER
uniform vec3 volumeOrcoLoc;
uniform vec3 volumeOrcoSize;
#endif

flat in int slice;

/* Warning: theses are not attributes, theses are global vars. */
vec3 worldPosition = vec3(0.0);
vec3 viewPosition = vec3(0.0);
vec3 viewNormal = vec3(0.0);
#ifdef MESH_SHADER
vec3 volumeObjectLocalCoord = vec3(0.0);
#endif

layout(location = 0) out vec4 volumeScattering;
layout(location = 1) out vec4 volumeExtinction;
layout(location = 2) out vec4 volumeEmissive;
layout(location = 3) out vec4 volumePhase;

/* Store volumetric properties into the froxel textures. */

void main()
{
  ivec3 volume_cell = ivec3(gl_FragCoord.xy, slice);
  vec3 ndc_cell = volume_to_ndc((vec3(volume_cell) + volJitter.xyz) * volInvTexSize.xyz);

  viewPosition = get_view_space_from_depth(ndc_cell.xy, ndc_cell.z);
  worldPosition = point_view_to_world(viewPosition);
#ifdef MESH_SHADER
  volumeObjectLocalCoord = point_world_to_object(worldPosition);
  volumeObjectLocalCoord = (volumeObjectLocalCoord - volumeOrcoLoc + volumeOrcoSize) /
                           (volumeOrcoSize * 2.0);

  if (any(lessThan(volumeObjectLocalCoord, vec3(0.0))) ||
      any(greaterThan(volumeObjectLocalCoord, vec3(1.0))))
    discard;
#endif

#ifdef CLEAR
  Closure cl = CLOSURE_DEFAULT;
#else
  Closure cl = nodetree_exec();
#endif

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
}
