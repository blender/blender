/* SPDX-FileCopyrightText: 2020 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bgpencil
 */

#include "BLI_math_color.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_main.h"
#include "BKE_material.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "ED_gpencil_legacy.hh"
#include "ED_view3d.hh"

#ifdef WIN32
#  include "utfconv.h"
#endif

#include "UI_view2d.hh"

#include "gpencil_io.h"
#include "gpencil_io_export_svg.hh"

#include "pugixml.hpp"

namespace blender ::io ::gpencil {

/* Constructor. */
GpencilExporterSVG::GpencilExporterSVG(const char *filepath, const GpencilIOParams *iparams)
    : GpencilExporter(iparams)
{
  filepath_set(filepath);

  invert_axis_[0] = false;
  invert_axis_[1] = true;
}

bool GpencilExporterSVG::add_newpage()
{
  create_document_header();
  return true;
}

bool GpencilExporterSVG::add_body()
{
  export_gpencil_layers();
  return true;
}

bool GpencilExporterSVG::write()
{
  bool result = true;
/* Support unicode character paths on Windows. */
#ifdef WIN32
  char filepath_cstr[FILE_MAX];
  BLI_strncpy(filepath_cstr, filepath_, FILE_MAX);

  UTF16_ENCODE(filepath_cstr);
  std::wstring wstr(filepath_cstr_16);
  result = main_doc_.save_file(wstr.c_str());

  UTF16_UN_ENCODE(filepath_cstr);
#else
  result = main_doc_.save_file(filepath_);
#endif

  return result;
}

void GpencilExporterSVG::create_document_header()
{
  /* Add a custom document declaration node. */
  pugi::xml_node decl = main_doc_.prepend_child(pugi::node_declaration);
  decl.append_attribute("version") = "1.0";
  decl.append_attribute("encoding") = "UTF-8";

  pugi::xml_node comment = main_doc_.append_child(pugi::node_comment);
  char txt[128];
  SNPRINTF(txt, " Generator: Blender, %s - %s ", SVG_EXPORTER_NAME, SVG_EXPORTER_VERSION);
  comment.set_value(txt);

  pugi::xml_node doctype = main_doc_.append_child(pugi::node_doctype);
  doctype.set_value(
      "svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" "
      "\"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\"");

  main_node_ = main_doc_.append_child("svg");
  main_node_.append_attribute("version").set_value("1.0");
  main_node_.append_attribute("x").set_value("0px");
  main_node_.append_attribute("y").set_value("0px");
  main_node_.append_attribute("xmlns").set_value("http://www.w3.org/2000/svg");

  std::string width;
  std::string height;

  width = std::to_string(render_x_);
  height = std::to_string(render_y_);

  main_node_.append_attribute("width").set_value((width + "px").c_str());
  main_node_.append_attribute("height").set_value((height + "px").c_str());
  std::string viewbox = "0 0 " + width + " " + height;
  main_node_.append_attribute("viewBox").set_value(viewbox.c_str());
}

void GpencilExporterSVG::export_gpencil_layers()
{
  const bool is_clipping = is_camera_mode() && (params_.flag & GP_EXPORT_CLIP_CAMERA) != 0;

  /* If is doing a set of frames, the list of objects can change for each frame. */
  create_object_list();

  for (ObjectZ &obz : ob_list_) {
    Object *ob = obz.ob;

    /* Camera clipping. */
    if (is_clipping) {
      pugi::xml_node clip_node = main_node_.append_child("clipPath");
      clip_node.append_attribute("id").set_value(("clip-path" + std::to_string(cfra_)).c_str());

      add_rect(clip_node, 0, 0, render_x_, render_y_, 0.0f, "#000000");
    }

    frame_node_ = main_node_.append_child("g");
    std::string frametxt = "blender_frame_" + std::to_string(cfra_);
    frame_node_.append_attribute("id").set_value(frametxt.c_str());

    /* Clip area. */
    if (is_clipping) {
      frame_node_.append_attribute("clip-path")
          .set_value(("url(#clip-path" + std::to_string(cfra_) + ")").c_str());
    }

    pugi::xml_node ob_node = frame_node_.append_child("g");

    char obtxt[96];
    SNPRINTF(obtxt, "blender_object_%s", ob->id.name + 2);
    ob_node.append_attribute("id").set_value(obtxt);

    /* Use evaluated version to get strokes with modifiers. */
    Object *ob_eval_ = (Object *)DEG_get_evaluated_id(depsgraph_, &ob->id);
    bGPdata *gpd_eval = (bGPdata *)ob_eval_->data;

    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd_eval->layers) {
      if (gpl->flag & GP_LAYER_HIDE) {
        continue;
      }
      prepare_layer_export_matrix(ob, gpl);

      bGPDframe *gpf = gpl->actframe;
      if ((gpf == nullptr) || (gpf->strokes.first == nullptr)) {
        continue;
      }

      /* Layer node. */
      std::string txt = "Layer: ";
      txt.append(gpl->info);
      ob_node.append_child(pugi::node_comment).set_value(txt.c_str());

      pugi::xml_node node_gpl = ob_node.append_child("g");
      node_gpl.append_attribute("id").set_value(gpl->info);

      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        if (gps->totpoints < 2) {
          continue;
        }
        if (!ED_gpencil_stroke_material_visible(ob, gps)) {
          continue;
        }

        /* Duplicate the stroke to apply any layer thickness change. */
        bGPDstroke *gps_duplicate = BKE_gpencil_stroke_duplicate(gps, true, false);

        MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob,
                                                                       gps_duplicate->mat_nr + 1);

        const bool is_stroke = ((gp_style->flag & GP_MATERIAL_STROKE_SHOW) &&
                                (gp_style->stroke_rgba[3] > GPENCIL_ALPHA_OPACITY_THRESH));
        const bool is_fill = ((gp_style->flag & GP_MATERIAL_FILL_SHOW) &&
                              (gp_style->fill_rgba[3] > GPENCIL_ALPHA_OPACITY_THRESH));

        prepare_stroke_export_colors(ob, gps_duplicate);

        /* Apply layer thickness change. */
        gps_duplicate->thickness += gpl->line_change;
        /* Apply object scale to thickness. */
        const float scalef = mat4_to_scale(ob->object_to_world);
        gps_duplicate->thickness = ceilf(float(gps_duplicate->thickness) * scalef);
        CLAMP_MIN(gps_duplicate->thickness, 1.0f);

        const bool is_normalized = ((params_.flag & GP_EXPORT_NORM_THICKNESS) != 0) ||
                                   BKE_gpencil_stroke_is_pressure_constant(gps);

        /* Fill. */
        if ((is_fill) && (params_.flag & GP_EXPORT_FILL)) {
          /* Fill is always exported as polygon because the stroke of the fill is done
           * in a different SVG command. */
          export_stroke_to_polyline(gpl, gps_duplicate, node_gpl, is_stroke, true);
        }

        /* Stroke. */
        if (is_stroke) {
          if (is_normalized) {
            export_stroke_to_polyline(gpl, gps_duplicate, node_gpl, is_stroke, false);
          }
          else {
            bGPDstroke *gps_perimeter = BKE_gpencil_stroke_perimeter_from_view(
                rv3d_->viewmat, gpd_, gpl, gps_duplicate, 3, diff_mat_.ptr(), 0.0f);

            /* Sample stroke. */
            if (params_.stroke_sample > 0.0f) {
              BKE_gpencil_stroke_sample(gpd_eval, gps_perimeter, params_.stroke_sample, false, 0);
            }

            export_stroke_to_path(gpl, gps_perimeter, node_gpl, false);

            BKE_gpencil_free_stroke(gps_perimeter);
          }
        }

        BKE_gpencil_free_stroke(gps_duplicate);
      }
    }
  }
}

