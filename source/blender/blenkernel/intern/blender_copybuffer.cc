/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Used for copy/paste operator, (using a temporary file).
 */

#include <cstdlib>

#include "BKE_blender_copybuffer.hh" /* own include */
#include "BKE_blendfile_link_append.hh"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"

#include "DEG_depsgraph_build.hh"

#include "BLO_readfile.hh"
#include "BLO_writefile.hh"

#include "IMB_colormanagement.hh"

/* -------------------------------------------------------------------- */
/** \name Paste API based on 'partial' blend-files.
 * \{ */

/* Common helper for paste functions. */
static void copybuffer_append(BlendfileLinkAppendContext *lapp_context,
                              Main *bmain,
                              ReportList *reports)
{
  /* Tag existing IDs in given `bmain_dst` as already existing. */
  BKE_main_id_tag_all(bmain, ID_TAG_PRE_EXISTING, true);

  BKE_blendfile_link_append_context_init_done(lapp_context);

  BKE_blendfile_link(lapp_context, reports);

  /* Mark all library linked objects to be updated. */
  BKE_main_lib_objects_recalc_all(bmain);
  IMB_colormanagement_check_file_config(bmain);

  /* Append, rather than linking */
  BKE_blendfile_append(lapp_context, reports);

  /* Instantiate loose data in the scene (e.g. add object to the active collection). */
  BKE_blendfile_link_append_instantiate_loose(lapp_context, reports);

  BKE_blendfile_link_append_context_finalize(lapp_context);

  /* This must be unset, otherwise these object won't link into other scenes from this blend
   * file. */
  BKE_main_id_tag_all(bmain, ID_TAG_PRE_EXISTING, false);

  /* Recreate dependency graph to include new objects. */
  DEG_relations_tag_update(bmain);
}

bool BKE_copybuffer_read(Main *bmain_dst,
                         const char *libname,
                         ReportList *reports,
                         const uint64_t id_types_mask)
{
  /* NOTE: No recursive append here (no `BLO_LIBLINK_APPEND_RECURSIVE`), external linked data
   * should remain linked. */
  const int flag = 0;
  const int id_tag_extra = 0;
  LibraryLink_Params liblink_params;
  BLO_library_link_params_init(&liblink_params, bmain_dst, flag, id_tag_extra);

  BlendfileLinkAppendContext *lapp_context = BKE_blendfile_link_append_context_new(
      &liblink_params);
  BKE_blendfile_link_append_context_library_add(lapp_context, libname, nullptr);

  const int num_pasted = BKE_blendfile_link_append_context_item_idtypes_from_library_add(
      lapp_context, reports, id_types_mask, 0);
  if (num_pasted == BLENDFILE_LINK_APPEND_INVALID) {
    BKE_blendfile_link_append_context_free(lapp_context);
    return false;
  }

  copybuffer_append(lapp_context, bmain_dst, reports);

  BKE_blendfile_link_append_context_free(lapp_context);
  return true;
}

int BKE_copybuffer_paste(bContext *C,
                         const char *libname,
                         const int flag,
                         ReportList *reports,
                         const uint64_t id_types_mask)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C); /* may be nullptr. */
  const int id_tag_extra = 0;

  /* NOTE: No recursive append here, external linked data should remain linked. */
  BLI_assert((flag & BLO_LIBLINK_APPEND_RECURSIVE) == 0);

  LibraryLink_Params liblink_params;
  BLO_library_link_params_init_with_context(
      &liblink_params, bmain, flag, id_tag_extra, scene, view_layer, v3d);

  BlendfileLinkAppendContext *lapp_context = BKE_blendfile_link_append_context_new(
      &liblink_params);
  BKE_blendfile_link_append_context_library_add(lapp_context, libname, nullptr);

  const int num_pasted = BKE_blendfile_link_append_context_item_idtypes_from_library_add(
      lapp_context, reports, id_types_mask, 0);
  if (num_pasted == BLENDFILE_LINK_APPEND_INVALID) {
    BKE_blendfile_link_append_context_free(lapp_context);
    return 0;
  }

  BKE_view_layer_base_deselect_all(scene, view_layer);

  copybuffer_append(lapp_context, bmain, reports);

  BKE_blendfile_link_append_context_free(lapp_context);
  return num_pasted;
}

/** \} */
