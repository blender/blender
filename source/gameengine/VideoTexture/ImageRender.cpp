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
#include <float.h>
#include <math.h>


#include "GL/glew.h"

#include "KX_PythonInit.h"
#include "DNA_scene_types.h"
#include "RAS_CameraData.h"
#include "RAS_MeshObject.h"
#include "BLI_math.h"

#include "ImageRender.h"
#include "ImageBase.h"
#include "BlendType.h"
#include "Exception.h"
#include "Texture.h"

ExceptionID SceneInvalid, CameraInvalid, ObserverInvalid;
ExceptionID MirrorInvalid, MirrorSizeInvalid, MirrorNormalInvalid, MirrorHorizontal, MirrorTooSmall;
ExpDesc SceneInvalidDesc (SceneInvalid, "Scene object is invalid");
ExpDesc CameraInvalidDesc (CameraInvalid, "Camera object is invalid");
ExpDesc ObserverInvalidDesc (ObserverInvalid, "Observer object is invalid");
ExpDesc MirrorInvalidDesc (MirrorInvalid, "Mirror object is invalid");
ExpDesc MirrorSizeInvalidDesc (MirrorSizeInvalid, "Mirror has no vertex or no size");
ExpDesc MirrorNormalInvalidDesc (MirrorNormalInvalid, "Cannot determine mirror plane");
ExpDesc MirrorHorizontalDesc (MirrorHorizontal, "Mirror is horizontal in local space");
ExpDesc MirrorTooSmallDesc (MirrorTooSmall, "Mirror is too small");

// constructor
ImageRender::ImageRender (KX_Scene * scene, KX_Camera * camera) : 
    ImageViewport(),
    m_render(true),
    m_scene(scene),
    m_camera(camera),
    m_owncamera(false),
    m_observer(NULL),
    m_mirror(NULL),
	m_clip(100.f)
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
    if (m_owncamera)
        m_camera->Release();
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
void ImageRender::calcImage (unsigned int texId, double ts)
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
	ImageViewport::calcImage(texId, ts);
    // restore OpenGL state
    m_canvas->EndFrame();
}

void ImageRender::Render()
{
	RAS_FrameFrustum frustrum;

    if (!m_render)
        return;

    if (m_mirror)
    {
        // mirror mode, compute camera frustrum, position and orientation
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
        if (observerDistance < 0.01f)
            return;
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
        // compute camera frustrum:
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
        frustrum.x1 = mirrorOffset[0]-width;
        frustrum.x2 = mirrorOffset[0]+width;
        frustrum.y1 = mirrorOffset[1]-height;
        frustrum.y2 = mirrorOffset[1]+height;
        frustrum.camnear = -mirrorOffset[2];
        frustrum.camfar = -mirrorOffset[2]+m_clip;
    }
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
    if (m_mirror)
    {
        // frustrum was computed above
        // get frustrum matrix and set projection matrix
		MT_Matrix4x4 projmat = m_rasterizer->GetFrustumMatrix(
			frustrum.x1, frustrum.x2, frustrum.y1, frustrum.y2, frustrum.camnear, frustrum.camfar);

		m_camera->SetProjectionMatrix(projmat);
    } else if (m_camera->hasValidProjectionMatrix())
	{
		m_rasterizer->SetProjectionMatrix(m_camera->GetProjectionMatrix());
    } else 
    {
		float lens = m_camera->GetLens();
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
				frustrum
			);

			projmat = m_rasterizer->GetOrthoMatrix(
				frustrum.x1, frustrum.x2, frustrum.y1, frustrum.y2, frustrum.camnear, frustrum.camfar);
		} else 
		{
			RAS_FramingManager::ComputeDefaultFrustum(
				nearfrust,
				farfrust,
				lens,
				aspect_ratio,
				frustrum);
			
			projmat = m_rasterizer->GetFrustumMatrix(
				frustrum.x1, frustrum.x2, frustrum.y1, frustrum.y2, frustrum.camnear, frustrum.camfar);
		}
		m_camera->SetProjectionMatrix(projmat);
	}

	MT_Transform camtrans(m_camera->GetWorldToCamera());
	MT_Matrix4x4 viewmat(camtrans);
	
	m_rasterizer->SetViewMatrix(viewmat, m_camera->NodeGetWorldOrientation(), m_camera->NodeGetWorldPosition(), m_camera->GetCameraData()->m_perspective);
	m_camera->SetModelviewMatrix(viewmat);
    // restore the stereo mode now that the matrix is computed
    m_rasterizer->SetStereoMode(stereomode);

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
	static const char *kwlist[] = {"sceneObj", "cameraObj", NULL};
	// get parameters
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO",
		const_cast<char**>(kwlist), &scene, &camera))
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
		|| !PyLong_Check(PySequence_Fast_GET_ITEM(value, 0))
		|| !PyLong_Check(PySequence_Fast_GET_ITEM(value, 1))
		|| !PyLong_Check(PySequence_Fast_GET_ITEM(value, 2))
		|| !PyLong_Check(PySequence_Fast_GET_ITEM(value, 3)))
	{
		PyErr_SetString(PyExc_TypeError, "The value must be a sequence of 4 integer between 0 and 255");
		return -1;
	}
	// set background color
	getImageRender(self)->setBackground((unsigned char)(PyLong_AsSsize_t(PySequence_Fast_GET_ITEM(value, 0))),
		(unsigned char)(PyLong_AsSsize_t(PySequence_Fast_GET_ITEM(value, 1))),
		(unsigned char)(PyLong_AsSsize_t(PySequence_Fast_GET_ITEM(value, 2))),
        (unsigned char)(PyLong_AsSsize_t(PySequence_Fast_GET_ITEM(value, 3))));
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
	{(char*)"valid", (getter)Image_valid, NULL, (char*)"bool to tell if an image is available", NULL},
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
static int ImageMirror_init (PyObject * pySelf, PyObject * args, PyObject * kwds)
{
	// parameters - scene object
	PyObject * scene;
	// reference object for mirror
	PyObject * observer;
    // object holding the mirror
    PyObject * mirror;
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
		
		if(scenePtr==NULL) /* incase the python proxy reference is invalid */
			THRWEXCP(SceneInvalid, S_OK);
		
		// get observer pointer
		KX_GameObject * observerPtr (NULL);
		if (observer != NULL && PyObject_TypeCheck(observer, &KX_GameObject::Type))
            observerPtr = static_cast<KX_GameObject*>BGE_PROXY_REF(observer);
        else if (observer != NULL && PyObject_TypeCheck(observer, &KX_Camera::Type))
            observerPtr = static_cast<KX_Camera*>BGE_PROXY_REF(observer);
		else
            THRWEXCP(ObserverInvalid, S_OK);
		
		if(observerPtr==NULL) /* incase the python proxy reference is invalid */
			THRWEXCP(ObserverInvalid, S_OK);

		// get mirror pointer
		KX_GameObject * mirrorPtr (NULL);
		if (mirror != NULL && PyObject_TypeCheck(mirror, &KX_GameObject::Type))
            mirrorPtr = static_cast<KX_GameObject*>BGE_PROXY_REF(mirror);
		else
            THRWEXCP(MirrorInvalid, S_OK);
		
		if(mirrorPtr==NULL) /* incase the python proxy reference is invalid */
			THRWEXCP(MirrorInvalid, S_OK);

        // locate the material in the mirror
		RAS_IPolyMaterial * material = getMaterial(mirror, materialID);
		if (material == NULL)
            THRWEXCP(MaterialNotAvail, S_OK);

		// get pointer to image structure
		PyImage * self = reinterpret_cast<PyImage*>(pySelf);

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
	// initialization succeded
	return 0;
}

