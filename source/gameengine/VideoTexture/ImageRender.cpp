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

#include <BIF_gl.h>

#include "KX_PythonInit.h"
#include "DNA_scene_types.h"

#include "ImageRender.h"
#include "ImageBase.h"
#include "BlendType.h"
#include "Exception.h"

ExceptionID SceneInvalid, CameraInvalid;
ExpDesc SceneInvalidDesc (SceneInvalid, "Scene object is invalid");
ExpDesc CameraInvalidDesc (CameraInvalid, "Camera object is invalid");

// constructor
ImageRender::ImageRender (KX_Scene * scene, KX_Camera * camera) : 
    ImageViewport(),
    m_scene(scene),
    m_camera(camera)
{
	// initialize background colour
	setBackground(0, 0, 255, 255);
    // retrieve rendering objects
    m_engine = KX_GetActiveEngine();
    m_rasterizer = m_engine->GetRasterizer();
    m_canvas = m_engine->GetCanvas();
    m_rendertools = m_engine->GetRenderTools();
}

// destructor
ImageRender::~ImageRender (void)
{
}


// set background color
void ImageRender::setBackground (int red, int green, int blue, int alpha)
{
    m_background[0] = (red < 0) ? 0.f : (red > 255) ? 1.f : float(red)/255.f;
	m_background[1] = (green < 0) ? 0.f : (green > 255) ? 1.f : float(green)/255.f;
	m_background[2] = (blue < 0) ? 0.f : (blue > 255) ? 1.f : float(blue)/255.f;
	m_background[3] = (alpha < 0) ? 0.f : (alpha > 255) ? 1.f : float(alpha)/255.f;
}


// capture image from viewport
void ImageRender::calcImage (unsigned int texId)
{
    if (m_rasterizer->GetDrawingMode() != RAS_IRasterizer::KX_TEXTURED ||   // no need for texture
        m_camera->GetViewport() ||        // camera must be inactive
        m_camera == m_scene->GetActiveCamera())
    {
        // no need to compute texture in non texture rendering
        m_avail = false;
        return;
    }
    // render the scene from the camera
    Render();
	// get image from viewport
	ImageViewport::calcImage(texId);
    // restore OpenGL state
    m_canvas->EndFrame();
}

void ImageRender::Render()
{
    const float ortho = 100.0;
    const RAS_IRasterizer::StereoMode stereomode = m_rasterizer->GetStereoMode();

    // The screen area that ImageViewport will copy is also the rendering zone
    m_canvas->SetViewPort(m_position[0], m_position[1], m_position[0]+m_capSize[0]-1, m_position[1]+m_capSize[1]-1);
    m_canvas->ClearColor(m_background[0], m_background[1], m_background[2], m_background[3]);
    m_canvas->ClearBuffer(RAS_ICanvas::COLOR_BUFFER|RAS_ICanvas::DEPTH_BUFFER);
    m_rasterizer->BeginFrame(RAS_IRasterizer::KX_TEXTURED,m_engine->GetClockTime());
    m_rendertools->BeginFrame(m_rasterizer);
    m_engine->SetWorldSettings(m_scene->GetWorldInfo());
    m_rendertools->SetAuxilaryClientInfo(m_scene);
    m_rasterizer->DisplayFog();
    // matrix calculation, don't apply any of the stereo mode
    m_rasterizer->SetStereoMode(RAS_IRasterizer::RAS_STEREO_NOSTEREO);
    if (m_camera->hasValidProjectionMatrix())
	{
		m_rasterizer->SetProjectionMatrix(m_camera->GetProjectionMatrix());
    } else 
    {
		RAS_FrameFrustum frustrum;
		float lens = m_camera->GetLens();
		bool orthographic = !m_camera->GetCameraData()->m_perspective;
		float nearfrust = m_camera->GetCameraNear();
		float farfrust = m_camera->GetCameraFar();
        float aspect_ratio = 1.0f;
        Scene *blenderScene = m_scene->GetBlenderScene();

        if (orthographic) {
			lens *= ortho;
			nearfrust = (nearfrust + 1.0)*ortho;
			farfrust *= ortho;
		}
		// compute the aspect ratio from frame blender scene settings so that render to texture
        // works the same in Blender and in Blender player
        if (blenderScene->r.ysch != 0)
            aspect_ratio = float(blenderScene->r.xsch) / float(blenderScene->r.ysch);

        RAS_FramingManager::ComputeDefaultFrustum(
            nearfrust,
            farfrust,
            lens,
            aspect_ratio,
            frustrum);
		
		MT_Matrix4x4 projmat = m_rasterizer->GetFrustumMatrix(
			frustrum.x1, frustrum.x2, frustrum.y1, frustrum.y2, frustrum.camnear, frustrum.camfar);

		m_camera->SetProjectionMatrix(projmat);
	}

	MT_Transform camtrans(m_camera->GetWorldToCamera());
	if (!m_camera->GetCameraData()->m_perspective)
		camtrans.getOrigin()[2] *= ortho;
	MT_Matrix4x4 viewmat(camtrans);
	
	m_rasterizer->SetViewMatrix(viewmat, m_camera->NodeGetWorldPosition(),
		m_camera->GetCameraLocation(), m_camera->GetCameraOrientation());
	m_camera->SetModelviewMatrix(viewmat);
    // restore the stereo mode now that the matrix is computed
    m_rasterizer->SetStereoMode(stereomode);

    // do not update the mesh, we don't want to do it more than once per frame
    //m_scene->UpdateMeshTransformations();

	m_scene->CalculateVisibleMeshes(m_rasterizer,m_camera);

	m_scene->RenderBuckets(camtrans, m_rasterizer, m_rendertools);
}


