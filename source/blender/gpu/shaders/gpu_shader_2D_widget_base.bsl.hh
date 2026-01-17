/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once
#pragma create_info

#include "gpu_shader_compat.hh"

#include "GPU_shader_shared.hh"
#include "gpu_interface_infos.hh"
#include "gpu_shader_colorspace_lib.glsl"
#include "gpu_shader_create_info.hh"

/* TODO(fclem): Share with C code. */
#define MAX_PARAM 12
#define MAX_INSTANCE 6

namespace builtin::widget {

struct VertOut {
  [[flat]] float discard_fac;
  [[flat]] float line_width;
  [[flat]] float2 out_rect_size;
  [[flat]] float4 border_color;
  [[flat]] float4 emboss_color;
  [[flat]] float4 out_round_corners;
  [[no_perspective]] float but_co;
  [[no_perspective]] float2 uv_interp;
  [[no_perspective]] float4 inner_color;
};

struct [[host_shared]] WidgetRaw {
  float4 data[12];
};

struct [[host_shared]] Widget {
  float4 recti;
  float4 rect;

  float radsi;
  float rads;
  float2 faci;

  float4 round_corners;
  float4 color_inner1;
  float4 color_inner2;
  float4 color_edge;
  float4 color_emboss;
  float4 color_tria;

  float2 tria1_center;
  float2 tria2_center;

  float tria1_size;
  float tria2_size;
  float shade_dir;
  float alpha_discard;

  float tria_type;
  float _pad0;
  float _pad1;
  float _pad2;

  /* We encode alpha check and discard factor together. */
  bool do_alpha_check() const
  {
    return alpha_discard < 0.0f;
  }
  float discard_factor() const
  {
    return abs(alpha_discard);
  }

  VertOut do_widget(int vert_id, float2 &pos)
  {
    VertOut v_out;
    /* Offset to avoid losing pixels (mimics conservative rasterization). */
    constexpr float2 ofs = float2(0.5f, -0.5f);
    v_out.line_width = abs(rect.x - recti.x);
    float2 emboss_ofs = float2(0.0f, -v_out.line_width);

    switch (vert_id) {
      default:
      case 0: {
        pos = rect.xz + emboss_ofs + ofs.yy;
        break;
      }
      case 1: {
        pos = rect.xw + ofs.yx;
        break;
      }
      case 2: {
        pos = rect.yz + emboss_ofs + ofs.xy;
        break;
      }
      case 3: {
        pos = rect.yw + ofs.xx;
        break;
      }
    }

    v_out.uv_interp = pos - rect.xz;
    v_out.out_rect_size = rect.yw - rect.xz;
    v_out.out_round_corners = rads * round_corners;

    float2 uv = v_out.uv_interp / v_out.out_rect_size;
    float fac = clamp((shade_dir > 0.0f) ? uv.y : uv.x, 0.0f, 1.0f);
    /* Note innerColor is premultiplied inside the fragment shader. */
    if (do_alpha_check()) {
      v_out.inner_color = color_inner1;
      v_out.but_co = uv.x;
    }
    else {
      v_out.inner_color = mix(color_inner2, color_inner1, fac);
      v_out.but_co = -abs(uv.x);
    }

    /* We need premultiplied color for transparency. */
    v_out.border_color = color_edge * float4(color_edge.aaa, 1.0f);
    v_out.emboss_color = color_emboss * float4(color_emboss.aaa, 1.0f);

    return v_out;
  }

