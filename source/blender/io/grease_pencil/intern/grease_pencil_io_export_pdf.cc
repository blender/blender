/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_bounds.hh"

#include "BKE_grease_pencil.hh"
#include "BKE_scene.hh"

#include "BLI_math_color.h"

#include "DEG_depsgraph_query.hh"

#include "DNA_grease_pencil_types.h"
#include "DNA_scene_types.h"

#include "grease_pencil_io.hh"
#include "grease_pencil_io_intern.hh"

#include "hpdf.h"

#include <iostream>

/** \file
 * \ingroup bgrease_pencil
 */

namespace blender::io::grease_pencil {

class PDFExporter : public GreasePencilExporter {
 public:
  using GreasePencilExporter::GreasePencilExporter;

  HPDF_Doc pdf_;
  HPDF_Page page_;

  bool export_scene(Scene &scene, StringRefNull filepath);
  void export_grease_pencil_objects(int frame_number);
  void export_grease_pencil_layer(const Object &object,
                                  const bke::greasepencil::Layer &layer,
                                  const bke::greasepencil::Drawing &drawing);

  bool create_document();
  bool add_page();

  void write_stroke_to_polyline(const float4x4 &transform,
                                const Span<float3> positions,
                                const bool cyclic,
                                const ColorGeometry4f &color,
                                const float opacity,
                                std::optional<float> width);
  bool write_to_file(StringRefNull filepath);
};

bool PDFExporter::export_scene(Scene &scene, StringRefNull filepath)
{
  bool result = false;
  Object &ob_eval = *DEG_get_evaluated(context_.depsgraph, params_.object);

  if (!create_document()) {
    return false;
  }

  switch (params_.frame_mode) {
    case ExportParams::FrameMode::Active: {
      const int frame_number = scene.r.cfra;

      this->prepare_render_params(scene, frame_number);
      this->add_page();
      this->export_grease_pencil_objects(frame_number);
      result = this->write_to_file(filepath);
      break;
    }
    case ExportParams::FrameMode::Selected: {
      case ExportParams::FrameMode::Scene:
        const bool only_selected = (params_.frame_mode == ExportParams::FrameMode::Selected);
        if (only_selected && ob_eval.type != OB_GREASE_PENCIL) {
          /* For exporting "Selected Frames", the active object is required to be a grease pencil
           * object, from which we will read selected frames from. */
          break;
        }
        const int orig_frame = scene.r.cfra;
        for (int frame_number = scene.r.sfra; frame_number <= scene.r.efra; frame_number++) {
          GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob_eval.data);
          if (only_selected && !this->is_selected_frame(grease_pencil, frame_number)) {
            continue;
          }

          scene.r.cfra = frame_number;
          BKE_scene_graph_update_for_newframe(context_.depsgraph);

          this->prepare_render_params(scene, frame_number);
          this->add_page();
          this->export_grease_pencil_objects(frame_number);
        }

        result = this->write_to_file(filepath);

        /* Back to original frame. */
        scene.r.cfra = orig_frame;
        BKE_scene_camera_switch_update(&scene);
        BKE_scene_graph_update_for_newframe(context_.depsgraph);
        break;
    }
    default:
      break;
  }

  return result;
}

void PDFExporter::export_grease_pencil_objects(const int frame_number)
{
  using bke::greasepencil::Drawing;

  Vector<ObjectInfo> objects = retrieve_objects();

  for (const ObjectInfo &info : objects) {
    const Object *ob = info.object;

    /* Use evaluated version to get strokes with modifiers. */
    const Object *ob_eval = DEG_get_evaluated(context_.depsgraph, ob);
    BLI_assert(ob_eval->type == OB_GREASE_PENCIL);
    const GreasePencil *grease_pencil_eval = static_cast<const GreasePencil *>(ob_eval->data);

    for (const bke::greasepencil::Layer *layer : grease_pencil_eval->layers()) {
      if (!layer->is_visible()) {
        continue;
      }
      const Drawing *drawing = grease_pencil_eval->get_drawing_at(*layer, frame_number);
      if (drawing == nullptr) {
        continue;
      }

      export_grease_pencil_layer(*ob_eval, *layer, *drawing);
    }
  }
}

void PDFExporter::export_grease_pencil_layer(const Object &object,
                                             const bke::greasepencil::Layer &layer,
                                             const bke::greasepencil::Drawing &drawing)
{
  using bke::greasepencil::Drawing;

  const float4x4 layer_to_world = layer.to_world_space(object);

  auto write_stroke = [&](const Span<float3> positions,
                          const Span<float3> /*positions_left*/,
                          const Span<float3> /*positions_right*/,
                          const bool cyclic,
                          const int8_t /*type*/,
                          const ColorGeometry4f &color,
                          const float opacity,
                          const std::optional<float> width,
                          const bool /*round_cap*/,
                          const bool /*is_outline*/) {
    write_stroke_to_polyline(layer_to_world, positions, cyclic, color, opacity, width);
  };

  foreach_stroke_in_layer(object, layer, drawing, write_stroke);
}

