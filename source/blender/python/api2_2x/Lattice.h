/* 
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Joseph Gilbert
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#ifndef EXPP_Lattice_H
#define EXPP_Lattice_H

#include <Python.h>
#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_library.h>
#include <BKE_lattice.h>
#include <BKE_utildefines.h>
#include <BKE_key.h>
#include <BLI_blenlib.h>
#include <DNA_lattice_types.h>
#include <DNA_key_types.h>
#include <BIF_editlattice.h>
#include <BIF_editkey.h>
#include "blendef.h"
#include "mydevice.h"
#include "constant.h"
#include "gen_utils.h"
#include "modules.h"

/*****************************************************************************/
/* Python API function prototypes for the Lattice module.												*/
/*****************************************************************************/
static PyObject *M_Lattice_New (PyObject *self, PyObject *args);
static PyObject *M_Lattice_Get(PyObject *self, PyObject *args);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.			 */
/* In Python these will be written to the console when doing a							 */
/* Blender.Lattice.__doc__		Lattice Module strings																								 */
/*****************************************************************************/
static char M_Lattice_doc[] =
"The Blender Lattice module\n\n";

static char M_Lattice_New_doc[] =
"() - return a new Lattice object";

static char M_Lattice_Get_doc[] =
"() - geta a Lattice from blender";

