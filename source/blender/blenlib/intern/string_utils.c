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
 * The Original Code is Copyright (C) 2017 by the Blender FOundation.
 * All rights reserved.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * 
 */

/** \file blender/blenlib/intern/string_utils.c
 *  \ingroup bli
 */

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "DNA_listBase.h"


#ifdef __GNUC__
#  pragma GCC diagnostic error "-Wsign-conversion"
#endif

/**
 * Looks for a numeric suffix preceded by delim character on the end of
 * name, puts preceding part into *left and value of suffix into *nr.
 * Returns the length of *left.
 *
 * Foo.001 -> "Foo", 1
 * Returning the length of "Foo"
 *
 * \param left  Where to return copy of part preceding delim
 * \param nr  Where to return value of numeric suffix
 * \param name  String to split
 * \param delim  Delimiter character
 * \return  Length of \a left
 */
int BLI_split_name_num(char *left, int *nr, const char *name, const char delim)
{
	const size_t name_len = strlen(name);

	*nr = 0;
	memcpy(left, name, (name_len + 1) * sizeof(char));

	/* name doesn't end with a delimiter "foo." */
	if ((name_len > 1 && name[name_len - 1] == delim) == 0) {
		int a = name_len;
		while (a--) {
			if (name[a] == delim) {
				left[a] = '\0';  /* truncate left part here */
				*nr = atol(name + a + 1);
				/* casting down to an int, can overflow for large numbers */
				if (*nr < 0)
					*nr = 0;
				return a;
			}
			else if (isdigit(name[a]) == 0) {
				/* non-numeric suffix - give up */
				break;
			}
		}
	}

	return name_len;
}


/* Unique name utils. */

/**
 * Ensures name is unique (according to criteria specified by caller in unique_check callback),
 * incrementing its numeric suffix as necessary. Returns true if name had to be adjusted.
 *
 * \param unique_check  Return true if name is not unique
 * \param arg  Additional arg to unique_check--meaning is up to caller
 * \param defname  To initialize name if latter is empty
 * \param delim  Delimits numeric suffix in name
 * \param name  Name to be ensured unique
 * \param name_len  Maximum length of name area
 * \return true if there if the name was changed
 */
bool BLI_uniquename_cb(
        UniquenameCheckCallback unique_check, void *arg, const char *defname, char delim, char *name, size_t name_len)
{
	if (name[0] == '\0') {
		BLI_strncpy(name, defname, name_len);
	}

	if (unique_check(arg, name)) {
		char numstr[16];
		char *tempname = alloca(name_len);
		char *left = alloca(name_len);
		int number;
		int len = BLI_split_name_num(left, &number, name, delim);
		do {
			/* add 1 to account for \0 */
			const size_t numlen = BLI_snprintf(numstr, sizeof(numstr), "%c%03d", delim, ++number) + 1;

			/* highly unlikely the string only has enough room for the number
			 * but support anyway */
			if ((len == 0) || (numlen >= name_len)) {
				/* number is know not to be utf-8 */
				BLI_strncpy(tempname, numstr, name_len);
			}
			else {
				char *tempname_buf;
				tempname_buf = tempname + BLI_strncpy_utf8_rlen(tempname, left, name_len - numlen);
				memcpy(tempname_buf, numstr, numlen);
			}
		} while (unique_check(arg, tempname));

		BLI_strncpy(name, tempname, name_len);

		return true;
	}

	return false;
}

/* little helper macro for BLI_uniquename */
#ifndef GIVE_STRADDR
#  define GIVE_STRADDR(data, offset) ( ((char *)data) + offset)
#endif

/* Generic function to set a unique name. It is only designed to be used in situations
 * where the name is part of the struct.
 *
 * For places where this is used, see constraint.c for example...
 *
 *  name_offs: should be calculated using offsetof(structname, membername) macro from stddef.h
 *  len: maximum length of string (to prevent overflows, etc.)
 *  defname: the name that should be used by default if none is specified already
 *  delim: the character which acts as a delimiter between parts of the name
 */
static bool uniquename_find_dupe(ListBase *list, void *vlink, const char *name, int name_offs)
{
	Link *link;

	for (link = list->first; link; link = link->next) {
		if (link != vlink) {
			if (STREQ(GIVE_STRADDR(link, name_offs), name)) {
				return true;
			}
		}
	}

	return false;
}

static bool uniquename_unique_check(void *arg, const char *name)
{
	struct {ListBase *lb; void *vlink; int name_offs; } *data = arg;
	return uniquename_find_dupe(data->lb, data->vlink, name, data->name_offs);
}

/**
 * Ensures that the specified block has a unique name within the containing list,
 * incrementing its numeric suffix as necessary. Returns true if name had to be adjusted.
 *
 * \param list  List containing the block
 * \param vlink  The block to check the name for
 * \param defname  To initialize block name if latter is empty
 * \param delim  Delimits numeric suffix in name
 * \param name_offs  Offset of name within block structure
 * \param name_len  Maximum length of name area
 */
bool BLI_uniquename(ListBase *list, void *vlink, const char *defname, char delim, int name_offs, size_t name_len)
{
	struct {ListBase *lb; void *vlink; int name_offs; } data;
	data.lb = list;
	data.vlink = vlink;
	data.name_offs = name_offs;

	BLI_assert(name_len > 1);

	/* See if we are given an empty string */
	if (ELEM(NULL, vlink, defname))
		return false;

	return BLI_uniquename_cb(uniquename_unique_check, &data, defname, delim, GIVE_STRADDR(vlink, name_offs), name_len);
}
