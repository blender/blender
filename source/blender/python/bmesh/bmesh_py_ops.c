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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/bmesh/bmesh_py_ops.c
 *  \ingroup pybmesh
 *
 * This file defines the 'bmesh.ops' module.
 * Operators from 'opdefines' are wrapped.
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "../generic/py_capi_utils.h"

#include "../mathutils/mathutils.h"

#include "bmesh.h"

#include "bmesh_py_ops.h"  /* own include */

#include "bmesh_py_types.h"

#include "bmesh_py_utils.h" /* own include */

static int bpy_bm_op_as_py_error(BMesh *bm)
{
	if (BMO_error_occurred(bm)) {
		const char *errmsg;
		if (BMO_error_get(bm, &errmsg, NULL)) {
			PyErr_Format(PyExc_RuntimeError,
			             "bmesh operator: %.200s",
			             errmsg);
			return -1;
		}
	}
	return 0;
}

/* bmesh operator 'bmesh.ops.*' callable types
 * ******************************************* */
PyTypeObject bmesh_op_Type;

typedef struct {
	PyObject_HEAD /* required python macro   */
	const char *opname;
} BPy_BMeshOpFunc;

static PyObject *bpy_bmesh_op_CreatePyObject(const char *opname)
{
	BPy_BMeshOpFunc *self = PyObject_New(BPy_BMeshOpFunc, &bmesh_op_Type);

	self->opname = opname;

	return (PyObject *)self;
}

static PyObject *bpy_bmesh_op_repr(BPy_BMeshOpFunc *self)
{
	return PyUnicode_FromFormat("<%.200s bmesh.ops.%.200s()>",
	                            Py_TYPE(self)->tp_name,
	                            self->opname);
}


