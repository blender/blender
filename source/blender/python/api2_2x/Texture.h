/* 
 * $Id: Texture.h 10269 2007-03-15 01:09:14Z campbellbarton $
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
 * Contributor(s): Alex Mole
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

/* based on Image.h */

#ifndef EXPP_TEXTURE_H
#define EXPP_TEXTURE_H

#include <Python.h>
#include "DNA_texture_types.h"

/*****************************************************************************/
/* Python BPy_Texture structure definition                                   */
/*****************************************************************************/

typedef struct {
	PyObject_HEAD 
	Tex * texture; /* libdata must be second */
} BPy_Texture;

extern PyTypeObject Texture_Type;

#define BPy_Texture_Check(v)  ((v)->ob_type == &Texture_Type)


/*****************************************************************************/
/* Module Blender.Texture - public functions                                 */
/*****************************************************************************/

PyObject *Texture_Init( void );
PyObject *Texture_CreatePyObject( struct Tex *tex );
Tex *Texture_FromPyObject( PyObject * pyobj );


#endif				/* EXPP_TEXTURE_H */
