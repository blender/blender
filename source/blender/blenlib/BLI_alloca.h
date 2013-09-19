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

#ifndef __BLI_ALLOCA_H__
#define __BLI_ALLOCA_H__

/** \file BLI_alloca.h
 *  \ingroup bli
 *
 * Defines alloca and utility macro BLI_array_alloca
 */

/* BLI_array_alloca / alloca */
#ifdef _MSC_VER
#  define alloca _alloca
#endif

#if defined(__MINGW32__)
#  include <malloc.h>  /* mingw needs for alloca() */
#endif

#if defined(__GNUC__) || defined(__clang__)
#define BLI_array_alloca(arr, realsize) \
	(typeof(arr))alloca(sizeof(*arr) * (realsize))
#else
#define BLI_array_alloca(arr, realsize) \
	alloca(sizeof(*arr) * (realsize))
#endif

#endif /* __BLI_ALLOCA_H__ */
