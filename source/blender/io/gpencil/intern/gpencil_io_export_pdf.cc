/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bgpencil
 */

#include "BLI_math_color.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BKE_context.h"
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
#include "gpencil_io_export_pdf.hh"

namespace blender ::io ::gpencil {

static void error_handler(HPDF_STATUS error_no, HPDF_STATUS detail_no, void * /*user_data*/)
{
  printf("ERROR: error_no=%04X, detail_no=%u\n", (HPDF_UINT)error_no, (HPDF_UINT)detail_no);
}

/* Constructor. */
GpencilExporterPDF::GpencilExporterPDF(const char *filepath, const GpencilIOParams *iparams)
    : GpencilExporter(iparams)
{
  filepath_set(filepath);

  invert_axis_[0] = false;
  invert_axis_[1] = false;

  pdf_ = nullptr;
  page_ = nullptr;
}

bool GpencilExporterPDF::new_document()
{
  return create_document();
}

bool GpencilExporterPDF::add_newpage()
{
  return add_page();
}

bool GpencilExporterPDF::add_body()
{
  export_gpencil_layers();
  return true;
}

bool GpencilExporterPDF::write()
{
  /* Support unicode character paths on Windows. */
  HPDF_STATUS res = 0;

  /* TODO: It looks `libharu` does not support unicode. */
#if 0 /* `ifdef WIN32` */
  char filepath_cstr[FILE_MAX];
  BLI_strncpy(filepath_cstr, filepath_, FILE_MAX);

  UTF16_ENCODE(filepath_cstr);
  std::wstring wstr(filepath_cstr_16);
  res = HPDF_SaveToFile(pdf_, wstr.c_str());

  UTF16_UN_ENCODE(filepath_cstr);
#else
  res = HPDF_SaveToFile(pdf_, filepath_);
#endif

  return (res == 0) ? true : false;
}

bool GpencilExporterPDF::create_document()
{
  pdf_ = HPDF_New(error_handler, nullptr);
  if (!pdf_) {
    std::cout << "error: cannot create PdfDoc object\n";
    return false;
  }
  return true;
}

bool GpencilExporterPDF::add_page()
{
  /* Add a new page object. */
  page_ = HPDF_AddPage(pdf_);
  if (!pdf_) {
    std::cout << "error: cannot create PdfPage\n";
    return false;
  }

  HPDF_Page_SetWidth(page_, render_x_);
  HPDF_Page_SetHeight(page_, render_y_);

  return true;
}

void GpencilExporterPDF::export_gpencil_layers()
{
  /* If is doing a set of frames, the list of objects can change for each frame. */
  create_object_list();

  const bool is_normalized = ((params_.flag & GP_EXPORT_NORM_THICKNESS) != 0);

  for (ObjectZ &obz : ob_list_) {
    Object *ob = obz.ob;

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

      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        if (gps->totpoints < 2) {
          continue;
        }
        if (!ED_gpencil_stroke_material_visible(ob, gps)) {
          continue;
        }
        /* Skip invisible lines. */
        prepare_stroke_export_colors(ob, gps);
        const float fill_opacity = fill_color_[3] * gpl->opacity;
        const float stroke_opacity = stroke_color_[3] * stroke_average_opacity_get() *
                                     gpl->opacity;
        if ((fill_opacity < GPENCIL_ALPHA_OPACITY_THRESH) &&
            (stroke_opacity < GPENCIL_ALPHA_OPACITY_THRESH))
        {
          continue;
        }

        MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, gps->mat_nr + 1);
        const bool is_stroke = ((gp_style->flag & GP_MATERIAL_STROKE_SHOW) &&
                                (gp_style->stroke_rgba[3] > GPENCIL_ALPHA_OPACITY_THRESH) &&
                                (stroke_opacity > GPENCIL_ALPHA_OPACITY_THRESH));
        const bool is_fill = ((gp_style->flag & GP_MATERIAL_FILL_SHOW) &&
                              (gp_style->fill_rgba[3] > GPENCIL_ALPHA_OPACITY_THRESH));

        if ((!is_stroke) && (!is_fill)) {
          continue;
        }

        /* Duplicate the stroke to apply any layer thickness change. */
        bGPDstroke *gps_duplicate = BKE_gpencil_stroke_duplicate(gps, true, false);

        /* Apply layer thickness change. */
        gps_duplicate->thickness += gpl->line_change;
        /* Apply object scale to thickness. */
        const float scalef = mat4_to_scale(ob->object_to_world);
        gps_duplicate->thickness = ceilf(float(gps_duplicate->thickness) * scalef);
        CLAMP_MIN(gps_duplicate->thickness, 1.0f);
        /* Fill. */
        if ((is_fill) && (params_.flag & GP_EXPORT_FILL)) {
          /* Fill is exported as polygon for fill and stroke in a different shape. */
          export_stroke_to_polyline(gpl, gps_duplicate, is_stroke, true, false);
        }

        /* Stroke. */
        if (is_stroke) {
          if (is_normalized) {
            export_stroke_to_polyline(gpl, gps_duplicate, is_stroke, false, true);
          }
          else {
            bGPDstroke *gps_perimeter = BKE_gpencil_stroke_perimeter_from_view(
                rv3d_->viewmat, gpd_, gpl, gps_duplicate, 3, diff_mat_.ptr(), 0.0f);

            /* Sample stroke. */
            if (params_.stroke_sample > 0.0f) {
              BKE_gpencil_stroke_sample(gpd_eval, gps_perimeter, params_.stroke_sample, false, 0);
            }

            export_stroke_to_polyline(gpl, gps_perimeter, is_stroke, false, false);

            BKE_gpencil_free_stroke(gps_perimeter);
          }
        }
        BKE_gpencil_free_stroke(gps_duplicate);
      }
    }
  }
}

