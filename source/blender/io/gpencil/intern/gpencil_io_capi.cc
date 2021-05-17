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

#include <cstdio>

#include "BLI_listbase.h"

#include "DNA_gpencil_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_gpencil.h"
#include "BKE_main.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "../gpencil_io.h"

#ifdef WITH_HARU
#  include "gpencil_io_export_pdf.hh"
#endif

#ifdef WITH_PUGIXML
#  include "gpencil_io_export_svg.hh"
#endif

#include "gpencil_io_import_svg.hh"

#ifdef WITH_HARU
using blender::io::gpencil::GpencilExporterPDF;
#endif
#ifdef WITH_PUGIXML
using blender::io::gpencil::GpencilExporterSVG;
#endif
using blender::io::gpencil::GpencilImporterSVG;

/* Check if frame is included. */
#ifdef WITH_HARU
static bool is_keyframe_included(bGPdata *gpd_, const int32_t framenum, const bool use_selected)
{
  /* Check if exist a frame. */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd_->layers) {
    if (gpl->flag & GP_LAYER_HIDE) {
      continue;
    }
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      if (gpf->framenum == framenum) {
        if ((!use_selected) || (use_selected && (gpf->flag & GP_FRAME_SELECT))) {
          return true;
        }
      }
    }
  }
  return false;
}
#endif

/* Import frame. */
static bool gpencil_io_import_frame(void *in_importer, const GpencilIOParams &iparams)
{

  bool result = false;
  switch (iparams.mode) {
    case GP_IMPORT_FROM_SVG: {
      GpencilImporterSVG *importer = (GpencilImporterSVG *)in_importer;
      result |= importer->read();
      break;
    }
    /* Add new import formats here. */
    default:
      break;
  }

  return result;
}

/* Export frame in PDF. */
#ifdef WITH_HARU
static bool gpencil_io_export_pdf(Depsgraph *depsgraph,
                                  Scene *scene,
                                  Object *ob,
                                  GpencilExporterPDF *exporter,
                                  const GpencilIOParams *iparams)
{
  bool result = false;
  Object *ob_eval_ = (Object *)DEG_get_evaluated_id(depsgraph, &ob->id);
  bGPdata *gpd_eval = (bGPdata *)ob_eval_->data;

  exporter->frame_number_set(iparams->frame_cur);
  result |= exporter->new_document();

  const bool use_frame_selected = (iparams->frame_mode == GP_EXPORT_FRAME_SELECTED);
  if (use_frame_selected) {
    for (int32_t i = iparams->frame_start; i < iparams->frame_end + 1; i++) {
      if (!is_keyframe_included(gpd_eval, i, use_frame_selected)) {
        continue;
      }

      CFRA = i;
      BKE_scene_graph_update_for_newframe(depsgraph);
      exporter->prepare_camera_params(iparams);
      exporter->frame_number_set(i);
      exporter->add_newpage();
      exporter->add_body();
    }
    result = exporter->write();
    /* Back to original frame. */
    exporter->frame_number_set(iparams->frame_cur);
    CFRA = iparams->frame_cur;
    BKE_scene_graph_update_for_newframe(depsgraph);
  }
  else {
    exporter->prepare_camera_params(iparams);
    exporter->add_newpage();
    exporter->add_body();
    result = exporter->write();
  }

  return result;
}
#endif

/* Export current frame in SVG. */
#ifdef WITH_PUGIXML
static bool gpencil_io_export_frame_svg(GpencilExporterSVG *exporter,
                                        const GpencilIOParams *iparams,
                                        const bool newpage,
                                        const bool body,
                                        const bool savepage)
{
  bool result = false;
  exporter->frame_number_set(iparams->frame_cur);
  exporter->prepare_camera_params(iparams);

  if (newpage) {
    result |= exporter->add_newpage();
  }
  if (body) {
    result |= exporter->add_body();
  }
  if (savepage) {
    result = exporter->write();
  }
  return result;
}
#endif

/* Main import entry point function. */
bool gpencil_io_import(const char *filename, GpencilIOParams *iparams)
{
  GpencilImporterSVG importer = GpencilImporterSVG(filename, iparams);

  return gpencil_io_import_frame(&importer, *iparams);
}

/* Main export entry point function. */
bool gpencil_io_export(const char *filename, GpencilIOParams *iparams)
{
  Depsgraph *depsgraph_ = CTX_data_depsgraph_pointer(iparams->C);
  Scene *scene_ = CTX_data_scene(iparams->C);
  Object *ob = CTX_data_active_object(iparams->C);

  UNUSED_VARS(filename, depsgraph_, scene_, ob);

  switch (iparams->mode) {
#ifdef WITH_PUGIXML
    case GP_EXPORT_TO_SVG: {
      GpencilExporterSVG exporter = GpencilExporterSVG(filename, iparams);
      return gpencil_io_export_frame_svg(&exporter, iparams, true, true, true);
      break;
    }
#endif
#ifdef WITH_HARU
    case GP_EXPORT_TO_PDF: {
      GpencilExporterPDF exporter = GpencilExporterPDF(filename, iparams);
      return gpencil_io_export_pdf(depsgraph_, scene_, ob, &exporter, iparams);
      break;
    }
#endif
    /* Add new export formats here. */
    default:
      break;
  }
  return false;
}
