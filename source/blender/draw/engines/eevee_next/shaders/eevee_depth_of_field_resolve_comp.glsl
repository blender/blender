
/**
 * Recombine Pass: Load separate convolution layer and composite with self
 * slight defocus convolution and in-focus fields.
 *
 * The halfres gather methods are fast but lack precision for small CoC areas.
 * To fix this we do a bruteforce gather to have a smooth transition between
 * in-focus and defocus regions.
 */

#pragma BLENDER_REQUIRE(eevee_depth_of_field_accumulator_lib.glsl)

shared uint shared_max_slight_focus_abs_coc;

/**
 * Returns The max CoC in the Slight Focus range inside this compute tile.
 */
float dof_slight_focus_coc_tile_get(vec2 frag_coord)
{
  if (all(equal(gl_LocalInvocationID, uvec3(0)))) {
    shared_max_slight_focus_abs_coc = floatBitsToUint(0.0);
  }
  barrier();

  float local_abs_max = 0.0;
  /* Sample in a cross (X) pattern. This covers all pixels over the whole tile, as long as
   * dof_max_slight_focus_radius is less than the group size. */
  for (int i = 0; i < 4; i++) {
    vec2 sample_uv = (frag_coord + quad_offsets[i] * 2.0 * dof_max_slight_focus_radius) /
                     vec2(textureSize(color_tx, 0));
    float coc = dof_coc_from_depth(dof_buf, sample_uv, textureLod(depth_tx, sample_uv, 0.0).r);
    if (abs(coc) < dof_max_slight_focus_radius) {
      local_abs_max = max(local_abs_max, abs(coc));
    }
  }
  /* Use atomic reduce operation. */
  atomicMax(shared_max_slight_focus_abs_coc, floatBitsToUint(local_abs_max));
  /* "Broadcast" result accross all threads. */
  barrier();

  return uintBitsToFloat(shared_max_slight_focus_abs_coc);
}

void main()
{
  vec2 frag_coord = vec2(gl_GlobalInvocationID.xy) + 0.5;
  ivec2 tile_co = ivec2(frag_coord / float(DOF_TILES_SIZE * 2));

  CocTile coc_tile = dof_coc_tile_load(in_tiles_fg_img, in_tiles_bg_img, tile_co);
  CocTilePrediction prediction = dof_coc_tile_prediction_get(coc_tile);

  vec2 uv = frag_coord / vec2(textureSize(color_tx, 0));
  vec2 uv_halfres = (frag_coord * 0.5) / vec2(textureSize(color_bg_tx, 0));

  float slight_focus_max_coc = 0.0;
  if (prediction.do_slight_focus) {
    slight_focus_max_coc = dof_slight_focus_coc_tile_get(frag_coord);
    prediction.do_slight_focus = slight_focus_max_coc > 0.5;
    if (prediction.do_slight_focus) {
      prediction.do_focus = false;
    }
  }

  if (prediction.do_focus) {
    float center_coc = (dof_coc_from_depth(dof_buf, uv, textureLod(depth_tx, uv, 0.0).r));
    prediction.do_focus = abs(center_coc) <= 0.5;
  }

  vec4 out_color = vec4(0.0);
  float weight = 0.0;

  vec4 layer_color;
  float layer_weight;

  if (!no_hole_fill_pass && prediction.do_hole_fill) {
    layer_color = textureLod(color_hole_fill_tx, uv_halfres, 0.0);
    layer_weight = textureLod(weight_hole_fill_tx, uv_halfres, 0.0).r;
    out_color = layer_color * safe_rcp(layer_weight);
    weight = float(layer_weight > 0.0);
  }

  if (!no_background_pass && prediction.do_background) {
    layer_color = textureLod(color_bg_tx, uv_halfres, 0.0);
    layer_weight = textureLod(weight_bg_tx, uv_halfres, 0.0).r;
    /* Always prefer background to hole_fill pass. */
    layer_color *= safe_rcp(layer_weight);
    layer_weight = float(layer_weight > 0.0);
    /* Composite background. */
    out_color = out_color * (1.0 - layer_weight) + layer_color;
    weight = weight * (1.0 - layer_weight) + layer_weight;
    /* Fill holes with the composited background. */
    out_color *= safe_rcp(weight);
    weight = float(weight > 0.0);
  }

  if (!no_slight_focus_pass && prediction.do_slight_focus) {
    dof_slight_focus_gather(
        depth_tx, color_tx, bokeh_lut_tx, slight_focus_max_coc, layer_color, layer_weight);
    /* Composite slight defocus. */
    out_color = out_color * (1.0 - layer_weight) + layer_color;
    weight = weight * (1.0 - layer_weight) + layer_weight;
  }

  if (!no_focus_pass && prediction.do_focus) {
    layer_color = safe_color(textureLod(color_tx, uv, 0.0));
    layer_weight = 1.0;
    /* Composite in focus. */
    out_color = out_color * (1.0 - layer_weight) + layer_color;
    weight = weight * (1.0 - layer_weight) + layer_weight;
  }

  if (!no_foreground_pass && prediction.do_foreground) {
    layer_color = textureLod(color_fg_tx, uv_halfres, 0.0);
    layer_weight = textureLod(weight_fg_tx, uv_halfres, 0.0).r;
    /* Composite foreground. */
    out_color = out_color * (1.0 - layer_weight) + layer_color;
  }

  /* Fix float precision issue in alpha compositing.  */
  if (out_color.a > 0.99) {
    out_color.a = 1.0;
  }

  if (debug_resolve_perf && prediction.do_slight_focus) {
    out_color.rgb *= vec3(1.0, 0.1, 0.1);
  }

  imageStore(out_color_img, ivec2(gl_GlobalInvocationID.xy), out_color);
}
