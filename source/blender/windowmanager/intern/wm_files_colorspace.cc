/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Functions for handling file colorspaces.
 */

#include "BLI_colorspace.hh"
#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BKE_context.hh"
#include "BKE_image.hh"
#include "BKE_main.hh"
#include "BKE_movieclip.h"
#include "BKE_report.hh"

#include "DNA_windowmanager_enums.h"
#include "DNA_windowmanager_types.h"
#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "IMB_colormanagement.hh"

#include "DEG_depsgraph.hh"

#include "UI_interface_c.hh"
#include "UI_interface_icons.hh"
#include "UI_interface_layout.hh"

#include "BLT_translation.hh"

#include "ED_image.hh"
#include "ED_render.hh"

#include "RE_pipeline.h"

#include "SEQ_prefetch.hh"
#include "SEQ_relations.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "wm_files.hh"

/* -------------------------------------------------------------------- */
/** \name Set Working Color Space Operator
 * \{ */

static const EnumPropertyItem *working_space_itemf(bContext * /*C*/,
                                                   PointerRNA * /*ptr*/,
                                                   PropertyRNA * /*prop*/,
                                                   bool *r_free)
{
  EnumPropertyItem *item = nullptr;
  int totitem = 0;
  IMB_colormanagement_working_space_items_add(&item, &totitem);
  RNA_enum_item_end(&item, &totitem);
  *r_free = true;
  return item;
}

static bool wm_set_working_space_check_safe(bContext *C, wmOperator *op)
{
  const wmWindowManager *wm = CTX_wm_manager(C);
  const Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);

  if (WM_jobs_test(wm, scene, WM_JOB_TYPE_ANY)) {
    BKE_report(
        op->reports, RPT_WARNING, RPT_("Can't change working space while jobs are running"));
    return false;
  }

  if (ED_image_should_save_modified(bmain)) {
    BKE_report(op->reports,
               RPT_WARNING,
               RPT_("Can't change working space with modified images, save them first"));
    return false;
  }

  return true;
}

static wmOperatorStatus wm_set_working_color_space_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  const bool convert_colors = RNA_boolean_get(op->ptr, "convert_colors");
  const int working_space_index = RNA_enum_get(op->ptr, "working_space");
  const char *working_space = IMB_colormanagement_working_space_get_indexed_name(
      working_space_index);

  if (!wm_set_working_space_check_safe(C, op)) {
    return OPERATOR_CANCELLED;
  }

  if (working_space[0] == '\0' || STREQ(working_space, bmain->colorspace.scene_linear_name)) {
    return OPERATOR_CANCELLED;
  }

  /* Stop all viewport renders. */
  ED_render_engine_changed(bmain, true);
  RE_FreeAllPersistentData();

  /* Change working space. */
  IMB_colormanagement_working_space_set_from_name(working_space);

  if (convert_colors) {
    const bool depsgraph_tag = true;
    IMB_colormanagement_working_space_convert(bmain,
                                              bmain->colorspace.scene_linear_to_xyz,
                                              blender::colorspace::xyz_to_scene_linear,
                                              depsgraph_tag);
  }

  STRNCPY(bmain->colorspace.scene_linear_name, working_space);
  bmain->colorspace.scene_linear_to_xyz = blender::colorspace::scene_linear_to_xyz;

  /* Free all render, compositor and sequencer caches. */
  RE_FreeAllRenderResults();
  RE_FreeInteractiveCompositorRenders();
  blender::seq::prefetch_stop_all();
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    blender::seq::cache_cleanup(scene, blender::seq::CacheCleanup::All);
  }

  /* Free all images, they may have scene linear float buffers. */
  LISTBASE_FOREACH (Image *, image, &bmain->images) {
    DEG_id_tag_update(&image->id, ID_RECALC_SOURCE);
    BKE_image_signal(bmain, image, nullptr, IMA_SIGNAL_COLORMANAGE);
    BKE_image_partial_update_mark_full_update(image);
  }
  LISTBASE_FOREACH (MovieClip *, clip, &bmain->movieclips) {
    BKE_movieclip_clear_cache(clip);
    BKE_movieclip_free_gputexture(clip);
    DEG_id_tag_update(&clip->id, ID_RECALC_SOURCE);
  }

  /* Redraw everything. */
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, nullptr);
  WM_main_add_notifier(NC_SCENE | ND_RENDER_OPTIONS, nullptr);
  WM_main_add_notifier(NC_SCENE | ND_NODES, nullptr);
  WM_main_add_notifier(NC_WINDOW, nullptr);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus wm_set_working_color_space_invoke(bContext *C,
                                                          wmOperator *op,
                                                          const wmEvent *event)
{
  if (!wm_set_working_space_check_safe(C, op)) {
    return OPERATOR_CANCELLED;
  }

  if (RNA_enum_get(op->ptr, "working_space") == -1) {
    RNA_enum_set(op->ptr,
                 "working_space",
                 IMB_colormanagement_working_space_get_named_index(
                     IMB_colormanagement_working_space_get_default()));
  }

  const Main *bmain = CTX_data_main(C);
  const char *working_space = IMB_colormanagement_working_space_get_indexed_name(
      RNA_enum_get(op->ptr, "working_space"));
  if (STREQ(working_space, bmain->colorspace.scene_linear_name)) {
    return OPERATOR_CANCELLED;
  }

  return WM_operator_props_popup_confirm_ex(
      C,
      op,
      event,
      std::nullopt,
      IFACE_("Apply"),
      false,
      IFACE_("To match renders with the previous working space as closely as possible,\n"
             "colors in all materials, lights and geometry must be converted.\n\n"
             "Some nodes graphs cannot be converted accurately and may need manual fix-ups."));
}

void WM_OT_set_working_color_space(wmOperatorType *ot)
{
  ot->name = "Set Blend File Working Color Space";
  ot->idname = "WM_OT_set_working_color_space";
  ot->description = "Change the working color space of all colors in this blend file";

  ot->exec = wm_set_working_color_space_exec;
  ot->invoke = wm_set_working_color_space_invoke;

  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

  RNA_def_boolean(ot->srna,
                  "convert_colors",
                  true,
                  "Convert Colors in All Data-blocks",
                  "Change colors in all data-blocks to the new working space");
  PropertyRNA *prop = RNA_def_enum(ot->srna,
                                   "working_space",
                                   rna_enum_dummy_NULL_items,
                                   -1,
                                   "Working Space",
                                   "Color space to set");
  RNA_def_enum_funcs(prop, working_space_itemf);

  ot->prop = prop;
}

/** \} */