// get background color
PyObject * getClip (PyImage * self, void * closure)
{
	return PyFloat_FromDouble(getImageRender(self)->getClip());
}

// set clip
static int setClip (PyImage * self, PyObject * value, void * closure)
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
	{(char*)"scale", (getter)Image_getScale, (setter)Image_setScale, (char*)"fast scale of image (near neighbour)",	NULL},
	{(char*)"flip", (getter)Image_getFlip, (setter)Image_setFlip, (char*)"flip image vertically", NULL},
	{(char*)"filter", (getter)Image_getFilter, (setter)Image_setFilter, (char*)"pixel filter", NULL},
	{NULL}
};


// constructor
ImageRender::ImageRender (KX_Scene * scene, KX_GameObject * observer, KX_GameObject * mirror, RAS_IPolyMaterial * mat) :
    ImageViewport(),
    m_render(false),
    m_scene(scene),
    m_observer(observer),
    m_mirror(mirror),
	m_clip(100.f)
{
    // this constructor is used for automatic planar mirror
    // create a camera, take all data by default, in any case we will recompute the frustrum on each frame
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
    m_rendertools = m_engine->GetRenderTools();
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
                // this polygon is part of the mirror,
                v1 = polygon->GetVertex(0);
                v2 = polygon->GetVertex(1);
                v3 = polygon->GetVertex(2);
                mirrorVerts.push_back(v1);
                mirrorVerts.push_back(v2);
                mirrorVerts.push_back(v3);
                if (polygon->VertexCount() == 4) 
                {
                    v4 = polygon->GetVertex(3);
                    mirrorVerts.push_back(v4);
                    area = normal_quad_v3( normal,(float*)v1->getXYZ(), (float*)v2->getXYZ(), (float*)v3->getXYZ(), (float*)v4->getXYZ());
                } else
                {
                    area = normal_tri_v3( normal,(float*)v1->getXYZ(), (float*)v2->getXYZ(), (float*)v3->getXYZ());
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
	if (fabs(mirrorNormal[2]) > fabs(mirrorNormal[1]) &&
		fabs(mirrorNormal[2]) > fabs(mirrorNormal[0]))
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
    if (fabs(dist) < FLT_EPSILON)
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
    copy_v3_v3(mirrorMat[2], mirrorNormal);
    mul_v3_fl(mirrorMat[2], -1.0f);
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

	setBackground(0, 0, 255, 255);
}




// define python type
PyTypeObject ImageMirrorType =
{ 
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


