/**
 * @file BLI_dynstr.h
 * 
 * A dynamically sized string ADT.
 * This ADT is designed purely for dynamic string creation
 * through appending, not for general usage, the intent is
 * to build up dynamic strings using a DynStr object, then
 * convert it to a c-string and work with that.
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#ifndef BLI_DYNSTR_H
#define BLI_DYNSTR_H

#include <stdarg.h>

struct DynStr;

	/** The abstract DynStr type */
typedef struct DynStr DynStr;

	/**
	 * Create a new DynStr.
	 * 
	 * @return Pointer to a new DynStr.
	 */
DynStr*	BLI_dynstr_new					(void);

	/**
	 * Append a c-string to a DynStr.
	 * 
	 * @param ds The DynStr to append to.
	 * @param cstr The c-string to append.
	 */
void	BLI_dynstr_append				(DynStr *ds, const char *cstr);

	/**
	 * Append a c-string to a DynStr, but with formatting like printf.
	 * 
	 * @param ds The DynStr to append to.
	 * @param format The printf format string to use.
	 */
void	BLI_dynstr_appendf				(DynStr *ds, const char *format, ...);
void	BLI_dynstr_vappendf				(DynStr *ds, const char *format, va_list args);

	/**
	 * Find the length of a DynStr.
	 * 
	 * @param ds The DynStr of interest.
	 * @return The length of @a ds.
	 */
int		BLI_dynstr_get_len				(DynStr *ds);

	/**
	 * Get a DynStr's contents as a c-string.
	 * <i> The returned c-string should be free'd
	 * using MEM_freeN. </i>
	 * 
	 * @param ds The DynStr of interest.
	 * @return The contents of @a ds as a c-string.
	 */
char*	BLI_dynstr_get_cstring			(DynStr *ds);

	/**
	 * Free the DynStr
	 * 
	 * @param ds The DynStr to free.
	 */
void	BLI_dynstr_free					(DynStr *ds);

#endif

