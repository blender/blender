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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "DNA_ID.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "rna_internal.h"

EnumPropertyItem property_type_items[] = {
	{PROP_BOOLEAN, "BOOLEAN", 0, "Boolean", ""},
	{PROP_INT, "INT", 0, "Integer", ""},
	{PROP_FLOAT, "FLOAT", 0, "Float", ""},
	{PROP_STRING, "STRING", 0, "String", ""},
	{PROP_ENUM, "ENUM", 0, "Enumeration", ""},
	{PROP_POINTER, "POINTER", 0, "Pointer", ""},
	{PROP_COLLECTION, "COLLECTION", 0, "Collection", ""},
	{0, NULL, 0, NULL, NULL}};

EnumPropertyItem property_unit_items[] = {
	{PROP_UNIT_NONE, "NONE", 0, "None", ""},
	{PROP_UNIT_LENGTH, "LENGTH", 0, "Length", ""},
	{PROP_UNIT_AREA, "AREA", 0, "Area", ""},
	{PROP_UNIT_VOLUME, "VOLUME", 0, "Volume", ""},
	{PROP_UNIT_ROTATION, "ROTATION", 0, "Rotation", ""},
	{PROP_UNIT_TIME, "TIME", 0, "Time", ""},
	{PROP_UNIT_VELOCITY, "VELOCITY", 0, "Velocity", ""},
	{PROP_UNIT_ACCELERATION, "ACCELERATION", 0, "Acceleration", ""},
	{0, NULL, 0, NULL, NULL}};

#ifdef RNA_RUNTIME

#include "BLI_ghash.h"

/* Struct */

static void rna_Struct_identifier_get(PointerRNA *ptr, char *value)
{
	strcpy(value, ((StructRNA*)ptr->data)->identifier);
}

static int rna_Struct_identifier_length(PointerRNA *ptr)
{
	return strlen(((StructRNA*)ptr->data)->identifier);
}

static void rna_Struct_description_get(PointerRNA *ptr, char *value)
{
	strcpy(value, ((StructRNA*)ptr->data)->description);
}

static int rna_Struct_description_length(PointerRNA *ptr)
{
	return strlen(((StructRNA*)ptr->data)->description);
}

static void rna_Struct_name_get(PointerRNA *ptr, char *value)
{
	strcpy(value, ((StructRNA*)ptr->data)->name);
}

static int rna_Struct_name_length(PointerRNA *ptr)
{
	return strlen(((StructRNA*)ptr->data)->name);
}

static PointerRNA rna_Struct_base_get(PointerRNA *ptr)
{
	return rna_pointer_inherit_refine(ptr, &RNA_Struct, ((StructRNA*)ptr->data)->base);
}

static PointerRNA rna_Struct_nested_get(PointerRNA *ptr)
{
	return rna_pointer_inherit_refine(ptr, &RNA_Struct, ((StructRNA*)ptr->data)->nested);
}

static PointerRNA rna_Struct_name_property_get(PointerRNA *ptr)
{
	return rna_pointer_inherit_refine(ptr, &RNA_Property, ((StructRNA*)ptr->data)->nameproperty);
}

/* Struct property iteration. This is quite complicated, the purpose is to
 * iterate over properties of all inheritance levels, and for each struct to
 * also iterator over id properties not known by RNA. */

static int rna_idproperty_known(CollectionPropertyIterator *iter, void *data)
{
	IDProperty *idprop= (IDProperty*)data;
	PropertyRNA *prop;
	StructRNA *ptype= iter->builtin_parent.type;

	/* function to skip any id properties that are already known by RNA,
	 * for the second loop where we go over unknown id properties */

	for(prop= ptype->cont.properties.first; prop; prop=prop->next)
		if(strcmp(prop->identifier, idprop->name) == 0)
			return 1;
	
	return 0;
}

static int rna_property_builtin(CollectionPropertyIterator *iter, void *data)
{
	PropertyRNA *prop= (PropertyRNA*)data;

	/* function to skip builtin rna properties */

	return (prop->flag & PROP_BUILTIN);
}

static int rna_function_builtin(CollectionPropertyIterator *iter, void *data)
{
	FunctionRNA *func= (FunctionRNA*)data;

	/* function to skip builtin rna functions */

	return (func->flag & FUNC_BUILTIN);
}

static void rna_inheritance_next_level_restart(CollectionPropertyIterator *iter, IteratorSkipFunc skip, int funcs)
{
	/* RNA struct inheritance */
	while(!iter->valid && iter->level > 0) {
		StructRNA *srna;
		int i;

		srna= (StructRNA*)iter->parent.data;
		iter->level--;
		for(i=iter->level; i>0; i--)
			srna= srna->base;

		rna_iterator_listbase_end(iter);

		if (funcs)
			rna_iterator_listbase_begin(iter, &srna->functions, skip);
		else
			rna_iterator_listbase_begin(iter, &srna->cont.properties, skip);
	}
}

static void rna_inheritance_properties_listbase_begin(CollectionPropertyIterator *iter, ListBase *lb, IteratorSkipFunc skip)
{
	rna_iterator_listbase_begin(iter, lb, skip);
	rna_inheritance_next_level_restart(iter, skip, 0);
}

