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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 * The engine ties all game modules together. 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#pragma warning (disable : 4786)
#endif //WIN32

#include <iostream>

#include "KX_KetsjiEngine.h"

#include "ListValue.h"
#include "IntValue.h"
#include "VectorValue.h"
#include "BoolValue.h"
#include "FloatValue.h"

#define KX_NUM_ITERATIONS 4
#include "RAS_BucketManager.h"
#include "RAS_Rect.h"
#include "RAS_IRasterizer.h"
#include "RAS_IRenderTools.h"
#include "RAS_ICanvas.h"
#include "STR_String.h"
#include "MT_Vector3.h"
#include "MT_Transform.h"
#include "SCA_IInputDevice.h"
#include "KX_Scene.h"
#include "MT_CmMatrix4x4.h"
#include "KX_Camera.h"
#include "KX_PythonInit.h"
#include "KX_PyConstraintBinding.h"
#include "PHY_IPhysicsEnvironment.h"

#include "SND_Scene.h"
#include "SND_IAudioDevice.h"

#include "NG_NetworkScene.h"
#include "NG_NetworkDeviceInterface.h"

#include "KX_WorldInfo.h"
#include "KX_ISceneConverter.h"
#include "KX_TimeCategoryLogger.h"

#include "RAS_FramingManager.h"

// If define: little test for Nzc: guarded drawing. If the canvas is
// not valid, skip rendering this frame.
//#define NZC_GUARDED_OUTPUT


const char KX_KetsjiEngine::m_profileLabels[tc_numCategories][15] = {
	"Physics:",		// tc_physics
	"Logic",		// tc_logic
	"Network:",		// tc_network
	"Scenegraph:",	// tc_scenegraph
	"Sound:",		// tc_sound
	"Rasterizer:",	// tc_rasterizer
	"Services:",	// tc_services
	"Overhead:",	// tc_overhead
	"Outside:"		// tc_outside
};




/**
 *	Constructor of the Ketsji Engine
 */
KX_KetsjiEngine::KX_KetsjiEngine(KX_ISystem* system)
:
	m_rasterizer(NULL),
	m_bInitialized(false),
	m_activecam(0)
{
	m_kxsystem = system;
	m_bFixedTime = false;

	// Initialize the time logger
	m_logger = new KX_TimeCategoryLogger (25);

	for (int i = tc_first; i < tc_numCategories; i++)
		m_logger->AddCategory((KX_TimeCategory)i);

	// Set up timing info display variables
	m_show_framerate = false;
	m_show_profile   = false;
	m_show_debug_properties = false;
	m_propertiesPresent = false;

	// Default behavior is to hide the cursor every frame.
	m_hideCursor = false;

	m_overrideFrameColor = false;
	m_overrideFrameColorR = (float)0;
	m_overrideFrameColorG = (float)0;
	m_overrideFrameColorB = (float)0;
	
	m_cameraZoom = 1.0;
	m_drawingmode = 5; /* textured drawing mode */
	m_overrideCam = false;

	m_exitcode = KX_EXIT_REQUEST_NO_REQUEST;
	m_exitstring = "";
}



/**
 *	Destructor of the Ketsji Engine, release all memory
 */
KX_KetsjiEngine::~KX_KetsjiEngine()
{
	if (m_logger)
		delete m_logger;
}



void KX_KetsjiEngine::SetKeyboardDevice(SCA_IInputDevice* keyboarddevice)
{
	assert(keyboarddevice);
	m_keyboarddevice = keyboarddevice;
}



void KX_KetsjiEngine::SetMouseDevice(SCA_IInputDevice* mousedevice)
{
	assert(mousedevice);
	m_mousedevice = mousedevice;
}



void KX_KetsjiEngine::SetNetworkDevice(NG_NetworkDeviceInterface* networkdevice)
{
	assert(networkdevice);
	m_networkdevice = networkdevice;
}



void KX_KetsjiEngine::SetAudioDevice(SND_IAudioDevice* audiodevice)
{
	assert(audiodevice);
	m_audiodevice = audiodevice;
}



void KX_KetsjiEngine::SetCanvas(RAS_ICanvas* canvas)
{
	assert(canvas);
	m_canvas = canvas;
}



void KX_KetsjiEngine::SetRenderTools(RAS_IRenderTools* rendertools)
{
	assert(rendertools);
	m_rendertools = rendertools;
}



