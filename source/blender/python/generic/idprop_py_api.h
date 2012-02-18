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
 * Contributor(s): Joseph Eagar, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/generic/idprop_py_api.h
 *  \ingroup pygen
 */


#ifndef __IDPROP_PY_API_H__
#define __IDPROP_PY_API_H__

struct ID;
struct IDProperty;
struct BPy_IDGroup_Iter;

typedef struct BPy_IDProperty {
	PyObject_VAR_HEAD
	struct ID *id;
	struct IDProperty *prop; /* must be second member */
	struct IDProperty *parent;
	PyObject *data_wrap;
} BPy_IDProperty;

typedef struct BPy_IDArray {
	PyObject_VAR_HEAD
	struct ID *id;
	struct IDProperty *prop;  /* must be second member */
} BPy_IDArray;

typedef struct BPy_IDGroup_Iter {
	PyObject_VAR_HEAD
	BPy_IDProperty *group;
	struct IDProperty *cur;
	int mode;
} BPy_IDGroup_Iter;

PyObject *BPy_Wrap_GetKeys(struct IDProperty *prop);
PyObject *BPy_Wrap_GetValues(struct ID *id, struct IDProperty *prop);
PyObject *BPy_Wrap_GetItems(struct ID *id, struct IDProperty *prop);
int BPy_Wrap_SetMapItem(struct IDProperty *prop, PyObject *key, PyObject *val);


PyObject *BPy_IDGroup_WrapData(struct ID *id, struct IDProperty *prop, struct IDProperty *parent);
const char *BPy_IDProperty_Map_ValidateAndCreate(PyObject *key, struct IDProperty *group, PyObject *ob);

void IDProp_Init_Types(void);

#define IDPROP_ITER_KEYS	0
#define IDPROP_ITER_ITEMS	1

#endif /* __IDPROP_PY_API_H__ */
