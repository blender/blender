/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright (c) 2007 The Zdeno Ash Miklas
 *
 * This source file is part of VideoTexture library
 *
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/VideoTexture/ImageRender.cpp
 *  \ingroup bgevideotex
 */

// implementation

#include "EXP_PyObjectPlus.h"
#include <structmember.h>
#include <float.h>
#include <math.h>


#include "GPU_glew.h"

#include "KX_PythonInit.h"
#include "DNA_scene_types.h"
#include "RAS_CameraData.h"
#include "RAS_MeshObject.h"
#include "RAS_Polygon.h"
#include "RAS_IOffScreen.h"
#include "RAS_ISync.h"
#include "BLI_math.h"

#include "ImageRender.h"
#include "ImageBase.h"
#include "BlendType.h"
#include "Exception.h"
#include "Texture.h"

ExceptionID SceneInvalid, CameraInvalid, ObserverInvalid, OffScreenInvalid;
ExceptionID MirrorInvalid, MirrorSizeInvalid, MirrorNormalInvalid, MirrorHorizontal, MirrorTooSmall;
ExpDesc SceneInvalidDesc(SceneInvalid, "Scene object is invalid");
ExpDesc CameraInvalidDesc(CameraInvalid, "Camera object is invalid");
ExpDesc ObserverInvalidDesc(ObserverInvalid, "Observer object is invalid");
ExpDesc OffScreenInvalidDesc(OffScreenInvalid, "Offscreen object is invalid");
ExpDesc MirrorInvalidDesc(MirrorInvalid, "Mirror object is invalid");
ExpDesc MirrorSizeInvalidDesc(MirrorSizeInvalid, "Mirror has no vertex or no size");
ExpDesc MirrorNormalInvalidDesc(MirrorNormalInvalid, "Cannot determine mirror plane");
ExpDesc MirrorHorizontalDesc(MirrorHorizontal, "Mirror is horizontal in local space");
ExpDesc MirrorTooSmallDesc(MirrorTooSmall, "Mirror is too small");

// constructor
ImageRender::ImageRender (KX_Scene *scene, KX_Camera * camera, PyRASOffScreen * offscreen) :
    ImageViewport(offscreen),
    m_render(true),
    m_done(false),
    m_scene(scene),
    m_camera(camera),
    m_owncamera(false),
    m_offscreen(offscreen),
    m_sync(NULL),
    m_observer(NULL),
    m_mirror(NULL),
    m_clip(100.f),
    m_mirrorHalfWidth(0.f),
    m_mirrorHalfHeight(0.f)
{
	// initialize background color to scene background color as default
	setBackgroundFromScene(m_scene);
	// retrieve rendering objects
	m_engine = KX_GetActiveEngine();
	m_rasterizer = m_engine->GetRasterizer();
	m_canvas = m_engine->GetCanvas();
	// keep a reference to the offscreen buffer
	if (m_offscreen) {
		Py_INCREF(m_offscreen);
	}
}

// destructor
ImageRender::~ImageRender (void)
{
	if (m_owncamera)
		m_camera->Release();
	if (m_sync)
		delete m_sync;
	Py_XDECREF(m_offscreen);
}

// get background color
float ImageRender::getBackground (int idx)
{
	return (idx < 0 || idx > 3) ? 0.0f : m_background[idx] * 255.0f;
}

// set background color
void ImageRender::setBackground (float red, float green, float blue, float alpha)
{
	m_background[0] = (red < 0.0f) ? 0.0f : (red > 255.0f) ? 1.0f : red / 255.0f;
	m_background[1] = (green < 0.0f) ? 0.0f : (green > 255.0f) ? 1.0f : green / 255.0f;
	m_background[2] = (blue < 0.0f) ? 0.0f : (blue > 255.0f) ? 1.0f : blue / 255.0f;
	m_background[3] = (alpha < 0.0f) ? 0.0f : (alpha > 255.0f) ? 1.0f : alpha / 255.0f;
}

// set background color from scene
void ImageRender::setBackgroundFromScene (KX_Scene *scene)
{
	if (scene) {
		const float *background_color = scene->GetWorldInfo()->getBackColorConverted();
		copy_v3_v3(m_background, background_color);
		m_background[3] = 1.0f;
	}
	else {
		const float blue_color[] = {0.0f, 0.0f, 1.0f, 1.0f};
		copy_v4_v4(m_background, blue_color);
	}
}