static void rna_inheritance_properties_listbase_next(CollectionPropertyIterator *iter, IteratorSkipFunc skip)
{
	rna_iterator_listbase_next(iter);
	rna_inheritance_next_level_restart(iter, skip, 0);
}

static void rna_inheritance_functions_listbase_begin(CollectionPropertyIterator *iter, ListBase *lb, IteratorSkipFunc skip)
{
	rna_iterator_listbase_begin(iter, lb, skip);
	rna_inheritance_next_level_restart(iter, skip, 1);
}

static void rna_inheritance_functions_listbase_next(CollectionPropertyIterator *iter, IteratorSkipFunc skip)
{
	rna_iterator_listbase_next(iter);
	rna_inheritance_next_level_restart(iter, skip, 1);
}

static void rna_Struct_properties_next(CollectionPropertyIterator *iter)
{
	ListBaseIterator *internal= iter->internal;
	IDProperty *group;

	if(internal->flag) {
		/* id properties */
		rna_iterator_listbase_next(iter);
	}
	else {
		/* regular properties */
		rna_inheritance_properties_listbase_next(iter, rna_property_builtin);

		/* try id properties */
		if(!iter->valid) {
			group= RNA_struct_idproperties(&iter->builtin_parent, 0);

			if(group) {
				rna_iterator_listbase_end(iter);
				rna_iterator_listbase_begin(iter, &group->data.group, rna_idproperty_known);
				internal= iter->internal;
				internal->flag= 1;
			}
		}
	}
}

static void rna_Struct_properties_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	StructRNA *srna;

	/* here ptr->data should always be the same as iter->parent.type */
	srna= (StructRNA *)ptr->data;

	while(srna->base) {
		iter->level++;
		srna= srna->base;
	}

	rna_inheritance_properties_listbase_begin(iter, &srna->cont.properties, rna_property_builtin);
}

static PointerRNA rna_Struct_properties_get(CollectionPropertyIterator *iter)
{
	ListBaseIterator *internal= iter->internal;

	/* we return either PropertyRNA* or IDProperty*, the rna_access.c
	 * functions can handle both as PropertyRNA* with some tricks */
	return rna_pointer_inherit_refine(&iter->parent, &RNA_Property, internal->link);
}

static void rna_Struct_functions_next(CollectionPropertyIterator *iter)
{
	rna_inheritance_functions_listbase_next(iter, rna_function_builtin);
}

static void rna_Struct_functions_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	StructRNA *srna;

	/* here ptr->data should always be the same as iter->parent.type */
	srna= (StructRNA *)ptr->data;

	while(srna->base) {
		iter->level++;
		srna= srna->base;
	}

	rna_inheritance_functions_listbase_begin(iter, &srna->functions, rna_function_builtin);
}

static PointerRNA rna_Struct_functions_get(CollectionPropertyIterator *iter)
{
	ListBaseIterator *internal= iter->internal;

	/* we return either PropertyRNA* or IDProperty*, the rna_access.c
	 * functions can handle both as PropertyRNA* with some tricks */
	return rna_pointer_inherit_refine(&iter->parent, &RNA_Function, internal->link);
}

/* Builtin properties iterator re-uses the Struct properties iterator, only
 * difference is that we need to set the ptr data to the type of the struct
 * whose properties we want to iterate over. */

void rna_builtin_properties_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	PointerRNA newptr;

	/* we create a new pointer with the type as the data */
	newptr.type= &RNA_Struct;
	newptr.data= ptr->type;

	if(ptr->type->flag & STRUCT_ID)
		newptr.id.data= ptr->data;
	else
		newptr.id.data= NULL;

	iter->parent= newptr;
	iter->builtin_parent = *ptr;

	rna_Struct_properties_begin(iter, &newptr);
}

void rna_builtin_properties_next(CollectionPropertyIterator *iter)
{
	rna_Struct_properties_next(iter);
}

PointerRNA rna_builtin_properties_get(CollectionPropertyIterator *iter)
{
	return rna_Struct_properties_get(iter);
}

PointerRNA rna_builtin_properties_lookup_string(PointerRNA *ptr, const char *key)
{
	StructRNA *srna;
	PropertyRNA *prop;
	PointerRNA propptr;

	memset(&propptr, 0, sizeof(propptr));
	srna= ptr->type;

	do {
		if(srna->cont.prophash) {
			prop= BLI_ghash_lookup(srna->cont.prophash, (void*)key);

			if(prop) {
				propptr.type= &RNA_Property;
				propptr.data= prop;
				return propptr;
			}
		}

		for(prop=srna->cont.properties.first; prop; prop=prop->next) {
			if(!(prop->flag & PROP_BUILTIN) && strcmp(prop->identifier, key)==0) {
				propptr.type= &RNA_Property;
				propptr.data= prop;
				return propptr;
			}
		}
	} while((srna=srna->base));

	/* this was used pre 2.5beta0, now ID property access uses python's
	 * getitem style access
	 * - ob["foo"] rather then ob.foo */
#if 0
	if(ptr->data) {
		IDProperty *group, *idp;

		group= RNA_struct_idproperties(ptr, 0);

		if(group) {
			for(idp=group->data.group.first; idp; idp=idp->next) {
				if(strcmp(idp->name, key) == 0) {
					propptr.type= &RNA_Property;
					propptr.data= idp;
					return propptr;
				}
			}
		}
	}
#endif
	return propptr;
}

