/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * The resources expected to be defined are:
 * - grids_infos_buf
 * - bricks_infos_buf
 * - irradiance_atlas_tx
 */

#pragma BLENDER_REQUIRE(gpu_shader_codegen_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_lightprobe_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_spherical_harmonics_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_reflection_probe_eval_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_base_lib.glsl)

/**
 * Return the brick coordinate inside the grid.
 */
ivec3 lightprobe_irradiance_grid_brick_coord(vec3 lP)
{
  ivec3 brick_coord = ivec3((lP - 0.5) / float(IRRADIANCE_GRID_BRICK_SIZE - 1));
  /* Avoid sampling adjacent bricks. */
  return max(brick_coord, ivec3(0));
}

/**
 * Return the local coordinated of the shading point inside the brick in unnormalized coordinate.
 */
vec3 lightprobe_irradiance_grid_brick_local_coord(IrradianceGridData grid_data,
                                                  vec3 lP,
                                                  ivec3 brick_coord)
{
  /* Avoid sampling adjacent bricks around the origin. */
  lP = max(lP, vec3(0.5));
  /* Local position inside the brick (still in grid sample spacing unit). */
  vec3 brick_lP = lP - vec3(brick_coord) * float(IRRADIANCE_GRID_BRICK_SIZE - 1);
  return brick_lP;
}

/**
 * Return the biased local brick local coordinated.
 */
vec3 lightprobe_irradiance_grid_bias_sample_coord(IrradianceGridData grid_data,
                                                  uvec2 brick_atlas_coord,
                                                  vec3 brick_lP,
                                                  vec3 lNg)
{
  /* A cell is the interpolation region between 8 texels. */
  vec3 cell_lP = brick_lP - 0.5;
  vec3 cell_start = floor(cell_lP);
  vec3 cell_fract = cell_lP - cell_start;

  /* NOTE(fclem): Use uint to avoid signed int modulo. */
  uint vis_comp = uint(cell_start.z) % 4u;
  /* Visibility is stored after the irradiance. */
  ivec3 vis_coord = ivec3(ivec2(brick_atlas_coord), IRRADIANCE_GRID_BRICK_SIZE * 4) +
                    ivec3(cell_start);
  /* Visibility is stored packed 1 cell per channel. */
  vis_coord.z -= int(vis_comp);
  float cell_visibility = texelFetch(irradiance_atlas_tx, vis_coord, 0)[vis_comp];
  int cell_visibility_bits = int(cell_visibility);
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
    ivec3 sample_position = lightprobe_irradiance_grid_cell_corner(i);

    vec3 trilinear = select(1.0 - cell_fract, cell_fract, bvec3(sample_position));
    float positional_weight = trilinear.x * trilinear.y * trilinear.z;

    float len;
    vec3 corner_vec = vec3(sample_position) - cell_fract;
    vec3 corner_dir = normalize_and_get_length(corner_vec, len);
    float cos_theta = (len > 1e-8) ? dot(lNg, corner_dir) : 1.0;
    float geometry_weight = saturate(cos_theta * 0.5 + 0.5);

    float validity_weight = float((cell_visibility_bits >> i) & 1);

    /* Biases. See McGuire's presentation. */
    positional_weight += 0.001;
    geometry_weight = square(geometry_weight) + 0.2 + grid_data.facing_bias;

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
  return 0.5 + cell_start + trilinear_coord;
}

