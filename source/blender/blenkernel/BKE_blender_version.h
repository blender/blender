/*
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
 */
#ifndef __BKE_BLENDER_VERSION_H__
#define __BKE_BLENDER_VERSION_H__

/** \file
 * \ingroup bke
 */

/**
 * The lines below use regex from scripts to extract their values,
 * Keep this in mind when modifying this file and keep this comment above the defines.
 *
 * \note Use #STRINGIFY() rather than defining with quotes.
 */
#define BLENDER_VERSION 280
#define BLENDER_SUBVERSION 73
/** Several breakages with 280, e.g. collections vs layers. */
#define BLENDER_MINVERSION 280
#define BLENDER_MINSUBVERSION 0

/** Used by packaging tools. */
/** Can be left blank, otherwise a,b,c... etc with no quotes. */
#define BLENDER_VERSION_CHAR
/** alpha/beta/rc/release, docs use this. */
#define BLENDER_VERSION_CYCLE beta

/** Defined in from blender.c */
extern char versionstr[];

#endif /* __BKE_BLENDER_VERSION_H__ */
