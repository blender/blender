/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef RNA_DEFINE_H
#define RNA_DEFINE_H

/* Functions used during preprocess, for defining the RNA.
 *
 * This is currently only used internally in the module, but should eventually
 * also become available outside of that, at runtime for python and plugins.
 * Where the result of such runtime RNA is stored and how it integrates needs
 * to be figured out still. */

#include "DNA_listBase.h"
#include "RNA_types.h"

/* Blender RNA */

BlenderRNA *RNA_create(void);
void RNA_define_free(BlenderRNA *brna);
void RNA_free(BlenderRNA *brna);

/* Struct */

StructRNA *RNA_def_struct(BlenderRNA *brna, const char *identifier, const char *name);
void RNA_def_struct_sdna(StructRNA *srna, const char *structname);
void RNA_def_struct_name_property(StructRNA *srna, PropertyRNA *prop);
void RNA_def_struct_flag(StructRNA *srna, int flag);

/* Property */

PropertyRNA *RNA_def_property(StructRNA *srna, const char *identifier, int type, int subtype);

void RNA_def_property_boolean_sdna(PropertyRNA *prop, const char *structname, const char *propname, int bit);
void RNA_def_property_int_sdna(PropertyRNA *prop, const char *structname, const char *propname);
void RNA_def_property_float_sdna(PropertyRNA *prop, const char *structname, const char *propname);
void RNA_def_property_string_sdna(PropertyRNA *prop, const char *structname, const char *propname);
void RNA_def_property_enum_sdna(PropertyRNA *prop, const char *structname, const char *propname);
void RNA_def_property_pointer_sdna(PropertyRNA *prop, const char *structname, const char *propname);
void RNA_def_property_collection_sdna(PropertyRNA *prop, const char *structname, const char *propname, const char *lengthpropname);

void RNA_def_property_flag(PropertyRNA *prop, int flag);
void RNA_def_property_array(PropertyRNA *prop, int arraylength);
void RNA_def_property_range(PropertyRNA *prop, double min, double max);

void RNA_def_property_enum_items(PropertyRNA *prop, const EnumPropertyItem *item);
void RNA_def_property_string_maxlength(PropertyRNA *prop, int maxlength);
void RNA_def_property_struct_type(PropertyRNA *prop, const char *type);

void RNA_def_property_boolean_default(PropertyRNA *prop, int value);
void RNA_def_property_boolean_array_default(PropertyRNA *prop, const int *array);
void RNA_def_property_int_default(PropertyRNA *prop, int value);
void RNA_def_property_int_array_default(PropertyRNA *prop, const int *array);
void RNA_def_property_float_default(PropertyRNA *prop, float value);
void RNA_def_property_float_array_default(PropertyRNA *prop, const float *array);
void RNA_def_property_enum_default(PropertyRNA *prop, int value);
void RNA_def_property_string_default(PropertyRNA *prop, const char *value);

void RNA_def_property_ui_text(PropertyRNA *prop, const char *name, const char *description);
void RNA_def_property_ui_range(PropertyRNA *prop, double min, double max, double step, int precision);

void RNA_def_property_funcs(PropertyRNA *prop, const char *notify, const char *readonly);
void RNA_def_property_boolean_funcs(PropertyRNA *prop, const char *get, const char *set);
void RNA_def_property_int_funcs(PropertyRNA *prop, const char *get, const char *set);
void RNA_def_property_float_funcs(PropertyRNA *prop, const char *get, const char *set);
void RNA_def_property_enum_funcs(PropertyRNA *prop, const char *get, const char *set);
void RNA_def_property_string_funcs(PropertyRNA *prop, const char *get, const char *length, const char *set);
void RNA_def_property_pointer_funcs(PropertyRNA *prop, const char *get, const char *type, const char *set);
void RNA_def_property_collection_funcs(PropertyRNA *prop, const char *begin, const char *next, const char *end, const char *get, const char *type, const char *length, const char *lookupint, const char *lookupstring);

#endif /* RNA_DEFINE_H */