void GpencilExporterPDF::export_stroke_to_polyline(bGPDlayer *gpl,
                                                   bGPDstroke *gps,
                                                   const bool is_stroke,
                                                   const bool do_fill,
                                                   const bool normalize)
{
  const bool cyclic = ((gps->flag & GP_STROKE_CYCLIC) != 0);
  const float avg_pressure = BKE_gpencil_stroke_average_pressure_get(gps);

  /* Get the thickness in pixels using a simple 1 point stroke. */
  bGPDstroke *gps_temp = BKE_gpencil_stroke_duplicate(gps, false, false);
  gps_temp->totpoints = 1;
  gps_temp->points = MEM_new<bGPDspoint>("gp_stroke_points");
  const bGPDspoint *pt_src = &gps->points[0];
  bGPDspoint *pt_dst = &gps_temp->points[0];
  copy_v3_v3(&pt_dst->x, &pt_src->x);
  pt_dst->pressure = avg_pressure;

  const float radius = stroke_point_radius_get(gpl, gps_temp);

  BKE_gpencil_free_stroke(gps_temp);

  color_set(gpl, do_fill);

  if (is_stroke && !do_fill) {
    HPDF_Page_SetLineJoin(page_, HPDF_ROUND_JOIN);
    const float defined_width = (gps->thickness * avg_pressure) + gpl->line_change;
    const float estimated_width = (radius * 2.0f) + gpl->line_change;
    const float final_width = (avg_pressure == 1.0f) ? MAX2(defined_width, estimated_width) :
                                                       estimated_width;
    HPDF_Page_SetLineWidth(page_, MAX2(final_width, 1.0f));
  }

  /* Loop all points. */
  for (const int i : IndexRange(gps->totpoints)) {
    bGPDspoint *pt = &gps->points[i];
    const float2 screen_co = gpencil_3D_point_to_2D(&pt->x);
    if (i == 0) {
      HPDF_Page_MoveTo(page_, screen_co.x, screen_co.y);
    }
    else {
      HPDF_Page_LineTo(page_, screen_co.x, screen_co.y);
    }
  }
  /* Close cyclic */
  if (cyclic) {
    HPDF_Page_ClosePath(page_);
  }

  if (do_fill || !normalize) {
    HPDF_Page_Fill(page_);
  }
  else {
    HPDF_Page_Stroke(page_);
  }

  HPDF_Page_GRestore(page_);
}

void GpencilExporterPDF::color_set(bGPDlayer *gpl, const bool do_fill)
{
  const float fill_opacity = fill_color_[3] * gpl->opacity;
  const float stroke_opacity = stroke_color_[3] * stroke_average_opacity_get() * gpl->opacity;
  const bool need_state = (do_fill && fill_opacity < 1.0f) || (stroke_opacity < 1.0f);

  HPDF_Page_GSave(page_);
  HPDF_ExtGState gstate = (need_state) ? HPDF_CreateExtGState(pdf_) : nullptr;

  float col[3];
  if (do_fill) {
    interp_v3_v3v3(col, fill_color_, gpl->tintcolor, gpl->tintcolor[3]);
    linearrgb_to_srgb_v3_v3(col, col);
    CLAMP3(col, 0.0f, 1.0f);
    HPDF_Page_SetRGBFill(page_, col[0], col[1], col[2]);
    if (gstate) {
      HPDF_ExtGState_SetAlphaFill(gstate, clamp_f(fill_opacity, 0.0f, 1.0f));
    }
  }
  else {
    interp_v3_v3v3(col, stroke_color_, gpl->tintcolor, gpl->tintcolor[3]);
    linearrgb_to_srgb_v3_v3(col, col);
    CLAMP3(col, 0.0f, 1.0f);

    HPDF_Page_SetRGBFill(page_, col[0], col[1], col[2]);
    HPDF_Page_SetRGBStroke(page_, col[0], col[1], col[2]);
    if (gstate) {
      HPDF_ExtGState_SetAlphaFill(gstate, clamp_f(stroke_opacity, 0.0f, 1.0f));
      HPDF_ExtGState_SetAlphaStroke(gstate, clamp_f(stroke_opacity, 0.0f, 1.0f));
    }
  }
  if (gstate) {
    HPDF_Page_SetExtGState(page_, gstate);
  }
}
}  // namespace blender::io::gpencil
