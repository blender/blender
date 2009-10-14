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

#ifdef __cplusplus
extern "C" {
#endif

struct ParameterList;
struct FunctionRNA;
struct PropertyRNA;
struct StructRNA;
struct BlenderRNA;
struct IDProperty;
struct bContext;
struct ReportList;

/* Pointer
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

/* Property */

typedef enum PropertyType {
	PROP_BOOLEAN = 0,
	PROP_INT = 1,
	PROP_FLOAT = 2,
	PROP_STRING = 3,
	PROP_ENUM = 4,
	PROP_POINTER = 5,
	PROP_COLLECTION = 6
} PropertyType;

/* also update rna_property_subtype_unit when you change this */
typedef enum PropertyUnit {
	PROP_UNIT_NONE = (0<<16),
	PROP_UNIT_LENGTH = (1<<16),			/* m */
	PROP_UNIT_AREA = (2<<16),			/* m^2 */
	PROP_UNIT_VOLUME = (3<<16),			/* m^3 */
	PROP_UNIT_MASS = (4<<16),			/* kg */
	PROP_UNIT_ROTATION = (5<<16),		/* rad */
	PROP_UNIT_TIME = (6<<16),			/* frame */
	PROP_UNIT_VELOCITY = (7<<16),		/* m/s */
	PROP_UNIT_ACCELERATION = (8<<16)	/* m/(s^2) */
} PropertyUnit;

#define RNA_SUBTYPE_UNIT(subtype) (subtype & 0x00FF0000)
#define RNA_SUBTYPE_UNIT_VALUE(subtype) (subtype>>16)

/* also update rna_property_subtypename when you change this */
typedef enum PropertySubType {
	PROP_NONE = 0,

	/* strings */
	PROP_FILEPATH = 1,
	PROP_DIRPATH = 2,
	PROP_FILENAME = 3,

	/* numbers */
	PROP_UNSIGNED = 13,
	PROP_PERCENTAGE = 14,
	PROP_FACTOR = 15,
	PROP_ANGLE = 16|PROP_UNIT_ROTATION,
	PROP_TIME = 17|PROP_UNIT_TIME,
	PROP_DISTANCE = 18|PROP_UNIT_LENGTH,

	/* number arrays */
	PROP_COLOR = 20,
	PROP_TRANSLATION = 21|PROP_UNIT_LENGTH,
	PROP_DIRECTION = 22,
	PROP_VELOCITY = 23|PROP_UNIT_VELOCITY,
	PROP_ACCELERATION = 24|PROP_UNIT_ACCELERATION,
	PROP_MATRIX = 25,
	PROP_EULER = 26|PROP_UNIT_ROTATION,
	PROP_QUATERNION = 27,
	PROP_AXISANGLE = 28,
	PROP_XYZ = 29,
	PROP_RGB = 30,

	/* booleans */
	PROP_LAYER = 40,
	PROP_LAYER_MEMBER = 41
} PropertySubType;

typedef enum PropertyFlag {
	/* editable means the property is editable in the user
	 * interface, properties are editable by default except
	 * for pointers and collections. */
	PROP_EDITABLE = 1<<0,

	/* this property is editable even if it is lib linked,
	 * meaning it will get lost on reload, but it's useful
	 * for editing. */
	PROP_LIB_EXCEPTION = 1<<16,

	/* animateable means the property can be driven by some
	 * other input, be it animation curves, expressions, ..
	 * properties are animateable by default except for pointers
	 * and collections */
	PROP_ANIMATEABLE = 1<<1,

	/* icon */
	PROP_ICONS_CONSECUTIVE = 1<12,

	/* hidden in  the user interface */
	PROP_HIDDEN = 1<<19,

	/* function paramater flags */
	PROP_REQUIRED = 1<<2,
	PROP_RETURN = 1<<3,
	PROP_RNAPTR = 1<<11,
	/* registering */
	PROP_REGISTER = 1<<4,
	PROP_REGISTER_OPTIONAL = (1<<4)|(1<<5),

	/* pointers */
	PROP_ID_REFCOUNT = 1<<6,

	/* disallow assigning a variable to its self, eg an object tracking its self
	 * only apply this to types that are derived from an ID ()*/
	PROP_ID_SELF_CHECK = 1<<20,

	PROP_NEVER_NULL = 1<<18,

	/* internal flags */
	PROP_BUILTIN = 1<<7,
	PROP_EXPORT = 1<<8,
	PROP_RUNTIME = 1<<9,
	PROP_IDPROPERTY = 1<<10,
	PROP_RAW_ACCESS = 1<<13,
	PROP_RAW_ARRAY = 1<<14,
	PROP_FREE_POINTERS = 1<<15,
	PROP_DYNAMIC = 1<<17 /* for dynamic arrays, and retvals of type string */
} PropertyFlag;

typedef struct CollectionPropertyIterator {
	/* internal */
	PointerRNA parent;
	PointerRNA builtin_parent;
	struct PropertyRNA *prop;
	void *internal;
	int idprop;
	int level;

	/* external */
	int valid;
	PointerRNA ptr;
} CollectionPropertyIterator;

typedef struct CollectionPointerLink {
	struct CollectionPointerLink *next, *prev;
	PointerRNA ptr;
} CollectionPointerLink;

typedef enum RawPropertyType {
	PROP_RAW_INT, // XXX - abused for types that are not set, eg. MFace.verts, needs fixing.
	PROP_RAW_SHORT,
	PROP_RAW_CHAR,
	PROP_RAW_DOUBLE,
	PROP_RAW_FLOAT
} RawPropertyType;

typedef struct RawArray {
	void *array;
	RawPropertyType type;
	int len;
	int stride;
} RawArray;

typedef struct EnumPropertyItem {
	int value;
	const char *identifier;
	int icon;
	const char *name;
	const char *description;
} EnumPropertyItem;

typedef EnumPropertyItem *(*EnumPropertyItemFunc)(struct bContext *C, PointerRNA *ptr, int *free);

typedef struct PropertyRNA PropertyRNA;

/* Parameter List */

typedef struct ParameterList {
	/* storage for parameters */
	void *data;

	/* store the parameter count */
	int tot;

	/* function passed at creation time */
	struct FunctionRNA *func;
} ParameterList;

typedef struct ParameterIterator {
	struct ParameterList *parms;
	PointerRNA funcptr;
	void *data;
	int size, offset;

	PropertyRNA *parm;
	int valid;
} ParameterIterator;

/* Function */

typedef enum FunctionFlag {
	FUNC_NO_SELF = 1, /* for static functions */
	FUNC_USE_CONTEXT = 2,
	FUNC_USE_REPORTS = 4,

	/* registering */
	FUNC_REGISTER = 8,
	FUNC_REGISTER_OPTIONAL = 8|16,

	/* internal flags */
	FUNC_BUILTIN = 128,
	FUNC_EXPORT = 256,
	FUNC_RUNTIME = 512,
	FUNC_FREE_POINTERS = 1024
} FunctionFlag;

typedef void (*CallFunc)(struct bContext *C, struct ReportList *reports, PointerRNA *ptr, ParameterList *parms);

typedef struct FunctionRNA FunctionRNA;

/* Struct */

typedef enum StructFlag {
	/* indicates that this struct is an ID struct, and to use refcounting */
	STRUCT_ID = 1,
	STRUCT_ID_REFCOUNT = 2,

	/* internal flags */
	STRUCT_RUNTIME = 4,
	STRUCT_GENERATED = 8,
	STRUCT_FREE_POINTERS = 16
} StructFlag;

typedef int (*StructValidateFunc)(struct PointerRNA *ptr, void *data, int *have_function);
typedef int (*StructCallbackFunc)(struct PointerRNA *ptr, struct FunctionRNA *func, ParameterList *list);
typedef void (*StructFreeFunc)(void *data);
typedef struct StructRNA *(*StructRegisterFunc)(const struct bContext *C, struct ReportList *reports, void *data,
	const char *identifier, StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free);
typedef void (*StructUnregisterFunc)(const struct bContext *C, struct StructRNA *type);

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

	int (*call)(PointerRNA *, FunctionRNA *, ParameterList *);
	void (*free)(void *data);
} ExtensionRNA;

#ifdef __cplusplus
}
#endif

#endif /* RNA_TYPES */


