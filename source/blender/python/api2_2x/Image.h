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
 * Contributor(s): Willian P. Germano
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#ifndef EXPP_IMAGE_H
#define EXPP_IMAGE_H

#include <Python.h>
#include <stdio.h>

#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_library.h>
#include <BKE_image.h>
#include <BLI_blenlib.h>
#include <DNA_image_types.h>

#include "gen_utils.h"
#include "modules.h"

/*****************************************************************************/
/* Python C_Image defaults:                                                 */
/*****************************************************************************/
#define EXPP_IMAGE_REP      1
#define EXPP_IMAGE_REP_MIN  1
#define EXPP_IMAGE_REP_MAX 16

/*****************************************************************************/
/* Python API function prototypes for the Image module.                     */
/*****************************************************************************/
static PyObject *M_Image_New (PyObject *self, PyObject *args,
                PyObject *keywords);
static PyObject *M_Image_Get (PyObject *self, PyObject *args);
static PyObject *M_Image_Load (PyObject *self, PyObject *args);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.Image.__doc__                                                    */
/*****************************************************************************/
char M_Image_doc[] =
"The Blender Image module\n\n";

char M_Image_New_doc[] =
"() - return a new Image object -- unimplemented";

char M_Image_Get_doc[] =
"(name) - return the image with the name 'name', \
returns None if not found.\n If 'name' is not specified, \
it returns a list of all images in the\ncurrent scene.";

char M_Image_Load_doc[] =
"(filename) - return image from file filename as Image Object, \
returns None if not found.\n";

/*****************************************************************************/
/* Python method structure definition for Blender.Image module:             */
/*****************************************************************************/
struct PyMethodDef M_Image_methods[] = {
  {"New",(PyCFunction)M_Image_New, METH_VARARGS|METH_KEYWORDS,
          M_Image_New_doc},
  {"Get",         M_Image_Get,         METH_VARARGS, M_Image_Get_doc},
  {"get",         M_Image_Get,         METH_VARARGS, M_Image_Get_doc},
  {"Load",        M_Image_Load,        METH_VARARGS, M_Image_Load_doc},
  {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python C_Image structure definition:                                     */
/*****************************************************************************/
typedef struct {
  PyObject_HEAD
  Image *image;
} C_Image;

/*****************************************************************************/
/* Python C_Image methods declarations:                                     */
/*****************************************************************************/
static PyObject *Image_getName(C_Image *self);
static PyObject *Image_getFilename(C_Image *self);
static PyObject *Image_setName(C_Image *self, PyObject *args);
static PyObject *Image_setXRep(C_Image *self, PyObject *args);
static PyObject *Image_setYRep(C_Image *self, PyObject *args);

/*****************************************************************************/
/* Python C_Image methods table:                                            */
/*****************************************************************************/
static PyMethodDef C_Image_methods[] = {
 /* name, method, flags, doc */
  {"getName", (PyCFunction)Image_getName, METH_NOARGS,
          "() - Return Image Data name"},
  {"getFilename", (PyCFunction)Image_getFilename, METH_VARARGS,
          "() - Return Image Data filename"},
  {"setName", (PyCFunction)Image_setName, METH_VARARGS,
          "(str) - Change Image Data name"},
  {"setXRep", (PyCFunction)Image_setXRep, METH_VARARGS,
          "(int) - Change Image Data x repetition value"},
  {"setYRep", (PyCFunction)Image_setYRep, METH_VARARGS,
          "(int) - Change Image Data y repetition value"},
  {0}
};

/*****************************************************************************/
/* Python Image_Type callback function prototypes:                          */
/*****************************************************************************/
static void ImageDeAlloc (C_Image *self);
static int ImagePrint (C_Image *self, FILE *fp, int flags);
static int ImageSetAttr (C_Image *self, char *name, PyObject *v);
static PyObject *ImageGetAttr (C_Image *self, char *name);
static int ImageCompare (C_Image *a, C_Image *b);
static PyObject *ImageRepr (C_Image *self);

/*****************************************************************************/
/* Python Image_Type structure definition:                                  */
/*****************************************************************************/
PyTypeObject Image_Type =
{
  PyObject_HEAD_INIT(&PyType_Type)
  0,                                     /* ob_size */
  "Image",                               /* tp_name */
  sizeof (C_Image),                      /* tp_basicsize */
  0,                                     /* tp_itemsize */
  /* methods */
  (destructor)ImageDeAlloc,              /* tp_dealloc */
  (printfunc)ImagePrint,                 /* tp_print */
  (getattrfunc)ImageGetAttr,             /* tp_getattr */
  (setattrfunc)ImageSetAttr,             /* tp_setattr */
  (cmpfunc)ImageCompare,                 /* tp_compare */
  (reprfunc)ImageRepr,                   /* tp_repr */
  0,                                     /* tp_as_number */
  0,                                     /* tp_as_sequence */
  0,                                     /* tp_as_mapping */
  0,                                     /* tp_as_hash */
  0,0,0,0,0,0,
  0,                                     /* tp_doc */ 
  0,0,0,0,0,0,
  C_Image_methods,                       /* tp_methods */
  0,                                     /* tp_members */
};

#endif /* EXPP_IMAGE_H */
