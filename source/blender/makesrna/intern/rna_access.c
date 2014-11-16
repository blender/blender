/*
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

/** \file blender/makesrna/intern/rna_access.c
 *  \ingroup RNA
 */


#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_dynstr.h"
#include "BLI_ghash.h"
#include "BLI_math.h"

#include "BLF_api.h"
#include "BLF_translation.h"

#include "BKE_animsys.h"
#include "BKE_context.h"
#include "BKE_idcode.h"
#include "BKE_idprop.h"
#include "BKE_fcurve.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"

/* flush updates */
#include "DNA_object_types.h"
#include "BKE_depsgraph.h"
#include "WM_types.h"

#include "rna_internal.h"

const PointerRNA PointerRNA_NULL = {{NULL}};

/* Init/Exit */

void RNA_init(void)
{
	StructRNA *srna;
	PropertyRNA *prop;

	for (srna = BLENDER_RNA.structs.first; srna; srna = srna->cont.next) {
		if (!srna->cont.prophash) {
			srna->cont.prophash = BLI_ghash_str_new("RNA_init gh");

			for (prop = srna->cont.properties.first; prop; prop = prop->next)
				if (!(prop->flag & PROP_BUILTIN))
					BLI_ghash_insert(srna->cont.prophash, (void *)prop->identifier, prop);
		}
	}
}

void RNA_exit(void)
{
	StructRNA *srna;
	
	RNA_property_update_cache_free();
	
	for (srna = BLENDER_RNA.structs.first; srna; srna = srna->cont.next) {
		if (srna->cont.prophash) {
			BLI_ghash_free(srna->cont.prophash, NULL, NULL);
			srna->cont.prophash = NULL;
		}
	}

	RNA_free(&BLENDER_RNA);
}

/* Pointer */

void RNA_main_pointer_create(struct Main *main, PointerRNA *r_ptr)
{
	r_ptr->id.data = NULL;
	r_ptr->type = &RNA_BlendData;
	r_ptr->data = main;
}

void RNA_id_pointer_create(ID *id, PointerRNA *r_ptr)
{
	StructRNA *type, *idtype = NULL;

	if (id) {
		PointerRNA tmp = {{NULL}};
		tmp.data = id;
		idtype = rna_ID_refine(&tmp);
		
		while (idtype->refine) {
			type = idtype->refine(&tmp);

			if (type == idtype)
				break;
			else
				idtype = type;
		}
	}
	
	r_ptr->id.data = id;
	r_ptr->type = idtype;
	r_ptr->data = id;
}

void RNA_pointer_create(ID *id, StructRNA *type, void *data, PointerRNA *r_ptr)
{
#if 0 /* UNUSED */
	StructRNA *idtype = NULL;

	if (id) {
		PointerRNA tmp = {{0}};
		tmp.data = id;
		idtype = rna_ID_refine(&tmp);
	}
#endif

	r_ptr->id.data = id;
	r_ptr->type = type;
	r_ptr->data = data;

	if (data) {
		while (r_ptr->type && r_ptr->type->refine) {
			StructRNA *rtype = r_ptr->type->refine(r_ptr);

			if (rtype == r_ptr->type)
				break;
			else
				r_ptr->type = rtype;
		}
	}
}

static void rna_pointer_inherit_id(StructRNA *type, PointerRNA *parent, PointerRNA *ptr)
{
	if (type && type->flag & STRUCT_ID) {
		ptr->id.data = ptr->data;
	}
	else {
		ptr->id.data = parent->id.data;
	}
}

void RNA_blender_rna_pointer_create(PointerRNA *r_ptr)
{
	r_ptr->id.data = NULL;
	r_ptr->type = &RNA_BlenderRNA;
	r_ptr->data = &BLENDER_RNA;
}

PointerRNA rna_pointer_inherit_refine(PointerRNA *ptr, StructRNA *type, void *data)
{
	if (data) {
		PointerRNA result;
		result.data = data;
		result.type = type;
		rna_pointer_inherit_id(type, ptr, &result);

		while (result.type->refine) {
			type = result.type->refine(&result);

			if (type == result.type)
				break;
			else
				result.type = type;
		}
		return result;
	}
	else {
		return PointerRNA_NULL;
	}
}

/**/
void RNA_pointer_recast(PointerRNA *ptr, PointerRNA *r_ptr)
{
#if 0 /* works but this case if covered by more general code below. */
	if (RNA_struct_is_ID(ptr->type)) {
		/* simple case */
		RNA_id_pointer_create(ptr->id.data, r_ptr);
	}
	else
#endif
	{
		StructRNA *base;
		PointerRNA t_ptr;
		*r_ptr = *ptr; /* initialize as the same in case cant recast */

		for (base = ptr->type->base; base; base = base->base) {
			t_ptr = rna_pointer_inherit_refine(ptr, base, ptr->data);
			if (t_ptr.type && t_ptr.type != ptr->type) {
				*r_ptr = t_ptr;
			}
		}
	}
}

/* ID Properties */

static void rna_idproperty_touch(IDProperty *idprop)
{
	/* so the property is seen as 'set' by rna */
	idprop->flag &= ~IDP_FLAG_GHOST;
}

/* return a UI local ID prop definition for this prop */
static IDProperty *rna_idproperty_ui(PropertyRNA *prop)
{
	IDProperty *idprop;

	for (idprop = ((IDProperty *)prop)->prev; idprop; idprop = idprop->prev) {
		if (strcmp(RNA_IDP_UI, idprop->name) == 0)
			break;
	}

	if (idprop == NULL) {
		for (idprop = ((IDProperty *)prop)->next; idprop; idprop = idprop->next) {
			if (strcmp(RNA_IDP_UI, idprop->name) == 0)
				break;
		}
	}

	if (idprop) {
		return IDP_GetPropertyTypeFromGroup(idprop, ((IDProperty *)prop)->name, IDP_GROUP);
	}

	return NULL;
}

IDProperty *RNA_struct_idprops(PointerRNA *ptr, bool create)
{
	StructRNA *type = ptr->type;

	if (type && type->idproperties)
		return type->idproperties(ptr, create);
	
	return NULL;
}

bool RNA_struct_idprops_check(StructRNA *srna)
{
	return (srna && srna->idproperties);
}

static IDProperty *rna_idproperty_find(PointerRNA *ptr, const char *name)
{
	IDProperty *group = RNA_struct_idprops(ptr, 0);

	if (group)
		return IDP_GetPropertyFromGroup(group, name);

	return NULL;
}

static void rna_idproperty_free(PointerRNA *ptr, const char *name)
{
	IDProperty *group = RNA_struct_idprops(ptr, 0);

	if (group) {
		IDProperty *idprop = IDP_GetPropertyFromGroup(group, name);
		if (idprop) {
			IDP_FreeFromGroup(group, idprop);
		}
	}
}

static int rna_ensure_property_array_length(PointerRNA *ptr, PropertyRNA *prop)
{
	if (prop->magic == RNA_MAGIC) {
		int arraylen[RNA_MAX_ARRAY_DIMENSION];
		return (prop->getlength && ptr->data) ? prop->getlength(ptr, arraylen) : prop->totarraylength;
	}
	else {
		IDProperty *idprop = (IDProperty *)prop;

		if (idprop->type == IDP_ARRAY)
			return idprop->len;
		else
			return 0;
	}
}

static bool rna_ensure_property_array_check(PropertyRNA *prop)
{
	if (prop->magic == RNA_MAGIC) {
		return (prop->getlength || prop->totarraylength);
	}
	else {
		IDProperty *idprop = (IDProperty *)prop;

		return (idprop->type == IDP_ARRAY);
	}
}

static void rna_ensure_property_multi_array_length(PointerRNA *ptr, PropertyRNA *prop, int length[])
{
	if (prop->magic == RNA_MAGIC) {
		if (prop->getlength)
			prop->getlength(ptr, length);
		else
			memcpy(length, prop->arraylength, prop->arraydimension * sizeof(int));
	}
	else {
		IDProperty *idprop = (IDProperty *)prop;

		if (idprop->type == IDP_ARRAY)
			length[0] = idprop->len;
		else
			length[0] = 0;
	}
}

static bool rna_idproperty_verify_valid(PointerRNA *ptr, PropertyRNA *prop, IDProperty *idprop)
{
	/* this verifies if the idproperty actually matches the property
	 * description and otherwise removes it. this is to ensure that
	 * rna property access is type safe, e.g. if you defined the rna
	 * to have a certain array length you can count on that staying so */
	
	switch (idprop->type) {
		case IDP_IDPARRAY:
			if (prop->type != PROP_COLLECTION)
				return false;
			break;
		case IDP_ARRAY:
			if (rna_ensure_property_array_length(ptr, prop) != idprop->len)
				return false;

			if (idprop->subtype == IDP_FLOAT && prop->type != PROP_FLOAT)
				return false;
			if (idprop->subtype == IDP_INT && !ELEM(prop->type, PROP_BOOLEAN, PROP_INT, PROP_ENUM))
				return false;

			break;
		case IDP_INT:
			if (!ELEM(prop->type, PROP_BOOLEAN, PROP_INT, PROP_ENUM))
				return false;
			break;
		case IDP_FLOAT:
		case IDP_DOUBLE:
			if (prop->type != PROP_FLOAT)
				return false;
			break;
		case IDP_STRING:
			if (prop->type != PROP_STRING)
				return false;
			break;
		case IDP_GROUP:
			if (prop->type != PROP_POINTER)
				return false;
			break;
		default:
			return false;
	}

	return true;
}

static PropertyRNA *typemap[IDP_NUMTYPES] = {
	(PropertyRNA *)&rna_PropertyGroupItem_string,
	(PropertyRNA *)&rna_PropertyGroupItem_int,
	(PropertyRNA *)&rna_PropertyGroupItem_float,
	NULL, NULL, NULL,
	(PropertyRNA *)&rna_PropertyGroupItem_group, NULL,
	(PropertyRNA *)&rna_PropertyGroupItem_double,
	(PropertyRNA *)&rna_PropertyGroupItem_idp_array
};

static PropertyRNA *arraytypemap[IDP_NUMTYPES] = {
	NULL, (PropertyRNA *)&rna_PropertyGroupItem_int_array,
	(PropertyRNA *)&rna_PropertyGroupItem_float_array,
	NULL, NULL, NULL,
	(PropertyRNA *)&rna_PropertyGroupItem_collection, NULL,
	(PropertyRNA *)&rna_PropertyGroupItem_double_array
};

IDProperty *rna_idproperty_check(PropertyRNA **prop, PointerRNA *ptr)
{
	/* This is quite a hack, but avoids some complexity in the API. we
	 * pass IDProperty structs as PropertyRNA pointers to the outside.
	 * We store some bytes in PropertyRNA structs that allows us to
	 * distinguish it from IDProperty structs. If it is an ID property,
	 * we look up an IDP PropertyRNA based on the type, and set the data
	 * pointer to the IDProperty. */

	if ((*prop)->magic == RNA_MAGIC) {
		if ((*prop)->flag & PROP_IDPROPERTY) {
			IDProperty *idprop = rna_idproperty_find(ptr, (*prop)->identifier);

			if (idprop && !rna_idproperty_verify_valid(ptr, *prop, idprop)) {
				IDProperty *group = RNA_struct_idprops(ptr, 0);

				IDP_FreeFromGroup(group, idprop);
				return NULL;
			}

			return idprop;
		}
		else
			return NULL;
	}

	{
		IDProperty *idprop = (IDProperty *)(*prop);

		if (idprop->type == IDP_ARRAY)
			*prop = arraytypemap[(int)(idprop->subtype)];
		else
			*prop = typemap[(int)(idprop->type)];

		return idprop;
	}
}

static PropertyRNA *rna_ensure_property(PropertyRNA *prop)
{
	/* the quick version if we don't need the idproperty */

	if (prop->magic == RNA_MAGIC)
		return prop;

	{
		IDProperty *idprop = (IDProperty *)prop;

		if (idprop->type == IDP_ARRAY)
			return arraytypemap[(int)(idprop->subtype)];
		else
			return typemap[(int)(idprop->type)];
	}
}

static const char *rna_ensure_property_identifier(const PropertyRNA *prop)
{
	if (prop->magic == RNA_MAGIC)
		return prop->identifier;
	else
		return ((IDProperty *)prop)->name;
}

static const char *rna_ensure_property_description(PropertyRNA *prop)
{
	const char *description = NULL;

	if (prop->magic == RNA_MAGIC)
		description = prop->description;
	else {
		/* attempt to get the local ID values */
		IDProperty *idp_ui = rna_idproperty_ui(prop);

		if (idp_ui) {
			IDProperty *item = IDP_GetPropertyTypeFromGroup(idp_ui, "description", IDP_STRING);
			if (item)
				description = IDP_String(item);
		}

		if (description == NULL)
			description = ((IDProperty *)prop)->name;  /* XXX - not correct */
	}

	return description;
}

static const char *rna_ensure_property_name(const PropertyRNA *prop)
{
	const char *name;

	if (prop->magic == RNA_MAGIC)
		name = prop->name;
	else
		name = ((IDProperty *)prop)->name;

	return name;
}

/* Structs */

StructRNA *RNA_struct_find(const char *identifier)
{
	StructRNA *type;
	if (identifier) {
		for (type = BLENDER_RNA.structs.first; type; type = type->cont.next)
			if (strcmp(type->identifier, identifier) == 0)
				return type;
	}
	return NULL;
}

const char *RNA_struct_identifier(const StructRNA *type)
{
	return type->identifier;
}

const char *RNA_struct_ui_name(const StructRNA *type)
{
	return CTX_IFACE_(type->translation_context, type->name);
}

const char *RNA_struct_ui_name_raw(const StructRNA *type)
{
	return type->name;
}

int RNA_struct_ui_icon(const StructRNA *type)
{
	if (type)
		return type->icon;
	else
		return ICON_DOT;
}

const char *RNA_struct_ui_description(const StructRNA *type)
{
	return TIP_(type->description);
}

const char *RNA_struct_ui_description_raw(const StructRNA *type)
{
	return type->description;
}

const char *RNA_struct_translation_context(const StructRNA *type)
{
	return type->translation_context;
}

PropertyRNA *RNA_struct_name_property(StructRNA *type)
{
	return type->nameproperty;
}

PropertyRNA *RNA_struct_iterator_property(StructRNA *type)
{
	return type->iteratorproperty;
}

StructRNA *RNA_struct_base(StructRNA *type)
{
	return type->base;
}

bool RNA_struct_is_ID(const StructRNA *type)
{
	return (type->flag & STRUCT_ID) != 0;
}

bool RNA_struct_undo_check(const StructRNA *type)
{
	return (type->flag & STRUCT_UNDO) != 0;
}

bool RNA_struct_idprops_register_check(const StructRNA *type)
{
	return (type->flag & STRUCT_NO_IDPROPERTIES) == 0;
}

/* remove an id-property */
bool RNA_struct_idprops_unset(PointerRNA *ptr, const char *identifier)
{
	IDProperty *group = RNA_struct_idprops(ptr, 0);

	if (group) {
		IDProperty *idp = IDP_GetPropertyFromGroup(group, identifier);
		if (idp) {
			IDP_FreeFromGroup(group, idp);

			return true;
		}
	}
	return false;
}

bool RNA_struct_is_a(const StructRNA *type, const StructRNA *srna)
{
	const StructRNA *base;

	if (srna == &RNA_AnyType)
		return true;

	if (!type)
		return false;

	/* ptr->type is always maximally refined */
	for (base = type; base; base = base->base)
		if (base == srna)
			return true;
	
	return false;
}

PropertyRNA *RNA_struct_find_property(PointerRNA *ptr, const char *identifier)
{
	if (identifier[0] == '[' && identifier[1] == '"') { /* "  (dummy comment to avoid confusing some
		                                                 * function lists in text editors) */
		/* id prop lookup, not so common */
		PropertyRNA *r_prop = NULL;
		PointerRNA r_ptr; /* only support single level props */
		if (RNA_path_resolve(ptr, identifier, &r_ptr, &r_prop) && (r_ptr.type == ptr->type) && (r_ptr.data == ptr->data))
			return r_prop;
	}
	else {
		/* most common case */
		PropertyRNA *iterprop = RNA_struct_iterator_property(ptr->type);
		PointerRNA propptr;

		if (RNA_property_collection_lookup_string(ptr, iterprop, identifier, &propptr))
			return propptr.data;
	}
	
	return NULL;
}

/* Find the property which uses the given nested struct */
static PropertyRNA *RNA_struct_find_nested(PointerRNA *ptr, StructRNA *srna)
{
	PropertyRNA *prop = NULL;

	RNA_STRUCT_BEGIN (ptr, iprop)
	{
		/* This assumes that there can only be one user of this nested struct */
		if (RNA_property_pointer_type(ptr, iprop) == srna) {
			prop = iprop;
			break;
		}
	}
	RNA_PROP_END;

	return prop;
}

bool RNA_struct_contains_property(PointerRNA *ptr, PropertyRNA *prop_test)
{
	/* note, prop_test could be freed memory, only use for comparison */

	/* validate the RNA is ok */
	PropertyRNA *iterprop;
	bool found = false;

	iterprop = RNA_struct_iterator_property(ptr->type);

	RNA_PROP_BEGIN (ptr, itemptr, iterprop)
	{
		/* PropertyRNA *prop = itemptr.data; */
		if (prop_test == (PropertyRNA *)itemptr.data) {
			found = true;
			break;
		}
	}
	RNA_PROP_END;

	return found;
}

/* low level direct access to type->properties, note this ignores parent classes so should be used with care */
const struct ListBase *RNA_struct_type_properties(StructRNA *srna)
{
	return &srna->cont.properties;
}

PropertyRNA *RNA_struct_type_find_property(StructRNA *srna, const char *identifier)
{
	return BLI_findstring_ptr(&srna->cont.properties, identifier, offsetof(PropertyRNA, identifier));
}

FunctionRNA *RNA_struct_find_function(StructRNA *srna, const char *identifier)
{
#if 1
	FunctionRNA *func;
	StructRNA *type;
	for (type = srna; type; type = type->base) {
		func = (FunctionRNA *)BLI_findstring_ptr(&type->functions, identifier, offsetof(FunctionRNA, identifier));
		if (func) {
			return func;
		}
	}
	return NULL;

	/* funcitonal but slow */
#else
	PointerRNA tptr;
	PropertyRNA *iterprop;
	FunctionRNA *func;

	RNA_pointer_create(NULL, &RNA_Struct, srna, &tptr);
	iterprop = RNA_struct_find_property(&tptr, "functions");

	func = NULL;

	RNA_PROP_BEGIN (&tptr, funcptr, iterprop)
	{
		if (strcmp(identifier, RNA_function_identifier(funcptr.data)) == 0) {
			func = funcptr.data;
			break;
		}
	}
	RNA_PROP_END;

	return func;
#endif
}

const ListBase *RNA_struct_type_functions(StructRNA *srna)
{
	return &srna->functions;
}

StructRegisterFunc RNA_struct_register(StructRNA *type)
{
	return type->reg;
}

StructUnregisterFunc RNA_struct_unregister(StructRNA *type)
{
	do {
		if (type->unreg)
			return type->unreg;
	} while ((type = type->base));

	return NULL;
}

void **RNA_struct_instance(PointerRNA *ptr)
{
	StructRNA *type = ptr->type;

	do {
		if (type->instance)
			return type->instance(ptr);
	} while ((type = type->base));

	return NULL;
}

void *RNA_struct_py_type_get(StructRNA *srna)
{
	return srna->py_type;
}

void RNA_struct_py_type_set(StructRNA *srna, void *py_type)
{
	srna->py_type = py_type;
}

void *RNA_struct_blender_type_get(StructRNA *srna)
{
	return srna->blender_type;
}

void RNA_struct_blender_type_set(StructRNA *srna, void *blender_type)
{
	srna->blender_type = blender_type;
}

char *RNA_struct_name_get_alloc(PointerRNA *ptr, char *fixedbuf, int fixedlen, int *r_len)
{
	PropertyRNA *nameprop;

	if (ptr->data && (nameprop = RNA_struct_name_property(ptr->type)))
		return RNA_property_string_get_alloc(ptr, nameprop, fixedbuf, fixedlen, r_len);

	return NULL;
}

/* Property Information */

const char *RNA_property_identifier(PropertyRNA *prop)
{
	return rna_ensure_property_identifier(prop);
}

const char *RNA_property_description(PropertyRNA *prop)
{
	return TIP_(rna_ensure_property_description(prop));
}

PropertyType RNA_property_type(PropertyRNA *prop)
{
	return rna_ensure_property(prop)->type;
}

PropertySubType RNA_property_subtype(PropertyRNA *prop)
{
	return rna_ensure_property(prop)->subtype;
}

PropertyUnit RNA_property_unit(PropertyRNA *prop)
{
	return RNA_SUBTYPE_UNIT(rna_ensure_property(prop)->subtype);
}

int RNA_property_flag(PropertyRNA *prop)
{
	return rna_ensure_property(prop)->flag;
}

void *RNA_property_py_data_get(PropertyRNA *prop)
{
	return prop->py_data;
}

int RNA_property_array_length(PointerRNA *ptr, PropertyRNA *prop)
{
	return rna_ensure_property_array_length(ptr, prop);
}

bool RNA_property_array_check(PropertyRNA *prop)
{
	return rna_ensure_property_array_check(prop);
}

/* used by BPY to make an array from the python object */
int RNA_property_array_dimension(PointerRNA *ptr, PropertyRNA *prop, int length[])
{
	PropertyRNA *rprop = rna_ensure_property(prop);

	if (length)
		rna_ensure_property_multi_array_length(ptr, prop, length);

	return rprop->arraydimension;
}

/* Return the size of Nth dimension. */
int RNA_property_multi_array_length(PointerRNA *ptr, PropertyRNA *prop, int dim)
{
	int len[RNA_MAX_ARRAY_DIMENSION];

	rna_ensure_property_multi_array_length(ptr, prop, len);

	return len[dim];
}

char RNA_property_array_item_char(PropertyRNA *prop, int index)
{
	const char *vectoritem = "XYZW";
	const char *quatitem = "WXYZ";
	const char *coloritem = "RGBA";
	PropertySubType subtype = rna_ensure_property(prop)->subtype;

	BLI_assert(index >= 0);

	/* get string to use for array index */
	if ((index < 4) && ELEM(subtype, PROP_QUATERNION, PROP_AXISANGLE)) {
		return quatitem[index];
	}
	else if ((index < 4) && ELEM(subtype, PROP_TRANSLATION, PROP_DIRECTION, PROP_XYZ, PROP_XYZ_LENGTH,
	                             PROP_EULER, PROP_VELOCITY, PROP_ACCELERATION, PROP_COORDS))
	{
		return vectoritem[index];
	}
	else if ((index < 4) && ELEM(subtype, PROP_COLOR, PROP_COLOR_GAMMA)) {
		return coloritem[index];
	}

	return '\0';
}

int RNA_property_array_item_index(PropertyRNA *prop, char name)
{
	PropertySubType subtype = rna_ensure_property(prop)->subtype;

	/* get index based on string name/alias */
	/* maybe a function to find char index in string would be better than all the switches */
	if (ELEM(subtype, PROP_QUATERNION, PROP_AXISANGLE)) {
		switch (name) {
			case 'w':
				return 0;
			case 'x':
				return 1;
			case 'y':
				return 2;
			case 'z':
				return 3;
		}
	}
	else if (ELEM(subtype, PROP_TRANSLATION, PROP_DIRECTION, PROP_XYZ, PROP_XYZ_LENGTH,
	               PROP_EULER, PROP_VELOCITY, PROP_ACCELERATION))
	{
		switch (name) {
			case 'x':
				return 0;
			case 'y':
				return 1;
			case 'z':
				return 2;
			case 'w':
				return 3;
		}
	}
	else if (ELEM(subtype, PROP_COLOR, PROP_COLOR_GAMMA)) {
		switch (name) {
			case 'r':
				return 0;
			case 'g':
				return 1;
			case 'b':
				return 2;
			case 'a':
				return 3;
		}
	}

	return -1;
}


