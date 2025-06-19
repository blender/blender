/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */
#include "BLO_readfile.hh"

#include "MEM_guardedalloc.h"

#include "BLI_string.h"

#include "BKE_main.hh"

#include "DNA_ID.h"

TempLibraryContext *BLO_library_temp_load_id(Main *real_main,
                                             const char *blend_file_path,
                                             const short idcode,
                                             const char *idname,
                                             ReportList *reports)
{
  TempLibraryContext *temp_lib_ctx = MEM_callocN<TempLibraryContext>(__func__);
  temp_lib_ctx->bmain_base = BKE_main_new();
  temp_lib_ctx->bf_reports.reports = reports;

  /* Copy the file path so any path remapping is performed properly. */
  STRNCPY(temp_lib_ctx->bmain_base->filepath, real_main->filepath);

  BlendHandle *blendhandle = BLO_blendhandle_from_file(blend_file_path, &temp_lib_ctx->bf_reports);

  LibraryLink_Params lib_link_params;
  BLO_library_link_params_init(&lib_link_params, temp_lib_ctx->bmain_base, 0, ID_TAG_TEMP_MAIN);

  Main *bmain_lib = BLO_library_link_begin(&blendhandle, blend_file_path, &lib_link_params);

  temp_lib_ctx->temp_id = BLO_library_link_named_part(
      bmain_lib, &blendhandle, idcode, idname, &lib_link_params);

  BLO_library_link_end(bmain_lib, &blendhandle, &lib_link_params, reports);
  BLO_blendhandle_close(blendhandle);

  return temp_lib_ctx;
}

void BLO_library_temp_free(TempLibraryContext *temp_lib_ctx)
{
  BKE_main_free(temp_lib_ctx->bmain_base);
  MEM_freeN(temp_lib_ctx);
}
