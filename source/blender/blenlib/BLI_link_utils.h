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
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_LINK_UTILS_H__
#define __BLI_LINK_UTILS_H__

/** \file BLI_link_utils.h
 *  \ingroup bli
 *  \brief Single link-list utility macros. (header only api).
 *
 * Use this api when the structure defines its own ``next`` pointer
 * and a double linked list such as #ListBase isnt needed.
 */

#define BLI_LINKS_PREPEND(list, link)  { \
	CHECK_TYPE_PAIR(list, link); \
	(link)->next = list; \
	list = link; \
} (void)0

#define BLI_LINKS_FREE(list)  { \
	while (list) { \
		void *next = list->next; \
		MEM_freeN(list); \
		list = next; \
	} \
} (void)0

#endif  /* __BLI_LINK_UTILS_H__ */
