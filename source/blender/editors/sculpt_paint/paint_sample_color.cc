/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include <cmath>
#include <cstdlib>

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "DNA_brush_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_math_color.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.hh"
#include "BLI_rect.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_brush.hh"
#include "BKE_bvhutils.hh"
#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_image.hh"
#include "BKE_layer.hh"
#include "BKE_material.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_sample.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_paint_types.hh"
#include "BKE_report.hh"

#include "DEG_depsgraph_query.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "IMB_imbuf_types.hh"
#include "IMB_interp.hh"

#include "ED_grease_pencil.hh"
#include "ED_image.hh"
#include "ED_screen.hh"
#include "ED_view3d.hh"

#include "ED_mesh.hh" /* for face mask functions */

#include "UI_interface_types.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "IMB_colormanagement.hh"

#include "paint_intern.hh"

#include "sculpt_color.hh"
#include "sculpt_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Sample Color Operator
 * \{ */

namespace blender::ed::sculpt_paint {

/* compute uv coordinates of mouse in face */
static float2 imapaint_pick_uv(const Mesh *mesh_eval,
                               Scene &scene,
                               Object *ob_eval,
                               const int tri_index,
                               const float3 &bary_coord)
{
  const ePaintCanvasSource mode = ePaintCanvasSource(scene.toolsettings->imapaint.mode);

  const Span<int3> tris = mesh_eval->corner_tris();
  const Span<int> tri_faces = mesh_eval->corner_tri_faces();

  const bke::AttributeAccessor attributes = mesh_eval->attributes();
  const VArray<int> material_indices = *attributes.lookup_or_default<int>(
      "material_index", bke::AttrDomain::Face, 0);

  /* face means poly here, not triangle, indeed */
  const int face_i = tri_faces[tri_index];

  VArraySpan<float2> uv_map;

  if (mode == PAINT_CANVAS_SOURCE_MATERIAL) {
    const Material *ma = BKE_object_material_get(ob_eval, material_indices[face_i] + 1);
    const TexPaintSlot *slot = &ma->texpaintslot[ma->paint_active_slot];
    if (slot && slot->uvname) {
      uv_map = *attributes.lookup<float2>(slot->uvname, bke::AttrDomain::Corner);
    }
  }

  if (uv_map.is_empty()) {
    uv_map = *attributes.lookup<float2>(mesh_eval->active_uv_map_name(), bke::AttrDomain::Corner);
  }

  return bke::mesh_surface_sample::sample_corner_attribute_with_bary_coords(
      bary_coord, tris[tri_index], uv_map);
}

/* returns 0 if not found, otherwise 1 */
static bool imapaint_pick_face(ViewContext *vc,
                               const int mval[2],
                               int *r_tri_index,
                               int *r_face_index,
                               float3 *r_bary_coord,
                               const Mesh &mesh)
{
  if (mesh.faces_num == 0) {
    return false;
  }

  float3 start_world;
  float3 end_world;
  ED_view3d_win_to_segment_clipped(
      vc->depsgraph, vc->region, vc->v3d, float2(mval[0], mval[1]), start_world, end_world, true);

  const float4x4 &world_to_object = vc->obact->world_to_object();
  const float3 start_object = math::transform_point(world_to_object, start_world);
  const float3 end_object = math::transform_point(world_to_object, end_world);

  bke::BVHTreeFromMesh mesh_bvh = mesh.bvh_corner_tris();

  BVHTreeRayHit ray_hit;
  ray_hit.dist = FLT_MAX;
  ray_hit.index = -1;
  BLI_bvhtree_ray_cast(mesh_bvh.tree,
                       start_object,
                       math::normalize(end_object - start_object),
                       0.0f,
                       &ray_hit,
                       mesh_bvh.raycast_callback,
                       &mesh_bvh);
  if (ray_hit.index == -1) {
    return false;
  }

  *r_bary_coord = bke::mesh_surface_sample::compute_bary_coord_in_triangle(
      mesh.vert_positions(), mesh.corner_verts(), mesh.corner_tris()[ray_hit.index], ray_hit.co);

  *r_tri_index = ray_hit.index;
  *r_face_index = mesh.corner_tri_faces()[ray_hit.index];
  return true;
}

static std::optional<float3> sample_texture_paint_color(
    const Depsgraph &depsgraph, Scene &scene, ViewContext &vc, Object *object, const int2 mval)
{
  if (!object) {
    return std::nullopt;
  }

  Object *ob_eval = DEG_get_evaluated(&depsgraph, object);

  const Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob_eval);
  const bke::AttributeAccessor attributes = mesh_eval->attributes();
  const VArray<int> material_indices = *attributes.lookup_or_default<int>(
      "material_index", bke::AttrDomain::Face, 0);

  if (mesh_eval->uv_map_names().is_empty()) {
    return std::nullopt;
  }

  int tri_index;
  float3 bary_coord;
  int face_index;
  const VArray<bool> hide_poly = *mesh_eval->attributes().lookup_or_default<bool>(
      ".hide_poly", bke::AttrDomain::Face, false);
  const bool is_hit = imapaint_pick_face(
                          &vc, mval, &tri_index, &face_index, &bary_coord, *mesh_eval) &&
                      !hide_poly[face_index];

  if (!is_hit) {
    return std::nullopt;
  }

  ImagePaintSettings *imapaint = &scene.toolsettings->imapaint;
  bool use_material = imapaint->mode == IMAGEPAINT_MODE_MATERIAL;
  Image *image = nullptr;
  int interp = SHD_INTERP_LINEAR;
  if (use_material) {
    /* Image and texture interpolation from material. */
    Material *ma = BKE_object_material_get(ob_eval, material_indices[face_index] + 1);

    /* Force refresh since paint slots are not updated when changing interpolation. */
    BKE_texpaint_slot_refresh_cache(&scene, ma, object);

    if (ma && ma->texpaintslot) {
      image = ma->texpaintslot[ma->paint_active_slot].ima;
      interp = ma->texpaintslot[ma->paint_active_slot].interp;
    }
  }
  else {
    /* Image and texture interpolation from tool settings. */
    image = imapaint->canvas;
    interp = imapaint->interp;
  }

  if (!image) {
    return std::nullopt;
  }

  /* XXX get appropriate ImageUser instead */
  ImageUser iuser;
  BKE_imageuser_default(&iuser);
  iuser.framenr = image->lastframe;

  float2 uv = imapaint_pick_uv(mesh_eval, scene, ob_eval, tri_index, bary_coord);
  if (image->source == IMA_SRC_TILED) {
    float new_uv[2];
    iuser.tile = BKE_image_get_tile_from_pos(image, uv, new_uv, nullptr);
    uv[0] = new_uv[0];
    uv[1] = new_uv[1];
  }

  ImBuf *ibuf = BKE_image_acquire_ibuf(image, &iuser, nullptr);
  BLI_SCOPED_DEFER([&]() { BKE_image_release_ibuf(image, ibuf, nullptr); });
  if (!ibuf || (!ibuf->byte_buffer.data && !ibuf->float_buffer.data)) {
    return std::nullopt;
  }

  float u = uv[0] * ibuf->x;
  float v = uv[1] * ibuf->y;
  if (interp != SHD_INTERP_CLOSEST) {
    u -= 0.5f;
    v -= 0.5f;
  }

  float4 rgba_f;
  if (ibuf->float_buffer.data) {
    rgba_f = interp == SHD_INTERP_CLOSEST ? imbuf::interpolate_nearest_wrap_fl(ibuf, u, v) :
                                            imbuf::interpolate_bilinear_wrap_fl(ibuf, u, v);
    rgba_f = math::clamp(rgba_f, 0.0f, 1.0f);
    straight_to_premul_v4(rgba_f);
  }
  else {
    uchar4 rgba = interp == SHD_INTERP_CLOSEST ? imbuf::interpolate_nearest_wrap_byte(ibuf, u, v) :
                                                 imbuf::interpolate_bilinear_wrap_byte(ibuf, u, v);
    rgba_uchar_to_float(rgba_f, rgba);

    if ((ibuf->colormanage_flag & IMB_COLORMANAGE_IS_DATA) == 0) {
      IMB_colormanagement_colorspace_to_scene_linear_v3(rgba_f, ibuf->byte_buffer.colorspace);
    }
  }
  return rgba_f.xyz();
}

static std::optional<float3> sample_mesh_attribute_color(ViewContext &vc,
                                                         Object &object,
                                                         int2 mval)
{
  const Mesh &mesh = *id_cast<const Mesh *>(object.data);
  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  const bke::GAttributeReader color_attribute = color::active_color_attribute(mesh);

  std::optional<ActiveElementInfo> active_element = active_element_info_get(vc, float2(mval));

  if (!color_attribute || !active_element || !std::holds_alternative<int>(active_element->vert)) {
    return std::nullopt;
  }

  const GVArraySpan colors = *color_attribute;
  return color::color_vert_get(faces,
                               corner_verts,
                               vert_to_face_map,
                               colors,
                               color_attribute.domain,
                               std::get<int>(active_element->vert))
      .xyz();
}

static void apply_sampled_color(Main &bMain,
                                Paint &paint,
                                const float3 &sampled_color,
                                const bool use_palette)
{
  if (use_palette) {
    Palette *palette = BKE_paint_palette(&paint);
    if (!palette) {
      palette = BKE_palette_add(&bMain, "Palette");
      BKE_paint_palette_set(&paint, palette);
    }

    PaletteColor *color = BKE_palette_color_add(palette);
    palette->active_color = BLI_listbase_count(&palette->colors) - 1;
    BKE_palette_color_set(color, sampled_color);
  }
  else {
    Brush *br = BKE_paint_brush(&paint);
    BKE_brush_color_set(&paint, br, sampled_color);
  }
}

static float3 paint_sample_color(bContext *C,
                                 ARegion *region,
                                 const int2 mval,
                                 const bool use_merged_texture)
{
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Paint *paint = BKE_paint_get_active_from_context(C);
  const PaintMode mode = paint->runtime->paint_mode;

  SpaceImage *sima = CTX_wm_space_image(C);
  const View3D *v3d = CTX_wm_view3d(C);
  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);

  std::optional<float3> sampled_color;
  if (v3d && !use_merged_texture) {
    ViewLayer *view_layer = CTX_data_view_layer(C);
    BKE_view_layer_synced_ensure(scene, view_layer);
    Object *ob = BKE_view_layer_active_object_get(view_layer);

    if (mode == PaintMode::Texture3D) {
      /* Attempt to sample from the mesh & active texture */

      sampled_color = sample_texture_paint_color(*depsgraph, *scene, vc, ob, mval);
    }
    else if (ELEM(mode, PaintMode::Sculpt, PaintMode::Vertex)) {
      BKE_sculpt_update_object_for_edit(depsgraph, ob, false);
      sampled_color = sample_mesh_attribute_color(vc, *ob, mval);
    }
  }
  else if (sima != nullptr) {
    /* Sample from the active image buffer. The sampled color is in
     * Linear Scene Reference Space. */
    float3 rgba_f;
    bool is_data;
    if (ED_space_image_color_sample(sima, region, mval, rgba_f, &is_data)) {
      sampled_color = rgba_f;
    }
  }

  /* No sample found; sample directly from the GPU front buffer. */
  if (!sampled_color) {
    float3 rgb_f;
    WM_window_pixels_read_sample(C,
                                 CTX_wm_window(C),
                                 int2(mval.x + region->winrct.xmin, mval.y + region->winrct.ymin),
                                 rgb_f);

    /* The sampled color is in display colorspace, convert to scene linear. */
    const ColorManagedDisplay *display = IMB_colormanagement_display_get_named(
        scene->display_settings.display_device);
    IMB_colormanagement_display_to_scene_linear_v3(rgb_f, display);
    sampled_color = rgb_f;
  }

  BLI_assert(sampled_color);
  return sampled_color.value_or(float3(0.0f));
}

