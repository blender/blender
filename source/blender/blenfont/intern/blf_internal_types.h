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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 * 
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BLF_INTERNAL_TYPES_H
#define BLF_INTERNAL_TYPES_H

typedef struct LangBLF {
	struct LangBLF *next;
	struct LangBLF *prev;

	char *line;
	char *language;
	char *code;
	int id;
} LangBLF;

#define BLF_LANG_FIND_BY_LINE 0
#define BLF_LANG_FIND_BY_LANGUAGE 1
#define BLF_LANG_FIND_BY_CODE 2

#endif /* BLF_INTERNAL_TYPES_H */
