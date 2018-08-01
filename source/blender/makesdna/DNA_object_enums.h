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

/** \file DNA_object_enums.h
 *  \ingroup DNA
 *
 * Enums typedef's for use in public headers.
 */

#ifndef __DNA_OBJECT_ENUMS_H__
#define __DNA_OBJECT_ENUMS_H__

/* Object.mode */
typedef enum eObjectMode {
	OB_MODE_OBJECT        = 0,
	OB_MODE_EDIT          = 1 << 0,
	OB_MODE_SCULPT        = 1 << 1,
	OB_MODE_VERTEX_PAINT  = 1 << 2,
	OB_MODE_WEIGHT_PAINT  = 1 << 3,
	OB_MODE_TEXTURE_PAINT = 1 << 4,
	OB_MODE_PARTICLE_EDIT = 1 << 5,
	OB_MODE_POSE          = 1 << 6,
	OB_MODE_GPENCIL_EDIT  = 1 << 7,
	OB_MODE_GPENCIL_PAINT = 1 << 8,
	OB_MODE_GPENCIL_SCULPT = 1 << 9,
	OB_MODE_GPENCIL_WEIGHT = 1 << 10,
} eObjectMode;

/* Any mode where the brush system is used. */
#define OB_MODE_ALL_PAINT (OB_MODE_SCULPT | OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT | OB_MODE_TEXTURE_PAINT)

/* Any mode that uses Object.sculpt. */
#define OB_MODE_ALL_SCULPT (OB_MODE_SCULPT | OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT)

/* Any mode that has data we need to free when switching modes, see: #ED_object_mode_generic_exit */
#define OB_MODE_ALL_MODE_DATA \
	(OB_MODE_EDIT | OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT | OB_MODE_SCULPT | OB_MODE_POSE)

#endif  /* __DNA_OBJECT_ENUMS_H__ */