void KX_KetsjiEngine::SetRasterizer(RAS_IRasterizer* rasterizer)
{
	assert(rasterizer);
	m_rasterizer = rasterizer;
}



void KX_KetsjiEngine::SetPythonDictionary(PyObject* pythondictionary)
{
	assert(pythondictionary);
	m_pythondictionary = pythondictionary;
}



void KX_KetsjiEngine::SetSceneConverter(KX_ISceneConverter* sceneconverter)
{
	assert(sceneconverter);
	m_sceneconverter = sceneconverter;
}



/**
 * Ketsji Init(), Initializes datastructures and converts data from
 * Blender into Ketsji native (realtime) format also sets up the
 * graphics context
 */
void KX_KetsjiEngine::StartEngine()
{
	m_previoustime = 0.0;
	m_missedtime = 0.0;
	m_firstframe = true;
	
	// for all scenes, initialize the scenegraph for the first time
	m_lasttime = m_kxsystem->GetTimeInSeconds()*100.0;

	m_bInitialized = true;
}



#define DELTALENGTH 25 

double	KX_KetsjiEngine::CalculateAverage(double newdelta)
{
	if (m_deltatimes.size() < DELTALENGTH)
	{
		m_deltatimes.push_back(newdelta);
	} else
	{
		//
		double totaltime = 0.0;
		double newlasttime,lasttime = newdelta;
		double peakmin = 10000;
		double peakmax = -10000;

		for (int i=m_deltatimes.size()-1;i>=0;i--)
		{	newlasttime = m_deltatimes[i];
			totaltime += newlasttime;
			if (peakmin > newlasttime)
				peakmin = newlasttime;
			if (peakmax < newlasttime)
				peakmax = newlasttime;

			m_deltatimes[i] = lasttime;
			lasttime = newlasttime;
		};
		double averagetime;
		
		if (peakmin < peakmax)
		{
		 	averagetime = ((totaltime - peakmin) - peakmax) / (double) (m_deltatimes.size()-2); 
		} else
		{	
			averagetime = totaltime / (double) m_deltatimes.size();
		}
		return averagetime;
	}	

	return newdelta;
}



bool KX_KetsjiEngine::BeginFrame()
{
	bool result = false;

	RAS_Rect vp;
	KX_Scene* firstscene = *m_scenes.begin();
	const RAS_FrameSettings &framesettings = firstscene->GetFramingType();

	// set the area used for rendering
	m_rasterizer->SetRenderArea();

	RAS_FramingManager::ComputeViewport(framesettings, m_canvas->GetDisplayArea(), vp);

	if (m_canvas->BeginDraw())
	{
		result = true;

		m_canvas->SetViewPort(vp.GetLeft(), vp.GetBottom(), vp.GetRight(), vp.GetTop());
		SetBackGround( firstscene->GetWorldInfo() );
		m_rasterizer->BeginFrame( m_drawingmode , m_kxsystem->GetTimeInSeconds());
		m_rendertools->BeginFrame( m_rasterizer);
	}
	
	return result;
}		


void KX_KetsjiEngine::EndFrame()
{
	// Show profiling info
	m_logger->StartLog(tc_overhead, m_kxsystem->GetTimeInSeconds(), true);
	if (m_show_framerate || m_show_profile || (m_show_debug_properties && m_propertiesPresent))
	{
		RenderDebugProperties();
	}
	// Go to next profiling measurement, time spend after this call is shown in the next frame.
	m_logger->NextMeasurement(m_kxsystem->GetTimeInSeconds());

	m_logger->StartLog(tc_rasterizer, m_kxsystem->GetTimeInSeconds(), true);
	m_rasterizer->EndFrame();
	// swap backbuffer (drawing into this buffer) <-> front/visible buffer
	m_rasterizer->SwapBuffers();
	m_rendertools->EndFrame(m_rasterizer);
	
	m_canvas->EndDraw();
}



