/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_gpencil_lib.glsl)

void gpencil_color_output(vec4 stroke_col, vec4 vert_col, float vert_strength, float mix_tex)
{
  /* Mix stroke with other colors. */
  vec4 mixed_col = stroke_col;
  mixed_col.rgb = mix(mixed_col.rgb, vert_col.rgb, vert_col.a * gpVertexColorOpacity);
  mixed_col.rgb = mix(mixed_col.rgb, gpLayerTint.rgb, gpLayerTint.a);
  mixed_col.a *= vert_strength * gpLayerOpacity;
  /**
   * This is what the fragment shader looks like.
   * out = col * gp_interp.color_mul + col.a * gp_interp.color_add.
   * gp_interp.color_mul is how much of the texture color to keep.
   * gp_interp.color_add is how much of the mixed color to add.
   * Note that we never add alpha. This is to keep the texture act as a stencil.
   * We do however, modulate the alpha (reduce it).
   */
  /* We add the mixed color. This is 100% mix (no texture visible). */
  gp_interp.color_mul = vec4(mixed_col.aaa, mixed_col.a);
  gp_interp.color_add = vec4(mixed_col.rgb * mixed_col.a, 0.0);
  /* Then we blend according to the texture mix factor.
   * Note that we keep the alpha modulation. */
  gp_interp.color_mul.rgb *= mix_tex;
  gp_interp.color_add.rgb *= 1.0 - mix_tex;
}

void main()
{
  float vert_strength;
  vec4 vert_color;
  vec3 vert_N;

  ivec4 ma1 = floatBitsToInt(texelFetch(gp_pos_tx, gpencil_stroke_point_id() * 3 + 1));
  gpMaterial gp_mat = gp_materials[ma1.x + gpMaterialOffset];
  gpMaterialFlag gp_flag = floatBitsToUint(gp_mat._flag);

  gl_Position = gpencil_vertex(vec4(viewportSize, 1.0 / viewportSize),
                               gp_flag,
                               gp_mat._alignment_rot,
                               gp_interp.pos,
                               vert_N,
                               vert_color,
                               vert_strength,
                               gp_interp.uv,
                               gp_interp_flat.sspos,
                               gp_interp_flat.aspect,
                               gp_interp_noperspective.thickness,
                               gp_interp_noperspective.hardness);

  if (gpencil_is_stroke_vertex()) {
    if (!flag_test(gp_flag, GP_STROKE_ALIGNMENT)) {
      gp_interp.uv.x *= gp_mat._stroke_u_scale;
    }

    /* Special case: We don't use vertex color if material Holdout. */
    if (flag_test(gp_flag, GP_STROKE_HOLDOUT)) {
      vert_color = vec4(0.0);
    }

    gpencil_color_output(
        gp_mat.stroke_color, vert_color, vert_strength, gp_mat._stroke_texture_mix);

    gp_interp_flat.mat_flag = gp_flag & ~GP_FILL_FLAGS;

    if (gpStrokeOrder3d) {
      /* Use the fragment depth (see fragment shader). */
      gp_interp_flat.depth = -1.0;
    }
    else if (flag_test(gp_flag, GP_STROKE_OVERLAP)) {
      /* Use the index of the point as depth.
       * This means the stroke can overlap itself. */
      float point_index = float(ma1.z);
      gp_interp_flat.depth = (point_index + gpStrokeIndexOffset + 2.0) * 0.0000002;
    }
    else {
      /* Use the index of first point of the stroke as depth.
       * We render using a greater depth test this means the stroke
       * cannot overlap itself.
       * We offset by one so that the fill can be overlapped by its stroke.
       * The offset is ok since we pad the strokes data because of adjacency infos. */
      float stroke_index = float(ma1.y);
      gp_interp_flat.depth = (stroke_index + gpStrokeIndexOffset + 2.0) * 0.0000002;
    }
  }
  else {
    int stroke_point_id = gpencil_stroke_point_id();
    vec4 uv1 = texelFetch(gp_pos_tx, stroke_point_id * 3 + 2);
    vec4 fcol1 = texelFetch(gp_col_tx, stroke_point_id * 2 + 1);
    vec4 fill_col = gp_mat.fill_color;

    /* Special case: We don't modulate alpha in gradient mode. */
    if (flag_test(gp_flag, GP_FILL_GRADIENT_USE)) {
      fill_col.a = 1.0;
    }

    /* Decode fill opacity. */
    vec4 fcol_decode = vec4(fcol1.rgb, floor(fcol1.a / 10.0));
    float fill_opacity = fcol1.a - (fcol_decode.a * 10);
    fcol_decode.a /= 10000.0;

    /* Special case: We don't use vertex color if material Holdout. */
    if (flag_test(gp_flag, GP_FILL_HOLDOUT)) {
      fcol_decode = vec4(0.0);
    }

    /* Apply opacity. */
    fill_col.a *= fill_opacity;
    /* If factor is > 1 force opacity. */
    if (fill_opacity > 1.0) {
      fill_col.a += fill_opacity - 1.0;
    }

    fill_col.a = clamp(fill_col.a, 0.0, 1.0);

    gpencil_color_output(fill_col, fcol_decode, 1.0, gp_mat._fill_texture_mix);

    gp_interp_flat.mat_flag = gp_flag & GP_FILL_FLAGS;
    gp_interp_flat.mat_flag |= uint(ma1.x + gpMaterialOffset) << GPENCIl_MATID_SHIFT;

    gp_interp.uv = mat2(gp_mat.fill_uv_rot_scale.xy, gp_mat.fill_uv_rot_scale.zw) * uv1.xy +
                   gp_mat._fill_uv_offset;

    if (gpStrokeOrder3d) {
      /* Use the fragment depth (see fragment shader). */
      gp_interp_flat.depth = -1.0;
    }
    else {
      /* Use the index of first point of the stroke as depth. */
      float stroke_index = float(ma1.y);
      gp_interp_flat.depth = (stroke_index + gpStrokeIndexOffset + 1.0) * 0.0000002;
    }
  }
}