void GpencilExporterSVG::export_stroke_to_path(bGPDlayer *gpl,
                                               bGPDstroke *gps,
                                               pugi::xml_node node_gpl,
                                               const bool do_fill)
{
  pugi::xml_node node_gps = node_gpl.append_child("path");

  float col[3];
  std::string stroke_hex;
  if (do_fill) {
    node_gps.append_attribute("fill-opacity").set_value(fill_color_[3] * gpl->opacity);

    interp_v3_v3v3(col, fill_color_, gpl->tintcolor, gpl->tintcolor[3]);
  }
  else {
    node_gps.append_attribute("fill-opacity")
        .set_value(stroke_color_[3] * stroke_average_opacity_get() * gpl->opacity);

    interp_v3_v3v3(col, stroke_color_, gpl->tintcolor, gpl->tintcolor[3]);
  }

  linearrgb_to_srgb_v3_v3(col, col);
  stroke_hex = rgb_to_hexstr(col);

  node_gps.append_attribute("fill").set_value(stroke_hex.c_str());
  node_gps.append_attribute("stroke").set_value("none");

  std::string txt = "M";
  for (const int i : IndexRange(gps->totpoints)) {
    if (i > 0) {
      txt.append("L");
    }
    bGPDspoint &pt = gps->points[i];
    const float2 screen_co = gpencil_3D_point_to_2D(&pt.x);
    txt.append(std::to_string(screen_co.x) + "," + std::to_string(screen_co.y));
  }
  /* Close patch (cyclic). */
  if (gps->flag & GP_STROKE_CYCLIC) {
    txt.append("z");
  }

  node_gps.append_attribute("d").set_value(txt.c_str());
}

