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
 * Contributor(s): Jacques Guignot
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#ifndef EXPP_METABALL_H
#define EXPP_METABALL_H

#include <Python.h>

#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_mball.h>
#include <BKE_object.h>
#include <BKE_library.h>
#include <BLI_blenlib.h>
#include <DNA_meta_types.h>

#include "constant.h"
#include "gen_utils.h"
#include "modules.h"

/*****************************************************************************/
/* Python C_Metaball defaults:                                                 */
/*****************************************************************************/


/*****************************************************************************/
/* Python API function prototypes for the Metaball module.                     */
/*****************************************************************************/
static PyObject *M_Metaball_New (PyObject *self, PyObject *args);
static PyObject *M_Metaball_Get (PyObject *self, PyObject *args);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.Metaball.__doc__                                                    */
/*****************************************************************************/
char M_Metaball_doc[] =
"The Blender Metaball module\n\n\nMetaballs are spheres\
 that can join each other to create smooth,\
 organic volumes\n. The spheres themseves are called\
 'Metaelements' and can be accessed from the Metaball module.";

char M_Metaball_New_doc[] ="Creates a new metaball";

char M_Metaball_Get_doc[] ="Retreives an existing metaball";

/*****************************************************************************/
/* Python method structure definition for Blender.Metaball module:             */
/*****************************************************************************/
struct PyMethodDef M_Metaball_methods[] = {
  {"New",M_Metaball_New, METH_VARARGS,M_Metaball_New_doc},
  {"Get",         M_Metaball_Get,         METH_VARARGS, M_Metaball_Get_doc},
  {"get",         M_Metaball_Get,         METH_VARARGS, M_Metaball_Get_doc},
  {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python C_Metaball structure definition:                                     */
/*****************************************************************************/
typedef struct {
  PyObject_HEAD
  MetaBall *metaball;
} C_Metaball;

/*****************************************************************************/
/* Python C_Metaball methods declarations:                                     */
/*****************************************************************************/
static PyObject *Metaball_getBbox(C_Metaball *self);
static PyObject *Metaball_getName(C_Metaball *self);
static PyObject *Metaball_setName(C_Metaball *self,PyObject*args);
static PyObject *Metaball_getWiresize(C_Metaball *self);
static PyObject *Metaball_setWiresize(C_Metaball *self,PyObject*args);
static PyObject *Metaball_getRendersize(C_Metaball *self);
static PyObject *Metaball_setRendersize(C_Metaball *self,PyObject*args);
static PyObject *Metaball_getThresh(C_Metaball *self);
static PyObject *Metaball_setThresh(C_Metaball *self,PyObject*args);
static PyObject *Metaball_getNMetaElems(C_Metaball *self);
static PyObject *Metaball_getNMetaElems1(C_Metaball *self);
static PyObject *Metaball_getMetatype(C_Metaball *self,PyObject*args);
static PyObject *Metaball_setMetatype(C_Metaball *self,PyObject*args);
static PyObject *Metaball_getMetadata(C_Metaball *self,PyObject*args);
static PyObject *Metaball_setMetadata(C_Metaball *self,PyObject*args);
static PyObject *Metaball_getMetalay(C_Metaball *self,PyObject*args);
static PyObject *Metaball_setMetalay(C_Metaball *self,PyObject*args);
static PyObject *Metaball_getMetaflag(C_Metaball *self,PyObject*args);
static PyObject *Metaball_setMetaflag(C_Metaball *self,PyObject*args);
static PyObject *Metaball_getMetaselcol(C_Metaball *self,PyObject*args);
static PyObject *Metaball_setMetaselcol(C_Metaball *self,PyObject*args);
static PyObject *Metaball_getMetapad(C_Metaball *self,PyObject*args);
static PyObject *Metaball_setMetapad(C_Metaball *self,PyObject*args);
static PyObject *Metaball_getMetax(C_Metaball *self,PyObject*args);
static PyObject *Metaball_setMetax(C_Metaball *self,PyObject*args);
static PyObject *Metaball_getMetay(C_Metaball *self,PyObject*args);
static PyObject *Metaball_setMetay(C_Metaball *self,PyObject*args);
static PyObject *Metaball_getMetaz(C_Metaball *self,PyObject*args);
static PyObject *Metaball_setMetaz(C_Metaball *self,PyObject*args);
static PyObject *Metaball_getMetaexpx(C_Metaball *self,PyObject*args);
static PyObject *Metaball_setMetaexpx(C_Metaball *self,PyObject*args);
static PyObject *Metaball_getMetaexpy(C_Metaball *self,PyObject*args);
static PyObject *Metaball_setMetaexpy(C_Metaball *self,PyObject*args);
static PyObject *Metaball_getMetaexpz(C_Metaball *self,PyObject*args);
static PyObject *Metaball_setMetaexpz(C_Metaball *self,PyObject*args);
static PyObject *Metaball_getMetarad(C_Metaball *self,PyObject*args);
static PyObject *Metaball_setMetarad(C_Metaball *self,PyObject*args);
static PyObject *Metaball_getMetarad2(C_Metaball *self,PyObject*args);
static PyObject *Metaball_setMetarad2(C_Metaball *self,PyObject*args);
static PyObject *Metaball_getMetas(C_Metaball *self,PyObject*args);
static PyObject *Metaball_setMetas(C_Metaball *self,PyObject*args);
static PyObject *Metaball_getMetalen(C_Metaball *self,PyObject*args);
static PyObject *Metaball_setMetalen(C_Metaball *self,PyObject*args);
static PyObject *Metaball_getMetamaxrad2(C_Metaball *self,PyObject*args);
static PyObject *Metaball_setMetamaxrad2(C_Metaball *self,PyObject*args);
static PyObject *Metaball_getloc(C_Metaball *self);
static PyObject *Metaball_setloc(C_Metaball *self,PyObject*args);
static PyObject *Metaball_getrot(C_Metaball *self);
static PyObject *Metaball_setrot(C_Metaball *self,PyObject*args);
static PyObject *Metaball_getsize(C_Metaball *self);
static PyObject *Metaball_setsize(C_Metaball *self,PyObject*args);

/*****************************************************************************/
/* Python C_Metaball methods table:                                            */
/*****************************************************************************/
static PyMethodDef C_Metaball_methods[] = {
 /* name, method, flags, doc */
  {"getName", (PyCFunction)Metaball_getName,\
   METH_NOARGS, "() - Return Metaball Data name"},

  {"setName", (PyCFunction)Metaball_setName,\
 METH_VARARGS, "() - Sets Metaball Data name"},

  {"getWiresize", (PyCFunction)Metaball_getWiresize,\
   METH_NOARGS, "() - Return Metaball Data wiresize"},

  {"setWiresize", (PyCFunction)Metaball_setWiresize,\
 METH_VARARGS, "() - Sets Metaball Data wiresize"},

  {"getRendersize", (PyCFunction)Metaball_getRendersize,\
   METH_NOARGS, "() - Return Metaball Data rendersize"},

  {"setRendersize", (PyCFunction)Metaball_setRendersize,\
 METH_VARARGS, "() - Sets Metaball Data rendersize"},

  {"getThresh", (PyCFunction)Metaball_getThresh,\
   METH_NOARGS, "() - Return Metaball Data thresh"},

  {"setThresh", (PyCFunction)Metaball_setThresh,\
 METH_VARARGS, "() - Sets Metaball Data thresh"},  

{"getBbox", (PyCFunction)Metaball_getBbox,\
   METH_NOARGS, "() - Return Metaball bounding box"},

  {"getNMetaElems",(PyCFunction)Metaball_getNMetaElems,\
METH_NOARGS, "() - "},

  {"getNMetaElems1",(PyCFunction)Metaball_getNMetaElems1,\
METH_NOARGS, "() - "},
  {"getMetatype", (PyCFunction)Metaball_getMetatype , \
METH_VARARGS, "() - "},
  {"setMetatype", (PyCFunction)Metaball_setMetatype , \
METH_VARARGS, "() - "},
  {"getMetadata", (PyCFunction)Metaball_getMetadata , \
METH_VARARGS, "() - Gets Metaball MetaData "},
  {"setMetadata", (PyCFunction)Metaball_setMetadata , \
METH_VARARGS, "() - "},
  {"getMetalay", (PyCFunction)Metaball_getMetalay , \
METH_VARARGS, "() - "},
  {"setMetalay", (PyCFunction)Metaball_setMetalay , \
METH_VARARGS, "() - "},
  {"getMetaflag", (PyCFunction)Metaball_getMetaflag , \
METH_VARARGS, "() - "},
  {"setMetaflag", (PyCFunction)Metaball_setMetaflag , \
METH_VARARGS, "() - "},
  {"getMetaselcol", (PyCFunction)Metaball_getMetaselcol , \
METH_VARARGS, "() - "},
  {"setMetaselcol", (PyCFunction)Metaball_setMetaselcol , \
METH_VARARGS, "() - "},
  {"getMetapad", (PyCFunction)Metaball_getMetapad , \
METH_VARARGS, "() - "},
  {"setMetapad", (PyCFunction)Metaball_setMetapad , \
METH_VARARGS, "() - "},
  {"getMetax", (PyCFunction)Metaball_getMetax , \
METH_VARARGS, "() - "},
  {"setMetax", (PyCFunction)Metaball_setMetax , \
METH_VARARGS, "() - "},
  {"getMetay", (PyCFunction)Metaball_getMetay , \
METH_VARARGS, "() - "},
  {"setMetay", (PyCFunction)Metaball_setMetay , \
METH_VARARGS, "() - "},
  {"getMetaz", (PyCFunction)Metaball_getMetaz , \
METH_VARARGS, "() - "},
  {"setMetaz", (PyCFunction)Metaball_setMetaz , \
METH_VARARGS, "() - "},
  {"getMetaexpx", (PyCFunction)Metaball_getMetaexpx , \
METH_VARARGS, "() - "},
  {"setMetaexpx", (PyCFunction)Metaball_setMetaexpx , \
METH_VARARGS, "() - "},
  {"getMetaexpy", (PyCFunction)Metaball_getMetaexpy , \
METH_VARARGS, "() - "},
  {"setMetaexpy", (PyCFunction)Metaball_setMetaexpy , \
METH_VARARGS, "() - "},
  {"getMetaexpz", (PyCFunction)Metaball_getMetaexpz , \
METH_VARARGS, "() - "},
  {"setMetaexpz", (PyCFunction)Metaball_setMetaexpz , \
METH_VARARGS, "() - "},
  {"getMetarad", (PyCFunction)Metaball_getMetarad , \
METH_VARARGS, "() - "},
  {"setMetarad", (PyCFunction)Metaball_setMetarad , \
METH_VARARGS, "() - "},
  {"getMetarad2", (PyCFunction)Metaball_getMetarad2 , \
METH_VARARGS, "() - "},
  {"setMetarad2", (PyCFunction)Metaball_setMetarad2 , \
METH_VARARGS, "() - "},
  {"getMetas", (PyCFunction)Metaball_getMetas , \
METH_VARARGS, "() - "},
  {"setMetas", (PyCFunction)Metaball_setMetas , \
METH_VARARGS, "() - "},
  {"getMetalen", (PyCFunction)Metaball_getMetalen , \
METH_VARARGS, "() - "},
  {"setMetalen", (PyCFunction)Metaball_setMetalen , \
METH_VARARGS, "() - "},
  {"getMetamaxrad2", (PyCFunction)Metaball_getMetamaxrad2 , \
METH_VARARGS, "() - "},
  {"setMetamaxrad2", (PyCFunction)Metaball_setMetamaxrad2 , \
METH_VARARGS, "() - "},
  {"getloc", (PyCFunction)Metaball_getloc , \
METH_NOARGS, "() - Gets Metaball loc values"},
  {"setloc", (PyCFunction)Metaball_setloc , \
METH_VARARGS, "(f f f) - Sets Metaball loc values"},
  {"getrot", (PyCFunction)Metaball_getrot , \
METH_NOARGS, "() - Gets Metaball rot values"},
  {"setrot", (PyCFunction)Metaball_setrot , \
METH_VARARGS, "(f f f) - Sets Metaball rot values"},
  {"getsize", (PyCFunction)Metaball_getsize , \
METH_NOARGS, "() - Gets Metaball size values"},
  {"setsize", (PyCFunction)Metaball_setsize , \
METH_VARARGS, "(f f f) - Sets Metaball size values"},
/*end of MetaElem data*/
 {0}
};

/*****************************************************************************/
/* Python Metaball_Type callback function prototypes:                          */
/*****************************************************************************/
static void MetaballDeAlloc (C_Metaball *self);
static int MetaballPrint (C_Metaball *self, FILE *fp, int flags);
static int MetaballSetAttr (C_Metaball *self, char *name, PyObject *v);
static PyObject *MetaballGetAttr (C_Metaball *self, char *name);
static PyObject *MetaballRepr (C_Metaball *self);

/*****************************************************************************/
/* Python Metaball_Type structure definition:                                  */
/*****************************************************************************/
PyTypeObject Metaball_Type =
{
  PyObject_HEAD_INIT(&PyType_Type)
  0,                                      /* ob_size */
  "Metaball",                               /* tp_name */
  sizeof (C_Metaball),                      /* tp_basicsize */
  0,                                      /* tp_itemsize */
  /* methods */
  (destructor)MetaballDeAlloc,              /* tp_dealloc */
  (printfunc)MetaballPrint,                 /* tp_print */
  (getattrfunc)MetaballGetAttr,             /* tp_getattr */
  (setattrfunc)MetaballSetAttr,             /* tp_setattr */
  0,                                      /* tp_compare */
  (reprfunc)MetaballRepr,                   /* tp_repr */
  0,                                      /* tp_as_number */
  0,                                      /* tp_as_sequence */
  0,                                      /* tp_as_mapping */
  0,                                      /* tp_as_hash */
  0,0,0,0,0,0,
  0,                                      /* tp_doc */ 
  0,0,0,0,0,0,
  C_Metaball_methods,                       /* tp_methods */
  0,                                      /* tp_members */
};

#endif /* EXPP_METABALL_H */
