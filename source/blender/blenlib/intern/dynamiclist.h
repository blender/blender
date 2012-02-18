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
 * Contributor(s): Jiri Hnidek.
 *
 * Documentation of Two way dynamic list with access array can be found at:
 *
 * http://wiki.blender.org/bin/view.pl/Blenderwiki/DynamicListWithAccessArray
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/intern/dynamiclist.h
 *  \ingroup bli
 */


#ifndef __DYNAMICLIST_H__
#define __DYNAMICLIST_H__

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
