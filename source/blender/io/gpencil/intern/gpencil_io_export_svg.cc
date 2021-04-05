/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation
 * All rights reserved.
 */

/** \file
 * \ingroup bgpencil
 */

#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_gpencil_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_gpencil.h"
#include "BKE_gpencil_geom.h"
#include "BKE_main.h"
#include "BKE_material.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "ED_gpencil.h"
#include "ED_view3d.h"

#ifdef WIN32
#  include "utfconv.h"
#endif

#include "UI_view2d.h"

#include "gpencil_io.h"
#include "gpencil_io_export_svg.hh"

#include "pugixml.hpp"

namespace blender ::io ::gpencil {

/* Constructor. */
GpencilExporterSVG::GpencilExporterSVG(const char *filename, const GpencilIOParams *iparams)
    : GpencilExporter(iparams)
{
  filename_set(filename);

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
  char filename_cstr[FILE_MAX];
  BLI_strncpy(filename_cstr, filename_, FILE_MAX);

  UTF16_ENCODE(filename_cstr);
  std::wstring wstr(filename_cstr_16);
  result = main_doc_.save_file(wstr.c_str());

  UTF16_UN_ENCODE(filename_cstr);
#else
  result = main_doc_.save_file(filename_);
#endif

  return result;
}

/* Create document header and main svg node. */
void GpencilExporterSVG::create_document_header()
{
  /* Add a custom document declaration node. */
  pugi::xml_node decl = main_doc_.prepend_child(pugi::node_declaration);
  decl.append_attribute("version") = "1.0";
  decl.append_attribute("encoding") = "UTF-8";

  pugi::xml_node comment = main_doc_.append_child(pugi::node_comment);
  char txt[128];
  sprintf(txt, " Generator: Blender, %s - %s ", SVG_EXPORTER_NAME, SVG_EXPORTER_VERSION);
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

/* Main layer loop. */
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
    sprintf(obtxt, "blender_object_%s", ob->id.name + 2);
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
        gps_duplicate->thickness *= mat4_to_scale(ob->obmat);
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
                rv3d_, gpd_, gpl, gps_duplicate, 3, diff_mat_.values);

            /* Sample stroke. */
            if (params_.stroke_sample > 0.0f) {
              BKE_gpencil_stroke_sample(gpd_eval, gps_perimeter, params_.stroke_sample, false);
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

/**
 * Export a stroke using SVG path
 * \param node_gpl: Node of the layer.
 * \param do_fill: True if the stroke is only fill
 */
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
  /* Close patch (cyclic)*/
  if (gps->flag & GP_STROKE_CYCLIC) {
    txt.append("z");
  }

  node_gps.append_attribute("d").set_value(txt.c_str());
}

/**
 * Export a stroke using polyline or polygon
 * \param node_gpl: Node of the layer.
 * \param do_fill: True if the stroke is only fill
 */
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
  gps_temp->points = (bGPDspoint *)MEM_callocN(sizeof(bGPDspoint), "gp_stroke_points");
  bGPDspoint *pt_src = &gps->points[0];
  bGPDspoint *pt_dst = &gps_temp->points[0];
  copy_v3_v3(&pt_dst->x, &pt_src->x);
  pt_dst->pressure = avg_pressure;

  const float radius = stroke_point_radius_get(gpl, gps_temp);

  BKE_gpencil_free_stroke(gps_temp);

  pugi::xml_node node_gps = node_gpl.append_child(do_fill || cyclic ? "polygon" : "polyline");

  color_string_set(gpl, gps, node_gps, do_fill);

  if (is_stroke && !do_fill) {
    node_gps.append_attribute("stroke-width").set_value((radius * 2.0f) - gpl->line_change);
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

/**
 * Set color SVG string for stroke
 * \param node_gps: Stroke node
 * @param do_fill: True if the stroke is only fill
 */
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

/**
 * Create a SVG rectangle
 * \param node: Parent node
 * \param x: X location
 * \param y: Y location
 * \param width: width of the rectangle
 * \param height: Height of the rectangle
 * \param thickness: Thickness of the line
 * \param hexcolor: Color of the line
 */
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

/**
 * Create SVG text
 * \param node: Parent node
 * \param x: X location
 * \param y: Y location
 * \param text: Text to include
 * \param size: Size of the text
 * \param hexcolor: Color of the text
 */
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

/** Convert a color to Hex value (#FFFFFF). */
std::string GpencilExporterSVG::rgb_to_hexstr(const float color[3])
{
  uint8_t r = color[0] * 255.0f;
  uint8_t g = color[1] * 255.0f;
  uint8_t b = color[2] * 255.0f;
  char hex_string[20];
  sprintf(hex_string, "#%02X%02X%02X", r, g, b);

  std::string hexstr = hex_string;

  return hexstr;
}

}  // namespace blender::io::gpencil
