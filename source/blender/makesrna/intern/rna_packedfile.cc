/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "BLI_utildefines.h"

#include "DNA_packedFile_types.h"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "BKE_packedFile.h"

#include "rna_internal.h"

const EnumPropertyItem rna_enum_unpack_method_items[] = {
    {PF_REMOVE, "REMOVE", 0, "Remove Pack", ""},
    {PF_USE_LOCAL, "USE_LOCAL", 0, "Use Local File", ""},
    {PF_WRITE_LOCAL, "WRITE_LOCAL", 0, "Write Local File (overwrite existing)", ""},
    {PF_USE_ORIGINAL, "USE_ORIGINAL", 0, "Use Original File", ""},
    {PF_WRITE_ORIGINAL, "WRITE_ORIGINAL", 0, "Write Original File (overwrite existing)", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME

static void rna_PackedImage_data_get(PointerRNA *ptr, char *value)
{
  PackedFile *pf = (PackedFile *)ptr->data;
  memcpy(value, pf->data, size_t(pf->size));
  value[pf->size] = '\0';
}

static int rna_PackedImage_data_len(PointerRNA *ptr)
{
  PackedFile *pf = (PackedFile *)ptr->data;
  return pf->size; /* No need to include trailing nullptr char here! */
}

#else

void RNA_def_packedfile(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "PackedFile", nullptr);
  RNA_def_struct_ui_text(srna, "Packed File", "External file packed into the .blend file");

  prop = RNA_def_property(srna, "size", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Size", "Size of packed file in bytes");

  prop = RNA_def_property(srna, "data", PROP_STRING, PROP_BYTESTRING);
  RNA_def_property_string_funcs(
      prop, "rna_PackedImage_data_get", "rna_PackedImage_data_len", nullptr);
  RNA_def_property_ui_text(prop, "Data", "Raw data (bytes, exact content of the embedded file)");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
}

#endif
