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

struct BlenderRNA;
struct StructRNA;
struct PropertyRNA;
struct PropertyEnumItem;

/* Blender RNA */

struct BlenderRNA *RNA_create(void);
void RNA_define_free(struct BlenderRNA *brna);
void RNA_free(struct BlenderRNA *brna);

/* Struct */

struct StructRNA *RNA_def_struct(struct BlenderRNA *brna, const char *cname, const char *name);
void RNA_def_struct_sdna(struct StructRNA *rna, const char *structname);
void RNA_def_struct_name_property(struct StructRNA *rna, struct PropertyRNA *prop);

/* Property */

struct PropertyRNA *RNA_def_property(struct StructRNA *strct, const char *cname, int type, int subtype);

void RNA_def_property_boolean_sdna(struct PropertyRNA *prop, const char *structname, const char *propname, int bit);
void RNA_def_property_int_sdna(struct PropertyRNA *prop, const char *structname, const char *propname);
void RNA_def_property_float_sdna(struct PropertyRNA *prop, const char *structname, const char *propname);
void RNA_def_property_string_sdna(struct PropertyRNA *prop, const char *structname, const char *propname);
void RNA_def_property_enum_sdna(struct PropertyRNA *prop, const char *structname, const char *propname);
void RNA_def_property_pointer_sdna(struct PropertyRNA *prop, const char *structname, const char *propname);
void RNA_def_property_collection_sdna(struct PropertyRNA *prop, const char *structname, const char *propname);

void RNA_def_property_array(struct PropertyRNA *prop, int arraylength);
void RNA_def_property_access(struct PropertyRNA *prop, int editable, int evaluatable);
void RNA_def_property_range(struct PropertyRNA *prop, double min, double max);

void RNA_def_property_enum_items(struct PropertyRNA *prop, const struct PropertyEnumItem *item);
void RNA_def_property_string_maxlength(struct PropertyRNA *prop, int maxlength);
void RNA_def_property_struct_type(struct PropertyRNA *prop, const char *type);

void RNA_def_property_boolean_default(struct PropertyRNA *prop, int value);
void RNA_def_property_boolean_array_default(struct PropertyRNA *prop, const int *array);
void RNA_def_property_int_default(struct PropertyRNA *prop, int value);
void RNA_def_property_int_array_default(struct PropertyRNA *prop, const int *array);
void RNA_def_property_float_default(struct PropertyRNA *prop, float value);
void RNA_def_property_float_array_default(struct PropertyRNA *prop, const float *array);
void RNA_def_property_enum_default(struct PropertyRNA *prop, int value);
void RNA_def_property_string_default(struct PropertyRNA *prop, const char *value);

void RNA_def_property_ui_text(struct PropertyRNA *prop, const char *name, const char *description);
void RNA_def_property_ui_range(struct PropertyRNA *prop, double min, double max, double step, double precision);

void RNA_def_property_funcs(struct PropertyRNA *prop, char *notify, char *readonly);
void RNA_def_property_boolean_funcs(struct PropertyRNA *prop, char *get, char *set);
void RNA_def_property_int_funcs(struct PropertyRNA *prop, char *get, char *set);
void RNA_def_property_float_funcs(struct PropertyRNA *prop, char *get, char *set);
void RNA_def_property_enum_funcs(struct PropertyRNA *prop, char *get, char *set);
void RNA_def_property_string_funcs(struct PropertyRNA *prop, char *get, char *length, char *set);
void RNA_def_property_pointer_funcs(struct PropertyRNA *prop, char *get, char *type, char *set);
void RNA_def_property_collection_funcs(struct PropertyRNA *prop, char *begin, char *next, char *end, char *get, char *type, char *length, char *lookupint, char *lookupstring);

#endif /* RNA_DEFINE_H */

