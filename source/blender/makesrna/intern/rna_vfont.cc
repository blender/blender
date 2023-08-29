/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "RNA_define.hh"

#include "rna_internal.h"

#include "DNA_vfont_types.h"

#include "WM_types.hh"

#ifdef RNA_RUNTIME

#  include "BKE_vfont.h"
#  include "DNA_object_types.h"

#  include "DEG_depsgraph.h"

#  include "WM_api.hh"

/* Matching function in rna_ID.cc */
static int rna_VectorFont_filepath_editable(PointerRNA *ptr, const char ** /*r_info*/)
{
  VFont *vfont = (VFont *)ptr->owner_id;
  if (BKE_vfont_is_builtin(vfont)) {
    return 0;
  }
  return PROP_EDITABLE;
}

static void rna_VectorFont_reload_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  VFont *vf = (VFont *)ptr->owner_id;
  BKE_vfont_free_data(vf);

  /* update */
  WM_main_add_notifier(NC_GEOM | ND_DATA, nullptr);
  DEG_id_tag_update(&vf->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
}

#else

void RNA_def_vfont(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "VectorFont", "ID");
  RNA_def_struct_ui_text(srna, "Vector Font", "Vector font for Text objects");
  RNA_def_struct_sdna(srna, "VFont");
  RNA_def_struct_ui_icon(srna, ICON_FILE_FONT);

  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_string_sdna(prop, nullptr, "filepath");
  RNA_def_property_editable_func(prop, "rna_VectorFont_filepath_editable");
  RNA_def_property_ui_text(prop, "File Path", "");
  RNA_def_property_update(prop, NC_GEOM | ND_DATA, "rna_VectorFont_reload_update");

  prop = RNA_def_property(srna, "packed_file", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "packedfile");
  RNA_def_property_ui_text(prop, "Packed File", "");

  RNA_api_vfont(srna);
}

#endif
