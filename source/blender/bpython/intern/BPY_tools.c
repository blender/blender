/* python API tool subroutines 
 * $Id$
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "Python.h"
#include "BKE_global.h"
#include "BPY_tools.h"

/* These should all have BPY prefixes later on */

PyObject *BPY_incr_ret(PyObject *ob) {
	Py_INCREF(ob);
	return ob;
}

PyObject *BPY_err_ret_ob(PyObject *type, char *err) {
	PyErr_SetString(type, err);
	return NULL;
}

int py_err_ret_int(PyObject *type, char *err) {
        PyErr_SetString(type, err);
        return -1;
}

int BPY_check_sequence_consistency(PyObject *seq, PyTypeObject *against)
{
	PyObject *ob;
	int len= PySequence_Length(seq);
	int i;
	
	for (i=0; i<len; i++) {
			ob= PySequence_GetItem(seq, i);
			if (ob->ob_type != against) {
					Py_DECREF(ob);
					return 0;
			}
			Py_DECREF(ob);
	}
	return 1;
}

int BPY_parsefloatvector(PyObject *vec, float *ptr, int n)
{
	int i;
	PyObject *o, *p;
	char errstring[80];

	if (PyArg_ParseTuple(vec, "ff|f", &ptr[0], &ptr[1], &ptr[2]))
		return 0;
	if (PyArg_Parse(vec, "O", &p)) {
		if (PySequence_Check(p) && PySequence_Length(p) == n ) {
			for (i = 0; i < n; i++) {
				o = PySequence_GetItem(vec, i);
				if (PyFloat_Check(o)) {
					ptr[i] = PyFloat_AsDouble(o);	
					Py_DECREF(o);		
				} else {	
					Py_DECREF(o);		
					return py_err_ret_int(PyExc_AttributeError, "vector assignment wants floats");
				}	
			}	
			return 0;
		}	
	}

	if (!PySequence_Check(p)) {
		printf("invalid sequence");
	}

	/* errors: */
	sprintf(errstring, "Float vector tuple of length %d expected", n);
	return py_err_ret_int(PyExc_AttributeError, errstring);
}