void KX_KetsjiEngine::NextFrame()
{
	m_logger->StartLog(tc_services, m_kxsystem->GetTimeInSeconds(), true);

	double deltatime = 0.02; 
	double curtime;

	if (m_bFixedTime)
	{
		curtime = m_previoustime + deltatime;
	}
	else
	{
		curtime = m_kxsystem->GetTimeInSeconds();
		if (m_previoustime)
			deltatime = curtime - m_previoustime;

		if (deltatime > 0.1)
			deltatime = 0.1;

		deltatime = CalculateAverage(deltatime);
	}

	m_previoustime = curtime;

	KX_SceneList::iterator sceneit;
	for (sceneit = m_scenes.begin();sceneit != m_scenes.end(); sceneit++)
	// for each scene, call the proceed functions
	{
		KX_Scene* scene = *sceneit;



		/* Suspension holds the physics and logic processing for an
		 * entire scene. Objects can be suspended individually, and
		 * the settings for that preceed the logic and physics
		 * update. */
		m_logger->StartLog(tc_logic, m_kxsystem->GetTimeInSeconds(), true);
		scene->UpdateObjectActivity();

		if (!scene->IsSuspended())
		{
			m_logger->StartLog(tc_network, m_kxsystem->GetTimeInSeconds(), true);
			scene->GetNetworkScene()->proceed(curtime, deltatime);

			// set Python hooks for each scene
			PHY_SetActiveEnvironment(scene->GetPhysicsEnvironment());
			PHY_SetActiveScene(scene);

			// Process sensors, and controllers
			m_logger->StartLog(tc_logic, m_kxsystem->GetTimeInSeconds(), true);
			scene->LogicBeginFrame(curtime,deltatime);

			// Scenegraph needs to be updated again, because Logic Controllers 
			// can affect the local matrices.
			m_logger->StartLog(tc_scenegraph, m_kxsystem->GetTimeInSeconds(), true);
			scene->UpdateParents(curtime);

			// Process actuators

			// Do some cleanup work for this logic frame
			m_logger->StartLog(tc_logic, m_kxsystem->GetTimeInSeconds(), true);
			scene->LogicUpdateFrame(curtime,deltatime);		
			scene->LogicEndFrame();

			// Actuators can affect the scenegraph
			m_logger->StartLog(tc_scenegraph, m_kxsystem->GetTimeInSeconds(), true);
			scene->UpdateParents(curtime);

			// Perform physics calculations on the scene. This can involve 
			// many iterations of the physics solver.
			m_logger->StartLog(tc_physics, m_kxsystem->GetTimeInSeconds(), true);
			scene->GetPhysicsEnvironment()->proceed(deltatime);

			// Update scenegraph after physics step. This maps physics calculations
			// into node positions.		
			m_logger->StartLog(tc_scenegraph, m_kxsystem->GetTimeInSeconds(), true);
			scene->UpdateParents(curtime);

		} // suspended

		DoSound(scene);

		m_logger->StartLog(tc_services, m_kxsystem->GetTimeInSeconds(), true);
	}

	// update system devices
	m_logger->StartLog(tc_logic, m_kxsystem->GetTimeInSeconds(), true);

	if (m_keyboarddevice)
		m_keyboarddevice->NextFrame();

	if (m_mousedevice)
		m_mousedevice->NextFrame();

	if (m_networkdevice)
		m_networkdevice->NextFrame();

	if (m_audiodevice)
		m_audiodevice->NextFrame();

	// scene management
	ProcessScheduledScenes();

	// Start logging time spend outside main loop
	m_logger->StartLog(tc_outside, m_kxsystem->GetTimeInSeconds(), true);
}



