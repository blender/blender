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

/** \file blender/makesrna/RNA_types.h
 *  \ingroup RNA
 */


#include "../blenlib/BLI_sys_types.h"

#ifndef __RNA_TYPES_H__
#define __RNA_TYPES_H__

#ifdef __cplusplus
extern "C" {
#endif

struct ParameterList;
struct FunctionRNA;
struct PropertyRNA;
struct StructRNA;
struct BlenderRNA;
struct bContext;
struct Main;
struct ReportList;

/** Pointer
 *
 * RNA pointers are not a single C pointer but include the type,
 * and a pointer to the ID struct that owns the struct, since
 * in some cases this information is needed to correctly get/set
 * the properties and validate them. */

typedef struct PointerRNA {
	struct {
		void *data;
	} id;

	struct StructRNA *type;
	void *data;
} PointerRNA;

typedef struct PropertyPointerRNA {
	PointerRNA ptr;
	struct PropertyRNA *prop;
} PropertyPointerRNA;

/**
 * Stored result of a RNA path lookup (as used by anim-system)
 */
typedef struct PathResolvedRNA {
	struct PointerRNA ptr;
	struct PropertyRNA *prop;
	/* -1 for non-array access */
	int prop_index;
} PathResolvedRNA;

/* Property */

typedef enum PropertyType {
	PROP_BOOLEAN      = 0,
	PROP_INT          = 1,
	PROP_FLOAT        = 2,
	PROP_STRING       = 3,
	PROP_ENUM         = 4,
	PROP_POINTER      = 5,
	PROP_COLLECTION   = 6,
} PropertyType;

/* also update rna_property_subtype_unit when you change this */
typedef enum PropertyUnit {
	PROP_UNIT_NONE         = (0 << 16),
	PROP_UNIT_LENGTH       = (1 << 16),   /* m */
	PROP_UNIT_AREA         = (2 << 16),   /* m^2 */
	PROP_UNIT_VOLUME       = (3 << 16),   /* m^3 */
	PROP_UNIT_MASS         = (4 << 16),   /* kg */
	PROP_UNIT_ROTATION     = (5 << 16),   /* radians */
	PROP_UNIT_TIME         = (6 << 16),   /* frame */
	PROP_UNIT_VELOCITY     = (7 << 16),   /* m/s */
	PROP_UNIT_ACCELERATION = (8 << 16),   /* m/(s^2) */
	PROP_UNIT_CAMERA       = (9 << 16),   /* mm */
} PropertyUnit;

#define RNA_SUBTYPE_UNIT(subtype)       ((subtype) &  0x00FF0000)
#define RNA_SUBTYPE_VALUE(subtype)      ((subtype) & ~0x00FF0000)
#define RNA_SUBTYPE_UNIT_VALUE(subtype) ((subtype) >> 16)

#define RNA_ENUM_BITFLAG_SIZE 32

#define RNA_TRANSLATION_PREC_DEFAULT 5

/* also update enums in bpy_props.c when adding items here
 * watch it: these values are written to files as part of
 * node socket button subtypes!
 */
typedef enum PropertySubType {
	PROP_NONE              = 0,

	/* strings */
	PROP_FILEPATH          = 1,
	PROP_DIRPATH           = 2,
	PROP_FILENAME          = 3,
	PROP_BYTESTRING        = 4, /* a string which should be represented as bytes in python, NULL terminated though. */
	/* 5 was used by "PROP_TRANSLATE" sub-type, which is now a flag. */
	PROP_PASSWORD          = 6, /* a string which should not be displayed in UI */

	/* numbers */
	PROP_PIXEL             = 12,
	PROP_UNSIGNED          = 13,
	PROP_PERCENTAGE        = 14,
	PROP_FACTOR            = 15,
	PROP_ANGLE             = 16 | PROP_UNIT_ROTATION,
	PROP_TIME              = 17 | PROP_UNIT_TIME,
	/* distance in 3d space, don't use for pixel distance for eg. */
	PROP_DISTANCE          = 18 | PROP_UNIT_LENGTH,
	PROP_DISTANCE_CAMERA   = 19 | PROP_UNIT_CAMERA,

	/* number arrays */
	PROP_COLOR             = 20,
	PROP_TRANSLATION       = 21 | PROP_UNIT_LENGTH,
	PROP_DIRECTION         = 22,
	PROP_VELOCITY          = 23 | PROP_UNIT_VELOCITY,
	PROP_ACCELERATION      = 24 | PROP_UNIT_ACCELERATION,
	PROP_MATRIX            = 25,
	PROP_EULER             = 26 | PROP_UNIT_ROTATION,
	PROP_QUATERNION        = 27,
	PROP_AXISANGLE         = 28,
	PROP_XYZ               = 29,
	PROP_XYZ_LENGTH        = 29 | PROP_UNIT_LENGTH,
	PROP_COLOR_GAMMA       = 30, /* used for colors which would be color managed before display */
	PROP_COORDS            = 31, /* generic array, no units applied, only that x/y/z/w are used (python vec) */

	/* booleans */
	PROP_LAYER             = 40,
	PROP_LAYER_MEMBER      = 41,
} PropertySubType;

/* Make sure enums are updated with these */
/* HIGHEST FLAG IN USE: 1 << 31
 * FREE FLAGS: 2, 3, 7, 9, 11, 13, 14, 15, 30 */
typedef enum PropertyFlag {
	/* editable means the property is editable in the user
	 * interface, properties are editable by default except
	 * for pointers and collections. */
	PROP_EDITABLE                = (1 << 0),

	/* this property is editable even if it is lib linked,
	 * meaning it will get lost on reload, but it's useful
	 * for editing. */
	PROP_LIB_EXCEPTION           = (1 << 16),

	/* animatable means the property can be driven by some
	 * other input, be it animation curves, expressions, ..
	 * properties are animatable by default except for pointers
	 * and collections */
	PROP_ANIMATABLE              = (1 << 1),

	/* This flag means when the property's widget is in 'textedit' mode, it will be updated
	 * after every typed char, instead of waiting final validation. Used e.g. for text searchbox.
	 * It will also cause UI_BUT_VALUE_CLEAR to be set for text buttons. We could add an own flag
	 * for search/filter properties, but this works just fine for now. */
	PROP_TEXTEDIT_UPDATE         = (1u << 31),

	/* icon */
	PROP_ICONS_CONSECUTIVE       = (1 << 12),

	/* hidden in  the user interface */
	PROP_HIDDEN                  = (1 << 19),
	/* do not write in presets */
	PROP_SKIP_SAVE               = (1 << 28),

	/* numbers */

	/* each value is related proportionally (object scale, image size) */
	PROP_PROPORTIONAL            = (1 << 26),

	/* pointers */
	PROP_ID_REFCOUNT             = (1 << 6),

	/* disallow assigning a variable to its self, eg an object tracking its self
	 * only apply this to types that are derived from an ID ()*/
	PROP_ID_SELF_CHECK           = (1 << 20),
	/* use for...
	 * - pointers: in the UI and python so unsetting or setting to None won't work
	 * - strings: so our internal generated get/length/set functions know to do NULL checks before access [#30865] */
	PROP_NEVER_NULL              = (1 << 18),
	/* currently only used for UI, this is similar to PROP_NEVER_NULL
	 * except that the value may be NULL at times, used for ObData, where an Empty's will be NULL
	 * but setting NULL on a mesh object is not possible. So, if its not NULL, setting NULL cant be done! */
	PROP_NEVER_UNLINK            = (1 << 25),

	/* flag contains multiple enums.
	 * note: not to be confused with prop->enumbitflags
	 * this exposes the flag as multiple options in python and the UI.
	 *
	 * note: these can't be animated so use with care.
	 */
	PROP_ENUM_FLAG               = (1 << 21),

	/* need context for update function */
	PROP_CONTEXT_UPDATE          = (1 << 22),
	PROP_CONTEXT_PROPERTY_UPDATE = PROP_CONTEXT_UPDATE | (1 << 27),

	/* registering */
	PROP_REGISTER                = (1 << 4),
	PROP_REGISTER_OPTIONAL       = PROP_REGISTER | (1 << 5),

	/* Use for arrays or for any data that should not have a reference kept
	 * most common case is functions that return arrays where the array */
	PROP_THICK_WRAP              = (1 << 23),

	PROP_EXPORT                  = (1 << 8),  /* XXX Is this still used? makesrna.c seems to ignore it currently... */
	PROP_IDPROPERTY              = (1 << 10), /* This is an IDProperty, not a DNA one. */
	PROP_DYNAMIC                 = (1 << 17), /* for dynamic arrays, and retvals of type string */
	PROP_ENUM_NO_CONTEXT         = (1 << 24), /* for enum that shouldn't be contextual */
	PROP_ENUM_NO_TRANSLATE       = (1 << 29), /* for enums not to be translated (e.g. renderlayers' names in nodes) */
} PropertyFlag;

/* Function parameters flags.
 * WARNING: 16bits only. */
typedef enum ParameterFlag {
	PARM_REQUIRED                = (1 << 0),
	PARM_OUTPUT                  = (1 << 1),
	PARM_RNAPTR                  = (1 << 2),
	/* This allows for non-breaking API updates, when adding non-critical new parameter to a callback function.
	 * This way, old py code defining funcs without that parameter would still work.
	 * WARNING: any parameter after the first PYFUNC_OPTIONAL one will be considered as optional!
	 * NOTE: only for input parameters!
	 */
	PARM_PYFUNC_OPTIONAL         = (1 << 3),
} ParameterFlag;

struct CollectionPropertyIterator;
struct Link;
typedef int (*IteratorSkipFunc)(struct CollectionPropertyIterator *iter, void *data);

typedef struct ListBaseIterator {
	struct Link *link;
	int flag;
	IteratorSkipFunc skip;
} ListBaseIterator;

typedef struct ArrayIterator {
	char *ptr;
	char *endptr;  /* past the last valid pointer, only for comparisons, ignores skipped values */
	void *free_ptr; /* will be freed if set */
	int itemsize;

	/* array length with no skip functions applied, take care not to compare against index from animsys
	 * or python indices */
	int length;

	/* optional skip function, when set the array as viewed by rna can contain only a subset of the members.
	 * this changes indices so quick array index lookups are not possible when skip function is used. */
	IteratorSkipFunc skip;
} ArrayIterator;

typedef struct CollectionPropertyIterator {
	/* internal */
	PointerRNA parent;
	PointerRNA builtin_parent;
	struct PropertyRNA *prop;
	union {
		ArrayIterator array;
		ListBaseIterator listbase;
	} internal;
	int idprop;
	int level;

	/* external */
	PointerRNA ptr;
	int valid;
} CollectionPropertyIterator;

typedef struct CollectionPointerLink {
	struct CollectionPointerLink *next, *prev;
	PointerRNA ptr;
} CollectionPointerLink;

/* Copy of ListBase for RNA... */
typedef struct CollectionListBase {
	struct CollectionPointerLink *first, *last;
} CollectionListBase;

typedef enum RawPropertyType {
	PROP_RAW_UNSET = -1,
	PROP_RAW_INT, // XXX - abused for types that are not set, eg. MFace.verts, needs fixing.
	PROP_RAW_SHORT,
	PROP_RAW_CHAR,
	PROP_RAW_BOOLEAN,
	PROP_RAW_DOUBLE,
	PROP_RAW_FLOAT
} RawPropertyType;

typedef struct RawArray {
	void *array;
	RawPropertyType type;
	int len;
	int stride;
} RawArray;

/**
 * This struct is are typically defined in arrays which define an *enum* for RNA,
 * which is used by the RNA API both for user-interface and the Python API.
 */
typedef struct EnumPropertyItem {
	/** The internal value of the enum, not exposed to users. */
	int value;
	/**
	 * Note that identifiers must be unique within the array,
	 * by convention they're upper case with underscores for separators.
	 * - An empty string is used to define menu separators.
	 * - NULL denotes the end of the array of items.
	 */
	const char *identifier;
	/** Optional icon, typically 'ICON_NONE' */
	int icon;
	/** Name displayed in the interface. */
	const char *name;
	/** Longer description used in the interface. */
	const char *description;
} EnumPropertyItem;

/* extended versions with PropertyRNA argument */
typedef bool (*BooleanPropertyGetFunc)(struct PointerRNA *ptr, struct PropertyRNA *prop);
typedef void (*BooleanPropertySetFunc)(struct PointerRNA *ptr, struct PropertyRNA *prop, bool value);
typedef void (*BooleanArrayPropertyGetFunc)(struct PointerRNA *ptr, struct PropertyRNA *prop, bool *values);
typedef void (*BooleanArrayPropertySetFunc)(struct PointerRNA *ptr, struct PropertyRNA *prop, const bool *values);
typedef int (*IntPropertyGetFunc)(struct PointerRNA *ptr, struct PropertyRNA *prop);
typedef void (*IntPropertySetFunc)(struct PointerRNA *ptr, struct PropertyRNA *prop, int value);
typedef void (*IntArrayPropertyGetFunc)(struct PointerRNA *ptr, struct PropertyRNA *prop, int *values);
typedef void (*IntArrayPropertySetFunc)(struct PointerRNA *ptr, struct PropertyRNA *prop, const int *values);
typedef void (*IntPropertyRangeFunc)(struct PointerRNA *ptr, struct PropertyRNA *prop, int *min, int *max, int *softmin, int *softmax);
typedef float (*FloatPropertyGetFunc)(struct PointerRNA *ptr, struct PropertyRNA *prop);
typedef void (*FloatPropertySetFunc)(struct PointerRNA *ptr, struct PropertyRNA *prop, float value);
typedef void (*FloatArrayPropertyGetFunc)(struct PointerRNA *ptr, struct PropertyRNA *prop, float *values);
typedef void (*FloatArrayPropertySetFunc)(struct PointerRNA *ptr, struct PropertyRNA *prop, const float *values);
typedef void (*FloatPropertyRangeFunc)(struct PointerRNA *ptr, struct PropertyRNA *prop, float *min, float *max, float *softmin, float *softmax);
typedef void (*StringPropertyGetFunc)(struct PointerRNA *ptr, struct PropertyRNA *prop, char *value);
typedef int (*StringPropertyLengthFunc)(struct PointerRNA *ptr, struct PropertyRNA *prop);
typedef void (*StringPropertySetFunc)(struct PointerRNA *ptr, struct PropertyRNA *prop, const char *value);
typedef int (*EnumPropertyGetFunc)(struct PointerRNA *ptr, struct PropertyRNA *prop);
typedef void (*EnumPropertySetFunc)(struct PointerRNA *ptr, struct PropertyRNA *prop, int value);
/* same as PropEnumItemFunc */
typedef const EnumPropertyItem *(*EnumPropertyItemFunc)(struct bContext *C, PointerRNA *ptr, struct PropertyRNA *prop, bool *r_free);

typedef struct PropertyRNA PropertyRNA;

/* Parameter List */

typedef struct ParameterList {
	/* storage for parameters */
	void *data;

	/* function passed at creation time */
	struct FunctionRNA *func;

	/* store the parameter size */
	int alloc_size;

	int arg_count, ret_count;
} ParameterList;

typedef struct ParameterIterator {
	struct ParameterList *parms;
	/* PointerRNA funcptr; */ /*UNUSED*/
	void *data;
	int size, offset;

	PropertyRNA *parm;
	int valid;
} ParameterIterator;

/* mainly to avoid confusing casts */
typedef struct ParameterDynAlloc {
	intptr_t array_tot; /* important, this breaks when set to an int */
	void *array;
} ParameterDynAlloc;

/* Function */

typedef enum FunctionFlag {
	/***** Options affecting callback signature. *****/
	/* Those add additional parameters at the beginning of the C callback, like that:
	 *     rna_my_func([ID *_selfid],
	 *                 [<DNA_STRUCT> *self|StructRNA *type],
	 *                 [Main *bmain],
	 *                 [bContext *C],
	 *                 [ReportList *reports],
	 *                 <other RNA-defined parameters>);
	 */
	/* Pass ID owning 'self' data (i.e. ptr->id.data, might be same as self in case data is an ID...). */
	FUNC_USE_SELF_ID       = (1 << 11),

	/* Do not pass the object (DNA struct pointer) from which it is called, used to define static or class functions. */
	FUNC_NO_SELF           = (1 << 0),
	/* Pass RNA type, used to define class functions, only valid when FUNC_NO_SELF is set. */
	FUNC_USE_SELF_TYPE     = (1 << 1),

	/* Pass Main, bContext and/or ReportList. */
	FUNC_USE_MAIN          = (1 << 2),
	FUNC_USE_CONTEXT       = (1 << 3),
	FUNC_USE_REPORTS       = (1 << 4),


	/***** Registering of python subclasses. *****/
	/* This function is part of the registerable class' interface, and can be implemented/redefined in python. */
	FUNC_REGISTER          = (1 << 5),
	/* Subclasses can choose not to implement this function. */
	FUNC_REGISTER_OPTIONAL = FUNC_REGISTER | (1 << 6),
	/* If not set, the python function implementing this call is not allowed to write into data-blocks.
	 * Except for WindowManager and Screen currently, see rna_id_write_error() in bpy_rna.c */
	FUNC_ALLOW_WRITE       = (1 << 12),

	/***** Internal flags. *****/
	/* UNUSED CURRENTLY? ??? */
	FUNC_BUILTIN           = (1 << 7),
	/* UNUSED CURRENTLY. ??? */
	FUNC_EXPORT            = (1 << 8),
	/* Function has been defined at runtime, not statically in RNA source code. */
	FUNC_RUNTIME           = (1 << 9),
	/* UNUSED CURRENTLY? Function owns its identifier and description strings, and has to free them when deleted. */
	FUNC_FREE_POINTERS     = (1 << 10),
} FunctionFlag;

typedef void (*CallFunc)(struct bContext *C, struct ReportList *reports, PointerRNA *ptr, ParameterList *parms);

typedef struct FunctionRNA FunctionRNA;

/* Struct */

typedef enum StructFlag {
	/* indicates that this struct is an ID struct, and to use refcounting */
	STRUCT_ID              = (1 << 0),
	STRUCT_ID_REFCOUNT     = (1 << 1),
	STRUCT_UNDO            = (1 << 2), /* defaults on, clear for user preferences and similar */

	/* internal flags */
	STRUCT_RUNTIME         = (1 << 3),
	STRUCT_GENERATED       = (1 << 4),
	STRUCT_FREE_POINTERS   = (1 << 5),
	STRUCT_NO_IDPROPERTIES = (1 << 6), /* Menus and Panels don't need properties */
	STRUCT_NO_DATABLOCK_IDPROPERTIES = (1 << 7), /* e.g. for Operator */
	STRUCT_CONTAINS_DATABLOCK_IDPROPERTIES = (1 << 8), /* for PropertyGroup which contains pointers to datablocks */
	STRUCT_PUBLIC_NAMESPACE = (1 << 9), /* Added to type-map #BlenderRNA.structs_map */
	STRUCT_PUBLIC_NAMESPACE_INHERIT = (1 << 10), /* All subtypes are added too. */
} StructFlag;

typedef int (*StructValidateFunc)(struct PointerRNA *ptr, void *data, int *have_function);
typedef int (*StructCallbackFunc)(struct bContext *C, struct PointerRNA *ptr, struct FunctionRNA *func, ParameterList *list);
typedef void (*StructFreeFunc)(void *data);
typedef struct StructRNA *(*StructRegisterFunc)(
        struct Main *bmain, struct ReportList *reports, void *data, const char *identifier,
        StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free);

typedef void (*StructUnregisterFunc)(struct Main *bmain, struct StructRNA *type);
typedef void **(*StructInstanceFunc)(PointerRNA *ptr);

typedef struct StructRNA StructRNA;

/* Blender RNA
 *
 * Root RNA data structure that lists all struct types. */

typedef struct BlenderRNA BlenderRNA;

/* Extending
 *
 * This struct must be embedded in *Type structs in
 * order to make then definable through RNA. */

typedef struct ExtensionRNA {
	void *data;
	StructRNA *srna;
	StructCallbackFunc call;
	StructFreeFunc free;

} ExtensionRNA;

#ifdef __cplusplus
}
#endif

#endif /* __RNA_TYPES_H__ */
