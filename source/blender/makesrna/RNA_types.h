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

typedef enum PropertySubType {
	PROP_NONE = 0,
	PROP_UNSIGNED = 1,
	PROP_FILEPATH = 2,
	PROP_DIRPATH = 3,
	PROP_COLOR = 4,
	PROP_VECTOR = 5,
	PROP_MATRIX = 6,
	PROP_ROTATION = 7,
	PROP_NEVER_NULL = 8,
	PROP_PERCENTAGE = 9
} PropertySubType;

typedef enum PropertyFlag {
	/* editable means the property is editable in the user
	 * interface, properties are editable by default except
	 * for pointers and collections. */
	PROP_EDITABLE = 1,

	/* animateable means the property can be driven by some
	 * other input, be it animation curves, expressions, ..
	 * properties are animateable by default except for pointers
	 * and collections */
	PROP_ANIMATEABLE = 2,

	/* function paramater flags */
	PROP_REQUIRED = 4,
	PROP_RETURN = 8,

	/* registering */
	PROP_REGISTER = 16,
	PROP_REGISTER_OPTIONAL = 16|32,

	/* internal flags */
	PROP_BUILTIN = 128,
	PROP_EXPORT = 256,
	PROP_RUNTIME = 512,
	PROP_IDPROPERTY = 1024
} PropertyFlag;

typedef struct CollectionPropertyIterator {
	/* internal */
	PointerRNA parent;
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

/* Iterator Utility */

typedef struct EnumPropertyItem {
	int value;
	const char *identifier;
	const char *name;
	const char *description;
} EnumPropertyItem;

typedef struct PropertyRNA PropertyRNA;

/* Parameter List */

typedef struct ParameterList ParameterList;

typedef struct ParameterIterator {
	ParameterList *parms;
	PointerRNA funcptr;
	void *data;
	int size, offset;

	PropertyRNA *parm;
	int valid;
} ParameterIterator;

/* Function */

typedef enum FunctionFlag {
	FUNC_TYPESTATIC = 1, /* for static functions, FUNC_ STATIC is taken by some windows header it seems */

	/* registering */
	FUNC_REGISTER = 2,
	FUNC_REGISTER_OPTIONAL = 2|4,

	/* internal flags */
	FUNC_BUILTIN = 128,
	FUNC_EXPORT = 256,
	FUNC_RUNTIME = 512
} FunctionFlag;

typedef void (*CallFunc)(PointerRNA *ptr, ParameterList *parms);

typedef struct FunctionRNA FunctionRNA;

/* Struct */

typedef enum StructFlag {
	/* indicates that this struct is an ID struct */
	STRUCT_ID = 1,

	/* internal flags */
	STRUCT_RUNTIME = 2,
	STRUCT_GENERATED = 4
} StructFlag;

typedef int (*StructValidateFunc)(struct PointerRNA *ptr, void *data, int *have_function);
typedef int (*StructCallbackFunc)(struct PointerRNA *ptr, struct FunctionRNA *func, struct ParameterList *list);
typedef void (*StructFreeFunc)(void *data);
typedef struct StructRNA *(*StructRegisterFunc)(const struct bContext *C, struct ReportList *reports, void *data,
	StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free);
typedef void (*StructUnregisterFunc)(const struct bContext *C, struct StructRNA *type);

typedef struct StructRNA StructRNA;

/* Blender RNA
 *
 * Root RNA data structure that lists all struct types. */

typedef struct BlenderRNA BlenderRNA;

#ifdef __cplusplus
}
#endif

#endif /* RNA_TYPES */