// capture image from viewport
void ImageRender::calcViewport (unsigned int texId, double ts, unsigned int format)
{
	// render the scene from the camera
	if (!m_done) {
		if (!Render()) {
			return;
		}
	}
	else if (m_offscreen) {
		m_offscreen->ofs->Bind(RAS_IOffScreen::RAS_OFS_BIND_READ);
	}
	// wait until all render operations are completed
	WaitSync();
	// get image from viewport (or FBO)
	ImageViewport::calcViewport(texId, ts, format);
	if (m_offscreen) {
		m_offscreen->ofs->Unbind();
	}
}

bool ImageRender::Render()
{
	RAS_FrameFrustum frustum;

	if (!m_render ||
	    m_rasterizer->GetDrawingMode() != RAS_IRasterizer::KX_TEXTURED ||   // no need for texture
        m_camera->GetViewport() ||        // camera must be inactive
        m_camera == m_scene->GetActiveCamera())
	{
		// no need to compute texture in non texture rendering
		return false;
	}

	if (!m_scene->IsShadowDone())
		m_engine->RenderShadowBuffers(m_scene);

	if (m_mirror)
	{
		// mirror mode, compute camera frustum, position and orientation
		// convert mirror position and normal in world space
		const MT_Matrix3x3 & mirrorObjWorldOri = m_mirror->GetSGNode()->GetWorldOrientation();
		const MT_Point3 & mirrorObjWorldPos = m_mirror->GetSGNode()->GetWorldPosition();
		const MT_Vector3 & mirrorObjWorldScale = m_mirror->GetSGNode()->GetWorldScaling();
		MT_Point3 mirrorWorldPos =
		        mirrorObjWorldPos + mirrorObjWorldScale * (mirrorObjWorldOri * m_mirrorPos);
		MT_Vector3 mirrorWorldZ = mirrorObjWorldOri * m_mirrorZ;
		// get observer world position
		const MT_Point3 & observerWorldPos = m_observer->GetSGNode()->GetWorldPosition();
		// get plane D term = mirrorPos . normal
		MT_Scalar mirrorPlaneDTerm = mirrorWorldPos.dot(mirrorWorldZ);
		// compute distance of observer to mirror = D - observerPos . normal
		MT_Scalar observerDistance = mirrorPlaneDTerm - observerWorldPos.dot(mirrorWorldZ);
		// if distance < 0.01 => observer is on wrong side of mirror, don't render
		if (observerDistance < 0.01)
			return false;
		// set camera world position = observerPos + normal * 2 * distance
		MT_Point3 cameraWorldPos = observerWorldPos + (MT_Scalar(2.0)*observerDistance)*mirrorWorldZ;
		m_camera->GetSGNode()->SetLocalPosition(cameraWorldPos);
		// set camera orientation: z=normal, y=mirror_up in world space, x= y x z
		MT_Vector3 mirrorWorldY = mirrorObjWorldOri * m_mirrorY;
		MT_Vector3 mirrorWorldX = mirrorObjWorldOri * m_mirrorX;
		MT_Matrix3x3 cameraWorldOri(
		            mirrorWorldX[0], mirrorWorldY[0], mirrorWorldZ[0],
		            mirrorWorldX[1], mirrorWorldY[1], mirrorWorldZ[1],
		            mirrorWorldX[2], mirrorWorldY[2], mirrorWorldZ[2]);
		m_camera->GetSGNode()->SetLocalOrientation(cameraWorldOri);
		m_camera->GetSGNode()->UpdateWorldData(0.0);
		// compute camera frustum:
		//   get position of mirror relative to camera: offset = mirrorPos-cameraPos
		MT_Vector3 mirrorOffset = mirrorWorldPos - cameraWorldPos;
		//   convert to camera orientation
		mirrorOffset = mirrorOffset * cameraWorldOri;
		//   scale mirror size to world scale:
		//     get closest local axis for mirror Y and X axis and scale height and width by local axis scale
		MT_Scalar x, y;
		x = fabs(m_mirrorY[0]);
		y = fabs(m_mirrorY[1]);
		float height = (x > y) ?
		            ((x > fabs(m_mirrorY[2])) ? mirrorObjWorldScale[0] : mirrorObjWorldScale[2]):
		            ((y > fabs(m_mirrorY[2])) ? mirrorObjWorldScale[1] : mirrorObjWorldScale[2]);
		x = fabs(m_mirrorX[0]);
		y = fabs(m_mirrorX[1]);
		float width = (x > y) ?
		            ((x > fabs(m_mirrorX[2])) ? mirrorObjWorldScale[0] : mirrorObjWorldScale[2]):
		            ((y > fabs(m_mirrorX[2])) ? mirrorObjWorldScale[1] : mirrorObjWorldScale[2]);
		width *= m_mirrorHalfWidth;
		height *= m_mirrorHalfHeight;
		//   left = offsetx-width
		//   right = offsetx+width
		//   top = offsety+height
		//   bottom = offsety-height
		//   near = -offsetz
		//   far = near+100
		frustum.x1 = mirrorOffset[0]-width;
		frustum.x2 = mirrorOffset[0]+width;
		frustum.y1 = mirrorOffset[1]-height;
		frustum.y2 = mirrorOffset[1]+height;
		frustum.camnear = -mirrorOffset[2];
		frustum.camfar = -mirrorOffset[2]+m_clip;
	}
	// Store settings to be restored later
	const RAS_IRasterizer::StereoMode stereomode = m_rasterizer->GetStereoMode();
	RAS_Rect area = m_canvas->GetWindowArea();

	// The screen area that ImageViewport will copy is also the rendering zone
	if (m_offscreen) {
		// bind the fbo and set the viewport to full size
		m_offscreen->ofs->Bind(RAS_IOffScreen::RAS_OFS_BIND_RENDER);
		// this is needed to stop crashing in canvas check
		m_canvas->UpdateViewPort(0, 0, m_offscreen->ofs->GetWidth(), m_offscreen->ofs->GetHeight());
	}
	else {
		m_canvas->SetViewPort(m_position[0], m_position[1], m_position[0]+m_capSize[0]-1, m_position[1]+m_capSize[1]-1);
	}
	m_canvas->ClearColor(m_background[0], m_background[1], m_background[2], m_background[3]);
	m_canvas->ClearBuffer(RAS_ICanvas::COLOR_BUFFER|RAS_ICanvas::DEPTH_BUFFER);
	m_rasterizer->BeginFrame(m_engine->GetClockTime());
	m_scene->GetWorldInfo()->UpdateWorldSettings();
	m_rasterizer->SetAuxilaryClientInfo(m_scene);
	m_rasterizer->DisplayFog();
	// matrix calculation, don't apply any of the stereo mode
	m_rasterizer->SetStereoMode(RAS_IRasterizer::RAS_STEREO_NOSTEREO);
	if (m_mirror)
	{
		// frustum was computed above
		// get frustum matrix and set projection matrix
		MT_Matrix4x4 projmat = m_rasterizer->GetFrustumMatrix(
		            frustum.x1, frustum.x2, frustum.y1, frustum.y2, frustum.camnear, frustum.camfar);

		m_camera->SetProjectionMatrix(projmat);
	}
	else if (m_camera->hasValidProjectionMatrix()) {
		m_rasterizer->SetProjectionMatrix(m_camera->GetProjectionMatrix());
	}
	else {
		float lens = m_camera->GetLens();
		float sensor_x = m_camera->GetSensorWidth();
		float sensor_y = m_camera->GetSensorHeight();
		float shift_x = m_camera->GetShiftHorizontal();
		float shift_y = m_camera->GetShiftVertical();
		bool orthographic = !m_camera->GetCameraData()->m_perspective;
		float nearfrust = m_camera->GetCameraNear();
		float farfrust = m_camera->GetCameraFar();
		float aspect_ratio = 1.0f;
		Scene *blenderScene = m_scene->GetBlenderScene();
		MT_Matrix4x4 projmat;

		// compute the aspect ratio from frame blender scene settings so that render to texture
		// works the same in Blender and in Blender player
		if (blenderScene->r.ysch != 0)
			aspect_ratio = float(blenderScene->r.xsch*blenderScene->r.xasp) / float(blenderScene->r.ysch*blenderScene->r.yasp);

		if (orthographic) {

			RAS_FramingManager::ComputeDefaultOrtho(
			            nearfrust,
			            farfrust,
			            m_camera->GetScale(),
			            aspect_ratio,
						m_camera->GetSensorFit(),
			            shift_x,
			            shift_y,
			            frustum
			            );

			projmat = m_rasterizer->GetOrthoMatrix(
			            frustum.x1, frustum.x2, frustum.y1, frustum.y2, frustum.camnear, frustum.camfar);
		}
		else {
			RAS_FramingManager::ComputeDefaultFrustum(
			            nearfrust,
			            farfrust,
			            lens,
			            sensor_x,
			            sensor_y,
			            RAS_SENSORFIT_AUTO,
			            shift_x,
			            shift_y,
			            aspect_ratio,
			            frustum);
			
			projmat = m_rasterizer->GetFrustumMatrix(
			            frustum.x1, frustum.x2, frustum.y1, frustum.y2, frustum.camnear, frustum.camfar);
		}
		m_camera->SetProjectionMatrix(projmat);
	}

	MT_Transform camtrans(m_camera->GetWorldToCamera());
	MT_Matrix4x4 viewmat(camtrans);
	
	m_rasterizer->SetViewMatrix(viewmat, m_camera->NodeGetWorldOrientation(), m_camera->NodeGetWorldPosition(), m_camera->NodeGetLocalScaling(), m_camera->GetCameraData()->m_perspective);
	m_camera->SetModelviewMatrix(viewmat);
	// restore the stereo mode now that the matrix is computed
	m_rasterizer->SetStereoMode(stereomode);

	if (m_rasterizer->Stereo())	{
		// stereo mode change render settings that disturb this render, cancel them all
		// we don't need to restore them as they are set before each frame render.
		glDrawBuffer(GL_BACK_LEFT);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDisable(GL_POLYGON_STIPPLE);
	}

	m_scene->CalculateVisibleMeshes(m_rasterizer,m_camera);

	m_engine->UpdateAnimations(m_scene);

	m_scene->RenderBuckets(camtrans, m_rasterizer);

	m_scene->RenderFonts();

	// restore the canvas area now that the render is completed
	m_canvas->GetWindowArea() = area;
	m_canvas->EndFrame();

	// In case multisample is active, blit the FBO
	if (m_offscreen)
		m_offscreen->ofs->Blit();
	// end of all render operations, let's create a sync object just in case
	if (m_sync) {
		// a sync from a previous render, should not happen
		delete m_sync;
		m_sync = NULL;
	}
	m_sync = m_rasterizer->CreateSync(RAS_ISync::RAS_SYNC_TYPE_FENCE);
	// remember that we have done render
	m_done = true;
	// the image is not available at this stage
	m_avail = false;
	return true;
}

