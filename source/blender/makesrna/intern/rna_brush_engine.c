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

#include "DNA_brush_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_workspace_types.h"

#include "BLI_math.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "IMB_imbuf.h"

#include "BKE_brush_engine.h"
#include "DNA_sculpt_brush_types.h"
#include "WM_types.h"

#ifdef RNA_RUNTIME

int rna_BrushChannelSet_channels_begin(CollectionPropertyIterator *iter, struct PointerRNA *ptr)
{
  BrushChannelSet *chset = ptr->data;

  rna_iterator_array_begin(
      iter, chset->channels, sizeof(BrushChannel), chset->totchannel, false, NULL);

  return 1;
}

int rna_BrushChannelSet_channels_assignint(struct PointerRNA *ptr,
                                           int key,
                                           const struct PointerRNA *assign_ptr)
{
  BrushChannelSet *chset = ptr->data;
  BrushChannel *ch = chset->channels + key;
  BrushChannel *src = assign_ptr->data;

  BKE_brush_channel_copy(ch, src);

  return 1;
}

#endif

void RNA_def_brush_channel(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "BrushChannel", NULL);
  RNA_def_struct_sdna(srna, "BrushChannel");
  RNA_def_struct_ui_text(srna, "Brush Channel", "Brush Channel");

  prop = RNA_def_property(srna, "idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, "BrushChannel", "idname");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE | PROP_ANIMATABLE);

  RNA_def_struct_name_property(srna, prop);
}

void RNA_def_brush_channelset(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "BrushChannelSet", NULL);
  RNA_def_struct_sdna(srna, "BrushChannelSet");
  RNA_def_struct_ui_text(srna, "Channel Set", "Brush Channel Collection");

  prop = RNA_def_property(srna, "channels", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "channels", "totchannel");
  RNA_def_property_collection_funcs(prop,
                                    "rna_BrushChannelSet_channels_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    NULL,
                                    NULL,
                                    NULL,
                                    "rna_BrushChannelSet_channels_assignint");
  RNA_def_property_struct_type(prop, "BrushChannel");
  RNA_def_property_override_flag(
      prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY | PROPOVERRIDE_LIBRARY_INSERTION);
}

void RNA_def_brush_engine(BlenderRNA *brna)
{
  RNA_def_brush_channel(brna);
  RNA_def_brush_channelset(brna);
}
