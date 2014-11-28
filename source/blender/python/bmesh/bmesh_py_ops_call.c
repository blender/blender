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

/** \file blender/python/bmesh/bmesh_py_ops_call.c
 *  \ingroup pybmesh
 *
 * This file provides __call__ aka BPy_BMO_call for
 * the bmesh operatorand has been given its own file
 * because argument conversion is involved.
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "../mathutils/mathutils.h"

#include "bmesh.h"

#include "bmesh_py_ops_call.h"  /* own include */

#include "bmesh_py_types.h"

static int bpy_bm_op_as_py_error(BMesh *bm)
{
	if (BMO_error_occurred(bm)) {
		/* note: we could have multiple errors */
		const char *errmsg;
		if (BMO_error_get(bm, &errmsg, NULL)) {
			PyErr_Format(PyExc_RuntimeError,
			             "bmesh operator: %.200s",
			             errmsg);
			BMO_error_clear(bm);
			return -1;
		}
	}
	return 0;
}

/**
 * \brief Utility function to check BMVert/BMEdge/BMFace's
 *
 * \param value
 * \param bm Check the \a value against this.
 * \param htype Test \a value matches this type.
 * \param descr Description text.
 */
static int bpy_slot_from_py_elem_check(BPy_BMElem *value, BMesh *bm, const char htype,
                                       /* for error messages */
                                       const char *opname, const char *slot_name, const char *descr)
{
	if (!BPy_BMElem_Check(value) ||
	    !(value->ele->head.htype & htype))
	{
		PyErr_Format(PyExc_TypeError,
		             "%.200s: keyword \"%.200s\" %.200s, expected a %.200s not *.200s",
		             opname, slot_name, descr,
		             BPy_BMElem_StringFromHType(htype),
		             Py_TYPE(value)->tp_name);
		return -1;
	}
	else if (value->bm == NULL) {
		PyErr_Format(PyExc_TypeError,
		             "%.200s: keyword \"%.200s\" %.200s invalidated element",
		             opname, slot_name, descr);
		return -1;
	}
	else if (value->bm != bm) {  /* we may want to make this check optional by setting 'bm' to NULL */
		PyErr_Format(PyExc_TypeError,
		             "%.200s: keyword \"%.200s\" %.200s invalidated element",
		             opname, slot_name, descr);
		return -1;
	}
	return 0;
}

/**
 * \brief Utility function to check BMVertSeq/BMEdgeSeq/BMFaceSeq's
 *
 * \param value Caller must check its a BMeshSeq
 * \param bm Check the \a value against this.
 * \param htype_py The type(s) of \a value.
 * \param htype_bmo The type(s) supported by the target slot.
 * \param descr Description text.
 */
static int bpy_slot_from_py_elemseq_check(BPy_BMGeneric *value, BMesh *bm,
                                          const char htype_py, const char htype_bmo,
                                          /* for error messages */
                                          const char *opname, const char *slot_name, const char *descr)
{
	if (value->bm == NULL) {
		PyErr_Format(PyExc_TypeError,
		             "%.200s: keyword \"%.200s\" %.200s, invalidated sequence",
		             opname, slot_name, descr);
		return -1;
	}
	else if (value->bm != bm) {  /* we may want to make this check optional by setting 'bm' to NULL */
		PyErr_Format(PyExc_TypeError,
		             "%.200s: keyword \"%.200s\" %.200s, invalidated sequence",
		             opname, slot_name, descr);
		return -1;
	}
	else if ((htype_py & htype_bmo) == 0) {
		char str_bmo[32];
		char str_py[32];
		PyErr_Format(PyExc_TypeError,
		             "%.200s: keyword \"%.200s\" %.200s, expected "
		             "a sequence of %.200s not %.200s",
		             opname, slot_name, descr,
		             BPy_BMElem_StringFromHType_ex(htype_bmo, str_bmo),
		             BPy_BMElem_StringFromHType_ex(htype_py,  str_py));
		return -1;
	}

	return 0;
}

/**
 * Use for giving py args to an operator.
 */
