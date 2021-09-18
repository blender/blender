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

float rna_BrushChannel_get_value(PointerRNA *rna)
{
  BrushChannel *ch = rna->data;

  return ch->fvalue;
}
void rna_BrushChannel_set_value(PointerRNA *rna, float value)
{
  BrushChannel *ch = rna->data;

  ch->fvalue = value;
}

void rna_BrushChannel_value_range(
    PointerRNA *rna, float *min, float *max, float *soft_min, float *soft_max)
{
  BrushChannel *ch = rna->data;

  if (ch->def) {
    *min = ch->def->min;
    *max = ch->def->max;
    *soft_min = ch->def->soft_min;
    *soft_max = ch->def->soft_max;
  }
  else {
    *min = 0.0f;
    *max = 1.0f;
    *soft_min = 0.0f;
    *soft_max = 1.0f;
  }
}
#endif

extern BrushChannelType *brush_builtin_channels;
extern const int builtin_channel_len;

EnumPropertyItem channel_types[] = {{BRUSH_CHANNEL_FLOAT, "FLOAT", ICON_NONE, "Float"},
                                    {BRUSH_CHANNEL_INT, "INT", ICON_NONE, "Int"},
                                    {BRUSH_CHANNEL_ENUM, "ENUM", ICON_NONE, "Enum"},
                                    {BRUSH_CHANNEL_BITMASK, "BITMASK", ICON_NONE, "Bitmask"},
                                    {-1, NULL, -1, NULL}};

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

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, "BrushChannel", "name");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE | PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Name", "Channel name");

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, "BrushChannel", "type");
  RNA_def_property_enum_items(prop, channel_types);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE | PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Type", "Value Type");

  prop = RNA_def_property(srna, "value", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, "BrushChannel", "fvalue");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Value", "Current value");
  RNA_def_property_float_funcs(prop,
                               "rna_BrushChannel_get_value",
                               "rna_BrushChannel_set_value",
                               "rna_BrushChannel_value_range");

  prop = RNA_def_property(srna, "inherit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, "BrushChannel", "flag", BRUSH_CHANNEL_INHERIT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Inherit", "Inherit from scene defaults");

  prop = RNA_def_property(srna, "inherit_if_unset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, "BrushChannel", "flag", BRUSH_CHANNEL_INHERIT_IF_UNSET);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Inherit If Unset", "Combine with default settings");
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
