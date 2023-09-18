/* SPDX-FileCopyrightText: 2015 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include "DNA_packedFile_types.h"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

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
    BKE_packedfile_unpack_sound(bmain, reports, sound, ePF_FileStatus(method));
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