void RNA_property_int_range(PointerRNA *ptr, PropertyRNA *prop, int *hardmin, int *hardmax)
{
	IntPropertyRNA *iprop = (IntPropertyRNA *)rna_ensure_property(prop);
	int softmin, softmax;

	if (prop->magic != RNA_MAGIC) {
		/* attempt to get the local ID values */
		IDProperty *idp_ui = rna_idproperty_ui(prop);

		if (idp_ui) {
			IDProperty *item;

			item = IDP_GetPropertyTypeFromGroup(idp_ui, "min", IDP_INT);
			*hardmin = item ? IDP_Int(item) : INT_MIN;

			item = IDP_GetPropertyTypeFromGroup(idp_ui, "max", IDP_INT);
			*hardmax = item ? IDP_Int(item) : INT_MAX;

			return;
		}
	}

	if (iprop->range) {
		*hardmin = INT_MIN;
		*hardmax = INT_MAX;

		iprop->range(ptr, hardmin, hardmax, &softmin, &softmax);
	}
	else if (iprop->range_ex) {
		*hardmin = INT_MIN;
		*hardmax = INT_MAX;

		iprop->range_ex(ptr, prop, hardmin, hardmax, &softmin, &softmax);
	}
	else {
		*hardmin = iprop->hardmin;
		*hardmax = iprop->hardmax;
	}
}

void RNA_property_int_ui_range(PointerRNA *ptr, PropertyRNA *prop, int *softmin, int *softmax, int *step)
{
	IntPropertyRNA *iprop = (IntPropertyRNA *)rna_ensure_property(prop);
	int hardmin, hardmax;
	
	if (prop->magic != RNA_MAGIC) {
		/* attempt to get the local ID values */
		IDProperty *idp_ui = rna_idproperty_ui(prop);

		if (idp_ui) {
			IDProperty *item;

			item = IDP_GetPropertyTypeFromGroup(idp_ui, "soft_min", IDP_INT);
			*softmin = item ? IDP_Int(item) : INT_MIN;

			item = IDP_GetPropertyTypeFromGroup(idp_ui, "soft_max", IDP_INT);
			*softmax = item ? IDP_Int(item) : INT_MAX;

			item = IDP_GetPropertyTypeFromGroup(idp_ui, "step", IDP_INT);
			*step = item ? IDP_Int(item) : 1;

			return;
		}
	}

	*softmin = iprop->softmin;
	*softmax = iprop->softmax;

	if (iprop->range) {
		hardmin = INT_MIN;
		hardmax = INT_MAX;

		iprop->range(ptr, &hardmin, &hardmax, softmin, softmax);

		*softmin = max_ii(*softmin, hardmin);
		*softmax = min_ii(*softmax, hardmax);
	}
	else if (iprop->range_ex) {
		hardmin = INT_MIN;
		hardmax = INT_MAX;

		iprop->range_ex(ptr, prop, &hardmin, &hardmax, softmin, softmax);

		*softmin = max_ii(*softmin, hardmin);
		*softmax = min_ii(*softmax, hardmax);
	}

	*step = iprop->step;
}

void RNA_property_float_range(PointerRNA *ptr, PropertyRNA *prop, float *hardmin, float *hardmax)
{
	FloatPropertyRNA *fprop = (FloatPropertyRNA *)rna_ensure_property(prop);
	float softmin, softmax;

	if (prop->magic != RNA_MAGIC) {
		/* attempt to get the local ID values */
		IDProperty *idp_ui = rna_idproperty_ui(prop);

		if (idp_ui) {
			IDProperty *item;

			item = IDP_GetPropertyTypeFromGroup(idp_ui, "min", IDP_DOUBLE);
			*hardmin = item ? (float)IDP_Double(item) : -FLT_MAX;

			item = IDP_GetPropertyTypeFromGroup(idp_ui, "max", IDP_DOUBLE);
			*hardmax = item ? (float)IDP_Double(item) : FLT_MAX;

			return;
		}
	}

	if (fprop->range) {
		*hardmin = -FLT_MAX;
		*hardmax = FLT_MAX;

		fprop->range(ptr, hardmin, hardmax, &softmin, &softmax);
	}
	else if (fprop->range_ex) {
		*hardmin = -FLT_MAX;
		*hardmax = FLT_MAX;

		fprop->range_ex(ptr, prop, hardmin, hardmax, &softmin, &softmax);
	}
	else {
		*hardmin = fprop->hardmin;
		*hardmax = fprop->hardmax;
	}
}

void RNA_property_float_ui_range(PointerRNA *ptr, PropertyRNA *prop, float *softmin, float *softmax,
                                 float *step, float *precision)
{
	FloatPropertyRNA *fprop = (FloatPropertyRNA *)rna_ensure_property(prop);
	float hardmin, hardmax;

	if (prop->magic != RNA_MAGIC) {
		/* attempt to get the local ID values */
		IDProperty *idp_ui = rna_idproperty_ui(prop);

		if (idp_ui) {
			IDProperty *item;

			item = IDP_GetPropertyTypeFromGroup(idp_ui, "soft_min", IDP_DOUBLE);
			*softmin = item ? (float)IDP_Double(item) : -FLT_MAX;

			item = IDP_GetPropertyTypeFromGroup(idp_ui, "soft_max", IDP_DOUBLE);
			*softmax = item ? (float)IDP_Double(item) : FLT_MAX;

			item = IDP_GetPropertyTypeFromGroup(idp_ui, "step", IDP_DOUBLE);
			*step = item ? (float)IDP_Double(item) : 1.0f;

			item = IDP_GetPropertyTypeFromGroup(idp_ui, "precision", IDP_DOUBLE);
			*precision = item ? (float)IDP_Double(item) : 3.0f;

			return;
		}
	}

	*softmin = fprop->softmin;
	*softmax = fprop->softmax;

	if (fprop->range) {
		hardmin = -FLT_MAX;
		hardmax = FLT_MAX;

		fprop->range(ptr, &hardmin, &hardmax, softmin, softmax);

		*softmin = max_ff(*softmin, hardmin);
		*softmax = min_ff(*softmax, hardmax);
	}
	else if (fprop->range_ex) {
		hardmin = -FLT_MAX;
		hardmax = FLT_MAX;

		fprop->range_ex(ptr, prop, &hardmin, &hardmax, softmin, softmax);

		*softmin = max_ff(*softmin, hardmin);
		*softmax = min_ff(*softmax, hardmax);
	}

	*step = fprop->step;
	*precision = (float)fprop->precision;
}

int RNA_property_float_clamp(PointerRNA *ptr, PropertyRNA *prop, float *value)
{
	float min, max;

	RNA_property_float_range(ptr, prop, &min, &max);

	if (*value < min) {
		*value = min;
		return -1;
	}
	else if (*value > max) {
		*value = max;
		return 1;
	}
	else {
		return 0;
	}
}

int RNA_property_int_clamp(PointerRNA *ptr, PropertyRNA *prop, int *value)
{
	int min, max;

	RNA_property_int_range(ptr, prop, &min, &max);

	if (*value < min) {
		*value = min;
		return -1;
	}
	else if (*value > max) {
		*value = max;
		return 1;
	}
	else {
		return 0;
	}
}

/* this is the max length including \0 terminator.
 * '0' used when their is no maximum */
int RNA_property_string_maxlength(PropertyRNA *prop)
{
	StringPropertyRNA *sprop = (StringPropertyRNA *)rna_ensure_property(prop);
	return sprop->maxlength;
}

StructRNA *RNA_property_pointer_type(PointerRNA *ptr, PropertyRNA *prop)
{
	prop = rna_ensure_property(prop);

	if (prop->type == PROP_POINTER) {
		PointerPropertyRNA *pprop = (PointerPropertyRNA *)prop;

		if (pprop->typef)
			return pprop->typef(ptr);
		else if (pprop->type)
			return pprop->type;
	}
	else if (prop->type == PROP_COLLECTION) {
		CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)prop;

		if (cprop->item_type)
			return cprop->item_type;
	}
	/* ignore other types, RNA_struct_find_nested calls with unchecked props */

	return &RNA_UnknownType;
}

int RNA_property_pointer_poll(PointerRNA *ptr, PropertyRNA *prop, PointerRNA *value)
{
	prop = rna_ensure_property(prop);

	if (prop->type == PROP_POINTER) {
		PointerPropertyRNA *pprop = (PointerPropertyRNA *)prop;
		if (pprop->poll)
			return pprop->poll(ptr, *value);

		return 1;
	}

	printf("%s %s: is not a pointer property.\n", __func__, prop->identifier);
	return 0;
}

/* Reuse for dynamic types  */
EnumPropertyItem DummyRNA_NULL_items[] = {
	{0, NULL, 0, NULL, NULL}
};

/* Reuse for dynamic types with default value */
EnumPropertyItem DummyRNA_DEFAULT_items[] = {
	{0, "DEFAULT", 0, "Default", ""},
	{0, NULL, 0, NULL, NULL}
};

void RNA_property_enum_items(bContext *C, PointerRNA *ptr, PropertyRNA *prop, EnumPropertyItem **r_item,
                             int *r_totitem, bool *r_free)
{
	EnumPropertyRNA *eprop = (EnumPropertyRNA *)rna_ensure_property(prop);

	*r_free = false;

	if (eprop->itemf && (C != NULL || (prop->flag & PROP_ENUM_NO_CONTEXT))) {
		EnumPropertyItem *item;

		if (prop->flag & PROP_ENUM_NO_CONTEXT)
			item = eprop->itemf(NULL, ptr, prop, r_free);
		else
			item = eprop->itemf(C, ptr, prop, r_free);

		/* any callbacks returning NULL should be fixed */
		BLI_assert(item != NULL);

		if (r_totitem) {
			int tot;
			for (tot = 0; item[tot].identifier; tot++) {
				/* pass */
			}
			*r_totitem = tot;
		}

		*r_item = item;
	}
	else {
		*r_item = eprop->item;
		if (r_totitem)
			*r_totitem = eprop->totitem;
	}
}

#ifdef WITH_INTERNATIONAL
static void property_enum_translate(PropertyRNA *prop, EnumPropertyItem **r_item, int *r_totitem, bool *r_free)
{
	if (!(prop->flag & PROP_ENUM_NO_TRANSLATE)) {
		int i;

		/* Note: Only do those tests once, and then use BLF_pgettext. */
		bool do_iface = BLF_translate_iface();
		bool do_tooltip = BLF_translate_tooltips();
		EnumPropertyItem *nitem;

		if (!(do_iface || do_tooltip))
			return;

		if (*r_free) {
			nitem = *r_item;
		}
		else {
			EnumPropertyItem *item = *r_item;
			int tot;

			if (r_totitem) {
				tot = *r_totitem;
			}
			else {
				/* count */
				for (tot = 0; item[tot].identifier; tot++) {
					/* pass */
				}
			}

			nitem = MEM_mallocN(sizeof(EnumPropertyItem) * (tot + 1), "enum_items_gettexted");
			memcpy(nitem, item, sizeof(EnumPropertyItem) * (tot + 1));

			*r_free = true;
		}

		for (i = 0; nitem[i].identifier; i++) {
			if (nitem[i].name && do_iface) {
				nitem[i].name = BLF_pgettext(prop->translation_context, nitem[i].name);
			}
			if (nitem[i].description && do_tooltip) {
				nitem[i].description = BLF_pgettext(NULL, nitem[i].description);
			}
		}

		*r_item = nitem;
	}
}
#endif

void RNA_property_enum_items_gettexted(bContext *C, PointerRNA *ptr, PropertyRNA *prop,
                                       EnumPropertyItem **r_item, int *r_totitem, bool *r_free)
{
	RNA_property_enum_items(C, ptr, prop, r_item, r_totitem, r_free);

#ifdef WITH_INTERNATIONAL
	property_enum_translate(prop, r_item, r_totitem, r_free);
#endif
}

void RNA_property_enum_items_gettexted_all(bContext *C, PointerRNA *ptr, PropertyRNA *prop,
                                       EnumPropertyItem **r_item, int *r_totitem, bool *r_free)
{
	EnumPropertyRNA *eprop = (EnumPropertyRNA *)rna_ensure_property(prop);
	int mem_size = sizeof(EnumPropertyItem) * (eprop->totitem + 1);
	/* first return all items */
	*r_free = true;
	*r_item = MEM_mallocN(mem_size, "enum_gettext_all");
	 memcpy(*r_item, eprop->item, mem_size);

	if (r_totitem)
		*r_totitem = eprop->totitem;

	if (eprop->itemf && (C != NULL || (prop->flag & PROP_ENUM_NO_CONTEXT))) {
		EnumPropertyItem *item;
		int i;
		bool free = false;

		if (prop->flag & PROP_ENUM_NO_CONTEXT)
			item = eprop->itemf(NULL, ptr, prop, &free);
		else
			item = eprop->itemf(C, ptr, prop, &free);

		/* any callbacks returning NULL should be fixed */
		BLI_assert(item != NULL);

		for (i = 0; i < eprop->totitem; i++) {
			bool exists = false;
			int i_fixed;

			/* items that do not exist on list are returned, but have their names/identifiers NULLed out */
			for (i_fixed = 0; item[i_fixed].identifier; i_fixed++) {
				if (STREQ(item[i_fixed].identifier, (*r_item)[i].identifier)) {
					exists = true;
					break;
				}
			}

			if (!exists) {
				(*r_item)[i].name = NULL;
				(*r_item)[i].identifier = "";
			}
		}

		if (free)
			MEM_freeN(item);
	}

#ifdef WITH_INTERNATIONAL
	property_enum_translate(prop, r_item, r_totitem, r_free);
#endif
}

bool RNA_property_enum_value(bContext *C, PointerRNA *ptr, PropertyRNA *prop, const char *identifier, int *r_value)
{
	EnumPropertyItem *item;
	bool free;
	bool found;

	RNA_property_enum_items(C, ptr, prop, &item, NULL, &free);

	if (item) {
		const int i = RNA_enum_from_identifier(item, identifier);
		if (i != -1) {
			*r_value = item[i].value;
			found = true;
		}
		else {
			found = false;
		}

		if (free) {
			MEM_freeN(item);
		}
	}
	else {
		found = false;
	}
	return found;
}

bool RNA_enum_identifier(EnumPropertyItem *item, const int value, const char **r_identifier)
{
	const int i = RNA_enum_from_value(item, value);
	if (i != -1) {
		*r_identifier = item[i].identifier;
		return true;
	}
	else {
		return false;
	}
}

int RNA_enum_bitflag_identifiers(EnumPropertyItem *item, const int value, const char **r_identifier)
{
	int index = 0;
	for (; item->identifier; item++) {
		if (item->identifier[0] && item->value & value) {
			r_identifier[index++] = item->identifier;
		}
	}
	r_identifier[index] = NULL;
	return index;
}

bool RNA_enum_name(EnumPropertyItem *item, const int value, const char **r_name)
{
	const int i = RNA_enum_from_value(item, value);
	if (i != -1) {
		*r_name = item[i].name;
		return true;
	}
	else {
		return false;
	}
}

bool RNA_enum_description(EnumPropertyItem *item, const int value, const char **r_description)
{
	const int i = RNA_enum_from_value(item, value);
	if (i != -1) {
		*r_description = item[i].description;
		return true;
	}
	else {
		return false;
	}
}

int RNA_enum_from_identifier(EnumPropertyItem *item, const char *identifier)
{
	int i = 0;
	for (; item->identifier; item++, i++) {
		if (item->identifier[0] && STREQ(item->identifier, identifier)) {
			return i;
		}
	}
	return -1;
}

int RNA_enum_from_value(EnumPropertyItem *item, const int value)
{
	int i = 0;
	for (; item->identifier; item++, i++) {
		if (item->identifier[0] && item->value == value) {
			return i;
		}
	}
	return -1;
}

bool RNA_property_enum_identifier(bContext *C, PointerRNA *ptr, PropertyRNA *prop, const int value,
                                  const char **identifier)
{
	EnumPropertyItem *item = NULL;
	bool free;
	
	RNA_property_enum_items(C, ptr, prop, &item, NULL, &free);
	if (item) {
		bool result;
		result = RNA_enum_identifier(item, value, identifier);
		if (free)
			MEM_freeN(item);

		return result;
	}
	return false;
}

bool RNA_property_enum_name(bContext *C, PointerRNA *ptr, PropertyRNA *prop, const int value, const char **name)
{
	EnumPropertyItem *item = NULL;
	bool free;
	
	RNA_property_enum_items(C, ptr, prop, &item, NULL, &free);
	if (item) {
		bool result;
		result = RNA_enum_name(item, value, name);
		if (free)
			MEM_freeN(item);
		
		return result;
	}
	return false;
}

bool RNA_property_enum_name_gettexted(bContext *C, PointerRNA *ptr, PropertyRNA *prop, const int value, const char **name)
{
	bool result;

	result = RNA_property_enum_name(C, ptr, prop, value, name);

	if (result) {
		if (!(prop->flag & PROP_ENUM_NO_TRANSLATE)) {
			if (BLF_translate_iface()) {
				*name = BLF_pgettext(prop->translation_context, *name);
			}
		}
	}

	return result;
}

int RNA_property_enum_bitflag_identifiers(bContext *C, PointerRNA *ptr, PropertyRNA *prop, const int value,
                                          const char **identifier)
{
	EnumPropertyItem *item = NULL;
	bool free;

	RNA_property_enum_items(C, ptr, prop, &item, NULL, &free);
	if (item) {
		int result;
		result = RNA_enum_bitflag_identifiers(item, value, identifier);
		if (free)
			MEM_freeN(item);

		return result;
	}
	return 0;
}

const char *RNA_property_ui_name(PropertyRNA *prop)
{
	return CTX_IFACE_(prop->translation_context, rna_ensure_property_name(prop));
}

const char *RNA_property_ui_name_raw(PropertyRNA *prop)
{
	return rna_ensure_property_name(prop);
}

const char *RNA_property_ui_description(PropertyRNA *prop)
{
	return TIP_(rna_ensure_property_description(prop));
}

const char *RNA_property_ui_description_raw(PropertyRNA *prop)
{
	return rna_ensure_property_description(prop);
}

const char *RNA_property_translation_context(PropertyRNA *_prop)
{
	PropertyRNA *prop = rna_ensure_property(_prop);
	return prop->translation_context;
}

int RNA_property_ui_icon(PropertyRNA *prop)
{
	return rna_ensure_property(prop)->icon;
}

bool RNA_property_editable(PointerRNA *ptr, PropertyRNA *prop)
{
	ID *id = ptr->id.data;
	int flag;

	prop = rna_ensure_property(prop);
	flag = prop->editable ? prop->editable(ptr) : prop->flag;
	return (flag & PROP_EDITABLE) && (!id || !id->lib || (prop->flag & PROP_LIB_EXCEPTION));
}

bool RNA_property_editable_flag(PointerRNA *ptr, PropertyRNA *prop)
{
	int flag;

	prop = rna_ensure_property(prop);
	flag = prop->editable ? prop->editable(ptr) : prop->flag;
	return (flag & PROP_EDITABLE) != 0;
}

/* same as RNA_property_editable(), except this checks individual items in an array */
bool RNA_property_editable_index(PointerRNA *ptr, PropertyRNA *prop, int index)
{
	ID *id;
	int flag;

	BLI_assert(index >= 0);

	prop = rna_ensure_property(prop);

	flag = prop->flag;
	
	if (prop->editable)
		flag &= prop->editable(ptr);

	if (prop->itemeditable)
		flag &= prop->itemeditable(ptr, index);

	id = ptr->id.data;

	return (flag & PROP_EDITABLE) && (!id || !id->lib || (prop->flag & PROP_LIB_EXCEPTION));
}

bool RNA_property_animateable(PointerRNA *ptr, PropertyRNA *prop)
{
	/* check that base ID-block can support animation data */
	if (!id_type_can_have_animdata(ptr->id.data))
		return false;
	
	prop = rna_ensure_property(prop);

	if (!(prop->flag & PROP_ANIMATABLE))
		return false;

	return (prop->flag & PROP_EDITABLE) != 0;
}

bool RNA_property_animated(PointerRNA *ptr, PropertyRNA *prop)
{
	int len = 1, index;
	bool driven;

	if (!prop)
		return false;

	if (RNA_property_array_check(prop))
		len = RNA_property_array_length(ptr, prop);

	for (index = 0; index < len; index++) {
		if (rna_get_fcurve(ptr, prop, index, NULL, NULL, &driven))
			return true;
	}

	return false;
}

/* this function is to check if its possible to create a valid path from the ID
 * its slow so don't call in a loop */
bool RNA_property_path_from_ID_check(PointerRNA *ptr, PropertyRNA *prop)
{
	char *path = RNA_path_from_ID_to_property(ptr, prop);
	bool ret = false;

	if (path) {
		PointerRNA id_ptr;
		PointerRNA r_ptr;
		PropertyRNA *r_prop;

		RNA_id_pointer_create(ptr->id.data, &id_ptr);
		if (RNA_path_resolve(&id_ptr, path, &r_ptr, &r_prop) == true) {
			ret = (prop == r_prop);
		}
		MEM_freeN(path);
	}

	return ret;
}


