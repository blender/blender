/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_bounds.hh"
#include "BLI_color.hh"
#include "BLI_string_utf8.h"
#include "BLI_vector.hh"

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_scene.hh"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "DEG_depsgraph_query.hh"

#include "GEO_resample_curves.hh"
#include "GEO_set_curve_type.hh"

#include "grease_pencil_io_intern.hh"

#include <fmt/core.h>
#include <fmt/format.h>
#include <optional>
#include <pugixml.hpp>

#ifdef WIN32
#  include "utfconv.hh"
#endif

/** \file
 * \ingroup bgrease_pencil
 */

namespace blender::io::grease_pencil {

constexpr const char *svg_exporter_name = "SVG Export for Grease Pencil";
constexpr const char *svg_exporter_version = "v2.1";

static std::string rgb_to_hexstr(const float color[3])
{
  uint8_t r = color[0] * 255.0f;
  uint8_t g = color[1] * 255.0f;
  uint8_t b = color[2] * 255.0f;
  return fmt::format("#{:02X}{:02X}{:02X}", r, g, b);
}

static void write_stroke_color_attribute(pugi::xml_node node,
                                         const ColorGeometry4f &stroke_color,
                                         const float stroke_opacity,
                                         const bool round_cap)
{
  ColorGeometry4f color;
  linearrgb_to_srgb_v3_v3(color, stroke_color);
  std::string stroke_hex = rgb_to_hexstr(color);

  node.append_attribute("stroke").set_value(stroke_hex.c_str());
  node.append_attribute("stroke-opacity").set_value(stroke_color.a * stroke_opacity);

  node.append_attribute("fill").set_value("none");
  node.append_attribute("stroke-linecap").set_value(round_cap ? "round" : "square");
}

static void write_fill_color_attribute(pugi::xml_node node,
                                       const ColorGeometry4f &fill_color,
                                       const float layer_opacity)
{
  ColorGeometry4f color;
  linearrgb_to_srgb_v3_v3(color, fill_color);
  std::string stroke_hex = rgb_to_hexstr(color);

  node.append_attribute("fill").set_value(stroke_hex.c_str());
  node.append_attribute("stroke").set_value("none");
  node.append_attribute("fill-opacity").set_value(fill_color.a * layer_opacity);
}

static void write_rect(pugi::xml_node node,
                       const float x,
                       const float y,
                       const float width,
                       const float height,
                       const float thickness,
                       const std::string &hexcolor)
{
  pugi::xml_node rect_node = node.append_child("rect");
  rect_node.append_attribute("x").set_value(x);
  rect_node.append_attribute("y").set_value(y);
  rect_node.append_attribute("width").set_value(width);
  rect_node.append_attribute("height").set_value(height);
  rect_node.append_attribute("fill").set_value("none");
  if (thickness > 0.0f) {
    rect_node.append_attribute("stroke").set_value(hexcolor.c_str());
    rect_node.append_attribute("stroke-width").set_value(thickness);
  }
}

class SVGExporter : public GreasePencilExporter {
  uint64_t _node_uuid = 0;

  std::string get_node_uuid_string();

 public:
  using GreasePencilExporter::GreasePencilExporter;

  pugi::xml_document main_doc_;

  ExportStatus export_scene(Scene &scene, StringRefNull filepath);
  void export_grease_pencil_objects(pugi::xml_node node, int frame_number);
  void export_grease_pencil_layer(pugi::xml_node node,
                                  const Object &object,
                                  const bke::greasepencil::Layer &layer,
                                  const bke::greasepencil::Drawing &drawing);

  void write_document_header();
  pugi::xml_node write_main_node();
  pugi::xml_node write_animation_node(pugi::xml_node parent_node,
                                      IndexMask frames,
                                      float duration);
  pugi::xml_node write_path(pugi::xml_node node,
                            const float4x4 &transform,
                            const Span<float3> positions,
                            const Span<float3> positions_left,
                            const Span<float3> positions_right,
                            const OffsetIndices<int> points_by_curve,
                            const Span<int> shape,
                            const VArray<bool> &cyclic,
                            const VArray<int8_t> &types,
                            std::optional<float> width);