  VertOut do_tria(int vert_id, float2 &pos)
  {
    VertOut v_out;
    int vidx = vert_id % 4;
    bool tria2 = vert_id > 7;

    pos = float2(0.0f);
    float size = (tria2) ? -tria2_size : tria1_size;
    float2 center = (tria2) ? tria2_center : tria1_center;

    float2 arrow_pos[] = {
        float2(0.0f, 0.6f), float2(0.6f, 0.0f), float2(-0.6f, 0.0f), float2(0.0f, -0.6f)};
    /* Rotated uv space by 45deg and mirrored. */
    float2 arrow_uvs[] = {
        float2(0.0f, 0.85f), float2(0.85f, 0.85f), float2(0.0f, 0.0f), float2(0.0f, 0.85f)};

    float2 point_pos[] = {
        float2(-1.0f, -1.0f), float2(-1.0f, 1.0f), float2(1.0f, -1.0f), float2(1.0f, 1.0f)};
    float2 point_uvs[] = {
        float2(0.0f, 0.0f), float2(0.0f, 1.0f), float2(1.0f, 0.0f), float2(1.0f, 1.0f)};

    /* We reuse the SDF round-box rendering of widget to render the tria shapes.
     * This means we do clever tricks to position the rectangle the way we want using
     * the 2 triangles uvs. */
    if (tria_type == 0.0f) {
      /* ROUNDBOX_TRIA_NONE */
      v_out.out_rect_size = v_out.uv_interp = pos = float2(0);
      v_out.out_round_corners = float4(0.01f);
    }
    else if (tria_type == 1.0f) {
      /* ROUNDBOX_TRIA_ARROWS */
      pos = arrow_pos[vidx];
      v_out.uv_interp = arrow_uvs[vidx];
      v_out.uv_interp -= float2(0.05f, 0.63f); /* Translate */
      v_out.out_rect_size = float2(0.74f, 0.17f);
      v_out.out_round_corners = float4(0.08f);
    }
    else if (tria_type == 2.0f) {
      /* ROUNDBOX_TRIA_SCROLL */
      pos = point_pos[vidx];
      v_out.uv_interp = point_uvs[vidx];
      v_out.out_rect_size = float2(1.0f);
      v_out.out_round_corners = float4(0.5f);
    }
    else if (tria_type == 3.0f) {
      /* ROUNDBOX_TRIA_MENU */
      pos = tria2 ? float2(0.0f) : arrow_pos[vidx]; /* Solo tria */
      pos = float2(pos.y, -pos.x);                  /* Rotate */
      pos += float2(-0.05f, 0.0f);                  /* Translate */
      size *= 0.8f;                                 /* Scale */
      v_out.uv_interp = arrow_uvs[vidx];
      v_out.uv_interp -= float2(0.05f, 0.63f); /* Translate */
      v_out.out_rect_size = float2(0.74f, 0.17f);
      v_out.out_round_corners = float4(0.01f);
    }
    else if (tria_type == 4.0f) {
      /* ROUNDBOX_TRIA_CHECK */
      /* A bit more hacky: We use the two triangles joined together to render
       * both sides of the check-mark with different length. */
      pos = arrow_pos[min(vidx, 2)];  /* Only keep 1 triangle. */
      pos.y = tria2 ? -pos.y : pos.y; /* Mirror along X */
      pos = pos.x * float2(0.0872f, -0.996f) +
            pos.y * float2(0.996f, 0.0872f); /* Rotate (85deg) */
      pos += float2(-0.1f, 0.2f);            /* Translate */
      center = tria1_center;
      size = tria1_size * 1.7f; /* Scale */
      v_out.uv_interp = arrow_uvs[vidx];
      v_out.uv_interp -= tria2 ? float2(0.4f, 0.65f) : float2(0.08f, 0.65f); /* Translate */
      v_out.out_rect_size = float2(0.74f, 0.14f);
      v_out.out_round_corners = float4(0.01f);
    }
    else if (tria_type == 5.0f) {
      /* ROUNDBOX_TRIA_HOLD_ACTION_ARROW */
      /* We use a single triangle to cut the round rect in half.
       * The edge will not be Anti-aliased. */
      pos = tria2 ? float2(0.0f) : arrow_pos[min(vidx, 2)]; /* Only keep 1 triangle. */
      pos = pos.x * float2(0.707f, 0.707f) + pos.y * float2(-0.707f, 0.707f); /* Rotate (45deg)
                                                                               */
      pos += float2(-1.7f, 2.4f); /* Translate (hard-coded, might want to remove). */
      size *= 0.4f;               /* Scale */
      v_out.uv_interp = arrow_uvs[vidx];
      v_out.uv_interp -= float2(0.05f, 0.05f); /* Translate */
      v_out.out_rect_size = float2(0.75f);
      v_out.out_round_corners = float4(0.01f);
    }
    else if (tria_type == 6.0f) {
      /* ROUNDBOX_TRIA_DASH */
      pos = point_pos[vidx];
      v_out.uv_interp = point_uvs[vidx];
      v_out.uv_interp -= float2(0.2f, 0.45f); /* Translate */
      v_out.out_rect_size = float2(0.6f, 0.1f);
      v_out.out_round_corners = float4(0.01f);
    }

    v_out.uv_interp *= abs(size);
    v_out.out_rect_size *= abs(size);
    v_out.out_round_corners *= abs(size);

    pos = pos * size + center;

    v_out.inner_color = color_tria * float4(color_tria.aaa, 1.0f);

    v_out.line_width = 0.0f;
    v_out.border_color = float4(0.0f);
    v_out.emboss_color = float4(0.0f);

    v_out.but_co = -2.0f;

    return v_out;
  }
};

/* WORKAROUND: We cannot use structs with push constants, so we push a float4 array and reinterpret
 * using a union. */
struct WidgetUnion {
  union {
    union_t<WidgetRaw> raw;
    union_t<Widget> data;
  };
};

struct Resources {
  [[legacy_info]] ShaderCreateInfo gpu_srgb_to_framebuffer_space;

