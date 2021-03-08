
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)

uniform vec4 cocParams;

#define cocMul cocParams[0]  /* distance * aperturesize * invsensorsize */
#define cocBias cocParams[1] /* aperturesize * invsensorsize */
#define cocNear cocParams[2] /* Near view depths value. */
#define cocFar cocParams[3]  /* Far view depths value. */

/* -------------- Debug Defines ------------- */

// #define DOF_DEBUG_GATHER_PERF
// #define DOF_DEBUG_SCATTER_PERF

const bool no_smooth_intersection = false;
const bool no_gather_occlusion = false;
const bool no_gather_mipmaps = false;
const bool no_gather_random = false;
const bool no_gather_filtering = false;
const bool no_scatter_occlusion = false;
const bool no_scatter_pass = false;
const bool no_foreground_pass = false;
const bool no_background_pass = false;
const bool no_slight_focus_pass = false;
const bool no_focus_pass = false;
const bool no_holefill_pass = false;

/* -------------- Quality Defines ------------- */

#ifdef DOF_HOLEFILL_PASS
/* No need for very high density for holefill. */
const int gather_ring_count = 3;
const int gather_ring_density = 3;
const int gather_max_density_change = 0;
const int gather_density_change_ring = 1;
#else
const int gather_ring_count = DOF_GATHER_RING_COUNT;
const int gather_ring_density = 3;
const int gather_max_density_change = 50; /* Dictates the maximum good quality blur. */
const int gather_density_change_ring = 1;
#endif

/* -------------- Utils ------------- */

const vec2 quad_offsets[4] = vec2[4](
    vec2(-0.5, 0.5), vec2(0.5, 0.5), vec2(0.5, -0.5), vec2(-0.5, -0.5));

/* Divide by sensor size to get the normalized size. */
#define calculate_coc_persp(zdepth) (cocMul / zdepth - cocBias)
#define calculate_coc_ortho(zdepth) ((zdepth + cocMul / cocBias) * cocMul)
#define calculate_coc(z) \
  (ProjectionMatrix[3][3] == 0.0) ? calculate_coc_persp(z) : calculate_coc_ortho(z)

/* Ortho conversion is only true for camera view! */
#define linear_depth_persp(d) ((cocNear * cocFar) / (d * (cocNear - cocFar) + cocFar))
#define linear_depth_ortho(d) (d * (cocNear - cocFar) + cocNear)

#define linear_depth(d) \
  ((ProjectionMatrix[3][3] == 0.0) ? linear_depth_persp(d) : linear_depth_ortho(d))

#define dof_coc_from_zdepth(d) calculate_coc(linear_depth(d))

float dof_hdr_color_weight(vec4 color)
{
  /* From UE4. Very fast "luma" weighting. */
  float luma = (color.g * 2.0) + (color.r + color.b);
  /* TODO(fclem) Pass correct exposure. */
  const float exposure = 1.0;
  return 1.0 / (luma * exposure + 4.0);
}

float dof_coc_select(vec4 cocs)
{
  /* Select biggest coc. */
  float selected_coc = cocs.x;
  if (abs(cocs.y) > abs(selected_coc)) {
    selected_coc = cocs.y;
  }
  if (abs(cocs.z) > abs(selected_coc)) {
    selected_coc = cocs.z;
  }
  if (abs(cocs.w) > abs(selected_coc)) {
    selected_coc = cocs.w;
  }
  return selected_coc;
}

/* NOTE: Do not forget to normalize weights afterwards. */
vec4 dof_downsample_bilateral_coc_weights(vec4 cocs)
{
  float chosen_coc = dof_coc_select(cocs);

  const float scale = 4.0; /* TODO(fclem) revisit. */
  /* NOTE: The difference between the cocs should be inside a abs() function,
   * but we follow UE4 implementation to improve how dithered transparency looks (see slide 19). */
  return saturate(1.0 - (chosen_coc - cocs) * scale);
}

