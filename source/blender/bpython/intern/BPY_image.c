/**
 * Image Datablocks
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
#include "opy_datablock.h"

#include "DNA_image_types.h"
#include "BKE_image.h"

#include "BPY_macros.h"
#include "b_interface.h"

PyObject *INITMODULE(Image)(void);

/* Image_Get */
DATABLOCK_GET(Imagemodule, image, getImageList())

char Imagemodule_load_doc[] = "(filename) - return image from file 'filename' as Image object";

PyObject *Imagemodule_load(PyObject *self, PyObject *args)
{
	char *name;
	Image *im;

	if (!PyArg_ParseTuple(args, "s", &name)) {
		PyErr_SetString(PyExc_TypeError, "filename expected");
		return 0;
	}
		
	im = add_image(name);
	if (im) {
		return DataBlock_fromData(im);
	} else {
		PyErr_SetString(PyExc_IOError, "couldn't load image");
		return 0;
	}	
}

DataBlockProperty Image_Properties[]= {
	{"xrep", "xrep", DBP_TYPE_SHO, 0, 1.0, 16.0},
	{"yrep", "yrep", DBP_TYPE_SHO, 0, 1.0, 16.0},
//	{"PackedFile", "*packedfile", DBP_TYPE_FUN, 0, 0.0, 0.0, {0}, {0}, 0, 0, get_DataBlock_func},
	{NULL}
};

#undef MethodDef
#define MethodDef(func) _MethodDef(func, Imagemodule)

struct PyMethodDef Imagemodule_methods[] = {
	MethodDef(get),
	MethodDef(load),
	// for compatibility:
	{"Load", Imagemodule_load, METH_VARARGS, Imagemodule_load_doc},
	{NULL, NULL}
};


/*
void Image_getattr(void *vdata, char *name)
{
}
*/


PyObject *INITMODULE(Image)(void) 
{
	PyObject *mod= Py_InitModule(SUBMODULE(Image), Imagemodule_methods);
	return mod;
}