void KX_KetsjiEngine::Render()
{
	KX_Scene* firstscene = *m_scenes.begin();
	const RAS_FrameSettings &framesettings = firstscene->GetFramingType();

	m_logger->StartLog(tc_rasterizer, m_kxsystem->GetTimeInSeconds(), true);

	// hiding mouse cursor each frame
	// (came back when going out of focus and then back in again)
	if (m_hideCursor)
		m_canvas->SetMouseState(RAS_ICanvas::MOUSE_INVISIBLE);

	// clear the entire game screen with the border color
	// only once per frame
	m_canvas->BeginDraw();
	if (m_drawingmode == RAS_IRasterizer::KX_TEXTURED) {
		m_canvas->SetViewPort(0, 0, m_canvas->GetWidth(), m_canvas->GetHeight());
		if (m_overrideFrameColor)
		{
			// Do not use the framing bar color set in the Blender scenes
			m_canvas->ClearColor(
				m_overrideFrameColorR,
				m_overrideFrameColorG,
				m_overrideFrameColorB,
				1.0
				);
		}
		else
		{
			// Use the framing bar color set in the Blender scenes
			m_canvas->ClearColor(
				framesettings.BarRed(),
				framesettings.BarGreen(),
				framesettings.BarBlue(),
				1.0
				);
		}
		// clear the -whole- viewport
		m_canvas->ClearBuffer(RAS_ICanvas::COLOR_BUFFER);
	}

	m_rasterizer->SetEye(RAS_IRasterizer::RAS_STEREO_LEFTEYE);

	// BeginFrame() sets the actual drawing area. You can use a part of the window
	if (!BeginFrame())
		return;

	KX_SceneList::iterator sceneit;
	for (sceneit = m_scenes.begin();sceneit != m_scenes.end(); sceneit++)
	// for each scene, call the proceed functions
	{
		KX_Scene* scene = *sceneit;

		// pass the scene's worldsettings to the rasterizer
		SetWorldSettings(scene->GetWorldInfo());
		
		if (scene->IsClearingZBuffer())
			m_rasterizer->ClearDepthBuffer();

		m_rendertools->SetAuxilaryClientInfo(scene);

		//Initialize scene viewport.
		SetupRenderFrame(scene);

		// do the rendering
		RenderFrame(scene);
	}

	// only one place that checks for stereo
	if(m_rasterizer->Stereo())
	{
		m_rasterizer->SetEye(RAS_IRasterizer::RAS_STEREO_RIGHTEYE);

		if (!BeginFrame())
			return;

		KX_SceneList::iterator sceneit;
		for (sceneit = m_scenes.begin();sceneit != m_scenes.end(); sceneit++)
		// for each scene, call the proceed functions
		{
			KX_Scene* scene = *sceneit;

			// pass the scene's worldsettings to the rasterizer
			SetWorldSettings(scene->GetWorldInfo());
		
			if (scene->IsClearingZBuffer())
				m_rasterizer->ClearDepthBuffer();

			//pass the scene, for picking and raycasting (shadows)
			m_rendertools->SetAuxilaryClientInfo(scene);

			//Initialize scene viewport.
			SetupRenderFrame(scene);

			// do the rendering
			RenderFrame(scene);
		}
	} // if(m_rasterizer->Stereo())

	EndFrame();
}



void KX_KetsjiEngine::RequestExit(int exitrequestmode)
{
	m_exitcode = exitrequestmode;
}



void KX_KetsjiEngine::SetNameNextGame(const STR_String& nextgame)
{
	m_exitstring = nextgame;
}



int KX_KetsjiEngine::GetExitCode()
{
	// if a gameactuator has set an exitcode or if there are no scenes left
	if (!m_exitcode)
	{
		if (m_scenes.begin()==m_scenes.end())
			m_exitcode = KX_EXIT_REQUEST_NO_SCENES_LEFT;
	}

	return m_exitcode;
}



const STR_String& KX_KetsjiEngine::GetExitString()
{
	return m_exitstring;
}



void KX_KetsjiEngine::DoSound(KX_Scene* scene)
{
	m_logger->StartLog(tc_sound, m_kxsystem->GetTimeInSeconds(), true);

	KX_Camera* cam = scene->GetActiveCamera();
	if (!cam)
		return;
	MT_Point3 listenerposition = cam->NodeGetWorldPosition();
	MT_Vector3 listenervelocity = cam->GetLinearVelocity();
	MT_Matrix3x3 listenerorientation = cam->NodeGetWorldOrientation();

	SND_Scene* soundscene = scene->GetSoundScene();
	soundscene->SetListenerTransform(
		listenerposition,
		listenervelocity,
		listenerorientation);

	soundscene->Proceed();
}



void KX_KetsjiEngine::SetBackGround(KX_WorldInfo* wi)
{
	if (wi->hasWorld())
	{
		if (m_drawingmode == RAS_IRasterizer::KX_TEXTURED)
		{	
			m_rasterizer->SetBackColor(
				wi->getBackColorRed(),
				wi->getBackColorGreen(),
				wi->getBackColorBlue(),
				0.0
			);
		}
	}
}



void KX_KetsjiEngine::SetWorldSettings(KX_WorldInfo* wi)
{
	if (wi->hasWorld())
	{
		if (m_drawingmode == RAS_IRasterizer::KX_TEXTURED)
		{	
			if (wi->hasMist())
			{
				m_rasterizer->SetFog(
					wi->getMistStart(),
					wi->getMistDistance(),
					wi->getMistColorRed(),
					wi->getMistColorGreen(),
					wi->getMistColorBlue()
				);
			}
			else
			{
				m_rasterizer->DisableFog();
			}
		}
	}
}