void ImageRender::Unbind()
{
	if (m_offscreen)
	{
		m_offscreen->ofs->Unbind();
	}
}

void ImageRender::WaitSync()
{
	if (m_sync) {
		m_sync->Wait();
		// done with it, deleted it
		delete m_sync;
		m_sync = NULL;
	}
	if (m_offscreen) {
		// this is needed to finalize the image if the target is a texture
		m_offscreen->ofs->MipMap();
	}
	// all rendered operation done and complete, invalidate render for next time
	m_done = false;
}

// cast Image pointer to ImageRender
inline ImageRender * getImageRender (PyImage *self)
{ return static_cast<ImageRender*>(self->m_image); }


// python methods

// Blender Scene type
static BlendType<KX_Scene> sceneType ("KX_Scene");
// Blender Camera type
static BlendType<KX_Camera> cameraType ("KX_Camera");


// object initialization
static int ImageRender_init(PyObject *pySelf, PyObject *args, PyObject *kwds)
{
	// parameters - scene object
	PyObject *scene;
	// camera object
	PyObject *camera;
	// offscreen buffer object
	PyRASOffScreen *offscreen = NULL;
	// parameter keywords
	static const char *kwlist[] = {"sceneObj", "cameraObj", "ofsObj", NULL};
	// get parameters
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO|O",
		const_cast<char**>(kwlist), &scene, &camera, &offscreen))
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

		if (offscreen) {
			if (Py_TYPE(offscreen) != &PyRASOffScreen_Type) {
				THRWEXCP(OffScreenInvalid, S_OK);
			}
		}
		// get pointer to image structure
		PyImage *self = reinterpret_cast<PyImage*>(pySelf);
		// create source object
		if (self->m_image != NULL) delete self->m_image;
		self->m_image = new ImageRender(scenePtr, cameraPtr, offscreen);
	}
	catch (Exception & exp)
	{
		exp.report();
		return -1;
	}
	// initialization succeded
	return 0;
}

