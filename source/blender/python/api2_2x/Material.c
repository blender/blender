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
 * Contributor(s): Willian P. Germano, Michel Selten
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_library.h>
#include <MEM_guardedalloc.h>
#include <DNA_ID.h>
#include <BLI_blenlib.h>

#include "constant.h"
#include "gen_utils.h"

#include "Material.h"

/*****************************************************************************/
/* Python BPy_Material defaults:																						 */
/*****************************************************************************/
#define EXPP_MAT_MODE_TRACEABLE						MA_TRACEBLE
#define EXPP_MAT_MODE_SHADOW							MA_SHADOW
#define EXPP_MAT_MODE_SHADELESS						MA_SHLESS
#define EXPP_MAT_MODE_WIRE								MA_WIRE
#define EXPP_MAT_MODE_VCOL_LIGHT					MA_VERTEXCOL
#define EXPP_MAT_MODE_HALO								MA_HALO
#define EXPP_MAT_MODE_ZTRANSP							MA_ZTRA
#define EXPP_MAT_MODE_VCOL_PAINT					MA_VERTEXCOLP
#define EXPP_MAT_MODE_ZINVERT							MA_ZINV
#define EXPP_MAT_MODE_HALORINGS						MA_HALO_RINGS
#define EXPP_MAT_MODE_ENV									MA_ENV
#define EXPP_MAT_MODE_HALOLINES						MA_HALO_LINES
#define EXPP_MAT_MODE_ONLYSHADOW					MA_ONLYSHADOW
#define EXPP_MAT_MODE_HALOXALPHA					MA_HALO_XALPHA
#define EXPP_MAT_MODE_HALOSTAR						MA_STAR
#define EXPP_MAT_MODE_TEXFACE							MA_FACETEXTURE
#define EXPP_MAT_MODE_HALOTEX							MA_HALOTEX
#define EXPP_MAT_MODE_HALOPUNO						MA_HALOPUNO
#define EXPP_MAT_MODE_NOMIST							MA_NOMIST
#define EXPP_MAT_MODE_HALOSHADE						MA_HALO_SHADE
#define EXPP_MAT_MODE_HALOFLARE						MA_HALO_FLARE
#define EXPP_MAT_MODE_RADIO								MA_RADIO

/* Material MIN, MAX values */
#define EXPP_MAT_ADD_MIN					 0.0
#define EXPP_MAT_ADD_MAX					 1.0
#define EXPP_MAT_ALPHA_MIN				 0.0
#define EXPP_MAT_ALPHA_MAX				 1.0
#define EXPP_MAT_AMB_MIN					 0.0
#define EXPP_MAT_AMB_MAX					 1.0
#define EXPP_MAT_COL_MIN					 0.0 /* min/max for all ... */
#define EXPP_MAT_COL_MAX					 1.0 /* ... color triplets	*/
#define EXPP_MAT_EMIT_MIN					 0.0
#define EXPP_MAT_EMIT_MAX					 1.0
#define EXPP_MAT_REF_MIN					 0.0
#define EXPP_MAT_REF_MAX					 1.0
#define EXPP_MAT_SPEC_MIN					 0.0
#define EXPP_MAT_SPEC_MAX					 2.0
#define EXPP_MAT_SPECTRA_MIN			 0.0
#define EXPP_MAT_SPECTRA_MAX			 1.0
#define EXPP_MAT_ZOFFS_MIN				 0.0
#define EXPP_MAT_ZOFFS_MAX				10.0
#define EXPP_MAT_HALOSIZE_MIN			 0.0
#define EXPP_MAT_HALOSIZE_MAX		 100.0
#define EXPP_MAT_FLARESIZE_MIN		 0.1
#define EXPP_MAT_FLARESIZE_MAX		25.0
#define EXPP_MAT_FLAREBOOST_MIN		 0.1
#define EXPP_MAT_FLAREBOOST_MAX		10.0
#define EXPP_MAT_SUBSIZE_MIN			 0.1
#define EXPP_MAT_SUBSIZE_MAX			25.0

#define EXPP_MAT_HARD_MIN				 1
#define EXPP_MAT_HARD_MAX			 255	/* 127 with MODE HALO ON */
#define EXPP_MAT_HALOSEED_MIN		 1
#define EXPP_MAT_HALOSEED_MAX  255
#define EXPP_MAT_NFLARES_MIN		 1
#define EXPP_MAT_NFLARES_MAX		32
#define EXPP_MAT_FLARESEED_MIN	 1
#define EXPP_MAT_FLARESEED_MAX 255
#define EXPP_MAT_NSTARS_MIN			 3
#define EXPP_MAT_NSTARS_MAX			50
#define EXPP_MAT_NLINES_MIN			 0
#define EXPP_MAT_NLINES_MAX		 250
#define EXPP_MAT_NRINGS_MIN			 0
#define EXPP_MAT_NRINGS_MAX			24

/*****************************************************************************/
/* Python API function prototypes for the Material module.									 */
/*****************************************************************************/
static PyObject *M_Material_New (PyObject *self, PyObject *args,
															 PyObject *keywords);
static PyObject *M_Material_Get (PyObject *self, PyObject *args);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.			 */
/* In Python these will be written to the console when doing a							 */
/* Blender.Material.__doc__																									 */
/*****************************************************************************/
static char M_Material_doc[] =
"The Blender Material module";

static char M_Material_New_doc[] =
"(name) - return a new material called 'name'\n\
() - return a new material called 'Mat'";

static char M_Material_Get_doc[] =
"(name) - return the material called 'name', None if not found.\n\
() - return a list of all materials in the current scene.";

/*****************************************************************************/
/* Python method structure definition for Blender.Material module:					 */
/*****************************************************************************/
struct PyMethodDef M_Material_methods[] = {
	{"New",(PyCFunction)M_Material_New, METH_VARARGS|METH_KEYWORDS,
					M_Material_New_doc},
	{"Get",					M_Material_Get,					METH_VARARGS, M_Material_Get_doc},
	{"get",					M_Material_Get,					METH_VARARGS, M_Material_Get_doc},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Function:							M_Material_New																		 */
/* Python equivalent:			Blender.Material.New															 */
/*****************************************************************************/
static PyObject *M_Material_New(PyObject *self, PyObject *args,
								PyObject *keywords)
{
	char				*name = "Mat";
	static char *kwlist[] = {"name", NULL};
	BPy_Material	*pymat; /* for Material Data object wrapper in Python */
	Material			*blmat; /* for actual Material Data we create in Blender */
	char				buf[21];

	if (!PyArg_ParseTupleAndKeywords(args, keywords, "|s", kwlist, &name))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected string or nothing as argument"));

	if (strcmp(name, "Mat") != 0) /* use gave us a name ?*/
		PyOS_snprintf(buf, sizeof(buf), "%s", name);

	blmat = add_material(name); /* first create the Material Data in Blender */

	if (blmat) /* now create the wrapper obj in Python */
		pymat = (BPy_Material *)Material_CreatePyObject (blmat);
	else
		return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
														"couldn't create Material Data in Blender"));

	blmat->id.us = 0; /* was incref'ed by add_material() above */

	if (pymat == NULL)
		return (EXPP_ReturnPyObjError (PyExc_MemoryError,
														"couldn't create Material Data object"));

	return (PyObject *)pymat;
}