static int bpy_slot_from_py(BMesh *bm, BMOperator *bmop, BMOpSlot *slot, PyObject *value,
                            /* the are just for exception messages */
                            const char *opname, const char *slot_name)
{
	switch (slot->slot_type) {
		case BMO_OP_SLOT_BOOL:
		{
			int param;

			param = PyLong_AsLong(value);

			if (param < 0) {
				PyErr_Format(PyExc_TypeError,
				             "%.200s: keyword \"%.200s\" expected True/False or 0/1, not %.200s",
				             opname, slot_name, Py_TYPE(value)->tp_name);
				return -1;
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
				             opname, slot_name, Py_TYPE(value)->tp_name);
				return -1;
			}
			else if (param == -1 && PyErr_Occurred()) {
				PyErr_Format(PyExc_TypeError,
				             "%.200s: keyword \"%.200s\" expected an int, not %.200s",
				             opname, slot_name, Py_TYPE(value)->tp_name);
				return -1;
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
				             opname, slot_name, Py_TYPE(value)->tp_name);
				return -1;
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
				             opname, slot_name, Py_TYPE(value)->tp_name);
				return -1;
			}
			else if (BaseMath_ReadCallback((MatrixObject *)value) == -1) {
				return -1;
			}
			else if (((size = ((MatrixObject *)value)->num_col) != ((MatrixObject *)value)->num_row) ||
			         (ELEM(size, 3, 4) == false))
			{
				PyErr_Format(PyExc_TypeError,
				             "%.200s: keyword \"%.200s\" expected a 3x3 or 4x4 matrix Matrix",
				             opname, slot_name);
				return -1;
			}

			BMO_slot_mat_set(bmop, bmop->slots_in, slot_name, ((MatrixObject *)value)->matrix, size);
			break;
		}
		case BMO_OP_SLOT_VEC:
		{
			/* passing slot name here is a bit non-descriptive */
			if (mathutils_array_parse(BMO_SLOT_AS_VECTOR(slot), 3, 3, value, slot_name) == -1) {
				return -1;
			}
			break;
		}
		case BMO_OP_SLOT_ELEMENT_BUF:
		{
			if (slot->slot_subtype.elem & BMO_OP_SLOT_SUBTYPE_ELEM_IS_SINGLE) {
				if (bpy_slot_from_py_elem_check((BPy_BMElem *)value, bm, (slot->slot_subtype.elem & BM_ALL_NOLOOP),
				                                opname, slot_name, "single element") == -1)
				{
					return -1;  /* error is set in bpy_slot_from_py_elem_check() */
				}

				BMO_slot_buffer_from_single(bmop, slot, &((BPy_BMElem *)value)->ele->head);
			}
			else {
				/* there are many ways we could interpret arguments, for now...
				 * - verts/edges/faces from the mesh direct,
				 *   this way the operator takes every item.
				 * - `TODO` a plain python sequence (list) of elements.
				 * - `TODO`  an iterator. eg.
				 *   face.verts
				 * - `TODO`  (type, flag) pair, eg.
				 *   ('VERT', {'TAG'})
				 */

				if (BPy_BMVertSeq_Check(value)) {
					if (bpy_slot_from_py_elemseq_check((BPy_BMGeneric *)value, bm,
					                                   BM_VERT, (slot->slot_subtype.elem & BM_ALL_NOLOOP),
					                                   opname, slot_name, "element buffer") == -1)
					{
						return -1;  /* error is set in bpy_slot_from_py_elem_check() */
					}

					BMO_slot_buffer_from_all(bm, bmop, bmop->slots_in, slot_name, BM_VERT);
				}
				else if (BPy_BMEdgeSeq_Check(value)) {
					if (bpy_slot_from_py_elemseq_check((BPy_BMGeneric *)value, bm,
					                                   BM_EDGE, (slot->slot_subtype.elem & BM_ALL_NOLOOP),
					                                   opname, slot_name, "element buffer") == -1)
					{
						return -1;  /* error is set in bpy_slot_from_py_elem_check() */
					}

					BMO_slot_buffer_from_all(bm, bmop, bmop->slots_in, slot_name, BM_EDGE);
				}
				else if (BPy_BMFaceSeq_Check(value)) {
					if (bpy_slot_from_py_elemseq_check((BPy_BMGeneric *)value, bm,
					                                   BM_FACE, (slot->slot_subtype.elem & BM_ALL_NOLOOP),
					                                   opname, slot_name, "element buffer") == -1)
					{
						return -1;  /* error is set in bpy_slot_from_py_elem_check() */
					}
					BMO_slot_buffer_from_all(bm, bmop, bmop->slots_in, slot_name, BM_FACE);
				}

				else if (BPy_BMElemSeq_Check(value)) {
					BMIter iter;
					BMHeader *ele;
					int tot;
					unsigned int i;

					if (bpy_slot_from_py_elemseq_check((BPy_BMGeneric *)value, bm,
					                                   bm_iter_itype_htype_map[((BPy_BMElemSeq *)value)->itype],
					                                   (slot->slot_subtype.elem & BM_ALL_NOLOOP),
					                                   opname, slot_name, "element buffer") == -1)
					{
						return -1;  /* error is set in bpy_slot_from_py_elem_check() */
					}

					/* this will loop over all elements which is a shame but
					 * we need to know this before alloc */
					/* calls bpy_bmelemseq_length() */
					tot = Py_TYPE(value)->tp_as_sequence->sq_length(value);

					BMO_slot_buffer_alloc(bmop, bmop->slots_in, slot_name, tot);

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
					                                       true, true, slot_name);

					/* error is set above */
					if (elem_array == NULL) {
						return -1;
					}

					BMO_slot_buffer_alloc(bmop, bmop->slots_in, slot_name, elem_array_len);
					memcpy(slot->data.buf, elem_array, sizeof(void *) * elem_array_len);
					PyMem_FREE(elem_array);
				}
				else {
					PyErr_Format(PyExc_TypeError,
					             "%.200s: keyword \"%.200s\" expected "
					             "a bmesh sequence, list, (htype, flag) pair, not %.200s",
					             opname, slot_name, Py_TYPE(value)->tp_name);
					return -1;
				}
			}
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
					             opname, slot_name, Py_TYPE(value)->tp_name);
					return -1;
				}
			}
			else {
				if (!PySet_Check(value)) {
					PyErr_Format(PyExc_TypeError,
					             "%.200s: keyword \"%.200s\" expected "
					             "a set, not %.200s",
					             opname, slot_name, Py_TYPE(value)->tp_name);
					return -1;
				}
			}

			switch (slot->slot_subtype.map) {
				case BMO_OP_SLOT_SUBTYPE_MAP_ELEM:
				{
					if (PyDict_Size(value) > 0) {
						PyObject *arg_key, *arg_value;
						Py_ssize_t arg_pos = 0;
						while (PyDict_Next(value, &arg_pos, &arg_key, &arg_value)) {
							if (bpy_slot_from_py_elem_check((BPy_BMElem *)arg_key, bm, BM_ALL_NOLOOP,
							                                opname, slot_name, "invalid key in dict") == -1)
							{
								return -1;  /* error is set in bpy_slot_from_py_elem_check() */
							}

							if (bpy_slot_from_py_elem_check((BPy_BMElem *)arg_value, bm, BM_ALL_NOLOOP,
							                                opname, slot_name, "invalid value in dict") == -1)
							{
								return -1;  /* error is set in bpy_slot_from_py_elem_check() */
							}

							BMO_slot_map_elem_insert(bmop, slot,
							                         ((BPy_BMElem *)arg_key)->ele, ((BPy_BMElem *)arg_value)->ele);
						}
					}
					break;
				}
				case BMO_OP_SLOT_SUBTYPE_MAP_FLT:
				{
					if (PyDict_Size(value) > 0) {
						PyObject *arg_key, *arg_value;
						Py_ssize_t arg_pos = 0;
						while (PyDict_Next(value, &arg_pos, &arg_key, &arg_value)) {
							float value_f;

							if (bpy_slot_from_py_elem_check((BPy_BMElem *)arg_key, bm, BM_ALL_NOLOOP,
							                                opname, slot_name, "invalid key in dict") == -1)
							{
								return -1;  /* error is set in bpy_slot_from_py_elem_check() */
							}

							value_f = PyFloat_AsDouble(arg_value);

							if (value_f == -1.0f && PyErr_Occurred()) {
								PyErr_Format(PyExc_TypeError,
								             "%.200s: keyword \"%.200s\" expected "
								             "a dict with float values, not %.200s",
								             opname, slot_name, Py_TYPE(arg_value)->tp_name);
								return -1;
							}

							BMO_slot_map_float_insert(bmop, slot,
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

							if (bpy_slot_from_py_elem_check((BPy_BMElem *)arg_key, bm, BM_ALL_NOLOOP,
							                                opname, slot_name, "invalid key in dict") == -1)
							{
								return -1;  /* error is set in bpy_slot_from_py_elem_check() */
							}

							value_i = PyLong_AsLong(arg_value);

							if (value_i == -1 && PyErr_Occurred()) {
								PyErr_Format(PyExc_TypeError,
								             "%.200s: keyword \"%.200s\" expected "
								             "a dict with int values, not %.200s",
								             opname, slot_name, Py_TYPE(arg_value)->tp_name);
								return -1;
							}

							BMO_slot_map_int_insert(bmop, slot,
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

							if (bpy_slot_from_py_elem_check((BPy_BMElem *)arg_key, bm, BM_ALL_NOLOOP,
							                                opname, slot_name, "invalid key in dict") == -1)
							{
								return -1;  /* error is set in bpy_slot_from_py_elem_check() */
							}

							value_i = PyLong_AsLong(arg_value);

							if (value_i == -1 && PyErr_Occurred()) {
								PyErr_Format(PyExc_TypeError,
								             "%.200s: keyword \"%.200s\" expected "
								             "a dict with bool values, not %.200s",
								             opname, slot_name, Py_TYPE(arg_value)->tp_name);
								return -1;
							}

							BMO_slot_map_bool_insert(bmop, slot,
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

							if (bpy_slot_from_py_elem_check((BPy_BMElem *)arg_key, bm, BM_ALL_NOLOOP,
							                                opname, slot_name, "invalid key in set") == -1)
							{
								return -1;  /* error is set in bpy_slot_from_py_elem_check() */
							}

							BMO_slot_map_empty_insert(bmop, slot,
							                         ((BPy_BMElem *)arg_key)->ele);
						}
					}
					break;
				}
				case BMO_OP_SLOT_SUBTYPE_MAP_INTERNAL:
				{
					/* can't convert from these */
					PyErr_Format(PyExc_NotImplementedError,
					             "This arguments mapping subtype %d is not supported", slot->slot_subtype.map);
					return -1;
				}
			}
			break;
		}
		default:
			/* TODO --- many others */
			PyErr_Format(PyExc_NotImplementedError,
			             "%.200s: keyword \"%.200s\" type %d not working yet!",
			             opname, slot_name, slot->slot_type);
			return -1;
	}

	/* all is well */
	return 0;
}

