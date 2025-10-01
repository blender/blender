/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpencil_infos.hh"

VERTEX_SHADER_CREATE_INFO(gpencil_geometry)

#include "draw_grease_pencil_lib.glsl"

void gpencil_color_output(float4 stroke_col, float4 vert_col, float vert_strength, float mix_tex)
{
  /* Mix stroke with other colors. */
  float4 mixed_col = stroke_col;
  mixed_col.rgb = mix(mixed_col.rgb, vert_col.rgb, vert_col.a * gp_vertex_color_opacity);
  mixed_col.rgb = mix(mixed_col.rgb, gp_layer_tint.rgb, gp_layer_tint.a);
  mixed_col.a *= vert_strength * gp_layer_opacity;
  /**
   * This is what the fragment shader looks like.
   * out = col * gp_interp.color_mul + col.a * gp_interp.color_add.
   * gp_interp.color_mul is how much of the texture color to keep.
   * gp_interp.color_add is how much of the mixed color to add.
   * Note that we never add alpha. This is to keep the texture act as a stencil.
   * We do however, modulate the alpha (reduce it).
   */
  /* We add the mixed color. This is 100% mix (no texture visible). */
  gp_interp.color_mul = float4(mixed_col.aaa, mixed_col.a);
  gp_interp.color_add = float4(mixed_col.rgb * mixed_col.a, 0.0f);
  /* Then we blend according to the texture mix factor.
   * Note that we keep the alpha modulation. */
  gp_interp.color_mul.rgb *= mix_tex;
  gp_interp.color_add.rgb *= 1.0f - mix_tex;
}

void main()
{
  float vert_strength;
  float4 vert_color;
  float3 vert_N;

  int4 ma1 = floatBitsToInt(texelFetch(gp_pos_tx, gpencil_stroke_point_id() * 3 + 1));
  PointData point_data1 = decode_ma(ma1);
  gpMaterial gp_mat = gp_materials[point_data1.mat + gp_material_offset];
  gpMaterialFlag gp_flag = gpMaterialFlag(floatBitsToUint(gp_mat._flag));

  gl_Position = gpencil_vertex(float4(viewport_size, 1.0f / viewport_size),
                               gp_flag,
                               gp_mat._alignment_rot,
                               gp_interp.pos,
                               vert_N,
                               vert_color,
                               vert_strength,
                               gp_interp.uv,
                               gp_interp_flat.sspos,
                               gp_interp_flat.sspos_adj,
                               gp_interp_flat.aspect,
                               gp_interp_noperspective.thickness,
                               gp_interp_noperspective.hardness);

  if (gpencil_is_stroke_vertex()) {
    if (!flag_test(gp_flag, GP_STROKE_ALIGNMENT)) {
      gp_interp.uv.x *= gp_mat._stroke_u_scale;
    }

    /* Special case: We don't use vertex color if material Holdout. */
    if (flag_test(gp_flag, GP_STROKE_HOLDOUT)) {
      vert_color = float4(0.0f);
    }

    gpencil_color_output(
        gp_mat.stroke_color, vert_color, vert_strength, gp_mat._stroke_texture_mix);

    gp_interp_flat.mat_flag = gp_flag & ~GP_FILL_FLAGS;

    if (gp_stroke_order3d) {
      /* Use the fragment depth (see fragment shader). */
      gp_interp_flat.depth = -1.0f;
    }
    else if (flag_test(gp_flag, GP_STROKE_OVERLAP)) {
      /* Use the index of the point as depth.
       * This means the stroke can overlap itself. */
      gp_interp_flat.depth = (point_data1.point_id + gp_stroke_index_offset + 2.0f) * 0.0000002f;
    }
    else {
      /* Use the index of first point of the stroke as depth.
       * We render using a greater depth test this means the stroke
       * cannot overlap itself.
       * We offset by one so that the fill can be overlapped by its stroke.
       * The offset is ok since we pad the strokes data because of adjacency infos. */
      gp_interp_flat.depth = (point_data1.stroke_id + gp_stroke_index_offset + 2.0f) * 0.0000002f;
    }
  }
  else {
    int stroke_point_id = gpencil_stroke_point_id();
    float4 uv1 = texelFetch(gp_pos_tx, stroke_point_id * 3 + 2);
    float4 fcol1 = texelFetch(gp_col_tx, stroke_point_id * 2 + 1);
    float4 fill_col = gp_mat.fill_color;

    /* Special case: We don't modulate alpha in gradient mode. */
    if (flag_test(gp_flag, GP_FILL_GRADIENT_USE)) {
      fill_col.a = 1.0f;
    }

    /* Decode fill opacity. */
    float4 fcol_decode = float4(fcol1.rgb, floor(fcol1.a / 10.0f));
    float fill_opacity = fcol1.a - (fcol_decode.a * 10);
    fcol_decode.a /= 10000.0f;

    /* Special case: We don't use vertex color if material Holdout. */
    if (flag_test(gp_flag, GP_FILL_HOLDOUT)) {
      fcol_decode = float4(0.0f);
    }

    /* Apply opacity. */
    fill_col.a *= fill_opacity;
    /* If factor is > 1 force opacity. */
    if (fill_opacity > 1.0f) {
      fill_col.a += fill_opacity - 1.0f;
    }

    fill_col.a = clamp(fill_col.a, 0.0f, 1.0f);

    gpencil_color_output(fill_col, fcol_decode, 1.0f, gp_mat._fill_texture_mix);

    gp_interp_flat.mat_flag = gp_flag & GP_FILL_FLAGS;
    gp_interp_flat.mat_flag |= uint(point_data1.mat + gp_material_offset) << GPENCIl_MATID_SHIFT;

    gp_interp.uv = float2x2(gp_mat.fill_uv_rot_scale.xy, gp_mat.fill_uv_rot_scale.zw) * uv1.xy +
                   gp_mat._fill_uv_offset;

    if (gp_stroke_order3d) {
      /* Use the fragment depth (see fragment shader). */
      gp_interp_flat.depth = -1.0f;
    }
    else {
      /* Use the index of first point of the stroke as depth. */
      gp_interp_flat.depth = (point_data1.stroke_id + gp_stroke_index_offset + 1.0f) * 0.0000002f;
    }
  }
}