/* NOTE: Do not forget to normalize weights afterwards. */
vec4 dof_downsample_bilateral_color_weights(vec4 colors[4])
{
  vec4 weights;
  for (int i = 0; i < 4; i++) {
    weights[i] = dof_hdr_color_weight(colors[i]);
  }
  return weights;
}

/* Makes sure the load functions distribute the energy correctly
 * to both scatter and gather passes. */
vec4 dof_load_gather_color(sampler2D gather_input_color_buffer, vec2 uv, float lod)
{
  vec4 color = textureLod(gather_input_color_buffer, uv, lod);
  return color;
}

vec4 dof_load_scatter_color(sampler2D scatter_input_color_buffer, vec2 uv, float lod)
{
  vec4 color = textureLod(scatter_input_color_buffer, uv, lod);
  return color;
}

float dof_load_gather_coc(sampler2D gather_input_coc_buffer, vec2 uv, float lod)
{
  float coc = textureLod(gather_input_coc_buffer, uv, lod).r;
  /* We gather at halfres. CoC must be divided by 2 to be compared against radii. */
  return coc * 0.5;
}

/* Distribute weights between near/slightfocus/far fields (slide 117). */
const float layer_threshold = 4.0;
/* Make sure it overlaps. */
const float layer_offset_fg = 0.5 + 1.0;
/* Extra offset for convolution layers to avoid light leaking from background. */
const float layer_offset = 0.5 + 0.5;

#define DOF_MAX_SLIGHT_FOCUS_RADIUS 5

float dof_layer_weight(float coc, const bool is_foreground)
{
/* NOTE: These are fullres pixel CoC value. */
#ifdef DOF_RESOLVE_PASS
  return saturate(-abs(coc) + layer_threshold + layer_offset) *
         float(is_foreground ? (coc <= 0.5) : (coc > -0.5));
#else
  coc *= 2.0; /* Account for half pixel gather. */
  float threshold = layer_threshold - ((is_foreground) ? layer_offset_fg : layer_offset);
  return saturate(((is_foreground) ? -coc : coc) - threshold);
#endif
}
vec4 dof_layer_weight(vec4 coc)
{
  /* NOTE: Used for scatter pass which already flipped the sign correctly. */
  coc *= 2.0; /* Account for half pixel gather. */
  return saturate(coc - layer_threshold + layer_offset);
}

/* NOTE: This is halfres CoC radius. */
float dof_sample_weight(float coc)
{
  /* Full intensity if CoC radius is below the pixel footprint. */
  const float min_coc = 1.0;
  coc = max(min_coc, abs(coc));
  return (M_PI * min_coc * min_coc) / (M_PI * coc * coc);
}
vec4 dof_sample_weight(vec4 coc)
{
  /* Full intensity if CoC radius is below the pixel footprint. */
  const float min_coc = 1.0;
  coc = max(vec4(min_coc), abs(coc));
  return (M_PI * min_coc * min_coc) / (M_PI * coc * coc);
}

/* Intersection with the center of the kernel. */
float dof_intersection_weight(float coc, float distance_from_center, float intersection_multiplier)
{
  if (no_smooth_intersection) {
    return step(0.0, (abs(coc) - distance_from_center));
  }
  else {
    /* (Slide 64). */
    return saturate((abs(coc) - distance_from_center) * intersection_multiplier + 0.5);
  }
}

/* Returns weight of the sample for the outer bucket (containing previous rings). */
float dof_gather_accum_weight(float coc, float bordering_radius, bool first_ring)
{
  /* First ring has nothing to be mixed against. */
  if (first_ring) {
    return 0.0;
  }
  return saturate(coc - bordering_radius);
}

bool dof_do_fast_gather(float max_absolute_coc, float min_absolute_coc, const bool is_foreground)
{
  float min_weight = dof_layer_weight((is_foreground) ? -min_absolute_coc : min_absolute_coc,
                                      is_foreground);
  if (min_weight < 1.0) {
    return false;
  }
  /* FIXME(fclem): This is a workaround to fast gather triggering too early.
   * Since we use custom opacity mask, the opacity is not given to be 100% even for
   * after normal threshold. */
  if (is_foreground && min_absolute_coc < layer_threshold) {
    return false;
  }
  return (max_absolute_coc - min_absolute_coc) < (DOF_FAST_GATHER_COC_ERROR * max_absolute_coc);
}

