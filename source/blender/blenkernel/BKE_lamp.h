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

#ifndef __BKE_LAMP_H__
#define __BKE_LAMP_H__

/** \file BKE_lamp.h
 *  \ingroup bke
 *  \brief General operations, lookup, etc. for blender lamps.
 */
#ifdef __cplusplus
extern "C" {
#endif

struct Lamp;

void *add_lamp(const char *name);
struct Lamp *BKE_lamp_copy(struct Lamp *la);
struct Lamp *localize_lamp(struct Lamp *la);
void make_local_lamp(struct Lamp *la);
void BKE_lamp_free(struct Lamp *la);

#ifdef __cplusplus
}
#endif

#endif
