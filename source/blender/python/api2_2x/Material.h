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

#ifndef EXPP_MATERIAL_H
#define EXPP_MATERIAL_H

#include <Python.h>

#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_library.h>
#include <DNA_ID.h>
#include <BKE_material.h>
#include <BLI_blenlib.h>
#include <DNA_material_types.h>

#include "constant.h"
#include "rgbTuple.h"
#include "gen_utils.h"
#include "modules.h"

/*****************************************************************************/
/* Python C_Material defaults:                                               */
/*****************************************************************************/
#define EXPP_MAT_MODE_TRACEABLE           MA_TRACEBLE
#define EXPP_MAT_MODE_SHADOW              MA_SHADOW
#define EXPP_MAT_MODE_SHADELESS           MA_SHLESS
#define EXPP_MAT_MODE_WIRE                MA_WIRE
#define EXPP_MAT_MODE_VCOLLIGHT           MA_VERTEXCOL
#define EXPP_MAT_MODE_HALO                MA_HALO
#define EXPP_MAT_MODE_ZTRANSP             MA_ZTRA
#define EXPP_MAT_MODE_VCOLPAINT           MA_VERTEXCOLP
#define EXPP_MAT_MODE_ZINVERT             MA_ZINV
#define EXPP_MAT_MODE_HALORINGS           MA_HALO_RINGS
#define EXPP_MAT_MODE_ENV                 MA_ENV
#define EXPP_MAT_MODE_HALOLINES           MA_HALO_LINES
#define EXPP_MAT_MODE_ONLYSHADOW          MA_ONLYSHADOW
#define EXPP_MAT_MODE_XALPHA              MA_HALO_XALPHA
#define EXPP_MAT_MODE_STAR                MA_STAR
#define EXPP_MAT_MODE_FACETEX             MA_FACETEXTURE
#define EXPP_MAT_MODE_HALOTEX             MA_HALOTEX
#define EXPP_MAT_MODE_HALOPUNO            MA_HALOPUNO
#define EXPP_MAT_MODE_NOMIST              MA_NOMIST
#define EXPP_MAT_MODE_HALOSHADE           MA_HALO_SHADE
#define EXPP_MAT_MODE_HALOFLARE           MA_HALO_FLARE

/* Material MIN, MAX values */
#define EXPP_MAT_ADD_MIN           0.0
#define EXPP_MAT_ADD_MAX           1.0
#define EXPP_MAT_ALPHA_MIN         0.0
#define EXPP_MAT_ALPHA_MAX         1.0
#define EXPP_MAT_AMB_MIN           0.0
#define EXPP_MAT_AMB_MAX           1.0
#define EXPP_MAT_ANG_MIN           0.0 /* XXX Confirm these two */
#define EXPP_MAT_ANG_MAX           1.0
#define EXPP_MAT_COL_MIN           0.0 /* min/max for all ... */
#define EXPP_MAT_COL_MAX           1.0 /* ... color triplets  */
#define EXPP_MAT_EMIT_MIN          0.0
#define EXPP_MAT_EMIT_MAX          1.0
#define EXPP_MAT_REF_MIN           0.0
#define EXPP_MAT_REF_MAX           1.0
#define EXPP_MAT_SPEC_MIN          0.0
#define EXPP_MAT_SPEC_MAX          2.0
#define EXPP_MAT_SPECTRA_MIN       0.0
#define EXPP_MAT_SPECTRA_MAX       1.0
#define EXPP_MAT_ZOFFS_MIN         0.0
#define EXPP_MAT_ZOFFS_MAX        10.0
#define EXPP_MAT_HALOSIZE_MIN      0.0
#define EXPP_MAT_HALOSIZE_MAX    100.0
#define EXPP_MAT_FLARESIZE_MIN     0.1
#define EXPP_MAT_FLARESIZE_MAX    25.0
#define EXPP_MAT_FLAREBOOST_MIN    0.1
#define EXPP_MAT_FLAREBOOST_MAX   10.0
#define EXPP_MAT_SUBSIZE_MIN       0.1
#define EXPP_MAT_SUBSIZE_MAX      25.0

#define EXPP_MAT_HARD_MIN        1
#define EXPP_MAT_HARD_MAX      255  /* 127 with MODE HALO ON */
#define EXPP_MAT_NFLARES_MIN     1
#define EXPP_MAT_NFLARES_MAX    32
#define EXPP_MAT_NSTARS_MIN      3
#define EXPP_MAT_NSTARS_MAX     50
#define EXPP_MAT_NLINES_MIN      0
#define EXPP_MAT_NLINES_MAX    250
#define EXPP_MAT_NRINGS_MIN      0
#define EXPP_MAT_NRINGS_MAX     24

