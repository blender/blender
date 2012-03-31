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

/** \file blender/makesrna/intern/rna_internal_types.h
 *  \ingroup RNA
 */


#ifndef __RNA_INTERNAL_TYPES_H__
#define __RNA_INTERNAL_TYPES_H__

#include "DNA_listBase.h"

struct BlenderRNA;
struct ContainerRNA;
struct StructRNA;
struct PropertyRNA;
struct PointerRNA;
struct FunctionRNA;
struct ReportList;
struct CollectionPropertyIterator;
struct bContext;
struct EnumProperty;
struct IDProperty;
struct GHash;
struct Main;
struct Scene;

#ifdef UNIT_TEST
#define RNA_MAX_ARRAY_LENGTH 64
#else
#define RNA_MAX_ARRAY_LENGTH 32
#endif

#define RNA_MAX_ARRAY_DIMENSION 3


/* store local properties here */
#define RNA_IDP_UI "_RNA_UI"

/* Function Callbacks */

typedef void (*UpdateFunc)(struct Main *main, struct Scene *scene, struct PointerRNA *ptr);
typedef void (*ContextPropUpdateFunc)(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop);
typedef void (*ContextUpdateFunc)(struct bContext *C, struct PointerRNA *ptr);
typedef int (*EditableFunc)(struct PointerRNA *ptr);
typedef int (*ItemEditableFunc)(struct PointerRNA *ptr, int index);
typedef struct IDProperty* (*IDPropertiesFunc)(struct PointerRNA *ptr, int create);
typedef struct StructRNA *(*StructRefineFunc)(struct PointerRNA *ptr);
typedef char *(*StructPathFunc)(struct PointerRNA *ptr);

typedef int (*PropArrayLengthGetFunc)(struct PointerRNA *ptr, int length[RNA_MAX_ARRAY_DIMENSION]);
typedef int (*PropBooleanGetFunc)(struct PointerRNA *ptr);
typedef void (*PropBooleanSetFunc)(struct PointerRNA *ptr, int value);
typedef void (*PropBooleanArrayGetFunc)(struct PointerRNA *ptr, int *values);
typedef void (*PropBooleanArraySetFunc)(struct PointerRNA *ptr, const int *values);
typedef int (*PropIntGetFunc)(struct PointerRNA *ptr);
typedef void (*PropIntSetFunc)(struct PointerRNA *ptr, int value);
typedef void (*PropIntArrayGetFunc)(struct PointerRNA *ptr, int *values);
typedef void (*PropIntArraySetFunc)(struct PointerRNA *ptr, const int *values);
typedef void (*PropIntRangeFunc)(struct PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax);
typedef float (*PropFloatGetFunc)(struct PointerRNA *ptr);
typedef void (*PropFloatSetFunc)(struct PointerRNA *ptr, float value);
typedef void (*PropFloatArrayGetFunc)(struct PointerRNA *ptr, float *values);
typedef void (*PropFloatArraySetFunc)(struct PointerRNA *ptr, const float *values);
typedef void (*PropFloatRangeFunc)(struct PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax);
typedef void (*PropStringGetFunc)(struct PointerRNA *ptr, char *value);
typedef int (*PropStringLengthFunc)(struct PointerRNA *ptr);
typedef void (*PropStringSetFunc)(struct PointerRNA *ptr, const char *value);
typedef int (*PropEnumGetFunc)(struct PointerRNA *ptr);
typedef void (*PropEnumSetFunc)(struct PointerRNA *ptr, int value);
typedef EnumPropertyItem *(*PropEnumItemFunc)(struct bContext *C, struct PointerRNA *ptr,
                                              struct PropertyRNA *prop, int *free);
