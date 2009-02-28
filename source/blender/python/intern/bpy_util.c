/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */


#include "bpy_util.h"
#include "BLI_dynstr.h"
#include "MEM_guardedalloc.h"
#include "bpy_compat.h"

PyObject *BPY_flag_to_list(struct BPY_flag_def *flagdef, int flag)
{
	PyObject *list = PyList_New(0);
	
	PyObject *item;
	BPY_flag_def *fd;

	fd= flagdef;
	while(fd->name) {
		if (fd->flag & flag) {
			item = PyUnicode_FromString(fd->name);
			PyList_Append(list, item);
			Py_DECREF(item);
		}
		fd++;
	}
	
	return list;

}

static char *bpy_flag_error_str(BPY_flag_def *flagdef)
{
	BPY_flag_def *fd= flagdef;
	DynStr *dynstr= BLI_dynstr_new();
	char *cstring;

	BLI_dynstr_append(dynstr, "Error converting a sequence of strings into a flag.\n\tExpected only these strings...\n\t");

	while(fd->name) {
		BLI_dynstr_appendf(dynstr, fd!=flagdef?", '%s'":"'%s'", fd->name);
		fd++;
	}
	
	cstring = BLI_dynstr_get_cstring(dynstr);
	BLI_dynstr_free(dynstr);
	return cstring;
}

int BPY_flag_from_seq(BPY_flag_def *flagdef, PyObject *seq, int *flag)
{
	int i, error_val= 0;
	char *cstring;
	PyObject *item;
	BPY_flag_def *fd;

	if (PySequence_Check(seq)) {
		i= PySequence_Length(seq);

		while(i--) {
			item = PySequence_ITEM(seq, i);
			cstring= _PyUnicode_AsString(item);
			if(cstring) {
				fd= flagdef;
				while(fd->name) {
					if (strcmp(cstring, fd->name) == 0)
						(*flag) |= fd->flag;
					fd++;
				}
				if (fd==NULL) { /* could not find a match */
					error_val= 1;
				}
			} else {
				error_val= 1;
			}
			Py_DECREF(item);
		}
	}
	else {
		error_val= 1;
	}

	if (error_val) {
		char *buf = bpy_flag_error_str(flagdef);
		PyErr_SetString(PyExc_AttributeError, buf);
		MEM_freeN(buf);
		return -1; /* error value */
	}

	return 0; /* ok */
}


/* Copied from pythons 3's Object.c */
#ifndef Py_CmpToRich
PyObject *
Py_CmpToRich(int op, int cmp)
{
	PyObject *res;
	int ok;

	if (PyErr_Occurred())
		return NULL;
	switch (op) {
	case Py_LT:
		ok = cmp <  0;
		break;
	case Py_LE:
		ok = cmp <= 0;
		break;
	case Py_EQ:
		ok = cmp == 0;
		break;
	case Py_NE:
		ok = cmp != 0;
		break;
	case Py_GT:
		ok = cmp >  0;
		break;
	case Py_GE:
		ok = cmp >= 0;
		break;
	default:
		PyErr_BadArgument();
		return NULL;
	}
	res = ok ? Py_True : Py_False;
	Py_INCREF(res);
	return res;
}
#endif

/* for debugging */
void PyObSpit(char *name, PyObject *var) {
	fprintf(stderr, "<%s> : ", name);
	if (var==NULL) {
		fprintf(stderr, "<NIL>");
	}
	else {
		PyObject_Print(var, stderr, 0);
	}
	fprintf(stderr, "\n");
}

void BPY_getFileAndNum(char **filename, int *lineno)
{
	PyObject *getframe, *frame;
	PyObject *f_lineno, *f_code, *co_filename;
	
	if (filename)	*filename= NULL;
	if (lineno)		*lineno = -1;
	
	getframe = PySys_GetObject("_getframe"); // borrowed
	if (getframe) {
		frame = PyObject_CallObject(getframe, NULL);
		if (frame) {
			f_lineno= PyObject_GetAttrString(frame, "f_lineno");
			f_code= PyObject_GetAttrString(frame, "f_code");
			if (f_lineno && f_code) {
				co_filename= PyObject_GetAttrString(f_code, "co_filename");
				if (co_filename) {
					
					if (filename)	*filename = _PyUnicode_AsString(co_filename);
					if (lineno)		*lineno = (int)PyLong_AsSsize_t(f_lineno);
					
					Py_DECREF(f_lineno);
					Py_DECREF(f_code);
					Py_DECREF(co_filename);
					Py_DECREF(frame);
					
					return;
				}
			}
		}
	}
	
	Py_XDECREF(co_filename);
	Py_XDECREF(f_lineno);
	Py_XDECREF(f_code);
	Py_XDECREF(frame);
	
	PyErr_SetString(PyExc_SystemError, "Could not access sys._getframe().f_code.co_filename");
}