static PyObject *ImageRender_refresh(PyImage *self, PyObject *args)
{
	ImageRender *imageRender = getImageRender(self);

	if (!imageRender) {
		PyErr_SetString(PyExc_TypeError, "Incomplete ImageRender() object");
		return NULL;
	}
	if (PyArg_ParseTuple(args, "")) {
		// refresh called with no argument.
		// For other image objects it simply invalidates the image buffer
		// For ImageRender it triggers a render+sync
		// Note that this only makes sense when doing offscreen render on texture
		if (!imageRender->isDone()) {
			if (!imageRender->Render()) {
				Py_RETURN_FALSE;
			}
			// as we are not trying to read the pixels, just unbind
			imageRender->Unbind();
		}
		// wait until all render operations are completed
		// this will also finalize the texture
		imageRender->WaitSync();
		Py_RETURN_TRUE;
	}
	else {
		// fallback on standard processing
		PyErr_Clear();
		return Image_refresh(self, args);
	}
}

// refresh image
static PyObject *ImageRender_render(PyImage *self)
{
	ImageRender *imageRender = getImageRender(self);

	if (!imageRender) {
		PyErr_SetString(PyExc_TypeError, "Incomplete ImageRender() object");
		return NULL;
	}
	if (!imageRender->Render()) {
		Py_RETURN_FALSE;
	}
	// we are not reading the pixels now, unbind
	imageRender->Unbind();
	Py_RETURN_TRUE;
}