/*****************************************************************************/
/* Function:							M_Material_Get																		 */
/* Python equivalent:			Blender.Material.Get															 */
/* Description:						Receives a string and returns the material whose	 */
/*												name matches the string.	If no argument is				 */
/*												passed in, a list with all materials in the				 */
/*												current scene is returned.												 */
/*****************************************************************************/
static PyObject *M_Material_Get(PyObject *self, PyObject *args)
{
	char	 *name = NULL;
	Material *mat_iter;

	if (!PyArg_ParseTuple(args, "|s", &name))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected string argument (or nothing)"));

	mat_iter = G.main->mat.first;

	if (name) { /* (name) - Search material by name */

		BPy_Material *wanted_mat = NULL;

		while ((mat_iter) && (wanted_mat == NULL)) {

			if (strcmp (name, mat_iter->id.name+2) == 0)
				wanted_mat = (BPy_Material *)Material_CreatePyObject (mat_iter);

			mat_iter = mat_iter->id.next;
		}

		if (wanted_mat == NULL) { /* Requested material doesn't exist */
			char error_msg[64];
			PyOS_snprintf(error_msg, sizeof(error_msg),
											"Material \"%s\" not found", name);
			return (EXPP_ReturnPyObjError (PyExc_NameError, error_msg));
		}

		return (PyObject *)wanted_mat;
	}

	else { /* () - return a list with all materials in the scene */
		int index = 0;
		PyObject *matlist, *pyobj;

		matlist = PyList_New (BLI_countlist (&(G.main->mat)));

		if (!matlist)
			return (PythonReturnErrorObject (PyExc_MemoryError,
							"couldn't create PyList"));

		while (mat_iter) {
			pyobj = Material_CreatePyObject (mat_iter);

			if (!pyobj)
				return (PythonReturnErrorObject (PyExc_MemoryError,
									"couldn't create PyObject"));

			PyList_SET_ITEM (matlist, index, pyobj);

			mat_iter = mat_iter->id.next;
			index++;
		}

		return matlist;
	}
}

