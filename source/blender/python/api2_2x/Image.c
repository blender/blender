/* 
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Willian P. Germano, Campbell Barton
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_library.h>
#include <BKE_image.h>
#include <BIF_drawimage.h>
#include <BLI_blenlib.h>
#include <IMB_imbuf_types.h> /* for the IB_rect define */
#include "gen_utils.h"

#include "Image.h"

/*****************************************************************************/
/* Python BPy_Image defaults:																								 */
/*****************************************************************************/
#define EXPP_IMAGE_REP			1
#define EXPP_IMAGE_REP_MIN	1
#define EXPP_IMAGE_REP_MAX 16


/************************/
/*** The Image Module ***/
/************************/

/*****************************************************************************/
/* Python API function prototypes for the Image module.											 */
/*****************************************************************************/
static PyObject *M_Image_New (PyObject *self, PyObject *args,
								PyObject *keywords);
static PyObject *M_Image_Get (PyObject *self, PyObject *args);
static PyObject *M_Image_Load (PyObject *self, PyObject *args);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.			 */
/* In Python these will be written to the console when doing a							 */
/* Blender.Image.__doc__																										 */
/*****************************************************************************/
static char M_Image_doc[] =
"The Blender Image module\n\n";

static char M_Image_New_doc[] =
"() - return a new Image object -- unimplemented";

static char M_Image_Get_doc[] =
"(name) - return the image with the name 'name', \
returns None if not found.\n If 'name' is not specified, \
it returns a list of all images in the\ncurrent scene.";

static char M_Image_Load_doc[] =
"(filename) - return image from file filename as Image Object, \
returns None if not found.\n";

