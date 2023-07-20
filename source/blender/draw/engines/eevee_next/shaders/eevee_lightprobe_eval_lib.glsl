
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
vec3 lightprobe_irradiance_grid_atlas_coord(IrradianceGridData grid_data,
                                            vec3 lP,
                                            vec3 lV,
                                            vec3 lNg)
{
  /* Shading point bias. */
  lP += lNg * grid_data.normal_bias;
  lP += lV * grid_data.view_bias;

  ivec3 brick_coord = ivec3((lP - 0.5) / float(IRRADIANCE_GRID_BRICK_SIZE - 1));
  /* Avoid sampling adjacent bricks. */
  brick_coord = max(brick_coord, ivec3(0));
  /* Avoid sampling adjacent bricks. */
  lP = max(lP, vec3(0.5));
  /* Local position inside the brick (still in grid sample spacing unit). */
  vec3 brick_lP = lP - vec3(brick_coord) * float(IRRADIANCE_GRID_BRICK_SIZE - 1);

  int brick_index = lightprobe_irradiance_grid_brick_index_get(grid_data, brick_coord);
  IrradianceBrick brick = irradiance_brick_unpack(bricks_infos_buf[brick_index]);

  /* A cell is the interpolation region between 8 texels. */
  vec3 cell_lP = brick_lP - 0.5;
  vec3 cell_start = floor(cell_lP);
  vec3 cell_fract = cell_lP - cell_start;
  /**
   * References:
   *
   * "Probe-based lighting, strand-based hair system, and physical hair shading in Unity’s Enemies"
   * by Francesco Cifariello Ciardi, Lasse Jon Fuglsang Pedersen and John Parsaie.
   *
   * "Multi-Scale Global Illumination in Quantum Break"
   * by Ari Silvennoinen and Ville Timonen.
   *
   * “Dynamic Diffuse Global Illumination with Ray-Traced Irradiance Fields”
   * by Morgan McGuire.
   */
  float trilinear_weights[8];
  float total_weight = 0.0;
  for (int i = 0; i < 8; i++) {
    ivec3 sample_position = (ivec3(i) >> ivec3(0, 1, 2)) & 1;

    vec3 trilinear = select(1.0 - cell_fract, cell_fract, bvec3(sample_position));
    float positional_weight = trilinear.x * trilinear.y * trilinear.z;

    float len;
    vec3 corner_vec = vec3(sample_position) - cell_fract;
    vec3 corner_dir = normalize_and_get_length(corner_vec, len);
    float cos_theta = (len > 1e-8) ? dot(lNg, corner_dir) : 1.0;
    float geometry_weight = saturate(cos_theta * 0.5 + 0.5);

    /* TODO(fclem): Need to bake validity. */
    float validity_weight = 1.0;

    /* Biases. See McGuire's presentation. */
    positional_weight += 0.001;
    geometry_weight = sqr(geometry_weight) + 0.2 + grid_data.facing_bias;

    trilinear_weights[i] = saturate(positional_weight * geometry_weight * validity_weight);
    total_weight += trilinear_weights[i];
  }
  float total_weight_inv = safe_rcp(total_weight);

  vec3 trilinear_coord = vec3(0.0);
  for (int i = 0; i < 8; i++) {
    vec3 sample_position = vec3((ivec3(i) >> ivec3(0, 1, 2)) & 1);
    trilinear_coord += sample_position * trilinear_weights[i] * total_weight_inv;
  }
  /* Replace sampling coordinates with manually weighted trilinear coordinates. */
  brick_lP = 0.5 + cell_start + trilinear_coord;

  vec3 output_coord = vec3(vec2(brick.atlas_coord), 0.0) + brick_lP;

  return output_coord;
}

vec4 textureUnormalizedCoord(sampler3D tx, vec3 co)
{
  return texture(tx, co / vec3(textureSize(tx, 0)));
}

SphericalHarmonicL1 lightprobe_irradiance_sample(sampler3D atlas_tx, vec3 P, vec3 V, vec3 Ng)
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

  /* TODO(fclem): Make sure this is working as expected. */
  mat3x3 world_to_grid_transposed = mat3x3(grids_infos_buf[grid_index].world_to_grid_transposed);
  vec3 lNg = safe_normalize(world_to_grid_transposed * Ng);
  vec3 lV = safe_normalize(V * world_to_grid_transposed);

  vec3 atlas_coord = lightprobe_irradiance_grid_atlas_coord(
      grids_infos_buf[grid_index], lP, lV, lNg);

  vec4 texture_coord = vec4(atlas_coord, float(IRRADIANCE_GRID_BRICK_SIZE)) /
                       vec3(textureSize(atlas_tx, 0)).xyzz;
  SphericalHarmonicL1 sh;
  sh.L0.M0 = textureLod(atlas_tx, texture_coord.xyz, 0.0);
  texture_coord.z += texture_coord.w;
  sh.L1.Mn1 = textureLod(atlas_tx, texture_coord.xyz, 0.0);
  texture_coord.z += texture_coord.w;
  sh.L1.M0 = textureLod(atlas_tx, texture_coord.xyz, 0.0);
  texture_coord.z += texture_coord.w;
  sh.L1.Mp1 = textureLod(atlas_tx, texture_coord.xyz, 0.0);
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
  /* NOTE: Use the diffuse normal for biasing the probe sampling location since it is smoother than
   * geometric normal. Could also try to use interp.N. */
  SphericalHarmonicL1 irradiance = lightprobe_irradiance_sample(
      irradiance_atlas_tx, P, V, diffuse.N);

  out_diffuse += spherical_harmonics_evaluate_lambert(diffuse.N, irradiance);
}
