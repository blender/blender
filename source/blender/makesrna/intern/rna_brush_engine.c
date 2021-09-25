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

#include "DNA_ID_enums.h"
#include "DNA_brush_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_listBase.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_workspace_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "IMB_imbuf.h"

#include "BKE_brush.h"
#include "BKE_brush_engine.h"
#include "BKE_colortools.h"
#include "DNA_sculpt_brush_types.h"

#include "WM_types.h"

static EnumPropertyItem null_enum[2] = {{0, "null", ICON_NONE, "null"}, {0, NULL, 0, NULL, NULL}};

#ifdef RNA_RUNTIME

#  include "RNA_access.h"

BrushChannelSet *rna_BrushChannelSet_get_set(struct PointerRNA *ptr)
{
  BrushChannelSet *chset = NULL;
  ID *id = ptr->owner_id;

  switch (GS(id->name)) {
    case ID_BR:
      chset = ((Brush *)id)->channels;
      break;
    case ID_SCE: {
      Scene *scene = (Scene *)id;

      if (!scene->toolsettings || !scene->toolsettings->sculpt) {
        return NULL;
      }

      chset = scene->toolsettings->sculpt->channels;
      break;
    }
  }

  return chset;
}

ATTR_NO_OPT int rna_BrushChannelSet_channels_begin(CollectionPropertyIterator *iter,
                                                   struct PointerRNA *ptr)
{
  BrushChannelSet *chset = rna_BrushChannelSet_get_set(ptr);

  if (!chset) {
    return 0;
  }

  rna_iterator_listbase_begin(iter, &chset->channels, NULL);

  return 1;
}

int rna_BrushChannelSet_channels_assignint(struct PointerRNA *ptr,
                                           int key,
                                           const struct PointerRNA *assign_ptr)
{
  BrushChannelSet *chset = rna_BrushChannelSet_get_set(ptr);

  BrushChannel *src = assign_ptr->data;
  BrushChannel *ch = BLI_findlink(&chset->channels, key);

  if (ch) {
    BKE_brush_channel_copy_data(ch, src, false);
  }

  return 1;
}

float rna_BrushChannel_get_value(PointerRNA *rna)
{
  BrushChannel *ch = (BrushChannel *)rna->data;

  return ch->fvalue;
}

static BrushChannel *get_paired_radius_channel(PointerRNA *rna)
{
  BrushChannel *ch = rna->data;

  bool is_radius = STREQ(ch->idname, "radius");
  bool is_unproj = STREQ(ch->idname, "unprojected_radius");

  /*
  the way the old brush system split view and scene
  radii but presented them as the same to the user,
  and also to parts of the API is proving difficult
  to disentangle. . . - joeedh
  */
  if ((is_radius || is_unproj) && rna->owner_id) {
    BrushChannelSet *chset = NULL;

    if (GS(rna->owner_id->name) == ID_SCE) {
      Scene *scene = ((Scene *)rna->owner_id);

      if (scene->toolsettings && scene->toolsettings->sculpt) {
        chset = scene->toolsettings->sculpt->channels;
      }
    }
    else if (GS(rna->owner_id->name) == ID_BR) {
      chset = ((Brush *)rna->owner_id)->channels;
    }

    if (!chset) {
      return NULL;
    }

    return is_radius ? BRUSHSET_LOOKUP(chset, unprojected_radius) : BRUSHSET_LOOKUP(chset, radius);
  }

  return NULL;
}

void rna_BrushChannel_inherit_set(PointerRNA *rna, bool value)
{
  BrushChannel *ch = (BrushChannel *)rna->data;
  BrushChannel *ch2 = get_paired_radius_channel(rna);

  if (value) {
    ch->flag |= BRUSH_CHANNEL_INHERIT;

    if (ch2) {
      ch2->flag |= BRUSH_CHANNEL_INHERIT;
    }
  }
  else {
    ch->flag &= ~BRUSH_CHANNEL_INHERIT;

    if (ch2) {
      ch2->flag &= ~BRUSH_CHANNEL_INHERIT;
    }
  }
}

bool rna_BrushChannel_inherit_get(PointerRNA *rna)
{
  BrushChannel *ch = (BrushChannel *)rna->data;

  return ch->flag & BRUSH_CHANNEL_INHERIT;
}

