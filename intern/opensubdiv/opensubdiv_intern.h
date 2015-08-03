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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __OPENSUBDIV_INTERN_H__
#define __OPENSUBDIV_INTERN_H__

/* Perform full topology validation when exporting it to OpenSubdiv. */
#ifdef NDEBUG
#  undef OPENSUBDIV_VALIDATE_TOPOLOGY
#else
/* TODO(sergey): Always disabled for now, the check doesn't handle
 * multiple non-manifolds from the OpenSubdiv side currently.
 */
#  undef OPENSUBDIV_VALIDATE_TOPOLOGY
#endif

/* Currently OpenSubdiv expects topology to be oriented,
 * but sometimes it's handy to disable orientation code
 * to check whether it causes some weird issues by using
 * pre-oriented model.
 */
#define OPENSUBDIV_ORIENT_TOPOLOGY

#endif  /* __OPENSUBDIV_INTERN_H__ */