static PyObject *Lamp_ModesDict (void)
{
	PyObject *Modes = M_constant_New();

#undef EXPP_ADDCONST
#define EXPP_ADDCONST(name) \
	constant_insert(c, #name, PyInt_FromLong(EXPP_MAT_MODE_##name))

/* So that:
 * EXPP_ADDCONST(TRACEABLE) becomes:
 * constant_insert(c, "TRACEABLE", PyInt_FromLong(EXPP_MAT_MODE_TRACEABLE))
 */

	if (Modes) {
		BPy_constant *c = (BPy_constant *)Modes;
		
		EXPP_ADDCONST(TRACEABLE);
		EXPP_ADDCONST(SHADOW);
		EXPP_ADDCONST(SHADELESS);
		EXPP_ADDCONST(WIRE);
		EXPP_ADDCONST(VCOL_LIGHT);
		EXPP_ADDCONST(HALO);
		EXPP_ADDCONST(ZTRANSP);
		EXPP_ADDCONST(VCOL_PAINT);
		EXPP_ADDCONST(ZINVERT);
		EXPP_ADDCONST(HALORINGS);
		EXPP_ADDCONST(ENV);
		EXPP_ADDCONST(HALOLINES);
		EXPP_ADDCONST(ONLYSHADOW);
		EXPP_ADDCONST(HALOXALPHA);
		EXPP_ADDCONST(HALOSTAR);
		EXPP_ADDCONST(TEXFACE);
		EXPP_ADDCONST(HALOTEX);
		EXPP_ADDCONST(HALOPUNO);
		EXPP_ADDCONST(NOMIST);
		EXPP_ADDCONST(HALOSHADE);
		EXPP_ADDCONST(HALOFLARE);
		EXPP_ADDCONST(RADIO);
	}

	return Modes;
}

/*****************************************************************************/
/* Function:							Material_Init																			 */
/*****************************************************************************/
PyObject *Material_Init (void)
{
	PyObject	*submodule, *Modes;

	Material_Type.ob_type = &PyType_Type;

	Modes = Lamp_ModesDict ();

	submodule = Py_InitModule3("Blender.Material",
									M_Material_methods, M_Material_doc);

	if (Modes) PyModule_AddObject(submodule, "Modes", Modes);
	return (submodule);
}

/***************************/
/*** The Material PyType ***/
/***************************/

/*****************************************************************************/
/* Python BPy_Material methods declarations:																 */
/*****************************************************************************/
static PyObject *Material_getName(BPy_Material *self);
static PyObject *Material_getMode(BPy_Material *self);
static PyObject *Material_getRGBCol(BPy_Material *self);
/*static PyObject *Material_getAmbCol(BPy_Material *self);*/
static PyObject *Material_getSpecCol(BPy_Material *self);
static PyObject *Material_getMirCol(BPy_Material *self);
static PyObject *Material_getAmb(BPy_Material *self);
static PyObject *Material_getEmit(BPy_Material *self);
static PyObject *Material_getAlpha(BPy_Material *self);
static PyObject *Material_getRef(BPy_Material *self);
static PyObject *Material_getSpec(BPy_Material *self);
static PyObject *Material_getSpecTransp(BPy_Material *self);
static PyObject *Material_getAdd(BPy_Material *self);
static PyObject *Material_getZOffset(BPy_Material *self);
static PyObject *Material_getHaloSize(BPy_Material *self);
static PyObject *Material_getHaloSeed(BPy_Material *self);
static PyObject *Material_getFlareSize(BPy_Material *self);
static PyObject *Material_getFlareSeed(BPy_Material *self);
static PyObject *Material_getFlareBoost(BPy_Material *self);
static PyObject *Material_getSubSize(BPy_Material *self);
static PyObject *Material_getHardness(BPy_Material *self);
static PyObject *Material_getNFlares(BPy_Material *self);
static PyObject *Material_getNStars(BPy_Material *self);
static PyObject *Material_getNLines(BPy_Material *self);
static PyObject *Material_getNRings(BPy_Material *self);
static PyObject *Material_setName(BPy_Material *self, PyObject *args);
static PyObject *Material_setMode(BPy_Material *self, PyObject *args);
static PyObject *Material_setIntMode(BPy_Material *self, PyObject *args);
static PyObject *Material_setRGBCol(BPy_Material *self, PyObject *args);
/*static PyObject *Material_setAmbCol(BPy_Material *self, PyObject *args);*/
static PyObject *Material_setSpecCol(BPy_Material *self, PyObject *args);
static PyObject *Material_setMirCol(BPy_Material *self, PyObject *args);
static PyObject *Material_setAmb(BPy_Material *self, PyObject *args);
static PyObject *Material_setEmit(BPy_Material *self, PyObject *args);
static PyObject *Material_setAlpha(BPy_Material *self, PyObject *args);
static PyObject *Material_setRef(BPy_Material *self, PyObject *args);
static PyObject *Material_setSpec(BPy_Material *self, PyObject *args);
static PyObject *Material_setSpecTransp(BPy_Material *self, PyObject *args);
static PyObject *Material_setAdd(BPy_Material *self, PyObject *args);
static PyObject *Material_setZOffset(BPy_Material *self, PyObject *args);
static PyObject *Material_setHaloSize(BPy_Material *self, PyObject *args);
static PyObject *Material_setHaloSeed(BPy_Material *self, PyObject *args);
static PyObject *Material_setFlareSize(BPy_Material *self, PyObject *args);
static PyObject *Material_setFlareSeed(BPy_Material *self, PyObject *args);
static PyObject *Material_setFlareBoost(BPy_Material *self, PyObject *args);
static PyObject *Material_setSubSize(BPy_Material *self, PyObject *args);
static PyObject *Material_setHardness(BPy_Material *self, PyObject *args);
static PyObject *Material_setNFlares(BPy_Material *self, PyObject *args);
static PyObject *Material_setNStars(BPy_Material *self, PyObject *args);
static PyObject *Material_setNLines(BPy_Material *self, PyObject *args);
static PyObject *Material_setNRings(BPy_Material *self, PyObject *args);

static PyObject *Material_setColorComponent(BPy_Material *self, char *key,
								PyObject *args);

/*****************************************************************************/
/* Python BPy_Material methods table:																				 */
/*****************************************************************************/
static PyMethodDef BPy_Material_methods[] = {
 /* name, method, flags, doc */
	{"getName", (PyCFunction)Material_getName, METH_NOARGS,
			"() - Return Material Data name"},
	{"getMode", (PyCFunction)Material_getMode, METH_NOARGS,
			"() - Return Material mode flags"},
	{"getRGBCol", (PyCFunction)Material_getRGBCol, METH_NOARGS,
			"() - Return Material's rgb color triplet"},
/*	{"getAmbCol", (PyCFunction)Material_getAmbCol, METH_NOARGS,
			"() - Return Material's ambient color"},*/
	{"getSpecCol", (PyCFunction)Material_getSpecCol, METH_NOARGS,
			"() - Return Material's specular color"},
	{"getMirCol", (PyCFunction)Material_getMirCol, METH_NOARGS,
			"() - Return Material's mirror color"},
	{"getAmb", (PyCFunction)Material_getAmb, METH_NOARGS,
			"() - Return Material's ambient color blend factor"},
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
			"() - Return Material's artificial offset for faces"},
	{"getHaloSize", (PyCFunction)Material_getHaloSize, METH_NOARGS,
			"() - Return Material's halo size"},
	{"getHaloSeed", (PyCFunction)Material_getHaloSeed, METH_NOARGS,
			"() - Return Material's seed for random ring dimension and line "
			"location in halos"},
	{"getFlareSize", (PyCFunction)Material_getFlareSize, METH_NOARGS,
			"() - Return Material's (flare size)/(halo size) factor"},
	{"getFlareSeed", (PyCFunction)Material_getFlareSeed, METH_NOARGS,
			"() - Return Material's flare offset in the seed table"},
	{"getFlareBoost", (PyCFunction)Material_getFlareBoost, METH_NOARGS,
			"() - Return Material's flare boost"},
	{"getSubSize", (PyCFunction)Material_getSubSize, METH_NOARGS,
			"() - Return Material's dimension of subflare, dots and circles"},
	{"getHardness", (PyCFunction)Material_getHardness, METH_NOARGS,
			"() - Return Material's specular hardness"},
	{"getNFlares", (PyCFunction)Material_getNFlares, METH_NOARGS,
			"() - Return Material's number of flares in halo"},
	{"getNStars", (PyCFunction)Material_getNStars, METH_NOARGS,
			"() - Return Material's number of points in the halo stars"},
	{"getNLines", (PyCFunction)Material_getNLines, METH_NOARGS,
			"() - Return Material's number of lines in halo"},
	{"getNRings", (PyCFunction)Material_getNRings, METH_NOARGS,
			"() - Return Material's number of rings in halo"},
	{"setName", (PyCFunction)Material_setName, METH_VARARGS,
			"(s) - Change Material Data name"},
	{"setMode", (PyCFunction)Material_setMode, METH_VARARGS,
			"([s[,s]]) - Set Material mode flag(s)"},
	{"setRGBCol", (PyCFunction)Material_setRGBCol, METH_VARARGS,
			"(f,f,f or [f,f,f]) - Set Material's rgb color triplet"},
/*	{"setAmbCol", (PyCFunction)Material_setAmbCol, METH_VARARGS,
			"(f,f,f or [f,f,f]) - Set Material's ambient color"},*/
	{"setSpecCol", (PyCFunction)Material_setSpecCol, METH_VARARGS,
			"(f,f,f or [f,f,f]) - Set Material's specular color"},
	{"setMirCol", (PyCFunction)Material_setMirCol, METH_VARARGS,
			"(f,f,f or [f,f,f]) - Set Material's mirror color"},
	{"setAmb", (PyCFunction)Material_setAmb, METH_VARARGS,
			"(f) - Set how much the Material's color is affected"
							" by \nthe global ambient colors - [0.0, 1.0]"},
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
	{"setHaloSeed", (PyCFunction)Material_setHaloSeed, METH_VARARGS,
			"(i) - Set Material's halo seed - [0, 255]"},
	{"setFlareSize", (PyCFunction)Material_setFlareSize, METH_VARARGS,
			"(f) - Set Material's factor: (flare size)/(halo size) - [0.1, 25.0]"},
	{"setFlareSeed", (PyCFunction)Material_setFlareSeed, METH_VARARGS,
			"(i) - Set Material's flare seed - [0, 255]"},
	{"setFlareBoost", (PyCFunction)Material_setFlareBoost, METH_VARARGS,
			"(f) - Set Material's flare boost - [0.1, 10.0]"},
	{"setSubSize", (PyCFunction)Material_setSubSize, METH_VARARGS,
			"(f) - Set Material's dimension of subflare,"
							" dots and circles - [0.1, 25.0]"},
	{"setHardness", (PyCFunction)Material_setHardness, METH_VARARGS,
			"(i) - Set Material's hardness - [1, 255 (127 if halo mode is ON)]"},
	{"setNFlares", (PyCFunction)Material_setNFlares, METH_VARARGS,
			"(i) - Set Material's number of flares in halo - [1, 32]"},
	{"setNStars", (PyCFunction)Material_setNStars, METH_VARARGS,
			"(i) - Set Material's number of stars in halo - [3, 50]"},
	{"setNLines", (PyCFunction)Material_setNLines, METH_VARARGS,
			"(i) - Set Material's number of lines in halo - [0, 250]"},
	{"setNRings", (PyCFunction)Material_setNRings, METH_VARARGS,
			"(i) - Set Material's number of rings in halo - [0, 24]"},
	{0}
};

/*****************************************************************************/
/* Python Material_Type callback function prototypes:												 */
/*****************************************************************************/
static void Material_dealloc (BPy_Material *self);
static int Material_setAttr (BPy_Material *self, char *name, PyObject *v);
static PyObject *Material_getAttr (BPy_Material *self, char *name);
static PyObject *Material_repr (BPy_Material *self);

/*****************************************************************************/
/* Python Material_Type structure definition:																 */
/*****************************************************************************/
PyTypeObject Material_Type =
{
	PyObject_HEAD_INIT(NULL)
	0,																			/* ob_size */
	"Blender Material",											/* tp_name */
	sizeof (BPy_Material),									/* tp_basicsize */
	0,																			/* tp_itemsize */
	/* methods */
	(destructor)Material_dealloc,						/* tp_dealloc */
	0,																			/* tp_print */
	(getattrfunc)Material_getAttr,					/* tp_getattr */
	(setattrfunc)Material_setAttr,					/* tp_setattr */
	0,																			/* tp_compare */
	(reprfunc)Material_repr,								/* tp_repr */
	0,																			/* tp_as_number */
	0,																			/* tp_as_sequence */
	0,																			/* tp_as_mapping */
	0,																			/* tp_as_hash */
	0,0,0,0,0,0,
	0,																			/* tp_doc */ 
	0,0,0,0,0,0,
	BPy_Material_methods,											/* tp_methods */
	0,																			/* tp_members */
};

