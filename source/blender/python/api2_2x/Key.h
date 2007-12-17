/*
 * $Id: Key.h 10783 2007-05-26 12:58:46Z campbellbarton $
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
 * Contributor(s): Pontus Lidman
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef EXPP_KEY_H
#define EXPP_KEY_H

#include "Python.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <DNA_key_types.h>
#include <DNA_curve_types.h>

extern PyTypeObject Key_Type;
extern PyTypeObject KeyBlock_Type;

typedef struct {
	PyObject_HEAD		/* required python macro   */
	Key * key;			/* libdata must be second */
	/* Object *object;*/		/* for vertex grouping info, since it's stored on the object */
	/*PyObject *keyBlock;*/
	/*PyObject *ipo;*/
} BPy_Key;

typedef struct {
	PyObject_HEAD		/* required python macro   */
	Key *key;
	KeyBlock * keyblock;
	/* Object *object;*/		/* for vertex grouping info, since it's stored on the object */
} BPy_KeyBlock;

PyObject *Key_CreatePyObject( Key * k );
PyObject *KeyBlock_CreatePyObject( KeyBlock * k, Key *parentKey );

PyObject *Key_Init( void );

#endif /* EXPP_KEY_H */