void KX_KetsjiEngine::SetDrawType(int drawingmode)
{
	m_drawingmode = drawingmode;
}


	
void KX_KetsjiEngine::EnableCameraOverride(const STR_String& forscene)
{
	m_overrideCam = true;
	m_overrideSceneName = forscene;
}



void KX_KetsjiEngine::SetCameraZoom(float camzoom)
{
	m_cameraZoom = camzoom;
}



void KX_KetsjiEngine::SetCameraOverrideUseOrtho(bool useOrtho)
{
	m_overrideCamUseOrtho = useOrtho;
}



void KX_KetsjiEngine::SetCameraOverrideProjectionMatrix(const MT_CmMatrix4x4& mat)
{
	m_overrideCamProjMat = mat;
}


void KX_KetsjiEngine::SetCameraOverrideViewMatrix(const MT_CmMatrix4x4& mat)
{
	m_overrideCamViewMat = mat;
}

	
void KX_KetsjiEngine::SetupRenderFrame(KX_Scene *scene)
{
	// In this function we make sure the rasterizer settings are upto
	// date. We compute the viewport so that logic
	// using this information is upto date.

	// Note we postpone computation of the projection matrix
	// so that we are using the latest camera position.

	RAS_Rect viewport;

	if (
		m_overrideCam || 
		(scene->GetName() != m_overrideSceneName) || 
		m_overrideCamUseOrtho
	) {
		RAS_FramingManager::ComputeViewport(
			scene->GetFramingType(),
			m_canvas->GetDisplayArea(),
			viewport
		);
	} else {
		viewport.SetLeft(0); 
		viewport.SetBottom(0);
		viewport.SetRight(int(m_canvas->GetWidth()));
		viewport.SetTop(int(m_canvas->GetHeight()));
	}
	// store the computed viewport in the scene

	scene->SetSceneViewport(viewport);	

	// set the viewport for this frame and scene
	m_canvas->SetViewPort(
		viewport.GetLeft(),
		viewport.GetBottom(),
		viewport.GetRight(),
		viewport.GetTop()
	);	

}		

	
// update graphics
void KX_KetsjiEngine::RenderFrame(KX_Scene* scene)
{
	float left, right, bottom, top, nearfrust, farfrust;
	KX_Camera* cam = scene->GetActiveCamera();
	
	if (!cam)
		return;

	m_rasterizer->DisplayFog();

	if (m_overrideCam && (scene->GetName() == m_overrideSceneName) && m_overrideCamUseOrtho) {
		MT_CmMatrix4x4 projmat = m_overrideCamProjMat;
		m_rasterizer->SetProjectionMatrix(projmat);
	} else {
		RAS_FrameFrustum frustum;

		RAS_FramingManager::ComputeFrustum(
			scene->GetFramingType(),
			m_canvas->GetDisplayArea(),
			scene->GetSceneViewport(),
			cam->GetLens(),
			cam->GetCameraNear(),
			cam->GetCameraFar(),
			frustum
		);

		left = frustum.x1 * m_cameraZoom;
		right = frustum.x2 * m_cameraZoom;
		bottom = frustum.y1 * m_cameraZoom;
		top = frustum.y2 * m_cameraZoom;
		nearfrust = frustum.camnear;
		farfrust = frustum.camfar;

		MT_Matrix4x4 projmat = m_rasterizer->GetFrustumMatrix(
			left, right, bottom, top, nearfrust, farfrust);
	
		m_rasterizer->SetProjectionMatrix(projmat);
		cam->SetProjectionMatrix(projmat);	
	}

	MT_Scalar cammat[16];
	cam->GetWorldToCamera().getValue(cammat);
	MT_Matrix4x4 viewmat;
	viewmat.setValue(cammat); // this _should transpose ... 
	                          // if finally transposed take care of correct usage
	                          // in RAS_OpenGLRasterizer ! (row major vs column major)

	m_rasterizer->SetViewMatrix(viewmat, cam->NodeGetWorldPosition(),
		cam->GetCameraLocation(), cam->GetCameraOrientation());
	cam->SetModelviewMatrix(viewmat);

	scene->UpdateMeshTransformations();

	// The following actually reschedules all vertices to be
	// redrawn. There is a cache between the actual rescheduling
	// and this call though. Visibility is imparted when this call
	// runs through the individual objects.
	scene->CalculateVisibleMeshes(m_rasterizer);

	scene->RenderBuckets(cam->GetWorldToCamera(), m_rasterizer, m_rendertools);
}