PointerRNA rna_builtin_type_get(PointerRNA *ptr)
{
	return rna_pointer_inherit_refine(ptr, &RNA_Struct, ptr->type);
}

/* Property */

static StructRNA *rna_Property_refine(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;

	rna_idproperty_check(&prop, ptr); /* XXX ptr? */

	switch(prop->type) {
		case PROP_BOOLEAN: return &RNA_BooleanProperty;
		case PROP_INT: return &RNA_IntProperty;
		case PROP_FLOAT: return &RNA_FloatProperty;
		case PROP_STRING: return &RNA_StringProperty;
		case PROP_ENUM: return &RNA_EnumProperty;
		case PROP_POINTER: return &RNA_PointerProperty;
		case PROP_COLLECTION: return &RNA_CollectionProperty;
		default: return &RNA_Property;
	}
}

static void rna_Property_identifier_get(PointerRNA *ptr, char *value)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	strcpy(value, ((PropertyRNA*)prop)->identifier);
}

static int rna_Property_identifier_length(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	return strlen(prop->identifier);
}

static void rna_Property_name_get(PointerRNA *ptr, char *value)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	strcpy(value, prop->name);
}

static int rna_Property_name_length(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	return strlen(prop->name);
}

static void rna_Property_description_get(PointerRNA *ptr, char *value)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	strcpy(value, prop->description ? prop->description:"");
}
static int rna_Property_description_length(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	return prop->description ? strlen(prop->description) : 0;
}

static int rna_Property_type_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	return prop->type;
}

static int rna_Property_subtype_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	return prop->subtype;
}

static PointerRNA rna_Property_srna_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	return rna_pointer_inherit_refine(ptr, &RNA_Struct, prop->srna);
}

static int rna_Property_unit_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	return RNA_SUBTYPE_UNIT(prop->subtype);
}

static int rna_Property_readonly_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;

	/* dont use this becaure it will call functions that check the internal
	 * data for introspection we only need to know if it can be edited so the
	 * flag is better for this */
//	return RNA_property_editable(ptr, prop);
	return prop->flag & PROP_EDITABLE ? 0:1;
}

static int rna_Property_use_output_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	return prop->flag & PROP_OUTPUT ? 1:0;
}

static int rna_Property_is_required_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	return prop->flag & PROP_REQUIRED ? 1:0;
}

static int rna_Property_is_never_none_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	return prop->flag & PROP_NEVER_NULL ? 1:0;
}

static int rna_Property_array_length_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	return prop->totarraylength;
}

static int rna_Property_registered_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	return prop->flag & PROP_REGISTER;
}

static int rna_Property_registered_optional_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	return prop->flag & PROP_REGISTER_OPTIONAL;
}

static int rna_BoolProperty_default_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	return ((BooleanPropertyRNA*)prop)->defaultvalue;
}

static int rna_IntProperty_default_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	return ((IntPropertyRNA*)prop)->defaultvalue;
}
/* int/float/bool */
static int rna_NumberProperty_default_array_get_length(PointerRNA *ptr, int length[RNA_MAX_ARRAY_DIMENSION])
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);

	length[0]= prop->totarraylength;

	return length[0];
}
static void rna_IntProperty_default_array_get(PointerRNA *ptr, int *values)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	IntPropertyRNA *nprop= (IntPropertyRNA*)prop;
	rna_idproperty_check(&prop, ptr);

	if(nprop->defaultarray) {
		memcpy(values, nprop->defaultarray, prop->totarraylength * sizeof(int));
	}
	else {
		int i;
		for(i=0; i < prop->totarraylength; i++)
			values[i]= nprop->defaultvalue;
	}
}
static void rna_BoolProperty_default_array_get(PointerRNA *ptr, int *values)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	BooleanPropertyRNA *nprop= (BooleanPropertyRNA*)prop;
	rna_idproperty_check(&prop, ptr);

	if(nprop->defaultarray) {
		memcpy(values, nprop->defaultarray, prop->totarraylength * sizeof(int));
	}
	else {
		int i;
		for(i=0; i < prop->totarraylength; i++)
			values[i]= nprop->defaultvalue;
	}
}
static void rna_FloatProperty_default_array_get(PointerRNA *ptr, float *values)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	FloatPropertyRNA *nprop= (FloatPropertyRNA*)prop;
	rna_idproperty_check(&prop, ptr);

	if(nprop->defaultarray) {
		memcpy(values, nprop->defaultarray, prop->totarraylength * sizeof(float));
	}
	else {
		int i;
		for(i=0; i < prop->totarraylength; i++)
			values[i]= nprop->defaultvalue;
	}
}

static int rna_IntProperty_hard_min_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	return ((IntPropertyRNA*)prop)->hardmin;
}

static int rna_IntProperty_hard_max_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	return ((IntPropertyRNA*)prop)->hardmax;
}

static int rna_IntProperty_soft_min_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	return ((IntPropertyRNA*)prop)->softmin;
}