static void rna_property_update(bContext *C, Main *bmain, Scene *scene, PointerRNA *ptr, PropertyRNA *prop)
{
	const bool is_rna = (prop->magic == RNA_MAGIC);
	prop = rna_ensure_property(prop);

	if (is_rna) {
		if (prop->update) {
			/* ideally no context would be needed for update, but there's some
			 * parts of the code that need it still, so we have this exception */
			if (prop->flag & PROP_CONTEXT_UPDATE) {
				if (C) {
					if (prop->flag & PROP_CONTEXT_PROPERTY_UPDATE) {
						((ContextPropUpdateFunc)prop->update)(C, ptr, prop);
					}
					else {
						((ContextUpdateFunc)prop->update)(C, ptr);
					}
				}
			}
			else
				prop->update(bmain, scene, ptr);
		}
		if (prop->noteflag)
			WM_main_add_notifier(prop->noteflag, ptr->id.data);
	}
	
	if (!is_rna || (prop->flag & PROP_IDPROPERTY)) {
		/* WARNING! This is so property drivers update the display!
		 * not especially nice  */
		DAG_id_tag_update(ptr->id.data, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
		WM_main_add_notifier(NC_WINDOW, NULL);
	}
}

/* must keep in sync with 'rna_property_update'
 * note, its possible this returns a false positive in the case of PROP_CONTEXT_UPDATE
 * but this isn't likely to be a performance problem. */
bool RNA_property_update_check(PropertyRNA *prop)
{
	return (prop->magic != RNA_MAGIC || prop->update || prop->noteflag);
}

void RNA_property_update(bContext *C, PointerRNA *ptr, PropertyRNA *prop)
{
	rna_property_update(C, CTX_data_main(C), CTX_data_scene(C), ptr, prop);
}

void RNA_property_update_main(Main *bmain, Scene *scene, PointerRNA *ptr, PropertyRNA *prop)
{
	rna_property_update(NULL, bmain, scene, ptr, prop);
}


/* RNA Updates Cache ------------------------ */
/* Overview of RNA Update cache system:
 *
 * RNA Update calls need to be cached in order to maintain reasonable performance
 * of the animation system (i.e. maintaining a somewhat interactive framerate)
 * while still allowing updates to be called (necessary in particular for modifier
 * property updates to actually work).
 *
 * The cache is structured with a dual-layer structure
 * - L1 = PointerRNA used as key; id.data is used (it should always be defined,
 *        and most updates end up using just that anyways)
 * - L2 = Update functions to be called on those PointerRNA's
 */

/* cache element */
typedef struct tRnaUpdateCacheElem {
	struct tRnaUpdateCacheElem *next, *prev;
	
	PointerRNA ptr;     /* L1 key - id as primary, data secondary/ignored? */
	ListBase L2Funcs;   /* L2 functions (LinkData<RnaUpdateFuncRef>) */
} tRnaUpdateCacheElem;

/* cache global (tRnaUpdateCacheElem's) - only accessible using these API calls */
static ListBase rna_updates_cache = {NULL, NULL};

/* ........................... */

void RNA_property_update_cache_add(PointerRNA *ptr, PropertyRNA *prop)
{
	const bool is_rna = (prop->magic == RNA_MAGIC);
	tRnaUpdateCacheElem *uce = NULL;
	UpdateFunc fn = NULL;
	LinkData *ld;
	
	/* sanity check */
	if (NULL == ptr)
		return;
		
	prop = rna_ensure_property(prop);
	
	/* we can only handle update calls with no context args for now (makes animsys updates easier) */
	if ((is_rna == false) || (prop->update == NULL) || (prop->flag & PROP_CONTEXT_UPDATE))
		return;
	fn = prop->update;
		
	/* find cache element for which key matches... */
	for (uce = rna_updates_cache.first; uce; uce = uce->next) {
		/* just match by id only for now, since most update calls that we'll encounter only really care about this */
		/* TODO: later, the cache might need to have some nesting on L1 to cope better
		 * with these problems + some tagging to indicate we need this */
		if (uce->ptr.id.data == ptr->id.data)
			break;
	}
	if (uce == NULL) {
		/* create new instance */
		uce = MEM_callocN(sizeof(tRnaUpdateCacheElem), "tRnaUpdateCacheElem");
		BLI_addtail(&rna_updates_cache, uce);
		
		/* copy pointer */
		RNA_pointer_create(ptr->id.data, ptr->type, ptr->data, &uce->ptr);
	}
	
	/* check on the update func */
	for (ld = uce->L2Funcs.first; ld; ld = ld->next) {
		/* stop on match - function already cached */
		if (fn == ld->data)
			return;
	}
	/* else... if still here, we need to add it */
	BLI_addtail(&uce->L2Funcs, BLI_genericNodeN(fn));
}

void RNA_property_update_cache_flush(Main *bmain, Scene *scene)
{
	tRnaUpdateCacheElem *uce;
	
	/* TODO: should we check that bmain and scene are valid? The above stuff doesn't! */
	
	/* execute the cached updates */
	for (uce = rna_updates_cache.first; uce; uce = uce->next) {
		LinkData *ld;
		
		for (ld = uce->L2Funcs.first; ld; ld = ld->next) {
			UpdateFunc fn = (UpdateFunc)ld->data;
			fn(bmain, scene, &uce->ptr);
		}
	}
}

void RNA_property_update_cache_free(void)
{
	tRnaUpdateCacheElem *uce, *ucn;
	
	for (uce = rna_updates_cache.first; uce; uce = ucn) {
		ucn = uce->next;
		
		/* free L2 cache */
		BLI_freelistN(&uce->L2Funcs);
		
		/* remove self */
		BLI_freelinkN(&rna_updates_cache, uce);
	}
}

/* ---------------------------------------------------------------------- */

/* Property Data */

int RNA_property_boolean_get(PointerRNA *ptr, PropertyRNA *prop)
{
	BoolPropertyRNA *bprop = (BoolPropertyRNA *)prop;
	IDProperty *idprop;

	BLI_assert(RNA_property_type(prop) == PROP_BOOLEAN);
	BLI_assert(RNA_property_array_check(prop) == false);

	if ((idprop = rna_idproperty_check(&prop, ptr)))
		return IDP_Int(idprop);
	else if (bprop->get)
		return bprop->get(ptr);
	else if (bprop->get_ex)
		return bprop->get_ex(ptr, prop);
	else
		return bprop->defaultvalue;
}

void RNA_property_boolean_set(PointerRNA *ptr, PropertyRNA *prop, int value)
{
	BoolPropertyRNA *bprop = (BoolPropertyRNA *)prop;
	IDProperty *idprop;

	BLI_assert(RNA_property_type(prop) == PROP_BOOLEAN);
	BLI_assert(RNA_property_array_check(prop) == false);

	/* just in case other values are passed */
	if (value) value = 1;

	if ((idprop = rna_idproperty_check(&prop, ptr))) {
		IDP_Int(idprop) = value;
		rna_idproperty_touch(idprop);
	}
	else if (bprop->set) {
		bprop->set(ptr, value);
	}
	else if (bprop->set_ex) {
		bprop->set_ex(ptr, prop, value);
	}
	else if (prop->flag & PROP_EDITABLE) {
		IDPropertyTemplate val = {0};
		IDProperty *group;

		val.i = value;

		group = RNA_struct_idprops(ptr, 1);
		if (group)
			IDP_AddToGroup(group, IDP_New(IDP_INT, &val, prop->identifier));
	}
}

void RNA_property_boolean_get_array(PointerRNA *ptr, PropertyRNA *prop, int *values)
{
	BoolPropertyRNA *bprop = (BoolPropertyRNA *)prop;
	IDProperty *idprop;

	BLI_assert(RNA_property_type(prop) == PROP_BOOLEAN);
	BLI_assert(RNA_property_array_check(prop) != false);

	if ((idprop = rna_idproperty_check(&prop, ptr))) {
		if (prop->arraydimension == 0)
			values[0] = RNA_property_boolean_get(ptr, prop);
		else
			memcpy(values, IDP_Array(idprop), sizeof(int) * idprop->len);
	}
	else if (prop->arraydimension == 0)
		values[0] = RNA_property_boolean_get(ptr, prop);
	else if (bprop->getarray)
		bprop->getarray(ptr, values);
	else if (bprop->getarray_ex)
		bprop->getarray_ex(ptr, prop, values);
	else if (bprop->defaultarray)
		memcpy(values, bprop->defaultarray, sizeof(int) * prop->totarraylength);
	else
		memset(values, 0, sizeof(int) * prop->totarraylength);
}

int RNA_property_boolean_get_index(PointerRNA *ptr, PropertyRNA *prop, int index)
{
	int tmp[RNA_MAX_ARRAY_LENGTH];
	int len = rna_ensure_property_array_length(ptr, prop);

	BLI_assert(RNA_property_type(prop) == PROP_BOOLEAN);
	BLI_assert(RNA_property_array_check(prop) != false);
	BLI_assert(index >= 0);

	if (len <= RNA_MAX_ARRAY_LENGTH) {
		RNA_property_boolean_get_array(ptr, prop, tmp);
		return tmp[index];
	}
	else {
		int *tmparray, value;

		tmparray = MEM_callocN(sizeof(int) * len, "RNA_property_boolean_get_index");
		RNA_property_boolean_get_array(ptr, prop, tmparray);
		value = tmparray[index];
		MEM_freeN(tmparray);

		return value;
	}
}

void RNA_property_boolean_set_array(PointerRNA *ptr, PropertyRNA *prop, const int *values)
{
	BoolPropertyRNA *bprop = (BoolPropertyRNA *)prop;
	IDProperty *idprop;

	BLI_assert(RNA_property_type(prop) == PROP_BOOLEAN);
	BLI_assert(RNA_property_array_check(prop) != false);

	if ((idprop = rna_idproperty_check(&prop, ptr))) {
		if (prop->arraydimension == 0)
			IDP_Int(idprop) = values[0];
		else
			memcpy(IDP_Array(idprop), values, sizeof(int) * idprop->len);

		rna_idproperty_touch(idprop);
	}
	else if (prop->arraydimension == 0)
		RNA_property_boolean_set(ptr, prop, values[0]);
	else if (bprop->setarray)
		bprop->setarray(ptr, values);
	else if (bprop->setarray_ex)
		bprop->setarray_ex(ptr, prop, values);
	else if (prop->flag & PROP_EDITABLE) {
		IDPropertyTemplate val = {0};
		IDProperty *group;

		val.array.len = prop->totarraylength;
		val.array.type = IDP_INT;

		group = RNA_struct_idprops(ptr, 1);
		if (group) {
			idprop = IDP_New(IDP_ARRAY, &val, prop->identifier);
			IDP_AddToGroup(group, idprop);
			memcpy(IDP_Array(idprop), values, sizeof(int) * idprop->len);
		}
	}
}

void RNA_property_boolean_set_index(PointerRNA *ptr, PropertyRNA *prop, int index, int value)
{
	int tmp[RNA_MAX_ARRAY_LENGTH];
	int len = rna_ensure_property_array_length(ptr, prop);

	BLI_assert(RNA_property_type(prop) == PROP_BOOLEAN);
	BLI_assert(RNA_property_array_check(prop) != false);
	BLI_assert(index >= 0);

	if (len <= RNA_MAX_ARRAY_LENGTH) {
		RNA_property_boolean_get_array(ptr, prop, tmp);
		tmp[index] = value;
		RNA_property_boolean_set_array(ptr, prop, tmp);
	}
	else {
		int *tmparray;

		tmparray = MEM_callocN(sizeof(int) * len, "RNA_property_boolean_get_index");
		RNA_property_boolean_get_array(ptr, prop, tmparray);
		tmparray[index] = value;
		RNA_property_boolean_set_array(ptr, prop, tmparray);
		MEM_freeN(tmparray);
	}
}

int RNA_property_boolean_get_default(PointerRNA *UNUSED(ptr), PropertyRNA *prop)
{
	BoolPropertyRNA *bprop = (BoolPropertyRNA *)rna_ensure_property(prop);

	BLI_assert(RNA_property_type(prop) == PROP_BOOLEAN);
	BLI_assert(RNA_property_array_check(prop) == false);

	return bprop->defaultvalue;
}

void RNA_property_boolean_get_default_array(PointerRNA *UNUSED(ptr), PropertyRNA *prop, int *values)
{
	BoolPropertyRNA *bprop = (BoolPropertyRNA *)rna_ensure_property(prop);
	
	BLI_assert(RNA_property_type(prop) == PROP_BOOLEAN);
	BLI_assert(RNA_property_array_check(prop) != false);

	if (prop->arraydimension == 0)
		values[0] = bprop->defaultvalue;
	else if (bprop->defaultarray)
		memcpy(values, bprop->defaultarray, sizeof(int) * prop->totarraylength);
	else
		memset(values, 0, sizeof(int) * prop->totarraylength);
}

int RNA_property_boolean_get_default_index(PointerRNA *ptr, PropertyRNA *prop, int index)
{
	int tmp[RNA_MAX_ARRAY_LENGTH];
	int len = rna_ensure_property_array_length(ptr, prop);

	BLI_assert(RNA_property_type(prop) == PROP_BOOLEAN);
	BLI_assert(RNA_property_array_check(prop) != false);
	BLI_assert(index >= 0);

	if (len <= RNA_MAX_ARRAY_LENGTH) {
		RNA_property_boolean_get_default_array(ptr, prop, tmp);
		return tmp[index];
	}
	else {
		int *tmparray, value;

		tmparray = MEM_callocN(sizeof(int) * len, "RNA_property_boolean_get_default_index");
		RNA_property_boolean_get_default_array(ptr, prop, tmparray);
		value = tmparray[index];
		MEM_freeN(tmparray);

		return value;
	}
}

int RNA_property_int_get(PointerRNA *ptr, PropertyRNA *prop)
{
	IntPropertyRNA *iprop = (IntPropertyRNA *)prop;
	IDProperty *idprop;

	BLI_assert(RNA_property_type(prop) == PROP_INT);
	BLI_assert(RNA_property_array_check(prop) == false);

	if ((idprop = rna_idproperty_check(&prop, ptr)))
		return IDP_Int(idprop);
	else if (iprop->get)
		return iprop->get(ptr);
	else if (iprop->get_ex)
		return iprop->get_ex(ptr, prop);
	else
		return iprop->defaultvalue;
}

void RNA_property_int_set(PointerRNA *ptr, PropertyRNA *prop, int value)
{
	IntPropertyRNA *iprop = (IntPropertyRNA *)prop;
	IDProperty *idprop;

	BLI_assert(RNA_property_type(prop) == PROP_INT);
	BLI_assert(RNA_property_array_check(prop) == false);
	/* useful to check on bad values but set function should clamp */
	/* BLI_assert(RNA_property_int_clamp(ptr, prop, &value) == 0); */

	if ((idprop = rna_idproperty_check(&prop, ptr))) {
		IDP_Int(idprop) = value;
		rna_idproperty_touch(idprop);
	}
	else if (iprop->set)
		iprop->set(ptr, value);
	else if (iprop->set_ex)
		iprop->set_ex(ptr, prop, value);
	else if (prop->flag & PROP_EDITABLE) {
		IDPropertyTemplate val = {0};
		IDProperty *group;

		RNA_property_int_clamp(ptr, prop, &value);

		val.i = value;

		group = RNA_struct_idprops(ptr, 1);
		if (group)
			IDP_AddToGroup(group, IDP_New(IDP_INT, &val, prop->identifier));
	}
}

void RNA_property_int_get_array(PointerRNA *ptr, PropertyRNA *prop, int *values)
{
	IntPropertyRNA *iprop = (IntPropertyRNA *)prop;
	IDProperty *idprop;

	BLI_assert(RNA_property_type(prop) == PROP_INT);
	BLI_assert(RNA_property_array_check(prop) != false);

	if ((idprop = rna_idproperty_check(&prop, ptr))) {
		BLI_assert(idprop->len == RNA_property_array_length(ptr, prop) || (prop->flag & PROP_IDPROPERTY));
		if (prop->arraydimension == 0)
			values[0] = RNA_property_int_get(ptr, prop);
		else
			memcpy(values, IDP_Array(idprop), sizeof(int) * idprop->len);
	}
	else if (prop->arraydimension == 0)
		values[0] = RNA_property_int_get(ptr, prop);
	else if (iprop->getarray)
		iprop->getarray(ptr, values);
	else if (iprop->getarray_ex)
		iprop->getarray_ex(ptr, prop, values);
	else if (iprop->defaultarray)
		memcpy(values, iprop->defaultarray, sizeof(int) * prop->totarraylength);
	else
		memset(values, 0, sizeof(int) * prop->totarraylength);
}

void RNA_property_int_get_array_range(PointerRNA *ptr, PropertyRNA *prop, int values[2])
{
	const int array_len = RNA_property_array_length(ptr, prop);

	if (array_len <= 0) {
		values[0] = 0;
		values[1] = 0;
	}
	else if (array_len == 1) {
		RNA_property_int_get_array(ptr, prop, values);
		values[1] = values[0];
	}
	else {
		int arr_stack[32];
		int *arr;
		int i;

		if (array_len > 32) {
			arr = MEM_mallocN(sizeof(int) * array_len, "RNA_property_int_get_array_range");
		}
		else {
			arr = arr_stack;
		}

		RNA_property_int_get_array(ptr, prop, arr);
		values[0] = values[1] = arr[0];
		for (i = 1; i < array_len; i++) {
			values[0] = MIN2(values[0], arr[i]);
			values[1] = MAX2(values[1], arr[i]);
		}

		if (arr != arr_stack) {
			MEM_freeN(arr);
		}
	}
}

int RNA_property_int_get_index(PointerRNA *ptr, PropertyRNA *prop, int index)
{
	int tmp[RNA_MAX_ARRAY_LENGTH];
	int len = rna_ensure_property_array_length(ptr, prop);

	BLI_assert(RNA_property_type(prop) == PROP_INT);
	BLI_assert(RNA_property_array_check(prop) != false);
	BLI_assert(index >= 0);

	if (len <= RNA_MAX_ARRAY_LENGTH) {
		RNA_property_int_get_array(ptr, prop, tmp);
		return tmp[index];
	}
	else {
		int *tmparray, value;

		tmparray = MEM_callocN(sizeof(int) * len, "RNA_property_int_get_index");
		RNA_property_int_get_array(ptr, prop, tmparray);
		value = tmparray[index];
		MEM_freeN(tmparray);

		return value;
	}
}

void RNA_property_int_set_array(PointerRNA *ptr, PropertyRNA *prop, const int *values)
{
	IntPropertyRNA *iprop = (IntPropertyRNA *)prop;
	IDProperty *idprop;

	BLI_assert(RNA_property_type(prop) == PROP_INT);
	BLI_assert(RNA_property_array_check(prop) != false);

	if ((idprop = rna_idproperty_check(&prop, ptr))) {
		BLI_assert(idprop->len == RNA_property_array_length(ptr, prop) || (prop->flag & PROP_IDPROPERTY));
		if (prop->arraydimension == 0)
			IDP_Int(idprop) = values[0];
		else
			memcpy(IDP_Array(idprop), values, sizeof(int) * idprop->len);

		rna_idproperty_touch(idprop);
	}
	else if (prop->arraydimension == 0)
		RNA_property_int_set(ptr, prop, values[0]);
	else if (iprop->setarray)
		iprop->setarray(ptr, values);
	else if (iprop->setarray_ex)
		iprop->setarray_ex(ptr, prop, values);
	else if (prop->flag & PROP_EDITABLE) {
		IDPropertyTemplate val = {0};
		IDProperty *group;

		/* TODO: RNA_property_int_clamp_array(ptr, prop, &value); */

		val.array.len = prop->totarraylength;
		val.array.type = IDP_INT;

		group = RNA_struct_idprops(ptr, 1);
		if (group) {
			idprop = IDP_New(IDP_ARRAY, &val, prop->identifier);
			IDP_AddToGroup(group, idprop);
			memcpy(IDP_Array(idprop), values, sizeof(int) * idprop->len);
		}
	}
}

void RNA_property_int_set_index(PointerRNA *ptr, PropertyRNA *prop, int index, int value)
{
	int tmp[RNA_MAX_ARRAY_LENGTH];
	int len = rna_ensure_property_array_length(ptr, prop);

	BLI_assert(RNA_property_type(prop) == PROP_INT);
	BLI_assert(RNA_property_array_check(prop) != false);
	BLI_assert(index >= 0);

	if (len <= RNA_MAX_ARRAY_LENGTH) {
		RNA_property_int_get_array(ptr, prop, tmp);
		tmp[index] = value;
		RNA_property_int_set_array(ptr, prop, tmp);
	}
	else {
		int *tmparray;

		tmparray = MEM_callocN(sizeof(int) * len, "RNA_property_int_get_index");
		RNA_property_int_get_array(ptr, prop, tmparray);
		tmparray[index] = value;
		RNA_property_int_set_array(ptr, prop, tmparray);
		MEM_freeN(tmparray);
	}
}

int RNA_property_int_get_default(PointerRNA *UNUSED(ptr), PropertyRNA *prop)
{
	IntPropertyRNA *iprop = (IntPropertyRNA *)rna_ensure_property(prop);
	return iprop->defaultvalue;
}

void RNA_property_int_get_default_array(PointerRNA *UNUSED(ptr), PropertyRNA *prop, int *values)
{
	IntPropertyRNA *iprop = (IntPropertyRNA *)rna_ensure_property(prop);
	
	BLI_assert(RNA_property_type(prop) == PROP_INT);
	BLI_assert(RNA_property_array_check(prop) != false);

	if (prop->arraydimension == 0)
		values[0] = iprop->defaultvalue;
	else if (iprop->defaultarray)
		memcpy(values, iprop->defaultarray, sizeof(int) * prop->totarraylength);
	else
		memset(values, 0, sizeof(int) * prop->totarraylength);
}

int RNA_property_int_get_default_index(PointerRNA *ptr, PropertyRNA *prop, int index)
{
	int tmp[RNA_MAX_ARRAY_LENGTH];
	int len = rna_ensure_property_array_length(ptr, prop);

	BLI_assert(RNA_property_type(prop) == PROP_INT);
	BLI_assert(RNA_property_array_check(prop) != false);
	BLI_assert(index >= 0);

	if (len <= RNA_MAX_ARRAY_LENGTH) {
		RNA_property_int_get_default_array(ptr, prop, tmp);
		return tmp[index];
	}
	else {
		int *tmparray, value;

		tmparray = MEM_callocN(sizeof(int) * len, "RNA_property_int_get_default_index");
		RNA_property_int_get_default_array(ptr, prop, tmparray);
		value = tmparray[index];
		MEM_freeN(tmparray);

		return value;
	}
}

float RNA_property_float_get(PointerRNA *ptr, PropertyRNA *prop)
{
	FloatPropertyRNA *fprop = (FloatPropertyRNA *)prop;
	IDProperty *idprop;

	BLI_assert(RNA_property_type(prop) == PROP_FLOAT);
	BLI_assert(RNA_property_array_check(prop) == false);

	if ((idprop = rna_idproperty_check(&prop, ptr))) {
		if (idprop->type == IDP_FLOAT)
			return IDP_Float(idprop);
		else
			return (float)IDP_Double(idprop);
	}
	else if (fprop->get)
		return fprop->get(ptr);
	else if (fprop->get_ex)
		return fprop->get_ex(ptr, prop);
	else
		return fprop->defaultvalue;
}

void RNA_property_float_set(PointerRNA *ptr, PropertyRNA *prop, float value)
{
	FloatPropertyRNA *fprop = (FloatPropertyRNA *)prop;
	IDProperty *idprop;

	BLI_assert(RNA_property_type(prop) == PROP_FLOAT);
	BLI_assert(RNA_property_array_check(prop) == false);
	/* useful to check on bad values but set function should clamp */
	/* BLI_assert(RNA_property_float_clamp(ptr, prop, &value) == 0); */

	if ((idprop = rna_idproperty_check(&prop, ptr))) {
		if (idprop->type == IDP_FLOAT)
			IDP_Float(idprop) = value;
		else
			IDP_Double(idprop) = value;

		rna_idproperty_touch(idprop);
	}
	else if (fprop->set) {
		fprop->set(ptr, value);
	}
	else if (fprop->set_ex) {
		fprop->set_ex(ptr, prop, value);
	}
	else if (prop->flag & PROP_EDITABLE) {
		IDPropertyTemplate val = {0};
		IDProperty *group;

		RNA_property_float_clamp(ptr, prop, &value);

		val.f = value;

		group = RNA_struct_idprops(ptr, 1);
		if (group)
			IDP_AddToGroup(group, IDP_New(IDP_FLOAT, &val, prop->identifier));
	}
}

void RNA_property_float_get_array(PointerRNA *ptr, PropertyRNA *prop, float *values)
{
	FloatPropertyRNA *fprop = (FloatPropertyRNA *)prop;
	IDProperty *idprop;
	int i;

	BLI_assert(RNA_property_type(prop) == PROP_FLOAT);
	BLI_assert(RNA_property_array_check(prop) != false);

	if ((idprop = rna_idproperty_check(&prop, ptr))) {
		BLI_assert(idprop->len == RNA_property_array_length(ptr, prop) || (prop->flag & PROP_IDPROPERTY));
		if (prop->arraydimension == 0)
			values[0] = RNA_property_float_get(ptr, prop);
		else if (idprop->subtype == IDP_FLOAT) {
			memcpy(values, IDP_Array(idprop), sizeof(float) * idprop->len);
		}
		else {
			for (i = 0; i < idprop->len; i++)
				values[i] =  (float)(((double *)IDP_Array(idprop))[i]);
		}
	}
	else if (prop->arraydimension == 0)
		values[0] = RNA_property_float_get(ptr, prop);
	else if (fprop->getarray)
		fprop->getarray(ptr, values);
	else if (fprop->getarray_ex)
		fprop->getarray_ex(ptr, prop, values);
	else if (fprop->defaultarray)
		memcpy(values, fprop->defaultarray, sizeof(float) * prop->totarraylength);
	else
		memset(values, 0, sizeof(float) * prop->totarraylength);
}

void RNA_property_float_get_array_range(PointerRNA *ptr, PropertyRNA *prop, float values[2])
{
	const int array_len = RNA_property_array_length(ptr, prop);

	if (array_len <= 0) {
		values[0] = 0.0f;
		values[1] = 0.0f;
	}
	else if (array_len == 1) {
		RNA_property_float_get_array(ptr, prop, values);
		values[1] = values[0];
	}
	else {
		float arr_stack[32];
		float *arr;
		int i;

		if (array_len > 32) {
			arr = MEM_mallocN(sizeof(float) * array_len, "RNA_property_float_get_array_range");
		}
		else {
			arr = arr_stack;
		}

		RNA_property_float_get_array(ptr, prop, arr);
		values[0] = values[1] = arr[0];
		for (i = 1; i < array_len; i++) {
			values[0] = MIN2(values[0], arr[i]);
			values[1] = MAX2(values[1], arr[i]);
		}

		if (arr != arr_stack) {
			MEM_freeN(arr);
		}
	}
}

float RNA_property_float_get_index(PointerRNA *ptr, PropertyRNA *prop, int index)
{
	float tmp[RNA_MAX_ARRAY_LENGTH];
	int len = rna_ensure_property_array_length(ptr, prop);

	BLI_assert(RNA_property_type(prop) == PROP_FLOAT);
	BLI_assert(RNA_property_array_check(prop) != false);
	BLI_assert(index >= 0);

	if (len <= RNA_MAX_ARRAY_LENGTH) {
		RNA_property_float_get_array(ptr, prop, tmp);
		return tmp[index];
	}
	else {
		float *tmparray, value;

		tmparray = MEM_callocN(sizeof(float) * len, "RNA_property_float_get_index");
		RNA_property_float_get_array(ptr, prop, tmparray);
		value = tmparray[index];
		MEM_freeN(tmparray);

		return value;
	}
}

void RNA_property_float_set_array(PointerRNA *ptr, PropertyRNA *prop, const float *values)
{
	FloatPropertyRNA *fprop = (FloatPropertyRNA *)prop;
	IDProperty *idprop;
	int i;

	BLI_assert(RNA_property_type(prop) == PROP_FLOAT);
	BLI_assert(RNA_property_array_check(prop) != false);

	if ((idprop = rna_idproperty_check(&prop, ptr))) {
		BLI_assert(idprop->len == RNA_property_array_length(ptr, prop) || (prop->flag & PROP_IDPROPERTY));
		if (prop->arraydimension == 0) {
			if (idprop->type == IDP_FLOAT)
				IDP_Float(idprop) = values[0];
			else
				IDP_Double(idprop) = values[0];
		}
		else if (idprop->subtype == IDP_FLOAT) {
			memcpy(IDP_Array(idprop), values, sizeof(float) * idprop->len);
		}
		else {
			for (i = 0; i < idprop->len; i++)
				((double *)IDP_Array(idprop))[i] = values[i];
		}

		rna_idproperty_touch(idprop);
	}
	else if (prop->arraydimension == 0)
		RNA_property_float_set(ptr, prop, values[0]);
	else if (fprop->setarray) {
		fprop->setarray(ptr, values);
	}
	else if (fprop->setarray_ex) {
		fprop->setarray_ex(ptr, prop, values);
	}
	else if (prop->flag & PROP_EDITABLE) {
		IDPropertyTemplate val = {0};
		IDProperty *group;

		/* TODO: RNA_property_float_clamp_array(ptr, prop, &value); */

		val.array.len = prop->totarraylength;
		val.array.type = IDP_FLOAT;

		group = RNA_struct_idprops(ptr, 1);
		if (group) {
			idprop = IDP_New(IDP_ARRAY, &val, prop->identifier);
			IDP_AddToGroup(group, idprop);
			memcpy(IDP_Array(idprop), values, sizeof(float) * idprop->len);
		}
	}
}

void RNA_property_float_set_index(PointerRNA *ptr, PropertyRNA *prop, int index, float value)
{
	float tmp[RNA_MAX_ARRAY_LENGTH];
	int len = rna_ensure_property_array_length(ptr, prop);

	BLI_assert(RNA_property_type(prop) == PROP_FLOAT);
	BLI_assert(RNA_property_array_check(prop) != false);
	BLI_assert(index >= 0);

	if (len <= RNA_MAX_ARRAY_LENGTH) {
		RNA_property_float_get_array(ptr, prop, tmp);
		tmp[index] = value;
		RNA_property_float_set_array(ptr, prop, tmp);
	}
	else {
		float *tmparray;

		tmparray = MEM_callocN(sizeof(float) * len, "RNA_property_float_get_index");
		RNA_property_float_get_array(ptr, prop, tmparray);
		tmparray[index] = value;
		RNA_property_float_set_array(ptr, prop, tmparray);
		MEM_freeN(tmparray);
	}
}

float RNA_property_float_get_default(PointerRNA *UNUSED(ptr), PropertyRNA *prop)
{
	FloatPropertyRNA *fprop = (FloatPropertyRNA *)rna_ensure_property(prop);

	BLI_assert(RNA_property_type(prop) == PROP_FLOAT);
	BLI_assert(RNA_property_array_check(prop) == false);

	return fprop->defaultvalue;
}

void RNA_property_float_get_default_array(PointerRNA *UNUSED(ptr), PropertyRNA *prop, float *values)
{
	FloatPropertyRNA *fprop = (FloatPropertyRNA *)rna_ensure_property(prop);
	
	BLI_assert(RNA_property_type(prop) == PROP_FLOAT);
	BLI_assert(RNA_property_array_check(prop) != false);

	if (prop->arraydimension == 0)
		values[0] = fprop->defaultvalue;
	else if (fprop->defaultarray)
		memcpy(values, fprop->defaultarray, sizeof(float) * prop->totarraylength);
	else
		memset(values, 0, sizeof(float) * prop->totarraylength);
}

float RNA_property_float_get_default_index(PointerRNA *ptr, PropertyRNA *prop, int index)
{
	float tmp[RNA_MAX_ARRAY_LENGTH];
	int len = rna_ensure_property_array_length(ptr, prop);

	BLI_assert(RNA_property_type(prop) == PROP_FLOAT);
	BLI_assert(RNA_property_array_check(prop) != false);
	BLI_assert(index >= 0);

	if (len <= RNA_MAX_ARRAY_LENGTH) {
		RNA_property_float_get_default_array(ptr, prop, tmp);
		return tmp[index];
	}
	else {
		float *tmparray, value;

		tmparray = MEM_callocN(sizeof(float) * len, "RNA_property_float_get_default_index");
		RNA_property_float_get_default_array(ptr, prop, tmparray);
		value = tmparray[index];
		MEM_freeN(tmparray);

		return value;
	}
}

void RNA_property_string_get(PointerRNA *ptr, PropertyRNA *prop, char *value)
{
	StringPropertyRNA *sprop = (StringPropertyRNA *)prop;
	IDProperty *idprop;

	BLI_assert(RNA_property_type(prop) == PROP_STRING);

	if ((idprop = rna_idproperty_check(&prop, ptr))) {
		/* editing bytes is not 100% supported
		 * since they can contain NIL chars */
		if (idprop->subtype == IDP_STRING_SUB_BYTE) {
			memcpy(value, IDP_String(idprop), idprop->len);
			value[idprop->len] = '\0';
		}
		else {
			memcpy(value, IDP_String(idprop), idprop->len);
		}
	}
	else if (sprop->get) {
		sprop->get(ptr, value);
	}
	else if (sprop->get_ex) {
		sprop->get_ex(ptr, prop, value);
	}
	else {
		strcpy(value, sprop->defaultvalue);
	}
}

char *RNA_property_string_get_alloc(PointerRNA *ptr, PropertyRNA *prop,
                                    char *fixedbuf, int fixedlen, int *r_len)
{
	char *buf;
	int length;

	BLI_assert(RNA_property_type(prop) == PROP_STRING);

	length = RNA_property_string_length(ptr, prop);

	if (length + 1 < fixedlen)
		buf = fixedbuf;
	else
		buf = MEM_mallocN(sizeof(char) * (length + 1), "RNA_string_get_alloc");

#ifndef NDEBUG
	/* safety check to ensure the string is actually set */
	buf[length] = 255;
#endif

	RNA_property_string_get(ptr, prop, buf);

#ifndef NDEBUG
	BLI_assert(buf[length] == '\0');
#endif

	if (r_len) {
		*r_len = length;
	}

	return buf;
}

/* this is the length without \0 terminator */
int RNA_property_string_length(PointerRNA *ptr, PropertyRNA *prop)
{
	StringPropertyRNA *sprop = (StringPropertyRNA *)prop;
	IDProperty *idprop;

	BLI_assert(RNA_property_type(prop) == PROP_STRING);

	if ((idprop = rna_idproperty_check(&prop, ptr))) {
		if (idprop->subtype == IDP_STRING_SUB_BYTE) {
			return idprop->len;
		}
		else {
#ifndef NDEBUG
			/* these _must_ stay in sync */
			BLI_assert(strlen(IDP_String(idprop)) == idprop->len - 1);
#endif
			return idprop->len - 1;
		}
	}
	else if (sprop->length)
		return sprop->length(ptr);
	else if (sprop->length_ex)
		return sprop->length_ex(ptr, prop);
	else
		return strlen(sprop->defaultvalue);
}

void RNA_property_string_set(PointerRNA *ptr, PropertyRNA *prop, const char *value)
{
	StringPropertyRNA *sprop = (StringPropertyRNA *)prop;
	IDProperty *idprop;

	BLI_assert(RNA_property_type(prop) == PROP_STRING);

	if ((idprop = rna_idproperty_check(&prop, ptr))) {
		/* both IDP_STRING_SUB_BYTE / IDP_STRING_SUB_UTF8 */
		IDP_AssignString(idprop, value, RNA_property_string_maxlength(prop) - 1);
		rna_idproperty_touch(idprop);
	}
	else if (sprop->set)
		sprop->set(ptr, value);  /* set function needs to clamp its self */
	else if (sprop->set_ex)
		sprop->set_ex(ptr, prop, value);  /* set function needs to clamp its self */
	else if (prop->flag & PROP_EDITABLE) {
		IDProperty *group;

		group = RNA_struct_idprops(ptr, 1);
		if (group)
			IDP_AddToGroup(group, IDP_NewString(value, prop->identifier, RNA_property_string_maxlength(prop)));
	}
}

void RNA_property_string_get_default(PointerRNA *UNUSED(ptr), PropertyRNA *prop, char *value)
{
	StringPropertyRNA *sprop = (StringPropertyRNA *)rna_ensure_property(prop);

	BLI_assert(RNA_property_type(prop) == PROP_STRING);

	strcpy(value, sprop->defaultvalue);
}

char *RNA_property_string_get_default_alloc(PointerRNA *ptr, PropertyRNA *prop, char *fixedbuf, int fixedlen)
{
	char *buf;
	int length;

	BLI_assert(RNA_property_type(prop) == PROP_STRING);

	length = RNA_property_string_default_length(ptr, prop);

	if (length + 1 < fixedlen)
		buf = fixedbuf;
	else
		buf = MEM_callocN(sizeof(char) * (length + 1), "RNA_string_get_alloc");

	RNA_property_string_get_default(ptr, prop, buf);

	return buf;
}

/* this is the length without \0 terminator */
int RNA_property_string_default_length(PointerRNA *UNUSED(ptr), PropertyRNA *prop)
{
	StringPropertyRNA *sprop = (StringPropertyRNA *)prop;

	BLI_assert(RNA_property_type(prop) == PROP_STRING);

	return strlen(sprop->defaultvalue);
}

int RNA_property_enum_get(PointerRNA *ptr, PropertyRNA *prop)
{
	EnumPropertyRNA *eprop = (EnumPropertyRNA *)prop;
	IDProperty *idprop;

	BLI_assert(RNA_property_type(prop) == PROP_ENUM);

	if ((idprop = rna_idproperty_check(&prop, ptr)))
		return IDP_Int(idprop);
	else if (eprop->get)
		return eprop->get(ptr);
	else if (eprop->get_ex)
		return eprop->get_ex(ptr, prop);
	else
		return eprop->defaultvalue;
}

void RNA_property_enum_set(PointerRNA *ptr, PropertyRNA *prop, int value)
{
	EnumPropertyRNA *eprop = (EnumPropertyRNA *)prop;
	IDProperty *idprop;

	BLI_assert(RNA_property_type(prop) == PROP_ENUM);

	if ((idprop = rna_idproperty_check(&prop, ptr))) {
		IDP_Int(idprop) = value;
		rna_idproperty_touch(idprop);
	}
	else if (eprop->set) {
		eprop->set(ptr, value);
	}
	else if (eprop->set_ex) {
		eprop->set_ex(ptr, prop, value);
	}
	else if (prop->flag & PROP_EDITABLE) {
		IDPropertyTemplate val = {0};
		IDProperty *group;

		val.i = value;

		group = RNA_struct_idprops(ptr, 1);
		if (group)
			IDP_AddToGroup(group, IDP_New(IDP_INT, &val, prop->identifier));
	}
}

int RNA_property_enum_get_default(PointerRNA *UNUSED(ptr), PropertyRNA *prop)
{
	EnumPropertyRNA *eprop = (EnumPropertyRNA *)rna_ensure_property(prop);

	BLI_assert(RNA_property_type(prop) == PROP_ENUM);

	return eprop->defaultvalue;
}

void *RNA_property_enum_py_data_get(PropertyRNA *prop)
{
	EnumPropertyRNA *eprop = (EnumPropertyRNA *)prop;

	BLI_assert(RNA_property_type(prop) == PROP_ENUM);

	return eprop->py_data;
}

PointerRNA RNA_property_pointer_get(PointerRNA *ptr, PropertyRNA *prop)
{
	PointerPropertyRNA *pprop = (PointerPropertyRNA *)prop;
	IDProperty *idprop;

	BLI_assert(RNA_property_type(prop) == PROP_POINTER);

	if ((idprop = rna_idproperty_check(&prop, ptr))) {
		pprop = (PointerPropertyRNA *)prop;

		/* for groups, data is idprop itself */
		if (pprop->typef)
			return rna_pointer_inherit_refine(ptr, pprop->typef(ptr), idprop);
		else
			return rna_pointer_inherit_refine(ptr, pprop->type, idprop);
	}
	else if (pprop->get) {
		return pprop->get(ptr);
	}
	else if (prop->flag & PROP_IDPROPERTY) {
		/* XXX temporary hack to add it automatically, reading should
		 * never do any write ops, to ensure thread safety etc .. */
		RNA_property_pointer_add(ptr, prop);
		return RNA_property_pointer_get(ptr, prop);
	}
	else {
		return PointerRNA_NULL;
	}
}

void RNA_property_pointer_set(PointerRNA *ptr, PropertyRNA *prop, PointerRNA ptr_value)
{
	/*IDProperty *idprop;*/

	BLI_assert(RNA_property_type(prop) == PROP_POINTER);

	if ((/*idprop = */ rna_idproperty_check(&prop, ptr))) {
		/* not supported */
		/* rna_idproperty_touch(idprop); */
	}
	else {
		PointerPropertyRNA *pprop = (PointerPropertyRNA *)prop;

		if (pprop->set &&
		    !((prop->flag & PROP_NEVER_NULL) && ptr_value.data == NULL) &&
		    !((prop->flag & PROP_ID_SELF_CHECK) && ptr->id.data == ptr_value.id.data))
		{
			pprop->set(ptr, ptr_value);
		}
	}
}

PointerRNA RNA_property_pointer_get_default(PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop))
{
	/*PointerPropertyRNA *pprop = (PointerPropertyRNA *)prop; */

	/* BLI_assert(RNA_property_type(prop) == PROP_POINTER); */

	return PointerRNA_NULL; /* FIXME: there has to be a way... */
}

