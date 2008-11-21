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

#ifndef RNA_INTERNAL_TYPES
#define RNA_INTERNAL_TYPES

#include "DNA_listBase.h"

struct BlenderRNA;
struct StructRNA;
struct PropertyRNA;
struct PointerRNA;
struct CollectionPropertyIterator;
struct bContext;

/* Function Callbacks */

typedef void (*NotifyFunc)(struct bContext *C, struct PointerRNA *ptr);
typedef struct StructRNA *(*StructRefineFunc)(struct PointerRNA *ptr);

typedef int (*PropBooleanGetFunc)(struct PointerRNA *ptr);
typedef void (*PropBooleanSetFunc)(struct PointerRNA *ptr, int value);
typedef int (*PropBooleanArrayGetFunc)(struct PointerRNA *ptr, int index);
typedef void (*PropBooleanArraySetFunc)(struct PointerRNA *ptr, int index, int value);
typedef int (*PropIntGetFunc)(struct PointerRNA *ptr);
typedef void (*PropIntSetFunc)(struct PointerRNA *ptr, int value);
typedef int (*PropIntArrayGetFunc)(struct PointerRNA *ptr, int index);
typedef void (*PropIntArraySetFunc)(struct PointerRNA *ptr, int index, int value);
typedef float (*PropFloatGetFunc)(struct PointerRNA *ptr);
typedef void (*PropFloatSetFunc)(struct PointerRNA *ptr, float value);
typedef float (*PropFloatArrayGetFunc)(struct PointerRNA *ptr, int index);
typedef void (*PropFloatArraySetFunc)(struct PointerRNA *ptr, int index, float value);
typedef void (*PropStringGetFunc)(struct PointerRNA *ptr, char *value);
typedef int (*PropStringLengthFunc)(struct PointerRNA *ptr);
typedef void (*PropStringSetFunc)(struct PointerRNA *ptr, const char *value);
typedef int (*PropEnumGetFunc)(struct PointerRNA *ptr);
typedef void (*PropEnumSetFunc)(struct PointerRNA *ptr, int value);
typedef void* (*PropPointerGetFunc)(struct PointerRNA *ptr);
typedef void (*PropPointerSetFunc)(struct PointerRNA *ptr, void *value);
typedef struct StructRNA* (*PropPointerTypeFunc)(struct PointerRNA *ptr);
typedef void (*PropCollectionBeginFunc)(struct CollectionPropertyIterator *iter, struct PointerRNA *ptr);
typedef void (*PropCollectionNextFunc)(struct CollectionPropertyIterator *iter);
typedef void (*PropCollectionEndFunc)(struct CollectionPropertyIterator *iter);
typedef void* (*PropCollectionGetFunc)(struct CollectionPropertyIterator *iter);
typedef struct StructRNA* (*PropCollectionTypeFunc)(struct CollectionPropertyIterator *iter);
typedef int (*PropCollectionLengthFunc)(struct PointerRNA *ptr);
typedef void* (*PropCollectionLookupIntFunc)(struct PointerRNA *ptr, int key, struct StructRNA **type);
typedef void* (*PropCollectionLookupStringFunc)(struct PointerRNA *ptr, const char *key, struct StructRNA **type);

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

	/* property type as it appears to the outside */
	PropertyType type;
	/* subtype, 'interpretation' of the property */
	PropertySubType subtype;
	/* if an array this is > 0, specifying the length */
	unsigned int arraylength;
	
	/* callback for notifys on change */
	NotifyFunc notify;
};

/* Property Types */

typedef struct BooleanPropertyRNA {
	PropertyRNA property;

	PropBooleanGetFunc get;
	PropBooleanSetFunc set;

	PropBooleanArrayGetFunc getarray;
	PropBooleanArraySetFunc setarray;

	int defaultvalue;
	const int *defaultarray;
} BooleanPropertyRNA;

typedef struct IntPropertyRNA {
	PropertyRNA property;

	PropIntGetFunc get;
	PropIntSetFunc set;

	PropIntArrayGetFunc getarray;
	PropIntArraySetFunc setarray;

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

	const EnumPropertyItem *item;
	int totitem;

	int defaultvalue;
} EnumPropertyRNA;

typedef struct PointerPropertyRNA {
	PropertyRNA property;

	PropPointerGetFunc get;
	PropPointerSetFunc set;
	PropPointerTypeFunc type;	/* optional */

	struct StructRNA *structtype;
} PointerPropertyRNA;

typedef struct CollectionPropertyRNA {
	PropertyRNA property;

	PropCollectionBeginFunc begin;
	PropCollectionNextFunc next;
	PropCollectionEndFunc end;						/* optional */
	PropCollectionGetFunc get;
	PropCollectionTypeFunc type;					/* optional */
	PropCollectionLengthFunc length;				/* optional */
	PropCollectionLookupIntFunc lookupint;			/* optional */
	PropCollectionLookupStringFunc lookupstring;	/* optional */

	struct StructRNA *structtype;
} CollectionPropertyRNA;

struct StructRNA {
	struct StructRNA *next, *prev;

	/* unique identifier */
	const char *identifier;
	/* various options */
	int flag;

	/* user readable name */
	const char *name;

	/* property that defines the name */
	PropertyRNA *nameproperty;

	/* property to iterate over properties */
	PropertyRNA *iteratorproperty;

	/* struct this is derivedfrom */
	struct StructRNA *from;

	/* callback for notifys on change */
	NotifyFunc notify;

	/* function to give the more specific type */
	StructRefineFunc refine; 

	/* properties of this struct */
	ListBase properties; 
};

/* Blender RNA
 *
 * Root RNA data structure that lists all struct types. */

struct BlenderRNA {
	ListBase structs;
};

#endif /* RNA_INTERNAL_TYPES */