typedef PointerRNA (*PropPointerGetFunc)(struct PointerRNA *ptr);
typedef StructRNA* (*PropPointerTypeFunc)(struct PointerRNA *ptr);
typedef void (*PropPointerSetFunc)(struct PointerRNA *ptr, const PointerRNA value);
typedef int (*PropPointerPollFunc)(struct PointerRNA *ptr, const PointerRNA value);
typedef void (*PropCollectionBeginFunc)(struct CollectionPropertyIterator *iter, struct PointerRNA *ptr);
typedef void (*PropCollectionNextFunc)(struct CollectionPropertyIterator *iter);
typedef void (*PropCollectionEndFunc)(struct CollectionPropertyIterator *iter);
typedef PointerRNA (*PropCollectionGetFunc)(struct CollectionPropertyIterator *iter);
typedef int (*PropCollectionLengthFunc)(struct PointerRNA *ptr);
typedef int (*PropCollectionLookupIntFunc)(struct PointerRNA *ptr, int key, struct PointerRNA *r_ptr);
typedef int (*PropCollectionLookupStringFunc)(struct PointerRNA *ptr, const char *key, struct PointerRNA *r_ptr);
typedef int (*PropCollectionAssignIntFunc)(struct PointerRNA *ptr, int key, const struct PointerRNA *assign_ptr);

/* Container - generic abstracted container of RNA properties */
typedef struct ContainerRNA {
	void *next, *prev;

	struct GHash *prophash;
	ListBase properties;
} ContainerRNA;

struct FunctionRNA {
	/* structs are containers of properties */
	ContainerRNA cont;

	/* unique identifier, keep after 'cont' */
	const char *identifier;
	/* various options */
	int flag;

	/* single line description, displayed in the tooltip for example */
	const char *description;

	/* callback to execute the function */
	CallFunc call;

	/* parameter for the return value
	 * note: this is only the C return value, rna functions can have multiple return values */
	PropertyRNA *c_ret;
};

struct PropertyRNA {
	struct PropertyRNA *next, *prev;

	/* magic bytes to distinguish with IDProperty */
	int magic;

	/* unique identifier */
	const char *identifier;
	/* various options */
	int flag;

	/* user readable name */
	const char *name;
	/* single line description, displayed in the tooltip for example */
	const char *description;
	/* icon ID */
	int icon;
	/* context for translation */
	const char *translation_context;

	/* property type as it appears to the outside */
	PropertyType type;
	/* subtype, 'interpretation' of the property */
	PropertySubType subtype;
	/* if non-NULL, overrides arraylength. Must not return 0? */
	PropArrayLengthGetFunc getlength;
	/* dimension of array */
	unsigned int arraydimension;
	/* array lengths lengths for all dimensions (when arraydimension > 0) */
	unsigned int arraylength[RNA_MAX_ARRAY_DIMENSION];
	unsigned int totarraylength;
	
	/* callback for updates on change */
	UpdateFunc update;
	int noteflag;

	/* callback for testing if editable */
	EditableFunc editable;
	/* callback for testing if array-item editable (if applicable) */
	ItemEditableFunc itemeditable;

	/* raw access */
	int rawoffset;
	RawPropertyType rawtype;

	/* This is used for accessing props/functions of this property
	 * any property can have this but should only be used for collections and arrays
	 * since python will convert int/bool/pointer's */
	struct StructRNA *srna;	/* attributes attached directly to this collection */

	/* python handle to hold all callbacks
	 * (in a pointer array at the moment, may later be a tuple) */
	void *py_data;
};

/* Property Types */

typedef struct BoolPropertyRNA {
	PropertyRNA property;

	PropBooleanGetFunc get;
	PropBooleanSetFunc set;

	PropBooleanArrayGetFunc getarray;
	PropBooleanArraySetFunc setarray;

	int defaultvalue;
	const int *defaultarray;
} BoolPropertyRNA;

typedef struct IntPropertyRNA {
	PropertyRNA property;

	PropIntGetFunc get;
	PropIntSetFunc set;

	PropIntArrayGetFunc getarray;
	PropIntArraySetFunc setarray;

	PropIntRangeFunc range;

	int softmin, softmax;
	int hardmin, hardmax;
	int step;

	int defaultvalue;
	const int *defaultarray;
} IntPropertyRNA;

