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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Jordi Rovira i Bonet
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#ifndef EXPP_ARMATURE_H
#define EXPP_ARMATURE_H

#include <Python.h>
#include <stdio.h>

#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_object.h>
#include <BKE_armature.h>
#include <BKE_library.h>
#include <BLI_blenlib.h>
#include <DNA_armature_types.h>

#include "constant.h"
#include "gen_utils.h"
#include "modules.h"


/*****************************************************************************/
/* Python API function prototypes for the Armature module.                   */
/*****************************************************************************/
static PyObject *M_Armature_New (PyObject *self, PyObject *args,
                                 PyObject *keywords);
static PyObject *M_Armature_Get (PyObject *self, PyObject *args);
PyObject *M_Armature_Init (void);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.Armature.__doc__                                                  */
/*****************************************************************************/
char M_Armature_doc[] =
"The Blender Armature module\n\n\
This module provides control over **Armature Data** objects in Blender.\n";

char M_Armature_New_doc[] =
"(name) - return a new Armature datablock of \n\
          optional name 'name'.";

char M_Armature_Get_doc[] =
"(name) - return the armature with the name 'name', \
returns None if not found.\n If 'name' is not specified, \
it returns a list of all armatures in the\ncurrent scene.";

char M_Armature_get_doc[] =
"(name) - DEPRECATED. Use 'Get' instead. \
return the armature with the name 'name', \
returns None if not found.\n If 'name' is not specified, \
it returns a list of all armatures in the\ncurrent scene.";

/*****************************************************************************/
/* Python method structure definition for Blender.Armature module:           */
/*****************************************************************************/
struct PyMethodDef M_Armature_methods[] = {
  {"New",(PyCFunction)M_Armature_New, METH_VARARGS|METH_KEYWORDS,
          M_Armature_New_doc},
  {"Get",         M_Armature_Get,         METH_VARARGS, M_Armature_Get_doc},
  {"get",         M_Armature_Get,         METH_VARARGS, M_Armature_get_doc},
  {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python C_Armature structure definition:                                   */
/*****************************************************************************/
typedef struct {
  PyObject_HEAD
  bArmature *armature;
} C_Armature;

/*****************************************************************************/
/* Python C_Armature methods declarations:                                   */
/*****************************************************************************/
static PyObject *Armature_getName(C_Armature *self);
static PyObject *Armature_getBones(C_Armature *self);
static PyObject *Armature_setName(C_Armature *self, PyObject *args);
//static PyObject *Armature_setBones(C_Armature *self, PyObject *args);

/*****************************************************************************/
/* Python C_Armature methods table:                                          */
/*****************************************************************************/
static PyMethodDef C_Armature_methods[] = {
 /* name, method, flags, doc */
  {"getName", (PyCFunction)Armature_getName, METH_NOARGS,
          "() - return Armature name"},
  {"getBones", (PyCFunction)Armature_getBones, METH_NOARGS,
          "() - return Armature root bones"},
  {"setName", (PyCFunction)Armature_setName, METH_VARARGS,
          "(str) - rename Armature"},
  /*  {"setBones", (PyCFunction)Armature_setBones, METH_VARARGS,
          "(list of bones) - replace the whole bone list of the armature"},
  */
  {0}
};

/*****************************************************************************/
/* Python TypeArmature callback function prototypes:                         */
/*****************************************************************************/
static void ArmatureDeAlloc (C_Armature *armature);
static PyObject *ArmatureGetAttr (C_Armature *armature, char *name);
static int ArmatureSetAttr (C_Armature *armature, char *name, PyObject *v);
static int ArmatureCmp (C_Armature *a1, C_Armature *a2);
static PyObject *ArmatureRepr (C_Armature *armature);
static int ArmaturePrint (C_Armature *armature, FILE *fp, int flags);

/*****************************************************************************/
/* Python TypeArmature structure definition:                                 */
/*****************************************************************************/
static PyTypeObject Armature_Type =
{
  PyObject_HEAD_INIT(NULL)
  0,                                      /* ob_size */
  "Armature",                               /* tp_name */
  sizeof (C_Armature),                     /* tp_basicsize */
  0,                                      /* tp_itemsize */
  /* methods */
  (destructor)ArmatureDeAlloc,              /* tp_dealloc */
  (printfunc)ArmaturePrint,                 /* tp_print */
  (getattrfunc)ArmatureGetAttr,             /* tp_getattr */
  (setattrfunc)ArmatureSetAttr,             /* tp_setattr */
  (cmpfunc)ArmatureCmp,                     /* tp_compare */
  (reprfunc)ArmatureRepr,                   /* tp_repr */
  0,                                      /* tp_as_number */
  0,                                      /* tp_as_sequence */
  0,                                      /* tp_as_mapping */
  0,                                      /* tp_as_hash */
  0,0,0,0,0,0,
  0,                                      /* tp_doc */ 
  0,0,0,0,0,0,
  C_Armature_methods,                      /* tp_methods */
  0,                                      /* tp_members */
};




#endif /* EXPP_ARMATURE_H */