void RNA_property_pointer_add(PointerRNA *ptr, PropertyRNA *prop)
{
	/*IDProperty *idprop;*/

	BLI_assert(RNA_property_type(prop) == PROP_POINTER);

	if ((/*idprop=*/ rna_idproperty_check(&prop, ptr))) {
		/* already exists */
	}
	else if (prop->flag & PROP_IDPROPERTY) {
		IDPropertyTemplate val = {0};
		IDProperty *group;

		val.i = 0;

		group = RNA_struct_idprops(ptr, 1);
		if (group)
			IDP_AddToGroup(group, IDP_New(IDP_GROUP, &val, prop->identifier));
	}
	else
		printf("%s %s.%s: only supported for id properties.\n", __func__, ptr->type->identifier, prop->identifier);
}

void RNA_property_pointer_remove(PointerRNA *ptr, PropertyRNA *prop)
{
	IDProperty *idprop, *group;

	BLI_assert(RNA_property_type(prop) == PROP_POINTER);

	if ((idprop = rna_idproperty_check(&prop, ptr))) {
		group = RNA_struct_idprops(ptr, 0);
		
		if (group) {
			IDP_FreeFromGroup(group, idprop);
		}
	}
	else
		printf("%s %s.%s: only supported for id properties.\n", __func__, ptr->type->identifier, prop->identifier);
}

static void rna_property_collection_get_idp(CollectionPropertyIterator *iter)
{
	CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)iter->prop;

	iter->ptr.data = rna_iterator_array_get(iter);
	iter->ptr.type = cprop->item_type;
	rna_pointer_inherit_id(cprop->item_type, &iter->parent, &iter->ptr);
}

void RNA_property_collection_begin(PointerRNA *ptr, PropertyRNA *prop, CollectionPropertyIterator *iter)
{
	IDProperty *idprop;

	BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);

	memset(iter, 0, sizeof(*iter));

	if ((idprop = rna_idproperty_check(&prop, ptr)) || (prop->flag & PROP_IDPROPERTY)) {
		iter->parent = *ptr;
		iter->prop = prop;

		if (idprop)
			rna_iterator_array_begin(iter, IDP_IDPArray(idprop), sizeof(IDProperty), idprop->len, 0, NULL);
		else
			rna_iterator_array_begin(iter, NULL, sizeof(IDProperty), 0, 0, NULL);

		if (iter->valid)
			rna_property_collection_get_idp(iter);

		iter->idprop = 1;
	}
	else {
		CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)prop;
		cprop->begin(iter, ptr);
	}
}

void RNA_property_collection_next(CollectionPropertyIterator *iter)
{
	CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)rna_ensure_property(iter->prop);

	if (iter->idprop) {
		rna_iterator_array_next(iter);

		if (iter->valid)
			rna_property_collection_get_idp(iter);
	}
	else
		cprop->next(iter);
}

void RNA_property_collection_skip(CollectionPropertyIterator *iter, int num)
{
	CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)rna_ensure_property(iter->prop);
	int i;

	if (num > 1 && (iter->idprop || (cprop->property.flag & PROP_RAW_ARRAY))) {
		/* fast skip for array */
		ArrayIterator *internal = &iter->internal.array;

		if (!internal->skip) {
			internal->ptr += internal->itemsize * (num - 1);
			iter->valid = (internal->ptr < internal->endptr);
			if (iter->valid)
				RNA_property_collection_next(iter);
			return;
		}
	}

	/* slow iteration otherwise */
	for (i = 0; i < num && iter->valid; i++)
		RNA_property_collection_next(iter);
}

void RNA_property_collection_end(CollectionPropertyIterator *iter)
{
	CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)rna_ensure_property(iter->prop);

	if (iter->idprop)
		rna_iterator_array_end(iter);
	else
		cprop->end(iter);
}

int RNA_property_collection_length(PointerRNA *ptr, PropertyRNA *prop)
{
	CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)prop;
	IDProperty *idprop;

	BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);

	if ((idprop = rna_idproperty_check(&prop, ptr))) {
		return idprop->len;
	}
	else if (cprop->length) {
		return cprop->length(ptr);
	}
	else {
		CollectionPropertyIterator iter;
		int length = 0;

		RNA_property_collection_begin(ptr, prop, &iter);
		for (; iter.valid; RNA_property_collection_next(&iter))
			length++;
		RNA_property_collection_end(&iter);

		return length;
	}
}

void RNA_property_collection_add(PointerRNA *ptr, PropertyRNA *prop, PointerRNA *r_ptr)
{
	IDProperty *idprop;
/*	CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)prop; */

	BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);

	if ((idprop = rna_idproperty_check(&prop, ptr))) {
		IDPropertyTemplate val = {0};
		IDProperty *item;

		item = IDP_New(IDP_GROUP, &val, "");
		IDP_AppendArray(idprop, item);
		/* IDP_FreeProperty(item);  *//* IDP_AppendArray does a shallow copy (memcpy), only free memory  */
		MEM_freeN(item);
		rna_idproperty_touch(idprop);
	}
	else if (prop->flag & PROP_IDPROPERTY) {
		IDProperty *group, *item;
		IDPropertyTemplate val = {0};

		group = RNA_struct_idprops(ptr, 1);
		if (group) {
			idprop = IDP_NewIDPArray(prop->identifier);
			IDP_AddToGroup(group, idprop);

			item = IDP_New(IDP_GROUP, &val, "");
			IDP_AppendArray(idprop, item);
			/* IDP_FreeProperty(item);  *//* IDP_AppendArray does a shallow copy (memcpy), only free memory */
			MEM_freeN(item);
		}
	}

	/* py api calls directly */
#if 0
	else if (cprop->add) {
		if (!(cprop->add->flag & FUNC_USE_CONTEXT)) { /* XXX check for this somewhere else */
			ParameterList params;
			RNA_parameter_list_create(&params, ptr, cprop->add);
			RNA_function_call(NULL, NULL, ptr, cprop->add, &params);
			RNA_parameter_list_free(&params);
		}
	}
	/*else
	    printf("%s %s.%s: not implemented for this property.\n", __func__, ptr->type->identifier, prop->identifier);*/
#endif

	if (r_ptr) {
		if (idprop) {
			CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)prop;

			r_ptr->data = IDP_GetIndexArray(idprop, idprop->len - 1);
			r_ptr->type = cprop->item_type;
			rna_pointer_inherit_id(NULL, ptr, r_ptr);
		}
		else
			memset(r_ptr, 0, sizeof(*r_ptr));
	}
}

bool RNA_property_collection_remove(PointerRNA *ptr, PropertyRNA *prop, int key)
{
	IDProperty *idprop;
/*	CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)prop; */

	BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);

	if ((idprop = rna_idproperty_check(&prop, ptr))) {
		IDProperty tmp, *array;
		int len;

		len = idprop->len;
		array = IDP_IDPArray(idprop);

		if (key >= 0 && key < len) {
			if (key + 1 < len) {
				/* move element to be removed to the back */
				memcpy(&tmp, &array[key], sizeof(IDProperty));
				memmove(array + key, array + key + 1, sizeof(IDProperty) * (len - (key + 1)));
				memcpy(&array[len - 1], &tmp, sizeof(IDProperty));
			}

			IDP_ResizeIDPArray(idprop, len - 1);
		}

		return true;
	}
	else if (prop->flag & PROP_IDPROPERTY) {
		return true;
	}

	/* py api calls directly */
#if 0
	else if (cprop->remove) {
		if (!(cprop->remove->flag & FUNC_USE_CONTEXT)) { /* XXX check for this somewhere else */
			ParameterList params;
			RNA_parameter_list_create(&params, ptr, cprop->remove);
			RNA_function_call(NULL, NULL, ptr, cprop->remove, &params);
			RNA_parameter_list_free(&params);
		}

		return false;
	}
	/*else
	    printf("%s %s.%s: only supported for id properties.\n", __func__, ptr->type->identifier, prop->identifier);*/
#endif
	return false;
}

bool RNA_property_collection_move(PointerRNA *ptr, PropertyRNA *prop, int key, int pos)
{
	IDProperty *idprop;

	BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);

	if ((idprop = rna_idproperty_check(&prop, ptr))) {
		IDProperty tmp, *array;
		int len;

		len = idprop->len;
		array = IDP_IDPArray(idprop);

		if (key >= 0 && key < len && pos >= 0 && pos < len && key != pos) {
			memcpy(&tmp, &array[key], sizeof(IDProperty));
			if (pos < key)
				memmove(array + pos + 1, array + pos, sizeof(IDProperty) * (key - pos));
			else
				memmove(array + key, array + key + 1, sizeof(IDProperty) * (pos - key));
			memcpy(&array[pos], &tmp, sizeof(IDProperty));
		}

		return true;
	}
	else if (prop->flag & PROP_IDPROPERTY) {
		return true;
	}

	return false;
}

void RNA_property_collection_clear(PointerRNA *ptr, PropertyRNA *prop)
{
	IDProperty *idprop;

	BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);

	if ((idprop = rna_idproperty_check(&prop, ptr))) {
		IDP_ResizeIDPArray(idprop, 0);
		rna_idproperty_touch(idprop);
	}
}

int RNA_property_collection_lookup_index(PointerRNA *ptr, PropertyRNA *prop, PointerRNA *t_ptr)
{
	CollectionPropertyIterator iter;
	int index = 0;
	
	BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);

	RNA_property_collection_begin(ptr, prop, &iter);
	for (index = 0; iter.valid; RNA_property_collection_next(&iter), index++) {
		if (iter.ptr.data == t_ptr->data)
			break;
	}
	RNA_property_collection_end(&iter);
	
	/* did we find it? */
	if (iter.valid)
		return index;
	else
		return -1;
}

int RNA_property_collection_lookup_int(PointerRNA *ptr, PropertyRNA *prop, int key, PointerRNA *r_ptr)
{
	CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)rna_ensure_property(prop);

	BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);

	if (cprop->lookupint) {
		/* we have a callback defined, use it */
		return cprop->lookupint(ptr, key, r_ptr);
	}
	else {
		/* no callback defined, just iterate and find the nth item */
		CollectionPropertyIterator iter;
		int i;

		RNA_property_collection_begin(ptr, prop, &iter);
		for (i = 0; iter.valid; RNA_property_collection_next(&iter), i++) {
			if (i == key) {
				*r_ptr = iter.ptr;
				break;
			}
		}
		RNA_property_collection_end(&iter);

		if (!iter.valid)
			memset(r_ptr, 0, sizeof(*r_ptr));

		return iter.valid;
	}
}

int RNA_property_collection_lookup_string(PointerRNA *ptr, PropertyRNA *prop, const char *key, PointerRNA *r_ptr)
{
	CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)rna_ensure_property(prop);

	BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);

	if (cprop->lookupstring) {
		/* we have a callback defined, use it */
		return cprop->lookupstring(ptr, key, r_ptr);
	}
	else {
		/* no callback defined, compare with name properties if they exist */
		CollectionPropertyIterator iter;
		PropertyRNA *nameprop;
		char name[256], *nameptr;
		int found = 0;
		int keylen = strlen(key);
		int namelen;

		RNA_property_collection_begin(ptr, prop, &iter);
		for (; iter.valid; RNA_property_collection_next(&iter)) {
			if (iter.ptr.data && iter.ptr.type->nameproperty) {
				nameprop = iter.ptr.type->nameproperty;

				nameptr = RNA_property_string_get_alloc(&iter.ptr, nameprop, name, sizeof(name), &namelen);

				if ((keylen == namelen) && (strcmp(nameptr, key) == 0)) {
					*r_ptr = iter.ptr;
					found = 1;
				}

				if ((char *)&name != nameptr)
					MEM_freeN(nameptr);

				if (found)
					break;
			}
		}
		RNA_property_collection_end(&iter);

		if (!iter.valid)
			memset(r_ptr, 0, sizeof(*r_ptr));

		return iter.valid;
	}
}

