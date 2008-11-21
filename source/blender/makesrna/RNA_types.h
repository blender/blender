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

/* Pointer
 *
 * RNA pointers are not a single C pointer but include the type,
 * and a pointer to the ID struct that owns the struct, since
 * in some cases this information is needed to correctly get/set
 * the properties and validate them. */

typedef struct PointerRNA {
	struct {
		struct StructRNA *type;
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
	PROP_COLOR = 3,
	PROP_VECTOR = 4,
	PROP_MATRIX = 5,
	PROP_ROTATION = 6
} PropertySubType;

typedef enum PropertyFlag {
	/* editable means the property is editable in the user
	 * interface, evaluated means that the property is set
	 * as part of an evaluation. these can change at runtime
	 * the property flag contains the default. editable is
	 * enabled by default except for collections. */
	PROP_NOT_EDITABLE = 1,
	PROP_EVALUATED = 2,

	/* driveable means the property can be driven by some
	 * other input, be it animation curves, expressions, ..
	 * in other words making the property evaluated.
	 * enable by default except for pointers and collections. */
	PROP_NOT_DRIVEABLE = 4,

#if 0
	/* for pointers and collections, means that the struct
	 * depends on the data pointed to for evaluation, such
	 * that a change in the data pointed to will affect the
	 * evaluated result of this struct. */
	PROP_EVALUATE_DEPENDENCY = 8,
	PROP_INVERSE_EVALUATE_DEPENDENCY = 16,

	/* for pointers and collections, means that the struct
	 * requires the data pointed to for rendering in the,
	 * be it the render engine or viewport */
	PROP_RENDER_DEPENDENCY = 32,
	PROP_INVERSE_RENDER_DEPENDENCY = 64,
#endif

	/* internal flags */
	PROP_BUILTIN = 128,
	PROP_EXPORT = 256,
	PROP_RUNTIME = 512,
	PROP_IDPROPERTY = 1024
} PropertyFlag;

typedef struct CollectionPropertyIterator {
	PointerRNA parent;
	void *internal;

	int valid;
	PointerRNA ptr;
} CollectionPropertyIterator;

typedef struct EnumPropertyItem {
	int value;
	const char *identifier;
	const char *name;
} EnumPropertyItem;

struct PropertyRNA;
typedef struct PropertyRNA PropertyRNA;

/* Struct */

typedef enum StructFlag {
	/* indicates that this struct is an ID struct */
	STRUCT_ID = 1,

	/* internal flags */
	STRUCT_RUNTIME = 2
} StructFlag;

struct StructRNA;
typedef struct StructRNA StructRNA;

/* Blender RNA
 *
 * Root RNA data structure that lists all struct types. */

struct BlenderRNA;
typedef struct BlenderRNA BlenderRNA;

#endif /* RNA_TYPES */

