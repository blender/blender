/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pymathutils
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
PyDoc_STRVAR(M_Interpolate_doc, "The Blender interpolate module");

/* ---------------------------------WEIGHT CALCULATION ----------------------- */

#ifndef MATH_STANDALONE

PyDoc_STRVAR(M_Interpolate_poly_3d_calc_doc,
             ".. function:: poly_3d_calc(veclist, pt)\n"
             "\n"
             "   Calculate barycentric weights for a point on a polygon.\n"
             "\n"
             "   :arg veclist: list of vectors\n"
             "   :arg pt: point"
             "   :rtype: list of per-vector weights\n");
static PyObject *M_Interpolate_poly_3d_calc(PyObject *UNUSED(self), PyObject *args)
{
  float fp[3];
  float(*vecs)[3];
  Py_ssize_t len;

  PyObject *point, *veclist, *ret;
  int i;

  if (!PyArg_ParseTuple(args, "OO:poly_3d_calc", &veclist, &point)) {
    return NULL;
  }

  if (mathutils_array_parse(
          fp, 2, 3 | MU_ARRAY_ZERO, point, "pt must be a 2-3 dimensional vector") == -1) {
    return NULL;
  }

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
    {"poly_3d_calc",
     (PyCFunction)M_Interpolate_poly_3d_calc,
     METH_VARARGS,
     M_Interpolate_poly_3d_calc_doc},
#endif
    {NULL, NULL, 0, NULL},
};

static struct PyModuleDef M_Interpolate_module_def = {
    PyModuleDef_HEAD_INIT,
    "mathutils.interpolate", /* m_name */
    M_Interpolate_doc,       /* m_doc */
    0,                       /* m_size */
    M_Interpolate_methods,   /* m_methods */
    NULL,                    /* m_slots */
    NULL,                    /* m_traverse */
    NULL,                    /* m_clear */
    NULL,                    /* m_free */
};

/*----------------------------MODULE INIT-------------------------*/
PyMODINIT_FUNC PyInit_mathutils_interpolate(void)
{
  PyObject *submodule = PyModule_Create(&M_Interpolate_module_def);
  return submodule;
}