/*****************************************************************************/
/* Function:		Material_dealloc																						 */
/* Description: This is a callback function for the BPy_Material type. It is */
/*							the destructor function.																		 */
/*****************************************************************************/
static void Material_dealloc (BPy_Material *self)
{
	Py_DECREF (self->col);
	Py_DECREF (self->amb);
	Py_DECREF (self->spec);
	Py_DECREF (self->mir);
	PyObject_DEL (self);
}

/*****************************************************************************/
/* Function:		Material_CreatePyObject																			 */
/* Description: This function will create a new BPy_Material from an existing*/
/*							Blender material structure.																	 */
/*****************************************************************************/
PyObject *Material_CreatePyObject (struct Material *mat)
{
	BPy_Material *pymat;
	float *col[3], *amb[3], *spec[3], *mir[3];

	pymat = (BPy_Material *)PyObject_NEW (BPy_Material, &Material_Type);

	if (!pymat)
		return EXPP_ReturnPyObjError (PyExc_MemoryError,
						"couldn't create BPy_Material object");

	pymat->material = mat;

	col[0] = &mat->r;
	col[1] = &mat->g;
	col[2] = &mat->b;

	amb[0] = &mat->ambr;
	amb[1] = &mat->ambg;
	amb[2] = &mat->ambb;

	spec[0] = &mat->specr;
	spec[1] = &mat->specg;
	spec[2] = &mat->specb;

	mir[0] = &mat->mirr;
	mir[1] = &mat->mirg;
	mir[2] = &mat->mirb;

	pymat->col	= (BPy_rgbTuple *)rgbTuple_New(col);
	pymat->amb	= (BPy_rgbTuple *)rgbTuple_New(amb);
	pymat->spec = (BPy_rgbTuple *)rgbTuple_New(spec);
	pymat->mir	= (BPy_rgbTuple *)rgbTuple_New(mir);

	return (PyObject *)pymat;
}

/*****************************************************************************/
/* Function:		Material_CheckPyObject																			 */
/* Description: This function returns true when the given PyObject is of the */
/*							type Material. Otherwise it will return false.							 */
/*****************************************************************************/
int Material_CheckPyObject (PyObject *pyobj)
{
	return (pyobj->ob_type == &Material_Type);
}

/*****************************************************************************/
/* Function:		Material_FromPyObject																				 */
/* Description: This function returns the Blender material from the given		 */
/*							PyObject.																										 */
/*****************************************************************************/
Material *Material_FromPyObject (PyObject *pyobj)
{
	return ((BPy_Material *)pyobj)->material;
}

/*****************************************************************************/
/* Description: Returns the object with the name specified by the argument	 */
/*							name. Note that the calling function has to remove the first */
/*							two characters of the object name. These two characters			 */
/*							specify the type of the object (OB, ME, WO, ...)						 */
/*							The function will return NULL when no object with the given  */
/*							name is found.																							 */
/*****************************************************************************/
Material * GetMaterialByName (char * name)
{
	Material	* mat_iter;

	mat_iter = G.main->mat.first;
	while (mat_iter)
	{
		if (StringEqual (name, GetIdName (&(mat_iter->id))))
		{
			return (mat_iter);
		}
		mat_iter = mat_iter->id.next;
	}

	/* There is no material with the given name */
	return (NULL);
}

/*****************************************************************************/
/* Python BPy_Material methods:																								 */
/*****************************************************************************/
static PyObject *Material_getName(BPy_Material *self)
{
	PyObject *attr = PyString_FromString(self->material->id.name+2);

	if (attr) return attr;

	return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
																	 "couldn't get Material.name attribute"));
}

static PyObject *Material_getMode(BPy_Material *self)
{
	PyObject *attr = PyInt_FromLong((long)self->material->mode);

	if (attr) return attr;

	return EXPP_ReturnPyObjError (PyExc_RuntimeError,
																	 "couldn't get Material.Mode attribute");
}

static PyObject *Material_getRGBCol(BPy_Material *self)
{
	return rgbTuple_getCol(self->col);
}
/*
static PyObject *Material_getAmbCol(BPy_Material *self)
{
	return rgbTuple_getCol(self->amb);
}
*/
static PyObject *Material_getSpecCol(BPy_Material *self)
{
	return rgbTuple_getCol(self->spec);
}

static PyObject *Material_getMirCol(BPy_Material *self)
{
	return rgbTuple_getCol(self->mir);
}

static PyObject *Material_getAmb(BPy_Material *self)
{
	PyObject *attr = PyFloat_FromDouble((double)self->material->amb);

	if (attr) return attr;

	return EXPP_ReturnPyObjError (PyExc_RuntimeError,
																	 "couldn't get Material.amb attribute");
}

static PyObject *Material_getEmit(BPy_Material *self)
{
	PyObject *attr = PyFloat_FromDouble((double)self->material->emit);

	if (attr) return attr;

	return EXPP_ReturnPyObjError (PyExc_RuntimeError,
																	 "couldn't get Material.emit attribute");
}

static PyObject *Material_getAlpha(BPy_Material *self)
{
	PyObject *attr = PyFloat_FromDouble((double)self->material->alpha);

	if (attr) return attr;

	return EXPP_ReturnPyObjError (PyExc_RuntimeError,
																	 "couldn't get Material.alpha attribute");
}

static PyObject *Material_getRef(BPy_Material *self)
{
	PyObject *attr = PyFloat_FromDouble((double)self->material->ref);

	if (attr) return attr;

	return EXPP_ReturnPyObjError (PyExc_RuntimeError,
																	 "couldn't get Material.ref attribute");
}

static PyObject *Material_getSpec(BPy_Material *self)
{
	PyObject *attr = PyFloat_FromDouble((double)self->material->spec);

	if (attr) return attr;

	return EXPP_ReturnPyObjError (PyExc_RuntimeError,
																	 "couldn't get Material.spec attribute");
}

static PyObject *Material_getSpecTransp(BPy_Material *self)
{
	PyObject *attr = PyFloat_FromDouble((double)self->material->spectra);

	if (attr) return attr;

	return EXPP_ReturnPyObjError (PyExc_RuntimeError,
							"couldn't get Material.specTransp attribute");
}

static PyObject *Material_getAdd(BPy_Material *self)
{
	PyObject *attr = PyFloat_FromDouble((double)self->material->add);

	if (attr) return attr;

	return EXPP_ReturnPyObjError (PyExc_RuntimeError,
																	 "couldn't get Material.add attribute");
}

static PyObject *Material_getZOffset(BPy_Material *self)
{
	PyObject *attr = PyFloat_FromDouble((double)self->material->zoffs);

	if (attr) return attr;

	return EXPP_ReturnPyObjError (PyExc_RuntimeError,
												"couldn't get Material.zOffset attribute");
}