/* ------------------- COC TILES UTILS ------------------- */

struct CocTile {
  float fg_min_coc;
  float fg_max_coc;
  float fg_max_intersectable_coc;
  float fg_slight_focus_max_coc;
  float bg_min_coc;
  float bg_max_coc;
  float bg_min_intersectable_coc;
};

struct CocTilePrediction {
  bool do_foreground;
  bool do_slight_focus;
  bool do_focus;
  bool do_background;
  bool do_holefill;
};

/* WATCH: Might have to change depending on the texture format. */
#define DOF_TILE_DEFOCUS 0.25
#define DOF_TILE_FOCUS 0.0
#define DOF_TILE_MIXED 0.75
#define DOF_TILE_LARGE_COC 1024.0

/* Init a CoC tile for reduction algorithms. */
CocTile dof_coc_tile_init(void)
{
  CocTile tile;
  tile.fg_min_coc = 0.0;
  tile.fg_max_coc = -DOF_TILE_LARGE_COC;
  tile.fg_max_intersectable_coc = DOF_TILE_LARGE_COC;
  tile.fg_slight_focus_max_coc = -1.0;
  tile.bg_min_coc = DOF_TILE_LARGE_COC;
  tile.bg_max_coc = 0.0;
  tile.bg_min_intersectable_coc = DOF_TILE_LARGE_COC;
  return tile;
}

CocTile dof_coc_tile_load(sampler2D fg_buffer, sampler2D bg_buffer, ivec2 tile_co)
{
  ivec2 tex_size = textureSize(fg_buffer, 0).xy;
  tile_co = clamp(tile_co, ivec2(0), tex_size - 1);

  vec4 fg = texelFetch(fg_buffer, tile_co, 0);
  vec3 bg = texelFetch(bg_buffer, tile_co, 0).xyz;

  CocTile tile;
  tile.fg_min_coc = -fg.x;
  tile.fg_max_coc = -fg.y;
  tile.fg_max_intersectable_coc = -fg.z;
  tile.fg_slight_focus_max_coc = fg.w;
  tile.bg_min_coc = bg.x;
  tile.bg_max_coc = bg.y;
  tile.bg_min_intersectable_coc = bg.z;
  return tile;
}

void dof_coc_tile_store(CocTile tile, out vec4 out_fg, out vec3 out_bg)
{
  out_fg.x = -tile.fg_min_coc;
  out_fg.y = -tile.fg_max_coc;
  out_fg.z = -tile.fg_max_intersectable_coc;
  out_fg.w = tile.fg_slight_focus_max_coc;
  out_bg.x = tile.bg_min_coc;
  out_bg.y = tile.bg_max_coc;
  out_bg.z = tile.bg_min_intersectable_coc;
}

CocTilePrediction dof_coc_tile_prediction_get(CocTile tile)
{
  /* Based on tile value, predict what pass we need to load. */
  CocTilePrediction predict;

  predict.do_foreground = (-tile.fg_min_coc > layer_threshold - layer_offset_fg);
  bool fg_fully_opaque = predict.do_foreground &&
                         dof_do_fast_gather(-tile.fg_min_coc, -tile.fg_max_coc, true);

  predict.do_slight_focus = !fg_fully_opaque && (tile.fg_slight_focus_max_coc >= 0.5);
  predict.do_focus = !fg_fully_opaque && (tile.fg_slight_focus_max_coc == DOF_TILE_FOCUS);

  predict.do_background = !predict.do_focus && !fg_fully_opaque &&
                          (tile.bg_max_coc > layer_threshold - layer_offset);
  bool bg_fully_opaque = predict.do_background &&
                         dof_do_fast_gather(-tile.bg_max_coc, tile.bg_min_coc, false);
  predict.do_holefill = !predict.do_focus && !fg_fully_opaque && -tile.fg_max_coc > 0.0;

#if 0 /* Debug */
  predict.do_foreground = predict.do_background = predict.do_holefill = true;
#endif
  return predict;
}

