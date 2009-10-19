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

#include <KX_GameObject.h>
#include <RAS_MeshObject.h>
#include <DNA_mesh_types.h>
#include <DNA_meshdata_types.h>
#include <DNA_image_types.h>
#include <IMB_imbuf_types.h>
#include <KX_PolygonMaterial.h>

#include <MEM_guardedalloc.h>

#include <KX_BlenderMaterial.h>
#include <BL_Texture.h>

#include "KX_KetsjiEngine.h"
#include "KX_PythonInit.h"
#include "Texture.h"
#include "ImageBase.h"
#include "Exception.h"

#include <memory.h>
#include "GL/glew.h"


// macro for exception handling and logging
#define CATCH_EXCP catch (Exception & exp) \
{ exp.report(); return NULL; }


// Blender GameObject type
BlendType<KX_GameObject> gameObjectType ("KX_GameObject");


// load texture
void loadTexture (unsigned int texId, unsigned int * texture, short * size,
				  bool mipmap)
{
	// load texture for rendering
	glBindTexture(GL_TEXTURE_2D, texId);
	if (mipmap)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, size[0], size[1], GL_RGBA, GL_UNSIGNED_BYTE, texture);
	} 
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, size[0], size[1], 0, GL_RGBA, GL_UNSIGNED_BYTE, texture);
	}
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}


// get pointer to material
RAS_IPolyMaterial * getMaterial (PyObject *obj, short matID)
{
	// if object is available
	if (obj != NULL)
	{
		// get pointer to texture image
		KX_GameObject * gameObj = gameObjectType.checkType(obj);
		if (gameObj != NULL && gameObj->GetMeshCount() > 0)
		{
			// get material from mesh
			RAS_MeshObject * mesh = gameObj->GetMesh(0);
			RAS_MeshMaterial *meshMat = mesh->GetMeshMaterial(matID);
			if (meshMat != NULL && meshMat->m_bucket != NULL)
				// return pointer to polygon or blender material
				return meshMat->m_bucket->GetPolyMaterial();
		}
	}
	// otherwise material was not found
	return NULL;
}


// get material ID
short getMaterialID (PyObject * obj, char * name)
{
	// search for material
	for (short matID = 0;; ++matID)
	{
		// get material
		RAS_IPolyMaterial * mat = getMaterial(obj, matID);
		// if material is not available, report that no material was found
		if (mat == NULL) 
			break;
		// name is a material name if it starts with MA and a UV texture name if it starts with IM
		if (name[0] == 'I' && name[1] == 'M')
		{
			// if texture name matches
			if (strcmp(mat->GetTextureName().ReadPtr(), name) == 0)
				return matID;
		} else 
		{
			// if material name matches
			if (strcmp(mat->GetMaterialName().ReadPtr(), name) == 0)
				return matID;
		}
	}
	// material was not found
	return -1;
}


// Texture object allocation
PyObject * Texture_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	// allocate object
	Texture * self = reinterpret_cast<Texture*>(type->tp_alloc(type, 0));
	// initialize object structure
	self->m_actTex = 0;
	self->m_orgSaved = false;
	self->m_imgTexture = NULL;
	self->m_matTexture = NULL;
	self->m_mipmap = false;
	self->m_scaledImg = NULL;
	self->m_scaledImgSize = 0;
	self->m_source = NULL;
	self->m_lastClock = 0.0;
	// return allocated object
	return reinterpret_cast<PyObject*>(self);
}


// forward declaration
PyObject * Texture_close(Texture * self);
int Texture_setSource (Texture * self, PyObject * value, void * closure);


// Texture object deallocation
void Texture_dealloc (Texture * self)
{
	// release renderer
	Py_XDECREF(self->m_source);
	// close texture
	PyObject* ret = Texture_close(self);
	Py_DECREF(ret);
	// release scaled image buffer
	delete [] self->m_scaledImg;
	// release object
	((PyObject *)self)->ob_type->tp_free((PyObject*)self);
}


ExceptionID MaterialNotAvail;
ExpDesc MaterialNotAvailDesc (MaterialNotAvail, "Texture material is not available");