bool PDFExporter::create_document()
{
  auto hpdf_error_handler = [](HPDF_STATUS error_no, HPDF_STATUS detail_no, void * /*user_data*/) {
    printf("ERROR: error_no=%04X, detail_no=%u\n", (HPDF_UINT)error_no, (HPDF_UINT)detail_no);
  };

  pdf_ = HPDF_New(hpdf_error_handler, nullptr);
  if (!pdf_) {
    std::cout << "error: cannot create PdfDoc object\n";
    return false;
  }
  return true;
}

bool PDFExporter::add_page()
{
  page_ = HPDF_AddPage(pdf_);
  if (!pdf_) {
    std::cout << "error: cannot create PdfPage\n";
    return false;
  }

  if (camera_persmat_) {
    HPDF_Page_SetWidth(page_, camera_rect_.size().x);
    HPDF_Page_SetHeight(page_, camera_rect_.size().y);
  }
  else {
    HPDF_Page_SetWidth(page_, screen_rect_.size().x);
    HPDF_Page_SetHeight(page_, screen_rect_.size().y);
  }

  return true;
}

void PDFExporter::write_stroke_to_polyline(const float4x4 &transform,
                                           const Span<float3> positions,
                                           const bool cyclic,
                                           const ColorGeometry4f &color,
                                           const float opacity,
                                           const std::optional<float> width)
{
  if (width) {
    HPDF_Page_SetLineJoin(page_, HPDF_ROUND_JOIN);
    HPDF_Page_SetLineWidth(page_, std::max(*width, 1.0f));
  }

  const float total_opacity = color.a * opacity;

  HPDF_Page_GSave(page_);
  HPDF_ExtGState gstate = (total_opacity < 1.0f) ? HPDF_CreateExtGState(pdf_) : nullptr;

  ColorGeometry4f srgb;
  linearrgb_to_srgb_v3_v3(srgb, color);
  if (width) {
    HPDF_Page_SetRGBFill(page_, srgb.r, srgb.g, srgb.b);
    HPDF_Page_SetRGBStroke(page_, srgb.r, srgb.g, srgb.b);
    if (gstate) {
      HPDF_ExtGState_SetAlphaFill(gstate, std::clamp(total_opacity, 0.0f, 1.0f));
      HPDF_ExtGState_SetAlphaStroke(gstate, std::clamp(total_opacity, 0.0f, 1.0f));
    }
  }
  else {
    HPDF_Page_SetRGBFill(page_, srgb.r, srgb.g, srgb.b);
    if (gstate) {
      HPDF_ExtGState_SetAlphaFill(gstate, std::clamp(total_opacity, 0.0f, 1.0f));
    }
  }
  if (gstate) {
    HPDF_Page_SetExtGState(page_, gstate);
  }

  for (const int i : positions.index_range()) {
    const float2 screen_co = this->project_to_screen(transform, positions[i]);
    if (i == 0) {
      HPDF_Page_MoveTo(page_, screen_co.x, screen_co.y);
    }
    else {
      HPDF_Page_LineTo(page_, screen_co.x, screen_co.y);
    }
  }
  if (cyclic) {
    HPDF_Page_ClosePath(page_);
  }

  if (width) {
    HPDF_Page_Stroke(page_);
  }
  else {
    HPDF_Page_Fill(page_);
  }

  HPDF_Page_GRestore(page_);
}

bool PDFExporter::write_to_file(StringRefNull filepath)
{
  /* Support unicode character paths on Windows. */
  HPDF_STATUS result = 0;

  /* TODO: It looks `libharu` does not support unicode. */
#if 0 /* `ifdef WIN32` */
  wchar_t *filepath_16 = alloc_utf16_from_8(filepath.c_str(), 0);
  std::wstring wstr(filepath_16);
  result = HPDF_SaveToFile(pdf_, wstr.c_str());
  free(filepath_16);
#else
  result = HPDF_SaveToFile(pdf_, filepath.c_str());
#endif

  return (result == 0) ? true : false;
}

bool export_pdf(const IOContext &context,
                const ExportParams &params,
                Scene &scene,
                StringRefNull filepath)
{
  PDFExporter exporter(context, params);
  return exporter.export_scene(scene, filepath);
}

}  // namespace blender::io::grease_pencil