static PyObject *pyrna_op_call(BPy_BMeshOpFunc *self, PyObject *args, PyObject *kw)
{
	PyObject *ret;
	BPy_BMesh *py_bm;
	BMesh *bm;

	BMOperator bmop;

	if ((PyTuple_GET_SIZE(args) == 1) &&
	    (py_bm = (BPy_BMesh *)PyTuple_GET_ITEM(args, 0)) &&
	    (BPy_BMesh_Check(py_bm))
		)
	{
		BPY_BM_CHECK_OBJ(py_bm);
		bm = py_bm->bm;
	}
	else {
		PyErr_SetString(PyExc_TypeError,
		                "calling a bmesh operator expects a single BMesh (non keyword) "
		                "as the first argument");
		return NULL;
	}

	/* TODO - error check this!, though we do the error check on attribute access */
	/* TODO - make flags optional */
	BMO_op_init(bm, &bmop, BMO_FLAG_DEFAULTS, self->opname);

	if (kw && PyDict_Size(kw) > 0) {
		/* setup properties, see bpy_rna.c: pyrna_py_to_prop()
		 * which shares this logic for parsing properties */

		PyObject *key, *value;
		Py_ssize_t pos = 0;
		while (PyDict_Next(kw, &pos, &key, &value)) {
			const char *slot_name = _PyUnicode_AsString(key);
			BMOpSlot *slot = BMO_slot_get(bmop.slots_in, slot_name);

			if (slot == NULL) {
				PyErr_Format(PyExc_TypeError,
				             "%.200s: keyword \"%.200s\" is invalid for this operator",
				             self->opname, slot_name);
				return NULL;
			}

			/* now assign the value */
			switch (slot->slot_type) {
				case BMO_OP_SLOT_BOOL:
				{
					int param;

					param = PyLong_AsLong(value);

					if (param < 0) {
						PyErr_Format(PyExc_TypeError,
						             "%.200s: keyword \"%.200s\" expected True/False or 0/1, not %.200s",
						             self->opname, slot_name, Py_TYPE(value)->tp_name);
						return NULL;
					}
					else {
						BMO_SLOT_AS_BOOL(slot) = param;
					}

					break;
				}
				case BMO_OP_SLOT_INT:
				{
					int overflow;
					long param = PyLong_AsLongAndOverflow(value, &overflow);
					if (overflow || (param > INT_MAX) || (param < INT_MIN)) {
						PyErr_Format(PyExc_ValueError,
						             "%.200s: keyword \"%.200s\" value not in 'int' range "
						             "(" STRINGIFY(INT_MIN) ", " STRINGIFY(INT_MAX) ")",
						             self->opname, slot_name, Py_TYPE(value)->tp_name);
						return NULL;
					}
					else if (param == -1 && PyErr_Occurred()) {
						PyErr_Format(PyExc_TypeError,
						             "%.200s: keyword \"%.200s\" expected an int, not %.200s",
						             self->opname, slot_name, Py_TYPE(value)->tp_name);
						return NULL;
					}
					else {
						BMO_SLOT_AS_INT(slot) = (int)param;
					}
					break;
				}
				case BMO_OP_SLOT_FLT:
				{
					float param = PyFloat_AsDouble(value);
					if (param == -1 && PyErr_Occurred()) {
						PyErr_Format(PyExc_TypeError,
						             "%.200s: keyword \"%.200s\" expected a float, not %.200s",
						             self->opname, slot_name, Py_TYPE(value)->tp_name);
						return NULL;
					}
					else {
						BMO_SLOT_AS_FLOAT(slot) = param;
					}
					break;
				}
				case BMO_OP_SLOT_MAT:
				{
					/* XXX - BMesh operator design is crappy here, operator slot should define matrix size,
					 * not the caller! */
					unsigned short size;
					if (!MatrixObject_Check(value)) {
						PyErr_Format(PyExc_TypeError,
						             "%.200s: keyword \"%.200s\" expected a Matrix, not %.200s",
						             self->opname, slot_name, Py_TYPE(value)->tp_name);
						return NULL;
					}
					else if (BaseMath_ReadCallback((MatrixObject *)value) == -1) {
						return NULL;
					}
					else if (((size = ((MatrixObject *)value)->num_col) != ((MatrixObject *)value)->num_row) ||
					         (ELEM(size, 3, 4) == FALSE))
					{
						PyErr_Format(PyExc_TypeError,
						             "%.200s: keyword \"%.200s\" expected a 3x3 or 4x4 matrix Matrix",
						             self->opname, slot_name);
						return NULL;
					}

					BMO_slot_mat_set(&bmop, bmop.slots_in, slot_name, ((MatrixObject *)value)->matrix, size);
					break;
				}
				case BMO_OP_SLOT_VEC:
				{
					/* passing slot name here is a bit non-descriptive */
					if (mathutils_array_parse(BMO_SLOT_AS_VECTOR(slot), 3, 3, value, slot_name) == -1) {
						return NULL;
					}
					break;
				}
				case BMO_OP_SLOT_ELEMENT_BUF:
				{
					/* there are many ways we could interpret arguments, for now...
					 * - verts/edges/faces from the mesh direct,
					 *   this way the operator takes every item.
					 * - `TODO` a plain python sequence (list) of elements.
					 * - `TODO`  an iterator. eg.
					 *   face.verts
					 * - `TODO`  (type, flag) pair, eg.
					 *   ('VERT', {'TAG'})
					 */

#define BPY_BM_GENERIC_MESH_TEST(type_string)  \
	if (((BPy_BMGeneric *)value)->bm != bm) {                                             \
	    PyErr_Format(PyExc_NotImplementedError,                                           \
	                 "%.200s: keyword \"%.200s\" " type_string " are from another bmesh", \
	                 self->opname, slot_name, slot->slot_type);                           \
	    return NULL;                                                                      \
	} (void)0

#define BPY_BM_ELEM_TYPE_TEST(type_string)  \
	if ((slot->slot_subtype.elem & BM_VERT) == 0) { \
	    PyErr_Format(PyExc_TypeError, \
	                 "%.200s: keyword \"%.200s\" expected " \
	                 "a list of %.200s not " type_string, \
	                 self->opname, slot_name, \
	                 BPy_BMElem_StringFromHType(slot->slot_subtype.elem & BM_ALL)); \
	    return NULL; \
	} (void)0

					if (BPy_BMVertSeq_Check(value)) {
						BPY_BM_GENERIC_MESH_TEST("verts");
						BPY_BM_ELEM_TYPE_TEST("verts");

						BMO_slot_buffer_from_all(bm, &bmop, bmop.slots_in, slot_name, BM_VERT);
					}
					else if (BPy_BMEdgeSeq_Check(value)) {
						BPY_BM_GENERIC_MESH_TEST("edges");
						BPY_BM_ELEM_TYPE_TEST("edges");
						BMO_slot_buffer_from_all(bm, &bmop, bmop.slots_in, slot_name, BM_EDGE);
					}
					else if (BPy_BMFaceSeq_Check(value)) {
						BPY_BM_GENERIC_MESH_TEST("faces");
						BPY_BM_ELEM_TYPE_TEST("faces");
						BMO_slot_buffer_from_all(bm, &bmop, bmop.slots_in, slot_name, BM_FACE);
					}

#undef BPY_BM_ELEM_TYPE_TEST

					else if (BPy_BMElemSeq_Check(value)) {
						BMIter iter;
						BMHeader *ele;
						int tot;
						unsigned int i;

						BPY_BM_GENERIC_MESH_TEST("elements");

						/* this will loop over all elements which is a shame but
						 * we need to know this before alloc */
						/* calls bpy_bmelemseq_length() */
						tot = Py_TYPE(value)->tp_as_sequence->sq_length((PyObject *)self);

						BMO_slot_buffer_alloc(&bmop, bmop.slots_in, slot_name, tot);

						i = 0;
						BM_ITER_BPY_BM_SEQ (ele, &iter, ((BPy_BMElemSeq *)value)) {
							slot->data.buf[i] = ele;
							i++;
						}
					}
					/* keep this last */
					else if (PySequence_Check(value)) {
						BMElem **elem_array = NULL;
						Py_ssize_t elem_array_len;

						elem_array = BPy_BMElem_PySeq_As_Array(&bm, value, 0, PY_SSIZE_T_MAX,
						                                       &elem_array_len, (slot->slot_subtype.elem & BM_ALL_NOLOOP),
						                                       TRUE, TRUE, slot_name);

						/* error is set above */
						if (elem_array == NULL) {
							return NULL;
						}

						BMO_slot_buffer_alloc(&bmop, bmop.slots_in, slot_name, elem_array_len);
						memcpy(slot->data.buf, elem_array, sizeof(void *) * elem_array_len);
						PyMem_FREE(elem_array);
					}
					else {
						PyErr_Format(PyExc_TypeError,
						             "%.200s: keyword \"%.200s\" expected "
						             "a bmesh sequence, list, (htype, flag) pair, not %.200s",
						             self->opname, slot_name, Py_TYPE(value)->tp_name);
						return NULL;
					}

#undef BPY_BM_GENERIC_MESH_TEST

					break;
				}
				case BMO_OP_SLOT_MAPPING:
				{
					/* first check types */
					if (slot->slot_subtype.map != BMO_OP_SLOT_SUBTYPE_MAP_EMPTY) {
						if (!PyDict_Check(value)) {
							PyErr_Format(PyExc_TypeError,
							             "%.200s: keyword \"%.200s\" expected "
							             "a dict, not %.200s",
							             self->opname, slot_name, Py_TYPE(value)->tp_name);
							return NULL;
						}
					}
					else {
						if (!PySet_Check(value)) {
							PyErr_Format(PyExc_TypeError,
							             "%.200s: keyword \"%.200s\" expected "
							             "a set, not %.200s",
							             self->opname, slot_name, Py_TYPE(value)->tp_name);
							return NULL;
						}
					}

					switch (slot->slot_subtype.map) {

						/* this could be a static function */
#define BPY_BM_MAPPING_KEY_CHECK(arg_key)  \
						if (!BPy_BMElem_Check(arg_key)) { \
							PyErr_Format(PyExc_TypeError, \
							             "%.200s: keyword \"%.200s\" expected " \
							             "a dict with bmesh element keys, not %.200s", \
							             self->opname, slot_name, Py_TYPE(arg_key)->tp_name); \
							return NULL; \
						} \
						else if (((BPy_BMGeneric *)arg_key)->bm == NULL) { \
							PyErr_Format(PyExc_TypeError, \
							             "%.200s: keyword \"%.200s\" invalidated element key in dict", \
							             self->opname, slot_name); \
							return NULL; \
						} (void)0


						case BMO_OP_SLOT_SUBTYPE_MAP_ELEM:
						{
							if (PyDict_Size(value) > 0) {
								PyObject *arg_key, *arg_value;
								Py_ssize_t arg_pos = 0;
								while (PyDict_Next(value, &arg_pos, &arg_key, &arg_value)) {
									/* TODO, check the elements come from the right mesh? */
									BPY_BM_MAPPING_KEY_CHECK(arg_key);

									if (!BPy_BMElem_Check(arg_value)) {
										PyErr_Format(PyExc_TypeError,
										             "%.200s: keyword \"%.200s\" expected "
										             "a dict with bmesh element values, not %.200s",
										             self->opname, slot_name, Py_TYPE(arg_value)->tp_name);
										return NULL;
									}
									else if (((BPy_BMGeneric *)arg_value)->bm == NULL) {
										PyErr_Format(PyExc_TypeError,
										             "%.200s: keyword \"%.200s\" invalidated element value in dict",
										             self->opname, slot_name);
										return NULL;
									}

									BMO_slot_map_elem_insert(&bmop, slot,
									                         ((BPy_BMElem *)arg_key)->ele, ((BPy_BMElem *)arg_value)->ele);
								}
							}
							break;
						}
						case BMO_OP_SLOT_SUBTYPE_MAP_FLOAT:
						{
							if (PyDict_Size(value) > 0) {
								PyObject *arg_key, *arg_value;
								Py_ssize_t arg_pos = 0;
								while (PyDict_Next(value, &arg_pos, &arg_key, &arg_value)) {
									float value_f;
									/* TODO, check the elements come from the right mesh? */
									BPY_BM_MAPPING_KEY_CHECK(arg_key);
									value_f = PyFloat_AsDouble(arg_value);

									if (value_f == -1.0f && PyErr_Occurred()) {
										PyErr_Format(PyExc_TypeError,
										             "%.200s: keyword \"%.200s\" expected "
										             "a dict with float values, not %.200s",
										             self->opname, slot_name, Py_TYPE(arg_value)->tp_name);
										return NULL;
									}

									BMO_slot_map_float_insert(&bmop, slot,
									                          ((BPy_BMElem *)arg_key)->ele, value_f);
								}
							}
							break;
						}
						case BMO_OP_SLOT_SUBTYPE_MAP_INT:
						{
							if (PyDict_Size(value) > 0) {
								PyObject *arg_key, *arg_value;
								Py_ssize_t arg_pos = 0;
								while (PyDict_Next(value, &arg_pos, &arg_key, &arg_value)) {
									int value_i;
									/* TODO, check the elements come from the right mesh? */
									BPY_BM_MAPPING_KEY_CHECK(arg_key);
									value_i = PyLong_AsLong(arg_value);

									if (value_i == -1 && PyErr_Occurred()) {
										PyErr_Format(PyExc_TypeError,
										             "%.200s: keyword \"%.200s\" expected "
										             "a dict with int values, not %.200s",
										             self->opname, slot_name, Py_TYPE(arg_value)->tp_name);
										return NULL;
									}

									BMO_slot_map_int_insert(&bmop, slot,
									                        ((BPy_BMElem *)arg_key)->ele, value_i);
								}
							}
							break;
						}
						case BMO_OP_SLOT_SUBTYPE_MAP_BOOL:
						{
							if (PyDict_Size(value) > 0) {
								PyObject *arg_key, *arg_value;
								Py_ssize_t arg_pos = 0;
								while (PyDict_Next(value, &arg_pos, &arg_key, &arg_value)) {
									int value_i;
									/* TODO, check the elements come from the right mesh? */
									BPY_BM_MAPPING_KEY_CHECK(arg_key);
									value_i = PyLong_AsLong(arg_value);

									if (value_i == -1 && PyErr_Occurred()) {
										PyErr_Format(PyExc_TypeError,
										             "%.200s: keyword \"%.200s\" expected "
										             "a dict with bool values, not %.200s",
										             self->opname, slot_name, Py_TYPE(arg_value)->tp_name);
										return NULL;
									}

									BMO_slot_map_bool_insert(&bmop, slot,
									                         ((BPy_BMElem *)arg_key)->ele, value_i != 0);
								}
							}
							break;
						}
						case BMO_OP_SLOT_SUBTYPE_MAP_EMPTY:
						{
							if (PySet_Size(value) > 0) {
								PyObject *arg_key;
								Py_ssize_t arg_pos = 0;
								Py_ssize_t arg_hash = 0;
								while (_PySet_NextEntry(value, &arg_pos, &arg_key, &arg_hash)) {
									/* TODO, check the elements come from the right mesh? */
									BPY_BM_MAPPING_KEY_CHECK(arg_key);

									BMO_slot_map_empty_insert(&bmop, slot,
									                         ((BPy_BMElem *)arg_key)->ele);
								}
							}
							break;
						}
						case BMO_OP_SLOT_SUBTYPE_MAP_INTERNAL:
						{
							/* can't convert from these */
							PyErr_Format(PyExc_NotImplementedError,
							             "This arguments mapping subtype %d is not supported", slot->slot_subtype);
							break;
						}
#undef BPY_BM_MAPPING_KEY_CHECK

					}
				}
				default:
					/* TODO --- many others */
					PyErr_Format(PyExc_NotImplementedError,
					             "%.200s: keyword \"%.200s\" type %d not working yet!",
					             self->opname, slot_name, slot->slot_type);
					return NULL;
					break;
			}
		}
	}

	BMO_op_exec(bm, &bmop);

	/* from here until the end of the function, no returns, just set 'ret' */
	if (UNLIKELY(bpy_bm_op_as_py_error(bm) == -1)) {
		ret = NULL;  /* exception raised above */
	}
	else if (bmop.slots_out[0].slot_name == NULL) {
		ret = (Py_INCREF(Py_None), Py_None);
	}
	else {
		/* build return value */
		int i;
		ret = PyDict_New();

		for (i = 0; bmop.slots_out[i].slot_name; i++) {
			// BMOpDefine *op_def = opdefines[bmop.type];
			// BMOSlotType *slot_type = op_def->slot_types_out[i];
			BMOpSlot *slot = &bmop.slots_out[i];
			PyObject *item = NULL;

			/* keep switch in same order as above */
			switch (slot->slot_type) {
				case BMO_OP_SLOT_BOOL:
					item = PyBool_FromLong((BMO_SLOT_AS_BOOL(slot)));
					break;
				case BMO_OP_SLOT_INT:
					item = PyLong_FromLong(BMO_SLOT_AS_INT(slot));
					break;
				case BMO_OP_SLOT_FLT:
					item = PyFloat_FromDouble((double)BMO_SLOT_AS_FLOAT(slot));
					break;
				case BMO_OP_SLOT_MAT:
					item = Matrix_CreatePyObject((float *)BMO_SLOT_AS_MATRIX(slot), 4, 4, Py_NEW, NULL);
					break;
				case BMO_OP_SLOT_VEC:
					item = Vector_CreatePyObject(BMO_SLOT_AS_VECTOR(slot), slot->len, Py_NEW, NULL);
					break;
				case BMO_OP_SLOT_PTR:
					BLI_assert(0);  /* currently we don't have any pointer return values in use */
					item = (Py_INCREF(Py_None), Py_None);
					break;
				case BMO_OP_SLOT_ELEMENT_BUF:
				{
					const int size = slot->len;
					void **buffer = BMO_SLOT_AS_BUFFER(slot);
					int j;

					item = PyList_New(size);
					for (j = 0; j < size; j++) {
						BMHeader *ele = buffer[i];
						PyList_SET_ITEM(item, j, ele ? BPy_BMElem_CreatePyObject(bm, ele) : (Py_INCREF(Py_None), Py_None));
					}
					break;
				}
				case BMO_OP_SLOT_MAPPING:
				{
					GHash *slot_hash = BMO_SLOT_AS_GHASH(slot);
					GHashIterator hash_iter;

					switch (slot->slot_subtype.map) {
						case BMO_OP_SLOT_SUBTYPE_MAP_ELEM:
						{
							item = PyDict_New();
							if (slot_hash) {
								GHASH_ITER (hash_iter, slot_hash) {
									BMHeader       *ele_key = BLI_ghashIterator_getKey(&hash_iter);
									BMOElemMapping *ele_val = BLI_ghashIterator_getValue(&hash_iter);

									PyObject *py_key =  BPy_BMElem_CreatePyObject(bm,  ele_key);
									PyObject *py_val =  BPy_BMElem_CreatePyObject(bm, *(void **)BMO_OP_SLOT_MAPPING_DATA(ele_val));

									BLI_assert(slot->slot_subtype.elem & ((BPy_BMElem *)py_val)->ele->head.htype);

									PyDict_SetItem(ret, py_key, py_val);
									Py_DECREF(py_key);
									Py_DECREF(py_val);
								}
							}
							break;
						}
						case BMO_OP_SLOT_SUBTYPE_MAP_FLOAT:
						{
							item = PyDict_New();
							if (slot_hash) {
								GHASH_ITER (hash_iter, slot_hash) {
									BMHeader       *ele_key = BLI_ghashIterator_getKey(&hash_iter);
									BMOElemMapping *ele_val = BLI_ghashIterator_getValue(&hash_iter);

									PyObject *py_key =  BPy_BMElem_CreatePyObject(bm,  ele_key);
									PyObject *py_val =  PyFloat_FromDouble(*(float *)BMO_OP_SLOT_MAPPING_DATA(ele_val));

									PyDict_SetItem(ret, py_key, py_val);
									Py_DECREF(py_key);
									Py_DECREF(py_val);
								}
							}
							break;
						}
						case BMO_OP_SLOT_SUBTYPE_MAP_INT:
						{
							item = PyDict_New();
							if (slot_hash) {
								GHASH_ITER (hash_iter, slot_hash) {
									BMHeader       *ele_key = BLI_ghashIterator_getKey(&hash_iter);
									BMOElemMapping *ele_val = BLI_ghashIterator_getValue(&hash_iter);

									PyObject *py_key =  BPy_BMElem_CreatePyObject(bm,  ele_key);
									PyObject *py_val =  PyLong_FromLong(*(int *)BMO_OP_SLOT_MAPPING_DATA(ele_val));

									PyDict_SetItem(ret, py_key, py_val);
									Py_DECREF(py_key);
									Py_DECREF(py_val);
								}
							}
							break;
						}
						case BMO_OP_SLOT_SUBTYPE_MAP_BOOL:
						{
							item = PyDict_New();
							if (slot_hash) {
								GHASH_ITER (hash_iter, slot_hash) {
									BMHeader       *ele_key = BLI_ghashIterator_getKey(&hash_iter);
									BMOElemMapping *ele_val = BLI_ghashIterator_getValue(&hash_iter);

									PyObject *py_key =  BPy_BMElem_CreatePyObject(bm,  ele_key);
									PyObject *py_val =  PyBool_FromLong(*(int *)BMO_OP_SLOT_MAPPING_DATA(ele_val));

									PyDict_SetItem(ret, py_key, py_val);
									Py_DECREF(py_key);
									Py_DECREF(py_val);
								}
							}
							break;
						}
						case BMO_OP_SLOT_SUBTYPE_MAP_EMPTY:
						{
							item = PySet_New(NULL);
							if (slot_hash) {
								GHASH_ITER (hash_iter, slot_hash) {
									BMHeader       *ele_key = BLI_ghashIterator_getKey(&hash_iter);

									PyObject *py_key =  BPy_BMElem_CreatePyObject(bm,  ele_key);

									PySet_Add(item, py_key);

									Py_DECREF(py_key);
								}
							}
							break;
						}
						case BMO_OP_SLOT_SUBTYPE_MAP_INTERNAL:
							/* can't convert from these */
							item = (Py_INCREF(Py_None), Py_None);
							break;
					}
					break;
				}
			}
			BLI_assert(item != NULL);
			if (item == NULL) {
				item = (Py_INCREF(Py_None), Py_None);
			}

#if 1
			/* temp code, strip off '.out' while we keep this convention */
			{
				char slot_name_strip[MAX_SLOTNAME];
				char *ch = strchr(slot->slot_name, '.');  /* can't fail! */
				int tot = ch - slot->slot_name;
				BLI_assert(ch != NULL);
				memcpy(slot_name_strip, slot->slot_name, tot);
				slot_name_strip[tot] = '\0';
				PyDict_SetItemString(ret, slot_name_strip, item);
			}
#else
			PyDict_SetItemString(ret, slot->slot_name, item);
#endif
			Py_DECREF(item);
		}
	}

	BMO_op_finish(bm, &bmop);
	return ret;
}