SphericalHarmonicL1 lightprobe_irradiance_sample_atlas(sampler3D atlas_tx, vec3 atlas_coord)
{
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

SphericalHarmonicL1 lightprobe_irradiance_sample(
    sampler3D atlas_tx, vec3 P, vec3 V, vec3 Ng, const bool do_bias)
{
  vec3 lP;
  int index = 0;
#ifdef IRRADIANCE_GRID_UPLOAD
  index = grid_start_index;
#endif
  for (; index < IRRADIANCE_GRID_MAX; index++) {
    /* Last grid is tagged as invalid to stop the iteration. */
    if (grids_infos_buf[index].grid_size.x == -1) {
      /* Sample the last grid instead. */
      index -= 1;
      break;
    }
    /* If sample fall inside the grid, step out of the loop. */
    if (lightprobe_irradiance_grid_local_coord(grids_infos_buf[index], P, lP)) {
      break;
    }
  }

  IrradianceGridData grid_data = grids_infos_buf[index];

  /* TODO(fclem): Make sure this is working as expected. */
  mat3x3 world_to_grid_transposed = mat3x3(grid_data.world_to_grid_transposed);
  vec3 lNg = safe_normalize(world_to_grid_transposed * Ng);
  vec3 lV = safe_normalize(V * world_to_grid_transposed);

  if (do_bias) {
    /* Shading point bias. */
    lP += lNg * grid_data.normal_bias;
    lP += lV * grid_data.view_bias;
  }
  else {
    lNg = vec3(0.0);
  }

  ivec3 brick_coord = lightprobe_irradiance_grid_brick_coord(lP);
  int brick_index = lightprobe_irradiance_grid_brick_index_get(grid_data, brick_coord);
  IrradianceBrick brick = irradiance_brick_unpack(bricks_infos_buf[brick_index]);

  vec3 brick_lP = lightprobe_irradiance_grid_brick_local_coord(grid_data, lP, brick_coord);

  /* Sampling point bias. */
  brick_lP = lightprobe_irradiance_grid_bias_sample_coord(
      grid_data, brick.atlas_coord, brick_lP, lNg);

  vec3 atlas_coord = vec3(vec2(brick.atlas_coord), 0.0) + brick_lP;

  return lightprobe_irradiance_sample_atlas(atlas_tx, atlas_coord);
}

SphericalHarmonicL1 lightprobe_irradiance_world()
{
  return lightprobe_irradiance_sample_atlas(irradiance_atlas_tx, vec3(0.0));
}

SphericalHarmonicL1 lightprobe_irradiance_sample(vec3 P)
{
  return lightprobe_irradiance_sample(irradiance_atlas_tx, P, vec3(0), vec3(0), false);
}

SphericalHarmonicL1 lightprobe_irradiance_sample(vec3 P, vec3 V, vec3 Ng)
{
  return lightprobe_irradiance_sample(irradiance_atlas_tx, P, V, Ng, true);
}

#ifdef REFLECTION_PROBE

struct LightProbeSample {
  SphericalHarmonicL1 volume_irradiance;
  int spherical_id;
};

#  if defined(GPU_FRAGMENT_SHADER)
#    define UTIL_TEXEL vec2(gl_FragCoord)
#  elif defined(GPU_COMPUTE_SHADER)
#    define UTIL_TEXEL vec2(gl_GlobalInvocationID)
#  else
#    define UTIL_TEXEL vec2(gl_VertexID, 0)
#  endif

/**
 * Return cached light-probe data at P.
 * Ng and V are use for biases.
 */
LightProbeSample lightprobe_load(vec3 P, vec3 Ng, vec3 V)
{
  /* TODO: Dependency hell */
  float noise = 0.0;

  LightProbeSample result;
  result.volume_irradiance = lightprobe_irradiance_sample(P, V, Ng);
  result.spherical_id = reflection_probes_select(P, noise);
  return result;
}

/* Return the best parallax corrected ray direction from the probe center. */
vec3 lightprobe_sphere_parallax(ReflectionProbeData probe, vec3 P, vec3 L)
{
  bool is_world = (probe.influence_scale == 0.0);
  if (is_world) {
    return L;
  }
  /* Correct reflection ray using parallax volume intersection. */
  vec3 lP = vec4(P, 1.0) * probe.world_to_probe_transposed;
  vec3 lL = (mat3x3(probe.world_to_probe_transposed) * L) / probe.parallax_distance;

  float dist = (probe.parallax_shape == SHAPE_ELIPSOID) ? line_unit_sphere_intersect_dist(lP, lL) :
                                                          line_unit_box_intersect_dist(lP, lL);

  /* Use distance in world space directly to recover intersection.
   * This works because we assume no shear in the probe matrix. */
  vec3 L_new = P + L * dist - probe.location;

  /* TODO(fclem): Roughness adjustment. */

  return L_new;
}

/**
 * Return spherical sample normalized by irradiance at sample position.
 * This avoid most of light leaking and reduce the need for many local probes.
 */
vec3 lightprobe_spherical_sample_normalized_with_parallax(
    int probe_index, vec3 P, vec3 L, float lod, SphericalHarmonicL1 P_sh)
{
  ReflectionProbeData probe = reflection_probe_buf[probe_index];
  ReflectionProbeLowFreqLight shading_sh = reflection_probes_extract_low_freq(P_sh);
  vec3 normalization_factor = reflection_probes_normalization_eval(
      L, shading_sh, probe.low_freq_light);
  L = lightprobe_sphere_parallax(probe, P, L);
  return normalization_factor * reflection_probes_sample(L, lod, probe.atlas_coord).rgb;
}

float pdf_to_lod(float pdf)
{
  return 1.0; /* TODO */
}

vec3 lightprobe_eval_direction(LightProbeSample samp, vec3 P, vec3 L, float pdf)
{
  vec3 radiance_sh = lightprobe_spherical_sample_normalized_with_parallax(
      samp.spherical_id, P, L, pdf_to_lod(pdf), samp.volume_irradiance);

  return radiance_sh;
}

float lightprobe_roughness_to_cube_sh_mix_fac(float roughness)
{
  /* Temporary. Do something better. */
  return square(saturate(roughness * 4.0 - 2.0));
}

float lightprobe_roughness_to_lod(float roughness)
{
  /* Temporary. Do something better. */
  return sqrt(roughness) * 11.0;
}

vec3 lightprobe_eval(LightProbeSample samp, ClosureDiffuse diffuse, vec3 P, vec3 V, vec2 noise)
{
  vec3 radiance_sh = spherical_harmonics_evaluate_lambert(diffuse.N, samp.volume_irradiance);
  return radiance_sh;
}

vec3 lightprobe_specular_dominant_dir(vec3 N, vec3 V, float roughness)
{
  vec3 R = -reflect(V, N);
  float smoothness = 1.0 - roughness;
  float fac = smoothness * (sqrt(smoothness) + roughness);
  return normalize(mix(N, R, fac));
}

vec3 lightprobe_eval(
    LightProbeSample samp, ClosureReflection reflection, vec3 P, vec3 V, vec2 noise)
{
  vec3 L = lightprobe_specular_dominant_dir(reflection.N, V, reflection.roughness);
  // vec3 L = ray_generate_direction(noise, reflection, V, pdf);

  float lod = lightprobe_roughness_to_lod(reflection.roughness);
  vec3 radiance_cube = lightprobe_spherical_sample_normalized_with_parallax(
      samp.spherical_id, P, L, lod, samp.volume_irradiance);

  float fac = lightprobe_roughness_to_cube_sh_mix_fac(reflection.roughness);
  vec3 radiance_sh = spherical_harmonics_evaluate_lambert(L, samp.volume_irradiance);
  return mix(radiance_cube, radiance_sh, fac);
}

#endif /* REFLECTION_PROBE */