/*****************************************************************************/
/* Python API function prototypes for the Material module.                     */
/*****************************************************************************/
static PyObject *M_Material_New (PyObject *self, PyObject *args,
                               PyObject *keywords);
static PyObject *M_Material_Get (PyObject *self, PyObject *args);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.Material.__doc__                                                    */
/*****************************************************************************/
char M_Material_doc[] =
"The Blender Material module";

char M_Material_New_doc[] =
"(name) - return a new material called 'name'\n\
() - return a new material called 'Mat'";

char M_Material_Get_doc[] =
"(name) - return the material called 'name', None if not found.\n\
() - return a list of all materials in the current scene.";

/*****************************************************************************/
/* Python method structure definition for Blender.Material module:             */
/*****************************************************************************/
struct PyMethodDef M_Material_methods[] = {
  {"New",(PyCFunction)M_Material_New, METH_VARARGS|METH_KEYWORDS,
          M_Material_New_doc},
  {"Get",         M_Material_Get,         METH_VARARGS, M_Material_Get_doc},
  {"get",         M_Material_Get,         METH_VARARGS, M_Material_Get_doc},
  {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python C_Material structure definition:                                   */
/*****************************************************************************/
typedef struct {
  PyObject_HEAD
  Material *material;
	C_rgbTuple *rgb, *amb, *spec, *mir;

} C_Material;

/*****************************************************************************/
/* Python C_Material methods declarations:                                   */
/*****************************************************************************/
static PyObject *Material_getName(C_Material *self);
static PyObject *Material_getMode(C_Material *self);
static PyObject *Material_getRGBCol(C_Material *self);
static PyObject *Material_getAmbCol(C_Material *self);
static PyObject *Material_getSpecCol(C_Material *self);
static PyObject *Material_getMirCol(C_Material *self);
static PyObject *Material_getAmb(C_Material *self);
static PyObject *Material_getAng(C_Material *self);
static PyObject *Material_getEmit(C_Material *self);
static PyObject *Material_getAlpha(C_Material *self);
static PyObject *Material_getRef(C_Material *self);
static PyObject *Material_getSpec(C_Material *self);
static PyObject *Material_getSpecTransp(C_Material *self);
static PyObject *Material_getAdd(C_Material *self);
static PyObject *Material_getZOffset(C_Material *self);
static PyObject *Material_getHaloSize(C_Material *self);
static PyObject *Material_getFlareSize(C_Material *self);
static PyObject *Material_getFlareBoost(C_Material *self);
static PyObject *Material_getSubSize(C_Material *self);
static PyObject *Material_getHardness(C_Material *self);
static PyObject *Material_getNFlares(C_Material *self);
static PyObject *Material_getNStars(C_Material *self);
static PyObject *Material_getNLines(C_Material *self);
static PyObject *Material_getNRings(C_Material *self);
static PyObject *Material_setName(C_Material *self, PyObject *args);
static PyObject *Material_setMode(C_Material *self, PyObject *args);
static PyObject *Material_setIntMode(C_Material *self, PyObject *args);
static PyObject *Material_setRGBCol(C_Material *self, PyObject *args);
static PyObject *Material_setAmbCol(C_Material *self, PyObject *args);
static PyObject *Material_setSpecCol(C_Material *self, PyObject *args);
static PyObject *Material_setMirCol(C_Material *self, PyObject *args);
static PyObject *Material_setAmb(C_Material *self, PyObject *args);
static PyObject *Material_setEmit(C_Material *self, PyObject *args);
static PyObject *Material_setAng(C_Material *self, PyObject *args);
static PyObject *Material_setAlpha(C_Material *self, PyObject *args);
static PyObject *Material_setRef(C_Material *self, PyObject *args);
static PyObject *Material_setSpec(C_Material *self, PyObject *args);
static PyObject *Material_setSpecTransp(C_Material *self, PyObject *args);
static PyObject *Material_setAdd(C_Material *self, PyObject *args);
static PyObject *Material_setZOffset(C_Material *self, PyObject *args);
static PyObject *Material_setHaloSize(C_Material *self, PyObject *args);
static PyObject *Material_setFlareSize(C_Material *self, PyObject *args);
static PyObject *Material_setFlareBoost(C_Material *self, PyObject *args);
static PyObject *Material_setSubSize(C_Material *self, PyObject *args);
static PyObject *Material_setHardness(C_Material *self, PyObject *args);
static PyObject *Material_setNFlares(C_Material *self, PyObject *args);
static PyObject *Material_setNStars(C_Material *self, PyObject *args);
static PyObject *Material_setNLines(C_Material *self, PyObject *args);
static PyObject *Material_setNRings(C_Material *self, PyObject *args);

static PyObject *Material_setColorComponent(C_Material *self, char *key,
								PyObject *args);

/*****************************************************************************/
/* Python C_Material methods table:                                            */
/*****************************************************************************/
static PyMethodDef C_Material_methods[] = {
 /* name, method, flags, doc */
  {"getName", (PyCFunction)Material_getName, METH_NOARGS,
      "() - Return Material Data name"},
  {"getMode", (PyCFunction)Material_getMode, METH_NOARGS,
      "() - Return Material mode flags"},
  {"getRGBCol", (PyCFunction)Material_getRGBCol, METH_NOARGS,
      "() - Return Material's rgb color triplet"},
  {"getAmbCol", (PyCFunction)Material_getAmbCol, METH_NOARGS,
      "() - Return Material's ambient color"},
  {"getSpecCol", (PyCFunction)Material_getSpecCol, METH_NOARGS,
      "() - Return Material's specular color"},
  {"getMirCol", (PyCFunction)Material_getMirCol, METH_NOARGS,
      "() - Return Material's mirror color"},
  {"getAmb", (PyCFunction)Material_getAmb, METH_NOARGS,
      "() - Return Material's ambient color blend factor"},
  {"getAng", (PyCFunction)Material_getAng, METH_NOARGS,
      "() - Return Material's ????"},
  {"getEmit", (PyCFunction)Material_getEmit, METH_NOARGS,
      "() - Return Material's emitting light intensity"},
  {"getAlpha", (PyCFunction)Material_getAlpha, METH_NOARGS,
      "() - Return Material's alpha (transparency) value"},
  {"getRef", (PyCFunction)Material_getRef, METH_NOARGS,
      "() - Return Material's reflectivity"},
  {"getSpec", (PyCFunction)Material_getSpec, METH_NOARGS,
      "() - Return Material's specularity"},
  {"getSpecTransp", (PyCFunction)Material_getSpecTransp, METH_NOARGS,
      "() - Return Material's specular transparency"},
  {"getAdd", (PyCFunction)Material_getAdd, METH_NOARGS,
      "() - Return Material's glow factor"},
  {"getZOffset", (PyCFunction)Material_getZOffset, METH_NOARGS,
      "() - Return Material's artificial offset "},
  {"getHaloSize", (PyCFunction)Material_getHaloSize, METH_NOARGS,
      "() - Return Material's halo size"},
  {"getFlareSize", (PyCFunction)Material_getFlareSize, METH_NOARGS,
      "() - Return Material's (flare size)/(halo size) factor"},
  {"getFlareBoost", (PyCFunction)Material_getFlareBoost, METH_NOARGS,
      "() - Return Material's flare boost"},
  {"getSubSize", (PyCFunction)Material_getSubSize, METH_NOARGS,
      "() - Return Material's dimension of subflare, dots and circles"},
  {"getHardness", (PyCFunction)Material_getHardness, METH_NOARGS,
      "() - Return Material's hardness"},
  {"getNFlares", (PyCFunction)Material_getNFlares, METH_NOARGS,
      "() - Return Material's number of flares in halo"},
  {"getNStars", (PyCFunction)Material_getNStars, METH_NOARGS,
      "() - Return Material's number of stars in halo"},
  {"getNLines", (PyCFunction)Material_getNLines, METH_NOARGS,
      "() - Return Material's number of lines in halo"},
  {"getNRings", (PyCFunction)Material_getNRings, METH_NOARGS,
      "() - Return Material's number of rings in halo"},
  {"setName", (PyCFunction)Material_setName, METH_VARARGS,
      "(s) - Change Material Data name"},
  {"setMode", (PyCFunction)Material_setMode, METH_VARARGS,
      "([s[,s]]) - Set Material mode flag(s)"},
  {"setRGBCol", (PyCFunction)Material_setMode, METH_VARARGS,
      "([s[,s]]) - Set Material's rgb color triplet"},
  {"setAmbCol", (PyCFunction)Material_setMode, METH_VARARGS,
      "([s[,s]]) - Set Material's ambient color"},
  {"setSpecCol", (PyCFunction)Material_setMode, METH_VARARGS,
      "([s[,s]]) - Set Material's specular color"},
  {"setMirCol", (PyCFunction)Material_setMode, METH_VARARGS,
      "([s[,s]]) - Set Material's mirror color"},
  {"setAmb", (PyCFunction)Material_setAmb, METH_VARARGS,
      "(f) - Set how much the Material's color is affected"
							" by \nthe global ambient colors - [0.0, 1.0]"},
  {"setAng", (PyCFunction)Material_setAng, METH_VARARGS,
      "(f) - Set Material's ?????"},
  {"setEmit", (PyCFunction)Material_setEmit, METH_VARARGS,
      "(f) - Set Material's emitting light intensity - [0.0, 1.0]"},
  {"setAlpha", (PyCFunction)Material_setAlpha, METH_VARARGS,
      "(f) - Set Material's alpha (transparency) - [0.0, 1.0]"},
  {"setRef", (PyCFunction)Material_setRef, METH_VARARGS,
      "(f) - Set Material's reflectivity - [0.0, 1.0]"},
  {"setSpec", (PyCFunction)Material_setSpec, METH_VARARGS,
      "(f) - Set Material's specularity - [0.0, 2.0]"},
  {"setSpecTransp", (PyCFunction)Material_setSpecTransp, METH_VARARGS,
      "(f) - Set Material's specular transparency - [0.0, 1.0]"},
  {"setAdd", (PyCFunction)Material_setAdd, METH_VARARGS,
      "(f) - Set Material's glow factor - [0.0, 1.0]"},
  {"setZOffset", (PyCFunction)Material_setZOffset, METH_VARARGS,
      "(f) - Set Material's artificial offset - [0.0, 10.0]"},
  {"setHaloSize", (PyCFunction)Material_setHaloSize, METH_VARARGS,
      "(f) - Set Material's halo size - [0.0, 100.0]"},
  {"setFlareSize", (PyCFunction)Material_setFlareSize, METH_VARARGS,
      "(f) - Set Material's factor: (flare size)/(halo size) - [0.1, 25.0]"},
  {"setFlareBoost", (PyCFunction)Material_setFlareBoost, METH_VARARGS,
      "(f) - Set Material's flare boost - [0.1, 10.0]"},
  {"setSubSize", (PyCFunction)Material_setSubSize, METH_VARARGS,
      "(f) - Set Material's dimension of subflare,"
							" dots and circles - [0.1, 25.0]"},
  {"setHardness", (PyCFunction)Material_setFlareBoost, METH_VARARGS,
      "(f) - Set Material's hardness - [1, 255 (127 if halo mode is ON)]"},
  {"setNFlares", (PyCFunction)Material_setFlareBoost, METH_VARARGS,
      "(f) - Set Material's number of flares in halo - [1, 32]"},
  {"setNStars", (PyCFunction)Material_setFlareBoost, METH_VARARGS,
      "(f) - Set Material's number of stars in halo - [3, 50]"},
  {"setNLines", (PyCFunction)Material_setFlareBoost, METH_VARARGS,
      "(f) - Set Material's number of lines in halo - [0, 250]"},
  {"setNRings", (PyCFunction)Material_setNRings, METH_VARARGS,
      "(f) - Set Material's number of rings in halo - [0, 24]"},
  {0}
};

/*****************************************************************************/
/* Python Material_Type callback function prototypes:                        */
/*****************************************************************************/
static void MaterialDeAlloc (C_Material *self);
static int MaterialPrint (C_Material *self, FILE *fp, int flags);
static int MaterialSetAttr (C_Material *self, char *name, PyObject *v);
static PyObject *MaterialGetAttr (C_Material *self, char *name);
static PyObject *MaterialRepr (C_Material *self);
static PyObject *Material_createPyObject (Material *mat);

/*****************************************************************************/
/* Python Material_Type structure definition:                                */
/*****************************************************************************/
PyTypeObject Material_Type =
{
  PyObject_HEAD_INIT(&PyType_Type)
  0,                                      /* ob_size */
  "Material",                             /* tp_name */
  sizeof (C_Material),                    /* tp_basicsize */
  0,                                      /* tp_itemsize */
  /* methods */
  (destructor)MaterialDeAlloc,            /* tp_dealloc */
  (printfunc)MaterialPrint,               /* tp_print */
  (getattrfunc)MaterialGetAttr,           /* tp_getattr */
  (setattrfunc)MaterialSetAttr,           /* tp_setattr */
  0,                                      /* tp_compare */
  (reprfunc)MaterialRepr,                 /* tp_repr */
  0,                                      /* tp_as_number */
  0,                                      /* tp_as_sequence */
  0,                                      /* tp_as_mapping */
  0,                                      /* tp_as_hash */
  0,0,0,0,0,0,
  0,                                      /* tp_doc */ 
  0,0,0,0,0,0,
  C_Material_methods,                     /* tp_methods */
  0,                                      /* tp_members */
};

#endif /* EXPP_MATERIAL_H */