void KX_KetsjiEngine::StopEngine()
{
	if (m_bInitialized)
	{
		KX_SceneList::iterator sceneit;
		for (sceneit = m_scenes.begin();sceneit != m_scenes.end() ; sceneit++)
		{
			KX_Scene* scene = *sceneit;
			delete scene;
		}	
		m_scenes.clear();

		// cleanup all the stuff		
		m_rasterizer->Exit();
	}
}

// Scene Management is able to switch between scenes
// and have several scene's running in parallel
void KX_KetsjiEngine::AddScene(KX_Scene* scene)
{ 
	m_scenes.push_back(scene);
	PostProcessScene(scene);
	SceneListsChanged();
}



void KX_KetsjiEngine::PostProcessScene(KX_Scene* scene)
{
	bool override_camera = (m_overrideCam && (scene->GetName() == m_overrideSceneName));
	
		// if there is no activecamera, or the camera is being
		// overridden we need to construct a temporarily camera
	if (!scene->GetActiveCamera() || override_camera)
	{
		KX_Camera* activecam = NULL;

		RAS_CameraData camdata = RAS_CameraData();
		activecam = new KX_Camera(scene,KX_Scene::m_callbacks,camdata);
		activecam->SetName("__default__cam__");
	
			// set transformation
		if (override_camera) {
			const MT_CmMatrix4x4& cammatdata = m_overrideCamViewMat;
			MT_Transform trans = MT_Transform(cammatdata.getPointer());
			MT_Transform camtrans;
			camtrans.invert(trans);
			
			activecam->NodeSetLocalPosition(camtrans.getOrigin());
			activecam->NodeSetLocalOrientation(camtrans.getBasis());
			activecam->NodeUpdateGS(0,true);
		} else {
			activecam->NodeSetLocalPosition(MT_Point3(0.0, 0.0, 0.0));
			activecam->NodeSetLocalOrientation(MT_Vector3(0.0, 0.0, 0.0));
			activecam->NodeUpdateGS(0,true);
		}

		scene->AddCamera(activecam);
		scene->SetActiveCamera(activecam);
		scene->GetObjectList()->Add(activecam->AddRef());
		scene->GetRootParentList()->Add(activecam->AddRef());
	}
	
	scene->UpdateParents(0.0);
}



void KX_KetsjiEngine::RenderDebugProperties()
{
	STR_String debugtxt;
	int xcoord = 10;	// mmmm, these constants were taken from blender source
	int ycoord = 14;	// to 'mimic' behaviour

	float tottime = m_logger->GetAverage();
	if (tottime < 1e-6f) {
		tottime = 1e-6f;
	}
	
	/* Framerate display */
	if (m_show_framerate) {
		debugtxt.Format("swap : %.3f (%.3f frames per second)", tottime, 1.0/tottime);
		m_rendertools->RenderText2D(RAS_IRenderTools::RAS_TEXT_PADDED, 
									debugtxt.Ptr(),
									xcoord,
									ycoord, 
									m_canvas->GetWidth() /* RdV, TODO ?? */, 
									m_canvas->GetHeight() /* RdV, TODO ?? */);
		ycoord += 14;
	}

	/* Profile and framerate display */
	if (m_show_profile)
	{		
		for (int j = tc_first; j < tc_numCategories; j++)
		{
			debugtxt.Format(m_profileLabels[j]);
			m_rendertools->RenderText2D(RAS_IRenderTools::RAS_TEXT_PADDED, 
										debugtxt.Ptr(),
										xcoord,ycoord,
										m_canvas->GetWidth(), 
										m_canvas->GetHeight());
			double time = m_logger->GetAverage((KX_TimeCategory)j);
			debugtxt.Format("%2.2f %%", time/tottime * 100.f);
			m_rendertools->RenderText2D(RAS_IRenderTools::RAS_TEXT_PADDED, 
										debugtxt.Ptr(),
										xcoord + 60 ,ycoord,
										m_canvas->GetWidth(), 
										m_canvas->GetHeight());
			ycoord += 14;
		}
	}

	/* Property display*/
	if (m_show_debug_properties && m_propertiesPresent)
	{
		KX_SceneList::iterator sceneit;
		for (sceneit = m_scenes.begin();sceneit != m_scenes.end() ; sceneit++)
		{
			KX_Scene* scene = *sceneit;
			/* the 'normal' debug props */
			vector<SCA_DebugProp*>& debugproplist = scene->GetDebugProperties();
			
			for (vector<SCA_DebugProp*>::iterator it = debugproplist.begin();
				 !(it==debugproplist.end());it++)
			{
				CValue* propobj = (*it)->m_obj;
				STR_String objname = propobj->GetName();
				STR_String propname = (*it)->m_name;
				CValue* propval = propobj->GetProperty(propname);
				if (propval)
				{
					STR_String text = propval->GetText();
					debugtxt = objname + "." + propname + " = " + text;
					m_rendertools->RenderText2D(RAS_IRenderTools::RAS_TEXT_PADDED, 
												debugtxt.Ptr(),
												xcoord,
												ycoord,
												m_canvas->GetWidth(),
												m_canvas->GetHeight());
					ycoord += 14;
				}
			}
		}
	}
}