static PyObject *Material_getHaloSize(BPy_Material *self)
{
	PyObject *attr = PyFloat_FromDouble((double)self->material->hasize);

	if (attr) return attr;

	return EXPP_ReturnPyObjError (PyExc_RuntimeError,
									 "couldn't get Material.haloSize attribute");
}

static PyObject *Material_getFlareSize(BPy_Material *self)
{
	PyObject *attr = PyFloat_FromDouble((double)self->material->flaresize);

	if (attr) return attr;

	return EXPP_ReturnPyObjError (PyExc_RuntimeError,
										 "couldn't get Material.flareSize attribute");
}

static PyObject *Material_getFlareBoost(BPy_Material *self)
{
	PyObject *attr = PyFloat_FromDouble((double)self->material->flareboost);

	if (attr) return attr;

	return EXPP_ReturnPyObjError (PyExc_RuntimeError,
										 "couldn't get Material.flareBoost attribute");
}

static PyObject *Material_getSubSize(BPy_Material *self)
{
	PyObject *attr = PyFloat_FromDouble((double)self->material->subsize);

	if (attr) return attr;

	return EXPP_ReturnPyObjError (PyExc_RuntimeError,
							 "couldn't get Material.subSize attribute");
}

static PyObject *Material_getHaloSeed(BPy_Material *self)
{
	PyObject *attr = PyInt_FromLong((long)self->material->seed1);

	if (attr) return attr;

	return EXPP_ReturnPyObjError (PyExc_RuntimeError,
											 "couldn't get Material.haloSeed attribute");
}

static PyObject *Material_getFlareSeed(BPy_Material *self)
{
	PyObject *attr = PyInt_FromLong((long)self->material->seed2);

	if (attr) return attr;

	return EXPP_ReturnPyObjError (PyExc_RuntimeError,
											 "couldn't get Material.flareSeed attribute");
}

static PyObject *Material_getHardness(BPy_Material *self)
{
	PyObject *attr = PyInt_FromLong((long)self->material->har);

	if (attr) return attr;

	return EXPP_ReturnPyObjError (PyExc_RuntimeError,
											 "couldn't get Material.hard attribute");
}

static PyObject *Material_getNFlares(BPy_Material *self)
{
	PyObject *attr = PyInt_FromLong((long)self->material->flarec);

	if (attr) return attr;

	return EXPP_ReturnPyObjError (PyExc_RuntimeError,
											 "couldn't get Material.nFlares attribute");
}

static PyObject *Material_getNStars(BPy_Material *self)
{
	PyObject *attr = PyInt_FromLong((long)self->material->starc);

	if (attr) return attr;

	return EXPP_ReturnPyObjError (PyExc_RuntimeError,
											 "couldn't get Material.nStars attribute");
}

static PyObject *Material_getNLines(BPy_Material *self)
{
	PyObject *attr = PyInt_FromLong((long)self->material->linec);

	if (attr) return attr;

	return EXPP_ReturnPyObjError (PyExc_RuntimeError,
											 "couldn't get Material.nLines attribute");
}

static PyObject *Material_getNRings(BPy_Material *self)
{
	PyObject *attr = PyInt_FromLong((long)self->material->ringc);

	if (attr) return attr;

	return EXPP_ReturnPyObjError (PyExc_RuntimeError,
											 "couldn't get Material.nRings attribute");
}

static PyObject *Material_setName(BPy_Material *self, PyObject *args)
{
	char *name;
	char buf[21];

	if (!PyArg_ParseTuple(args, "s", &name))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
																		 "expected string argument"));

	PyOS_snprintf(buf, sizeof(buf), "%s", name);

	rename_id(&self->material->id, buf);

	Py_INCREF(Py_None);
	return Py_None;
}

/* Possible modes are traceable, shadow, shadeless, wire, vcolLight,
 * vcolPaint, halo, ztransp, zinvert, haloRings, env, haloLines,
 * onlyShadow, xalpha, star, faceTexture, haloTex, haloPuno, noMist,
 * haloShaded, haloFlare */
static PyObject *Material_setMode(BPy_Material *self, PyObject *args)
{
	int i, flag = 0;
	char *m[22] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL,
								 NULL, NULL, NULL, NULL, NULL, NULL, NULL,
								 NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

	if (!PyArg_ParseTuple(args, "|ssssssssssssssssssssss",
					&m[0], &m[1], &m[2], &m[3],  &m[4],  &m[5],  &m[6],
					&m[7], &m[8], &m[9], &m[10], &m[11], &m[12], &m[13],
					&m[14], &m[15], &m[16], &m[17], &m[18], &m[19], &m[20], &m[21]))
	{
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected from none to 22 string argument(s)"));
	}

	for (i = 0; i < 22; i++) {
		if (m[i] == NULL) break;
		if (strcmp(m[i], "Traceable") == 0)
			flag |= EXPP_MAT_MODE_TRACEABLE;
		else if (strcmp(m[i], "Shadow") == 0)
			flag |= EXPP_MAT_MODE_SHADOW;
		else if (strcmp(m[i], "Shadeless") == 0)
			flag |= EXPP_MAT_MODE_SHADELESS;
		else if (strcmp(m[i], "Wire") == 0)
			flag |= EXPP_MAT_MODE_WIRE;
		else if (strcmp(m[i], "VColLight") == 0)
			flag |= EXPP_MAT_MODE_VCOL_LIGHT;
		else if (strcmp(m[i], "VColPaint") == 0)
			flag |= EXPP_MAT_MODE_VCOL_PAINT;
		else if (strcmp(m[i], "Halo") == 0)
			flag |= EXPP_MAT_MODE_HALO;
		else if (strcmp(m[i], "ZTransp") == 0)
			flag |= EXPP_MAT_MODE_ZTRANSP;
		else if (strcmp(m[i], "ZInvert") == 0)
			flag |= EXPP_MAT_MODE_ZINVERT;
		else if (strcmp(m[i], "HaloRings") == 0)
			flag |= EXPP_MAT_MODE_HALORINGS;
		else if (strcmp(m[i], "Env") == 0)
			flag |= EXPP_MAT_MODE_ENV;
		else if (strcmp(m[i], "HaloLines") == 0)
			flag |= EXPP_MAT_MODE_HALOLINES;
		else if (strcmp(m[i], "OnlyShadow") == 0)
			flag |= EXPP_MAT_MODE_ONLYSHADOW;
		else if (strcmp(m[i], "HaloXAlpha") == 0)
			flag |= EXPP_MAT_MODE_HALOXALPHA;
		else if (strcmp(m[i], "HaloStar") == 0)
			flag |= EXPP_MAT_MODE_HALOSTAR;
		else if (strcmp(m[i], "TexFace") == 0)
			flag |= EXPP_MAT_MODE_TEXFACE;
		else if (strcmp(m[i], "HaloTex") == 0)
			flag |= EXPP_MAT_MODE_HALOTEX;
		else if (strcmp(m[i], "HaloPuno") == 0)
			flag |= EXPP_MAT_MODE_HALOPUNO;
		else if (strcmp(m[i], "NoMist") == 0)
			flag |= EXPP_MAT_MODE_NOMIST;
		else if (strcmp(m[i], "HaloShaded") == 0)
			flag |= EXPP_MAT_MODE_HALOSHADE;
		else if (strcmp(m[i], "HaloFlare") == 0)
			flag |= EXPP_MAT_MODE_HALOFLARE;
		else if (strcmp(m[i], "Radio") == 0)
			flag |= EXPP_MAT_MODE_RADIO;
		else
			return (EXPP_ReturnPyObjError (PyExc_AttributeError,
							"unknown Material mode argument"));
	}

	self->material->mode = flag;

	Py_INCREF(Py_None);
	return Py_None;
}