// get background color
static PyObject *getBackground (PyImage *self, void *closure)
{
	return Py_BuildValue("[ffff]",
	                     getImageRender(self)->getBackground(0),
	                     getImageRender(self)->getBackground(1),
	                     getImageRender(self)->getBackground(2),
	                     getImageRender(self)->getBackground(3));
}

// set color
static int setBackground(PyImage *self, PyObject *value, void *closure)
{
	// check validity of parameter
	if (value == NULL || !PySequence_Check(value) || PySequence_Size(value) != 4
		|| (!PyFloat_Check(PySequence_Fast_GET_ITEM(value, 0)) && !PyLong_Check(PySequence_Fast_GET_ITEM(value, 0)))
		|| (!PyFloat_Check(PySequence_Fast_GET_ITEM(value, 1)) && !PyLong_Check(PySequence_Fast_GET_ITEM(value, 1)))
		|| (!PyFloat_Check(PySequence_Fast_GET_ITEM(value, 2)) && !PyLong_Check(PySequence_Fast_GET_ITEM(value, 2)))
		|| (!PyFloat_Check(PySequence_Fast_GET_ITEM(value, 3)) && !PyLong_Check(PySequence_Fast_GET_ITEM(value, 3)))) {

		PyErr_SetString(PyExc_TypeError, "The value must be a sequence of 4 floats or ints between 0.0 and 255.0");
		return -1;
	}
	// set background color
	getImageRender(self)->setBackground(
	        PyFloat_AsDouble(PySequence_Fast_GET_ITEM(value, 0)),
	        PyFloat_AsDouble(PySequence_Fast_GET_ITEM(value, 1)),
	        PyFloat_AsDouble(PySequence_Fast_GET_ITEM(value, 2)),
	        PyFloat_AsDouble(PySequence_Fast_GET_ITEM(value, 3)));
	// success
	return 0;
}


// methods structure
static PyMethodDef imageRenderMethods[] =
{ // methods from ImageBase class
	{"refresh", (PyCFunction)ImageRender_refresh, METH_VARARGS, "Refresh image - invalidate its current content after optionally transferring its content to a target buffer"},
	{"render", (PyCFunction)ImageRender_render, METH_NOARGS, "Render scene - run before refresh() to performs asynchronous render"},
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
	{(char*)"valid", (getter)Image_valid, NULL, (char*)"bool to tell if an image is available", NULL},
	{(char*)"image", (getter)Image_getImage, NULL, (char*)"image data", NULL},
	{(char*)"size", (getter)Image_getSize, NULL, (char*)"image size", NULL},
	{(char*)"scale", (getter)Image_getScale, (setter)Image_setScale, (char*)"fast scale of image (near neighbor)",	NULL},
	{(char*)"flip", (getter)Image_getFlip, (setter)Image_setFlip, (char*)"flip image vertically", NULL},
	{(char*)"zbuff", (getter)Image_getZbuff, (setter)Image_setZbuff, (char*)"use depth buffer as texture", NULL},
	{(char*)"depth", (getter)Image_getDepth, (setter)Image_setDepth, (char*)"get depth information from z-buffer using unsigned int precision", NULL},
	{(char*)"filter", (getter)Image_getFilter, (setter)Image_setFilter, (char*)"pixel filter", NULL},
	{NULL}
};


