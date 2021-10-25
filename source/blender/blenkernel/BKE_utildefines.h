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

/** \file BKE_utildefines.h
 *  \ingroup bke
 *  \brief blender format specific macros
 *  \note generic defines should go in BLI_utildefines.h
 */


#ifndef __BKE_UTILDEFINES_H__
#define __BKE_UTILDEFINES_H__

#ifdef __cplusplus
extern "C" {
#endif

#define BKE_BIT_TEST_SET(value, test, flag) \
{                                           \
	if (test) (value) |=  flag;             \
	else      (value) &= ~flag;             \
} (void)0

#ifdef __cplusplus
}
#endif

#endif  /* __BKE_UTILDEFINES_H__ */
