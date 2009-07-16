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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * Contributor(s): Jiri Hnidek.
 *
 * Documentation of Two way dynamic list with access array can be found at:
 *
 * http://wiki.blender.org/bin/view.pl/Blenderwiki/DynamicListWithAccessArray
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef B_DYNAMIC_LIST_H
#define B_DYNAMIC_LIST_H

#define PAGE_SIZE 4

struct ListBase;

/*
 * Access array using realloc 
 */
typedef struct DynamicArray{
	unsigned int count;		/* count of items in list */
	unsigned int max_item_index;	/* max available index */
	unsigned int last_item_index;	/* max used index */
	void **items;			/* dynamicaly allocated array of pointers
					   pointing at items in list */
} DynamicArray;

/*
 * Two way dynamic list with access array
 */
typedef struct DynamicList {
	struct DynamicArray da;		/* access array */
	struct ListBase lb;		/* two way linked dynamic list */
} DynamicList;

#endif
