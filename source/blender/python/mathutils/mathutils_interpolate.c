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

/** \file \ingroup pymathutils
 */


#include <Python.h>

#include "mathutils.h"
#include "mathutils_interpolate.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#ifndef MATH_STANDALONE /* define when building outside blender */
#  include "MEM_guardedalloc.h"
#endif

/*-------------------------DOC STRINGS ---------------------------*/
PyDoc_STRVAR(M_Interpolate_doc,
"The Blender interpolate module"
);

/* ---------------------------------WEIGHT CALCULATION ----------------------- */

#ifndef MATH_STANDALONE

PyDoc_STRVAR(M_Interpolate_poly_3d_calc_doc,
".. function:: poly_3d_calc(veclist, pt)\n"
"\n"
"   Calculate barycentric weights for a point on a polygon.\n"
"\n"
"   :arg veclist: list of vectors\n"
"   :arg pt: point"
"   :rtype: list of per-vector weights\n"
);
static PyObject *M_Interpolate_poly_3d_calc(PyObject *UNUSED(self), PyObject *args)
{
	float fp[3];
	float (*vecs)[3];
	Py_ssize_t len;

	PyObject *point, *veclist, *ret;
	int i;

	if (!PyArg_ParseTuple(
	        args, "OO!:poly_3d_calc",
	        &veclist,
	        &vector_Type, &point))
	{
		return NULL;
	}

	if (BaseMath_ReadCallback((VectorObject *)point) == -1)
		return NULL;

	fp[0] = ((VectorObject *)point)->vec[0];
	fp[1] = ((VectorObject *)point)->vec[1];
	if (((VectorObject *)point)->size > 2)
		fp[2] = ((VectorObject *)point)->vec[2];
	else
		fp[2] = 0.0f;  /* if its a 2d vector then set the z to be zero */

	len = mathutils_array_parse_alloc_v(((float **)&vecs), 3, veclist, __func__);
	if (len == -1) {
		return NULL;
	}

	if (len) {
		float *weights = MEM_mallocN(sizeof(float) * len, __func__);

		interp_weights_poly_v3(weights, vecs, len, fp);

		ret = PyList_New(len);
		for (i = 0; i < len; i++) {
			PyList_SET_ITEM(ret, i, PyFloat_FromDouble(weights[i]));
		}

		MEM_freeN(weights);

		PyMem_Free(vecs);
	}
	else {
		ret = PyList_New(0);
	}

	return ret;
}

#endif /* MATH_STANDALONE */


static PyMethodDef M_Interpolate_methods[] = {
#ifndef MATH_STANDALONE
	{"poly_3d_calc", (PyCFunction) M_Interpolate_poly_3d_calc, METH_VARARGS, M_Interpolate_poly_3d_calc_doc},
#endif
	{NULL, NULL, 0, NULL},
};

static struct PyModuleDef M_Interpolate_module_def = {
	PyModuleDef_HEAD_INIT,
	"mathutils.interpolate",  /* m_name */
	M_Interpolate_doc,  /* m_doc */
	0,  /* m_size */
	M_Interpolate_methods,  /* m_methods */
	NULL,  /* m_reload */
	NULL,  /* m_traverse */
	NULL,  /* m_clear */
	NULL,  /* m_free */
};

/*----------------------------MODULE INIT-------------------------*/
PyMODINIT_FUNC PyInit_mathutils_interpolate(void)
{
	PyObject *submodule = PyModule_Create(&M_Interpolate_module_def);
	return submodule;
}