/**
 * Use for getting return values from an operator thats already executed.
 *
 * \note Don't throw any exceptions and should always return a valid (PyObject *).
 */
static PyObject *bpy_slot_to_py(BMesh *bm, BMOpSlot *slot)
{
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
			if (slot->slot_subtype.elem & BMO_OP_SLOT_SUBTYPE_ELEM_IS_SINGLE) {
				BMHeader *ele = BMO_slot_buffer_get_single(slot);
				item = ele ? BPy_BMElem_CreatePyObject(bm, ele) : (Py_INCREF(Py_None), Py_None);
			}
			else {
				const int size = slot->len;
				void **buffer = BMO_SLOT_AS_BUFFER(slot);
				int j;

				item = PyList_New(size);
				for (j = 0; j < size; j++) {
					BMHeader *ele = buffer[j];
					PyList_SET_ITEM(item, j, BPy_BMElem_CreatePyObject(bm, ele));
				}
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
							BMHeader *ele_key = BLI_ghashIterator_getKey(&hash_iter);
							void     *ele_val = BLI_ghashIterator_getValue(&hash_iter);

							PyObject *py_key =  BPy_BMElem_CreatePyObject(bm, ele_key);
							PyObject *py_val =  BPy_BMElem_CreatePyObject(bm, ele_val);

							PyDict_SetItem(item, py_key, py_val);
							Py_DECREF(py_key);
							Py_DECREF(py_val);
						}
					}
					break;
				}
				case BMO_OP_SLOT_SUBTYPE_MAP_FLT:
				{
					item = PyDict_New();
					if (slot_hash) {
						GHASH_ITER (hash_iter, slot_hash) {
							BMHeader *ele_key = BLI_ghashIterator_getKey(&hash_iter);
							void     *ele_val = BLI_ghashIterator_getValue(&hash_iter);

							PyObject *py_key =  BPy_BMElem_CreatePyObject(bm,  ele_key);
							PyObject *py_val =  PyFloat_FromDouble(*(float *)&ele_val);

							PyDict_SetItem(item, py_key, py_val);
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
							BMHeader *ele_key = BLI_ghashIterator_getKey(&hash_iter);
							void     *ele_val = BLI_ghashIterator_getValue(&hash_iter);

							PyObject *py_key =  BPy_BMElem_CreatePyObject(bm,  ele_key);
							PyObject *py_val =  PyLong_FromLong(*(int *)&ele_val);

							PyDict_SetItem(item, py_key, py_val);
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
							BMHeader *ele_key = BLI_ghashIterator_getKey(&hash_iter);
							void     *ele_val = BLI_ghashIterator_getValue(&hash_iter);

							PyObject *py_key =  BPy_BMElem_CreatePyObject(bm,  ele_key);
							PyObject *py_val =  PyBool_FromLong(*(bool *)&ele_val);

							PyDict_SetItem(item, py_key, py_val);
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

	return item;
}

/**
 * This is the __call__ for bmesh.ops.xxx()
 */
PyObject *BPy_BMO_call(BPy_BMeshOpFunc *self, PyObject *args, PyObject *kw)
{
	PyObject *ret;
	BPy_BMesh *py_bm;
	BMesh *bm;

	BMOperator bmop;

	if ((PyTuple_GET_SIZE(args) == 1) &&
	    (py_bm = (BPy_BMesh *)PyTuple_GET_ITEM(args, 0)) &&
	    (BPy_BMesh_Check(py_bm)))
	{
		BPY_BM_CHECK_OBJ(py_bm);
		bm = py_bm->bm;

		/* could complain about entering with exceptions... */
		BMO_error_clear(bm);
	}
	else {
		PyErr_SetString(PyExc_TypeError,
		                "bmesh operators expect a single BMesh positional argument, all other args must be keywords");
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
			BMOpSlot *slot;

			if (!BMO_slot_exists(bmop.slots_in, slot_name)) {
				PyErr_Format(PyExc_TypeError,
				             "%.200s: keyword \"%.200s\" is invalid for this operator",
				             self->opname, slot_name);
				BMO_op_finish(bm, &bmop);
				return NULL;
			}

			slot = BMO_slot_get(bmop.slots_in, slot_name);

			/* now assign the value */
			if (bpy_slot_from_py(bm, &bmop, slot, value,
			                     self->opname, slot_name) == -1)
			{
				BMO_op_finish(bm, &bmop);
				return NULL;
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
			PyObject *item;

			/* this function doesn't throw exceptions */
			item = bpy_slot_to_py(bm, slot);
			if (item == NULL) {
				item = (Py_INCREF(Py_None), Py_None);
			}

#if 1
			/* temp code, strip off '.out' while we keep this convention */
			{
				char slot_name_strip[MAX_SLOTNAME];
				const char *ch = strchr(slot->slot_name, '.');  /* can't fail! */
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