/*****************************************************************************/
/* Python method structure definition for Blender.Image module:							 */
/*****************************************************************************/
struct PyMethodDef M_Image_methods[] = {
	{"New",(PyCFunction)M_Image_New, METH_VARARGS|METH_KEYWORDS,
					M_Image_New_doc},
	{"Get",					M_Image_Get,				 METH_VARARGS, M_Image_Get_doc},
	{"get",					M_Image_Get,				 METH_VARARGS, M_Image_Get_doc},
	{"Load",				M_Image_Load,				 METH_VARARGS, M_Image_Load_doc},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Function:							M_Image_New																				 */
/* Python equivalent:			Blender.Image.New																	 */
/*****************************************************************************/
static PyObject *M_Image_New(PyObject *self, PyObject *args, PyObject *keywords)
{
	printf ("In Image_New() - unimplemented in 2.25\n");

	Py_INCREF(Py_None);
	return Py_None;
}

/*****************************************************************************/
/* Function:							M_Image_Get																				 */
/* Python equivalent:			Blender.Image.Get																	 */
/* Description:						Receives a string and returns the image object		 */
/*												whose name matches the string.	If no argument is  */
/*												passed in, a list of all image names in the				 */
/*												current scene is returned.												 */
/*****************************************************************************/
static PyObject *M_Image_Get(PyObject *self, PyObject *args)
{
	char	*name = NULL;
	Image *img_iter;

	if (!PyArg_ParseTuple(args, "|s", &name))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected string argument (or nothing)"));

	img_iter = G.main->image.first;

	if (name) { /* (name) - Search image by name */

		BPy_Image *wanted_image = NULL;

		while ((img_iter) && (wanted_image == NULL)) {
			if (strcmp (name, img_iter->id.name+2) == 0) {
				wanted_image = (BPy_Image *)PyObject_NEW(BPy_Image, &Image_Type);
				if (wanted_image) wanted_image->image = img_iter;
			}
			img_iter = img_iter->id.next;
		}

		if (wanted_image == NULL) { /* Requested image doesn't exist */
			char error_msg[64];
			PyOS_snprintf(error_msg, sizeof(error_msg),
											"Image \"%s\" not found", name);
			return (EXPP_ReturnPyObjError (PyExc_NameError, error_msg));
		}

		return (PyObject *)wanted_image;
	}

	else { /* () - return a list of all images in the scene */
		int index = 0;
		PyObject *img_list, *pyobj;

		img_list = PyList_New (BLI_countlist (&(G.main->image)));

		if (img_list == NULL)
			return (EXPP_ReturnPyObjError (PyExc_MemoryError,
							"couldn't create PyList"));

		while (img_iter) {
			pyobj = Image_CreatePyObject (img_iter);

			if (!pyobj)
				return (EXPP_ReturnPyObjError (PyExc_MemoryError,
									"couldn't create PyObject"));

			PyList_SET_ITEM (img_list, index, pyobj);

			img_iter = img_iter->id.next;
			index++;
		}

		return (img_list);
	}
}

/*****************************************************************************/
/* Function:							M_Image_Load																			 */
/* Python equivalent:			Blender.Image.Load																 */
/* Description:						Receives a string and returns the image object		 */
/*												whose filename matches the string.								 */
/*****************************************************************************/
static PyObject *M_Image_Load(PyObject *self, PyObject *args)
{
	char		*fname;
	Image		*img_ptr;
	BPy_Image *img;

	if (!PyArg_ParseTuple(args, "s", &fname))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected string argument"));

	img = (BPy_Image *)PyObject_NEW(BPy_Image, &Image_Type);

	if (!img)
		return (EXPP_ReturnPyObjError (PyExc_MemoryError,
						"couldn't create PyObject Image_Type"));

	img_ptr = add_image(fname);
	if (!img_ptr)
		return (EXPP_ReturnPyObjError (PyExc_IOError,
						"couldn't load image"));

	img->image = img_ptr;

	return (PyObject *)img;
}

/*****************************************************************************/
/* Function:							Image_Init																				 */
/*****************************************************************************/
PyObject *Image_Init (void)
{
	PyObject	*submodule;

	Image_Type.ob_type = &PyType_Type;

	submodule = Py_InitModule3("Blender.Image", M_Image_methods, M_Image_doc);

	return (submodule);
}

/************************/
/*** The Image PyType ***/
/************************/

/*****************************************************************************/
/* Python BPy_Image methods declarations:																		 */
/*****************************************************************************/
static PyObject *Image_getName(BPy_Image *self);
static PyObject *Image_getFilename(BPy_Image *self);
static PyObject *Image_getSize(BPy_Image *self);
static PyObject *Image_getDepth(BPy_Image *self);
static PyObject *Image_getXRep(BPy_Image *self);
static PyObject *Image_getYRep(BPy_Image *self);
static PyObject *Image_setName(BPy_Image *self, PyObject *args);
static PyObject *Image_setXRep(BPy_Image *self, PyObject *args);
static PyObject *Image_setYRep(BPy_Image *self, PyObject *args);
static PyObject *Image_reload(BPy_Image *self); /* by Campbell */

/*****************************************************************************/
/* Python BPy_Image methods table:																					 */
/*****************************************************************************/
static PyMethodDef BPy_Image_methods[] = {
 /* name, method, flags, doc */
	{"getName", (PyCFunction)Image_getName, METH_NOARGS,
					"() - Return Image object name"},
	{"getFilename", (PyCFunction)Image_getFilename, METH_NOARGS,
					"() - Return Image object filename"},
	{"getSize", (PyCFunction)Image_getSize, METH_NOARGS,
					"() - Return Image object [width, height] dimension in pixels"},
	{"getDepth", (PyCFunction)Image_getDepth, METH_NOARGS,
					"() - Return Image object pixel depth"},
	{"getXRep", (PyCFunction)Image_getXRep, METH_NOARGS,
					"() - Return Image object x repetition value"},
	{"getYRep", (PyCFunction)Image_getYRep, METH_NOARGS,
					"() - Return Image object y repetition value"},
	{"reload", (PyCFunction)Image_reload, METH_NOARGS,
					"() - Reload the image from the filesystem"},
	{"setName", (PyCFunction)Image_setName, METH_VARARGS,
					"(str) - Change Image object name"},
	{"setXRep", (PyCFunction)Image_setXRep, METH_VARARGS,
					"(int) - Change Image object x repetition value"},
	{"setYRep", (PyCFunction)Image_setYRep, METH_VARARGS,
					"(int) - Change Image object y repetition value"},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python Image_Type callback function prototypes:													 */
/*****************************************************************************/
static void Image_dealloc (BPy_Image *self);
static int Image_setAttr (BPy_Image *self, char *name, PyObject *v);
static int Image_compare (BPy_Image *a, BPy_Image *b);
static PyObject *Image_getAttr (BPy_Image *self, char *name);
static PyObject *Image_repr (BPy_Image *self);

/*****************************************************************************/
/* Python Image_Type structure definition:																	 */
/*****************************************************************************/
PyTypeObject Image_Type =
{
	PyObject_HEAD_INIT(NULL)
	0,																		 /* ob_size */
	"Blender Image",											 /* tp_name */
	sizeof (BPy_Image),										 /* tp_basicsize */
	0,																		 /* tp_itemsize */
	/* methods */
	(destructor)Image_dealloc,						 /* tp_dealloc */
	0,																		 /* tp_print */
	(getattrfunc)Image_getAttr,						 /* tp_getattr */
	(setattrfunc)Image_setAttr,						 /* tp_setattr */
	(cmpfunc)Image_compare,								 /* tp_compare */
	(reprfunc)Image_repr,									 /* tp_repr */
	0,																		 /* tp_as_number */
	0,																		 /* tp_as_sequence */
	0,																		 /* tp_as_mapping */
	0,																		 /* tp_as_hash */
	0,0,0,0,0,0,
	0,																		 /* tp_doc */ 
	0,0,0,0,0,0,
	BPy_Image_methods,											 /* tp_methods */
	0,																		 /* tp_members */
};

/*****************************************************************************/
/* Function:		Image_dealloc																								 */
/* Description: This is a callback function for the BPy_Image type. It is		 */
/*							the destructor function.																		 */
/*****************************************************************************/
static void Image_dealloc (BPy_Image *self)
{
	PyObject_DEL (self);
}

/*****************************************************************************/
/* Function:		Image_CreatePyObject																				 */
/* Description: This function will create a new BPy_Image from an existing	 */
/*							Blender image structure.																		 */
/*****************************************************************************/
PyObject *Image_CreatePyObject (Image *image)
{
	BPy_Image *py_img;

	py_img = (BPy_Image *)PyObject_NEW (BPy_Image, &Image_Type);

	if (!py_img)
		return EXPP_ReturnPyObjError (PyExc_MemoryError,
						"couldn't create BPy_Image object");

	py_img->image = image;

	return (PyObject *)py_img;
}

/*****************************************************************************/
/* Function:		Image_CheckPyObject																					 */
/* Description: This function returns true when the given PyObject is of the */
/*							type Image. Otherwise it will return false.									 */
/*****************************************************************************/
int Image_CheckPyObject (PyObject *pyobj)
{
	return (pyobj->ob_type == &Image_Type);
}

/*****************************************************************************/
/* Function:		Image_FromPyObject																					 */
/* Description: Returns the Blender Image associated with this object				 */
/*****************************************************************************/
Image *Image_FromPyObject (PyObject *pyobj)
{
	return ((BPy_Image *)pyobj)->image;
}

/*****************************************************************************/
/* Python BPy_Image methods:																								 */
/*****************************************************************************/
static PyObject *Image_getName(BPy_Image *self)
{
	PyObject *attr = PyString_FromString(self->image->id.name+2);

	if (attr) return attr;

	return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
					"couldn't get Image.name attribute"));
}