/* Special function to return the correct max value of 2 slight focus coc. */
float dof_coc_max_slight_focus(float coc1, float coc2)
{
  /* Do not consider values below 0.5 for expansion as they are "encoded".
   * See setup pass shader for more infos. */
  if ((coc1 == DOF_TILE_DEFOCUS && coc2 == DOF_TILE_FOCUS) ||
      (coc1 == DOF_TILE_FOCUS && coc2 == DOF_TILE_DEFOCUS)) {
    /* Tile where completely out of focus and in focus are both present.
     * Consider as very slightly out of focus. */
    return DOF_TILE_MIXED;
  }
  return max(coc1, coc2);
}

/* ------------------- GATHER UTILS ------------------- */

struct DofGatherData {
  vec4 color;
  float weight;
  float dist; /* TODO remove */
  /* For scatter occlusion. */
  float coc;
  float coc_sqr;
  /* For ring bucket merging. */
  float transparency;

  float layer_opacity;
};

#define GATHER_DATA_INIT DofGatherData(vec4(0.0), 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)

void dof_gather_ammend_weight(inout DofGatherData sample_data, float weight)
{
  sample_data.color *= weight;
  sample_data.coc *= weight;
  sample_data.coc_sqr *= weight;
  sample_data.weight *= weight;
}

void dof_gather_accumulate_sample(DofGatherData sample_data,
                                  float weight,
                                  inout DofGatherData accum_data)
{
  accum_data.color += sample_data.color * weight;
  accum_data.coc += sample_data.coc * weight;
  accum_data.coc_sqr += sample_data.coc * (sample_data.coc * weight);
  accum_data.weight += weight;
}

void dof_gather_accumulate_sample_pair(DofGatherData pair_data[2],
                                       float bordering_radius,
                                       float intersection_multiplier,
                                       bool first_ring,
                                       const bool do_fast_gather,
                                       const bool is_foreground,
                                       inout DofGatherData ring_data,
                                       inout DofGatherData accum_data)
{
  if (do_fast_gather) {
    for (int i = 0; i < 2; i++) {
      dof_gather_accumulate_sample(pair_data[i], 1.0, accum_data);
      accum_data.layer_opacity += 1.0;
    }
    return;
  }

#if 0
  const float mirroring_threshold = -layer_threshold - layer_offset;
  /* TODO(fclem) Promote to parameter? dither with Noise? */
  const float mirroring_min_distance = 15.0;
  if (pair_data[0].coc < mirroring_threshold &&
      (pair_data[1].coc - mirroring_min_distance) > pair_data[0].coc) {
    pair_data[1].coc = pair_data[0].coc;
  }
  else if (pair_data[1].coc < mirroring_threshold &&
           (pair_data[0].coc - mirroring_min_distance) > pair_data[1].coc) {
    pair_data[0].coc = pair_data[1].coc;
  }
#endif

  for (int i = 0; i < 2; i++) {
    float sample_weight = dof_sample_weight(pair_data[i].coc);
    float layer_weight = dof_layer_weight(pair_data[i].coc, is_foreground);
    float inter_weight = dof_intersection_weight(
        pair_data[i].coc, pair_data[i].dist, intersection_multiplier);
    float weight = inter_weight * layer_weight * sample_weight;

    /**
     * If a CoC is larger than bordering radius we accumulate it to the general accumulator.
     * If not, we accumulate to the ring bucket. This is to have more consistent sample occlusion.
     **/
    float accum_weight = dof_gather_accum_weight(pair_data[i].coc, bordering_radius, first_ring);
    dof_gather_accumulate_sample(pair_data[i], weight * accum_weight, accum_data);
    dof_gather_accumulate_sample(pair_data[i], weight * (1.0 - accum_weight), ring_data);

    accum_data.layer_opacity += layer_weight;

    if (is_foreground) {
      ring_data.transparency += 1.0 - inter_weight * layer_weight;
    }
    else {
      float coc = is_foreground ? -pair_data[i].coc : pair_data[i].coc;
      ring_data.transparency += saturate(coc - bordering_radius);
    }
  }
}