void rna_BrushChannel_set_value(PointerRNA *rna, float value)
{
  BrushChannel *ch = rna->data;
  BrushChannel *ch2 = get_paired_radius_channel(rna);

  if (ch2 && value != 0.0f && ch->fvalue != 0.0f) {
    float ratio = value / ch->fvalue;
    ch2->fvalue *= ratio;
  }

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

int rna_BrushChannel_get_ivalue(PointerRNA *rna)
{
  BrushChannel *ch = rna->data;

  return ch->ivalue;
}
void rna_BrushChannel_set_ivalue(PointerRNA *rna, int value)
{
  BrushChannel *ch = rna->data;

  ch->ivalue = value;
}

void rna_BrushChannel_ivalue_range(
    PointerRNA *rna, int *min, int *max, int *soft_min, int *soft_max)
{
  BrushChannel *ch = rna->data;

  if (ch->def) {
    *min = (int)ch->def->min;
    *max = (int)ch->def->max;
    *soft_min = (int)ch->def->soft_min;
    *soft_max = (int)ch->def->soft_max;
  }
  else {
    *min = 0;
    *max = 65535;
    *soft_min = 0;
    *soft_max = 1024;
  }
}

PointerRNA rna_BrushMapping_curve_get(PointerRNA *ptr)
{
  BrushMapping *mapping = (BrushMapping *)ptr->data;

  // make sure we can write to curve
  BKE_brush_mapping_ensure_write(mapping);

  return rna_pointer_inherit_refine(ptr, &RNA_CurveMapping, mapping->curve);
}

PointerRNA rna_BrushCurve_curve_get(PointerRNA *ptr)
{
  BrushCurve *curve = (BrushCurve *)ptr->data;

  // make sure we can write to curve
  BKE_brush_channel_curve_ensure_write(curve);

  return rna_pointer_inherit_refine(ptr, &RNA_CurveMapping, curve->curve);
}

int rna_BrushChannel_mappings_begin(CollectionPropertyIterator *iter, struct PointerRNA *ptr)
{
  BrushChannel *ch = ptr->data;

  rna_iterator_array_begin(
      iter, ch->mappings, sizeof(BrushMapping), BRUSH_MAPPING_MAX, false, NULL);

  return 1;
}

int rna_BrushChannel_mappings_assignint(struct PointerRNA *ptr,
                                        int key,
                                        const struct PointerRNA *assign_ptr)
{
  BrushChannel *ch = ptr->data;
  BrushMapping *dst = ch->mappings + key;
  BrushMapping *src = assign_ptr->data;

  BKE_brush_mapping_copy_data(dst, src);

  return 1;
}

int rna_BrushChannel_mappings_lookupstring(PointerRNA *rna, const char *key, PointerRNA *r_ptr)
{
  BrushChannel *ch = (BrushChannel *)rna->data;

  for (int i = 0; i < BRUSH_MAPPING_MAX; i++) {
    if (STREQ(key, BKE_brush_mapping_type_to_typename(i))) {
      if (r_ptr) {
        *r_ptr = rna_pointer_inherit_refine(rna, &RNA_BrushMapping, ch->mappings + i);
      }

      return 1;
    }
  }

  return 0;
}

int rna_BrushChannel_mappings_length(PointerRNA *ptr)
{
  return BRUSH_MAPPING_MAX;
}

int rna_BrushChannel_enum_value_get(PointerRNA *ptr)
{
  BrushChannel *ch = (BrushChannel *)ptr->data;

  return ch->ivalue;
}

int rna_BrushChannel_enum_value_set(PointerRNA *ptr, int val)
{
  BrushChannel *ch = (BrushChannel *)ptr->data;

  ch->ivalue = val;

  return 1;
}

int lookup_icon_id(const char *icon)
{
  const EnumPropertyItem *item = rna_enum_icon_items;
  int i = 0;

  while (item->identifier) {
    if (STREQ(item->identifier, icon)) {
      return i;
    }

    item++;
    i++;
  }

  return ICON_NONE;
}

const EnumPropertyItem *rna_BrushChannel_enum_value_get_items(struct bContext *C,
                                                              PointerRNA *ptr,
                                                              PropertyRNA *prop,
                                                              bool *r_free)
{
  BrushChannel *ch = (BrushChannel *)ptr->data;

  if (!ch->def || !ELEM(ch->type, BRUSH_CHANNEL_ENUM, BRUSH_CHANNEL_BITMASK)) {
    return null_enum;
  }

  BKE_brush_channeltype_rna_check(ch->def, lookup_icon_id);

  return ch->def->rna_enumdef;
}

static int rna_enum_check_separator(CollectionPropertyIterator *UNUSED(iter), void *data)
{
  EnumPropertyItem *item = (EnumPropertyItem *)data;

  return (item->identifier[0] == 0);
}

static void rna_BrushChannel_enum_items_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  /* EnumPropertyRNA *eprop; */ /* UNUSED */
  const EnumPropertyItem *item = NULL;
  int totitem;

  BrushChannel *ch = (BrushChannel *)ptr->data;

  if (!ch->def || !ELEM(ch->type, BRUSH_CHANNEL_ENUM, BRUSH_CHANNEL_BITMASK)) {
    if (!ch->def) {
      printf("%s: channel '%s' had no definition\n", __func__, ch->idname);
    }
    else {
      // printf("%s: channel '%s' is not an enum/bitmask\n", __func__, ch->idname);
    }

    item = null_enum;

    rna_iterator_array_begin(
        iter, (void *)item, sizeof(EnumPropertyItem), 0, false, rna_enum_check_separator);
    return;
  }

  BKE_brush_channeltype_rna_check(ch->def, lookup_icon_id);

  totitem = 0;
  while (ch->def->rna_enumdef[totitem].name) {
    totitem++;
  }

  item = ch->def->rna_enumdef;

  rna_iterator_array_begin(
      iter, (void *)item, sizeof(EnumPropertyItem), totitem, false, rna_enum_check_separator);
}