static PyObject *Image_getFilename(BPy_Image *self)
{
	PyObject *attr = PyString_FromString(self->image->name);

	if (attr) return attr;

	return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
					"couldn't get Image.filename attribute"));
}

static PyObject *Image_getSize(BPy_Image *self)
{
	PyObject *attr;
	Image *image = self->image;

	if (!image->ibuf) /* if no image data available */
		load_image(image, IB_rect, "", 0); /* loading it */

	if (!image->ibuf) /* didn't work */
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
					"couldn't load image data in Blender");

	attr = Py_BuildValue("[hh]", image->ibuf->x, image->ibuf->y);

	if (attr) return attr;

	return EXPP_ReturnPyObjError (PyExc_RuntimeError,
					"couldn't get Image.size attribute");
}

static PyObject *Image_getDepth(BPy_Image *self)
{
	PyObject *attr;
	Image *image = self->image;

	if (!image->ibuf) /* if no image data available */
		load_image(image, IB_rect, "", 0); /* loading it */

	if (!image->ibuf) /* didn't work */
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
					"couldn't load image data in Blender");

	attr = Py_BuildValue("h", image->ibuf->depth);

	if (attr) return attr;

	return EXPP_ReturnPyObjError (PyExc_RuntimeError,
					"couldn't get Image.depth attribute");
}


static PyObject *Image_getXRep(BPy_Image *self)
{
	PyObject *attr = PyInt_FromLong(self->image->xrep);

	if (attr) return attr;

	return EXPP_ReturnPyObjError (PyExc_RuntimeError,
					"couldn't get Image.xrep attribute");
}

static PyObject *Image_getYRep(BPy_Image *self)
{
	PyObject *attr = PyInt_FromLong(self->image->yrep);

	if (attr) return attr;

	return EXPP_ReturnPyObjError (PyExc_RuntimeError,
					"couldn't get Image.yrep attribute");
}

