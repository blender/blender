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

/** \file source/blender/freestyle/intern/python/UnaryFunction0D/UnaryFunction0D_Vec2f/BPy_Normal2DF0D.h
 *  \ingroup freestyle
 */

#ifndef __FREESTYLE_PYTHON_NORMAL2DF0D_H__
#define __FREESTYLE_PYTHON_NORMAL2DF0D_H__

#include "../BPy_UnaryFunction0DVec2f.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject Normal2DF0D_Type;

#define BPy_Normal2DF0D_Check(v) (PyObject_IsInstance((PyObject *)v, (PyObject *)&Normal2DF0D_Type))

/*---------------------------Python BPy_Normal2DF0D structure definition----------*/
typedef struct {
	BPy_UnaryFunction0DVec2f py_uf0D_vec2f;
} BPy_Normal2DF0D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* __FREESTYLE_PYTHON_NORMAL2DF0D_H__ */
