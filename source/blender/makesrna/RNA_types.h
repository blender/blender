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

#ifndef RNA_TYPES
#define RNA_TYPES

#include "DNA_listBase.h"

struct BlenderRNA;
struct StructRNA;
struct PropertyRNA;
struct CollectionPropertyIterator;
struct bContext;

typedef void (*PropNotifyFunc)(struct bContext *C, void *data);
typedef int (*PropBooleanGetFunc)(void *data);
typedef void (*PropBooleanSetFunc)(void *data, int value);
typedef int (*PropBooleanArrayGetFunc)(void *data, int index);
typedef void (*PropBooleanArraySetFunc)(void *data, int index, int value);
typedef int (*PropIntGetFunc)(void *data);
typedef void (*PropIntSetFunc)(void *data, int value);
typedef int (*PropIntArrayGetFunc)(void *data, int index);
typedef void (*PropIntArraySetFunc)(void *data, int index, int value);
typedef float (*PropFloatGetFunc)(void *data);
typedef void (*PropFloatSetFunc)(void *data, float value);
typedef float (*PropFloatArrayGetFunc)(void *data, int index);
typedef void (*PropFloatArraySetFunc)(void *data, int index, float value);
typedef void (*PropStringGetFunc)(void *data, char *value);
typedef int (*PropStringLengthFunc)(void *data);
typedef void (*PropStringSetFunc)(void *data, const char *value);
typedef int (*PropEnumGetFunc)(void *data);
typedef void (*PropEnumSetFunc)(void *data, int value);
typedef void* (*PropPointerGetFunc)(void *data);
typedef void (*PropPointerSetFunc)(void *data, void *value);
typedef struct StructRNA* (*PropPointerTypeFunc)(void *data);
typedef void (*PropCollectionBeginFunc)(struct CollectionPropertyIterator *iter, void *data);
typedef void (*PropCollectionNextFunc)(struct CollectionPropertyIterator *iter);
typedef void (*PropCollectionEndFunc)(struct CollectionPropertyIterator *iter);
typedef void* (*PropCollectionGetFunc)(struct CollectionPropertyIterator *iter);
typedef struct StructRNA* (*PropCollectionTypeFunc)(struct CollectionPropertyIterator *iter);
typedef int (*PropCollectionLengthFunc)(void *data);
typedef void* (*PropCollectionLookupIntFunc)(void *data, int key, struct StructRNA **type);
typedef void* (*PropCollectionLookupStringFunc)(void *data, const char *key, struct StructRNA **type);

typedef enum PropertyType {
	PROP_BOOLEAN = 0,
	PROP_INT = 1,
	PROP_FLOAT = 2,
	PROP_STRING = 3,
	PROP_ENUM = 4,
	PROP_POINTER = 5,
	PROP_COLLECTION = 6
} PropertyType;

typedef enum PropertySubType {
	PROP_NONE = 0,
	PROP_UNSIGNED = 1,
	PROP_FILEPATH = 2,
	PROP_COLOR = 3,
	PROP_VECTOR = 4,
	PROP_MATRIX = 5,
	PROP_ROTATION = 6
} PropertySubType;

typedef enum PropertyFlag {
	PROP_EDITABLE = 1,
	PROP_EVALUATEABLE = 2
} PropertyFlag;

typedef struct CollectionPropertyIterator {
	void *internal;
	int valid;
} CollectionPropertyIterator;

typedef struct PropertyEnumItem {
	int value;
	const char *cname;
	const char *name;
} PropertyEnumItem;

typedef struct PropertyRNA {
	struct PropertyRNA *next, *prev;

	/* C code name */
	const char *cname;
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
	PropNotifyFunc notify;
} PropertyRNA;

/* information specific to the property type */
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
	float step, precision;

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

	const PropertyEnumItem *item;
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

typedef struct StructRNA {
	struct StructRNA *next, *prev;

	/* C code name */
	const char *cname;
	/* various options */
	int flag;

	/* user readable name */
	const char *name;

	/* property that defines the name */
	PropertyRNA *nameproperty;

	/* properties of this struct */
	ListBase properties; 
} StructRNA;

typedef struct BlenderRNA {
	/* structs */
	ListBase structs;
} BlenderRNA;

#endif /* RNA_TYPES */

