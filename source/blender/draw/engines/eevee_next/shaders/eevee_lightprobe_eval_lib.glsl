
/**
 * The resources expected to be defined are:
 * - grids_infos_buf
 * - bricks_infos_buf
 * - irradiance_atlas_tx
 */

#pragma BLENDER_REQUIRE(eevee_lightprobe_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_spherical_harmonics_lib.glsl)

/**
 * Return sample coordinates of the first SH coef in unormalized texture space.
 */
vec3 lightprobe_irradiance_grid_atlas_coord(IrradianceGridData grid_data, vec3 lP)
{
  ivec3 brick_coord = ivec3((lP - 0.5) / float(IRRADIANCE_GRID_BRICK_SIZE - 1));
  /* Avoid sampling adjacent bricks. */
  brick_coord = max(brick_coord, ivec3(0));
  /* Avoid sampling adjacent bricks. */
  lP = max(lP, vec3(0.5));
  /* Local position inside the brick (still in grid sample spacing unit). */
  vec3 brick_lP = lP - vec3(brick_coord) * float(IRRADIANCE_GRID_BRICK_SIZE - 1);

  int brick_index = lightprobe_irradiance_grid_brick_index_get(grid_data, brick_coord);

  IrradianceBrick brick = irradiance_brick_unpack(bricks_infos_buf[brick_index]);
  vec3 output_coord = vec3(vec2(brick.atlas_coord), 0.0) + brick_lP;

  return output_coord;
}

vec4 textureUnormalizedCoord(sampler3D tx, vec3 co)
{
  return texture(tx, co / vec3(textureSize(tx, 0)));
}

SphericalHarmonicL1 lightprobe_irradiance_sample(sampler3D atlas_tx, vec3 P)
{
  vec3 lP;
  int grid_index;
  for (grid_index = 0; grid_index < IRRADIANCE_GRID_MAX; grid_index++) {
    /* Last grid is tagged as invalid to stop the iteration. */
    if (grids_infos_buf[grid_index].grid_size.x == -1) {
      /* Sample the last grid instead. */
      grid_index -= 1;
      break;
    }
    /* If sample fall inside the grid, step out of the loop. */
    if (lightprobe_irradiance_grid_local_coord(grids_infos_buf[grid_index], P, lP)) {
      break;
    }
  }

  vec3 atlas_coord = lightprobe_irradiance_grid_atlas_coord(grids_infos_buf[grid_index], lP);

  SphericalHarmonicL1 sh;
  sh.L0.M0 = textureUnormalizedCoord(atlas_tx, atlas_coord);
  atlas_coord.z += float(IRRADIANCE_GRID_BRICK_SIZE);
  sh.L1.Mn1 = textureUnormalizedCoord(atlas_tx, atlas_coord);
  atlas_coord.z += float(IRRADIANCE_GRID_BRICK_SIZE);
  sh.L1.M0 = textureUnormalizedCoord(atlas_tx, atlas_coord);
  atlas_coord.z += float(IRRADIANCE_GRID_BRICK_SIZE);
  sh.L1.Mp1 = textureUnormalizedCoord(atlas_tx, atlas_coord);
  return sh;
}

void lightprobe_eval(ClosureDiffuse diffuse,
                     ClosureReflection reflection,
                     vec3 P,
                     vec3 Ng,
                     vec3 V,
                     inout vec3 out_diffuse,
                     inout vec3 out_specular)
{
  SphericalHarmonicL1 irradiance = lightprobe_irradiance_sample(irradiance_atlas_tx, P);

  out_diffuse += spherical_harmonics_evaluate_lambert(diffuse.N, irradiance);
}