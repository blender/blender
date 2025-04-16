/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_bounds.hh"
#include "BLI_color.hh"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "BKE_grease_pencil.hh"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "DEG_depsgraph_query.hh"

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
constexpr const char *svg_exporter_version = "v2.0";

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
 public:
  using GreasePencilExporter::GreasePencilExporter;

  pugi::xml_document main_doc_;

  bool export_scene(Scene &scene, StringRefNull filepath);
  void export_grease_pencil_objects(pugi::xml_node node, int frame_number);
  void export_grease_pencil_layer(pugi::xml_node node,
                                  const Object &object,
                                  const bke::greasepencil::Layer &layer,
                                  const bke::greasepencil::Drawing &drawing);

  void write_document_header();
  pugi::xml_node write_main_node();
  pugi::xml_node write_polygon(pugi::xml_node node,
                               const float4x4 &transform,
                               Span<float3> positions);
  pugi::xml_node write_polyline(pugi::xml_node node,
                                const float4x4 &transform,
                                Span<float3> positions,
                                bool cyclic,
                                std::optional<float> width);
  pugi::xml_node write_path(pugi::xml_node node,
                            const float4x4 &transform,
                            Span<float3> positions,
                            bool cyclic);

  bool write_to_file(StringRefNull filepath);
};

bool SVGExporter::export_scene(Scene &scene, StringRefNull filepath)
{
  const int frame_number = scene.r.cfra;

  this->prepare_render_params(scene, frame_number);

  this->write_document_header();
  pugi::xml_node main_node = this->write_main_node();
  this->export_grease_pencil_objects(main_node, frame_number);

  return this->write_to_file(filepath);
}

void SVGExporter::export_grease_pencil_objects(pugi::xml_node node, const int frame_number)
{
  using bke::greasepencil::Drawing;

  const bool is_clipping = camera_persmat_ && params_.use_clip_camera;

  Vector<ObjectInfo> objects = retrieve_objects();

  for (const ObjectInfo &info : objects) {
    const Object *ob = info.object;

    /* Camera clipping. */
    if (is_clipping) {
      pugi::xml_node clip_node = node.append_child("clipPath");
      clip_node.append_attribute("id").set_value(
          ("clip-path" + std::to_string(frame_number)).c_str());

      write_rect(clip_node, 0, 0, camera_rect_.size().x, camera_rect_.size().y, 0.0f, "#000000");
    }

    pugi::xml_node frame_node = node.append_child("g");
    std::string frametxt = "blender_frame_" + std::to_string(frame_number);
    frame_node.append_attribute("id").set_value(frametxt.c_str());

    /* Clip area. */
    if (is_clipping) {
      frame_node.append_attribute("clip-path")
          .set_value(("url(#clip-path" + std::to_string(frame_number) + ")").c_str());
    }

    pugi::xml_node ob_node = frame_node.append_child("g");

    char obtxt[96];
    SNPRINTF(obtxt, "blender_object_%s", ob->id.name + 2);
    ob_node.append_attribute("id").set_value(obtxt);

    /* Use evaluated version to get strokes with modifiers. */
    Object *ob_eval = DEG_get_evaluated_object(context_.depsgraph, const_cast<Object *>(ob));
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

      /* Layer node. */
      const std::string txt = "Layer: " + layer->name();
      ob_node.append_child(pugi::node_comment).set_value(txt.c_str());

      pugi::xml_node layer_node = ob_node.append_child("g");
      layer_node.append_attribute("id").set_value(layer->name().c_str());

      export_grease_pencil_layer(layer_node, *ob_eval, *layer, *drawing);
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

  auto write_stroke = [&](const Span<float3> positions,
                          const bool cyclic,
                          const ColorGeometry4f &color,
                          const float opacity,
                          const std::optional<float> width,
                          const bool round_cap,
                          const bool is_outline) {
    if (is_outline) {
      pugi::xml_node element_node = write_path(layer_node, layer_to_world, positions, cyclic);
      write_fill_color_attribute(element_node, color, opacity);
    }
    else {
      /* Fill is always exported as polygon because the stroke of the fill is done
       * in a different SVG command. */
      pugi::xml_node element_node = write_polyline(
          layer_node, layer_to_world, positions, cyclic, width);

      if (width) {
        write_stroke_color_attribute(element_node, color, opacity, round_cap);
      }
      else {
        write_fill_color_attribute(element_node, color, opacity);
      }
    }
  };

  foreach_stroke_in_layer(object, layer, drawing, write_stroke);
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
  main_node.append_attribute("version").set_value("1.0");
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

pugi::xml_node SVGExporter::write_polygon(pugi::xml_node node,
                                          const float4x4 &transform,
                                          const Span<float3> positions)
{
  pugi::xml_node element_node = node.append_child("polygon");

  std::string txt;
  for (const int i : positions.index_range()) {
    if (i > 0) {
      txt.append(" ");
    }
    /* SVG has inverted Y axis. */
    const float2 screen_co = this->project_to_screen(transform, positions[i]);
    if (camera_persmat_) {
      txt.append(std::to_string(screen_co.x) + "," +
                 std::to_string(camera_rect_.size().y - screen_co.y));
    }
    else {
      txt.append(std::to_string(screen_co.x) + "," +
                 std::to_string(screen_rect_.size().y - screen_co.y));
    }
  }

  element_node.append_attribute("points").set_value(txt.c_str());

  return element_node;
}

pugi::xml_node SVGExporter::write_polyline(pugi::xml_node node,
                                           const float4x4 &transform,
                                           const Span<float3> positions,
                                           const bool cyclic,
                                           const std::optional<float> width)
{
  pugi::xml_node element_node = node.append_child(cyclic ? "polygon" : "polyline");

  if (width) {
    element_node.append_attribute("stroke-width").set_value(*width);
  }

  std::string txt;
  for (const int i : positions.index_range()) {
    if (i > 0) {
      txt.append(" ");
    }
    /* SVG has inverted Y axis. */
    const float2 screen_co = this->project_to_screen(transform, positions[i]);
    if (camera_persmat_) {
      txt.append(std::to_string(screen_co.x) + "," +
                 std::to_string(camera_rect_.size().y - screen_co.y));
    }
    else {
      txt.append(std::to_string(screen_co.x) + "," +
                 std::to_string(screen_rect_.size().y - screen_co.y));
    }
  }

  element_node.append_attribute("points").set_value(txt.c_str());

  return element_node;
}

pugi::xml_node SVGExporter::write_path(pugi::xml_node node,
                                       const float4x4 &transform,
                                       const Span<float3> positions,
                                       const bool cyclic)
{
  pugi::xml_node element_node = node.append_child("path");

  std::string txt = "M";
  for (const int i : positions.index_range()) {
    if (i > 0) {
      txt.append("L");
    }
    const float2 screen_co = this->project_to_screen(transform, positions[i]);
    /* SVG has inverted Y axis. */
    if (camera_persmat_) {
      txt.append(std::to_string(screen_co.x) + "," +
                 std::to_string(camera_rect_.size().y - screen_co.y));
    }
    else {
      txt.append(std::to_string(screen_co.x) + "," +
                 std::to_string(screen_rect_.size().y - screen_co.y));
    }
  }
  /* Close patch (cyclic). */
  if (cyclic) {
    txt.append("z");
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

bool export_svg(const IOContext &context,
                const ExportParams &params,
                Scene &scene,
                StringRefNull filepath)
{
  SVGExporter exporter(context, params);
  return exporter.export_scene(scene, filepath);
}

}  // namespace blender::io::grease_pencil
