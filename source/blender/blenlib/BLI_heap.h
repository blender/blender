/**
 * A heap / priority queue ADT
 * 
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: none of this file.
 *
 * Contributor(s): Brecht Van Lommel
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#ifndef BLI_HEAP_H
#define BLI_HEAP_H

struct Heap;
struct HeapNode;
typedef struct Heap Heap;
typedef struct HeapNode HeapNode;

typedef	void	(*HeapFreeFP)(void *ptr);

/* Creates a new heap. BLI_memarena is used for allocating nodes. Removed nodes
   are recycled, so memory usage will not shrink. */
Heap*			BLI_heap_new	(void);
void			BLI_heap_free	(Heap *heap, HeapFreeFP ptrfreefp);

/* Insert heap node with a value (often a 'cost') and pointer into the heap,
   duplicate values are allowed. */
HeapNode*		BLI_heap_insert		(Heap *heap, float value, void *ptr);

/* Remove a heap node. */
void			BLI_heap_remove		(Heap *heap, HeapNode *node);

/* Return 0 if the heap is empty, 1 otherwise. */
int				BLI_heap_empty		(Heap *heap);

/* Return the size of the heap. */
int				BLI_heap_size		(Heap *heap);

/* Return the top node of the heap. This is the node with the lowest value. */
HeapNode*		BLI_heap_top		(Heap *heap);

/* Pop the top node off the heap and return it's pointer. */
void*			BLI_heap_popmin		(Heap *heap);

/* Return the value or pointer of a heap node. */
float			BLI_heap_node_value	(HeapNode *heap);
void*			BLI_heap_node_ptr	(HeapNode *heap);

#endif

