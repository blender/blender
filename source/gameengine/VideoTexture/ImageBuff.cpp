/* $Id$
-----------------------------------------------------------------------------
This source file is part of VideoTexture library

Copyright (c) 2007 The Zdeno Ash Miklas

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place - Suite 330, Boston, MA 02111-1307, USA, or go to
http://www.gnu.org/copyleft/lesser.txt.
-----------------------------------------------------------------------------
*/

// implementation

#include <PyObjectPlus.h>
#include <structmember.h>

#include "ImageBuff.h"

#include "ImageBase.h"
#include "FilterSource.h"


// default filter
FilterRGB24 defFilter;


// load image from buffer
void ImageBuff::load (unsigned char * img, short width, short height)
{
	// initialize image buffer
	init(width, height);
	// original size
	short orgSize[2] = {width, height};
	// is filter available
	if (m_pyfilter != NULL)
		// use it to process image
		convImage(*(m_pyfilter->m_filter), img, orgSize);
	else
		// otherwise use default filter
		convImage(defFilter, img, orgSize);
	// image is available
	m_avail = true;
}



// cast Image pointer to ImageBuff
inline ImageBuff * getImageBuff (PyImage * self)
{ return static_cast<ImageBuff*>(self->m_image); }


// python methods

// load image
static PyObject * load (PyImage * self, PyObject * args)
{
	// parameters: string image buffer, its size, width, height
	unsigned char * buff;
	unsigned int buffSize;
	short width;
	short height;
	// parse parameters
	if (!PyArg_ParseTuple(args, "s#hh:load", &buff, &buffSize, &width, &height))
	{
		// report error
		return NULL;
	}
	// else check buffer size
	else
	{
		// calc proper buffer size
		unsigned int propSize = width * height;
		// use pixel size from filter
		if (self->m_image->getFilter() != NULL)
			propSize *= self->m_image->getFilter()->m_filter->firstPixelSize();
		else
			propSize *= defFilter.firstPixelSize();
		// check if buffer size is correct
		if (propSize != buffSize)
		{
			// if not, report error
			PyErr_SetString(PyExc_TypeError, "Buffer hasn't correct size");
			return NULL;
		}
		else
			// if correct, load image
			getImageBuff(self)->load(buff, width, height);
	}
	Py_RETURN_NONE;	
}


// methods structure
static PyMethodDef imageBuffMethods[] =
{ 
	{"load", (PyCFunction)load, METH_VARARGS, "Load image from buffer"},
	{NULL}
};
// attributes structure
static PyGetSetDef imageBuffGetSets[] =
{	// attributes from ImageBase class
	{(char*)"image", (getter)Image_getImage, NULL, (char*)"image data", NULL},
	{(char*)"size", (getter)Image_getSize, NULL, (char*)"image size", NULL},
	{(char*)"scale", (getter)Image_getScale, (setter)Image_setScale, (char*)"fast scale of image (near neighbour)", NULL},
	{(char*)"flip", (getter)Image_getFlip, (setter)Image_setFlip, (char*)"flip image vertically", NULL},
	{(char*)"filter", (getter)Image_getFilter, (setter)Image_setFilter, (char*)"pixel filter", NULL},
	{NULL}
};


// define python type
PyTypeObject ImageBuffType =
{ 
	PyVarObject_HEAD_INIT(NULL, 0)
	"VideoTexture.ImageBuff",   /*tp_name*/
	sizeof(PyImage),          /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	(destructor)Image_dealloc, /*tp_dealloc*/
	0,                         /*tp_print*/
	0,                         /*tp_getattr*/
	0,                         /*tp_setattr*/
	0,                         /*tp_compare*/
	0,                         /*tp_repr*/
	0,                         /*tp_as_number*/
	0,                         /*tp_as_sequence*/
	0,                         /*tp_as_mapping*/
	0,                         /*tp_hash */
	0,                         /*tp_call*/
	0,                         /*tp_str*/
	0,                         /*tp_getattro*/
	0,                         /*tp_setattro*/
	0,                         /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT,        /*tp_flags*/
	"Image source from image buffer",       /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	imageBuffMethods,    /* tp_methods */
	0,                   /* tp_members */
	imageBuffGetSets,          /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	(initproc)Image_init<ImageBuff>,     /* tp_init */
	0,                         /* tp_alloc */
	Image_allocNew,           /* tp_new */
};