PyTypeObject bmesh_op_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"BMeshOpFunc",              /* tp_name */
	sizeof(BPy_BMeshOpFunc),    /* tp_basicsize */
	0,                          /* tp_itemsize */
	/* methods */
	NULL,                       /* tp_dealloc */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* tp_compare */ /* DEPRECATED in python 3.0! */
	(reprfunc) bpy_bmesh_op_repr, /* tp_repr */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	(ternaryfunc)pyrna_op_call, /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */

	/* will only use these if this is a subtype of a py class */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

	/*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
	/*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

	/***  Assigned meaning in release 2.1 ***/
	/*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

	/***  weak reference enabler ***/
	0,
	/*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

	/*** Attribute descriptor and subclassing stuff ***/
	NULL,                       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NULL,                       /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};


/* bmesh fake module 'bmesh.ops'
 * ***************************** */

static PyObject *bpy_bmesh_ops_fakemod_getattro(PyObject *UNUSED(self), PyObject *pyname)
{
	const unsigned int tot = bmo_opdefines_total;
	unsigned int i;
	const char *opname = _PyUnicode_AsString(pyname);

	for (i = 0; i < tot; i++) {
		if (strcmp(bmo_opdefines[i]->opname, opname) == 0) {
			return bpy_bmesh_op_CreatePyObject(opname);
		}
	}

	PyErr_Format(PyExc_AttributeError,
	             "BMeshOpsModule: operator \"%.200s\" doesn't exist",
	             opname);
	return NULL;
}

