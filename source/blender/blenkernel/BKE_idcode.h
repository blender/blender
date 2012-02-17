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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_IDCODE_H__
#define __BKE_IDCODE_H__

/** \file BKE_idcode.h
 *  \ingroup bke
 */

/**
 * Convert an idcode into a name.
 * 
 * @param code The code to convert.
 * @return A static string representing the name of
 * the code.
 */
const char *BKE_idcode_to_name(int code);

/**
 * Convert an idcode into a name (plural).
 * 
 * @param code The code to convert.
 * @return A static string representing the name of
 * the code.
 */
const char *BKE_idcode_to_name_plural(int code);

/**
 * Convert a name into an idcode (ie. ID_SCE)
 * 
 * @param name The name to convert.
 * @return The code for the name, or 0 if invalid.
 */
int BKE_idcode_from_name(const char *name);

/**
 * Return non-zero when an ID type is linkable.
 * 
 * @param code The code to check.
 * @return Boolean, 0 when non linkable.
 */
int BKE_idcode_is_linkable(int code);

/**
 * Return if the ID code is a valid ID code.
 * 
 * @param code The code to check.
 * @return Boolean, 0 when invalid.
 */
int BKE_idcode_is_valid(int code);

/**
 * Return an ID code and steps the index forward 1.
 *
 * @param index, start as 0.
 * @return the code, 0 when all codes have been returned.
 */
int BKE_idcode_iter_step(int *index);

#endif