char *rna_BrushChannel_rnapath(PointerRNA *ptr)
{
  BrushChannel *ch = (BrushChannel *)ptr->data;
  char buf[512];

  if (!ptr->owner_id) {
    return NULL;
  }

  if (GS(ptr->owner_id->name) == ID_BR) {
    return BLI_sprintfN("channels[\"%s\"]", ch->idname);
  }
  else if (GS(ptr->owner_id->name) == ID_SCE) {
    return BLI_sprintfN("tool_settings.sculpt.channels[\"%s\"]", ch->idname);
  }
  else {
    return NULL;
  }
}

ATTR_NO_OPT void rna_BrushChannelSet_ensure(ID *id, BrushChannel *channel)
{
  PointerRNA ptr;

  ptr.owner_id = id;
  ptr.data = NULL;
  ptr.type = NULL;

  BrushChannelSet *chset = rna_BrushChannelSet_get_set(&ptr);
  if (chset) {
    BKE_brush_channelset_ensure_existing(chset, channel);
  }
}

ATTR_NO_OPT int rna_BrushChannelSet_length(PointerRNA *ptr)
{
  BrushChannelSet *chset = rna_BrushChannelSet_get_set(ptr);
  // BrushChannelSet *chset = (BrushChannelSet *)ptr->data;

  return chset->totchannel;
}

#endif

static EnumPropertyItem mapping_type_items[] = {
    {BRUSH_MAPPING_PRESSURE, "PRESSURE", ICON_NONE, "Pressure"},
    {BRUSH_MAPPING_XTILT, "XTILT", ICON_NONE, "X Tilt"},
    {BRUSH_MAPPING_YTILT, "YTILT", ICON_NONE, "Y Tilt"},
    {BRUSH_MAPPING_ANGLE, "ANGLE", ICON_NONE, "Angle"},
    {BRUSH_MAPPING_SPEED, "SPEED", ICON_NONE, "Speed"},
    {0, NULL, 0, NULL, NULL},
};

