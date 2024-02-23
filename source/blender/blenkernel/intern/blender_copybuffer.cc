/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Used for copy/paste operator, (using a temporary file).
 */

#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLI_utildefines.h"

#include "BKE_blender_copybuffer.hh" /* own include */
#include "BKE_blendfile.hh"
#include "BKE_blendfile_link_append.hh"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"

#include "DEG_depsgraph_build.hh"

#include "BLO_readfile.hh"
#include "BLO_writefile.hh"

#include "IMB_colormanagement.hh"

/* -------------------------------------------------------------------- */
/** \name Copy/Paste `.blend`, partial saves.
 * \{ */

void BKE_copybuffer_copy_begin(Main *bmain_src)
{
  BKE_blendfile_write_partial_begin(bmain_src);
}

void BKE_copybuffer_copy_tag_ID(ID *id)
{
  BKE_blendfile_write_partial_tag_ID(id, true);
}

bool BKE_copybuffer_copy_end(Main *bmain_src, const char *filename, ReportList *reports)
{
  const int write_flags = 0;
  const eBLO_WritePathRemap remap_mode = BLO_WRITE_PATH_REMAP_RELATIVE;

  bool retval = BKE_blendfile_write_partial(bmain_src, filename, write_flags, remap_mode, reports);

  BKE_blendfile_write_partial_end(bmain_src);

  return retval;
}

/* Common helper for paste functions. */
static void copybuffer_append(BlendfileLinkAppendContext *lapp_context,
                              Main *bmain,
                              ReportList *reports)
{
  /* Tag existing IDs in given `bmain_dst` as already existing. */
  BKE_main_id_tag_all(bmain, LIB_TAG_PRE_EXISTING, true);

  BKE_blendfile_link(lapp_context, reports);

  /* Mark all library linked objects to be updated. */
  BKE_main_lib_objects_recalc_all(bmain);
  IMB_colormanagement_check_file_config(bmain);

  /* Append, rather than linking */
  BKE_blendfile_append(lapp_context, reports);

  /* This must be unset, otherwise these object won't link into other scenes from this blend
   * file. */
  BKE_main_id_tag_all(bmain, LIB_TAG_PRE_EXISTING, false);

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