// Texture object initialization
int Texture_init (Texture *self, PyObject *args, PyObject *kwds)
{
	// parameters - game object with video texture
	PyObject * obj = NULL;
	// material ID
	short matID = 0;
	// texture ID
	short texID = 0;
	// texture object with shared texture ID
	Texture * texObj = NULL;

	static char *kwlist[] = {"gameObj", "materialID", "textureID", "textureObj", NULL};

	// get parameters
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|hhO!", kwlist, &obj, &matID,
		&texID, &TextureType, &texObj))
		return -1; 

	// if parameters are available
	if (obj != NULL)
	{
		// process polygon material or blender material
		try
		{
			// get pointer to texture image
			RAS_IPolyMaterial * mat = getMaterial(obj, matID);
			if (mat != NULL)
			{
				// is it blender material or polygon material
				if (mat->GetFlag() & RAS_BLENDERGLSL) 
				{
					self->m_imgTexture = static_cast<KX_BlenderMaterial*>(mat)->getImage(texID);
					self->m_useMatTexture = false;
				} else if (mat->GetFlag() & RAS_BLENDERMAT)
				{
					// get blender material texture
					self->m_matTexture = static_cast<KX_BlenderMaterial*>(mat)->getTex(texID);
					self->m_useMatTexture = true;
				}
				else
				{
					// get texture pointer from polygon material
					MTFace * tface = static_cast<KX_PolygonMaterial*>(mat)->GetMTFace();
					self->m_imgTexture = (Image*)tface->tpage;
					self->m_useMatTexture = false;
				}
			}
			// check if texture is available, if not, initialization failed
			if (self->m_imgTexture == NULL && self->m_matTexture == NULL)
				// throw exception if initialization failed
				THRWEXCP(MaterialNotAvail, S_OK);

			// if texture object is provided
			if (texObj != NULL)
			{
				// copy texture code
				self->m_actTex = texObj->m_actTex;
				self->m_mipmap = texObj->m_mipmap;
				if (texObj->m_source != NULL)
					Texture_setSource(self, reinterpret_cast<PyObject*>(texObj->m_source), NULL);
			}
			else
				// otherwise generate texture code
				glGenTextures(1, (GLuint*)&self->m_actTex);
		}
		catch (Exception & exp)
		{
			exp.report();
			return -1;
		}
	}
	// initialization succeded
	return 0;
}


// close added texture
PyObject * Texture_close(Texture * self)
{
	// restore texture
	if (self->m_orgSaved)
	{
		self->m_orgSaved = false;
		// restore original texture code
		if (self->m_useMatTexture)
			self->m_matTexture->swapTexture(self->m_orgTex);
		else
			self->m_imgTexture->bindcode = self->m_orgTex;
		// drop actual texture
		if (self->m_actTex != 0)
		{
			glDeleteTextures(1, (GLuint *)&self->m_actTex);
			self->m_actTex = 0;
		}
	}
	Py_RETURN_NONE;
}


