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
 * The Original Code is Copyright (C) 2015 by the Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup RNA
 */

#include "DNA_packedFile_types.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "BKE_packedFile.h"

#include "rna_internal.h"

#ifdef RNA_RUNTIME

static void rna_Sound_pack(bSound *sound, Main *bmain, ReportList *reports)
{
  sound->packedfile = BKE_packedfile_new(
      reports, sound->filepath, ID_BLEND_PATH(bmain, &sound->id));
}

static void rna_Sound_unpack(bSound *sound, Main *bmain, ReportList *reports, int method)
{
  if (!sound->packedfile) {
    BKE_report(reports, RPT_ERROR, "Sound not packed");
  }
  else {
    /* reports its own error on failure */
    BKE_packedfile_unpack_sound(bmain, reports, sound, method);
  }
}

#else

void RNA_api_sound(StructRNA *srna)
{
  FunctionRNA *func;

  func = RNA_def_function(srna, "pack", "rna_Sound_pack");
  RNA_def_function_ui_description(func, "Pack the sound into the current blend file");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_MAIN);

  func = RNA_def_function(srna, "unpack", "rna_Sound_unpack");
  RNA_def_function_ui_description(func, "Unpack the sound to the samples filename");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_MAIN);
  RNA_def_enum(
      func, "method", rna_enum_unpack_method_items, PF_USE_LOCAL, "method", "How to unpack");
}

#endif