static PyObject *bpy_bmesh_ops_fakemod_dir(PyObject *UNUSED(self))
{
	const unsigned int tot = bmo_opdefines_total;
	unsigned int i;
	PyObject *ret;

	ret = PyList_New(bmo_opdefines_total);

	for (i = 0; i < tot; i++) {
		PyList_SET_ITEM(ret, i, PyUnicode_FromString(bmo_opdefines[i]->opname));
	}

	return ret;
}

static struct PyMethodDef bpy_bmesh_ops_fakemod_methods[] = {
	{"__dir__", (PyCFunction)bpy_bmesh_ops_fakemod_dir, METH_NOARGS, NULL},
	{NULL, NULL, 0, NULL}
};

static PyTypeObject bmesh_ops_fakemod_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"BMeshOpsModule",           /* tp_name */
	0,                          /* tp_basicsize */
	0,                          /* tp_itemsize */
	/* methods */
	NULL,                       /* tp_dealloc */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* tp_compare */ /* DEPRECATED in python 3.0! */
	NULL,                       /* tp_repr */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */

	/* will only use these if this is a subtype of a py class */
	bpy_bmesh_ops_fakemod_getattro,    /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

	/*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
	/*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

	/***  Assigned meaning in release 2.1 ***/
	/*** rich comparisons ***/
	NULL, /* subclassed */		/* richcmpfunc tp_richcompare; */

	/***  weak reference enabler ***/
	0,
	/*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

	/*** Attribute descriptor and subclassing stuff ***/
	bpy_bmesh_ops_fakemod_methods,  /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NULL,                       /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};

PyObject *BPyInit_bmesh_ops(void)
{
	PyObject *submodule;

	if (PyType_Ready(&bmesh_ops_fakemod_Type) < 0)
		return NULL;

	if (PyType_Ready(&bmesh_op_Type) < 0)
		return NULL;

	submodule = PyObject_New(PyObject, &bmesh_ops_fakemod_Type);

	/* prevent further creation of instances */
	bmesh_ops_fakemod_Type.tp_init = NULL;
	bmesh_ops_fakemod_Type.tp_new = NULL;

	return submodule;
}