typedef struct FloatPropertyRNA {
	PropertyRNA property;

	PropFloatGetFunc get;
	PropFloatSetFunc set;

	PropFloatArrayGetFunc getarray;
	PropFloatArraySetFunc setarray;

	PropFloatRangeFunc range;

	float softmin, softmax;
	float hardmin, hardmax;
	float step;
	int precision;

	float defaultvalue;
	const float *defaultarray;
} FloatPropertyRNA;

typedef struct StringPropertyRNA {
	PropertyRNA property;

	PropStringGetFunc get;
	PropStringLengthFunc length;
	PropStringSetFunc set;

	int maxlength;	/* includes string terminator! */

	const char *defaultvalue;
} StringPropertyRNA;

typedef struct EnumPropertyRNA {
	PropertyRNA property;

	PropEnumGetFunc get;
	PropEnumSetFunc set;
	PropEnumItemFunc itemf;
	void *py_data; /* store py callback here */

	EnumPropertyItem *item;
	int totitem;

	int defaultvalue;
} EnumPropertyRNA;

typedef struct PointerPropertyRNA {
	PropertyRNA property;

	PropPointerGetFunc get;
	PropPointerSetFunc set;
	PropPointerTypeFunc typef;
	PropPointerPollFunc poll; /* unlike operators, 'set' can still run if poll fails, used for filtering display */

	struct StructRNA *type;
} PointerPropertyRNA;

typedef struct CollectionPropertyRNA {
	PropertyRNA property;

	PropCollectionBeginFunc begin;
	PropCollectionNextFunc next;
	PropCollectionEndFunc end;						/* optional */
	PropCollectionGetFunc get;
	PropCollectionLengthFunc length;				/* optional */
	PropCollectionLookupIntFunc lookupint;			/* optional */
	PropCollectionLookupStringFunc lookupstring;	/* optional */
	PropCollectionAssignIntFunc assignint;			/* optional */

	struct StructRNA *item_type;			/* the type of this item */
} CollectionPropertyRNA;


/* changes to this struct require updating rna_generate_struct in makesrna.c */
struct StructRNA {
	/* structs are containers of properties */
	ContainerRNA cont;

	/* unique identifier, keep after 'cont' */
	const char *identifier;

	/* python type, this is a subtype of pyrna_struct_Type but used so each struct can have its own type
	 * which is useful for subclassing RNA */
	void *py_type;
	void *blender_type;
	
	/* various options */
	int flag;

	/* user readable name */
	const char *name;
	/* single line description, displayed in the tooltip for example */
	const char *description;
	/* context for translation */
	const char *translation_context;
	/* icon ID */
	int icon;
	
	/* property that defines the name */
	PropertyRNA *nameproperty;

	/* property to iterate over properties */
	PropertyRNA *iteratorproperty;

	/* struct this is derivedfrom */
	struct StructRNA *base;

	/* only use for nested structs, where both the parent and child access
	 * the same C Struct but nesting is used for grouping properties.
	 * The parent property is used so we know NULL checks are not needed,
	 * and that this struct will never exist without its parent */
	struct StructRNA *nested;

	/* function to give the more specific type */
	StructRefineFunc refine;

	/* function to find path to this struct in an ID */
	StructPathFunc path;

	/* function to register/unregister subclasses */
	StructRegisterFunc reg;
	StructUnregisterFunc unreg;
	StructInstanceFunc instance;

	/* callback to get id properties */
	IDPropertiesFunc idproperties;

	/* functions of this struct */
	ListBase functions;
};

/* Blender RNA
 *
 * Root RNA data structure that lists all struct types. */

struct BlenderRNA {
	ListBase structs;
};

#define CONTAINER_RNA_ID(cont) (*(const char **)(((ContainerRNA *)(cont))+1))

#endif /* __RNA_INTERNAL_TYPES_H__ */