/* zero return is an assignment error */
int RNA_property_collection_assign_int(PointerRNA *ptr, PropertyRNA *prop, const int key, const PointerRNA *assign_ptr)
{
	CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)rna_ensure_property(prop);

	BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);

	if (cprop->assignint) {
		/* we have a callback defined, use it */
		return cprop->assignint(ptr, key, assign_ptr);
	}

	return 0;
}

bool RNA_property_collection_type_get(PointerRNA *ptr, PropertyRNA *prop, PointerRNA *r_ptr)
{
	BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);

	*r_ptr = *ptr;
	return ((r_ptr->type = rna_ensure_property(prop)->srna) ? 1 : 0);
}

int RNA_property_collection_raw_array(PointerRNA *ptr, PropertyRNA *prop, PropertyRNA *itemprop, RawArray *array)
{
	CollectionPropertyIterator iter;
	ArrayIterator *internal;
	char *arrayp;

	BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);

	if (!(prop->flag & PROP_RAW_ARRAY) || !(itemprop->flag & PROP_RAW_ACCESS))
		return 0;

	RNA_property_collection_begin(ptr, prop, &iter);

	if (iter.valid) {
		/* get data from array iterator and item property */
		internal = &iter.internal.array;
		arrayp = (iter.valid) ? iter.ptr.data : NULL;

		if (internal->skip || !RNA_property_editable(&iter.ptr, itemprop)) {
			/* we might skip some items, so it's not a proper array */
			RNA_property_collection_end(&iter);
			return 0;
		}

		array->array = arrayp + itemprop->rawoffset;
		array->stride = internal->itemsize;
		array->len = ((char *)internal->endptr - arrayp) / internal->itemsize;
		array->type = itemprop->rawtype;
	}
	else
		memset(array, 0, sizeof(RawArray));

	RNA_property_collection_end(&iter);

	return 1;
}

#define RAW_GET(dtype, var, raw, a)                                           \
{                                                                             \
	switch (raw.type) {                                                       \
		case PROP_RAW_CHAR: var = (dtype)((char *)raw.array)[a]; break;       \
		case PROP_RAW_SHORT: var = (dtype)((short *)raw.array)[a]; break;     \
		case PROP_RAW_INT: var = (dtype)((int *)raw.array)[a]; break;         \
		case PROP_RAW_FLOAT: var = (dtype)((float *)raw.array)[a]; break;     \
		case PROP_RAW_DOUBLE: var = (dtype)((double *)raw.array)[a]; break;   \
		default: var = (dtype)0;                                              \
	}                                                                         \
} (void)0

#define RAW_SET(dtype, raw, a, var)                                           \
{                                                                             \
	switch (raw.type) {                                                       \
		case PROP_RAW_CHAR: ((char *)raw.array)[a] = (char)var; break;        \
		case PROP_RAW_SHORT: ((short *)raw.array)[a] = (short)var; break;     \
		case PROP_RAW_INT: ((int *)raw.array)[a] = (int)var; break;           \
		case PROP_RAW_FLOAT: ((float *)raw.array)[a] = (float)var; break;     \
		case PROP_RAW_DOUBLE: ((double *)raw.array)[a] = (double)var; break;  \
		default: break;                                                       \
	}                                                                         \
} (void)0

int RNA_raw_type_sizeof(RawPropertyType type)
{
	switch (type) {
		case PROP_RAW_CHAR: return sizeof(char);
		case PROP_RAW_SHORT: return sizeof(short);
		case PROP_RAW_INT: return sizeof(int);
		case PROP_RAW_FLOAT: return sizeof(float);
		case PROP_RAW_DOUBLE: return sizeof(double);
		default: return 0;
	}
}

static int rna_property_array_length_all_dimensions(PointerRNA *ptr, PropertyRNA *prop)
{
	int i, len[RNA_MAX_ARRAY_DIMENSION];
	const int dim = RNA_property_array_dimension(ptr, prop, len);
	int size;

	if (dim == 0)
		return 0;

	for (size = 1, i = 0; i < dim; i++)
		size *= len[i];
	
	return size;
}

static int rna_raw_access(ReportList *reports, PointerRNA *ptr, PropertyRNA *prop, const char *propname,
                          void *inarray, RawPropertyType intype, int inlen, int set)
{
	StructRNA *ptype;
	PointerRNA itemptr;
	PropertyRNA *itemprop, *iprop;
	PropertyType itemtype = 0;
	RawArray in;
	int itemlen = 0;

	/* initialize in array, stride assumed 0 in following code */
	in.array = inarray;
	in.type = intype;
	in.len = inlen;
	in.stride = 0;

	ptype = RNA_property_pointer_type(ptr, prop);

	/* try to get item property pointer */
	RNA_pointer_create(NULL, ptype, NULL, &itemptr);
	itemprop = RNA_struct_find_property(&itemptr, propname);

	if (itemprop) {
		/* we have item property pointer */
		RawArray out;

		/* check type */
		itemtype = RNA_property_type(itemprop);

		if (!ELEM(itemtype, PROP_BOOLEAN, PROP_INT, PROP_FLOAT)) {
			BKE_report(reports, RPT_ERROR, "Only boolean, int and float properties supported");
			return 0;
		}

		/* check item array */
		itemlen = RNA_property_array_length(&itemptr, itemprop);

		/* dynamic array? need to get length per item */
		if (itemprop->getlength) {
			itemprop = NULL;
		}
		/* try to access as raw array */
		else if (RNA_property_collection_raw_array(ptr, prop, itemprop, &out)) {
			int arraylen = (itemlen == 0) ? 1 : itemlen;
			if (in.len != arraylen * out.len) {
				BKE_reportf(reports, RPT_ERROR, "Array length mismatch (expected %d, got %d)",
				            out.len * arraylen, in.len);
				return 0;
			}
			
			/* matching raw types */
			if (out.type == in.type) {
				void *inp = in.array;
				void *outp = out.array;
				int a, size;

				size = RNA_raw_type_sizeof(out.type) * arraylen;

				for (a = 0; a < out.len; a++) {
					if (set) memcpy(outp, inp, size);
					else memcpy(inp, outp, size);

					inp = (char *)inp + size;
					outp = (char *)outp + out.stride;
				}

				return 1;
			}

			/* could also be faster with non-matching types,
			 * for now we just do slower loop .. */
		}
	}

	{
		void *tmparray = NULL;
		int tmplen = 0;
		int err = 0, j, a = 0;
		int needconv = 1;

		if (((itemtype == PROP_BOOLEAN || itemtype == PROP_INT) && in.type == PROP_RAW_INT) ||
		    (itemtype == PROP_FLOAT && in.type == PROP_RAW_FLOAT))
		{
			/* avoid creating temporary buffer if the data type match */
			needconv = 0;
		}
		/* no item property pointer, can still be id property, or
		 * property of a type derived from the collection pointer type */
		RNA_PROP_BEGIN (ptr, itemptr, prop)
		{
			if (itemptr.data) {
				if (itemprop) {
					/* we got the property already */
					iprop = itemprop;
				}
				else {
					/* not yet, look it up and verify if it is valid */
					iprop = RNA_struct_find_property(&itemptr, propname);

					if (iprop) {
						itemlen = rna_property_array_length_all_dimensions(&itemptr, iprop);
						itemtype = RNA_property_type(iprop);
					}
					else {
						BKE_reportf(reports, RPT_ERROR, "Property named '%s' not found", propname);
						err = 1;
						break;
					}

					if (!ELEM(itemtype, PROP_BOOLEAN, PROP_INT, PROP_FLOAT)) {
						BKE_report(reports, RPT_ERROR, "Only boolean, int and float properties supported");
						err = 1;
						break;
					}
				}

				/* editable check */
				if (!set || RNA_property_editable(&itemptr, iprop)) {
					if (a + itemlen > in.len) {
						BKE_reportf(reports, RPT_ERROR, "Array length mismatch (got %d, expected more)", in.len);
						err = 1;
						break;
					}

					if (itemlen == 0) {
						/* handle conversions */
						if (set) {
							switch (itemtype) {
								case PROP_BOOLEAN:
								{
									int b;
									RAW_GET(int, b, in, a);
									RNA_property_boolean_set(&itemptr, iprop, b);
									break;
								}
								case PROP_INT:
								{
									int i;
									RAW_GET(int, i, in, a);
									RNA_property_int_set(&itemptr, iprop, i);
									break;
								}
								case PROP_FLOAT:
								{
									float f;
									RAW_GET(float, f, in, a);
									RNA_property_float_set(&itemptr, iprop, f);
									break;
								}
								default:
									break;
							}
						}
						else {
							switch (itemtype) {
								case PROP_BOOLEAN:
								{
									int b = RNA_property_boolean_get(&itemptr, iprop);
									RAW_SET(int, in, a, b);
									break;
								}
								case PROP_INT:
								{
									int i = RNA_property_int_get(&itemptr, iprop);
									RAW_SET(int, in, a, i);
									break;
								}
								case PROP_FLOAT:
								{
									float f = RNA_property_float_get(&itemptr, iprop);
									RAW_SET(float, in, a, f);
									break;
								}
								default:
									break;
							}
						}
						a++;
					}
					else if (needconv == 1) {
						/* allocate temporary array if needed */
						if (tmparray && tmplen != itemlen) {
							MEM_freeN(tmparray);
							tmparray = NULL;
						}
						if (!tmparray) {
							tmparray = MEM_callocN(sizeof(float) * itemlen, "RNA tmparray\n");
							tmplen = itemlen;
						}

						/* handle conversions */
						if (set) {
							switch (itemtype) {
								case PROP_BOOLEAN:
								{
									for (j = 0; j < itemlen; j++, a++)
										RAW_GET(int, ((int *)tmparray)[j], in, a);
									RNA_property_boolean_set_array(&itemptr, iprop, tmparray);
									break;
								}
								case PROP_INT:
								{
									for (j = 0; j < itemlen; j++, a++)
										RAW_GET(int, ((int *)tmparray)[j], in, a);
									RNA_property_int_set_array(&itemptr, iprop, tmparray);
									break;
								}
								case PROP_FLOAT:
								{
									for (j = 0; j < itemlen; j++, a++)
										RAW_GET(float, ((float *)tmparray)[j], in, a);
									RNA_property_float_set_array(&itemptr, iprop, tmparray);
									break;
								}
								default:
									break;
							}
						}
						else {
							switch (itemtype) {
								case PROP_BOOLEAN:
								{
									RNA_property_boolean_get_array(&itemptr, iprop, tmparray);
									for (j = 0; j < itemlen; j++, a++)
										RAW_SET(int, in, a, ((int *)tmparray)[j]);
									break;
								}
								case PROP_INT:
								{
									RNA_property_int_get_array(&itemptr, iprop, tmparray);
									for (j = 0; j < itemlen; j++, a++)
										RAW_SET(int, in, a, ((int *)tmparray)[j]);
									break;
								}
								case PROP_FLOAT:
								{
									RNA_property_float_get_array(&itemptr, iprop, tmparray);
									for (j = 0; j < itemlen; j++, a++)
										RAW_SET(float, in, a, ((float *)tmparray)[j]);
									break;
								}
								default:
									break;
							}
						}
					}
					else {
						if (set) {
							switch (itemtype) {
								case PROP_BOOLEAN:
								{
									RNA_property_boolean_set_array(&itemptr, iprop, &((int *)in.array)[a]);
									a += itemlen;
									break;
								}
								case PROP_INT:
								{
									RNA_property_int_set_array(&itemptr, iprop, &((int *)in.array)[a]);
									a += itemlen;
									break;
								}
								case PROP_FLOAT:
								{
									RNA_property_float_set_array(&itemptr, iprop, &((float *)in.array)[a]);
									a += itemlen;
									break;
								}
								default:
									break;
							}
						}
						else {
							switch (itemtype) {
								case PROP_BOOLEAN:
								{
									RNA_property_boolean_get_array(&itemptr, iprop, &((int *)in.array)[a]);
									a += itemlen;
									break;
								}
								case PROP_INT:
								{
									RNA_property_int_get_array(&itemptr, iprop, &((int *)in.array)[a]);
									a += itemlen;
									break;
								}
								case PROP_FLOAT:
								{
									RNA_property_float_get_array(&itemptr, iprop, &((float *)in.array)[a]);
									a += itemlen;
									break;
								}
								default:
									break;
							}
						}
					}
				}
			}
		}
		RNA_PROP_END;

		if (tmparray)
			MEM_freeN(tmparray);

		return !err;
	}
}

RawPropertyType RNA_property_raw_type(PropertyRNA *prop)
{
	if (prop->rawtype == PROP_RAW_UNSET) {
		/* this property has no raw access, yet we try to provide a raw type to help building the array */
		switch (prop->type) {
			case PROP_BOOLEAN:
				return PROP_RAW_INT;
			case PROP_INT:
				return PROP_RAW_INT;
			case PROP_FLOAT:
				return PROP_RAW_FLOAT;
			case PROP_ENUM:
				return PROP_RAW_INT;
			default:
				break;
		}
	}
	return prop->rawtype;
}

int RNA_property_collection_raw_get(ReportList *reports, PointerRNA *ptr, PropertyRNA *prop, const char *propname,
                                    void *array, RawPropertyType type, int len)
{
	return rna_raw_access(reports, ptr, prop, propname, array, type, len, 0);
}

int RNA_property_collection_raw_set(ReportList *reports, PointerRNA *ptr, PropertyRNA *prop, const char *propname,
                                    void *array, RawPropertyType type, int len)
{
	return rna_raw_access(reports, ptr, prop, propname, array, type, len, 1);
}

/* Standard iterator functions */

void rna_iterator_listbase_begin(CollectionPropertyIterator *iter, ListBase *lb, IteratorSkipFunc skip)
{
	ListBaseIterator *internal = &iter->internal.listbase;

	internal->link = (lb) ? lb->first : NULL;
	internal->skip = skip;

	iter->valid = (internal->link != NULL);

	if (skip && iter->valid && skip(iter, internal->link))
		rna_iterator_listbase_next(iter);
}

void rna_iterator_listbase_next(CollectionPropertyIterator *iter)
{
	ListBaseIterator *internal = &iter->internal.listbase;

	if (internal->skip) {
		do {
			internal->link = internal->link->next;
			iter->valid = (internal->link != NULL);
		} while (iter->valid && internal->skip(iter, internal->link));
	}
	else {
		internal->link = internal->link->next;
		iter->valid = (internal->link != NULL);
	}
}

void *rna_iterator_listbase_get(CollectionPropertyIterator *iter)
{
	ListBaseIterator *internal = &iter->internal.listbase;

	return internal->link;
}

void rna_iterator_listbase_end(CollectionPropertyIterator *UNUSED(iter))
{
}

PointerRNA rna_listbase_lookup_int(PointerRNA *ptr, StructRNA *type, struct ListBase *lb, int index)
{
	void *data = BLI_findlink(lb, index);
	return rna_pointer_inherit_refine(ptr, type, data);
}

void rna_iterator_array_begin(CollectionPropertyIterator *iter, void *ptr, int itemsize, int length,
                              bool free_ptr, IteratorSkipFunc skip)
{
	ArrayIterator *internal;

	if (ptr == NULL)
		length = 0;
	else if (length == 0) {
		ptr = NULL;
		itemsize = 0;
	}

	internal = &iter->internal.array;
	internal->ptr = ptr;
	internal->free_ptr = free_ptr ? ptr : NULL;
	internal->endptr = ((char *)ptr) + length * itemsize;
	internal->itemsize = itemsize;
	internal->skip = skip;
	internal->length = length;
	
	iter->valid = (internal->ptr != internal->endptr);

	if (skip && iter->valid && skip(iter, internal->ptr))
		rna_iterator_array_next(iter);
}

void rna_iterator_array_next(CollectionPropertyIterator *iter)
{
	ArrayIterator *internal = &iter->internal.array;

	if (internal->skip) {
		do {
			internal->ptr += internal->itemsize;
			iter->valid = (internal->ptr != internal->endptr);
		} while (iter->valid && internal->skip(iter, internal->ptr));
	}
	else {
		internal->ptr += internal->itemsize;
		iter->valid = (internal->ptr != internal->endptr);
	}
}

void *rna_iterator_array_get(CollectionPropertyIterator *iter)
{
	ArrayIterator *internal = &iter->internal.array;

	return internal->ptr;
}

void *rna_iterator_array_dereference_get(CollectionPropertyIterator *iter)
{
	ArrayIterator *internal = &iter->internal.array;

	/* for ** arrays */
	return *(void **)(internal->ptr);
}

void rna_iterator_array_end(CollectionPropertyIterator *iter)
{
	ArrayIterator *internal = &iter->internal.array;
	
	if (internal->free_ptr) {
		MEM_freeN(internal->free_ptr);
		internal->free_ptr = NULL;
	}
}

PointerRNA rna_array_lookup_int(PointerRNA *ptr, StructRNA *type, void *data, int itemsize, int length, int index)
{
	if (index < 0 || index >= length)
		return PointerRNA_NULL;

	return rna_pointer_inherit_refine(ptr, type, ((char *)data) + index * itemsize);
}

/* RNA Path - Experiment */

static char *rna_path_token(const char **path, char *fixedbuf, int fixedlen, int bracket)
{
	const char *p;
	char *buf;
	char quote = '\0';
	int i, j, len, escape;

	len = 0;

	if (bracket) {
		/* get data between [], check escaping ] with \] */
		if (**path == '[') (*path)++;
		else return NULL;

		p = *path;

		/* 2 kinds of lookups now, quoted or unquoted */
		quote = *p;

		if (quote != '"') /* " - this comment is hack for Aligorith's text editor's sanity */
			quote = 0;

		if (quote == 0) {
			while (*p && (*p != ']')) {
				len++;
				p++;
			}
		}
		else {
			escape = 0;
			/* skip the first quote */
			len++;
			p++;
			while (*p && (*p != quote || escape)) {
				escape = (*p == '\\');
				len++;
				p++;
			}
			
			/* skip the last quoted char to get the ']' */
			len++;
			p++;
		}

		if (*p != ']') return NULL;
	}
	else {
		/* get data until . or [ */
		p = *path;

		while (*p && *p != '.' && *p != '[') {
			len++;
			p++;
		}
	}
	
	/* empty, return */
	if (len == 0)
		return NULL;
	
	/* try to use fixed buffer if possible */
	if (len + 1 < fixedlen)
		buf = fixedbuf;
	else
		buf = MEM_callocN(sizeof(char) * (len + 1), "rna_path_token");

	/* copy string, taking into account escaped ] */
	if (bracket) {
		for (p = *path, i = 0, j = 0; i < len; i++, p++) {
			if (*p == '\\' && *(p + 1) == quote) {}
			else buf[j++] = *p;
		}

		buf[j] = 0;
	}
	else {
		memcpy(buf, *path, sizeof(char) * len);
		buf[len] = '\0';
	}

	/* set path to start of next token */
	if (*p == ']') p++;
	if (*p == '.') p++;
	*path = p;

	return buf;
}

static int rna_token_strip_quotes(char *token)
{
	if (token[0] == '"') {
		int len = strlen(token);
		if (len >= 2 && token[len - 1] == '"') {
			/* strip away "" */
			token[len - 1] = '\0';
			return 1;
		}
	}
	return 0;
}

static bool rna_path_parse_collection_key(const char **path, PointerRNA *ptr, PropertyRNA *prop, PointerRNA *r_nextptr)
{
	char fixedbuf[256];
	int intkey;
	
	*r_nextptr = *ptr;

	/* end of path, ok */
	if (!(**path))
		return true;
	
	if (**path == '[') {
		char *token;

		/* resolve the lookup with [] brackets */
		token = rna_path_token(path, fixedbuf, sizeof(fixedbuf), 1);
		
		if (!token)
			return false;
		
		/* check for "" to see if it is a string */
		if (rna_token_strip_quotes(token)) {
			if (RNA_property_collection_lookup_string(ptr, prop, token + 1, r_nextptr)) {
				/* pass */
			}
			else {
				r_nextptr->data = NULL;
			}
		}
		else {
			/* otherwise do int lookup */
			intkey = atoi(token);
			if (intkey == 0 && (token[0] != '0' || token[1] != '\0')) {
				return false; /* we can be sure the fixedbuf was used in this case */
			}
			if (RNA_property_collection_lookup_int(ptr, prop, intkey, r_nextptr)) {
				/* pass */
			}
			else {
				r_nextptr->data = NULL;
			}
		}
		
		if (token != fixedbuf) {
			MEM_freeN(token);
		}
	}
	else {
		if (RNA_property_collection_type_get(ptr, prop, r_nextptr)) {
			/* pass */
		}
		else {
			/* ensure we quit on invalid values */
			r_nextptr->data = NULL;
		}
	}
	
	return true;
}

static bool rna_path_parse_array_index(const char **path, PointerRNA *ptr, PropertyRNA *prop, int *r_index)
{
	char fixedbuf[256];
	int index_arr[RNA_MAX_ARRAY_DIMENSION] = {0};
	int len[RNA_MAX_ARRAY_DIMENSION];
	const int dim = RNA_property_array_dimension(ptr, prop, len);
	int i;
	
	*r_index = -1;
	
	/* end of path, ok */
	if (!(**path))
		return true;
	
	for (i = 0; i < dim; i++) {
		int temp_index = -1;
		char *token;
		
		/* multi index resolve */
		if (**path == '[') {
			token = rna_path_token(path, fixedbuf, sizeof(fixedbuf), 1);
			
			if (token == NULL) {
				/* invalid syntax blah[] */
				return false;
			}
			/* check for "" to see if it is a string */
			else if (rna_token_strip_quotes(token)) {
				temp_index = RNA_property_array_item_index(prop, *(token + 1));
			}
			else {
				/* otherwise do int lookup */
				temp_index = atoi(token);
				
				if (temp_index == 0 && (token[0] != '0' || token[1] != '\0')) {
					if (token != fixedbuf) {
						MEM_freeN(token);
					}
					
					return false;
				}
			}
		}
		else if (dim == 1) {
			/* location.x || scale.X, single dimension arrays only */
			token = rna_path_token(path, fixedbuf, sizeof(fixedbuf), 0);
			if (token == NULL) {
				/* invalid syntax blah.. */
				return false;
			}
			temp_index = RNA_property_array_item_index(prop, *token);
		}
		else {
			/* just to avoid uninitialized pointer use */
			token = fixedbuf;
		}
		
		if (token != fixedbuf) {
			MEM_freeN(token);
		}
		
		/* out of range */
		if (temp_index < 0 || temp_index >= len[i])
			return false;
		
		index_arr[i] = temp_index;
		/* end multi index resolve */
	}
	
	/* arrays always contain numbers so further values are not valid */
	if (**path)
		return false;
	
	/* flatten index over all dimensions */
	{
		int totdim = 1;
		int flat_index = 0;
		
		for (i = dim - 1; i >= 0; i--) {
			flat_index += index_arr[i] * totdim;
			totdim *= len[i];
		}
		
		*r_index = flat_index;
	}
	return true;
}

