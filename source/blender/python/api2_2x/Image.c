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

#include "Image.h"

/*****************************************************************************/
/* Function:              M_Image_New                                       */
/* Python equivalent:     Blender.Image.New                                 */
/*****************************************************************************/
static PyObject *M_Image_New(PyObject *self, PyObject *args, PyObject *keywords)
{
  printf ("In Image_New() - unimplemented in 2.25\n");

	Py_INCREF(Py_None);
	return Py_None;
}

/*****************************************************************************/
/* Function:              M_Image_Get                                       */
/* Python equivalent:     Blender.Image.Get                                 */
/*****************************************************************************/
static PyObject *M_Image_Get(PyObject *self, PyObject *args)
{
  char     *name;
  Image   *img_iter;
	C_Image *wanted_img;

  printf ("In Image_Get()\n");
  if (!PyArg_ParseTuple(args, "s", &name))
  {
    return (PythonReturnErrorObject (PyExc_AttributeError,
            "expected string argument"));
  }

  /* Use the name to search for the image requested. */
  wanted_img = NULL;
  img_iter = G.main->image.first;

  while ((img_iter) && (wanted_img == NULL)) {

	  if (strcmp (name, GetIdName (&(img_iter->id))) == 0)
      wanted_img = (C_Image *)ImageCreatePyObject(img_iter);

		img_iter = img_iter->id.next;
  }

  if (wanted_img == NULL) {
  /* No image exists with the name specified in the argument name. */
    char error_msg[64];
    PyOS_snprintf(error_msg, sizeof(error_msg),
                    "Image \"%s\" not found", name);
    return (PythonReturnErrorObject (PyExc_NameError, error_msg));
  }

  return ((PyObject*)wanted_img);
}

static PyObject *M_Image_Load(PyObject *self, PyObject *args)
{
	char *fname;
	Image *img_ptr;
	C_Image *img;

  printf ("In Image_Load()\n");
	
  if (!PyArg_ParseTuple(args, "s", &fname))
  {
    return (PythonReturnErrorObject (PyExc_AttributeError,
            "expected string argument"));
  }
	
	img_ptr = add_image(fname);
	if (!img_ptr)
		return (PythonReturnErrorObject (PyExc_IOError,
						"couldn't load image"));

	img = (C_Image *)ImageCreatePyObject(img_ptr);

	return (PyObject *)img;
}

/*****************************************************************************/
/* Function:              M_Image_Init                                      */
/*****************************************************************************/
PyObject *M_Image_Init (void)
{
  PyObject  *module;

  printf ("In M_Image_Init()\n");

  module = Py_InitModule3("Image", M_Image_methods, M_Image_doc);

  return (module);
}

/*****************************************************************************/
/* Python C_Image methods:                                                  */
/*****************************************************************************/
static PyObject *Image_getName(C_Image *self)
{
	PyObject *attr;
	attr = PyDict_GetItemString(self->dict, "name");
	if (attr) {
		Py_INCREF(attr);
		return attr;
	}
	return (PythonReturnErrorObject (PyExc_RuntimeError,
					"couldn't get Image.name attribute"));
}

static PyObject *Image_getFilename(C_Image *self)
{
	PyObject *attr;
	attr = PyDict_GetItemString(self->dict, "filename");
	if (attr) {
		Py_INCREF(attr);
		return attr;
	}
	return (PythonReturnErrorObject (PyExc_RuntimeError,
					"couldn't get Image.filename attribute"));
}