KX_SceneList* KX_KetsjiEngine::CurrentScenes()
{
	return &m_scenes;	
}



KX_Scene* KX_KetsjiEngine::FindScene(const STR_String& scenename)
{
	KX_SceneList::iterator sceneit = m_scenes.begin();

	// bit risky :) better to split the second clause 
	while ( (sceneit != m_scenes.end()) 
			&& ((*sceneit)->GetName() != scenename))
	{
		sceneit++;
	}

	return ((sceneit == m_scenes.end()) ? NULL : *sceneit);	
}



void KX_KetsjiEngine::ConvertAndAddScene(const STR_String& scenename,bool overlay)
{
	// only add scene when it doesn't exist!
	if (FindScene(scenename))
	{
		STR_String tmpname = scenename;
		printf("warning: scene %s already exists, not added!\n",tmpname.Ptr());
	}
	else
	{
		if (overlay)
		{
			m_addingOverlayScenes.insert(scenename);
		}
		else
		{
			m_addingBackgroundScenes.insert(scenename);
		}
	}
}




void KX_KetsjiEngine::RemoveScene(const STR_String& scenename)
{
	if (FindScene(scenename))
	{
		m_removingScenes.insert(scenename);
	}
	else
	{
//		STR_String tmpname = scenename;
		std::cout << "warning: scene " << scenename << " does not exist, not removed!" << std::endl;
	}
}



void KX_KetsjiEngine::RemoveScheduledScenes()
{
	if (m_removingScenes.size())
	{
		set<STR_String>::iterator scenenameit;
		for (scenenameit=m_removingScenes.begin();scenenameit != m_removingScenes.end();scenenameit++)
		{
			STR_String scenename = *scenenameit;

			KX_SceneList::iterator sceneit;
			for (sceneit = m_scenes.begin();sceneit != m_scenes.end() ; sceneit++)
			{
				KX_Scene* scene = *sceneit;
				if (scene->GetName()==scenename)
				{
					delete scene;
					m_scenes.erase(sceneit);
					break;
				}
			}	
		}
		m_removingScenes.clear();
	}
}



KX_Scene* KX_KetsjiEngine::CreateScene(const STR_String& scenename)
{

	KX_Scene* tmpscene = new KX_Scene(m_keyboarddevice,
									  m_mousedevice,
									  m_networkdevice,
									  m_audiodevice,
									  scenename);

	m_sceneconverter->ConvertScene(scenename,
							  tmpscene,
							  m_pythondictionary,
							  m_keyboarddevice,
							  m_rendertools,
							  m_canvas);

	return tmpscene;
}



void KX_KetsjiEngine::AddScheduledScenes()
{
	set<STR_String>::iterator scenenameit;

	if (m_addingOverlayScenes.size())
	{
		for (scenenameit = m_addingOverlayScenes.begin();
			scenenameit != m_addingOverlayScenes.end();
			scenenameit++)
		{
			STR_String scenename = *scenenameit;
			KX_Scene* tmpscene = CreateScene(scenename);
			m_scenes.push_back(tmpscene);
			PostProcessScene(tmpscene);
		}
		m_addingOverlayScenes.clear();
	}
	
	if (m_addingBackgroundScenes.size())
	{
		for (scenenameit = m_addingBackgroundScenes.begin();
			scenenameit != m_addingBackgroundScenes.end();
			scenenameit++)
		{
			STR_String scenename = *scenenameit;
			KX_Scene* tmpscene = CreateScene(scenename);
			m_scenes.insert(m_scenes.begin(),tmpscene);
			PostProcessScene(tmpscene);

		}
		m_addingBackgroundScenes.clear();
	}
}