static bool rna_path_parse(PointerRNA *ptr, const char *path,
                           PointerRNA *r_ptr, PropertyRNA **r_prop, int *r_index,
                           ListBase *r_elements,
                           const bool eval_pointer)
{
	PropertyRNA *prop;
	PointerRNA curptr;
	PropertyElemRNA *prop_elem = NULL;
	int index = -1;
	char fixedbuf[256];
	int type;

	prop = NULL;
	curptr = *ptr;

	if (path == NULL || *path == '\0')
		return false;

	while (*path) {
		int use_id_prop = (*path == '[') ? 1 : 0;
		char *token;
		/* custom property lookup ?
		 * C.object["someprop"]
		 */

		if (!curptr.data)
			return false;

		/* look up property name in current struct */
		token = rna_path_token(&path, fixedbuf, sizeof(fixedbuf), use_id_prop);

		if (!token)
			return false;

		prop = NULL;
		if (use_id_prop) { /* look up property name in current struct */
			IDProperty *group = RNA_struct_idprops(&curptr, 0);
			if (group && rna_token_strip_quotes(token))
				prop = (PropertyRNA *)IDP_GetPropertyFromGroup(group, token + 1);
		}
		else {
			prop = RNA_struct_find_property(&curptr, token);
		}

		if (token != fixedbuf)
			MEM_freeN(token);

		if (!prop)
			return false;

		if (r_elements) {
			prop_elem = MEM_mallocN(sizeof(PropertyElemRNA), __func__);
			prop_elem->ptr = curptr;
			prop_elem->prop = prop;
			prop_elem->index = -1;  /* index will be added later, if needed. */
			BLI_addtail(r_elements, prop_elem);
		}

		type = RNA_property_type(prop);

		/* now look up the value of this property if it is a pointer or
		 * collection, otherwise return the property rna so that the
		 * caller can read the value of the property itself */
		switch (type) {
			case PROP_POINTER: {
				/* resolve pointer if further path elements follow
				 * or explicitly requested
				 */
				if (eval_pointer || *path) {
					PointerRNA nextptr = RNA_property_pointer_get(&curptr, prop);
					
					curptr = nextptr;
					prop = NULL; /* now we have a PointerRNA, the prop is our parent so forget it */
					index = -1;
				}
				break;
			}
			case PROP_COLLECTION: {
				/* Resolve pointer if further path elements follow.
				 * Note that if path is empty, rna_path_parse_collection_key will do nothing anyway,
				 * so eval_pointer is of no use here (esp. as in this case, we want to keep found prop,
				 * erasing it breaks operators - e.g. bpy.types.Operator.bl_rna.foobar errors...).
				 */
				if (*path) {
					PointerRNA nextptr;
					if (!rna_path_parse_collection_key(&path, &curptr, prop, &nextptr))
						return false;
					
					curptr = nextptr;
					prop = NULL; /* now we have a PointerRNA, the prop is our parent so forget it */
					index = -1;
				}
				break;
			}
			default:
				if (r_index || prop_elem) {
					if (!rna_path_parse_array_index(&path, &curptr, prop, &index)) {
						return false;
					}

					if (prop_elem) {
						prop_elem->index = index;
					}
				}
				break;
		}
	}

	if (r_ptr)
		*r_ptr = curptr;
	if (r_prop)
		*r_prop = prop;
	if (r_index)
		*r_index = index;

	if (prop_elem && (prop_elem->ptr.data != curptr.data || prop_elem->prop != prop || prop_elem->index != index)) {
		PropertyElemRNA *prop_elem = MEM_mallocN(sizeof(PropertyElemRNA), __func__);
		prop_elem->ptr = curptr;
		prop_elem->prop = prop;
		prop_elem->index = index;
		BLI_addtail(r_elements, prop_elem);
	}

	return true;
}

/**
 * Resolve the given RNA Path to find the pointer and/or property indicated by fully resolving the path.
 *
 * \note Assumes all pointers provided are valid
 * \return True if path can be resolved to a valid "pointer + property" OR "pointer only"
 */
bool RNA_path_resolve(PointerRNA *ptr, const char *path, PointerRNA *r_ptr, PropertyRNA **r_prop)
{
	if (!rna_path_parse(ptr, path, r_ptr, r_prop, NULL, NULL, true))
		return false;

	return r_ptr->data != NULL;
}

/**
 * Resolve the given RNA Path to find the pointer and/or property + array index indicated by fully resolving the path.
 *
 * \note Assumes all pointers provided are valid.
 * \return True if path can be resolved to a valid "pointer + property" OR "pointer only"
 */
bool RNA_path_resolve_full(PointerRNA *ptr, const char *path, PointerRNA *r_ptr, PropertyRNA **r_prop, int *r_index)
{
	if (!rna_path_parse(ptr, path, r_ptr, r_prop, r_index, NULL, true))
		return false;

	return r_ptr->data != NULL;
}

/**
 * Resolve the given RNA Path to find both the pointer AND property indicated by fully resolving the path.
 *
 * This is a convenience method to avoid logic errors and ugly syntax.
 * \note Assumes all pointers provided are valid
 * \return True only if both a valid pointer and property are found after resolving the path
 */
bool RNA_path_resolve_property(PointerRNA *ptr, const char *path, PointerRNA *r_ptr, PropertyRNA **r_prop)
{
	if (!rna_path_parse(ptr, path, r_ptr, r_prop, NULL, NULL, false))
		return false;

	return r_ptr->data != NULL && *r_prop != NULL;
}

/**
 * Resolve the given RNA Path to find the pointer AND property (as well as the array index)
 * indicated by fully resolving the path.
 *
 * This is a convenience method to avoid logic errors and ugly syntax.
 *  \note Assumes all pointers provided are valid
 * \return True only if both a valid pointer and property are found after resolving the path
 */
bool RNA_path_resolve_property_full(PointerRNA *ptr, const char *path, PointerRNA *r_ptr, PropertyRNA **r_prop, int *r_index)
{
	if (!rna_path_parse(ptr, path, r_ptr, r_prop, r_index, NULL, false))
		return false;

	return r_ptr->data != NULL && *r_prop != NULL;
}

/**
 * Resolve the given RNA Path into a linked list of PropertyElemRNA's.
 *
 * To be used when complex operations over path are needed, like e.g. get relative paths, to avoid too much
 * string operations.
 *
 * \return True if there was no error while resolving the path
 * \note Assumes all pointers provided are valid
 */
bool RNA_path_resolve_elements(PointerRNA *ptr, const char *path, ListBase *r_elements)
{
	return rna_path_parse(ptr, path, NULL, NULL, NULL, r_elements, false);
}

char *RNA_path_append(const char *path, PointerRNA *UNUSED(ptr), PropertyRNA *prop, int intkey, const char *strkey)
{
	DynStr *dynstr;
	const char *s;
	char appendstr[128], *result;
	
	dynstr = BLI_dynstr_new();

	/* add .identifier */
	if (path) {
		BLI_dynstr_append(dynstr, (char *)path);
		if (*path)
			BLI_dynstr_append(dynstr, ".");
	}

	BLI_dynstr_append(dynstr, RNA_property_identifier(prop));

	if (RNA_property_type(prop) == PROP_COLLECTION) {
		/* add ["strkey"] or [intkey] */
		BLI_dynstr_append(dynstr, "[");

		if (strkey) {
			BLI_dynstr_append(dynstr, "\"");
			for (s = strkey; *s; s++) {
				if (*s == '[') {
					appendstr[0] = '\\';
					appendstr[1] = *s;
					appendstr[2] = 0;
				}
				else {
					appendstr[0] = *s;
					appendstr[1] = 0;
				}
				BLI_dynstr_append(dynstr, appendstr);
			}
			BLI_dynstr_append(dynstr, "\"");
		}
		else {
			BLI_snprintf(appendstr, sizeof(appendstr), "%d", intkey);
			BLI_dynstr_append(dynstr, appendstr);
		}

		BLI_dynstr_append(dynstr, "]");
	}

	result = BLI_dynstr_get_cstring(dynstr);
	BLI_dynstr_free(dynstr);

	return result;
}

char *RNA_path_back(const char *path)
{
	char fixedbuf[256];
	const char *previous, *current;
	char *result;
	int i;

	if (!path)
		return NULL;

	previous = NULL;
	current = path;

	/* parse token by token until the end, then we back up to the previous
	 * position and strip of the next token to get the path one step back */
	while (*current) {
		char *token;

		token = rna_path_token(&current, fixedbuf, sizeof(fixedbuf), 0);

		if (!token)
			return NULL;
		if (token != fixedbuf)
			MEM_freeN(token);

		/* in case of collection we also need to strip off [] */
		token = rna_path_token(&current, fixedbuf, sizeof(fixedbuf), 1);
		if (token && token != fixedbuf)
			MEM_freeN(token);
		
		if (!*current)
			break;

		previous = current;
	}

	if (!previous)
		return NULL;

	/* copy and strip off last token */
	i = previous - path;
	result = BLI_strdup(path);

	if (i > 0 && result[i - 1] == '.') i--;
	result[i] = 0;

	return result;
}

/* generic path search func
 * if its needed this could also reference the IDProperty direct */
typedef struct IDP_Chain {
	struct IDP_Chain *up; /* parent member, reverse and set to child for path conversion. */

	const char *name;
	int index;

} IDP_Chain;

static char *rna_idp_path_create(IDP_Chain *child_link)
{
	DynStr *dynstr = BLI_dynstr_new();
	char *path;
	bool is_first = true;

	int tot = 0;
	IDP_Chain *link = child_link;

	/* reverse the list */
	IDP_Chain *link_prev;
	link_prev = NULL;
	while (link) {
		IDP_Chain *link_next = link->up;
		link->up = link_prev;
		link_prev = link;
		link = link_next;
		tot++;
	}

	for (link = link_prev; link; link = link->up) {
		/* pass */
		if (link->index >= 0) {
			BLI_dynstr_appendf(dynstr, is_first ? "%s[%d]" : ".%s[%d]", link->name, link->index);
		}
		else {
			BLI_dynstr_appendf(dynstr, is_first ? "%s" : ".%s", link->name);
		}

		is_first = false;
	}

	path = BLI_dynstr_get_cstring(dynstr);
	BLI_dynstr_free(dynstr);

	if (*path == '\0') {
		MEM_freeN(path);
		path = NULL;
	}

	return path;
}

static char *rna_idp_path(PointerRNA *ptr, IDProperty *haystack, IDProperty *needle, IDP_Chain *parent_link)
{
	char *path = NULL;
	IDP_Chain link;

	IDProperty *iter;
	int i;

	BLI_assert(haystack->type == IDP_GROUP);

	link.up = parent_link;
	/* always set both name and index,
	 * else a stale value might get used */
	link.name = NULL;
	link.index = -1;

	for (i = 0, iter = haystack->data.group.first; iter; iter = iter->next, i++) {
		if (needle == iter) {  /* found! */
			link.name = iter->name;
			link.index = -1;
			path = rna_idp_path_create(&link);
			break;
		}
		else {
			if (iter->type == IDP_GROUP) {
				/* ensure this is RNA */
				PropertyRNA *prop = RNA_struct_find_property(ptr, iter->name);
				if (prop && prop->type == PROP_POINTER) {
					PointerRNA child_ptr = RNA_property_pointer_get(ptr, prop);
					link.name = iter->name;
					link.index = -1;
					if ((path = rna_idp_path(&child_ptr, iter, needle, &link))) {
						break;
					}
				}
			}
			else if (iter->type == IDP_IDPARRAY) {
				PropertyRNA *prop = RNA_struct_find_property(ptr, iter->name);
				if (prop && prop->type == PROP_COLLECTION) {
					IDProperty *array = IDP_IDPArray(iter);
					if (needle >= array && needle < (iter->len + array)) { /* found! */
						link.name = iter->name;
						link.index = (int)(needle - array);
						path = rna_idp_path_create(&link);
						break;
					}
					else {
						int i;
						link.name = iter->name;
						for (i = 0; i < iter->len; i++, array++) {
							PointerRNA child_ptr;
							if (RNA_property_collection_lookup_int(ptr, prop, i, &child_ptr)) {
								link.index = i;
								if ((path = rna_idp_path(&child_ptr, array, needle, &link))) {
									break;
								}
							}
						}
						if (path)
							break;
					}
				}
			}
		}
	}

	return path;
}

static char *rna_path_from_ID_to_idpgroup(PointerRNA *ptr)
{
	PointerRNA id_ptr;
	IDProperty *haystack;
	IDProperty *needle;

	BLI_assert(ptr->id.data != NULL);

	/* TODO, Support Bones/PoseBones. no pointers stored to the bones from here, only the ID. See example in [#25746]
	 *       Unless this is added only way to find this is to also search all bones and pose bones
	 *       of an armature or object */
	RNA_id_pointer_create(ptr->id.data, &id_ptr);

	haystack = RNA_struct_idprops(&id_ptr, false);
	if (haystack) { /* can fail when called on bones */
		needle = ptr->data;
		return rna_idp_path(&id_ptr, haystack, needle, NULL);
	}
	else {
		return NULL;
	}
}

char *RNA_path_from_ID_to_struct(PointerRNA *ptr)
{
	char *ptrpath = NULL;

	if (!ptr->id.data || !ptr->data)
		return NULL;
	
	if (!RNA_struct_is_ID(ptr->type)) {
		if (ptr->type->path) {
			/* if type has a path to some ID, use it */
			ptrpath = ptr->type->path(ptr);
		}
		else if (ptr->type->nested && RNA_struct_is_ID(ptr->type->nested)) {
			PointerRNA parentptr;
			PropertyRNA *userprop;
			
			/* find the property in the struct we're nested in that references this struct, and
			 * use its identifier as the first part of the path used...
			 */
			RNA_id_pointer_create(ptr->id.data, &parentptr);
			userprop = RNA_struct_find_nested(&parentptr, ptr->type);
			
			if (userprop)
				ptrpath = BLI_strdup(RNA_property_identifier(userprop));
			else
				return NULL;  /* can't do anything about this case yet... */
		}
		else if (RNA_struct_is_a(ptr->type, &RNA_PropertyGroup)) {
			/* special case, easier to deal with here then in ptr->type->path() */
			return rna_path_from_ID_to_idpgroup(ptr);
		}
		else
			return NULL;
	}
	
	return ptrpath;
}

char *RNA_path_from_ID_to_property(PointerRNA *ptr, PropertyRNA *prop)
{
	const bool is_rna = (prop->magic == RNA_MAGIC);
	const char *propname;
	char *ptrpath, *path;

	if (!ptr->id.data || !ptr->data)
		return NULL;
	
	/* path from ID to the struct holding this property */
	ptrpath = RNA_path_from_ID_to_struct(ptr);

	propname = RNA_property_identifier(prop);

	if (ptrpath) {
		if (is_rna) {
			path = BLI_sprintfN("%s.%s", ptrpath, propname);
		}
		else {
			char propname_esc[MAX_IDPROP_NAME * 2];
			BLI_strescape(propname_esc, propname, sizeof(propname_esc));
			path = BLI_sprintfN("%s[\"%s\"]", ptrpath, propname_esc);
		}
		MEM_freeN(ptrpath);
	}
	else if (RNA_struct_is_ID(ptr->type)) {
		if (is_rna) {
			path = BLI_strdup(propname);
		}
		else {
			char propname_esc[MAX_IDPROP_NAME * 2];
			BLI_strescape(propname_esc, propname, sizeof(propname_esc));
			path = BLI_sprintfN("[\"%s\"]", propname_esc);
		}
	}
	else {
		path = NULL;
	}

	return path;
}

/**
 * \return the path to given ptr/prop from the closest ancestor of given type, if any (else return NULL).
 */
char *RNA_path_resolve_from_type_to_property(
        PointerRNA *ptr, PropertyRNA *prop,
        const StructRNA *type)
{
	/* Try to recursively find an "type"'d ancestor,
	 * to handle situations where path from ID is not enough. */
	PointerRNA idptr;
	ListBase path_elems = {NULL};
	char *path = NULL;
	char *full_path = RNA_path_from_ID_to_property(ptr, prop);

	if (full_path == NULL) {
		return NULL;
	}

	RNA_id_pointer_create(ptr->id.data, &idptr);

	if (RNA_path_resolve_elements(&idptr, full_path, &path_elems)) {
		PropertyElemRNA *prop_elem;

		for (prop_elem = path_elems.last; prop_elem; prop_elem = prop_elem->prev) {
			if (RNA_struct_is_a(prop_elem->ptr.type, type)) {
				char *ref_path = RNA_path_from_ID_to_struct(&prop_elem->ptr);
				if (ref_path) {
					path = BLI_strdup(full_path + strlen(ref_path) + 1);  /* +1 for the linking '.' */
					MEM_freeN(ref_path);
				}
				break;
			}
		}

		BLI_freelistN(&path_elems);
	}

	MEM_freeN(full_path);
	return path;
}

/**
 * Get the ID as a python representation, eg:
 *   bpy.data.foo["bar"]
 */
char *RNA_path_full_ID_py(ID *id)
{
	char id_esc[(sizeof(id->name) - 2) * 2];

	BLI_strescape(id_esc, id->name + 2, sizeof(id_esc));

	return BLI_sprintfN("bpy.data.%s[\"%s\"]", BKE_idcode_to_name_plural(GS(id->name)), id_esc);
}

/**
 * Get the ID.struct as a python representation, eg:
 *   bpy.data.foo["bar"].some_struct
 */
char *RNA_path_full_struct_py(struct PointerRNA *ptr)
{
	char *id_path;
	char *data_path;

	char *ret;

	if (!ptr->id.data) {
		return NULL;
	}

	/* never fails */
	id_path = RNA_path_full_ID_py(ptr->id.data);

	data_path = RNA_path_from_ID_to_struct(ptr);

	/* XXX data_path may be NULL (see #36788), do we want to get the 'bpy.data.foo["bar"].(null)' stuff? */
	ret = BLI_sprintfN("%s.%s", id_path, data_path);

	if (data_path) {
		MEM_freeN(data_path);
	}
	MEM_freeN(id_path);

	return ret;
}

/**
 * Get the ID.struct.property as a python representation, eg:
 *   bpy.data.foo["bar"].some_struct.some_prop[10]
 */
char *RNA_path_full_property_py(PointerRNA *ptr, PropertyRNA *prop, int index)
{
	char *id_path;
	char *data_path;

	char *ret;

	if (!ptr->id.data) {
		return NULL;
	}

	/* never fails */
	id_path = RNA_path_full_ID_py(ptr->id.data);

	data_path = RNA_path_from_ID_to_property(ptr, prop);

	if ((index == -1) || (RNA_property_array_check(prop) == false)) {
		ret = BLI_sprintfN("%s.%s",
		                   id_path, data_path);
	}
	else {
		ret = BLI_sprintfN("%s.%s[%d]",
		                   id_path, data_path, index);
	}
	MEM_freeN(id_path);
	if (data_path) {
		MEM_freeN(data_path);
	}

	return ret;
}

/**
 * Get the struct.property as a python representation, eg:
 *   some_struct.some_prop[10]
 */
char *RNA_path_struct_property_py(PointerRNA *ptr, PropertyRNA *prop, int index)
{
	char *data_path;

	char *ret;

	if (!ptr->id.data) {
		return NULL;
	}

	data_path = RNA_path_from_ID_to_property(ptr, prop);

	if (data_path == NULL) {
		/* this may not be an ID at all, check for simple when pointer owns property.
		 * TODO, more complex nested case */
		if (!RNA_struct_is_ID(ptr->type)) {
			if (RNA_struct_find_property(ptr, RNA_property_identifier(prop)) == prop) {
				data_path = BLI_strdup(RNA_property_identifier(prop));
			}
		}
	}

	if ((index == -1) || (RNA_property_array_check(prop) == false)) {
		ret = BLI_sprintfN("%s",
		                   data_path);
	}
	else {
		ret = BLI_sprintfN("%s[%d]",
		                   data_path, index);
	}

	if (data_path) {
		MEM_freeN(data_path);
	}

	return ret;
}

/**
 * Get the struct.property as a python representation, eg:
 *   some_prop[10]
 */
char *RNA_path_property_py(PointerRNA *UNUSED(ptr), PropertyRNA *prop, int index)
{
	char *ret;

	if ((index == -1) || (RNA_property_array_check(prop) == false)) {
		ret = BLI_sprintfN("%s",
		                   RNA_property_identifier(prop));
	}
	else {
		ret = BLI_sprintfN("%s[%d]",
		                   RNA_property_identifier(prop), index);
	}

	return ret;
}

/* Quick name based property access */

int RNA_boolean_get(PointerRNA *ptr, const char *name)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, name);

	if (prop) {
		return RNA_property_boolean_get(ptr, prop);
	}
	else {
		printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
		return 0;
	}
}

void RNA_boolean_set(PointerRNA *ptr, const char *name, int value)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, name);

	if (prop)
		RNA_property_boolean_set(ptr, prop, value);
	else
		printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
}

void RNA_boolean_get_array(PointerRNA *ptr, const char *name, int *values)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, name);

	if (prop)
		RNA_property_boolean_get_array(ptr, prop, values);
	else
		printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
}

void RNA_boolean_set_array(PointerRNA *ptr, const char *name, const int *values)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, name);

	if (prop)
		RNA_property_boolean_set_array(ptr, prop, values);
	else
		printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
}

int RNA_int_get(PointerRNA *ptr, const char *name)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, name);

	if (prop) {
		return RNA_property_int_get(ptr, prop);
	}
	else {
		printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
		return 0;
	}
}

void RNA_int_set(PointerRNA *ptr, const char *name, int value)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, name);

	if (prop)
		RNA_property_int_set(ptr, prop, value);
	else
		printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
}

void RNA_int_get_array(PointerRNA *ptr, const char *name, int *values)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, name);

	if (prop)
		RNA_property_int_get_array(ptr, prop, values);
	else
		printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
}

void RNA_int_set_array(PointerRNA *ptr, const char *name, const int *values)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, name);

	if (prop)
		RNA_property_int_set_array(ptr, prop, values);
	else
		printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
}

float RNA_float_get(PointerRNA *ptr, const char *name)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, name);

	if (prop) {
		return RNA_property_float_get(ptr, prop);
	}
	else {
		printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
		return 0;
	}
}

void RNA_float_set(PointerRNA *ptr, const char *name, float value)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, name);

	if (prop)
		RNA_property_float_set(ptr, prop, value);
	else
		printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
}

void RNA_float_get_array(PointerRNA *ptr, const char *name, float *values)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, name);

	if (prop)
		RNA_property_float_get_array(ptr, prop, values);
	else
		printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
}

void RNA_float_set_array(PointerRNA *ptr, const char *name, const float *values)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, name);

	if (prop)
		RNA_property_float_set_array(ptr, prop, values);
	else
		printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
}

int RNA_enum_get(PointerRNA *ptr, const char *name)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, name);

	if (prop) {
		return RNA_property_enum_get(ptr, prop);
	}
	else {
		printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
		return 0;
	}
}

void RNA_enum_set(PointerRNA *ptr, const char *name, int value)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, name);

	if (prop)
		RNA_property_enum_set(ptr, prop, value);
	else
		printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
}

void RNA_enum_set_identifier(PointerRNA *ptr, const char *name, const char *id)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, name);

	if (prop) {
		int value;
		if (RNA_property_enum_value(NULL, ptr, prop, id, &value))
			RNA_property_enum_set(ptr, prop, value);
		else
			printf("%s: %s.%s has no enum id '%s'.\n", __func__, ptr->type->identifier, name, id);
	}
	else {
		printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
	}
}

bool RNA_enum_is_equal(bContext *C, PointerRNA *ptr, const char *name, const char *enumname)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, name);
	EnumPropertyItem *item;
	bool free;

	if (prop) {
		int i;
		bool cmp;

		RNA_property_enum_items(C, ptr, prop, &item, NULL, &free);
		i = RNA_enum_from_identifier(item, enumname);
		if (i != -1) {
			cmp = (item[i].value == RNA_property_enum_get(ptr, prop));
		}

		if (free) {
			MEM_freeN(item);
		}

		if (i != -1) {
			return cmp;
		}

		printf("%s: %s.%s item %s not found.\n", __func__, ptr->type->identifier, name, enumname);
		return false;
	}
	else {
		printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
		return false;
	}
}

bool RNA_enum_value_from_id(EnumPropertyItem *item, const char *identifier, int *r_value)
{
	const int i = RNA_enum_from_identifier(item, identifier);
	if (i != -1) {
		*r_value = item[i].value;
		return true;
	}
	else {
		return false;
	}
}

bool RNA_enum_id_from_value(EnumPropertyItem *item, int value, const char **r_identifier)
{
	const int i = RNA_enum_from_value(item, value);
	if (i != -1) {
		*r_identifier = item[i].identifier;
		return true;
	}
	else {
		return false;
	}
}

bool RNA_enum_icon_from_value(EnumPropertyItem *item, int value, int *r_icon)
{
	const int i = RNA_enum_from_value(item, value);
	if (i != -1) {
		*r_icon = item[i].icon;
		return true;
	}
	else {
		return false;
	}
}

bool RNA_enum_name_from_value(EnumPropertyItem *item, int value, const char **r_name)
{
	const int i = RNA_enum_from_value(item, value);
	if (i != -1) {
		*r_name = item[i].name;
		return true;
	}
	else {
		return false;
	}
}

void RNA_string_get(PointerRNA *ptr, const char *name, char *value)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, name);

	if (prop) {
		RNA_property_string_get(ptr, prop, value);
	}
	else {
		printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
		value[0] = '\0';
	}
}

char *RNA_string_get_alloc(PointerRNA *ptr, const char *name, char *fixedbuf, int fixedlen)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, name);

	if (prop) {
		return RNA_property_string_get_alloc(ptr, prop, fixedbuf, fixedlen, NULL); /* TODO, pass length */
	}
	else {
		printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
		return NULL;
	}
}

int RNA_string_length(PointerRNA *ptr, const char *name)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, name);

	if (prop) {
		return RNA_property_string_length(ptr, prop);
	}
	else {
		printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
		return 0;
	}
}

void RNA_string_set(PointerRNA *ptr, const char *name, const char *value)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, name);

	if (prop)
		RNA_property_string_set(ptr, prop, value);
	else
		printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
}

PointerRNA RNA_pointer_get(PointerRNA *ptr, const char *name)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, name);

	if (prop) {
		return RNA_property_pointer_get(ptr, prop);
	}
	else {
		printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);

		return PointerRNA_NULL;
	}
}