void GpencilExporterSVG::export_stroke_to_polyline(bGPDlayer *gpl,
                                                   bGPDstroke *gps,
                                                   pugi::xml_node node_gpl,
                                                   const bool is_stroke,
                                                   const bool do_fill)
{
  const bool cyclic = ((gps->flag & GP_STROKE_CYCLIC) != 0);
  const float avg_pressure = BKE_gpencil_stroke_average_pressure_get(gps);

  /* Get the thickness in pixels using a simple 1 point stroke. */
  bGPDstroke *gps_temp = BKE_gpencil_stroke_duplicate(gps, false, false);
  gps_temp->totpoints = 1;
  gps_temp->points = MEM_new<bGPDspoint>("gp_stroke_points");
  bGPDspoint *pt_src = &gps->points[0];
  bGPDspoint *pt_dst = &gps_temp->points[0];
  copy_v3_v3(&pt_dst->x, &pt_src->x);
  pt_dst->pressure = avg_pressure;

  const float radius = stroke_point_radius_get(gpl, gps_temp);

  BKE_gpencil_free_stroke(gps_temp);

  pugi::xml_node node_gps = node_gpl.append_child(do_fill || cyclic ? "polygon" : "polyline");

  color_string_set(gpl, gps, node_gps, do_fill);

  if (is_stroke && !do_fill) {
    const float defined_width = (gps->thickness * avg_pressure) + gpl->line_change;
    const float estimated_width = (radius * 2.0f) + gpl->line_change;
    const float final_width = (avg_pressure == 1.0f) ? MAX2(defined_width, estimated_width) :
                                                       estimated_width;
    node_gps.append_attribute("stroke-width").set_value(MAX2(final_width, 1.0f));
  }

  std::string txt;
  for (const int i : IndexRange(gps->totpoints)) {
    if (i > 0) {
      txt.append(" ");
    }
    bGPDspoint *pt = &gps->points[i];
    const float2 screen_co = gpencil_3D_point_to_2D(&pt->x);
    txt.append(std::to_string(screen_co.x) + "," + std::to_string(screen_co.y));
  }

  node_gps.append_attribute("points").set_value(txt.c_str());
}

void GpencilExporterSVG::color_string_set(bGPDlayer *gpl,
                                          bGPDstroke *gps,
                                          pugi::xml_node node_gps,
                                          const bool do_fill)
{
  const bool round_cap = (gps->caps[0] == GP_STROKE_CAP_ROUND ||
                          gps->caps[1] == GP_STROKE_CAP_ROUND);

  float col[3];
  if (do_fill) {
    interp_v3_v3v3(col, fill_color_, gpl->tintcolor, gpl->tintcolor[3]);
    linearrgb_to_srgb_v3_v3(col, col);
    std::string stroke_hex = rgb_to_hexstr(col);
    node_gps.append_attribute("fill").set_value(stroke_hex.c_str());
    node_gps.append_attribute("stroke").set_value("none");
    node_gps.append_attribute("fill-opacity").set_value(fill_color_[3] * gpl->opacity);
  }
  else {
    interp_v3_v3v3(col, stroke_color_, gpl->tintcolor, gpl->tintcolor[3]);
    linearrgb_to_srgb_v3_v3(col, col);
    std::string stroke_hex = rgb_to_hexstr(col);
    node_gps.append_attribute("stroke").set_value(stroke_hex.c_str());
    node_gps.append_attribute("stroke-opacity")
        .set_value(stroke_color_[3] * stroke_average_opacity_get() * gpl->opacity);

    if (gps->totpoints > 1) {
      node_gps.append_attribute("fill").set_value("none");
      node_gps.append_attribute("stroke-linecap").set_value(round_cap ? "round" : "square");
    }
    else {
      node_gps.append_attribute("fill").set_value(stroke_hex.c_str());
      node_gps.append_attribute("fill-opacity").set_value(fill_color_[3] * gpl->opacity);
    }
  }
}

void GpencilExporterSVG::add_rect(pugi::xml_node node,
                                  float x,
                                  float y,
                                  float width,
                                  float height,
                                  float thickness,
                                  std::string hexcolor)
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

void GpencilExporterSVG::add_text(pugi::xml_node node,
                                  float x,
                                  float y,
                                  std::string text,
                                  const float size,
                                  std::string hexcolor)
{
  pugi::xml_node nodetxt = node.append_child("text");

  nodetxt.append_attribute("x").set_value(x);
  nodetxt.append_attribute("y").set_value(y);
  // nodetxt.append_attribute("font-family").set_value("'system-ui'");
  nodetxt.append_attribute("font-size").set_value(size);
  nodetxt.append_attribute("fill").set_value(hexcolor.c_str());
  nodetxt.text().set(text.c_str());
}

std::string GpencilExporterSVG::rgb_to_hexstr(const float color[3])
{
  uint8_t r = color[0] * 255.0f;
  uint8_t g = color[1] * 255.0f;
  uint8_t b = color[2] * 255.0f;
  char hex_string[20];
  SNPRINTF(hex_string, "#%02X%02X%02X", r, g, b);

  std::string hexstr = hex_string;

  return hexstr;
}

}  // namespace blender::io::gpencil