/* Another helper function, for the same reason.
 * (See comment before Material_setIntType above). */
static PyObject *Material_setIntMode(BPy_Material *self, PyObject *args)
{
	int value;

	if (!PyArg_ParseTuple(args, "i", &value))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
																		 "expected int argument"));

	self->material->mode = value;

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Material_setRGBCol (BPy_Material *self, PyObject *args)
{
	return rgbTuple_setCol(self->col, args);
}
/*
static PyObject *Material_setAmbCol (BPy_Material *self, PyObject *args)
{
	return rgbTuple_setCol(self->amb, args);
}
*/
static PyObject *Material_setSpecCol (BPy_Material *self, PyObject *args)
{
	return rgbTuple_setCol(self->spec, args);
}

static PyObject *Material_setMirCol (BPy_Material *self, PyObject *args)
{
	return rgbTuple_setCol(self->mir, args);
}

static PyObject *Material_setColorComponent(BPy_Material *self, char *key,
								PyObject *args)
{ /* for compatibility with old bpython */
	float value;

	if (!PyArg_ParseTuple(args, "f", &value))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected float argument in [0.0, 1.0]"));

	value = EXPP_ClampFloat (value, EXPP_MAT_COL_MIN,
									EXPP_MAT_COL_MAX);

	if (!strcmp(key, "R"))
		self->material->r = value;
	else if (!strcmp(key, "G"))
		self->material->g = value;
	else if (!strcmp(key, "B"))
		self->material->b = value;
	else if (!strcmp(key, "specR"))
		self->material->specr = value;
	else if (!strcmp(key, "specG"))
		self->material->specg = value;
	else if (!strcmp(key, "specB"))
		self->material->specb = value;

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setAmb(BPy_Material *self, PyObject *args)
{
	float value;

	if (!PyArg_ParseTuple(args, "f", &value))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected float argument in [0.0, 1.0]"));

	self->material->amb = EXPP_ClampFloat (value, EXPP_MAT_AMB_MIN,
									EXPP_MAT_AMB_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setEmit(BPy_Material *self, PyObject *args)
{
	float value;

	if (!PyArg_ParseTuple(args, "f", &value))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected float argument in [0.0, 1.0]"));

	self->material->emit = EXPP_ClampFloat (value, EXPP_MAT_EMIT_MIN,
									EXPP_MAT_EMIT_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setSpecTransp(BPy_Material *self, PyObject *args)
{
	float value;

	if (!PyArg_ParseTuple(args, "f", &value))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected float argument in [0.0, 1.0]"));

	self->material->spectra = EXPP_ClampFloat (value, EXPP_MAT_SPECTRA_MIN,
									EXPP_MAT_SPECTRA_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setAlpha(BPy_Material *self, PyObject *args)
{
	float value;

	if (!PyArg_ParseTuple(args, "f", &value))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected float argument in [0.0, 1.0]"));

	self->material->alpha = EXPP_ClampFloat (value, EXPP_MAT_ALPHA_MIN,
									EXPP_MAT_ALPHA_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setRef(BPy_Material *self, PyObject *args)
{
	float value;

	if (!PyArg_ParseTuple(args, "f", &value))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected float argument in [0.0, 1.0]"));

	self->material->ref = EXPP_ClampFloat (value, EXPP_MAT_REF_MIN,
									EXPP_MAT_REF_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setSpec(BPy_Material *self, PyObject *args)
{
	float value;

	if (!PyArg_ParseTuple(args, "f", &value))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected float argument in [0.0, 1.0]"));

	self->material->spec = EXPP_ClampFloat (value, EXPP_MAT_SPEC_MIN,
									EXPP_MAT_SPEC_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setZOffset(BPy_Material *self, PyObject *args)
{
	float value;

	if (!PyArg_ParseTuple(args, "f", &value))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected float argument in [0.0, 10.0]"));

	self->material->zoffs = EXPP_ClampFloat (value, EXPP_MAT_ZOFFS_MIN,
									EXPP_MAT_ZOFFS_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setAdd(BPy_Material *self, PyObject *args)
{
	float value;

	if (!PyArg_ParseTuple(args, "f", &value))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected float argument in [0.0, 1.0]"));

	self->material->add = EXPP_ClampFloat (value, EXPP_MAT_ADD_MIN,
									EXPP_MAT_ADD_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setHaloSize(BPy_Material *self, PyObject *args)
{
	float value;

	if (!PyArg_ParseTuple(args, "f", &value))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected float argument in [0.0, 100.0]"));

	self->material->hasize = EXPP_ClampFloat (value, EXPP_MAT_HALOSIZE_MIN,
									EXPP_MAT_HALOSIZE_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setFlareSize(BPy_Material *self, PyObject *args)
{
	 float value;

	if (!PyArg_ParseTuple(args, "f", &value))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected float argument in [0.1, 25.0]"));

	self->material->flaresize = EXPP_ClampFloat (value, EXPP_MAT_FLARESIZE_MIN,
									EXPP_MAT_FLARESIZE_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setFlareBoost(BPy_Material *self, PyObject *args)
{
	 float value;

	if (!PyArg_ParseTuple(args, "f", &value))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected float argument in [0.1, 10.0]"));

	self->material->flareboost = EXPP_ClampFloat(value, EXPP_MAT_FLAREBOOST_MIN,
									EXPP_MAT_FLAREBOOST_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setSubSize(BPy_Material *self, PyObject *args)
{
	 float value;

	if (!PyArg_ParseTuple(args, "f", &value))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected float argument in [0.1, 25.0]"));

	self->material->subsize = EXPP_ClampFloat (value, EXPP_MAT_SUBSIZE_MIN,
									EXPP_MAT_SUBSIZE_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setHaloSeed(BPy_Material *self, PyObject *args)
{
	short value;

	if (!PyArg_ParseTuple(args, "h", &value))			 
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected int argument in [1, 255]"));

	self->material->seed1 = EXPP_ClampInt (value, EXPP_MAT_HALOSEED_MIN,
									EXPP_MAT_HALOSEED_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setFlareSeed(BPy_Material *self, PyObject *args)
{
	short value;

	if (!PyArg_ParseTuple(args, "h", &value))			 
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected int argument in [1, 255]"));

	self->material->seed2 = EXPP_ClampInt (value, EXPP_MAT_FLARESEED_MIN,
									EXPP_MAT_FLARESEED_MAX);

	return EXPP_incr_ret (Py_None);
}


static PyObject *Material_setHardness(BPy_Material *self, PyObject *args)
{
	short value;

	if (!PyArg_ParseTuple(args, "h", &value))			 
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected int argument in [1, 255]"));

	self->material->har = EXPP_ClampInt (value, EXPP_MAT_HARD_MIN,
									EXPP_MAT_HARD_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setNFlares(BPy_Material *self, PyObject *args)
{
	short value;

	if (!PyArg_ParseTuple(args, "h", &value))			 
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected int argument in [1, 32]"));

	self->material->flarec = EXPP_ClampInt (value, EXPP_MAT_NFLARES_MIN,
									EXPP_MAT_NFLARES_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setNStars(BPy_Material *self, PyObject *args)
{
	short value;

	if (!PyArg_ParseTuple(args, "h", &value))			 
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected int argument in [3, 50]"));

	self->material->starc = EXPP_ClampInt (value, EXPP_MAT_NSTARS_MIN,
									EXPP_MAT_NSTARS_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setNLines(BPy_Material *self, PyObject *args)
{
	short value;

	if (!PyArg_ParseTuple(args, "h", &value))			 
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected int argument in [0, 250]"));

	self->material->linec = EXPP_ClampInt (value, EXPP_MAT_NLINES_MIN,
									EXPP_MAT_NLINES_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setNRings(BPy_Material *self, PyObject *args)
{
	short value;

	if (!PyArg_ParseTuple(args, "h", &value))			 
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected int argument in [0, 24]"));

	self->material->ringc = EXPP_ClampInt (value, EXPP_MAT_NRINGS_MIN,
									EXPP_MAT_NRINGS_MAX);

	return EXPP_incr_ret (Py_None);
}

/*****************************************************************************/
/* Function:		Material_getAttr																						 */
/* Description: This is a callback function for the BPy_Material type. It is */
/*							the function that accesses BPy_Material "member variables"	 */
/*							and methods.																								 */
/*****************************************************************************/
static PyObject *Material_getAttr (BPy_Material *self, char *name)
{
	PyObject *attr = Py_None;

	if (strcmp(name, "name") == 0)
		attr = PyString_FromString(self->material->id.name+2);
	else if (strcmp(name, "mode") == 0)
		attr = PyInt_FromLong(self->material->mode);
	else if (strcmp(name, "rgbCol") == 0)
		attr = Material_getRGBCol(self);
/*	else if (strcmp(name, "ambCol") == 0)
		attr = Material_getAmbCol(self);*/
	else if (strcmp(name, "specCol") == 0)
		attr = Material_getSpecCol(self);
	else if (strcmp(name, "mirCol") == 0)
		attr = Material_getMirCol(self);
	else if (strcmp(name, "R") == 0)
		attr = PyFloat_FromDouble((double)self->material->r);
	else if (strcmp(name, "G") == 0)
		attr = PyFloat_FromDouble((double)self->material->g);
	else if (strcmp(name, "B") == 0)
		attr = PyFloat_FromDouble((double)self->material->b);
	else if (strcmp(name, "specR") == 0)
		attr = PyFloat_FromDouble((double)self->material->specr);
	else if (strcmp(name, "specG") == 0)
		attr = PyFloat_FromDouble((double)self->material->specg);
	else if (strcmp(name, "specB") == 0)
		attr = PyFloat_FromDouble((double)self->material->specb);
	else if (strcmp(name, "amb") == 0)
		attr = PyFloat_FromDouble((double)self->material->amb);
	else if (strcmp(name, "emit") == 0)
		attr = PyFloat_FromDouble((double)self->material->emit);
	else if (strcmp(name, "alpha") == 0)
		attr = PyFloat_FromDouble((double)self->material->alpha);
	else if (strcmp(name, "ref") == 0)
		attr = PyFloat_FromDouble((double)self->material->ref);
	else if (strcmp(name, "spec") == 0)
		attr = PyFloat_FromDouble((double)self->material->spec);
	else if (strcmp(name, "specTransp") == 0)
		attr = PyFloat_FromDouble((double)self->material->spectra);
	else if (strcmp(name, "add") == 0)
		attr = PyFloat_FromDouble((double)self->material->add);
	else if (strcmp(name, "zOffset") == 0)
		attr = PyFloat_FromDouble((double)self->material->zoffs);
	else if (strcmp(name, "haloSize") == 0)
		attr = PyFloat_FromDouble((double)self->material->hasize);
	else if (strcmp(name, "haloSeed") == 0)
		attr = PyInt_FromLong((double)self->material->seed1);
	else if (strcmp(name, "flareSize") == 0)
		attr = PyFloat_FromDouble((double)self->material->flaresize);
	else if (strcmp(name, "flareBoost") == 0)
		attr = PyFloat_FromDouble((double)self->material->flareboost);
	else if (strcmp(name, "flareSeed") == 0)
		attr = PyInt_FromLong((double)self->material->seed2);
	else if (strcmp(name, "subSize") == 0)
		attr = PyFloat_FromDouble((double)self->material->subsize);
	else if (strcmp(name, "hard") == 0)
		attr = PyInt_FromLong((long)self->material->har);
	else if (strcmp(name, "nFlares") == 0)
		attr = PyInt_FromLong((long)self->material->flarec);
	else if (strcmp(name, "nStars") == 0)
		attr = PyInt_FromLong((long)self->material->starc);
	else if (strcmp(name, "nLines") == 0)
		attr = PyInt_FromLong((long)self->material->linec);
	else if (strcmp(name, "nRings") == 0)
		attr = PyInt_FromLong((long)self->material->ringc);

	else if (strcmp(name, "__members__") == 0) {
		attr = /* 27 items */
			Py_BuildValue("[s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s]",
				"name", "mode", "rgbCol", "specCol", "mirCol",
				"R", "G", "B", "alpha", "amb", "emit", "ref", "spec",
				"specTransp", "add", "zOffset", "haloSize", "haloSeed",
				"flareSize", "flareBoost", "flareSeed", "subSize", "hard",
				"nFlares", "nStars", "nLines", "nRings");
	}

	if (!attr)
		return (EXPP_ReturnPyObjError (PyExc_MemoryError,
											"couldn't create PyObject"));

	if (attr != Py_None) return attr; /* member attribute found, return it */

	/* not an attribute, search the methods table */
	return Py_FindMethod(BPy_Material_methods, (PyObject *)self, name);
}

/****************************************************************************/
/* Function:		Material_setAttr																						*/
/* Description: This is a callback function for the BPy_Material type.			*/
/*							It is the function that sets Material attributes (member		*/
/*							variables).																									*/
/****************************************************************************/
static int Material_setAttr (BPy_Material *self, char *name, PyObject *value)
{
	PyObject *valtuple; 
	PyObject *error = NULL;

/* We're playing a trick on the Python API users here.	Even if they use
 * Material.member = val instead of Material.setMember(val), we end up using the
 * function anyway, since it already has error checking, clamps to the right
 * interval and updates the Blender Material structure when necessary. */

/* First we put "value" in a tuple, because we want to pass it to functions
 * that only accept PyTuples. */
	valtuple = Py_BuildValue("(O)", value);

	if (!valtuple) /* everything OK with our PyObject? */
		return EXPP_ReturnIntError(PyExc_MemoryError,
												 "MaterialSetAttr: couldn't create PyTuple");

/* Now we just compare "name" with all possible BPy_Material member variables */
	if (strcmp (name, "name") == 0)
		error = Material_setName (self, valtuple);
	else if (strcmp (name, "mode") == 0)
		error = Material_setIntMode (self, valtuple); /* special case */
	else if (strcmp (name, "rgbCol") == 0)
		error = Material_setRGBCol (self, valtuple);
/*	else if (strcmp (name, "ambCol") == 0)
		error = Material_setAmbCol (self, valtuple);*/
	else if (strcmp (name, "specCol") == 0)
		error = Material_setSpecCol (self, valtuple);
	else if (strcmp (name, "mirCol") == 0)
		error = Material_setMirCol (self, valtuple);
	else if (strcmp (name, "R") == 0)
		error = Material_setColorComponent (self, "R", valtuple);
	else if (strcmp (name, "G") == 0)
		error = Material_setColorComponent (self, "G", valtuple);
	else if (strcmp (name, "B") == 0)
		error = Material_setColorComponent (self, "B", valtuple);
	else if (strcmp (name, "specR") == 0)
		error = Material_setColorComponent (self, "specR", valtuple);
	else if (strcmp (name, "specG") == 0)
		error = Material_setColorComponent (self, "specG", valtuple);
	else if (strcmp (name, "specB") == 0)
		error = Material_setColorComponent (self, "specB", valtuple);
	else if (strcmp (name, "amb") == 0)
		error = Material_setAmb (self, valtuple);
	else if (strcmp (name, "emit") == 0)
		error = Material_setEmit (self, valtuple);
	else if (strcmp (name, "alpha") == 0)
		error = Material_setAlpha (self, valtuple);
	else if (strcmp (name, "ref") == 0)
		error = Material_setRef (self, valtuple);
	else if (strcmp (name, "spec") == 0)
		error = Material_setSpec (self, valtuple);
	else if (strcmp (name, "specTransp") == 0)
		error = Material_setSpecTransp (self, valtuple);
	else if (strcmp (name, "add") == 0)
		error = Material_setAdd (self, valtuple);
	else if (strcmp (name, "zOffset") == 0)
		error = Material_setZOffset (self, valtuple);
	else if (strcmp (name, "haloSize") == 0)
		error = Material_setHaloSize (self, valtuple);
	else if (strcmp (name, "haloSeed") == 0)
		error = Material_setHaloSeed (self, valtuple);
	else if (strcmp (name, "flareSize") == 0)
		error = Material_setFlareSize (self, valtuple);
	else if (strcmp (name, "flareBoost") == 0)
		error = Material_setFlareBoost (self, valtuple);
	else if (strcmp (name, "flareSeed") == 0)
		error = Material_setFlareSeed (self, valtuple);
	else if (strcmp (name, "subSize") == 0)
		error = Material_setSubSize (self, valtuple);
	else if (strcmp (name, "hard") == 0)
		error = Material_setHardness (self, valtuple);
	else if (strcmp (name, "nFlares") == 0)
		error = Material_setNFlares (self, valtuple);
	else if (strcmp (name, "nStars") == 0)
		error = Material_setNStars (self, valtuple);
	else if (strcmp (name, "nLines") == 0)
		error = Material_setNLines (self, valtuple);
	else if (strcmp (name, "nRings") == 0)
		error = Material_setNRings (self, valtuple);

	else { /* Error */
		Py_DECREF(valtuple);
		return (EXPP_ReturnIntError (PyExc_AttributeError, name));
	}

/* valtuple won't be returned to the caller, so we need to DECREF it */
	Py_DECREF(valtuple);

	if (error != Py_None) return -1;

/* Py_None was incref'ed by the called Material_set* function. We probably
 * don't need to decref Py_None (!), but since Python/C API manual tells us
 * to treat it like any other PyObject regarding ref counting ... */
	Py_DECREF(Py_None);
	return 0; /* normal exit */
}

/*****************************************************************************/
/* Function:		Material_repr																								 */
/* Description: This is a callback function for the BPy_Material type. It		 */
/*							builds a meaninful string to represent material objects.		 */
/*****************************************************************************/
static PyObject *Material_repr (BPy_Material *self)
{
	return PyString_FromFormat ("[Material \"%s\"]", self->material->id.name+2);
}


/*****************************************************************************/
/* These functions are used in NMesh.c and Object.c													 */
/*****************************************************************************/
PyObject *EXPP_PyList_fromMaterialList (Material **matlist, int len)
{
	PyObject *list;
	int i;

	list = PyList_New(0);
	if (!matlist) return list;

	for (i = 0; i < len; i++) {
		Material *mat = matlist[i];
		PyObject *ob;

		if (mat) {
			ob = Material_CreatePyObject (mat);
			PyList_Append (list, ob);
			Py_DECREF (ob); /* because Append increfs */
		}
	}

	return list;
}

Material **EXPP_newMaterialList_fromPyList (PyObject *list)
{
	int i, len;
	BPy_Material *pymat = 0;
	Material *mat;
	Material **matlist;

	len = PySequence_Length (list);
	if (len > 16) len = 16;

	matlist = EXPP_newMaterialList (len);

	for (i= 0; i < len; i++) {

		pymat = (BPy_Material *)PySequence_GetItem (list, i);

		if (Material_CheckPyObject ((PyObject *)pymat)) {
			mat = pymat->material;
			matlist[i] = mat;
		}

		else { /* error; illegal type in material list */
			Py_DECREF(pymat);
			MEM_freeN(matlist);
			return NULL;
		}

		Py_DECREF(pymat);
	}

	return matlist;
}

Material **EXPP_newMaterialList(int len)
{
	Material **matlist =
		(Material **)MEM_mallocN(len * sizeof(Material *), "MaterialList");

	return matlist;
}

int EXPP_releaseMaterialList (Material **matlist, int len)
{
		int						i;
		Material		* mat;

		if ((len < 0) || (len > MAXMAT)) {
				printf ("illegal matindex!\n");
				return 0;
		}

		for (i=0 ; i<len ; i++) {
				mat = matlist[i];
				if (mat != NULL) {
						if (((ID *)mat)->us > 0)
								((ID *)mat)->us--;
						else
								printf ("FATAL: material usage=0: %s", ((ID *)mat)->name);
				}
		}
		MEM_freeN (matlist);

		return 1;
}

/** expands pointer array of length 'oldsize' to length 'newsize'.
	* A pointer to the (void *) array must be passed as first argument 
	* The array pointer content can be NULL, in this case a new array of length
	* 'newsize' is created.
	*/

static int expandPtrArray(void **p, int oldsize, int newsize)
{
	void *newarray;

	if (newsize < oldsize) {
		return 0;
	}	
	newarray = MEM_callocN(newsize * sizeof(void *), "PtrArray");
	if (*p) {
		memcpy(newarray, *p, oldsize);
		MEM_freeN(*p);
	}	
	*p = newarray;
	return 1;
}

int EXPP_synchronizeMaterialLists (Object *object, void *data)
{
		Material	*** p_dataMaterials = give_matarar (object);
		short				* nmaterials = give_totcolp (object);

		if (object->totcol > *nmaterials) {
				/* More object mats than data mats */
				*nmaterials = object->totcol;
				return expandPtrArray ((void *) p_dataMaterials,
															 *nmaterials,
															 object->totcol);
		}
		else {
				if (object->totcol < *nmaterials) {
						/* More data mats than object mats */
						object->totcol = *nmaterials;
						return expandPtrArray ((void *) &object->mat,
																	 object->totcol,
																	 *nmaterials);
				}
		}

		/* No synchronization is needed; they're of equal length */
		return 1;
}

void EXPP_incr_mats_us (Material **matlist, int len)
{
	int i;
	Material *mat;

	if (len <= 0) return;

	for (i = 0; i < len; i++) {
		mat = matlist[i];
		if (mat) mat->id.us++;
	}

	return;
}