// define python type
PyTypeObject ImageRenderType = {
	PyVarObject_HEAD_INIT(NULL, 0)
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
	&imageBufferProcs,         /*tp_as_buffer*/
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

// object initialization
static int ImageMirror_init(PyObject *pySelf, PyObject *args, PyObject *kwds)
{
	// parameters - scene object
	PyObject *scene;
	// reference object for mirror
	PyObject *observer;
	// object holding the mirror
	PyObject *mirror;
	// material of the mirror
	short materialID = 0;
	// parameter keywords
	static const char *kwlist[] = {"scene", "observer", "mirror", "material", NULL};
	// get parameters
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "OOO|h",
	                                 const_cast<char**>(kwlist), &scene, &observer, &mirror, &materialID))
		return -1;
	try
	{
		// get scene pointer
		KX_Scene * scenePtr (NULL);
		if (scene != NULL && PyObject_TypeCheck(scene, &KX_Scene::Type))
			scenePtr = static_cast<KX_Scene*>BGE_PROXY_REF(scene);
		else
			THRWEXCP(SceneInvalid, S_OK);
		
		if (scenePtr==NULL) /* in case the python proxy reference is invalid */
			THRWEXCP(SceneInvalid, S_OK);
		
		// get observer pointer
		KX_GameObject * observerPtr (NULL);
		if (observer != NULL && PyObject_TypeCheck(observer, &KX_GameObject::Type))
			observerPtr = static_cast<KX_GameObject*>BGE_PROXY_REF(observer);
		else if (observer != NULL && PyObject_TypeCheck(observer, &KX_Camera::Type))
			observerPtr = static_cast<KX_Camera*>BGE_PROXY_REF(observer);
		else
			THRWEXCP(ObserverInvalid, S_OK);
		
		if (observerPtr==NULL) /* in case the python proxy reference is invalid */
			THRWEXCP(ObserverInvalid, S_OK);

		// get mirror pointer
		KX_GameObject * mirrorPtr (NULL);
		if (mirror != NULL && PyObject_TypeCheck(mirror, &KX_GameObject::Type))
			mirrorPtr = static_cast<KX_GameObject*>BGE_PROXY_REF(mirror);
		else
			THRWEXCP(MirrorInvalid, S_OK);
		
		if (mirrorPtr==NULL) /* in case the python proxy reference is invalid */
			THRWEXCP(MirrorInvalid, S_OK);

		// locate the material in the mirror
		RAS_IPolyMaterial * material = getMaterial(mirror, materialID);
		if (material == NULL)
			THRWEXCP(MaterialNotAvail, S_OK);

		// get pointer to image structure
		PyImage *self = reinterpret_cast<PyImage*>(pySelf);

		// create source object
		if (self->m_image != NULL)
		{
			delete self->m_image;
			self->m_image = NULL;
		}
		self->m_image = new ImageRender(scenePtr, observerPtr, mirrorPtr, material);
	}
	catch (Exception & exp)
	{
		exp.report();
		return -1;
	}
	// initialization succeeded
	return 0;
}

// get background color
static PyObject *getClip (PyImage *self, void *closure)
{
	return PyFloat_FromDouble(getImageRender(self)->getClip());
}

// set clip
static int setClip(PyImage *self, PyObject *value, void *closure)
{
	// check validity of parameter
	double clip;
	if (value == NULL || !PyFloat_Check(value) || (clip = PyFloat_AsDouble(value)) < 0.01 || clip > 5000.0)
	{
		PyErr_SetString(PyExc_TypeError, "The value must be an float between 0.01 and 5000");
		return -1;
	}
	// set background color
	getImageRender(self)->setClip(float(clip));
	// success
	return 0;
}

// attributes structure
static PyGetSetDef imageMirrorGetSets[] =
{ 
	{(char*)"clip", (getter)getClip, (setter)setClip, (char*)"clipping distance", NULL},
	// attribute from ImageRender
	{(char*)"background", (getter)getBackground, (setter)setBackground, (char*)"background color", NULL},
	// attribute from ImageViewport
	{(char*)"capsize", (getter)ImageViewport_getCaptureSize, (setter)ImageViewport_setCaptureSize, (char*)"size of render area", NULL},
	{(char*)"alpha", (getter)ImageViewport_getAlpha, (setter)ImageViewport_setAlpha, (char*)"use alpha in texture", NULL},
	{(char*)"whole", (getter)ImageViewport_getWhole, (setter)ImageViewport_setWhole, (char*)"use whole viewport to render", NULL},
	// attributes from ImageBase class
	{(char*)"valid", (getter)Image_valid, NULL, (char*)"bool to tell if an image is available", NULL},
	{(char*)"image", (getter)Image_getImage, NULL, (char*)"image data", NULL},
	{(char*)"size", (getter)Image_getSize, NULL, (char*)"image size", NULL},
	{(char*)"scale", (getter)Image_getScale, (setter)Image_setScale, (char*)"fast scale of image (near neighbor)",	NULL},
	{(char*)"flip", (getter)Image_getFlip, (setter)Image_setFlip, (char*)"flip image vertically", NULL},
	{(char*)"zbuff", (getter)Image_getZbuff, (setter)Image_setZbuff, (char*)"use depth buffer as texture", NULL},
	{(char*)"depth", (getter)Image_getDepth, (setter)Image_setDepth, (char*)"get depth information from z-buffer using unsigned int precision", NULL},
	{(char*)"filter", (getter)Image_getFilter, (setter)Image_setFilter, (char*)"pixel filter", NULL},
	{NULL}
};


