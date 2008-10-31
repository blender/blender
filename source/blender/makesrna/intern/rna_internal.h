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

struct StructRNA;
struct PropertyRNA;
struct SDNA;
struct ListBase;

/* Data structures used during define */

typedef struct PropertyDefRNA {
	struct PropertyDefRNA *next, *prev;

	struct StructRNA *strct;
	struct PropertyRNA *prop;

	const char *dnastructname;
	const char *dnaname;
	const char *dnatype;
	int dnaarraylength;

	int booleanbit;
} PropertyDefRNA;

typedef struct StructDefRNA {
	struct StructDefRNA *next, *prev;

	struct StructRNA *strct;

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

void RNA_def_object(struct BlenderRNA *brna);
void RNA_def_scene(struct BlenderRNA *brna);

/* Standard iterator functions */

void rna_iterator_listbase_begin(struct CollectionPropertyIterator *iter, struct ListBase *lb);
void rna_iterator_listbase_next(struct CollectionPropertyIterator *iter);
void *rna_iterator_listbase_get(struct CollectionPropertyIterator *iter);

void rna_iterator_array_begin(struct CollectionPropertyIterator *iter, void *ptr, int itemsize, int length);
void rna_iterator_array_next(struct CollectionPropertyIterator *iter);
void *rna_iterator_array_get(struct CollectionPropertyIterator *iter);
void rna_iterator_array_end(struct CollectionPropertyIterator *iter);

/* Duplicated code since we can't link in blenlib */

void rna_addtail(struct ListBase *listbase, void *vlink);
void rna_freelistN(struct ListBase *listbase);

#endif /* RNA_INTERNAL_H */