  bool write_to_file(StringRefNull filepath);
};

std::string SVGExporter::get_node_uuid_string()
{
  std::string id = fmt::format(".uuid_{:#x}", this->_node_uuid++);
  return id;
}

ExportStatus SVGExporter::export_scene(Scene &scene, StringRefNull filepath)
{
  this->_node_uuid = 0;

  switch (params_.frame_mode) {
    case ExportParams::FrameMode::Active: {
      const int frame_number = scene.r.cfra;
      this->prepare_render_params(scene, frame_number);

      this->write_document_header();
      pugi::xml_node main_node = this->write_main_node();

      this->export_grease_pencil_objects(main_node, frame_number);

      const bool write_success = this->write_to_file(filepath);
      return write_success ? ExportStatus::Ok : ExportStatus::FileWriteError;
    }
    case ExportParams::FrameMode::Selected:
    case ExportParams::FrameMode::Scene: {
      const bool selection_only = params_.frame_mode == ExportParams::FrameMode::Selected;
      const int orig_frame = scene.r.cfra;

      IndexMask frames = IndexMask(IndexRange(scene.r.sfra, scene.r.efra - scene.r.sfra + 1));

      IndexMaskMemory memory;
      if (selection_only) {
        const Object &ob_eval = *DEG_get_evaluated(context_.depsgraph, params_.object);
        if (ob_eval.type != OB_GREASE_PENCIL) {
          return ExportStatus::InvalidActiveObjectType;
        }
        const GreasePencil &grease_pencil = *id_cast<GreasePencil *>(ob_eval.data);
        frames = IndexMask::from_predicate(
            frames, GrainSize(1024), memory, [&](const int frame_number) {
              return this->is_selected_frame(grease_pencil, frame_number);
            });
      }

      if (frames.is_empty()) {
        return ExportStatus::NoFramesSelected;
      }

      this->prepare_render_params(scene, frames.first());

      this->write_document_header();
      pugi::xml_node main_node = this->write_main_node();

      /* Put frames in a hidden group. They are referenced later by a `<use>-node` that displays
       * them in order. Use a group rather than a `<defs>-node` because some graphics applications
       * don't expose those to users making it hard for them to work with the file.
       */
      pugi::xml_node frames_group_node = main_node.append_child("g");
      frames_group_node.append_attribute("id").set_value("blender_frames");
      frames_group_node.append_attribute("display").set_value("none");

      const int frame_count = frames.size();
      const float duration = scene.r.frs_sec_base * frame_count / scene.r.frs_sec;

      frames.foreach_index([&](const int frame_number) {
        scene.r.cfra = frame_number;
        BKE_scene_graph_update_for_newframe(context_.depsgraph);
        this->prepare_render_params(scene, frame_number);
        this->export_grease_pencil_objects(frames_group_node, frame_number);
      });

      /* Back to original frame. */
      scene.r.cfra = orig_frame;
      BKE_scene_camera_switch_update(&scene);
      BKE_scene_graph_update_for_newframe(context_.depsgraph);

      this->write_animation_node(main_node, frames, duration);

      const bool write_success = this->write_to_file(filepath);
      return write_success ? ExportStatus::Ok : ExportStatus::FileWriteError;
    }
    default:
      BLI_assert_unreachable();
      return ExportStatus::UnknownError;
  }
}

static std::string frame_name(int frame_number)
{
  std::string frametxt = "blender_frame." + std::to_string(frame_number);
  return frametxt;
}

void SVGExporter::export_grease_pencil_objects(pugi::xml_node node, const int frame_number)
{
  using bke::greasepencil::Drawing;

  const bool is_clipping = camera_persmat_ && params_.use_clip_camera;

  Vector<ObjectInfo> objects = retrieve_objects();

  /* Camera clipping. */
  if (is_clipping) {
    pugi::xml_node clip_node = node.append_child("clipPath");
    clip_node.append_attribute("id").set_value(
        ("clip-path." + std::to_string(frame_number)).c_str());

    write_rect(clip_node, 0, 0, camera_rect_.size().x, camera_rect_.size().y, 0.0f, "#000000");
  }

  pugi::xml_node frame_node = node.append_child("g");
  frame_node.append_attribute("id").set_value(frame_name(frame_number).c_str());

  /* Clip area. */
  if (is_clipping) {
    frame_node.append_attribute("clip-path")
        .set_value(("url(#clip-path." + std::to_string(frame_number) + ")").c_str());
  }

  for (const ObjectInfo &info : objects) {
    const Object *ob = info.object;

    pugi::xml_node ob_node = frame_node.append_child("g");

    char obtxt[15 + (MAX_ID_NAME - 2) + 1 + 11 + 1]; /* Final +1 for the null terminator. */
    SNPRINTF_UTF8(obtxt, "blender_object.%s.%d", ob->id.name + 2, frame_number);
    std::string object_id = std::string(obtxt) + this->get_node_uuid_string();
    ob_node.append_attribute("id").set_value(object_id.c_str());

    /* Use evaluated version to get strokes with modifiers. */
    const Object *ob_eval = DEG_get_evaluated(context_.depsgraph, ob);
    BLI_assert(ob_eval->type == OB_GREASE_PENCIL);
    const GreasePencil *grease_pencil_eval = id_cast<const GreasePencil *>(ob_eval->data);

    for (const bke::greasepencil::Layer *layer : grease_pencil_eval->layers()) {
      if (!layer->is_visible()) {
        continue;
      }
      const Drawing *drawing = grease_pencil_eval->get_drawing_at(*layer, frame_number);
      if (drawing == nullptr) {
        continue;
      }

      /* Layer node. */
      pugi::xml_node layer_node = ob_node.append_child("g");
      std::string layer_node_id = "layer." + layer->name() + this->get_node_uuid_string();
      layer_node.append_attribute("id").set_value(layer_node_id.c_str());

      const bke::CurvesGeometry &curves = drawing->strokes();
      /* Convert NURBS and Catmull Rom to bezier then export. */
      if (curves.has_curve_with_type({CURVE_TYPE_CATMULL_ROM, CURVE_TYPE_NURBS})) {
        IndexMaskMemory memory;
        const IndexMask non_poly_selection = curves.indices_for_curve_type(CURVE_TYPE_POLY, memory)
                                                 .complement(curves.curves_range(), memory);

        geometry::ConvertCurvesOptions options;
        options.convert_bezier_handles_to_poly_points = false;
        options.convert_bezier_handles_to_catmull_rom_points = false;
        options.keep_bezier_shape_as_nurbs = true;
        options.keep_catmull_rom_shape_as_nurbs = true;

        Drawing export_drawing;
        export_drawing.strokes_for_write() = geometry::convert_curves(
            curves, non_poly_selection, CURVE_TYPE_BEZIER, {}, options);
        export_drawing.tag_topology_changed();
        export_grease_pencil_layer(layer_node, *ob_eval, *layer, export_drawing);
      }
      else {
        export_grease_pencil_layer(layer_node, *ob_eval, *layer, *drawing);
      }
    }
  }
}

void SVGExporter::export_grease_pencil_layer(pugi::xml_node layer_node,
                                             const Object &object,
                                             const bke::greasepencil::Layer &layer,
                                             const bke::greasepencil::Drawing &drawing)
{
  using bke::greasepencil::Drawing;

  const float4x4 layer_to_world = layer.to_world_space(object);

  auto write_shape = [&](const Span<float3> positions,
                         const Span<float3> positions_left,
                         const Span<float3> positions_right,
                         const OffsetIndices<int> points_by_curve,
                         const Span<int> shape,
                         const VArray<bool> &cyclic,
                         const VArray<int8_t> &types,
                         const ColorGeometry4f &color,
                         const float opacity,
                         const std::optional<float> width,
                         const bool round_cap,
                         const bool is_outline) {
    pugi::xml_node element_node = write_path(layer_node,
                                             layer_to_world,
                                             positions,
                                             positions_left,
                                             positions_right,
                                             points_by_curve,
                                             shape,
                                             cyclic,
                                             types,
                                             width);

    if (is_outline) {
      write_fill_color_attribute(element_node, color, opacity);
      /* Outlines might self-overlap which creates visual holes with the `even-odd` fill rule. */
      element_node.append_attribute("fill-rule").set_value("nonzero");
    }
    else {
      element_node.append_attribute("fill-rule").set_value("evenodd");
      if (width) {
        write_stroke_color_attribute(element_node, color, opacity, round_cap);
      }
      else {
        write_fill_color_attribute(element_node, color, opacity);
      }
    }
  };

  foreach_shape_in_layer(object, layer, drawing, write_shape);
}

void SVGExporter::write_document_header()
{
  /* Add a custom document declaration node. */
  pugi::xml_node decl = main_doc_.prepend_child(pugi::node_declaration);
  decl.append_attribute("version") = "1.0";
  decl.append_attribute("encoding") = "UTF-8";

  pugi::xml_node comment = main_doc_.append_child(pugi::node_comment);
  std::string txt = std::string(" Generator: Blender, ") + svg_exporter_name + " - " +
                    svg_exporter_version + " ";
  comment.set_value(txt.c_str());

  pugi::xml_node doctype = main_doc_.append_child(pugi::node_doctype);
  doctype.set_value(
      "svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" "
      "\"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\"");
}

pugi::xml_node SVGExporter::write_main_node()
{
  pugi::xml_node main_node = main_doc_.append_child("svg");
  main_node.append_attribute("version").set_value("1.1");
  main_node.append_attribute("x").set_value("0px");
  main_node.append_attribute("y").set_value("0px");
  main_node.append_attribute("xmlns").set_value("http://www.w3.org/2000/svg");

  std::string width, height;

  if (camera_persmat_) {
    width = std::to_string(camera_rect_.size().x);
    height = std::to_string(camera_rect_.size().y);
  }
  else {
    width = std::to_string(screen_rect_.size().x);
    height = std::to_string(screen_rect_.size().y);
  }

  main_node.append_attribute("width").set_value((width + "px").c_str());
  main_node.append_attribute("height").set_value((height + "px").c_str());
  std::string viewbox = "0 0 " + width + " " + height;
  main_node.append_attribute("viewBox").set_value(viewbox.c_str());

  return main_node;
}

pugi::xml_node SVGExporter::write_animation_node(pugi::xml_node parent_node,
                                                 IndexMask frames,
                                                 const float duration)
{
  pugi::xml_node use_node = parent_node.append_child("use");
  use_node.append_attribute("id").set_value("blender_animation");
  std::string href_text = "#" + frame_name(frames.first());
  use_node.append_attribute("href").set_value(href_text.c_str());

  pugi::xml_node animate_node = use_node.append_child("animate");
  animate_node.append_attribute("id").set_value("frame-by-frame_animation");
  animate_node.append_attribute("attributeName").set_value("href");

  std::string duration_text = std::to_string(duration) + "s";
  animate_node.append_attribute("dur").set_value(duration_text.c_str());
  animate_node.append_attribute("repeatCount").set_value("indefinite");

  std::string animated_frame_ids = [&]() {
    std::string frame_ids_text = "";
    frames.foreach_index([&](const int frame) {
      std::string frame_url_entry = "#" + frame_name(frame) + ";";
      frame_ids_text.append(frame_url_entry);
    });
    return frame_ids_text;
  }();

  animate_node.append_attribute("values").set_value(animated_frame_ids.c_str());

  return use_node;
}

pugi::xml_node SVGExporter::write_path(pugi::xml_node node,
                                       const float4x4 &transform,
                                       const Span<float3> positions,
                                       const Span<float3> positions_left,
                                       const Span<float3> positions_right,
                                       const OffsetIndices<int> points_by_curve,
                                       const Span<int> shape,
                                       const VArray<bool> &cyclic,
                                       const VArray<int8_t> &types,
                                       const std::optional<float> width)
{
  pugi::xml_node element_node = node.append_child("path");

  if (width) {
    element_node.append_attribute("stroke-width").set_value(*width);
  }

  std::string txt;
  for (const int curve_i : shape) {
    txt.append("M");
    const IndexRange points = points_by_curve[curve_i];

    const Span<float3> curve_pos = positions.slice(points);

    if (types[curve_i] != CURVE_TYPE_BEZIER) {
      for (const int i : curve_pos.index_range()) {
        const float2 screen_co = this->project_to_screen(transform, curve_pos[i]);

        if (i > 0) {
          txt.append("L");
        }

        txt.append(coord_to_svg_string(screen_co));
      }
      /* Close path (cyclic). */
      if (cyclic) {
        txt.append("z");
      }
    }
    else {
      const Span<float3> curve_pos_right = positions_right.slice(points);
      const Span<float3> curve_pos_left = positions_left.slice(points);

      for (const int i : curve_pos.index_range().drop_back(1)) {
        const float2 screen_co = this->project_to_screen(transform, curve_pos[i]);
        const float2 screen_co_right = this->project_to_screen(transform, curve_pos_right[i]);
        const float2 screen_co_left = this->project_to_screen(transform, curve_pos_left[i + 1]);

        txt.append(coord_to_svg_string(screen_co));
        txt.append(" C ");
        txt.append(coord_to_svg_string(screen_co_right));
        txt.append(", ");
        txt.append(coord_to_svg_string(screen_co_left));

        if (i != curve_pos.size() - 2) {
          txt.append(", ");
        }
      }

      {
        txt.append(", ");
        const float2 screen_co = this->project_to_screen(transform, curve_pos.last());
        txt.append(coord_to_svg_string(screen_co));
      }

      /* Close path (cyclic). */
      if (cyclic) {
        const float2 screen_co_right = this->project_to_screen(transform, curve_pos_right.last());
        const float2 screen_co_left = this->project_to_screen(transform, curve_pos_left.first());
        const float2 screen_co = this->project_to_screen(transform, curve_pos.first());

        txt.append(" C ");
        txt.append(coord_to_svg_string(screen_co_right));
        txt.append(", ");
        txt.append(coord_to_svg_string(screen_co_left));
        txt.append(", ");
        txt.append(coord_to_svg_string(screen_co));
        txt.append("z");
      }
    }
  }

  element_node.append_attribute("d").set_value(txt.c_str());

  return element_node;
}

bool SVGExporter::write_to_file(StringRefNull filepath)
{
  bool result = true;
  /* Support unicode character paths on Windows. */
#ifdef WIN32
  wchar_t *filepath_16 = alloc_utf16_from_8(filepath.c_str(), 0);
  std::wstring wstr(filepath_16);
  result = main_doc_.save_file(wstr.c_str());
  free(filepath_16);
#else
  result = main_doc_.save_file(filepath.c_str());
#endif

  return result;
}

ExportStatus export_svg(const IOContext &context,
                        const ExportParams &params,
                        Scene &scene,
                        StringRefNull filepath)
{
  SVGExporter exporter(context, params);
  return exporter.export_scene(scene, filepath);
}

}  // namespace blender::io::grease_pencil