void RNA_def_brush_mapping(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "BrushMapping", NULL);
  RNA_def_struct_sdna(srna, "BrushMapping");
  RNA_def_struct_ui_text(srna, "Brush Mapping", "Brush Mapping");

  prop = RNA_def_property(srna, "inherit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, "BrushMapping", "flag", BRUSH_MAPPING_INHERIT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(
      prop,
      "Inherit",
      "Inherit from parent channel even if owning channel is set not to inherit.");

  prop = RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Curve Sensitivity", "Curve used for the sensitivity");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_pointer_funcs(prop, "rna_BrushMapping_curve_get", NULL, NULL, NULL);

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, "BrushMapping", "type");
  RNA_def_property_enum_items(prop, mapping_type_items);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE | PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Type", "Channel Type");

  prop = RNA_def_property(srna, "enabled", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, "BrushMapping", "flag", BRUSH_MAPPING_ENABLED);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Enabled", "Input Mapping Is Enabled");

  prop = RNA_def_property(srna, "ui_expanded", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, "BrushMapping", "flag", BRUSH_MAPPING_UI_EXPANDED);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Expanded", "View advanced properties");
}

extern BrushChannelType *brush_builtin_channels;
extern const int builtin_channel_len;

EnumPropertyItem channel_types[] = {{BRUSH_CHANNEL_FLOAT, "FLOAT", ICON_NONE, "Float"},
                                    {BRUSH_CHANNEL_INT, "INT", ICON_NONE, "Int"},
                                    {BRUSH_CHANNEL_ENUM, "ENUM", ICON_NONE, "Enum"},
                                    {BRUSH_CHANNEL_BITMASK, "BITMASK", ICON_NONE, "Bitmask"},
                                    {BRUSH_CHANNEL_BOOL, "BOOL", ICON_NONE, "Boolean"},
                                    {BRUSH_CHANNEL_VEC3, "VEC3", ICON_NONE, "Color3"},
                                    {BRUSH_CHANNEL_VEC4, "VEC4", ICON_NONE, "Color4"},
                                    {BRUSH_CHANNEL_CURVE, "CURVE", ICON_NONE, "Curve"},
                                    {0, NULL, 0, NULL, NULL}};

// getting weird link errors here
// extern const EnumPropertyItem brush_curve_preset_items[];

static const EnumPropertyItem brush_curve_preset_items[] = {
    {BRUSH_CURVE_CUSTOM, "CUSTOM", ICON_RNDCURVE, "Custom", ""},
    {BRUSH_CURVE_SMOOTH, "SMOOTH", ICON_SMOOTHCURVE, "Smooth", ""},
    {BRUSH_CURVE_SMOOTHER, "SMOOTHER", ICON_SMOOTHCURVE, "Smoother", ""},
    {BRUSH_CURVE_SPHERE, "SPHERE", ICON_SPHERECURVE, "Sphere", ""},
    {BRUSH_CURVE_ROOT, "ROOT", ICON_ROOTCURVE, "Root", ""},
    {BRUSH_CURVE_SHARP, "SHARP", ICON_SHARPCURVE, "Sharp", ""},
    {BRUSH_CURVE_LIN, "LIN", ICON_LINCURVE, "Linear", ""},
    {BRUSH_CURVE_POW4, "POW4", ICON_SHARPCURVE, "Sharper", ""},
    {BRUSH_CURVE_INVSQUARE, "INVSQUARE", ICON_INVERSESQUARECURVE, "Inverse Square", ""},
    {BRUSH_CURVE_CONSTANT, "CONSTANT", ICON_NOCURVE, "Constant", ""},
    {0, NULL, 0, NULL, NULL},
};

void RNA_def_brush_curve(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "BrushCurve", NULL);
  RNA_def_struct_sdna(srna, "BrushCurve");
  RNA_def_struct_ui_text(srna, "Brush Curve", "Brush Curve");

  prop = RNA_def_property(srna, "curve_preset", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, "BrushCurve", "preset");
  RNA_def_property_enum_items(prop, brush_curve_preset_items);
  RNA_def_property_ui_text(prop, "Curve Preset", "");

  prop = RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Curve Sensitivity", "Curve used for the sensitivity");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_pointer_funcs(prop, "rna_BrushCurve_curve_get", NULL, NULL, NULL);
}