static PyObject *Image_reload(BPy_Image *self)
{
	Image *img = self->image;
	
	free_image_buffers(img); /* force read again */
	img->ok = 1;
	image_changed(G.sima, 0);
	
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Image_setName(BPy_Image *self, PyObject *args)
{
	char *name;
	char buf[21];

	if (!PyArg_ParseTuple(args, "s", &name))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected string argument"));
	
	PyOS_snprintf(buf, sizeof(buf), "%s", name);
	
	rename_id(&self->image->id, buf);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Image_setXRep(BPy_Image *self, PyObject *args)
{
	short value;

	if (!PyArg_ParseTuple(args, "h", &value))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected int argument in [1,16]"));

	if (value >= EXPP_IMAGE_REP_MIN || value <= EXPP_IMAGE_REP_MAX)
		self->image->xrep = value;
	else
		return (EXPP_ReturnPyObjError (PyExc_ValueError,
						"expected int argument in [1,16]"));

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Image_setYRep(BPy_Image *self, PyObject *args)
{
	short value;

	if (!PyArg_ParseTuple(args, "h", &value))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected int argument in [1,16]"));

	if (value >= EXPP_IMAGE_REP_MIN || value <= EXPP_IMAGE_REP_MAX)
		self->image->yrep = value;
	else
		return (EXPP_ReturnPyObjError (PyExc_ValueError,
						"expected int argument in [1,16]"));

	Py_INCREF(Py_None);
	return Py_None;
}

/*****************************************************************************/
/* Function:		Image_getAttr																								 */
/* Description: This is a callback function for the BPy_Image type. It is		 */
/*							the function that accesses BPy_Image member variables and		 */
/*							methods.																										 */
/*****************************************************************************/
static PyObject *Image_getAttr (BPy_Image *self, char *name)
{
	PyObject *attr = Py_None;

	if (strcmp(name, "name") == 0)
		attr = PyString_FromString(self->image->id.name+2);
	else if (strcmp(name, "filename") == 0)
		attr = PyString_FromString(self->image->name);
	else if (strcmp(name, "size") == 0)
		attr = Image_getSize(self);
	else if (strcmp(name, "depth") == 0)
		attr = Image_getDepth(self);
	else if (strcmp(name, "xrep") == 0)
		attr = PyInt_FromLong(self->image->xrep);
	else if (strcmp(name, "yrep") == 0)
		attr = PyInt_FromLong(self->image->yrep);

	else if (strcmp(name, "__members__") == 0)
		attr = Py_BuildValue("[s,s,s,s,s,s]",
										"name", "filename", "size", "depth", "xrep", "yrep");

	if (!attr)
		return (EXPP_ReturnPyObjError (PyExc_MemoryError,
														"couldn't create PyObject"));

	if (attr != Py_None) return attr; /* attribute found, return its value */

	/* not an attribute, search the methods table */
	return Py_FindMethod(BPy_Image_methods, (PyObject *)self, name);
}

/*****************************************************************************/
/* Function:		Image_setAttr																								 */
/* Description: This is a callback function for the BPy_Image type. It is the*/
/*							function that changes Image object members values. If this	 */
/*							data is linked to a Blender Image, it also gets updated.		 */
/*****************************************************************************/
static int Image_setAttr (BPy_Image *self, char *name, PyObject *value)
{
	PyObject *valtuple; 
	PyObject *error = NULL;

/* We're playing a trick on the Python API users here.	Even if they use
 * Image.member = val instead of Image.setMember(value), we end up using the
 * function anyway, since it already has error checking, clamps to the right
 * interval and updates the Blender Image structure when necessary. */

	valtuple = Py_BuildValue("(O)", value); /*the set* functions expect a tuple*/

	if (!valtuple)
		return EXPP_ReturnIntError(PyExc_MemoryError,
									"ImageSetAttr: couldn't create PyTuple");

	if (strcmp (name, "name") == 0)
		error = Image_setName (self, valtuple);
	else if (strcmp (name, "xrep") == 0)
		error = Image_setXRep (self, valtuple);
	else if (strcmp (name, "yrep") == 0)
		error = Image_setYRep (self, valtuple);
	else { /* Error: no such member in the Image object structure */
		Py_DECREF(value);
		Py_DECREF(valtuple);
		return (EXPP_ReturnIntError (PyExc_KeyError,
						"attribute not found or immutable"));
	}

	Py_DECREF(valtuple);

	if (error != Py_None) return -1;

	Py_DECREF(Py_None); /* incref'ed by the called set* function */
	return 0; /* normal exit */
}

/*****************************************************************************/
/* Function:		Image_compare																								 */
/* Description: This is a callback function for the BPy_Image type. It			 */
/*							compares two Image_Type objects. Only the "==" and "!="			 */
/*							comparisons are meaninful. Returns 0 for equality and -1 if  */
/*							they don't point to the same Blender Image struct.					 */
/*							In Python it becomes 1 if they are equal, 0 otherwise.			 */
/*****************************************************************************/
static int Image_compare (BPy_Image *a, BPy_Image *b)
{
	Image *pa = a->image, *pb = b->image;
	return (pa == pb) ? 0:-1;
}

/*****************************************************************************/
/* Function:		Image_repr																									 */
/* Description: This is a callback function for the BPy_Image type. It			 */
/*							builds a meaninful string to represent image objects.				 */
/*****************************************************************************/
static PyObject *Image_repr (BPy_Image *self)
{
	return PyString_FromFormat("[Image \"%s\"]", self->image->id.name+2);
}