void RNA_pointer_set(PointerRNA *ptr, const char *name, PointerRNA ptr_value)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, name);

	if (prop) {
		RNA_property_pointer_set(ptr, prop, ptr_value);
	}
	else {
		printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
	}
}

void RNA_pointer_add(PointerRNA *ptr, const char *name)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, name);

	if (prop)
		RNA_property_pointer_add(ptr, prop);
	else
		printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
}

void RNA_collection_begin(PointerRNA *ptr, const char *name, CollectionPropertyIterator *iter)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, name);

	if (prop)
		RNA_property_collection_begin(ptr, prop, iter);
	else
		printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
}

void RNA_collection_add(PointerRNA *ptr, const char *name, PointerRNA *r_value)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, name);

	if (prop)
		RNA_property_collection_add(ptr, prop, r_value);
	else
		printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
}

void RNA_collection_clear(PointerRNA *ptr, const char *name)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, name);

	if (prop)
		RNA_property_collection_clear(ptr, prop);
	else
		printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
}

int RNA_collection_length(PointerRNA *ptr, const char *name)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, name);

	if (prop) {
		return RNA_property_collection_length(ptr, prop);
	}
	else {
		printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
		return 0;
	}
}

bool RNA_property_is_set_ex(PointerRNA *ptr, PropertyRNA *prop, bool use_ghost)
{
	prop = rna_ensure_property(prop);
	if (prop->flag & PROP_IDPROPERTY) {
		IDProperty *idprop = rna_idproperty_find(ptr, prop->identifier);
		return ((idprop != NULL) && (use_ghost == false || !(idprop->flag & IDP_FLAG_GHOST)));
	}
	else {
		return true;
	}
}

bool RNA_property_is_set(PointerRNA *ptr, PropertyRNA *prop)
{
	prop = rna_ensure_property(prop);
	if (prop->flag & PROP_IDPROPERTY) {
		IDProperty *idprop = rna_idproperty_find(ptr, prop->identifier);
		return ((idprop != NULL) && !(idprop->flag & IDP_FLAG_GHOST));
	}
	else {
		return true;
	}
}

void RNA_property_unset(PointerRNA *ptr, PropertyRNA *prop)
{
	prop = rna_ensure_property(prop);
	if (prop->flag & PROP_IDPROPERTY) {
		rna_idproperty_free(ptr, prop->identifier);
	}
}

bool RNA_struct_property_is_set_ex(PointerRNA *ptr, const char *identifier, bool use_ghost)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, identifier);

	if (prop) {
		return RNA_property_is_set_ex(ptr, prop, use_ghost);
	}
	else {
		/* python raises an error */
		/* printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name); */
		return 0;
	}
}

bool RNA_struct_property_is_set(PointerRNA *ptr, const char *identifier)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, identifier);

	if (prop) {
		return RNA_property_is_set(ptr, prop);
	}
	else {
		/* python raises an error */
		/* printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name); */
		return 0;
	}
}

void RNA_struct_property_unset(PointerRNA *ptr, const char *identifier)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, identifier);

	if (prop) {
		RNA_property_unset(ptr, prop);
	}
}

bool RNA_property_is_idprop(PropertyRNA *prop)
{
	return (prop->magic != RNA_MAGIC);
}

/* mainly for the UI */
bool RNA_property_is_unlink(PropertyRNA *prop)
{
	const int flag = RNA_property_flag(prop);
	if (RNA_property_type(prop) == PROP_STRING) {
		return (flag & PROP_NEVER_UNLINK) == 0;
	}
	else {
		return (flag & (PROP_NEVER_UNLINK | PROP_NEVER_NULL)) == 0;
	}
}

/* string representation of a property, python
 * compatible but can be used for display too,
 * context may be NULL */
char *RNA_pointer_as_string_id(bContext *C, PointerRNA *ptr)
{
	DynStr *dynstr = BLI_dynstr_new();
	char *cstring;
	
	const char *propname;
	int first_time = 1;
	
	BLI_dynstr_append(dynstr, "{");
	
	RNA_STRUCT_BEGIN (ptr, prop)
	{
		propname = RNA_property_identifier(prop);
		
		if (strcmp(propname, "rna_type") == 0)
			continue;
		
		if (first_time == 0)
			BLI_dynstr_append(dynstr, ", ");
		first_time = 0;
		
		cstring = RNA_property_as_string(C, ptr, prop, -1, INT_MAX);
		BLI_dynstr_appendf(dynstr, "\"%s\":%s", propname, cstring);
		MEM_freeN(cstring);
	}
	RNA_STRUCT_END;

	BLI_dynstr_append(dynstr, "}");
	
	
	cstring = BLI_dynstr_get_cstring(dynstr);
	BLI_dynstr_free(dynstr);
	return cstring;
}

static char *rna_pointer_as_string__bldata(PointerRNA *ptr)
{
	if (ptr->type == NULL) {
		return BLI_strdup("None");
	}
	else if (RNA_struct_is_ID(ptr->type)) {
		return RNA_path_full_ID_py(ptr->id.data);
	}
	else {
		return RNA_path_full_struct_py(ptr);
	}
}

char *RNA_pointer_as_string(bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *prop_ptr, PointerRNA *ptr_prop)
{
	if (RNA_property_flag(prop_ptr) & PROP_IDPROPERTY) {
		return RNA_pointer_as_string_id(C, ptr_prop);
	}
	else {
		return rna_pointer_as_string__bldata(ptr_prop);
	}
}

/* context can be NULL */
char *RNA_pointer_as_string_keywords_ex(bContext *C, PointerRNA *ptr,
                                        const bool as_function, const bool all_args, const bool nested_args,
                                        const int max_prop_length,
                                        PropertyRNA *iterprop)
{
	const char *arg_name = NULL;

	PropertyRNA *prop;

	DynStr *dynstr = BLI_dynstr_new();
	char *cstring, *buf;
	bool first_iter = true;
	int flag;

	RNA_PROP_BEGIN (ptr, propptr, iterprop)
	{
		prop = propptr.data;

		flag = RNA_property_flag(prop);

		if (as_function && (flag & PROP_OUTPUT)) {
			continue;
		}

		arg_name = RNA_property_identifier(prop);

		if (strcmp(arg_name, "rna_type") == 0) {
			continue;
		}

		if ((nested_args == false) && (RNA_property_type(prop) == PROP_POINTER)) {
			continue;
		}

		if (as_function && (flag & PROP_REQUIRED)) {
			/* required args don't have useful defaults */
			BLI_dynstr_appendf(dynstr, first_iter ? "%s" : ", %s", arg_name);
			first_iter = false;
		}
		else {
			bool ok = true;

			if (all_args == true) {
				/* pass */
			}
			else if (RNA_struct_idprops_check(ptr->type)) {
				ok = RNA_property_is_set(ptr, prop);
			}

			if (ok) {
				if (as_function && RNA_property_type(prop) == PROP_POINTER) {
					/* don't expand pointers for functions */
					if (flag & PROP_NEVER_NULL) {
						/* we cant really do the right thing here. arg=arg?, hrmf! */
						buf = BLI_strdup(arg_name);
					}
					else {
						buf = BLI_strdup("None");
					}
				}
				else {
					buf = RNA_property_as_string(C, ptr, prop, -1, max_prop_length);
				}

				BLI_dynstr_appendf(dynstr, first_iter ? "%s=%s" : ", %s=%s", arg_name, buf);
				first_iter = false;
				MEM_freeN(buf);
			}
		}
	}
	RNA_PROP_END;

	cstring = BLI_dynstr_get_cstring(dynstr);
	BLI_dynstr_free(dynstr);
	return cstring;
}

char *RNA_pointer_as_string_keywords(bContext *C, PointerRNA *ptr,
                                     const bool as_function, const bool all_args, const bool nested_args,
                                     const int max_prop_length)
{
	PropertyRNA *iterprop;

	iterprop = RNA_struct_iterator_property(ptr->type);

	return RNA_pointer_as_string_keywords_ex(C, ptr, as_function, all_args, nested_args,
	                                         max_prop_length, iterprop);
}

char *RNA_function_as_string_keywords(bContext *C, FunctionRNA *func,
                                      const bool as_function, const bool all_args,
                                      const int max_prop_length)
{
	PointerRNA funcptr;
	PropertyRNA *iterprop;

	RNA_pointer_create(NULL, &RNA_Function, func, &funcptr);

	iterprop = RNA_struct_find_property(&funcptr, "parameters");

	RNA_struct_iterator_property(funcptr.type);

	return RNA_pointer_as_string_keywords_ex(C, &funcptr, as_function, all_args, true,
	                                         max_prop_length, iterprop);
}

static const char *bool_as_py_string(const int var)
{
	return var ? "True" : "False";
}

char *RNA_property_as_string(bContext *C, PointerRNA *ptr, PropertyRNA *prop, int index, int max_prop_length)
{
	int type = RNA_property_type(prop);
	int len = RNA_property_array_length(ptr, prop);
	int i;

	DynStr *dynstr = BLI_dynstr_new();
	char *cstring;
	

	/* see if we can coerce into a python type - PropertyType */
	switch (type) {
		case PROP_BOOLEAN:
			if (len == 0) {
				BLI_dynstr_append(dynstr, bool_as_py_string(RNA_property_boolean_get(ptr, prop)));
			}
			else {
				if (index != -1) {
					BLI_dynstr_append(dynstr, bool_as_py_string(RNA_property_boolean_get_index(ptr, prop, index)));
				}
				else {
					BLI_dynstr_append(dynstr, "(");
					for (i = 0; i < len; i++) {
						BLI_dynstr_appendf(dynstr, i ? ", %s" : "%s",
						                   bool_as_py_string(RNA_property_boolean_get_index(ptr, prop, i)));
					}
					if (len == 1)
						BLI_dynstr_append(dynstr, ",");  /* otherwise python wont see it as a tuple */
					BLI_dynstr_append(dynstr, ")");
				}
			}
			break;
		case PROP_INT:
			if (len == 0) {
				BLI_dynstr_appendf(dynstr, "%d", RNA_property_int_get(ptr, prop));
			}
			else {
				if (index != -1) {
					BLI_dynstr_appendf(dynstr, "%d", RNA_property_int_get_index(ptr, prop, index));
				}
				else {
					BLI_dynstr_append(dynstr, "(");
					for (i = 0; i < len; i++) {
						BLI_dynstr_appendf(dynstr, i ? ", %d" : "%d", RNA_property_int_get_index(ptr, prop, i));
					}
					if (len == 1)
						BLI_dynstr_append(dynstr, ",");  /* otherwise python wont see it as a tuple */
					BLI_dynstr_append(dynstr, ")");
				}
			}
			break;
		case PROP_FLOAT:
			if (len == 0) {
				BLI_dynstr_appendf(dynstr, "%g", RNA_property_float_get(ptr, prop));
			}
			else {
				if (index != -1) {
					BLI_dynstr_appendf(dynstr, "%g", RNA_property_float_get_index(ptr, prop, index));
				}
				else {
					BLI_dynstr_append(dynstr, "(");
					for (i = 0; i < len; i++) {
						BLI_dynstr_appendf(dynstr, i ? ", %g" : "%g", RNA_property_float_get_index(ptr, prop, i));
					}
					if (len == 1)
						BLI_dynstr_append(dynstr, ",");  /* otherwise python wont see it as a tuple */
					BLI_dynstr_append(dynstr, ")");
				}
			}
			break;
		case PROP_STRING:
		{
			char *buf_esc;
			char *buf;
			int length;

			length = RNA_property_string_length(ptr, prop);
			buf = MEM_mallocN(sizeof(char) * (length + 1), "RNA_property_as_string");
			buf_esc = MEM_mallocN(sizeof(char) * (length * 2 + 1), "RNA_property_as_string esc");
			RNA_property_string_get(ptr, prop, buf);
			BLI_strescape(buf_esc, buf, length * 2 + 1);
			MEM_freeN(buf);
			BLI_dynstr_appendf(dynstr, "\"%s\"", buf_esc);
			MEM_freeN(buf_esc);
			break;
		}
		case PROP_ENUM:
		{
			/* string arrays don't exist */
			const char *identifier;
			int val = RNA_property_enum_get(ptr, prop);

			if (RNA_property_flag(prop) & PROP_ENUM_FLAG) {
				/* represent as a python set */
				if (val) {
					EnumPropertyItem *item_array;
					bool free;

					BLI_dynstr_append(dynstr, "{");

					RNA_property_enum_items(C, ptr, prop, &item_array, NULL, &free);
					if (item_array) {
						EnumPropertyItem *item = item_array;
						bool is_first = true;
						for (; item->identifier; item++) {
							if (item->identifier[0] && item->value & val) {
								BLI_dynstr_appendf(dynstr, is_first ? "'%s'" : ", '%s'", item->identifier);
								is_first = false;
							}
						}

						if (free) {
							MEM_freeN(item_array);
						}
					}

					BLI_dynstr_append(dynstr, "}");
				}
				else {
					/* annoying exception, don't confuse with dictionary syntax above: {} */
					BLI_dynstr_append(dynstr, "set()");
				}
			}
			else if (RNA_property_enum_identifier(C, ptr, prop, val, &identifier)) {
				BLI_dynstr_appendf(dynstr, "'%s'", identifier);
			}
			else {
				BLI_dynstr_append(dynstr, "'<UNKNOWN ENUM>'");
			}
			break;
		}
		case PROP_POINTER:
		{
			PointerRNA tptr = RNA_property_pointer_get(ptr, prop);
			cstring = RNA_pointer_as_string(C, ptr, prop, &tptr);
			BLI_dynstr_append(dynstr, cstring);
			MEM_freeN(cstring);
			break;
		}
		case PROP_COLLECTION:
		{
			int i = 0;
			CollectionPropertyIterator collect_iter;
			BLI_dynstr_append(dynstr, "[");

			for (RNA_property_collection_begin(ptr, prop, &collect_iter);
			     (i < max_prop_length) && collect_iter.valid;
			     RNA_property_collection_next(&collect_iter), i++)
			{
				PointerRNA itemptr = collect_iter.ptr;

				if (i != 0)
					BLI_dynstr_append(dynstr, ", ");

				/* now get every prop of the collection */
				cstring = RNA_pointer_as_string(C, ptr, prop, &itemptr);
				BLI_dynstr_append(dynstr, cstring);
				MEM_freeN(cstring);
			}
		
			RNA_property_collection_end(&collect_iter);
			BLI_dynstr_append(dynstr, "]");
			break;
		}
		default:
			BLI_dynstr_append(dynstr, "'<UNKNOWN TYPE>'"); /* TODO */
			break;
	}

	cstring = BLI_dynstr_get_cstring(dynstr);
	BLI_dynstr_free(dynstr);
	return cstring;
}

/* Function */

const char *RNA_function_identifier(FunctionRNA *func)
{
	return func->identifier;
}

const char *RNA_function_ui_description(FunctionRNA *func)
{
	return TIP_(func->description);
}

const char *RNA_function_ui_description_raw(FunctionRNA *func)
{
	return func->description;
}

int RNA_function_flag(FunctionRNA *func)
{
	return func->flag;
}

int RNA_function_defined(FunctionRNA *func)
{
	return func->call != NULL;
}

PropertyRNA *RNA_function_get_parameter(PointerRNA *UNUSED(ptr), FunctionRNA *func, int index)
{
	return BLI_findlink(&func->cont.properties, index);
}

PropertyRNA *RNA_function_find_parameter(PointerRNA *UNUSED(ptr), FunctionRNA *func, const char *identifier)
{
	PropertyRNA *parm;

	parm = func->cont.properties.first;
	for (; parm; parm = parm->next)
		if (strcmp(RNA_property_identifier(parm), identifier) == 0)
			break;

	return parm;
}

const ListBase *RNA_function_defined_parameters(FunctionRNA *func)
{
	return &func->cont.properties;
}

/* Utility */

ParameterList *RNA_parameter_list_create(ParameterList *parms, PointerRNA *UNUSED(ptr), FunctionRNA *func)
{
	PropertyRNA *parm;
	void *data;
	int alloc_size = 0, size;

	parms->arg_count = 0;
	parms->ret_count = 0;

	/* allocate data */
	for (parm = func->cont.properties.first; parm; parm = parm->next) {
		alloc_size += rna_parameter_size(parm);

		if (parm->flag & PROP_OUTPUT)
			parms->ret_count++;
		else
			parms->arg_count++;
	}

	parms->data = MEM_callocN(alloc_size, "RNA_parameter_list_create");
	parms->func = func;
	parms->alloc_size = alloc_size;

	/* set default values */
	data = parms->data;

	for (parm = func->cont.properties.first; parm; parm = parm->next) {
		size = rna_parameter_size(parm);

		/* set length to 0, these need to be set later, see bpy_array.c's py_to_array */
		if (parm->flag & PROP_DYNAMIC) {
			ParameterDynAlloc *data_alloc = data;
			data_alloc->array_tot = 0;
			data_alloc->array = NULL;
		}
		
		if (!(parm->flag & PROP_REQUIRED) && !(parm->flag & PROP_DYNAMIC)) {
			switch (parm->type) {
				case PROP_BOOLEAN:
					if (parm->arraydimension) memcpy(data, ((BoolPropertyRNA *)parm)->defaultarray, size);
					else memcpy(data, &((BoolPropertyRNA *)parm)->defaultvalue, size);
					break;
				case PROP_INT:
					if (parm->arraydimension) memcpy(data, ((IntPropertyRNA *)parm)->defaultarray, size);
					else memcpy(data, &((IntPropertyRNA *)parm)->defaultvalue, size);
					break;
				case PROP_FLOAT:
					if (parm->arraydimension) memcpy(data, ((FloatPropertyRNA *)parm)->defaultarray, size);
					else memcpy(data, &((FloatPropertyRNA *)parm)->defaultvalue, size);
					break;
				case PROP_ENUM:
					memcpy(data, &((EnumPropertyRNA *)parm)->defaultvalue, size);
					break;
				case PROP_STRING:
				{
					const char *defvalue = ((StringPropertyRNA *)parm)->defaultvalue;
					if (defvalue && defvalue[0]) {
						/* causes bug [#29988], possibly this is only correct for thick wrapped
						 * need to look further into it - campbell */
#if 0
						BLI_strncpy(data, defvalue, size);
#else
						memcpy(data, &defvalue, size);
#endif
					}
					break;
				}
				case PROP_POINTER:
				case PROP_COLLECTION:
					break;
			}
		}

		data = ((char *)data) + rna_parameter_size(parm);
	}

	return parms;
}

void RNA_parameter_list_free(ParameterList *parms)
{
	PropertyRNA *parm;
	int tot;

	parm = parms->func->cont.properties.first;
	for (tot = 0; parm; parm = parm->next) {
		if (parm->type == PROP_COLLECTION)
			BLI_freelistN((ListBase *)((char *)parms->data + tot));
		else if (parm->flag & PROP_DYNAMIC) {
			/* for dynamic arrays and strings, data is a pointer to an array */
			ParameterDynAlloc *data_alloc = (void *)(((char *)parms->data) + tot);
			if (data_alloc->array)
				MEM_freeN(data_alloc->array);
		}

		tot += rna_parameter_size(parm);
	}

	MEM_freeN(parms->data);
	parms->data = NULL;

	parms->func = NULL;
}

int  RNA_parameter_list_size(ParameterList *parms)
{
	return parms->alloc_size;
}

int  RNA_parameter_list_arg_count(ParameterList *parms)
{
	return parms->arg_count;
}

int  RNA_parameter_list_ret_count(ParameterList *parms)
{
	return parms->ret_count;
}

void RNA_parameter_list_begin(ParameterList *parms, ParameterIterator *iter)
{
	/* may be useful but unused now */
	/* RNA_pointer_create(NULL, &RNA_Function, parms->func, &iter->funcptr); */ /*UNUSED*/

	iter->parms = parms;
	iter->parm = parms->func->cont.properties.first;
	iter->valid = iter->parm != NULL;
	iter->offset = 0;

	if (iter->valid) {
		iter->size = rna_parameter_size(iter->parm);
		iter->data = (((char *)iter->parms->data)); /* +iter->offset, always 0 */
	}
}

void RNA_parameter_list_next(ParameterIterator *iter)
{
	iter->offset += iter->size;
	iter->parm = iter->parm->next;
	iter->valid = iter->parm != NULL;

	if (iter->valid) {
		iter->size = rna_parameter_size(iter->parm);
		iter->data = (((char *)iter->parms->data) + iter->offset);
	}
}

void RNA_parameter_list_end(ParameterIterator *UNUSED(iter))
{
	/* nothing to do */
}

void RNA_parameter_get(ParameterList *parms, PropertyRNA *parm, void **value)
{
	ParameterIterator iter;

	RNA_parameter_list_begin(parms, &iter);

	for (; iter.valid; RNA_parameter_list_next(&iter))
		if (iter.parm == parm)
			break;

	if (iter.valid) {
		if (parm->flag & PROP_DYNAMIC) {
			/* for dynamic arrays and strings, data is a pointer to an array */
			ParameterDynAlloc *data_alloc = iter.data;
			*value = data_alloc->array;
		}
		else {
			*value = iter.data;
		}
	}
	else {
		*value = NULL;
	}

	RNA_parameter_list_end(&iter);
}

void RNA_parameter_get_lookup(ParameterList *parms, const char *identifier, void **value)
{
	PropertyRNA *parm;

	parm = parms->func->cont.properties.first;
	for (; parm; parm = parm->next)
		if (strcmp(RNA_property_identifier(parm), identifier) == 0)
			break;

	if (parm)
		RNA_parameter_get(parms, parm, value);
}

void RNA_parameter_set(ParameterList *parms, PropertyRNA *parm, const void *value)
{
	ParameterIterator iter;

	RNA_parameter_list_begin(parms, &iter);

	for (; iter.valid; RNA_parameter_list_next(&iter))
		if (iter.parm == parm)
			break;

	if (iter.valid) {
		if (parm->flag & PROP_DYNAMIC) {
			/* for dynamic arrays and strings, data is a pointer to an array */
			ParameterDynAlloc *data_alloc = iter.data;
			size_t size = 0;
			switch (parm->type) {
				case PROP_STRING:
					size = sizeof(char);
					break;
				case PROP_INT:
				case PROP_BOOLEAN:
					size = sizeof(int);
					break;
				case PROP_FLOAT:
					size = sizeof(float);
					break;
				default:
					break;
			}
			size *= data_alloc->array_tot;
			if (data_alloc->array)
				MEM_freeN(data_alloc->array);
			data_alloc->array = MEM_mallocN(size, __func__);
			memcpy(data_alloc->array, value, size);
		}
		else {
			memcpy(iter.data, value, iter.size);
		}
	}

	RNA_parameter_list_end(&iter);
}

void RNA_parameter_set_lookup(ParameterList *parms, const char *identifier, const void *value)
{
	PropertyRNA *parm;

	parm = parms->func->cont.properties.first;
	for (; parm; parm = parm->next)
		if (strcmp(RNA_property_identifier(parm), identifier) == 0)
			break;

	if (parm)
		RNA_parameter_set(parms, parm, value);
}

int RNA_parameter_dynamic_length_get(ParameterList *parms, PropertyRNA *parm)
{
	ParameterIterator iter;
	int len = 0;

	RNA_parameter_list_begin(parms, &iter);

	for (; iter.valid; RNA_parameter_list_next(&iter))
		if (iter.parm == parm)
			break;

	if (iter.valid)
		len = RNA_parameter_dynamic_length_get_data(parms, parm, iter.data);

	RNA_parameter_list_end(&iter);

	return len;
}

void RNA_parameter_dynamic_length_set(ParameterList *parms, PropertyRNA *parm, int length)
{
	ParameterIterator iter;

	RNA_parameter_list_begin(parms, &iter);

	for (; iter.valid; RNA_parameter_list_next(&iter))
		if (iter.parm == parm)
			break;

	if (iter.valid)
		RNA_parameter_dynamic_length_set_data(parms, parm, iter.data, length);

	RNA_parameter_list_end(&iter);
}

int RNA_parameter_dynamic_length_get_data(ParameterList *UNUSED(parms), PropertyRNA *parm, void *data)
{
	if (parm->flag & PROP_DYNAMIC) {
		return (int)((ParameterDynAlloc *)data)->array_tot;
	}
	return 0;
}

void RNA_parameter_dynamic_length_set_data(ParameterList *UNUSED(parms), PropertyRNA *parm, void *data, int length)
{
	if (parm->flag & PROP_DYNAMIC) {
		((ParameterDynAlloc *)data)->array_tot = (intptr_t)length;
	}
}

