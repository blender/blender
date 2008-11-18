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

#ifndef RNA_INTERNAL_H
#define RNA_INTERNAL_H

#include "rna_internal_types.h"

#define RNA_MAGIC ((int)~0)

struct IDProperty;
struct SDNA;

/* Data structures used during define */

typedef struct PropertyDefRNA {
	struct PropertyDefRNA *next, *prev;

	struct StructRNA *srna;
	struct PropertyRNA *prop;

	const char *dnastructname;
	const char *dnaname;
	const char *dnalengthstructname;
	const char *dnalengthname;
	const char *dnatype;
	int dnaarraylength;

	int booleanbit;
} PropertyDefRNA;

typedef struct StructDefRNA {
	struct StructDefRNA *next, *prev;

	struct StructRNA *srna;

	const char *dnaname;
	ListBase properties;
} StructDefRNA;

typedef struct AllocDefRNA {
	struct AllocDefRNA *next, *prev;
	void *mem;
} AllocDefRNA;

typedef struct BlenderDefRNA {
	struct SDNA *sdna;
	ListBase structs;
	ListBase allocs;
	int error, silent;
} BlenderDefRNA;

extern BlenderDefRNA DefRNA;

/* Define functions for all types */

extern StringPropertyRNA rna_IDProperty_string;
extern IntPropertyRNA rna_IDProperty_int;
extern IntPropertyRNA rna_IDProperty_intarray;
extern FloatPropertyRNA rna_IDProperty_float;
extern FloatPropertyRNA rna_IDProperty_floatarray;
extern PointerPropertyRNA rna_IDProperty_group;
extern FloatPropertyRNA rna_IDProperty_double;
extern FloatPropertyRNA rna_IDProperty_doublearray;

extern StructRNA RNA_Main;
extern StructRNA RNA_Mesh;
extern StructRNA RNA_Object;
extern StructRNA RNA_Scene;
extern StructRNA RNA_Lamp;
extern StructRNA RNA_Struct;

void RNA_def_ID(struct StructRNA *srna);
void RNA_def_ID_types(struct BlenderRNA *brna);

void RNA_def_main(struct BlenderRNA *brna);
void RNA_def_mesh(struct BlenderRNA *brna);
void RNA_def_object(struct BlenderRNA *brna);
void RNA_def_rna(struct BlenderRNA *brna);
void RNA_def_scene(struct BlenderRNA *brna);
void RNA_def_lamp(struct BlenderRNA *brna);

/* Internal Functions */

void rna_def_builtin_properties(struct StructRNA *srna);

struct IDProperty *rna_idproperty_check(struct PropertyRNA **prop, struct PointerRNA *ptr);

typedef struct ListBaseIterator {
	Link *link;
	int flag;
} ListBaseIterator;

void rna_iterator_listbase_begin(struct CollectionPropertyIterator *iter, struct ListBase *lb);
void rna_iterator_listbase_next(struct CollectionPropertyIterator *iter);
void *rna_iterator_listbase_get(struct CollectionPropertyIterator *iter);
void rna_iterator_listbase_end(struct CollectionPropertyIterator *iter);

typedef struct ArrayIterator {
	char *ptr;
	char *endptr;
	int itemsize;
} ArrayIterator;

void rna_iterator_array_begin(struct CollectionPropertyIterator *iter, void *ptr, int itemsize, int length);
void rna_iterator_array_next(struct CollectionPropertyIterator *iter);
void *rna_iterator_array_get(struct CollectionPropertyIterator *iter);
void rna_iterator_array_end(struct CollectionPropertyIterator *iter);

/* Duplicated code since we can't link in blenlib */

void rna_addtail(struct ListBase *listbase, void *vlink);
void rna_freelistN(struct ListBase *listbase);

#endif /* RNA_INTERNAL_H */