  [[push_constant]] const float4x4 ModelViewProjectionMatrix;
  [[push_constant]] const float3 checkerColorAndSize;

  [[compilation_constant]] const bool instanced;
  [[push_constant, condition(instanced)]] const float4 parameters_inst[MAX_PARAM * MAX_INSTANCE];
  [[push_constant, condition(!instanced)]] const float4 parameters[MAX_PARAM];

  /** Unpack widget data passed as raw array of float4 through push constants. */
  Widget get_widget(int index)
  {
    /* Hopefully, all of these move instructions are optimized out. */
    WidgetRaw raw;
    if (this->instanced) [[static_branch]] {
      for (int i = 0; i < 12; i++) [[unroll]] {
        raw.data[i] = parameters_inst[index * MAX_PARAM + i];
      }
    }
    else {
      for (int i = 0; i < 12; i++) [[unroll]] {
        raw.data[i] = parameters[i];
      }
    }
    /* Equivalent of reinterpret_cast. */
    WidgetUnion widget;
    widget.raw() = raw;
    return widget.data();
  }

  float4 do_checkerboard(float2 frag_co)
  {
    float size = checkerColorAndSize.z;
    float2 phase = mod(frag_co.xy, size * 2.0f);

    if ((phase.x > size && phase.y < size) || (phase.x < size && phase.y > size)) {
      return float4(checkerColorAndSize.xxx, 1.0f);
    }
    return float4(checkerColorAndSize.yyy, 1.0f);
  }
};

[[vertex]] void vert([[vertex_id]] const int vert_id,
                     [[instance_id]] const int inst_id,
                     [[resource_table]] Resources &srt,
                     [[out]] VertOut &v_out,
                     [[position]] float4 &position)
{
  Widget widget = srt.get_widget(inst_id);

  bool is_tria = (vert_id > 3);
  float2 pos;
  VertOut vert_out = (is_tria) ? widget.do_tria(vert_id, pos) : widget.do_widget(vert_id, pos);

  /* WORKAROUND: Quirk of current BSL implementation.
   * Current implementation doesn't allow to assign the output struct at once. */
  v_out.discard_fac = widget.discard_factor();
  v_out.line_width = vert_out.line_width;
  v_out.out_rect_size = vert_out.out_rect_size;
  v_out.border_color = vert_out.border_color;
  v_out.emboss_color = vert_out.emboss_color;
  v_out.out_round_corners = vert_out.out_round_corners;
  v_out.but_co = vert_out.but_co;
  v_out.uv_interp = vert_out.uv_interp;
  v_out.inner_color = vert_out.inner_color;

  position = srt.ModelViewProjectionMatrix * float4(pos, 0.0f, 1.0f);
}

struct FragOut {
  [[frag_color(0)]] float4 color;
};

[[fragment]] void frag([[in]] const VertOut &v_out,
                       [[out]] FragOut &frag_out,
                       [[frag_coord]] const float4 frag_co,
                       [[resource_table]] Resources &srt)
{
  if (min(1.0f, -v_out.but_co) > v_out.discard_fac) {
    gpu_discard_fragment();
  }

  float2 uv = v_out.uv_interp;

  bool upper_half = uv.y > v_out.out_rect_size.y * 0.5f;
  bool right_half = uv.x > v_out.out_rect_size.x * 0.5f;
  float corner_rad;

  /* Correct aspect ratio for 2D views not using uniform scaling.
   * uv is already in pixel space so a uniform scale should give us a ratio of 1. */
  float ratio = (v_out.but_co != -2.0f) ? abs(gpu_dfdy(uv.y) / gpu_dfdx(uv.x)) : 1.0f;
  float2 uv_sdf = uv;
  uv_sdf.x *= ratio;

  if (right_half) {
    uv_sdf.x = v_out.out_rect_size.x * ratio - uv_sdf.x;
  }
  if (upper_half) {
    uv_sdf.y = v_out.out_rect_size.y - uv_sdf.y;
    corner_rad = right_half ? v_out.out_round_corners.z : v_out.out_round_corners.w;
  }
  else {
    corner_rad = right_half ? v_out.out_round_corners.y : v_out.out_round_corners.x;
  }

  /* Fade emboss at the border. */
  float emboss_size = upper_half ? 0.0f : min(1.0f, uv_sdf.x / (corner_rad * ratio));

  /* Signed distance field from the corner (in pixel).
   * inner_sdf is sharp and outer_sdf is rounded. */
  uv_sdf -= corner_rad;
  float inner_sdf = max(0.0f, min(uv_sdf.x, uv_sdf.y));
  float outer_sdf = -length(min(uv_sdf, 0.0f));
  float sdf = inner_sdf + outer_sdf + corner_rad;

  /* Clamp line width to be at least 1px wide. This can happen if the projection matrix
   * has been scaled (i.e: Node editor)... */
  float line_width = (v_out.line_width > 0.0f) ? max(gpu_fwidth(uv.y), v_out.line_width) : 0.0f;

  constexpr float aa_radius = 0.5f;
  float3 masks;
  masks.x = smoothstep(-aa_radius, aa_radius, sdf);
  masks.y = smoothstep(-aa_radius, aa_radius, sdf - line_width);
  masks.z = smoothstep(-aa_radius, aa_radius, sdf + line_width * emboss_size);

  /* Compose masks together to avoid having too much alpha. */
  masks.zx = max(float2(0.0f), masks.zx - masks.xy);

  if (v_out.but_co > 0.0f) {
    /* Alpha checker widget. */
    if (v_out.but_co > 0.5f) {
      float4 checker = srt.do_checkerboard(frag_co.xy);
      frag_out.color = mix(checker, v_out.inner_color, v_out.inner_color.a);
    }
    else {
      /* Set alpha to 1.0f. */
      frag_out.color = v_out.inner_color;
    }
    frag_out.color.a = 1.0f;
  }
  else {
    /* Pre-multiply here. */
    frag_out.color = v_out.inner_color * float4(v_out.inner_color.aaa, 1.0f);
  }
  frag_out.color *= masks.y;
  frag_out.color += masks.x * v_out.border_color;
  frag_out.color += masks.z * v_out.emboss_color;

  /* Un-pre-multiply because the blend equation is already doing the multiplication. */
  if (frag_out.color.a > 0.0f) {
    frag_out.color.rgb /= frag_out.color.a;
  }

  frag_out.color = blender_srgb_to_framebuffer_space(frag_out.color);
}

}  // namespace builtin::widget

PipelineGraphic gpu_shader_2D_widget_base(builtin::widget::vert,
                                          builtin::widget::frag,
                                          builtin::widget::Resources{.instanced = false});
PipelineGraphic gpu_shader_2D_widget_base_inst(builtin::widget::vert,
                                               builtin::widget::frag,
                                               builtin::widget::Resources{.instanced = true});