static int rna_IntProperty_soft_max_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	return ((IntPropertyRNA*)prop)->softmax;
}

static int rna_IntProperty_step_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	return ((IntPropertyRNA*)prop)->step;
}

static float rna_FloatProperty_default_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	return ((FloatPropertyRNA*)prop)->defaultvalue;
}
static float rna_FloatProperty_hard_min_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	return ((FloatPropertyRNA*)prop)->hardmin;
}

static float rna_FloatProperty_hard_max_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	return ((FloatPropertyRNA*)prop)->hardmax;
}

static float rna_FloatProperty_soft_min_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	return ((FloatPropertyRNA*)prop)->softmin;
}

static float rna_FloatProperty_soft_max_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	return ((FloatPropertyRNA*)prop)->softmax;
}

static float rna_FloatProperty_step_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	return ((FloatPropertyRNA*)prop)->step;
}

static int rna_FloatProperty_precision_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	return ((FloatPropertyRNA*)prop)->precision;
}

static void rna_StringProperty_default_get(PointerRNA *ptr, char *value)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	strcpy(value, ((StringPropertyRNA*)prop)->defaultvalue);
}
static int rna_StringProperty_default_length(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	return strlen(((StringPropertyRNA*)prop)->defaultvalue);
}

static int rna_StringProperty_max_length_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	return ((StringPropertyRNA*)prop)->maxlength;
}

static EnumPropertyItem *rna_EnumProperty_default_itemf(bContext *C, PointerRNA *ptr, int *free)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	EnumPropertyRNA *eprop;

	rna_idproperty_check(&prop, ptr);
	eprop= (EnumPropertyRNA*)prop;

	if(		(eprop->itemf == NULL) ||
			(eprop->itemf == rna_EnumProperty_default_itemf) ||
			(ptr->type == &RNA_EnumProperty) ||
			(C == NULL))
	{
		return eprop->item;
	}

	return eprop->itemf(C, ptr, free);
}

/* XXX - not sure this is needed? */
static int rna_EnumProperty_default_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	return ((EnumPropertyRNA*)prop)->defaultvalue;
}

static int rna_enum_check_separator(CollectionPropertyIterator *iter, void *data)
{
	EnumPropertyItem *item= (EnumPropertyItem*)data;

	return (item->identifier[0] == 0);
}

static void rna_EnumProperty_items_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	EnumPropertyRNA *eprop;
	EnumPropertyItem *item= NULL;
	int totitem, free= 0;
	
	rna_idproperty_check(&prop, ptr);
	eprop= (EnumPropertyRNA*)prop;
	
	RNA_property_enum_items(NULL, ptr, prop, &item, &totitem, &free);
	rna_iterator_array_begin(iter, (void*)item, sizeof(EnumPropertyItem), totitem, free, rna_enum_check_separator);
}

static void rna_EnumPropertyItem_identifier_get(PointerRNA *ptr, char *value)
{
	strcpy(value, ((EnumPropertyItem*)ptr->data)->identifier);
}

static int rna_EnumPropertyItem_identifier_length(PointerRNA *ptr)
{
	return strlen(((EnumPropertyItem*)ptr->data)->identifier);
}

static void rna_EnumPropertyItem_name_get(PointerRNA *ptr, char *value)
{
	strcpy(value, ((EnumPropertyItem*)ptr->data)->name);
}

static int rna_EnumPropertyItem_name_length(PointerRNA *ptr)
{
	return strlen(((EnumPropertyItem*)ptr->data)->name);
}

static void rna_EnumPropertyItem_description_get(PointerRNA *ptr, char *value)
{
	EnumPropertyItem *eprop= (EnumPropertyItem*)ptr->data;

	if(eprop->description)
		strcpy(value, eprop->description);
	else
		value[0]= '\0';
}

static int rna_EnumPropertyItem_description_length(PointerRNA *ptr)
{
	EnumPropertyItem *eprop= (EnumPropertyItem*)ptr->data;

	if(eprop->description)
		return strlen(eprop->description);
	else
		return 0;
}

static int rna_EnumPropertyItem_value_get(PointerRNA *ptr)
{
	return ((EnumPropertyItem*)ptr->data)->value;
}

static PointerRNA rna_PointerProperty_fixed_type_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	return rna_pointer_inherit_refine(ptr, &RNA_Struct, ((PointerPropertyRNA*)prop)->type);
}

static PointerRNA rna_CollectionProperty_fixed_type_get(PointerRNA *ptr)
{
	PropertyRNA *prop= (PropertyRNA*)ptr->data;
	rna_idproperty_check(&prop, ptr);
	return rna_pointer_inherit_refine(ptr, &RNA_Struct, ((CollectionPropertyRNA*)prop)->item_type);
}

/* Function */

static void rna_Function_identifier_get(PointerRNA *ptr, char *value)
{
	strcpy(value, ((FunctionRNA*)ptr->data)->identifier);
}

static int rna_Function_identifier_length(PointerRNA *ptr)
{
	return strlen(((FunctionRNA*)ptr->data)->identifier);
}