int RNA_function_call(bContext *C, ReportList *reports, PointerRNA *ptr, FunctionRNA *func, ParameterList *parms)
{
	if (func->call) {
		func->call(C, reports, ptr, parms);

		return 0;
	}

	return -1;
}

int RNA_function_call_lookup(bContext *C, ReportList *reports, PointerRNA *ptr, const char *identifier,
                             ParameterList *parms)
{
	FunctionRNA *func;

	func = RNA_struct_find_function(ptr->type, identifier);

	if (func)
		return RNA_function_call(C, reports, ptr, func, parms);

	return -1;
}

int RNA_function_call_direct(bContext *C, ReportList *reports, PointerRNA *ptr, FunctionRNA *func,
                             const char *format, ...)
{
	va_list args;
	int ret;

	va_start(args, format);

	ret = RNA_function_call_direct_va(C, reports, ptr, func, format, args);

	va_end(args);

	return ret;
}

int RNA_function_call_direct_lookup(bContext *C, ReportList *reports, PointerRNA *ptr, const char *identifier,
                                    const char *format, ...)
{
	FunctionRNA *func;

	func = RNA_struct_find_function(ptr->type, identifier);

	if (func) {
		va_list args;
		int ret;

		va_start(args, format);

		ret = RNA_function_call_direct_va(C, reports, ptr, func, format, args);

		va_end(args);

		return ret;
	}

	return -1;
}

static int rna_function_format_array_length(const char *format, int ofs, int flen)
{
	char lenbuf[16];
	int idx = 0;

	if (format[ofs++] == '[')
		for (; ofs < flen && format[ofs] != ']' && idx < sizeof(*lenbuf) - 1; idx++, ofs++)
			lenbuf[idx] = format[ofs];

	if (ofs < flen && format[ofs + 1] == ']') {
		/* XXX put better error reporting for (ofs >= flen) or idx over lenbuf capacity */
		lenbuf[idx] = '\0';
		return atoi(lenbuf);
	}

	return 0;
}

static int rna_function_parameter_parse(PointerRNA *ptr, PropertyRNA *prop, PropertyType type,
                                        char ftype, int len, void *dest, void *src, StructRNA *srna,
                                        const char *tid, const char *fid, const char *pid)
{
	/* ptr is always a function pointer, prop always a parameter */

	switch (type) {
		case PROP_BOOLEAN:
		{
			if (ftype != 'b') {
				fprintf(stderr, "%s.%s: wrong type for parameter %s, a boolean was expected\n", tid, fid, pid);
				return -1;
			}

			if (len == 0)
				*((int *)dest) = *((int *)src);
			else
				memcpy(dest, src, len * sizeof(int));

			break;
		}
		case PROP_INT:
		{
			if (ftype != 'i') {
				fprintf(stderr, "%s.%s: wrong type for parameter %s, an integer was expected\n", tid, fid, pid);
				return -1;
			}

			if (len == 0)
				*((int *)dest) = *((int *)src);
			else
				memcpy(dest, src, len * sizeof(int));

			break;
		}
		case PROP_FLOAT:
		{
			if (ftype != 'f') {
				fprintf(stderr, "%s.%s: wrong type for parameter %s, a float was expected\n", tid, fid, pid);
				return -1;
			}

			if (len == 0)
				*((float *)dest) = *((float *)src);
			else
				memcpy(dest, src, len * sizeof(float));

			break;
		}
		case PROP_STRING:
		{
			if (ftype != 's') {
				fprintf(stderr, "%s.%s: wrong type for parameter %s, a string was expected\n", tid, fid, pid);
				return -1;
			}

			*((char **)dest) = *((char **)src);

			break;
		}
		case PROP_ENUM:
		{
			if (ftype != 'e') {
				fprintf(stderr, "%s.%s: wrong type for parameter %s, an enum was expected\n", tid, fid, pid);
				return -1;
			}

			*((int *)dest) = *((int *)src);

			break;
		}
		case PROP_POINTER:
		{
			StructRNA *ptype;

			if (ftype != 'O') {
				fprintf(stderr, "%s.%s: wrong type for parameter %s, an object was expected\n", tid, fid, pid);
				return -1;
			}

			ptype = RNA_property_pointer_type(ptr, prop);

			if (prop->flag & PROP_RNAPTR) {
				*((PointerRNA *)dest) = *((PointerRNA *)src);
				break;
			}
			
			if (ptype != srna && !RNA_struct_is_a(srna, ptype)) {
				fprintf(stderr, "%s.%s: wrong type for parameter %s, "
				        "an object of type %s was expected, passed an object of type %s\n",
				        tid, fid, pid, RNA_struct_identifier(ptype), RNA_struct_identifier(srna));
				return -1;
			}
 
			*((void **)dest) = *((void **)src);

			break;
		}
		case PROP_COLLECTION:
		{
			StructRNA *ptype;
			ListBase *lb, *clb;
			Link *link;
			CollectionPointerLink *clink;

			if (ftype != 'C') {
				fprintf(stderr, "%s.%s: wrong type for parameter %s, a collection was expected\n", tid, fid, pid);
				return -1;
			}

			lb = (ListBase *)src;
			clb = (ListBase *)dest;
			ptype = RNA_property_pointer_type(ptr, prop);
			
			if (ptype != srna && !RNA_struct_is_a(srna, ptype)) {
				fprintf(stderr, "%s.%s: wrong type for parameter %s, "
				        "a collection of objects of type %s was expected, "
				        "passed a collection of objects of type %s\n",
				        tid, fid, pid, RNA_struct_identifier(ptype), RNA_struct_identifier(srna));
				return -1;
			}

			for (link = lb->first; link; link = link->next) {
				clink = MEM_callocN(sizeof(CollectionPointerLink), "CCollectionPointerLink");
				RNA_pointer_create(NULL, srna, link, &clink->ptr);
				BLI_addtail(clb, clink);
			}

			break;
		}
		default:
		{
			if (len == 0)
				fprintf(stderr, "%s.%s: unknown type for parameter %s\n", tid, fid, pid);
			else
				fprintf(stderr, "%s.%s: unknown array type for parameter %s\n", tid, fid, pid);

			return -1;
		}
	}

	return 0;
}

int RNA_function_call_direct_va(bContext *C, ReportList *reports, PointerRNA *ptr, FunctionRNA *func,
                                const char *format, va_list args)
{
	PointerRNA funcptr;
	ParameterList parms;
	ParameterIterator iter;
	PropertyRNA *pret, *parm;
	PropertyType type;
	int i, ofs, flen, flag, len, alen, err = 0;
	const char *tid, *fid, *pid = NULL;
	char ftype;
	void **retdata = NULL;

	RNA_pointer_create(NULL, &RNA_Function, func, &funcptr);

	tid = RNA_struct_identifier(ptr->type);
	fid = RNA_function_identifier(func);
	pret = func->c_ret;
	flen = strlen(format);

	RNA_parameter_list_create(&parms, ptr, func);
	RNA_parameter_list_begin(&parms, &iter);

	for (i = 0, ofs = 0; iter.valid; RNA_parameter_list_next(&iter), i++) {
		parm = iter.parm;
		flag = RNA_property_flag(parm);

		if (parm == pret) {
			retdata = iter.data;
			continue;
		}
		else if (flag & PROP_OUTPUT) {
			continue;
		}

		pid = RNA_property_identifier(parm);

		if (ofs >= flen || format[ofs] == 'N') {
			if (flag & PROP_REQUIRED) {
				err = -1;
				fprintf(stderr, "%s.%s: missing required parameter %s\n", tid, fid, pid);
				break;
			}
			ofs++;
			continue;
		}

		type = RNA_property_type(parm);
		ftype = format[ofs++];
		len = RNA_property_array_length(&funcptr, parm);
		alen = rna_function_format_array_length(format, ofs, flen);

		if (len != alen) {
			err = -1;
			fprintf(stderr, "%s.%s: for parameter %s, "
			        "was expecting an array of %i elements, "
			        "passed %i elements instead\n",
			        tid, fid, pid, len, alen);
			break;
		}

		switch (type) {
			case PROP_BOOLEAN:
			case PROP_INT:
			case PROP_ENUM:
			{
				int arg = va_arg(args, int);
				err = rna_function_parameter_parse(&funcptr, parm, type, ftype, len, iter.data, &arg,
				                                   NULL, tid, fid, pid);
				break;
			}
			case PROP_FLOAT:
			{
				double arg = va_arg(args, double);
				err = rna_function_parameter_parse(&funcptr, parm, type, ftype, len, iter.data, &arg,
				                                   NULL, tid, fid, pid);
				break;
			}
			case PROP_STRING:
			{
				const char *arg = va_arg(args, char *);
				err = rna_function_parameter_parse(&funcptr, parm, type, ftype, len, iter.data, &arg,
				                                   NULL, tid, fid, pid);
				break;
			}
			case PROP_POINTER:
			{
				StructRNA *srna = va_arg(args, StructRNA *);
				void *arg = va_arg(args, void *);
				err = rna_function_parameter_parse(&funcptr, parm, type, ftype, len, iter.data, &arg,
				                                   srna, tid, fid, pid);
				break;
			}
			case PROP_COLLECTION:
			{
				StructRNA *srna = va_arg(args, StructRNA *);
				ListBase *arg = va_arg(args, ListBase *);
				err = rna_function_parameter_parse(&funcptr, parm, type, ftype, len, iter.data, &arg,
				                                   srna, tid, fid, pid);
				break;
			}
			default:
			{
				/* handle errors */
				err = rna_function_parameter_parse(&funcptr, parm, type, ftype, len, iter.data, NULL,
				                                   NULL, tid, fid, pid);
				break;
			}
		}

		if (err != 0)
			break;
	}

	if (err == 0)
		err = RNA_function_call(C, reports, ptr, func, &parms);

	/* XXX throw error when more parameters than those needed are passed or leave silent? */
	if (err == 0 && pret && ofs < flen && format[ofs++] == 'R') {
		parm = pret;

		type = RNA_property_type(parm);
		ftype = format[ofs++];
		len = RNA_property_array_length(&funcptr, parm);
		alen = rna_function_format_array_length(format, ofs, flen);

		if (len != alen) {
			err = -1;
			fprintf(stderr, "%s.%s: for return parameter %s, "
			        "was expecting an array of %i elements, passed %i elements instead\n",
			        tid, fid, pid, len, alen);
		}
		else {
			switch (type) {
				case PROP_BOOLEAN:
				case PROP_INT:
				case PROP_ENUM:
				{
					int *arg = va_arg(args, int *);
					err = rna_function_parameter_parse(&funcptr, parm, type, ftype, len, arg, retdata,
					                                   NULL, tid, fid, pid);
					break;
				}
				case PROP_FLOAT:
				{
					float *arg = va_arg(args, float *);
					err = rna_function_parameter_parse(&funcptr, parm, type, ftype, len, arg, retdata,
					                                   NULL, tid, fid, pid);
					break;
				}
				case PROP_STRING:
				{
					const char **arg = va_arg(args, const char **);
					err = rna_function_parameter_parse(&funcptr, parm, type, ftype, len, arg, retdata,
					                                   NULL, tid, fid, pid);
					break;
				}
				case PROP_POINTER:
				{
					StructRNA *srna = va_arg(args, StructRNA *);
					void **arg = va_arg(args, void **);
					err = rna_function_parameter_parse(&funcptr, parm, type, ftype, len, arg, retdata,
					                                   srna, tid, fid, pid);
					break;
				}
				case PROP_COLLECTION:
				{
					StructRNA *srna = va_arg(args, StructRNA *);
					ListBase **arg = va_arg(args, ListBase * *);
					err = rna_function_parameter_parse(&funcptr, parm, type, ftype, len, arg, retdata,
					                                   srna, tid, fid, pid);
					break;
				}
				default:
				{
					/* handle errors */
					err = rna_function_parameter_parse(&funcptr, parm, type, ftype, len, NULL, NULL,
					                                   NULL, tid, fid, pid);
					break;
				}
			}
		}
	}

	RNA_parameter_list_end(&iter);
	RNA_parameter_list_free(&parms);

	return err;
}

int RNA_function_call_direct_va_lookup(bContext *C, ReportList *reports, PointerRNA *ptr,
                                       const char *identifier, const char *format, va_list args)
{
	FunctionRNA *func;

	func = RNA_struct_find_function(ptr->type, identifier);

	if (func)
		return RNA_function_call_direct_va(C, reports, ptr, func, format, args);

	return 0;
}

bool RNA_property_reset(PointerRNA *ptr, PropertyRNA *prop, int index)
{
	int len;

	/* get the length of the array to work with */
	len = RNA_property_array_length(ptr, prop);
	
	/* get and set the default values as appropriate for the various types */
	switch (RNA_property_type(prop)) {
		case PROP_BOOLEAN:
			if (len) {
				if (index == -1) {
					int *tmparray = MEM_callocN(sizeof(int) * len, "reset_defaults - boolean");
					
					RNA_property_boolean_get_default_array(ptr, prop, tmparray);
					RNA_property_boolean_set_array(ptr, prop, tmparray);
					
					MEM_freeN(tmparray);
				}
				else {
					int value = RNA_property_boolean_get_default_index(ptr, prop, index);
					RNA_property_boolean_set_index(ptr, prop, index, value);
				}
			}
			else {
				int value = RNA_property_boolean_get_default(ptr, prop);
				RNA_property_boolean_set(ptr, prop, value);
			}
			return true;
		case PROP_INT:
			if (len) {
				if (index == -1) {
					int *tmparray = MEM_callocN(sizeof(int) * len, "reset_defaults - int");
					
					RNA_property_int_get_default_array(ptr, prop, tmparray);
					RNA_property_int_set_array(ptr, prop, tmparray);
					
					MEM_freeN(tmparray);
				}
				else {
					int value = RNA_property_int_get_default_index(ptr, prop, index);
					RNA_property_int_set_index(ptr, prop, index, value);
				}
			}
			else {
				int value = RNA_property_int_get_default(ptr, prop);
				RNA_property_int_set(ptr, prop, value);
			}
			return true;
		case PROP_FLOAT:
			if (len) {
				if (index == -1) {
					float *tmparray = MEM_callocN(sizeof(float) * len, "reset_defaults - float");
					
					RNA_property_float_get_default_array(ptr, prop, tmparray);
					RNA_property_float_set_array(ptr, prop, tmparray);
					
					MEM_freeN(tmparray);
				}
				else {
					float value = RNA_property_float_get_default_index(ptr, prop, index);
					RNA_property_float_set_index(ptr, prop, index, value);
				}
			}
			else {
				float value = RNA_property_float_get_default(ptr, prop);
				RNA_property_float_set(ptr, prop, value);
			}
			return true;
		case PROP_ENUM:
		{
			int value = RNA_property_enum_get_default(ptr, prop);
			RNA_property_enum_set(ptr, prop, value);
			return true;
		}
		
		case PROP_STRING:
		{
			char *value = RNA_property_string_get_default_alloc(ptr, prop, NULL, 0);
			RNA_property_string_set(ptr, prop, value);
			MEM_freeN(value);
			return true;
		}
		
		case PROP_POINTER:
		{
			PointerRNA value = RNA_property_pointer_get_default(ptr, prop);
			RNA_property_pointer_set(ptr, prop, value);
			return true;
		}
		
		default:
			/* FIXME: are there still any cases that haven't been handled? comment out "default" block to check :) */
			return false;
	}
}

bool RNA_property_copy(PointerRNA *ptr, PointerRNA *fromptr, PropertyRNA *prop, int index)
{
	int len, fromlen;
	PropertyRNA *fromprop = prop;

	if (prop->magic != RNA_MAGIC) {
		/* In case of IDProperty, we have to find the *real* idprop of ptr,
		 * since prop in this case is just a fake wrapper around actual IDProp data, and not a 'real' PropertyRNA. */
		prop = (PropertyRNA *)rna_idproperty_find(ptr, ((IDProperty *)fromprop)->name);
		/* Even though currently we now prop will always be the 'fromprop', this might not be the case in the future. */
		if (prop == fromprop) {
			fromprop = (PropertyRNA *)rna_idproperty_find(fromptr, ((IDProperty *)prop)->name);
		}
	}

	/* get the length of the array to work with */
	len = RNA_property_array_length(ptr, prop);
	fromlen = RNA_property_array_length(fromptr, fromprop);

	if (len != fromlen)
		return false;

	/* get and set the default values as appropriate for the various types */
	switch (RNA_property_type(prop)) {
		case PROP_BOOLEAN:
			if (len) {
				if (index == -1) {
					int *tmparray = MEM_callocN(sizeof(int) * len, "copy - boolean");
					
					RNA_property_boolean_get_array(fromptr, fromprop, tmparray);
					RNA_property_boolean_set_array(ptr, prop, tmparray);
					
					MEM_freeN(tmparray);
				}
				else {
					int value = RNA_property_boolean_get_index(fromptr, fromprop, index);
					RNA_property_boolean_set_index(ptr, prop, index, value);
				}
			}
			else {
				int value = RNA_property_boolean_get(fromptr, fromprop);
				RNA_property_boolean_set(ptr, prop, value);
			}
			return true;
		case PROP_INT:
			if (len) {
				if (index == -1) {
					int *tmparray = MEM_callocN(sizeof(int) * len, "copy - int");
					
					RNA_property_int_get_array(fromptr, fromprop, tmparray);
					RNA_property_int_set_array(ptr, prop, tmparray);
					
					MEM_freeN(tmparray);
				}
				else {
					int value = RNA_property_int_get_index(fromptr, fromprop, index);
					RNA_property_int_set_index(ptr, prop, index, value);
				}
			}
			else {
				int value = RNA_property_int_get(fromptr, fromprop);
				RNA_property_int_set(ptr, prop, value);
			}
			return true;
		case PROP_FLOAT:
			if (len) {
				if (index == -1) {
					float *tmparray = MEM_callocN(sizeof(float) * len, "copy - float");
					
					RNA_property_float_get_array(fromptr, fromprop, tmparray);
					RNA_property_float_set_array(ptr, prop, tmparray);
					
					MEM_freeN(tmparray);
				}
				else {
					float value = RNA_property_float_get_index(fromptr, fromprop, index);
					RNA_property_float_set_index(ptr, prop, index, value);
				}
			}
			else {
				float value = RNA_property_float_get(fromptr, fromprop);
				RNA_property_float_set(ptr, prop, value);
			}
			return true;
		case PROP_ENUM:
		{
			int value = RNA_property_enum_get(fromptr, fromprop);
			RNA_property_enum_set(ptr, prop, value);
			return true;
		}
		case PROP_POINTER:
		{
			PointerRNA value = RNA_property_pointer_get(fromptr, fromprop);
			RNA_property_pointer_set(ptr, prop, value);
			return true;
		}
		case PROP_STRING:
		{
			char *value = RNA_property_string_get_alloc(fromptr, fromprop, NULL, 0, NULL);
			RNA_property_string_set(ptr, prop, value);
			MEM_freeN(value);
			return true;
		}
		default:
			return false;
	}

	return false;
}

/* use RNA_warning macro which includes __func__ suffix */
void _RNA_warning(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	vprintf(format, args);
	va_end(args);

	/* gcc macro adds '\n', but cant use for other compilers */
#ifndef __GNUC__
	fputc('\n', stdout);
#endif

#ifdef WITH_PYTHON
	{
		extern void PyC_LineSpit(void);
		PyC_LineSpit();
	}
#endif
}

bool RNA_property_equals(PointerRNA *a, PointerRNA *b, PropertyRNA *prop, eRNAEqualsMode mode)
{
	int len, fromlen;

	if (mode == RNA_EQ_UNSET_MATCH_ANY) {
		/* uninitialized properties are assumed to match anything */
		if (!RNA_property_is_set(a, prop) || !RNA_property_is_set(b, prop))
			return true;
	}
	else if (mode == RNA_EQ_UNSET_MATCH_NONE) {
		/* unset properties never match set properties */
		if (RNA_property_is_set(a, prop) != RNA_property_is_set(b, prop))
			return false;
	}

	/* get the length of the array to work with */
	len = RNA_property_array_length(a, prop);
	fromlen = RNA_property_array_length(b, prop);

	if (len != fromlen)
		return false;

	/* get and set the default values as appropriate for the various types */
	switch (RNA_property_type(prop)) {
		case PROP_BOOLEAN:
		{
			if (len) {
				int fixed_a[16], fixed_b[16];
				int *array_a, *array_b;
				bool equals;

				array_a = (len > 16) ? MEM_callocN(sizeof(int) * len, "RNA equals") : fixed_a;
				array_b = (len > 16) ? MEM_callocN(sizeof(int) * len, "RNA equals") : fixed_b;

				RNA_property_boolean_get_array(a, prop, array_a);
				RNA_property_boolean_get_array(b, prop, array_b);

				equals = memcmp(array_a, array_b, sizeof(int) * len) == 0;

				if (array_a != fixed_a) MEM_freeN(array_a);
				if (array_b != fixed_b) MEM_freeN(array_b);

				return equals;
			}
			else {
				int value = RNA_property_boolean_get(a, prop);
				return value == RNA_property_boolean_get(b, prop);
			}
		}

		case PROP_INT:
		{
			if (len) {
				int fixed_a[16], fixed_b[16];
				int *array_a, *array_b;
				bool equals;

				array_a = (len > 16) ? MEM_callocN(sizeof(int) * len, "RNA equals") : fixed_a;
				array_b = (len > 16) ? MEM_callocN(sizeof(int) * len, "RNA equals") : fixed_b;

				RNA_property_int_get_array(a, prop, array_a);
				RNA_property_int_get_array(b, prop, array_b);

				equals = memcmp(array_a, array_b, sizeof(int) * len) == 0;

				if (array_a != fixed_a) MEM_freeN(array_a);
				if (array_b != fixed_b) MEM_freeN(array_b);

				return equals;
			}
			else {
				int value = RNA_property_int_get(a, prop);
				return value == RNA_property_int_get(b, prop);
			}
		}

		case PROP_FLOAT:
		{
			if (len) {
				float fixed_a[16], fixed_b[16];
				float *array_a, *array_b;
				bool equals;

				array_a = (len > 16) ? MEM_callocN(sizeof(float) * len, "RNA equals") : fixed_a;
				array_b = (len > 16) ? MEM_callocN(sizeof(float) * len, "RNA equals") : fixed_b;

				RNA_property_float_get_array(a, prop, array_a);
				RNA_property_float_get_array(b, prop, array_b);

				equals = memcmp(array_a, array_b, sizeof(float) * len) == 0;

				if (array_a != fixed_a) MEM_freeN(array_a);
				if (array_b != fixed_b) MEM_freeN(array_b);

				return equals;
			}
			else {
				float value = RNA_property_float_get(a, prop);
				return value == RNA_property_float_get(b, prop);
			}
		}

		case PROP_ENUM:
		{
			int value = RNA_property_enum_get(a, prop);
			return value == RNA_property_enum_get(b, prop);
		}

		case PROP_STRING:
		{
			char fixed_a[128], fixed_b[128];
			int len_a, len_b;
			char *value_a = RNA_property_string_get_alloc(a, prop, fixed_a, sizeof(fixed_a), &len_a);
			char *value_b = RNA_property_string_get_alloc(b, prop, fixed_b, sizeof(fixed_b), &len_b);
			bool equals = strcmp(value_a, value_b) == 0;

			if (value_a != fixed_a) MEM_freeN(value_a);
			if (value_b != fixed_b) MEM_freeN(value_b);

			return equals;
		}

		case PROP_POINTER:
		{
			if (!STREQ(RNA_property_identifier(prop), "rna_type")) {
				PointerRNA propptr_a = RNA_property_pointer_get(a, prop);
				PointerRNA propptr_b = RNA_property_pointer_get(b, prop);
				return RNA_struct_equals(&propptr_a, &propptr_b, mode);
			}
			break;
		}

		default:
			break;
	}

	return true;
}

bool RNA_struct_equals(PointerRNA *a, PointerRNA *b, eRNAEqualsMode mode)
{
	CollectionPropertyIterator iter;
//	CollectionPropertyRNA *citerprop;  /* UNUSED */
	PropertyRNA *iterprop;
	bool equals = true;

	if (a == NULL && b == NULL)
		return true;
	else if (a == NULL || b == NULL)
		return false;
	else if (a->type != b->type)
		return false;

	iterprop = RNA_struct_iterator_property(a->type);
//	citerprop = (CollectionPropertyRNA *)rna_ensure_property(iterprop);  /* UNUSED */

	RNA_property_collection_begin(a, iterprop, &iter);
	for (; iter.valid; RNA_property_collection_next(&iter)) {
		PropertyRNA *prop = iter.ptr.data;

		if (!RNA_property_equals(a, b, prop, mode)) {
			equals = false;
			break;
		}
	}
	RNA_property_collection_end(&iter);

	return equals;
}