/*****************************************************************************/
/* Python method structure definition for Blender.Lattice module:								*/
/*****************************************************************************/
struct PyMethodDef M_Lattice_methods[] = {
	{"New",(PyCFunction)M_Lattice_New, METH_VARARGS,
					M_Lattice_New_doc},
	{"Get",(PyCFunction)M_Lattice_Get, METH_VARARGS,
					M_Lattice_Get_doc},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python BPy_Lattice structure definition:																			*/
/*****************************************************************************/
typedef struct {
	PyObject_HEAD
	Lattice *Lattice;
} BPy_Lattice;

/*****************************************************************************/
/* Python BPy_Lattice methods declarations:																			*/
/*****************************************************************************/
static PyObject *Lattice_getName(BPy_Lattice *self);
static PyObject *Lattice_setName(BPy_Lattice *self, PyObject *args);
static PyObject *Lattice_setPartitions(BPy_Lattice *self, PyObject *args);
static PyObject *Lattice_getPartitions(BPy_Lattice *self, PyObject *args);
static PyObject *Lattice_setKeyTypes(BPy_Lattice *self, PyObject *args);
static PyObject *Lattice_getKeyTypes(BPy_Lattice *self, PyObject *args);
static PyObject *Lattice_setMode(BPy_Lattice *self, PyObject *args);
static PyObject *Lattice_getMode(BPy_Lattice *self, PyObject *args);
static PyObject *Lattice_setPoint(BPy_Lattice *self, PyObject *args);
static PyObject *Lattice_getPoint(BPy_Lattice *self, PyObject *args);
static PyObject *Lattice_applyDeform(BPy_Lattice *self);
static PyObject *Lattice_insertKey(BPy_Lattice *self, PyObject *args);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.			 */
/* In Python these will be written to the console when doing a							 */
/* Blender.Lattice.__doc__			 Lattice Strings																						 */
/*****************************************************************************/
static char Lattice_getName_doc[] =
"() - Return Lattice Object name";

static char Lattice_setName_doc[] =
"(str) - Change Lattice Object name";

static char Lattice_setPartitions_doc[] =
"(str) - Set the number of Partitions in x,y,z";

static char Lattice_getPartitions_doc[] =
"(str) - Get the number of Partitions in x,y,z";

static char Lattice_setKeyTypes_doc[] =
"(str) - Set the key types for x,y,z dimensions";

static char Lattice_getKeyTypes_doc[] =
"(str) - Get the key types for x,y,z dimensions";

static char Lattice_setMode_doc[] =
"(str) - Make an outside or grid lattice";

static char Lattice_getMode_doc[] =
"(str) - Get lattice mode type";

static char Lattice_setPoint_doc[] =
"(str) - Set the coordinates of a point on the lattice";

static char Lattice_getPoint_doc[] =
"(str) - Get the coordinates of a point on the lattice";

static char Lattice_applyDeform_doc[] =
"(str) - Apply the new lattice deformation to children";

static char Lattice_insertKey_doc[] =
"(str) - Set a new key for the lattice at specified frame";

/*****************************************************************************/
/* Python BPy_Lattice methods table:																						*/
/*****************************************************************************/
static PyMethodDef BPy_Lattice_methods[] = {
 /* name, method, flags, doc */
	{"getName", (PyCFunction)Lattice_getName, METH_NOARGS, 
			Lattice_getName_doc},
	{"setName", (PyCFunction)Lattice_setName, METH_VARARGS, 
			Lattice_setName_doc},
	{"setPartitions", (PyCFunction)Lattice_setPartitions, METH_VARARGS, 
			Lattice_setPartitions_doc},
	{"getPartitions", (PyCFunction)Lattice_getPartitions, METH_NOARGS,
			Lattice_getPartitions_doc},
	{"setKeyTypes", (PyCFunction)Lattice_setKeyTypes, METH_VARARGS,
			Lattice_setKeyTypes_doc},
	{"getKeyTypes", (PyCFunction)Lattice_getKeyTypes, METH_NOARGS,
			Lattice_getKeyTypes_doc},
	{"setMode", (PyCFunction)Lattice_setMode, METH_VARARGS,
			Lattice_setMode_doc},
	{"getMode", (PyCFunction)Lattice_getMode, METH_NOARGS,
			Lattice_getMode_doc},
	{"setPoint", (PyCFunction)Lattice_setPoint, METH_VARARGS,
			Lattice_setPoint_doc},
	{"getPoint", (PyCFunction)Lattice_getPoint, METH_VARARGS,
			Lattice_getPoint_doc},
	{"applyDeform", (PyCFunction)Lattice_applyDeform, METH_NOARGS,
			Lattice_applyDeform_doc},
	{"insertKey", (PyCFunction)Lattice_insertKey, METH_VARARGS,
			Lattice_insertKey_doc},
	{0}
};

/*****************************************************************************/
/* Python Lattice_Type callback function prototypes:														*/
/*****************************************************************************/
static void Lattice_dealloc (BPy_Lattice *self);
static int	Lattice_setAttr (BPy_Lattice *self, char *name, PyObject *v);
static		PyObject *Lattice_getAttr (BPy_Lattice *self, char *name);
static		PyObject *Lattice_repr (BPy_Lattice *self);

/*****************************************************************************/
/* Python Lattice_Type structure definition:																		*/
/*****************************************************************************/
PyTypeObject Lattice_Type =
{
	PyObject_HEAD_INIT(NULL)
	0,																		/* ob_size */
	"Blender Lattice",										/* tp_name */
	sizeof (BPy_Lattice),									/* tp_basicsize */
	0,																		/* tp_itemsize */
	/* methods */
	(destructor)Lattice_dealloc,					/* tp_dealloc */
	0,																		/* tp_print */
	(getattrfunc)Lattice_getAttr,					/* tp_getattr */
	(setattrfunc)Lattice_setAttr,					/* tp_setattr */
	0,												/* tp_compare */
	(reprfunc)Lattice_repr,								/* tp_repr */
	0,																		/* tp_as_number */
	0,																		/* tp_as_sequence */
	0,																		/* tp_as_mapping */
	0,																		/* tp_as_hash */
	0,0,0,0,0,0,
	0,																		/* tp_doc */ 
	0,0,0,0,0,0,
	BPy_Lattice_methods,									/* tp_methods */
	0,																		/* tp_members */
};

static int Lattice_InLatList(BPy_Lattice *self);
static int Lattice_IsLinkedToObject(BPy_Lattice *self);

#endif /* EXPP_LATTICE_H */