void KX_KetsjiEngine::ReplaceScene(const STR_String& oldscene,const STR_String& newscene)
{
	m_replace_scenes.insert(std::make_pair(oldscene,newscene));
}

// replace scene is not the same as removing and adding because the
// scene must be in exact the same place (to maintain drawingorder)
// (nzc) - should that not be done with a scene-display list? It seems
// stupid to rely on the mem allocation order...
void KX_KetsjiEngine::ReplaceScheduledScenes()
{
	if (m_replace_scenes.size())
	{
		set<pair<STR_String,STR_String> >::iterator scenenameit;
		
		for (scenenameit = m_replace_scenes.begin();
			scenenameit != m_replace_scenes.end();
			scenenameit++)
		{
			STR_String oldscenename = (*scenenameit).first;
			STR_String newscenename = (*scenenameit).second;
			int i=0;
			/* Scenes are not supposed to be included twice... I think */
			KX_SceneList::iterator sceneit;
			for (sceneit = m_scenes.begin();sceneit != m_scenes.end() ; sceneit++)
			{
				KX_Scene* scene = *sceneit;
				if (scene->GetName() == oldscenename)
				{
					delete scene;
					KX_Scene* tmpscene = CreateScene(newscenename);
					m_scenes[i]=tmpscene;
					PostProcessScene(tmpscene);
				}
				i++;
			}
		}
		m_replace_scenes.clear();
	}	
}



void KX_KetsjiEngine::SuspendScene(const STR_String& scenename)
{
	KX_Scene*  scene = FindScene(scenename);
	if (scene) scene->Suspend();
}



void KX_KetsjiEngine::ResumeScene(const STR_String& scenename)
{
	KX_Scene*  scene = FindScene(scenename);
	if (scene) scene->Resume();
}



void KX_KetsjiEngine::SetUseFixedTime(bool bUseFixedTime)
{
	m_bFixedTime = bUseFixedTime;
}



bool KX_KetsjiEngine::GetUseFixedTime(void) const
{
	return m_bFixedTime;
}



void KX_KetsjiEngine::SetTimingDisplay(bool frameRate, bool profile, bool properties)
{
	m_show_framerate = frameRate;
	m_show_profile = profile;
	m_show_debug_properties = properties;
}



void KX_KetsjiEngine::GetTimingDisplay(bool& frameRate, bool& profile, bool& properties) const
{
	frameRate = m_show_framerate;
	profile = m_show_profile;
	properties = m_show_debug_properties;
}



void KX_KetsjiEngine::ProcessScheduledScenes(void)
{
	// Check whether there will be changes to the list of scenes
	if (m_addingOverlayScenes.size() ||
		m_addingBackgroundScenes.size() ||
		m_replace_scenes.size() ||
		m_removingScenes.size()) {

		// Change the scene list
		ReplaceScheduledScenes();
		RemoveScheduledScenes();
		AddScheduledScenes();

		// Notify
		SceneListsChanged();
	}
}



void KX_KetsjiEngine::SceneListsChanged(void)
{
	m_propertiesPresent = false;
	KX_SceneList::iterator sceneit = m_scenes.begin();
	while ((sceneit != m_scenes.end()) && (!m_propertiesPresent))
	{
		KX_Scene* scene = *sceneit;
		vector<SCA_DebugProp*>& debugproplist = scene->GetDebugProperties();	
		m_propertiesPresent = !debugproplist.empty();
		sceneit++;
	}
}


void KX_KetsjiEngine::SetHideCursor(bool hideCursor)
{
	m_hideCursor = hideCursor;
}


bool KX_KetsjiEngine::GetHideCursor(void) const
{
	return m_hideCursor;
}


void KX_KetsjiEngine::SetUseOverrideFrameColor(bool overrideFrameColor)
{
	m_overrideFrameColor = overrideFrameColor;
}


bool KX_KetsjiEngine::GetUseOverrideFrameColor(void) const
{
	return m_overrideFrameColor;
}


void KX_KetsjiEngine::SetOverrideFrameColor(float r, float g, float b)
{
	m_overrideFrameColorR = r;
	m_overrideFrameColorG = g;
	m_overrideFrameColorB = b;
}


void KX_KetsjiEngine::GetOverrideFrameColor(float& r, float& g, float& b) const
{
	r = m_overrideFrameColorR;
	g = m_overrideFrameColorG;
	b = m_overrideFrameColorB;
}