// cast Image pointer to ImageRender
inline ImageRender * getImageRender (PyImage * self)
{ return static_cast<ImageRender*>(self->m_image); }


// python methods

// Blender Scene type
BlendType<KX_Scene> sceneType ("KX_Scene");
// Blender Camera type
BlendType<KX_Camera> cameraType ("KX_Camera");


// object initialization
static int ImageRender_init (PyObject * pySelf, PyObject * args, PyObject * kwds)
{
	// parameters - scene object
	PyObject * scene;
	// camera object
	PyObject * camera;
	// parameter keywords
	static char *kwlist[] = {"sceneObj", "cameraObj", NULL};
	// get parameters
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO", kwlist, &scene, &camera))
		return -1;
	try
	{
		// get scene pointer
		KX_Scene * scenePtr (NULL);
		if (scene != NULL) scenePtr = sceneType.checkType(scene);
		// throw exception if scene is not available
		if (scenePtr == NULL) THRWEXCP(SceneInvalid, S_OK);

		// get camera pointer
		KX_Camera * cameraPtr (NULL);
		if (camera != NULL) cameraPtr = cameraType.checkType(camera);
		// throw exception if camera is not available
		if (cameraPtr == NULL) THRWEXCP(CameraInvalid, S_OK);

		// get pointer to image structure
		PyImage * self = reinterpret_cast<PyImage*>(pySelf);
		// create source object
		if (self->m_image != NULL) delete self->m_image;
		self->m_image = new ImageRender(scenePtr, cameraPtr);
	}
	catch (Exception & exp)
	{
		exp.report();
		return -1;
	}
	// initialization succeded
	return 0;
}


// get background color
PyObject * getBackground (PyImage * self, void * closure)
{
	return Py_BuildValue("[BBBB]", 
        getImageRender(self)->getBackground(0),
		getImageRender(self)->getBackground(1), 
        getImageRender(self)->getBackground(2),
        getImageRender(self)->getBackground(3));
}

// set color
static int setBackground (PyImage * self, PyObject * value, void * closure)
{
	// check validity of parameter
	if (value == NULL || !PySequence_Check(value) || PySequence_Length(value) != 4
		|| !PyInt_Check(PySequence_Fast_GET_ITEM(value, 0))
		|| !PyInt_Check(PySequence_Fast_GET_ITEM(value, 1))
		|| !PyInt_Check(PySequence_Fast_GET_ITEM(value, 2))
		|| !PyInt_Check(PySequence_Fast_GET_ITEM(value, 3)))
	{
		PyErr_SetString(PyExc_TypeError, "The value must be a sequence of 4 integer between 0 and 255");
		return -1;
	}
	// set background color
	getImageRender(self)->setBackground((unsigned char)(PyInt_AsLong(PySequence_Fast_GET_ITEM(value, 0))),
		(unsigned char)(PyInt_AsLong(PySequence_Fast_GET_ITEM(value, 1))),
		(unsigned char)(PyInt_AsLong(PySequence_Fast_GET_ITEM(value, 2))),
        (unsigned char)(PyInt_AsLong(PySequence_Fast_GET_ITEM(value, 3))));
	// success
	return 0;
}


// methods structure
static PyMethodDef imageRenderMethods[] =
{ // methods from ImageBase class
	{"refresh", (PyCFunction)Image_refresh, METH_NOARGS, "Refresh image - invalidate its current content"},
	{NULL}
};
// attributes structure
static PyGetSetDef imageRenderGetSets[] =
{ 
	{(char*)"background", (getter)getBackground, (setter)setBackground, (char*)"background color", NULL},
    // attribute from ImageViewport
	{(char*)"capsize", (getter)ImageViewport_getCaptureSize, (setter)ImageViewport_setCaptureSize, (char*)"size of render area", NULL},
	{(char*)"alpha", (getter)ImageViewport_getAlpha, (setter)ImageViewport_setAlpha, (char*)"use alpha in texture", NULL},
	{(char*)"whole", (getter)ImageViewport_getWhole, (setter)ImageViewport_setWhole, (char*)"use whole viewport to render", NULL},
	// attributes from ImageBase class
	{(char*)"image", (getter)Image_getImage, NULL, (char*)"image data", NULL},
	{(char*)"size", (getter)Image_getSize, NULL, (char*)"image size", NULL},
	{(char*)"scale", (getter)Image_getScale, (setter)Image_setScale, (char*)"fast scale of image (near neighbour)",	NULL},
	{(char*)"flip", (getter)Image_getFlip, (setter)Image_setFlip, (char*)"flip image vertically", NULL},
	{(char*)"filter", (getter)Image_getFilter, (setter)Image_setFilter, (char*)"pixel filter", NULL},
	{NULL}
};


// define python type
PyTypeObject ImageRenderType =
{ 
	PyObject_HEAD_INIT(NULL)
	0,                         /*ob_size*/
	"VideoTexture.ImageRender",   /*tp_name*/
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
	"Image source from render",       /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	imageRenderMethods,    /* tp_methods */
	0,                   /* tp_members */
	imageRenderGetSets,          /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	(initproc)ImageRender_init,     /* tp_init */
	0,                         /* tp_alloc */
	Image_allocNew,           /* tp_new */
};