static void rna_Function_description_get(PointerRNA *ptr, char *value)
{
	strcpy(value, ((FunctionRNA*)ptr->data)->description);
}

static int rna_Function_description_length(PointerRNA *ptr)
{
	return strlen(((FunctionRNA*)ptr->data)->description);
}

static void rna_Function_parameters_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	rna_iterator_listbase_begin(iter, &((FunctionRNA*)ptr->data)->cont.properties, rna_property_builtin);
}

static int rna_Function_registered_get(PointerRNA *ptr)
{
	FunctionRNA *func= (FunctionRNA*)ptr->data;
	return func->flag & FUNC_REGISTER;
}

static int rna_Function_registered_optional_get(PointerRNA *ptr)
{
	FunctionRNA *func= (FunctionRNA*)ptr->data;
	return func->flag & FUNC_REGISTER_OPTIONAL;
}

/* Blender RNA */

static void rna_BlenderRNA_structs_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	rna_iterator_listbase_begin(iter, &((BlenderRNA*)ptr->data)->structs, NULL);
}

/* optional, for faster lookups */
static int rna_BlenderRNA_structs_length(PointerRNA *ptr)
{
	return BLI_countlist(&((BlenderRNA*)ptr->data)->structs);
}
static PointerRNA rna_BlenderRNA_structs_lookup_int(PointerRNA *ptr, int index)
{
	StructRNA *srna= BLI_findlink(&((BlenderRNA*)ptr->data)->structs, index);

	if(srna) {
		PointerRNA r_ptr;
		RNA_pointer_create(NULL, &RNA_Struct, srna, &r_ptr);
		return r_ptr;
	}
	else {
		return PointerRNA_NULL;
	}
}
static PointerRNA rna_BlenderRNA_structs_lookup_string(PointerRNA *ptr, const char *key)
{
	StructRNA *srna= ((BlenderRNA*)ptr->data)->structs.first;
	for(; srna; srna=srna->cont.next)
		if(key[0] == srna->identifier[0] && strcmp(key, srna->identifier)==0)
			break;

	if(srna) {
		PointerRNA r_ptr;
		RNA_pointer_create(NULL, &RNA_Struct, srna, &r_ptr);
		return r_ptr;
	}
	else {
		return PointerRNA_NULL;
	}
}


#else

