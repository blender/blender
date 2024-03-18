/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bgpencil
 */

#include <cstdio>

#include "BLI_listbase.h"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_context.hh"
#include "BKE_gpencil_legacy.h"
#include "BKE_main.hh"
#include "BKE_scene.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

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

  switch (iparams->frame_mode) {
    case GP_EXPORT_FRAME_ACTIVE: {
      exporter->prepare_camera_params(scene, iparams);
      exporter->add_newpage();
      exporter->add_body();
      result = exporter->write();
      break;
    }
    case GP_EXPORT_FRAME_SELECTED:
    case GP_EXPORT_FRAME_SCENE: {
      for (int32_t i = iparams->frame_start; i < iparams->frame_end + 1; i++) {
        if ((iparams->frame_mode == GP_EXPORT_FRAME_SELECTED) &&
            !is_keyframe_included(gpd_eval, i, true))
        {
          continue;
        }

        scene->r.cfra = i;
        BKE_scene_graph_update_for_newframe(depsgraph);
        exporter->prepare_camera_params(scene, iparams);
        exporter->frame_number_set(i);
        exporter->add_newpage();
        exporter->add_body();
      }
      result = exporter->write();
      /* Back to original frame. */
      exporter->frame_number_set(iparams->frame_cur);
      scene->r.cfra = iparams->frame_cur;
      BKE_scene_camera_switch_update(scene);
      BKE_scene_graph_update_for_newframe(depsgraph);
      break;
    }
    default:
      break;
  }

  return result;
}
#endif

/* Export current frame in SVG. */
#ifdef WITH_PUGIXML
static bool gpencil_io_export_frame_svg(GpencilExporterSVG *exporter,
                                        Scene *scene,
                                        const GpencilIOParams *iparams,
                                        const bool newpage,
                                        const bool body,
                                        const bool savepage)
{
  bool result = false;
  exporter->frame_number_set(iparams->frame_cur);
  exporter->prepare_camera_params(scene, iparams);

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

bool gpencil_io_import(const char *filepath, GpencilIOParams *iparams)
{
  GpencilImporterSVG importer = GpencilImporterSVG(filepath, iparams);

  return gpencil_io_import_frame(&importer, *iparams);
}

bool gpencil_io_export(const char *filepath, GpencilIOParams *iparams)
{
  Depsgraph *depsgraph_ = CTX_data_depsgraph_pointer(iparams->C);
  Scene *scene_ = CTX_data_scene(iparams->C);
  Object *ob = CTX_data_active_object(iparams->C);

  UNUSED_VARS(filepath, depsgraph_, scene_, ob);

  switch (iparams->mode) {
#ifdef WITH_PUGIXML
    case GP_EXPORT_TO_SVG: {
      GpencilExporterSVG exporter = GpencilExporterSVG(filepath, iparams);
      return gpencil_io_export_frame_svg(&exporter, scene_, iparams, true, true, true);
      break;
    }
#endif
#ifdef WITH_HARU
    case GP_EXPORT_TO_PDF: {
      GpencilExporterPDF exporter = GpencilExporterPDF(filepath, iparams);
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