// refresh texture
PyObject * Texture_refresh (Texture * self, PyObject * args)
{
	// get parameter - refresh source
	PyObject * param;
	if (!PyArg_ParseTuple(args, "O:refresh", &param) || !PyBool_Check(param))
	{
		// report error
		PyErr_SetString(PyExc_TypeError, "The value must be a bool");
		return NULL;
	}
	// some trick here: we are in the business of loading a texture,
	// no use to do it if we are still in the same rendering frame.
	// We find this out by looking at the engine current clock time
	KX_KetsjiEngine* engine = KX_GetActiveEngine();
	if (engine->GetClockTime() != self->m_lastClock) 
	{
		self->m_lastClock = engine->GetClockTime();
		// set source refresh
		bool refreshSource = (param == Py_True);
		// try to proces texture from source
		try
		{
			// if source is available
			if (self->m_source != NULL)
			{
				// check texture code
				if (!self->m_orgSaved)
				{
					self->m_orgSaved = true;
					// save original image code
					if (self->m_useMatTexture)
						self->m_orgTex = self->m_matTexture->swapTexture(self->m_actTex);
					else
					{
						self->m_orgTex = self->m_imgTexture->bindcode;
						self->m_imgTexture->bindcode = self->m_actTex;
					}
				}

				// get texture
				unsigned int * texture = self->m_source->m_image->getImage(self->m_actTex);
				// if texture is available
				if (texture != NULL)
				{
					// get texture size
					short * orgSize = self->m_source->m_image->getSize();
					// calc scaled sizes
					short size[] = {ImageBase::calcSize(orgSize[0]), ImageBase::calcSize(orgSize[1])};
					// scale texture if needed
					if (size[0] != orgSize[0] || size[1] != orgSize[1])
					{
						// if scaled image buffer is smaller than needed
						if (self->m_scaledImgSize < (unsigned int)(size[0] * size[1]))
						{
							// new size
							self->m_scaledImgSize = size[0] * size[1];
							// allocate scaling image
							delete [] self->m_scaledImg;
							self->m_scaledImg = new unsigned int[self->m_scaledImgSize];
						}
						// scale texture
						gluScaleImage(GL_RGBA, orgSize[0], orgSize[1], GL_UNSIGNED_BYTE, texture,
							size[0], size[1], GL_UNSIGNED_BYTE, self->m_scaledImg);
						// use scaled image instead original
						texture = self->m_scaledImg;
					}
					// load texture for rendering
					loadTexture (self->m_actTex, texture, size, self->m_mipmap);

					// refresh texture source, if required
					if (refreshSource) self->m_source->m_image->refresh();
				}
			}
		}
		CATCH_EXCP;
	}
	Py_RETURN_NONE;
}


// get mipmap value
PyObject * Texture_getMipmap (Texture * self, void * closure)
{
	// return true if flag is set, otherwise false
	if (self->m_mipmap) Py_RETURN_TRUE;
	else Py_RETURN_FALSE;
}

// set mipmap value
int Texture_setMipmap (Texture * self, PyObject * value, void * closure)
{
	// check parameter, report failure
	if (value == NULL || !PyBool_Check(value))
	{
		PyErr_SetString(PyExc_TypeError, "The value must be a bool");
		return -1;
	}
	// set mipmap
	self->m_mipmap = value == Py_True;
	// success
	return 0;
}


// get source object
PyObject * Texture_getSource (Texture * self, PyObject * value, void * closure)
{
	// if source exists
	if (self->m_source != NULL)
	{
		Py_INCREF(self->m_source);
		return reinterpret_cast<PyObject*>(self->m_source);
	}
	// otherwise return None
	Py_RETURN_NONE;
}


// set source object
int Texture_setSource (Texture * self, PyObject * value, void * closure)
{
	// check new value
	if (value == NULL || !pyImageTypes.in(value->ob_type))
	{
		// report value error
		PyErr_SetString(PyExc_TypeError, "Invalid type of value");
		return -1;
	}
	// increase ref count for new value
	Py_INCREF(value);
	// release previous
	Py_XDECREF(self->m_source);
	// set new value
	self->m_source = reinterpret_cast<PyImage*>(value);
	// return success
	return 0;
}


// class Texture methods
static PyMethodDef textureMethods[] =
{
	{ "close", (PyCFunction)Texture_close, METH_NOARGS, "Close dynamic texture and restore original"},
	{ "refresh", (PyCFunction)Texture_refresh, METH_VARARGS, "Refresh texture from source"},
	{NULL}  /* Sentinel */
};

// class Texture attributes
static PyGetSetDef textureGetSets[] =
{ 
	{(char*)"source", (getter)Texture_getSource, (setter)Texture_setSource, (char*)"source of texture", NULL},
	{(char*)"mipmap", (getter)Texture_getMipmap, (setter)Texture_setMipmap, (char*)"mipmap texture", NULL},
	{NULL}
};


// class Texture declaration
PyTypeObject TextureType =
{
	PyVarObject_HEAD_INIT(NULL, 0)
	"VideoTexture.Texture",   /*tp_name*/
	sizeof(Texture),           /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	(destructor)Texture_dealloc,/*tp_dealloc*/
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
	"Texture objects",       /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	textureMethods,      /* tp_methods */
	0,                   /* tp_members */
	textureGetSets,            /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	(initproc)Texture_init,    /* tp_init */
	0,                         /* tp_alloc */
	Texture_new,               /* tp_new */
};