static PyObject *Image_rename(C_Image *self, PyObject *args)
{
	char *name_str;
	char buf[21];
	ID *tmp_id;
	PyObject *name;

	if (!PyArg_ParseTuple(args, "s", &name_str))
		return (PythonReturnErrorObject (PyExc_AttributeError,
						"expected string argument"));
	
	PyOS_snprintf(buf, sizeof(buf), "%s", name_str);
	
	/* update the Blender Image, too */
	tmp_id = &self->image->id;
	rename_id(tmp_id, buf);
	PyOS_snprintf(buf, sizeof(buf), "%s", tmp_id->name+2);/* may have changed */

	name = PyString_FromString(buf);

	if (!name)
		return (PythonReturnErrorObject (PyExc_MemoryError,
						"couldn't create PyString Object"));

	if (PyDict_SetItemString(self->dict, "name", name) != 0) {
		Py_DECREF(name);
    return (PythonReturnErrorObject (PyExc_RuntimeError,
            "couldn't set Image.name attribute"));
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Image_setXRep(C_Image *self, PyObject *args)
{
	short value;
	PyObject *rep;

	if (!PyArg_ParseTuple(args, "h", &value))
		return (PythonReturnErrorObject (PyExc_AttributeError,
						"expected int argument in [1,16]"));

	if (value >= EXPP_IMAGE_REP_MIN || value <= EXPP_IMAGE_REP_MAX)
		rep = PyInt_FromLong(value);
	else
		return (PythonReturnErrorObject (PyExc_AttributeError,
						"expected int argument in [1,16]"));

	if (!rep)
		return (PythonReturnErrorObject (PyExc_MemoryError,
						"couldn't create PyInt Object"));

	if (PyDict_SetItemString(self->dict, "xrep", rep) != 0) {
		Py_DECREF(rep);
		return (PythonReturnErrorObject (PyExc_RuntimeError,
						"could not set Image.xrep attribute"));
	}

	/* update the Blender Image, too */
  self->image->xrep = value;

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Image_setYRep(C_Image *self, PyObject *args)
{
	short value;
	PyObject *rep;

	if (!PyArg_ParseTuple(args, "h", &value))
		return (PythonReturnErrorObject (PyExc_AttributeError,
						"expected int argument in [1,16]"));

	if (value >= EXPP_IMAGE_REP_MIN || value <= EXPP_IMAGE_REP_MAX)
		rep = PyInt_FromLong(value);
	else
		return (PythonReturnErrorObject (PyExc_AttributeError,
						"expected int argument in [1,16]"));

	if (!rep)
		return (PythonReturnErrorObject (PyExc_MemoryError,
						"couldn't create PyInt Object"));

	if (PyDict_SetItemString(self->dict, "yrep", rep) != 0) {
		Py_DECREF(rep);
		return (PythonReturnErrorObject (PyExc_RuntimeError,
						"could not set Image.yrep attribute"));
	}

	/* update the Blender Image, too */
  self->image->yrep = value;

	Py_INCREF(Py_None);
	return Py_None;
}

/*****************************************************************************/
/* Function:    ImageCreatePyObject                                         */
/* Description: This function will create a new C_Image.  If the Image     */
/*              struct passed to it is not NULL, it'll use its attributes.   */
/*****************************************************************************/
PyObject *ImageCreatePyObject (Image *blenderImage)
{
	PyObject *name, *filename, *xrep, *yrep;
  C_Image  *img;

  printf ("In ImageCreatePyObject\n");

	img = (C_Image *)PyObject_NEW(C_Image, &Image_Type);
	
	if (img == NULL)
		return NULL;

	img->dict = PyDict_New();

	if (img->dict == NULL) {
		Py_DECREF((PyObject *)img);
		return NULL;
	}
            /*branch currently unused*/
	if (blenderImage == NULL) { /* Not linked to an Image Object yet */
		name = PyString_FromString("DATA");
		filename = PyString_FromString("");
		xrep = PyInt_FromLong(EXPP_IMAGE_REP); /* rep default is 1, of course */
		yrep = PyInt_FromLong(EXPP_IMAGE_REP);
	}
	else { /* Image Object available, get its attributes directly */
		name = PyString_FromString(blenderImage->id.name+2);
		filename = PyString_FromString(blenderImage->name);
		xrep = PyInt_FromLong(blenderImage->xrep);
		yrep = PyInt_FromLong(blenderImage->yrep);
	}

	if (name == NULL || filename == NULL ||
			xrep == NULL || yrep == NULL)
		goto fail;

	if ((PyDict_SetItemString(img->dict, "name", name) != 0) ||
	    (PyDict_SetItemString(img->dict, "filename", filename) != 0) ||
	    (PyDict_SetItemString(img->dict, "xrep", xrep) != 0) ||
	    (PyDict_SetItemString(img->dict, "yrep", yrep) != 0) ||
			(PyDict_SetItemString(img->dict, "__members__",
														PyDict_Keys(img->dict)) != 0))
		goto fail;

  img->image = blenderImage; /* it's NULL when creating only image "data" */
  return ((PyObject*)img);

fail:
	Py_XDECREF(name);
	Py_XDECREF(filename);
	Py_XDECREF(xrep);
	Py_XDECREF(yrep);
  Py_DECREF(img->dict);
	Py_DECREF((PyObject *)img);
	return NULL;
}

/*****************************************************************************/
/* Function:    ImageDeAlloc                                                */
/* Description: This is a callback function for the C_Image type. It is     */
/*              the destructor function.                                     */
/*****************************************************************************/
static void ImageDeAlloc (C_Image *self)
{
	Py_DECREF(self->dict);
  PyObject_DEL (self);
}

/*****************************************************************************/
/* Function:    ImageGetAttr                                                */
/* Description: This is a callback function for the C_Image type. It is     */
/*              the function that accesses C_Image member variables and     */
/*              methods.                                                     */
/*****************************************************************************/
static PyObject* ImageGetAttr (C_Image *img, char *name)
{/* first try the attributes dictionary */
	if (img->dict) {
		PyObject *v = PyDict_GetItemString(img->dict, name);
		if (v) {
			Py_INCREF(v); /* was a borrowed ref */
			return v;
		}
	}

/* not an attribute, search the methods table */
	return Py_FindMethod(C_Image_methods, (PyObject *)img, name);
}

/*****************************************************************************/
/* Function:    ImageSetAttr                                                */
/* Description: This is a callback function for the C_Image type. It is the */
/*              function that changes Image Data members values. If this    */
/*              data is linked to a Blender Image, it also gets updated.    */
/*****************************************************************************/
static int ImageSetAttr (C_Image *self, char *name, PyObject *value)
{
	PyObject *valtuple; 
	PyObject *error = NULL;

	if (self->dict == NULL) return -1;

/* We're playing a trick on the Python API users here.  Even if they use
 * Image.member = val instead of Image.setMember(value), we end up using the
 * function anyway, since it already has error checking, clamps to the right
 * interval and updates the Blender Image structure when necessary. */

	valtuple = PyTuple_New(1); /* the set* functions expect a tuple */

	if (!valtuple)
		return EXPP_intError(PyExc_MemoryError,
									"ImageSetAttr: couldn't create PyTuple");

	if (PyTuple_SetItem(valtuple, 0, value) != 0) {
		Py_DECREF(value); /* PyTuple_SetItem incref's value even when it fails */
		Py_DECREF(valtuple);
		return EXPP_intError(PyExc_RuntimeError,
									 "ImageSetAttr: couldn't fill tuple");
	}

	if (strcmp (name, "name") == 0)
    error = Image_rename (self, valtuple);
	else if (strcmp (name, "xrep") == 0)
		error = Image_setXRep (self, valtuple);
	else if (strcmp (name, "yrep") == 0)
		error = Image_setYRep (self, valtuple);
	else { /* Error: no such member in the Image Data structure */
		Py_DECREF(value);
		Py_DECREF(valtuple);
  	return (EXPP_intError (PyExc_KeyError,
    	      "attribute not found or immutable"));
	}

	if (error == Py_None) return 0; /* normal exit */

	Py_DECREF(value);
	Py_DECREF(valtuple);

	return -1;
}

/*****************************************************************************/
/* Function:    ImagePrint                                                  */
/* Description: This is a callback function for the C_Image type. It        */
/*              builds a meaninful string to 'print' image objects.         */
/*****************************************************************************/
static int ImagePrint(C_Image *self, FILE *fp, int flags)
{ 
	char *name;

	name = PyString_AsString(Image_getName(self));

	fprintf(fp, "[Image \"%s\"]", name);

  return 0;
}

/*****************************************************************************/
/* Function:    ImageRepr                                                   */
/* Description: This is a callback function for the C_Image type. It        */
/*              builds a meaninful string to represent image objects.       */
/*****************************************************************************/
static PyObject *ImageRepr (C_Image *self)
{
	char buf[64];
	char *name;

	name = PyString_AsString(Image_getName(self));

	PyOS_snprintf(buf, sizeof(buf), "[Image \"%s\"]", name);

  return PyString_FromString(buf);
}
