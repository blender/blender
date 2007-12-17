/* 
 * $Id: Registry.h 3209 2004-10-07 19:25:40Z stiv $
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * This is a new part of Blender.
 *
 * Contributor(s): Willian P. Germano
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

/* This submodule was introduced as a way to preserve configured data in
 * scripts.  A very simple idea: the script writer saves this data in a dict
 * and registers this dict in the "Registry" dict.  This way we can discard
 * the global interpreter dictionary after a script is executed, since the
 * data meant to be kept was copied to the Registry elsewhere.  The current
 * implementation is naive: scripts can deliberately mess with data saved by
 * other scripts.  This is so new script versions can delete older entries, if
 * they need to.  XXX Or should we block this? */

#ifndef EXPP_REGISTRY_H
#define EXPP_REGISTRY_H

#include <Python.h>

extern PyObject *bpy_registryDict;
PyObject *Registry_Init( void );

#endif				/* EXPP_REGISTRY_H */
