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
 * \ingroup blenloader
 */
#include "BLO_readfile.h"

#include "MEM_guardedalloc.h"

#include "BKE_report.h"

#include "DNA_ID.h"

TempLibraryContext *BLO_library_temp_load_id(struct Main *real_main,
                                             const char *blend_file_path,
                                             const short idcode,
                                             const char *idname,
                                             struct ReportList *reports)
{
  TempLibraryContext *temp_lib_ctx = MEM_callocN(sizeof(*temp_lib_ctx), __func__);

  temp_lib_ctx->blendhandle = BLO_blendhandle_from_file(blend_file_path, reports);

  BLO_library_link_params_init(&temp_lib_ctx->liblink_params, real_main, 0, LIB_TAG_TEMP_MAIN);

  temp_lib_ctx->temp_main = BLO_library_link_begin(
      &temp_lib_ctx->blendhandle, blend_file_path, &temp_lib_ctx->liblink_params);

  temp_lib_ctx->temp_id = BLO_library_link_named_part(temp_lib_ctx->temp_main,
                                                      &temp_lib_ctx->blendhandle,
                                                      idcode,
                                                      idname,
                                                      &temp_lib_ctx->liblink_params);

  return temp_lib_ctx;
}

void BLO_library_temp_free(TempLibraryContext *temp_lib_ctx)
{
  BLO_library_link_end(
      temp_lib_ctx->temp_main, &temp_lib_ctx->blendhandle, &temp_lib_ctx->liblink_params);
  BLO_blendhandle_close(temp_lib_ctx->blendhandle);
  MEM_freeN(temp_lib_ctx);
}
