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

/** \file source/blender/freestyle/intern/python/BPy_MediumType.h
 *  \ingroup freestyle
 */

#ifndef __FREESTYLE_PYTHON_MEDIUMTYPE_H__
#define __FREESTYLE_PYTHON_MEDIUMTYPE_H__

#include <Python.h>

#include "../stroke/Stroke.h"

using namespace Freestyle;

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject MediumType_Type;

#define BPy_MediumType_Check(v) (PyObject_IsInstance((PyObject *)v, (PyObject *)&MediumType_Type))

/*---------------------------Python BPy_MediumType structure definition----------*/
typedef struct {
	PyLongObject i;
} BPy_MediumType;

/*---------------------------Python BPy_MediumType visible prototypes-----------*/

int MediumType_Init(PyObject *module);

// internal constants
extern PyLongObject _BPy_MediumType_DRY_MEDIUM;
extern PyLongObject _BPy_MediumType_HUMID_MEDIUM;
extern PyLongObject _BPy_MediumType_OPAQUE_MEDIUM;
// public constants
#define BPy_MediumType_DRY_MEDIUM     ((PyObject *)&_BPy_MediumType_DRY_MEDIUM)
#define BPy_MediumType_HUMID_MEDIUM   ((PyObject *)&_BPy_MediumType_HUMID_MEDIUM)
#define BPy_MediumType_OPAQUE_MEDIUM  ((PyObject *)&_BPy_MediumType_OPAQUE_MEDIUM)

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* __FREESTYLE_PYTHON_MEDIUMTYPE_H__ */
