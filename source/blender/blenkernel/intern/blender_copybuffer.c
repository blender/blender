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
 */

/** \file
 * \ingroup bke
 *
 * Used for copy/paste operator, (using a temporary file).
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "IMB_imbuf.h"
#include "IMB_moviecache.h"

#include "BKE_blender_copybuffer.h" /* own include */
#include "BKE_blendfile.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "BLO_readfile.h"
#include "BLO_writefile.h"

#include "IMB_colormanagement.h"

/* -------------------------------------------------------------------- */
/** \name Copy/Paste `.blend`, partial saves.
 * \{ */

void BKE_copybuffer_begin(Main *bmain_src)
{
  BKE_blendfile_write_partial_begin(bmain_src);
}

void BKE_copybuffer_tag_ID(ID *id)
{
  BKE_blendfile_write_partial_tag_ID(id, true);
}

/**
 * \return Success.
 */
bool BKE_copybuffer_save(Main *bmain_src, const char *filename, ReportList *reports)
{
  const int write_flags = 0;
  const eBLO_WritePathRemap remap_mode = BLO_WRITE_PATH_REMAP_RELATIVE;

  bool retval = BKE_blendfile_write_partial(bmain_src, filename, write_flags, remap_mode, reports);

  BKE_blendfile_write_partial_end(bmain_src);

  return retval;
}

bool BKE_copybuffer_read(Main *bmain_dst,
                         const char *libname,
                         ReportList *reports,
                         const uint64_t id_types_mask)
{
  BlendHandle *bh = BLO_blendhandle_from_file(libname, reports);
  if (bh == NULL) {
    /* Error reports will have been made by BLO_blendhandle_from_file(). */
    return false;
  }
  /* Here appending/linking starts. */
  const int flag = 0;
  const int id_tag_extra = 0;
  struct LibraryLink_Params liblink_params;
  BLO_library_link_params_init(&liblink_params, bmain_dst, flag, id_tag_extra);
  Main *mainl = BLO_library_link_begin(&bh, libname, &liblink_params);
  BLO_library_link_copypaste(mainl, bh, id_types_mask);
  BLO_library_link_end(mainl, &bh, &liblink_params);
  /* Mark all library linked objects to be updated. */
  BKE_main_lib_objects_recalc_all(bmain_dst);
  IMB_colormanagement_check_file_config(bmain_dst);
  /* Append, rather than linking. */
  Library *lib = BLI_findstring(&bmain_dst->libraries, libname, offsetof(Library, filepath_abs));
  BKE_library_make_local(bmain_dst, lib, NULL, true, false);
  /* Important we unset, otherwise these object wont
   * link into other scenes from this blend file.
   */
  BKE_main_id_tag_all(bmain_dst, LIB_TAG_PRE_EXISTING, false);
  BLO_blendhandle_close(bh);
  return true;
}

/**
 * \return Number of IDs directly pasted from the buffer
 * (does not includes indirectly pulled out ones).
 */
int BKE_copybuffer_paste(bContext *C,
                         const char *libname,
                         const short flag,
                         ReportList *reports,
                         const uint64_t id_types_mask)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C); /* may be NULL. */
  Main *mainl = NULL;
  Library *lib;
  BlendHandle *bh;
  const int id_tag_extra = 0;

  bh = BLO_blendhandle_from_file(libname, reports);

  if (bh == NULL) {
    /* error reports will have been made by BLO_blendhandle_from_file() */
    return 0;
  }

  BKE_view_layer_base_deselect_all(view_layer);

  /* tag everything, all untagged data can be made local
   * its also generally useful to know what is new
   *
   * take extra care BKE_main_id_flag_all(bmain, LIB_TAG_PRE_EXISTING, false) is called after! */
  BKE_main_id_tag_all(bmain, LIB_TAG_PRE_EXISTING, true);

  /* here appending/linking starts */
  struct LibraryLink_Params liblink_params;
  BLO_library_link_params_init_with_context(
      &liblink_params, bmain, flag, id_tag_extra, scene, view_layer, v3d);
  mainl = BLO_library_link_begin(&bh, libname, &liblink_params);

  const int num_pasted = BLO_library_link_copypaste(mainl, bh, id_types_mask);

  BLO_library_link_end(mainl, &bh, &liblink_params);

  /* mark all library linked objects to be updated */
  BKE_main_lib_objects_recalc_all(bmain);
  IMB_colormanagement_check_file_config(bmain);

  /* append, rather than linking */
  lib = BLI_findstring(&bmain->libraries, libname, offsetof(Library, filepath_abs));
  BKE_library_make_local(bmain, lib, NULL, true, false);

  /* important we unset, otherwise these object wont
   * link into other scenes from this blend file */
  BKE_main_id_tag_all(bmain, LIB_TAG_PRE_EXISTING, false);

  /* recreate dependency graph to include new objects */
  DEG_relations_tag_update(bmain);

  /* Tag update the scene to flush base collection settings, since the new object is added to a
   * new (active) collection, not its original collection, thus need recalculation. */
  DEG_id_tag_update(&scene->id, 0);

  BLO_blendhandle_close(bh);
  /* remove library... */

  return num_pasted;
}

/** \} */