// constructor
ImageRender::ImageRender (KX_Scene *scene, KX_GameObject *observer, KX_GameObject *mirror, RAS_IPolyMaterial *mat) :
    ImageViewport(),
    m_render(false),
    m_done(false),
    m_scene(scene),
    m_offscreen(NULL),
    m_sync(NULL),
    m_observer(observer),
    m_mirror(mirror),
    m_clip(100.f)
{
	// this constructor is used for automatic planar mirror
	// create a camera, take all data by default, in any case we will recompute the frustum on each frame
	RAS_CameraData camdata;
	vector<RAS_TexVert*> mirrorVerts;
	vector<RAS_TexVert*>::iterator it;
	float mirrorArea = 0.f;
	float mirrorNormal[3] = {0.f, 0.f, 0.f};
	float mirrorUp[3];
	float dist, vec[3], axis[3];
	float zaxis[3] = {0.f, 0.f, 1.f};
	float yaxis[3] = {0.f, 1.f, 0.f};
	float mirrorMat[3][3];
	float left, right, top, bottom, back;
	// make sure this camera will delete its node
	m_camera= new KX_Camera(scene, KX_Scene::m_callbacks, camdata, true, true);
	m_camera->SetName("__mirror__cam__");
	// don't add the camera to the scene object list, it doesn't need to be accessible
	m_owncamera = true;
	// retrieve rendering objects
	m_engine = KX_GetActiveEngine();
	m_rasterizer = m_engine->GetRasterizer();
	m_canvas = m_engine->GetCanvas();
	// locate the vertex assigned to mat and do following calculation in mesh coordinates
	for (int meshIndex = 0; meshIndex < mirror->GetMeshCount(); meshIndex++)
	{
		RAS_MeshObject*	mesh = mirror->GetMesh(meshIndex);
		int numPolygons = mesh->NumPolygons();
		for (int polygonIndex=0; polygonIndex < numPolygons; polygonIndex++)
		{
			RAS_Polygon* polygon = mesh->GetPolygon(polygonIndex);
			if (polygon->GetMaterial()->GetPolyMaterial() == mat)
			{
				RAS_TexVert *v1, *v2, *v3, *v4;
				float normal[3];
				float area;
				// this polygon is part of the mirror
				v1 = polygon->GetVertex(0);
				v2 = polygon->GetVertex(1);
				v3 = polygon->GetVertex(2);
				mirrorVerts.push_back(v1);
				mirrorVerts.push_back(v2);
				mirrorVerts.push_back(v3);
				if (polygon->VertexCount() == 4) {
					v4 = polygon->GetVertex(3);
					mirrorVerts.push_back(v4);
					area = normal_quad_v3(normal,(float*)v1->getXYZ(), (float*)v2->getXYZ(), (float*)v3->getXYZ(), (float*)v4->getXYZ());
				}
				else {
					area = normal_tri_v3(normal,(float*)v1->getXYZ(), (float*)v2->getXYZ(), (float*)v3->getXYZ());
				}
				area = fabs(area);
				mirrorArea += area;
				mul_v3_fl(normal, area);
				add_v3_v3v3(mirrorNormal, mirrorNormal, normal);
			}
		}
	}
	if (mirrorVerts.size() == 0 || mirrorArea < FLT_EPSILON)
	{
		// no vertex or zero size mirror
		THRWEXCP(MirrorSizeInvalid, S_OK);
	}
	// compute average normal of mirror faces
	mul_v3_fl(mirrorNormal, 1.0f/mirrorArea);
	if (normalize_v3(mirrorNormal) == 0.f)
	{
		// no normal
		THRWEXCP(MirrorNormalInvalid, S_OK);
	}
	// the mirror plane has an equation of the type ax+by+cz = d where (a,b,c) is the normal vector
	// if the mirror is more vertical then horizontal, the Z axis is the up direction.
	// otherwise the Y axis is the up direction.
	// If the mirror is not perfectly vertical(horizontal), the Z(Y) axis projection on the mirror
	// plan by the normal will be the up direction.
	if (fabsf(mirrorNormal[2]) > fabsf(mirrorNormal[1]) &&
	    fabsf(mirrorNormal[2]) > fabsf(mirrorNormal[0]))
	{
		// the mirror is more horizontal than vertical
		copy_v3_v3(axis, yaxis);
	}
	else
	{
		// the mirror is more vertical than horizontal
		copy_v3_v3(axis, zaxis);
	}
	dist = dot_v3v3(mirrorNormal, axis);
	if (fabsf(dist) < FLT_EPSILON)
	{
		// the mirror is already fully aligned with up axis
		copy_v3_v3(mirrorUp, axis);
	}
	else
	{
		// projection of axis to mirror plane through normal
		copy_v3_v3(vec, mirrorNormal);
		mul_v3_fl(vec, dist);
		sub_v3_v3v3(mirrorUp, axis, vec);
		if (normalize_v3(mirrorUp) == 0.f)
		{
			// should not happen
			THRWEXCP(MirrorHorizontal, S_OK);
			return;
		}
	}
	// compute rotation matrix between local coord and mirror coord
	// to match camera orientation, we select mirror z = -normal, y = up, x = y x z
	negate_v3_v3(mirrorMat[2], mirrorNormal);
	copy_v3_v3(mirrorMat[1], mirrorUp);
	cross_v3_v3v3(mirrorMat[0], mirrorMat[1], mirrorMat[2]);
	// transpose to make it a orientation matrix from local space to mirror space
	transpose_m3(mirrorMat);
	// transform all vertex to plane coordinates and determine mirror position
	left = FLT_MAX;
	right = -FLT_MAX;
	bottom = FLT_MAX;
	top = -FLT_MAX;
	back = -FLT_MAX; // most backward vertex (=highest Z coord in mirror space)
	for (it = mirrorVerts.begin(); it != mirrorVerts.end(); it++)
	{
		copy_v3_v3(vec, (float*)(*it)->getXYZ());
		mul_m3_v3(mirrorMat, vec);
		if (vec[0] < left)
			left = vec[0];
		if (vec[0] > right)
			right = vec[0];
		if (vec[1] < bottom)
			bottom = vec[1];
		if (vec[1] > top)
			top = vec[1];
		if (vec[2] > back)
			back = vec[2];
	}
	// now store this information in the object for later rendering
	m_mirrorHalfWidth = (right-left)*0.5f;
	m_mirrorHalfHeight = (top-bottom)*0.5f;
	if (m_mirrorHalfWidth < 0.01f || m_mirrorHalfHeight < 0.01f)
	{
		// mirror too small
		THRWEXCP(MirrorTooSmall, S_OK);
	}
	// mirror position in mirror coord
	vec[0] = (left+right)*0.5f;
	vec[1] = (top+bottom)*0.5f;
	vec[2] = back;
	// convert it in local space: transpose again the matrix to get back to mirror to local transform
	transpose_m3(mirrorMat);
	mul_m3_v3(mirrorMat, vec);
	// mirror position in local space
	m_mirrorPos.setValue(vec[0], vec[1], vec[2]);
	// mirror normal vector (pointed towards the back of the mirror) in local space
	m_mirrorZ.setValue(-mirrorNormal[0], -mirrorNormal[1], -mirrorNormal[2]);
	m_mirrorY.setValue(mirrorUp[0], mirrorUp[1], mirrorUp[2]);
	m_mirrorX = m_mirrorY.cross(m_mirrorZ);
	m_render = true;

	// set mirror background color to scene background color as default
	setBackgroundFromScene(m_scene);
}




// define python type
PyTypeObject ImageMirrorType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"VideoTexture.ImageMirror",   /*tp_name*/
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
	&imageBufferProcs,         /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT,        /*tp_flags*/
	"Image source from mirror",       /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	imageRenderMethods,    /* tp_methods */
	0,                   /* tp_members */
	imageMirrorGetSets,          /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	(initproc)ImageMirror_init,     /* tp_init */
	0,                         /* tp_alloc */
	Image_allocNew,           /* tp_new */
};