static void rna_def_struct(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "Struct", NULL);
	RNA_def_struct_ui_text(srna, "Struct Definition", "RNA structure definition");
	RNA_def_struct_ui_icon(srna, ICON_RNA);

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_Struct_name_get", "rna_Struct_name_length", NULL);
	RNA_def_property_ui_text(prop, "Name", "Human readable name");

	prop= RNA_def_property(srna, "identifier", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_Struct_identifier_get", "rna_Struct_identifier_length", NULL);
	RNA_def_property_ui_text(prop, "Identifier", "Unique name used in the code and scripting");
	RNA_def_struct_name_property(srna, prop);
	
	prop= RNA_def_property(srna, "description", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_Struct_description_get", "rna_Struct_description_length", NULL);
	RNA_def_property_ui_text(prop, "Description", "Description of the Struct's purpose");
	
	prop= RNA_def_property(srna, "base", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "Struct");
	RNA_def_property_pointer_funcs(prop, "rna_Struct_base_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Base", "Struct definition this is derived from");

	prop= RNA_def_property(srna, "nested", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "Struct");
	RNA_def_property_pointer_funcs(prop, "rna_Struct_nested_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Nested", "Struct in which this struct is always nested, and to which it logically belongs");

	prop= RNA_def_property(srna, "name_property", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "StringProperty");
	RNA_def_property_pointer_funcs(prop, "rna_Struct_name_property_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Name Property", "Property that gives the name of the struct");

	prop= RNA_def_property(srna, "properties", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "Property");
	RNA_def_property_collection_funcs(prop, "rna_Struct_properties_begin", "rna_Struct_properties_next", "rna_iterator_listbase_end", "rna_Struct_properties_get", 0, 0, 0);
	RNA_def_property_ui_text(prop, "Properties", "Properties in the struct");

	prop= RNA_def_property(srna, "functions", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "Function");
	RNA_def_property_collection_funcs(prop, "rna_Struct_functions_begin", "rna_Struct_functions_next", "rna_iterator_listbase_end", "rna_Struct_functions_get", 0, 0, 0);
	RNA_def_property_ui_text(prop, "Functions", "");
}

static void rna_def_property(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem subtype_items[] = {
		{PROP_NONE, "NONE", 0, "None", ""},
		{PROP_FILEPATH, "FILE_PATH", 0, "File Path", ""},
		{PROP_DIRPATH, "DIRECTORY_PATH", 0, "Directory Path", ""},
		{PROP_UNSIGNED, "UNSIGNED", 0, "Unsigned Number", ""},
		{PROP_PERCENTAGE, "PERCENTAGE", 0, "Percentage", ""},
		{PROP_FACTOR, "FACTOR", 0, "Factor", ""},
		{PROP_ANGLE, "ANGLE", 0, "Angle", ""},
		{PROP_TIME, "TIME", 0, "Time", ""},
		{PROP_DISTANCE, "DISTANCE", 0, "Distance", ""},
		{PROP_COLOR, "COLOR", 0, "Color", ""},
		{PROP_TRANSLATION, "TRANSLATION", 0, "Translation", ""},
		{PROP_DIRECTION, "DIRECTION", 0, "Direction", ""},
		{PROP_MATRIX, "MATRIX", 0, "Matrix", ""},
		{PROP_EULER, "EULER", 0, "Euler", ""},
		{PROP_QUATERNION, "QUATERNION", 0, "Quaternion", ""},
		{PROP_XYZ, "XYZ", 0, "XYZ", ""},
		{PROP_COLOR_GAMMA, "COLOR_GAMMA", 0, "Gamma Corrected Color", ""},
		{PROP_LAYER, "LAYER", 0, "Layer", ""},
		{PROP_LAYER_MEMBER, "LAYER_MEMBERSHIP", 0, "Layer Membership", ""},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "Property", NULL);
	RNA_def_struct_ui_text(srna, "Property Definition", "RNA property definition");
	RNA_def_struct_refine_func(srna, "rna_Property_refine");
	RNA_def_struct_ui_icon(srna, ICON_RNA);

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_Property_name_get", "rna_Property_name_length", NULL);
	RNA_def_property_ui_text(prop, "Name", "Human readable name");

	prop= RNA_def_property(srna, "identifier", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_Property_identifier_get", "rna_Property_identifier_length", NULL);
	RNA_def_property_ui_text(prop, "Identifier", "Unique name used in the code and scripting");
	RNA_def_struct_name_property(srna, prop);
		
	prop= RNA_def_property(srna, "description", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_Property_description_get", "rna_Property_description_length", NULL);
	RNA_def_property_ui_text(prop, "Description", "Description of the property for tooltips");

	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_items(prop, property_type_items);
	RNA_def_property_enum_funcs(prop, "rna_Property_type_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Type", "Data type of the property");

	prop= RNA_def_property(srna, "subtype", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_items(prop, subtype_items);
	RNA_def_property_enum_funcs(prop, "rna_Property_subtype_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Subtype", "Semantic interpretation of the property");

	prop= RNA_def_property(srna, "srna", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "Struct");
	RNA_def_property_pointer_funcs(prop, "rna_Property_srna_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Base", "Struct definition used for properties assigned to this item");

	prop= RNA_def_property(srna, "unit", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_items(prop, property_unit_items);
	RNA_def_property_enum_funcs(prop, "rna_Property_unit_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Unit", "Type of units for this property");

	prop= RNA_def_property(srna, "is_readonly", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_boolean_funcs(prop, "rna_Property_readonly_get", NULL);
	RNA_def_property_ui_text(prop, "Read Only", "Property is editable through RNA");

	prop= RNA_def_property(srna, "is_required", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_boolean_funcs(prop, "rna_Property_is_required_get", NULL);
	RNA_def_property_ui_text(prop, "Required", "False when this property is an optional argument in an rna function");

	prop= RNA_def_property(srna, "is_never_none", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_boolean_funcs(prop, "rna_Property_is_never_none_get", NULL);
	RNA_def_property_ui_text(prop, "Never None", "True when this value can't be set to None");

	prop= RNA_def_property(srna, "use_output", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_boolean_funcs(prop, "rna_Property_use_output_get", NULL);
	RNA_def_property_ui_text(prop, "Return", "True when this property is an output value from an rna function");

	prop= RNA_def_property(srna, "registered", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_boolean_funcs(prop, "rna_Property_registered_get", NULL);
	RNA_def_property_ui_text(prop, "Registered", "Property is registerd as part of type registration");

	prop= RNA_def_property(srna, "registered_optional", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_boolean_funcs(prop, "rna_Property_registered_optional_get", NULL);
	RNA_def_property_ui_text(prop, "Registered Optionally", "Property is optionally registerd as part of type registration");
}

static void rna_def_function(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "Function", NULL);
	RNA_def_struct_ui_text(srna, "Function Definition", "RNA function definition");
	RNA_def_struct_ui_icon(srna, ICON_RNA);

	prop= RNA_def_property(srna, "identifier", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_Function_identifier_get", "rna_Function_identifier_length", NULL);
	RNA_def_property_ui_text(prop, "Identifier", "Unique name used in the code and scripting");
	RNA_def_struct_name_property(srna, prop);

	prop= RNA_def_property(srna, "description", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_Function_description_get", "rna_Function_description_length", NULL);
	RNA_def_property_ui_text(prop, "Description", "Description of the Function's purpose");

	prop= RNA_def_property(srna, "parameters", PROP_COLLECTION, PROP_NONE);
	/*RNA_def_property_clear_flag(prop, PROP_EDITABLE);*/
	RNA_def_property_struct_type(prop, "Property");
	RNA_def_property_collection_funcs(prop, "rna_Function_parameters_begin", "rna_iterator_listbase_next", "rna_iterator_listbase_end", "rna_iterator_listbase_get", 0, 0, 0);
	RNA_def_property_ui_text(prop, "Parameters", "Parameters for the function");

	prop= RNA_def_property(srna, "registered", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_boolean_funcs(prop, "rna_Function_registered_get", NULL);
	RNA_def_property_ui_text(prop, "Registered", "Function is registerd as callback as part of type registration");

	prop= RNA_def_property(srna, "registered_optional", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_boolean_funcs(prop, "rna_Function_registered_optional_get", NULL);
	RNA_def_property_ui_text(prop, "Registered Optionally", "Function is optionally registerd as callback part of type registration");
}

static void rna_def_number_property(StructRNA *srna, PropertyType type)
{
	PropertyRNA *prop;

	prop= RNA_def_property(srna, "default", type, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Default", "Default value for this number");

	switch(type) {
		case PROP_BOOLEAN:
			RNA_def_property_boolean_funcs(prop, "rna_BoolProperty_default_get", NULL);
			break;
		case PROP_INT:
			RNA_def_property_int_funcs(prop, "rna_IntProperty_default_get", NULL, NULL);
			break;
		case PROP_FLOAT:
			RNA_def_property_float_funcs(prop, "rna_FloatProperty_default_get", NULL, NULL);
			break;
		default:
			break;
	}


	prop= RNA_def_property(srna, "default_array", type, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_array(prop, RNA_MAX_ARRAY_DIMENSION); /* no fixed default length, important its not 0 though */
	RNA_def_property_flag(prop, PROP_DYNAMIC);
	RNA_def_property_dynamic_array_funcs(prop, "rna_NumberProperty_default_array_get_length"); /* same for all types */

	switch(type) {
		case PROP_BOOLEAN:
			RNA_def_property_boolean_funcs(prop, "rna_BoolProperty_default_array_get", NULL);
			break;
		case PROP_INT:
			RNA_def_property_int_funcs(prop, "rna_IntProperty_default_array_get", NULL, NULL);
			break;
		case PROP_FLOAT:
			RNA_def_property_float_funcs(prop, "rna_FloatProperty_default_array_get", NULL, NULL);
			break;
		default:
			break;
	}
	RNA_def_property_ui_text(prop, "Default Array", "Default value for this array");


	prop= RNA_def_property(srna, "array_length", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_int_funcs(prop, "rna_Property_array_length_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Array Length", "Maximum length of the array, 0 means unlimited");

	if(type == PROP_BOOLEAN)
		return;

	prop= RNA_def_property(srna, "hard_min", type, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	if(type == PROP_INT) RNA_def_property_int_funcs(prop, "rna_IntProperty_hard_min_get", NULL, NULL);
	else RNA_def_property_float_funcs(prop, "rna_FloatProperty_hard_min_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Hard Minimum", "Minimum value used by buttons");

	prop= RNA_def_property(srna, "hard_max", type, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	if(type == PROP_INT) RNA_def_property_int_funcs(prop, "rna_IntProperty_hard_max_get", NULL, NULL);
	else RNA_def_property_float_funcs(prop, "rna_FloatProperty_hard_max_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Hard Maximum", "Maximum value used by buttons");

	prop= RNA_def_property(srna, "soft_min", type, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	if(type == PROP_INT) RNA_def_property_int_funcs(prop, "rna_IntProperty_soft_min_get", NULL, NULL);
	else RNA_def_property_float_funcs(prop, "rna_FloatProperty_soft_min_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Soft Minimum", "Minimum value used by buttons");

	prop= RNA_def_property(srna, "soft_max", type, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	if(type == PROP_INT) RNA_def_property_int_funcs(prop, "rna_IntProperty_soft_max_get", NULL, NULL);
	else RNA_def_property_float_funcs(prop, "rna_FloatProperty_soft_max_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Soft Maximum", "Maximum value used by buttons");

	prop= RNA_def_property(srna, "step", type, PROP_UNSIGNED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	if(type == PROP_INT) RNA_def_property_int_funcs(prop, "rna_IntProperty_step_get", NULL, NULL);
	else RNA_def_property_float_funcs(prop, "rna_FloatProperty_step_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Step", "Step size used by number buttons, for floats 1/100th of the step size");

	if(type == PROP_FLOAT) {
		prop= RNA_def_property(srna, "precision", PROP_INT, PROP_UNSIGNED);
		RNA_def_property_clear_flag(prop, PROP_EDITABLE);
		RNA_def_property_int_funcs(prop, "rna_FloatProperty_precision_get", NULL, NULL);
		RNA_def_property_ui_text(prop, "Precision", "Number of digits after the dot used by buttons");
	}
}

static void rna_def_string_property(StructRNA *srna)
{
	PropertyRNA *prop;

	prop= RNA_def_property(srna, "default", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_StringProperty_default_get", "rna_StringProperty_default_length", NULL);
	RNA_def_property_ui_text(prop, "Default", "string default value");

	prop= RNA_def_property(srna, "max_length", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_int_funcs(prop, "rna_StringProperty_max_length_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Maximum Length", "Maximum length of the string, 0 means unlimited");
}

static void rna_def_enum_property(BlenderRNA *brna, StructRNA *srna)
{
	PropertyRNA *prop;

	/* the itemf func is used instead, keep blender happy */
	static EnumPropertyItem default_dummy_items[] = {
		{PROP_NONE, "DUMMY", 0, "Dummy", ""},
		{0, NULL, 0, NULL, NULL}};

	prop= RNA_def_property(srna, "default", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_items(prop, default_dummy_items);
	RNA_def_property_enum_funcs(prop, "rna_EnumProperty_default_get", NULL, "rna_EnumProperty_default_itemf");
	RNA_def_property_ui_text(prop, "Default", "Default value for this enum");

	prop= RNA_def_property(srna, "items", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "EnumPropertyItem");
	RNA_def_property_collection_funcs(prop, "rna_EnumProperty_items_begin", "rna_iterator_array_next", "rna_iterator_array_end", "rna_iterator_array_get", 0, 0, 0);
	RNA_def_property_ui_text(prop, "Items", "Possible values for the property");

	srna= RNA_def_struct(brna, "EnumPropertyItem", NULL);
	RNA_def_struct_ui_text(srna, "Enum Item Definition", "Definition of a choice in an RNA enum property");
	RNA_def_struct_ui_icon(srna, ICON_RNA);

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_EnumPropertyItem_name_get", "rna_EnumPropertyItem_name_length", NULL);
	RNA_def_property_ui_text(prop, "Name", "Human readable name");

	prop= RNA_def_property(srna, "description", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_EnumPropertyItem_description_get", "rna_EnumPropertyItem_description_length", NULL);
	RNA_def_property_ui_text(prop, "Description", "Description of the item's purpose");

	prop= RNA_def_property(srna, "identifier", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_EnumPropertyItem_identifier_get", "rna_EnumPropertyItem_identifier_length", NULL);
	RNA_def_property_ui_text(prop, "Identifier", "Unique name used in the code and scripting");
	RNA_def_struct_name_property(srna, prop);

	prop= RNA_def_property(srna, "value", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_int_funcs(prop, "rna_EnumPropertyItem_value_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Value", "Value of the item");
}

static void rna_def_pointer_property(StructRNA *srna, PropertyType type)
{
	PropertyRNA *prop;

	prop= RNA_def_property(srna, "fixed_type", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "Struct");
	if(type == PROP_POINTER)
		RNA_def_property_pointer_funcs(prop, "rna_PointerProperty_fixed_type_get", NULL, NULL);
	else
		RNA_def_property_pointer_funcs(prop, "rna_CollectionProperty_fixed_type_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Pointer Type", "Fixed pointer type, empty if variable type");
}

void RNA_def_rna(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	/* Struct*/
	rna_def_struct(brna);

	/* Property */
	rna_def_property(brna);

	/* BooleanProperty */
	srna= RNA_def_struct(brna, "BooleanProperty", "Property");
	RNA_def_struct_ui_text(srna, "Boolean Definition", "RNA boolean property definition");
	rna_def_number_property(srna, PROP_BOOLEAN);

	/* IntProperty */
	srna= RNA_def_struct(brna, "IntProperty", "Property");
	RNA_def_struct_ui_text(srna, "Int Definition", "RNA integer number property definition");
	rna_def_number_property(srna, PROP_INT);

	/* FloatProperty */
	srna= RNA_def_struct(brna, "FloatProperty", "Property");
	RNA_def_struct_ui_text(srna, "Float Definition", "RNA floating pointer number property definition");
	rna_def_number_property(srna, PROP_FLOAT);

	/* StringProperty */
	srna= RNA_def_struct(brna, "StringProperty", "Property");
	RNA_def_struct_ui_text(srna, "String Definition", "RNA text string property definition");
	rna_def_string_property(srna);

	/* EnumProperty */
	srna= RNA_def_struct(brna, "EnumProperty", "Property");
	RNA_def_struct_ui_text(srna, "Enum Definition", "RNA enumeration property definition, to choose from a number of predefined options");
	rna_def_enum_property(brna, srna);

	/* PointerProperty */
	srna= RNA_def_struct(brna, "PointerProperty", "Property");
	RNA_def_struct_ui_text(srna, "Pointer Definition", "RNA pointer property to point to another RNA struct");
	rna_def_pointer_property(srna, PROP_POINTER);

	/* CollectionProperty */
	srna= RNA_def_struct(brna, "CollectionProperty", "Property");
	RNA_def_struct_ui_text(srna, "Collection Definition", "RNA collection property to define lists, arrays and mappings");
	rna_def_pointer_property(srna, PROP_COLLECTION);
	
	/* Function */
	rna_def_function(brna);

	/* Blender RNA */
	srna= RNA_def_struct(brna, "BlenderRNA", NULL);
	RNA_def_struct_ui_text(srna, "Blender RNA", "Blender RNA structure definitions");
	RNA_def_struct_ui_icon(srna, ICON_RNA);

	prop= RNA_def_property(srna, "structs", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "Struct");
	RNA_def_property_collection_funcs(prop, "rna_BlenderRNA_structs_begin", "rna_iterator_listbase_next", "rna_iterator_listbase_end", "rna_iterator_listbase_get",
		/* included for speed, can be removed */
#if 0
			0,0,0);
#else
			"rna_BlenderRNA_structs_length", "rna_BlenderRNA_structs_lookup_int", "rna_BlenderRNA_structs_lookup_string");
#endif

	RNA_def_property_ui_text(prop, "Structs", "");
}

#endif

