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
 * \ingroup RNA
 */

#include <stdlib.h>

#include "RNA_define.h"

#include "rna_internal.h"

#include "DNA_vfont_types.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

#  include "BKE_font.h"
#  include "DNA_object_types.h"

#  include "DEG_depsgraph.h"

#  include "WM_api.h"

/* matching fnction in rna_ID.c */
static int rna_VectorFont_filepath_editable(PointerRNA *ptr, const char **UNUSED(r_info))
{
  VFont *vfont = (VFont *)ptr->owner_id;
  if (BKE_vfont_is_builtin(vfont)) {
    return 0;
  }
  return PROP_EDITABLE;
}

static void rna_VectorFont_reload_update(Main *UNUSED(bmain),
                                         Scene *UNUSED(scene),
                                         PointerRNA *ptr)
{
  VFont *vf = (VFont *)ptr->owner_id;
  BKE_vfont_free_data(vf);

  /* update */
  WM_main_add_notifier(NC_GEOM | ND_DATA, NULL);
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
  RNA_def_property_string_sdna(prop, NULL, "filepath");
  RNA_def_property_editable_func(prop, "rna_VectorFont_filepath_editable");
  RNA_def_property_ui_text(prop, "File Path", "");
  RNA_def_property_update(prop, NC_GEOM | ND_DATA, "rna_VectorFont_reload_update");

  prop = RNA_def_property(srna, "packed_file", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "packedfile");
  RNA_def_property_ui_text(prop, "Packed File", "");

  RNA_api_vfont(srna);
}

#endif
