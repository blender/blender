/* SPDX-FileCopyrightText: 2017-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Based on Frosbite Unified Volumetric.
 * https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite */

#pragma BLENDER_REQUIRE(volumetric_lib.glsl)

/* Store volumetric properties into the froxel textures. */

/* WARNING: these are not attributes, these are global vars. */
vec3 worldPosition = vec3(0.0);
vec3 objectPosition = vec3(0.0);
vec3 viewPosition = vec3(0.0);
vec3 viewNormal = vec3(0.0);
vec3 volumeOrco = vec3(0.0);

int attr_id = 0;

#ifndef CLEAR
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
  window.xy = project_point(ProjectionMatrix, transform_point(ViewMatrix, P)).xy * 0.5 + 0.5;
  window.xy = window.xy * cameraUvScaleBias.xy + cameraUvScaleBias.zw;
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

float film_scaling_factor_get()
{
  return 1.0;
}
#endif

void main()
{
  ivec3 volume_cell = ivec3(ivec2(gl_FragCoord.xy), volumetric_geom_iface.slice);
  vec3 ndc_cell = volume_to_ndc((vec3(volume_cell) + volJitter.xyz) * volInvTexSize.xyz);

  viewPosition = get_view_space_from_depth(ndc_cell.xy, ndc_cell.z);
  worldPosition = point_view_to_world(viewPosition);
#ifdef MESH_SHADER
  objectPosition = point_world_to_object(worldPosition);
  volumeOrco = OrcoTexCoFactors[0].xyz + objectPosition * OrcoTexCoFactors[1].xyz;

  if (any(lessThan(volumeOrco, vec3(0.0))) || any(greaterThan(volumeOrco, vec3(1.0)))) {
    /* NOTE: Discard is not an explicit return in Metal prior to versions 2.3.
     * adding return after discard ensures consistent behavior and avoids GPU
     * side-effects where control flow continues with undefined values. */
    discard;
    return;
  }
#else /* WORLD_SHADER */
  volumeOrco = worldPosition;
#endif

#ifdef CLEAR
  volumeScattering = vec4(0.0, 0.0, 0.0, 1.0);
  volumeExtinction = vec4(0.0, 0.0, 0.0, 1.0);
  volumeEmissive = vec4(0.0, 0.0, 0.0, 1.0);
  volumePhase = vec4(0.0, 0.0, 0.0, 0.0);
#else
  g_data = init_globals();
#  ifndef NO_ATTRIB_LOAD
  attrib_load();
#  endif
  Closure cl = nodetree_exec();
#  ifdef MESH_SHADER
  cl.scatter *= drw_volume.density_scale;
  cl.absorption *= drw_volume.density_scale;
  cl.emission *= drw_volume.density_scale;
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

vec3 grid_coordinates()
{
  vec3 co = volumeOrco;
#ifdef MESH_SHADER
  co = (drw_volume.grids_xform[attr_id] * vec4(objectPosition, 1.0)).xyz;
#endif
  attr_id += 1;
  return co;
}

vec3 attr_load_orco(sampler3D orco)
{
  attr_id += 1;
  return volumeOrco;
}
vec4 attr_load_tangent(sampler3D tangent)
{
  attr_id += 1;
  return vec4(0);
}
vec4 attr_load_vec4(sampler3D tex)
{
  return texture(tex, grid_coordinates());
}
vec3 attr_load_vec3(sampler3D tex)
{
  return texture(tex, grid_coordinates()).rgb;
}
vec2 attr_load_vec2(sampler3D tex)
{
  return texture(tex, grid_coordinates()).rg;
}
float attr_load_float(sampler3D tex)
{
  return texture(tex, grid_coordinates()).r;
}

/* TODO(@fclem): These implementation details should concern the DRWManager and not be a fix on
 * the engine side. But as of now, the engines are responsible for loading the attributes. */
float attr_load_temperature_post(float attr)
{
#ifdef MESH_SHADER
  /* Bring the into standard range without having to modify the grid values */
  attr = (attr > 0.01) ? (attr * drw_volume.temperature_mul + drw_volume.temperature_bias) : 0.0;
#endif
  return attr;
}
vec4 attr_load_color_post(vec4 attr)
{
#ifdef MESH_SHADER
  /* Density is premultiplied for interpolation, divide it out here. */
  attr.rgb *= safe_rcp(attr.a);
  attr.rgb *= drw_volume.color_mul.rgb;
  attr.a = 1.0;
#endif
  return attr;
}
vec4 attr_load_uniform(vec4 attr, const uint attr_hash)
{
  return attr;
}