void RNA_def_brush_channel(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "BrushChannel", NULL);
  RNA_def_struct_sdna(srna, "BrushChannel");
  RNA_def_struct_ui_text(srna, "Brush Channel", "Brush Channel");
  RNA_def_struct_path_func(srna, "rna_BrushChannel_rnapath");

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

  prop = RNA_def_property(srna, "bool_value", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, "BrushChannel", "ivalue", 1);
  RNA_def_property_ui_text(prop, "Value", "Current value");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  prop = RNA_def_property(srna, "ui_order", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, "BrushChannel", "ui_order");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Ordering", "Order of brush channel in panels and the header");

  prop = RNA_def_property(srna, "int_value", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, "BrushChannel", "ivalue");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Value", "Current value");
  RNA_def_property_int_funcs(prop,
                             "rna_BrushChannel_get_ivalue",
                             "rna_BrushChannel_set_ivalue",
                             "rna_BrushChannel_ivalue_range");

  prop = RNA_def_property(srna, "float_value", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, "BrushChannel", "fvalue");
  RNA_def_property_ui_text(prop, "Value", "Current value");
  RNA_def_property_float_funcs(prop,
                               "rna_BrushChannel_get_value",
                               "rna_BrushChannel_set_value",
                               "rna_BrushChannel_value_range");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  // XXX hack warning: these next two are duplicates of above
  // to get different subtypes
  prop = RNA_def_property(srna, "factor_value", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, "BrushChannel", "fvalue");
  RNA_def_property_ui_text(prop, "Value", "Current value");
  RNA_def_property_float_funcs(prop,
                               "rna_BrushChannel_get_value",
                               "rna_BrushChannel_set_value",
                               "rna_BrushChannel_value_range");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  prop = RNA_def_property(srna, "percent_value", PROP_FLOAT, PROP_PERCENTAGE);
  RNA_def_property_float_sdna(prop, "BrushChannel", "fvalue");
  RNA_def_property_ui_text(prop, "Value", "Current value");
  RNA_def_property_float_funcs(prop,
                               "rna_BrushChannel_get_value",
                               "rna_BrushChannel_set_value",
                               "rna_BrushChannel_value_range");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  prop = RNA_def_property(srna, "inherit", PROP_BOOLEAN, PROP_NONE);
  // RNA_def_property_boolean_sdna(prop, "BrushChannel", "flag", BRUSH_CHANNEL_INHERIT);
  RNA_def_property_ui_text(prop, "Inherit", "Inherit from scene defaults");
  RNA_def_property_boolean_funcs(
      prop, "rna_BrushChannel_inherit_get", "rna_BrushChannel_inherit_set");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  prop = RNA_def_property(srna, "show_in_header", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, "BrushChannel", "flag", BRUSH_CHANNEL_SHOW_IN_HEADER);
  RNA_def_property_ui_text(prop, "In Header", "Show in header");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  prop = RNA_def_property(srna, "show_in_workspace", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, "BrushChannel", "flag", BRUSH_CHANNEL_SHOW_IN_WORKSPACE);
  RNA_def_property_ui_text(prop, "In Workspace", "Show in workspace");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  prop = RNA_def_property(srna, "is_color", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, "BrushChannel", "flag", BRUSH_CHANNEL_COLOR);
  RNA_def_property_ui_text(prop, "Is Color", "Is this channel a color");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  prop = RNA_def_property(srna, "ui_expanded", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, "BrushChannel", "flag", BRUSH_CHANNEL_UI_EXPANDED);
  RNA_def_property_ui_text(prop, "Expanded", "View advanced properties");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  prop = RNA_def_property(srna, "inherit_if_unset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, "BrushChannel", "flag", BRUSH_CHANNEL_INHERIT_IF_UNSET);
  RNA_def_property_ui_text(prop, "Combine", "Combine with default settings");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  prop = RNA_def_property(srna, "mappings", PROP_COLLECTION, PROP_NONE);
  // RNA_def_property_collection_sdna(prop, "BrushChannel", "mappings", NULL);
  RNA_def_property_collection_funcs(prop,
                                    "rna_BrushChannel_mappings_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_BrushChannel_mappings_length",
                                    NULL,
                                    "rna_BrushChannel_mappings_lookupstring",
                                    "rna_BrushChannel_mappings_assignint");
  RNA_def_property_struct_type(prop, "BrushMapping");

  prop = RNA_def_property(srna, "color3_value", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, "BrushChannel", "vector");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Color", "");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  prop = RNA_def_property(srna, "color4_value", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, "BrushChannel", "vector");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Color", "");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  prop = RNA_def_property(srna, "vector3_value", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, "BrushChannel", "vector");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Vector", "");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  prop = RNA_def_property(srna, "vector4_value", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, "BrushChannel", "vector");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Vector", "");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  prop = RNA_def_property(srna, "enum_value", PROP_ENUM, PROP_UNIT_NONE);
  RNA_def_property_ui_text(prop, "Enum Value", "Enum values (for enums");
  RNA_def_property_enum_items(prop, null_enum);
  RNA_def_property_enum_funcs(prop,
                              "rna_BrushChannel_enum_value_get",
                              "rna_BrushChannel_enum_value_set",
                              "rna_BrushChannel_enum_value_get_items");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  prop = RNA_def_property(srna, "enum_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE | PROP_ANIMATABLE);
  RNA_def_property_struct_type(prop, "EnumPropertyItem");
  RNA_def_property_collection_funcs(prop,
                                    "rna_BrushChannel_enum_items_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_ui_text(prop, "Items", "Possible values for the property");

  prop = RNA_def_property(srna, "flags_value", PROP_ENUM, PROP_UNIT_NONE);
  RNA_def_property_ui_text(prop, "Flags Value", "Flags values");

  RNA_def_property_enum_bitflag_sdna(prop, "BrushChannel", "ivalue");
  RNA_def_property_enum_items(prop, null_enum);
  RNA_def_property_enum_funcs(prop,
                              "rna_BrushChannel_enum_value_get",
                              "rna_BrushChannel_enum_value_set",
                              "rna_BrushChannel_enum_value_get_items");
  RNA_def_property_flag(prop, PROP_ENUM_FLAG);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  prop = RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "BrushCurve");
  RNA_def_property_ui_text(prop, "Curve", "Curve");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  // PROP_ENUM_FLAG
}