void dof_gather_accumulate_sample_ring(DofGatherData ring_data,
                                       int sample_count,
                                       bool first_ring,
                                       const bool do_fast_gather,
                                       /* accum_data occludes the ring_data if true. */
                                       const bool reversed_occlusion,
                                       inout DofGatherData accum_data)
{
  if (do_fast_gather) {
    /* Do nothing as ring_data contains nothing. All samples are already in accum_data. */
    return;
  }

  if (first_ring) {
    /* Layer opacity is directly accumulated into accum_data data. */
    accum_data.color = ring_data.color;
    accum_data.coc = ring_data.coc;
    accum_data.coc_sqr = ring_data.coc_sqr;
    accum_data.weight = ring_data.weight;

    accum_data.transparency = ring_data.transparency / float(sample_count);
    return;
  }

  if (ring_data.weight == 0.0) {
    return;
  }

  float ring_avg_coc = ring_data.coc / ring_data.weight;
  float accum_avg_coc = accum_data.coc / accum_data.weight;

  /* Smooth test to set opacity to see if the ring average coc occludes the accumulation.
   * Test is reversed to be multiplied against opacity. */
  float ring_occlu = saturate(accum_avg_coc - ring_avg_coc);
  /* The bias here is arbitrary. Seems to avoid weird looking foreground in most cases.
   * We might need to make it a parameter or find a relative bias. */
  float accum_occlu = saturate((ring_avg_coc - accum_avg_coc) * 0.1 - 1.0);

#ifdef DOF_RESOLVE_PASS
  ring_occlu = accum_occlu = 0.0;
#endif

  if (no_gather_occlusion) {
    ring_occlu = 0.0;
    accum_occlu = 0.0;
  }

  /* (Slide 40) */
  float ring_opacity = saturate(1.0 - ring_data.transparency / float(sample_count));
  float accum_opacity = 1.0 - accum_data.transparency;

  if (reversed_occlusion) {
    /* Accum_data occludes the ring. */
    float alpha = (accum_data.weight == 0.0) ? 0.0 : accum_opacity * accum_occlu;
    float one_minus_alpha = 1.0 - alpha;

    accum_data.color += ring_data.color * one_minus_alpha;
    accum_data.coc += ring_data.coc * one_minus_alpha;
    accum_data.coc_sqr += ring_data.coc_sqr * one_minus_alpha;
    accum_data.weight += ring_data.weight * one_minus_alpha;

    accum_data.transparency *= 1.0 - ring_opacity;
  }
  else {
    /* Ring occludes the accum_data (Same as reference). */
    float alpha = (accum_data.weight == 0.0) ? 1.0 : (ring_opacity * ring_occlu);
    float one_minus_alpha = 1.0 - alpha;

    accum_data.color = accum_data.color * one_minus_alpha + ring_data.color;
    accum_data.coc = accum_data.coc * one_minus_alpha + ring_data.coc;
    accum_data.coc_sqr = accum_data.coc_sqr * one_minus_alpha + ring_data.coc_sqr;
    accum_data.weight = accum_data.weight * one_minus_alpha + ring_data.weight;
  }
}

/* FIXME(fclem) Seems to be wrong since it needs ringcount+1 as input for slightfocus gather. */
int dof_gather_total_sample_count(const int ring_count, const int ring_density)
{
  return (ring_count * ring_count - ring_count) * ring_density + 1;
}