struct SampleColorData {
  bool show_cursor;
  short launch_event;
  float3 initial_color;
  bool sample_palette;
};

static void sample_color_update_header(SampleColorData *data, bContext *C)
{
  char msg[UI_MAX_DRAW_STR];
  ScrArea *area = CTX_wm_area(C);

  if (area) {
    SNPRINTF_UTF8(msg,
                  IFACE_("Sample color for %s"),
                  !data->sample_palette ?
                      IFACE_("Brush. Use Left Click to sample for palette instead") :
                      IFACE_("Palette. Use Left Click to sample more colors"));
    ED_workspace_status_text(C, msg);
  }
}

static wmOperatorStatus sample_color_exec(bContext *C, wmOperator *op)
{
  Scene &scene = *CTX_data_scene(C);
  Object &object = *CTX_data_active_object(C);
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);
  ARegion *region = CTX_wm_region(C);
  wmWindow *win = CTX_wm_window(C);

  const bool use_merged_texture = RNA_boolean_get(op->ptr, "merged");
  const PaintMode mode = paint->runtime->paint_mode;
  if (ELEM(mode, PaintMode::Vertex, PaintMode::Sculpt) && !use_merged_texture) {
    if (!color_supported_check(scene, object, op->reports)) {
      return OPERATOR_CANCELLED;
    }
  }

  const bool show_cursor = ((paint->flags & PAINT_SHOW_BRUSH) != 0);
  paint->flags &= ~PAINT_SHOW_BRUSH;

  /* force redraw without cursor */
  WM_paint_cursor_tag_redraw(win, region);
  WM_redraw_windows(C);

  int2 location;
  RNA_int_get_array(op->ptr, "location", location);
  location.x = std::clamp(location.x, 0, int(region->winx));
  location.y = std::clamp(location.y, 0, int(region->winy));

  const bool use_palette = RNA_boolean_get(op->ptr, "palette");

  const float3 sampled_color = paint_sample_color(C, region, location, use_merged_texture);
  apply_sampled_color(*CTX_data_main(C), *paint, sampled_color, use_palette);

  if (show_cursor) {
    paint->flags |= PAINT_SHOW_BRUSH;
  }

  WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, brush);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus sample_color_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Scene &scene = *CTX_data_scene(C);
  Object &object = *CTX_data_active_object(C);
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);
  ARegion *region = CTX_wm_region(C);
  wmWindow *win = CTX_wm_window(C);

  const bool use_merged_texture = RNA_boolean_get(op->ptr, "merged");
  const PaintMode mode = paint->runtime->paint_mode;
  if (ELEM(mode, PaintMode::Vertex, PaintMode::Sculpt) && !use_merged_texture) {
    if (!color_supported_check(scene, object, op->reports)) {
      return OPERATOR_CANCELLED;
    }
  }

  SampleColorData *data = MEM_new<SampleColorData>(__func__);

  data->launch_event = WM_userdef_event_type_from_keymap_type(event->type);
  data->show_cursor = ((paint->flags & PAINT_SHOW_BRUSH) != 0);
  data->initial_color = BKE_brush_color_get(paint, brush);
  data->sample_palette = false;
  op->customdata = data;
  paint->flags &= ~PAINT_SHOW_BRUSH;

  sample_color_update_header(data, C);

  WM_event_add_modal_handler(C, op);

  /* force redraw without cursor */
  WM_paint_cursor_tag_redraw(win, region);
  WM_redraw_windows(C);

  RNA_int_set_array(op->ptr, "location", event->mval);

  int2 mval(std::clamp(event->mval[0], 0, (int)region->winx),
            std::clamp(event->mval[1], 0, (int)region->winy));
  const float3 sampled_color = paint_sample_color(C, region, mval, use_merged_texture);
  /* On initial invoke, we never sample to the palette. */
  apply_sampled_color(*CTX_data_main(C), *paint, sampled_color, false);

  WM_cursor_modal_set(win, WM_CURSOR_EYEDROPPER);

  WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, brush);

  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus sample_color_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  SampleColorData *data = static_cast<SampleColorData *>(op->customdata);
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);

  if ((event->type == data->launch_event) && (event->val == KM_RELEASE)) {
    if (data->show_cursor) {
      paint->flags |= PAINT_SHOW_BRUSH;
    }

    if (data->sample_palette) {
      /* If we sampled any colors to the palette, reset the brush to its initial color */
      BKE_brush_color_set(paint, brush, data->initial_color);
      RNA_boolean_set(op->ptr, "palette", true);
      WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, brush);
    }
    WM_cursor_modal_restore(CTX_wm_window(C));
    MEM_delete(data);
    ED_workspace_status_text(C, nullptr);

    return OPERATOR_FINISHED;
  }
  ARegion *region = CTX_wm_region(C);
  int2 mval(std::clamp(event->mval[0], 0, int(region->winx)),
            std::clamp(event->mval[1], 0, int(region->winy)));

  const bool use_merged_texture = RNA_boolean_get(op->ptr, "merged");

  switch (event->type) {
    case MOUSEMOVE: {
      RNA_int_set_array(op->ptr, "location", event->mval);
      const float3 sampled_color = paint_sample_color(C, region, mval, use_merged_texture);
      apply_sampled_color(*CTX_data_main(C), *paint, sampled_color, false);
      WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, brush);
      break;
    }

    case LEFTMOUSE:
      if (event->val == KM_PRESS) {
        RNA_int_set_array(op->ptr, "location", event->mval);
        const float3 sampled_color = paint_sample_color(C, region, mval, use_merged_texture);
        apply_sampled_color(*CTX_data_main(C), *paint, sampled_color, true);
        if (!data->sample_palette) {
          data->sample_palette = true;
          sample_color_update_header(data, C);
          BKE_report(op->reports, RPT_INFO, "Sampling color for palette");
        }
        WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, brush);
      }
      break;
    default: {
      break;
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

static bool sample_color_poll(bContext *C)
{
  return (image_paint_poll_ignore_tool(C) || vertex_paint_poll_ignore_tool(C) ||
          SCULPT_mode_poll(C) || ed::greasepencil::grease_pencil_painting_poll(C) ||
          ed::greasepencil::grease_pencil_vertex_painting_poll(C));
}

void PAINT_OT_sample_color(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Sample Color";
  ot->idname = "PAINT_OT_sample_color";
  ot->description = "Use the mouse to sample a color in the image";

  /* API callbacks. */
  ot->exec = sample_color_exec;
  ot->invoke = sample_color_invoke;
  ot->modal = sample_color_modal;
  ot->poll = sample_color_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_DEPENDS_ON_CURSOR;

  /* properties */
  PropertyRNA *prop;

  prop = RNA_def_int_vector(
      ot->srna, "location", 2, nullptr, 0, INT_MAX, "Location", "", 0, 16384);
  RNA_def_property_flag(prop, (PROP_SKIP_SAVE | PROP_HIDDEN));

  RNA_def_boolean(ot->srna, "merged", false, "Sample Merged", "Sample the output display color");
  RNA_def_boolean(ot->srna, "palette", false, "Add to Palette", "");
}

}  // namespace blender::ed::sculpt_paint

/** \} */