void RNA_def_brush_channelset(BlenderRNA *brna, PropertyRNA *cprop, const char *type_prefix)
{
  StructRNA *srna;
  PropertyRNA *prop;
  FunctionRNA *func;
  PropertyRNA *parm;

  char buf[256], *name;
  sprintf(buf, "%sBrushChannels", type_prefix);
  name = strdup(buf);

  RNA_def_property_srna(cprop, name);

  srna = RNA_def_struct(brna, name, NULL);
  RNA_def_struct_sdna(srna, "BrushChannelSet");
  RNA_def_struct_ui_text(srna, "Brush Channels", "Collection of brush channels");

  // srna = RNA_def_struct(brna, "BrushChannelSet", NULL);
  // RNA_def_struct_sdna(srna, "BrushChannelSet");
  // RNA_def_struct_ui_text(srna, "Channel Set", "Brush Channel Collection");

  func = RNA_def_function(srna, "ensure", "rna_BrushChannelSet_ensure");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_NO_SELF);

  parm = RNA_def_pointer(
      func, "channel", "BrushChannel", "", "Ensure a copy of channel exists in this channel set");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

  // RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

  // prop = RNA_def_property(srna, "channels", PROP_COLLECTION, PROP_NONE);
  prop = cprop;

  RNA_def_property_collection_sdna(prop, NULL, "channels", NULL);
  RNA_def_property_collection_funcs(prop,
                                    "rna_BrushChannelSet_channels_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_iterator_listbase_get",
                                    "rna_BrushChannelSet_length",
                                    NULL,
                                    NULL,
                                    "rna_BrushChannelSet_channels_assignint");
  RNA_def_property_struct_type(prop, "BrushChannel");

  RNA_def_property_clear_flag(prop, PROP_PTR_NO_OWNERSHIP);
  RNA_def_property_flag(prop, PROP_THICK_WRAP | PROP_DYNAMIC);
  RNA_def_property_override_flag(
      prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY | PROPOVERRIDE_LIBRARY_INSERTION);
}

void RNA_def_brush_engine(BlenderRNA *brna)
{
  RNA_def_brush_curve(brna);
  RNA_def_brush_mapping(brna);
  RNA_def_brush_channel(brna);
}