void dof_gather_accumulate_center_sample(DofGatherData center_data,
                                         float bordering_radius,
#ifdef DOF_RESOLVE_PASS
                                         int i_radius,
#endif
                                         const bool do_fast_gather,
                                         const bool is_foreground,
                                         inout DofGatherData accum_data)
{
  float layer_weight = dof_layer_weight(center_data.coc, is_foreground);
  float sample_weight = dof_sample_weight(center_data.coc);
  float weight = layer_weight * sample_weight;
  float accum_weight = dof_gather_accum_weight(center_data.coc, bordering_radius, false);

  if (do_fast_gather) {
    /* Hope for the compiler to optimize the above. */
    layer_weight = 1.0;
    sample_weight = 1.0;
    accum_weight = 1.0;
    weight = 1.0;
  }

  center_data.transparency = 1.0 - weight;

  dof_gather_accumulate_sample(center_data, weight * accum_weight, accum_data);

  if (!do_fast_gather) {
#ifdef DOF_RESOLVE_PASS
    /* NOTE(fclem): Hack to smooth transition to full in-focus opacity. */
    int total_sample_count = dof_gather_total_sample_count(i_radius + 1, DOF_SLIGHT_FOCUS_DENSITY);
    float fac = saturate(1.0 - abs(center_data.coc) / float(layer_threshold));
    accum_data.layer_opacity += float(total_sample_count) * fac * fac;
#endif
    accum_data.layer_opacity += layer_weight;

    /* Logic of dof_gather_accumulate_sample(). */
    weight *= (1.0 - accum_weight);
    center_data.coc_sqr = center_data.coc * (center_data.coc * weight);
    center_data.color *= weight;
    center_data.coc *= weight;
    center_data.weight = weight;

#ifdef DOF_FOREGROUND_PASS /* Reduce issue with closer foreground over distant foreground. */
    float ring_area = sqr(bordering_radius);
    dof_gather_ammend_weight(center_data, ring_area);
#endif

    /* Accumulate center as its own ring. */
    dof_gather_accumulate_sample_ring(
        center_data, 1, false, do_fast_gather, is_foreground, accum_data);
  }
}

int dof_gather_total_sample_count_with_density_change(const int ring_count,
                                                      const int ring_density,
                                                      int density_change)
{
  int sample_count_per_density_change = dof_gather_total_sample_count(ring_count, ring_density) -
                                        dof_gather_total_sample_count(
                                            ring_count - gather_density_change_ring, ring_density);

  return dof_gather_total_sample_count(ring_count, ring_density) +
         sample_count_per_density_change * density_change;
}

void dof_gather_accumulate_resolve(int total_sample_count,
                                   DofGatherData accum_data,
                                   out vec4 out_col,
                                   out float out_weight,
                                   out vec2 out_occlusion)
{
  float weight_inv = safe_rcp(accum_data.weight);
  out_col = accum_data.color * weight_inv;
  out_occlusion = vec2(abs(accum_data.coc), accum_data.coc_sqr) * weight_inv;

#ifdef DOF_FOREGROUND_PASS
  out_weight = 1.0 - accum_data.transparency;
#else
  if (accum_data.weight > 0.0) {
    out_weight = accum_data.layer_opacity / float(total_sample_count);
  }
  else {
    out_weight = 0.0;
  }
#endif
  /* Gathering may not accumulate to 1.0 alpha because of float precision. */
  if (out_weight > 0.99) {
    out_weight = 1.0;
  }
  else if (out_weight < 0.01) {
    out_weight = 0.0;
  }
  /* Same thing for alpha channel. */
  if (out_col.a > 0.99) {
    out_col.a = 1.0;
  }
  else if (out_col.a < 0.01) {
    out_col.a = 0.0;
  }
}

ivec2 dof_square_ring_sample_offset(int ring_distance, int sample_id)
{
  /**
   * Generate samples in a square pattern with the ring radius. X is the center tile.
   *
   *    Dist1          Dist2
   *                 6 5 4 3 2
   *    3 2 1        7       1
   *    . X 0        .   X   0
   *    . . .        .       .
   *                 . . . . .
   *
   * Samples are expected to be mirrored to complete the pattern.
   **/
  ivec2 offset;
  if (sample_id < ring_distance) {
    offset.x = ring_distance;
    offset.y = sample_id;
  }
  else if (sample_id < ring_distance * 3) {
    offset.x = ring_distance - sample_id + ring_distance;
    offset.y = ring_distance;
  }
  else {
    offset.x = -ring_distance;
    offset.y = ring_distance - sample_id + 3 * ring_distance;
  }
  return offset;
}