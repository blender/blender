/*
 * $Id$
 *
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * The engine ties all game modules together. 
 */

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
#include "KX_Dome.h"
#include "KX_Light.h"
#include "KX_PythonInit.h"
#include "KX_PyConstraintBinding.h"
#include "PHY_IPhysicsEnvironment.h"

#include "AUD_C-API.h"

#include "NG_NetworkScene.h"
#include "NG_NetworkDeviceInterface.h"

#include "KX_WorldInfo.h"
#include "KX_ISceneConverter.h"
#include "KX_TimeCategoryLogger.h"

#include "RAS_FramingManager.h"
#include "stdio.h"
#include "DNA_world_types.h"
#include "DNA_scene_types.h"

// If define: little test for Nzc: guarded drawing. If the canvas is
// not valid, skip rendering this frame.
//#define NZC_GUARDED_OUTPUT
#define DEFAULT_LOGIC_TIC_RATE 60.0
#define DEFAULT_PHYSICS_TIC_RATE 60.0

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

double KX_KetsjiEngine::m_ticrate = DEFAULT_LOGIC_TIC_RATE;
int	   KX_KetsjiEngine::m_maxLogicFrame = 5;
int	   KX_KetsjiEngine::m_maxPhysicsFrame = 5;
double KX_KetsjiEngine::m_anim_framerate = 25.0;
double KX_KetsjiEngine::m_suspendedtime = 0.0;
double KX_KetsjiEngine::m_suspendeddelta = 0.0;
double KX_KetsjiEngine::m_average_framerate = 0.0;


/**
 *	Constructor of the Ketsji Engine
 */
KX_KetsjiEngine::KX_KetsjiEngine(KX_ISystem* system)
     :	m_canvas(NULL),
	m_rasterizer(NULL),
	m_kxsystem(system),
	m_rendertools(NULL),
	m_sceneconverter(NULL),
	m_networkdevice(NULL),
#ifndef DISABLE_PYTHON
	m_pythondictionary(NULL),
#endif
	m_keyboarddevice(NULL),
	m_mousedevice(NULL),

	m_propertiesPresent(false),

	m_bInitialized(false),
	m_activecam(0),
	m_bFixedTime(false),
	
	m_firstframe(true),
	
	m_frameTime(0.f),
	m_clockTime(0.f),
	m_previousClockTime(0.f),


	m_exitcode(KX_EXIT_REQUEST_NO_REQUEST),
	m_exitstring(""),
	
	m_drawingmode(5),
	m_cameraZoom(1.0),
	
	m_overrideCam(false),
	m_overrideCamUseOrtho(false),
	m_overrideCamNear(0.0),
	m_overrideCamFar(0.0),

	m_stereo(false),
	m_curreye(0),

	m_logger(NULL),
	
	// Set up timing info display variables
	m_show_framerate(false),
	m_show_profile(false),
	m_showProperties(false),
	m_showBackground(false),
	m_show_debug_properties(false),

	m_animation_record(false),

	// Default behavior is to hide the cursor every frame.
	m_hideCursor(false),

	m_overrideFrameColor(false),
	m_overrideFrameColorR(0.0),
	m_overrideFrameColorG(0.0),
	m_overrideFrameColorB(0.0),

	m_usedome(false)
{
	// Initialize the time logger
	m_logger = new KX_TimeCategoryLogger (25);

	for (int i = tc_first; i < tc_numCategories; i++)
		m_logger->AddCategory((KX_TimeCategory)i);
		
}



/**
 *	Destructor of the Ketsji Engine, release all memory
 */
KX_KetsjiEngine::~KX_KetsjiEngine()
{
	delete m_logger;
	if(m_usedome)
		delete m_dome;
}



void KX_KetsjiEngine::SetKeyboardDevice(SCA_IInputDevice* keyboarddevice)
{
	MT_assert(keyboarddevice);
	m_keyboarddevice = keyboarddevice;
}



void KX_KetsjiEngine::SetMouseDevice(SCA_IInputDevice* mousedevice)
{
	MT_assert(mousedevice);
	m_mousedevice = mousedevice;
}



void KX_KetsjiEngine::SetNetworkDevice(NG_NetworkDeviceInterface* networkdevice)
{
	MT_assert(networkdevice);
	m_networkdevice = networkdevice;
}


void KX_KetsjiEngine::SetCanvas(RAS_ICanvas* canvas)
{
	MT_assert(canvas);
	m_canvas = canvas;
}



void KX_KetsjiEngine::SetRenderTools(RAS_IRenderTools* rendertools)
{
	MT_assert(rendertools);
	m_rendertools = rendertools;
}



void KX_KetsjiEngine::SetRasterizer(RAS_IRasterizer* rasterizer)
{
	MT_assert(rasterizer);
	m_rasterizer = rasterizer;
}

#ifndef DISABLE_PYTHON
/*
 * At the moment the GameLogic module is imported into 'pythondictionary' after this function is called.
 * if this function ever changes to assign a copy, make sure the game logic module is imported into this dictionary before hand.
 */
void KX_KetsjiEngine::SetPyNamespace(PyObject* pythondictionary)
{
	MT_assert(pythondictionary);
	m_pythondictionary = pythondictionary;
}
#endif


void KX_KetsjiEngine::SetSceneConverter(KX_ISceneConverter* sceneconverter)
{
	MT_assert(sceneconverter);
	m_sceneconverter = sceneconverter;
}

void KX_KetsjiEngine::InitDome(short res, short mode, short angle, float resbuf, short tilt, struct Text* text)
{
	m_dome = new KX_Dome(m_canvas, m_rasterizer, m_rendertools,this, res, mode, angle, resbuf, tilt, text);
	m_usedome = true;
}

void KX_KetsjiEngine::RenderDome()
{
	GLuint	viewport[4]={0};
	glGetIntegerv(GL_VIEWPORT,(GLint *)viewport);
	
	m_dome->SetViewPort(viewport);

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

	// BeginFrame() sets the actual drawing area. You can use a part of the window
	if (!BeginFrame())
		return;

	KX_SceneList::iterator sceneit;
	KX_Scene* scene;

	int n_renders=m_dome->GetNumberRenders();// usually 4 or 6
	for (int i=0;i<n_renders;i++){
		m_canvas->ClearBuffer(RAS_ICanvas::COLOR_BUFFER|RAS_ICanvas::DEPTH_BUFFER);
		for (sceneit = m_scenes.begin();sceneit != m_scenes.end(); sceneit++)
		// for each scene, call the proceed functions
		{
			scene = *sceneit;
			KX_Camera* cam = scene->GetActiveCamera();

			m_rendertools->BeginFrame(m_rasterizer);
			// pass the scene's worldsettings to the rasterizer
			SetWorldSettings(scene->GetWorldInfo());

			// shadow buffers
			if (i == 0){
				RenderShadowBuffers(scene);
			}
			// Avoid drawing the scene with the active camera twice when it's viewport is enabled
			if(cam && !cam->GetViewport())
			{
				if (scene->IsClearingZBuffer())
					m_rasterizer->ClearDepthBuffer();
		
				m_rendertools->SetAuxilaryClientInfo(scene);
		
				// do the rendering
				m_dome->RenderDomeFrame(scene,cam, i);
			}
			
			list<class KX_Camera*>* cameras = scene->GetCameras();
			
			// Draw the scene once for each camera with an enabled viewport
			list<KX_Camera*>::iterator it = cameras->begin();
			while(it != cameras->end())
			{
				if((*it)->GetViewport())
				{
					if (scene->IsClearingZBuffer())
						m_rasterizer->ClearDepthBuffer();
			
					m_rendertools->SetAuxilaryClientInfo(scene);
			
					// do the rendering
					m_dome->RenderDomeFrame(scene, (*it),i);
				}
				
				it++;
			}
			// Part of PostRenderScene()
			m_rendertools->MotionBlur(m_rasterizer);
			scene->Render2DFilters(m_canvas);
			// no RunDrawingCallBacks
			// no FlushDebugLines
		}
		m_dome->BindImages(i);
	}	

	m_canvas->EndFrame();//XXX do we really need that?

	m_canvas->SetViewPort(0, 0, m_canvas->GetWidth(), m_canvas->GetHeight());

	if (m_overrideFrameColor) //XXX why do we want
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
	m_dome->Draw();
	// Draw Callback for the last scene
#ifndef DISABLE_PYTHON
	scene->RunDrawingCallbacks(scene->GetPostDrawCB());
#endif	
	EndFrame();
}

/**
 * Ketsji Init(), Initializes datastructures and converts data from
 * Blender into Ketsji native (realtime) format also sets up the
 * graphics context
 */
void KX_KetsjiEngine::StartEngine(bool clearIpo)
{
	m_clockTime = m_kxsystem->GetTimeInSeconds();
	m_frameTime = m_kxsystem->GetTimeInSeconds();
	m_previousClockTime = m_kxsystem->GetTimeInSeconds();

	m_firstframe = true;
	m_bInitialized = true;
	// there is always one scene enabled at startup
	Scene* scene = m_scenes[0]->GetBlenderScene();
	if (scene)
	{
		m_ticrate = scene->gm.ticrate ? scene->gm.ticrate : DEFAULT_LOGIC_TIC_RATE;
		m_maxLogicFrame = scene->gm.maxlogicstep ? scene->gm.maxlogicstep : 5;
		m_maxPhysicsFrame = scene->gm.maxphystep ? scene->gm.maxlogicstep : 5;
	}
	else
	{
		m_ticrate = DEFAULT_LOGIC_TIC_RATE;
		m_maxLogicFrame = 5;
		m_maxPhysicsFrame = 5;
	}
	
	if (m_animation_record)
	{
		m_sceneconverter->ResetPhysicsObjectsAnimationIpo(clearIpo);
		m_sceneconverter->WritePhysicsObjectToAnimationIpo(m_currentFrame);
	}

}

void KX_KetsjiEngine::ClearFrame()
{
	// clear unless we're drawing overlapping stereo
	if(m_rasterizer->InterlacedStereo() &&
		m_rasterizer->GetEye() == RAS_IRasterizer::RAS_STEREO_RIGHTEYE)
		return;

	// clear the viewports with the background color of the first scene
	bool doclear = false;
	KX_SceneList::iterator sceneit;
	RAS_Rect clearvp, area, viewport;

	for (sceneit = m_scenes.begin(); sceneit != m_scenes.end(); sceneit++)
	{
		KX_Scene* scene = *sceneit;
		//const RAS_FrameSettings &framesettings = scene->GetFramingType();
		list<class KX_Camera*>* cameras = scene->GetCameras();

		list<KX_Camera*>::iterator it;
		for(it = cameras->begin(); it != cameras->end(); it++)
		{
			GetSceneViewport(scene, (*it), area, viewport);

			if(!doclear) {
				clearvp = viewport;
				doclear = true;
			}
			else {
				if(viewport.GetLeft() < clearvp.GetLeft())
					clearvp.SetLeft(viewport.GetLeft());
				if(viewport.GetBottom() < clearvp.GetBottom())
					clearvp.SetBottom(viewport.GetBottom());
				if(viewport.GetRight() > clearvp.GetRight())
					clearvp.SetRight(viewport.GetRight());
				if(viewport.GetTop() > clearvp.GetTop())
					clearvp.SetTop(viewport.GetTop());

			}
		}
	}

	if(doclear) {
		KX_Scene* firstscene = *m_scenes.begin();
		SetBackGround(firstscene->GetWorldInfo());

		m_canvas->SetViewPort(clearvp.GetLeft(), clearvp.GetBottom(),
			clearvp.GetRight(), clearvp.GetTop());	
		m_rasterizer->ClearColorBuffer();
	}
}

bool KX_KetsjiEngine::BeginFrame()
{
	// set the area used for rendering (stereo can assign only a subset)
	m_rasterizer->SetRenderArea();

	if (m_canvas->BeginDraw())
	{
		ClearFrame();

		m_rasterizer->BeginFrame(m_drawingmode , m_kxsystem->GetTimeInSeconds());
		m_rendertools->BeginFrame(m_rasterizer);

		return true;
	}
	
	return false;
}		


void KX_KetsjiEngine::EndFrame()
{
	// Show profiling info
	m_logger->StartLog(tc_overhead, m_kxsystem->GetTimeInSeconds(), true);
	if (m_show_framerate || m_show_profile || (m_show_debug_properties && m_propertiesPresent))
	{
		RenderDebugProperties();
	}

	m_average_framerate = m_logger->GetAverage();
	if (m_average_framerate < 1e-6)
		m_average_framerate = 1e-6;
	m_average_framerate = 1.0/m_average_framerate;

	// Go to next profiling measurement, time spend after this call is shown in the next frame.
	m_logger->NextMeasurement(m_kxsystem->GetTimeInSeconds());

	m_logger->StartLog(tc_rasterizer, m_kxsystem->GetTimeInSeconds(), true);
	m_rasterizer->EndFrame();
	// swap backbuffer (drawing into this buffer) <-> front/visible buffer
	m_rasterizer->SwapBuffers();
	m_rendertools->EndFrame(m_rasterizer);

	
	m_canvas->EndDraw();
}

//#include "PIL_time.h"
//#include "LinearMath/btQuickprof.h"


bool KX_KetsjiEngine::NextFrame()
{
	double timestep = 1.0/m_ticrate;
	double framestep = timestep;
//	static hidden::Clock sClock;

m_logger->StartLog(tc_services, m_kxsystem->GetTimeInSeconds(),true);

//float dt = sClock.getTimeMicroseconds() * 0.000001f;
//sClock.reset();

if (m_bFixedTime)
	m_clockTime += timestep;
else
{

//	m_clockTime += dt;
	m_clockTime = m_kxsystem->GetTimeInSeconds();
}
	
	double deltatime = m_clockTime - m_frameTime;
	if (deltatime<0.f)
	{
		printf("problem with clock\n");
		deltatime = 0.f;
		m_clockTime = 0.f;
		m_frameTime = 0.f;
	}


	// Compute the number of logic frames to do each update (fixed tic bricks)
	int frames =int(deltatime*m_ticrate+1e-6);
//	if (frames>1)
//		printf("****************************************");
//	printf("dt = %f, deltatime = %f, frames = %d\n",dt, deltatime,frames);
	
//	if (!frames)
//		PIL_sleep_ms(1);
	
	KX_SceneList::iterator sceneit;
	
	if (frames>m_maxPhysicsFrame)
	{
	
	//	printf("framedOut: %d\n",frames);
		m_frameTime+=(frames-m_maxPhysicsFrame)*timestep;
		frames = m_maxPhysicsFrame;
	}
	

	bool doRender = frames>0;

	if (frames > m_maxLogicFrame)
	{
		framestep = (frames*timestep)/m_maxLogicFrame;
		frames = m_maxLogicFrame;
	}
		
	while (frames)
	{
	

		m_frameTime += framestep;
		
		for (sceneit = m_scenes.begin();sceneit != m_scenes.end(); ++sceneit)
		// for each scene, call the proceed functions
		{
			KX_Scene* scene = *sceneit;
	
			/* Suspension holds the physics and logic processing for an
			* entire scene. Objects can be suspended individually, and
			* the settings for that preceed the logic and physics
			* update. */
			m_logger->StartLog(tc_logic, m_kxsystem->GetTimeInSeconds(), true);

			m_sceneconverter->resetNoneDynamicObjectToIpo();//this is for none dynamic objects with ipo

			scene->UpdateObjectActivity();
	
			if (!scene->IsSuspended())
			{
				// if the scene was suspended recalcutlate the delta tu "curtime"
				m_suspendedtime = scene->getSuspendedTime();
				if (scene->getSuspendedTime()!=0.0)
					scene->setSuspendedDelta(scene->getSuspendedDelta()+m_clockTime-scene->getSuspendedTime());
				m_suspendeddelta = scene->getSuspendedDelta();

				
				m_logger->StartLog(tc_network, m_kxsystem->GetTimeInSeconds(), true);
				SG_SetActiveStage(SG_STAGE_NETWORK);
				scene->GetNetworkScene()->proceed(m_frameTime);
	
				//m_logger->StartLog(tc_scenegraph, m_kxsystem->GetTimeInSeconds(), true);
				//SG_SetActiveStage(SG_STAGE_NETWORK_UPDATE);
				//scene->UpdateParents(m_frameTime);
				
				m_logger->StartLog(tc_physics, m_kxsystem->GetTimeInSeconds(), true);
				SG_SetActiveStage(SG_STAGE_PHYSICS1);
				// set Python hooks for each scene
#ifndef DISABLE_PYTHON
				PHY_SetActiveEnvironment(scene->GetPhysicsEnvironment());
#endif
				KX_SetActiveScene(scene);
	
				scene->GetPhysicsEnvironment()->endFrame();
				
				// Update scenegraph after physics step. This maps physics calculations
				// into node positions.		
				//m_logger->StartLog(tc_scenegraph, m_kxsystem->GetTimeInSeconds(), true);
				//SG_SetActiveStage(SG_STAGE_PHYSICS1_UPDATE);
				//scene->UpdateParents(m_frameTime);
				
				// Process sensors, and controllers
				m_logger->StartLog(tc_logic, m_kxsystem->GetTimeInSeconds(), true);
				SG_SetActiveStage(SG_STAGE_CONTROLLER);
				scene->LogicBeginFrame(m_frameTime);
	
				// Scenegraph needs to be updated again, because Logic Controllers 
				// can affect the local matrices.
				m_logger->StartLog(tc_scenegraph, m_kxsystem->GetTimeInSeconds(), true);
				SG_SetActiveStage(SG_STAGE_CONTROLLER_UPDATE);
				scene->UpdateParents(m_frameTime);
	
				// Process actuators
	
				// Do some cleanup work for this logic frame
				m_logger->StartLog(tc_logic, m_kxsystem->GetTimeInSeconds(), true);
				SG_SetActiveStage(SG_STAGE_ACTUATOR);
				scene->LogicUpdateFrame(m_frameTime, true);
				
				scene->LogicEndFrame();
	
				// Actuators can affect the scenegraph
				m_logger->StartLog(tc_scenegraph, m_kxsystem->GetTimeInSeconds(), true);
				SG_SetActiveStage(SG_STAGE_ACTUATOR_UPDATE);
				scene->UpdateParents(m_frameTime);
				
				m_logger->StartLog(tc_physics, m_kxsystem->GetTimeInSeconds(), true);
				SG_SetActiveStage(SG_STAGE_PHYSICS2);
				scene->GetPhysicsEnvironment()->beginFrame();
		
				// Perform physics calculations on the scene. This can involve 
				// many iterations of the physics solver.
				scene->GetPhysicsEnvironment()->proceedDeltaTime(m_frameTime,timestep,framestep);//m_deltatimerealDeltaTime);

				m_logger->StartLog(tc_scenegraph, m_kxsystem->GetTimeInSeconds(), true);
				SG_SetActiveStage(SG_STAGE_PHYSICS2_UPDATE);
				scene->UpdateParents(m_frameTime);
			
			
				if (m_animation_record)
				{					
					m_sceneconverter->WritePhysicsObjectToAnimationIpo(++m_currentFrame);
				}

				scene->setSuspendedTime(0.0);
			} // suspended
			else
				if(scene->getSuspendedTime()==0.0)
					scene->setSuspendedTime(m_clockTime);
	
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

		// scene management
		ProcessScheduledScenes();
		
		frames--;
	}

	bool bUseAsyncLogicBricks= false;//true;

	if (bUseAsyncLogicBricks)
	{	
		// Logic update sub frame: this will let some logic bricks run at the
		// full frame rate.
		for (sceneit = m_scenes.begin();sceneit != m_scenes.end(); ++sceneit)
		// for each scene, call the proceed functions
		{
			KX_Scene* scene = *sceneit;

			if (!scene->IsSuspended())
			{
				// if the scene was suspended recalcutlate the delta tu "curtime"
				m_suspendedtime = scene->getSuspendedTime();
				if (scene->getSuspendedTime()!=0.0)
					scene->setSuspendedDelta(scene->getSuspendedDelta()+m_clockTime-scene->getSuspendedTime());
				m_suspendeddelta = scene->getSuspendedDelta();
				
				// set Python hooks for each scene
#ifndef DISABLE_PYTHON
				PHY_SetActiveEnvironment(scene->GetPhysicsEnvironment());
#endif
				KX_SetActiveScene(scene);
				
				m_logger->StartLog(tc_scenegraph, m_kxsystem->GetTimeInSeconds(), true);
				SG_SetActiveStage(SG_STAGE_PHYSICS1);
				scene->UpdateParents(m_clockTime);

				// Perform physics calculations on the scene. This can involve 
				// many iterations of the physics solver.
				m_logger->StartLog(tc_physics, m_kxsystem->GetTimeInSeconds(), true);
				scene->GetPhysicsEnvironment()->proceedDeltaTime(m_clockTime,timestep,timestep);
				// Update scenegraph after physics step. This maps physics calculations
				// into node positions.		
				m_logger->StartLog(tc_scenegraph, m_kxsystem->GetTimeInSeconds(), true);
				SG_SetActiveStage(SG_STAGE_PHYSICS2);
				scene->UpdateParents(m_clockTime);
				
				// Do some cleanup work for this logic frame
				m_logger->StartLog(tc_logic, m_kxsystem->GetTimeInSeconds(), true);
				scene->LogicUpdateFrame(m_clockTime, false);

				// Actuators can affect the scenegraph
				m_logger->StartLog(tc_scenegraph, m_kxsystem->GetTimeInSeconds(), true);
				SG_SetActiveStage(SG_STAGE_ACTUATOR);
				scene->UpdateParents(m_clockTime);
				 
 				scene->setSuspendedTime(0.0);
			} // suspended
 			else
 				if(scene->getSuspendedTime()==0.0)
 					scene->setSuspendedTime(m_clockTime);

			DoSound(scene);

			m_logger->StartLog(tc_services, m_kxsystem->GetTimeInSeconds(), true);
		}
	}


	m_previousClockTime = m_clockTime;
	
	// Start logging time spend outside main loop
	m_logger->StartLog(tc_outside, m_kxsystem->GetTimeInSeconds(), true);
	
	return doRender;
}



void KX_KetsjiEngine::Render()
{
	if(m_usedome){
		RenderDome();
		return;
	}
	KX_Scene* firstscene = *m_scenes.begin();
	const RAS_FrameSettings &framesettings = firstscene->GetFramingType();

	m_logger->StartLog(tc_rasterizer, m_kxsystem->GetTimeInSeconds(), true);
	SG_SetActiveStage(SG_STAGE_RENDER);

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
		m_canvas->ClearBuffer(RAS_ICanvas::COLOR_BUFFER|RAS_ICanvas::DEPTH_BUFFER);
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
		KX_Camera* cam = scene->GetActiveCamera();
		// pass the scene's worldsettings to the rasterizer
		SetWorldSettings(scene->GetWorldInfo());

		// this is now done incrementatlly in KX_Scene::CalculateVisibleMeshes
		//scene->UpdateMeshTransformations();

		// shadow buffers
		RenderShadowBuffers(scene);

		// Avoid drawing the scene with the active camera twice when it's viewport is enabled
		if(cam && !cam->GetViewport())
		{
			if (scene->IsClearingZBuffer())
				m_rasterizer->ClearDepthBuffer();
	
			m_rendertools->SetAuxilaryClientInfo(scene);
	
			// do the rendering
			RenderFrame(scene, cam);
		}
		
		list<class KX_Camera*>* cameras = scene->GetCameras();
		
		// Draw the scene once for each camera with an enabled viewport
		list<KX_Camera*>::iterator it = cameras->begin();
		while(it != cameras->end())
		{
			if((*it)->GetViewport())
			{
				if (scene->IsClearingZBuffer())
					m_rasterizer->ClearDepthBuffer();
		
				m_rendertools->SetAuxilaryClientInfo(scene);
		
				// do the rendering
				RenderFrame(scene, (*it));
			}
			
			it++;
		}
		PostRenderScene(scene);
	}

	// only one place that checks for stereo
	if(m_rasterizer->Stereo())
	{
		m_rasterizer->SetEye(RAS_IRasterizer::RAS_STEREO_RIGHTEYE);

		if (!BeginFrame())
			return;


		for (sceneit = m_scenes.begin();sceneit != m_scenes.end(); sceneit++)
		// for each scene, call the proceed functions
		{
			KX_Scene* scene = *sceneit;
			KX_Camera* cam = scene->GetActiveCamera();

			// pass the scene's worldsettings to the rasterizer
			SetWorldSettings(scene->GetWorldInfo());
		
			if (scene->IsClearingZBuffer())
				m_rasterizer->ClearDepthBuffer();

			//pass the scene, for picking and raycasting (shadows)
			m_rendertools->SetAuxilaryClientInfo(scene);

			// do the rendering
			//RenderFrame(scene);
			RenderFrame(scene, cam);

			list<class KX_Camera*>* cameras = scene->GetCameras();			
	
			// Draw the scene once for each camera with an enabled viewport
			list<KX_Camera*>::iterator it = cameras->begin();
			while(it != cameras->end())
			{
				if((*it)->GetViewport())
				{
					if (scene->IsClearingZBuffer())
						m_rasterizer->ClearDepthBuffer();
			
					m_rendertools->SetAuxilaryClientInfo(scene);
			
					// do the rendering
					RenderFrame(scene, (*it));
				}
				
				it++;
			}
			PostRenderScene(scene);
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

	{
		AUD_3DData data;
		float f;

		listenerorientation.getValue3x3(data.orientation);
		listenerposition.getValue(data.position);
		listenervelocity.getValue(data.velocity);

		f = data.position[1];
		data.position[1] = data.position[2];
		data.position[2] = -f;

		f = data.velocity[1];
		data.velocity[1] = data.velocity[2];
		data.velocity[2] = -f;

		f = data.orientation[1];
		data.orientation[1] = data.orientation[2];
		data.orientation[2] = -f;

		f = data.orientation[3];
		data.orientation[3] = -data.orientation[6];
		data.orientation[6] = f;

		f = data.orientation[4];
		data.orientation[4] = -data.orientation[8];
		data.orientation[8] = -f;

		f = data.orientation[5];
		data.orientation[5] = data.orientation[7];
		data.orientation[7] = f;

		AUD_updateListener(&data);
	}
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
		// ...
		m_rasterizer->SetAmbientColor(
			wi->getAmbientColorRed(),
			wi->getAmbientColorGreen(),
			wi->getAmbientColorBlue()
		);

		if (m_drawingmode >= RAS_IRasterizer::KX_SOLID)
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

void KX_KetsjiEngine::SetCameraOverrideClipping(float near, float far)
{
	m_overrideCamNear = near;
	m_overrideCamFar = far;
}

void KX_KetsjiEngine::SetCameraOverrideLens(float lens)
{
	m_overrideCamLens = lens;
}

void KX_KetsjiEngine::GetSceneViewport(KX_Scene *scene, KX_Camera* cam, RAS_Rect& area, RAS_Rect& viewport)
{
	// In this function we make sure the rasterizer settings are upto
	// date. We compute the viewport so that logic
	// using this information is upto date.

	// Note we postpone computation of the projection matrix
	// so that we are using the latest camera position.
	if (cam->GetViewport()) {
		RAS_Rect userviewport;

		userviewport.SetLeft(cam->GetViewportLeft()); 
		userviewport.SetBottom(cam->GetViewportBottom());
		userviewport.SetRight(cam->GetViewportRight());
		userviewport.SetTop(cam->GetViewportTop());

		// Don't do bars on user specified viewport
		RAS_FrameSettings settings = scene->GetFramingType();
		if(settings.FrameType() == RAS_FrameSettings::e_frame_bars)
			settings.SetFrameType(RAS_FrameSettings::e_frame_extend);

		RAS_FramingManager::ComputeViewport(
			scene->GetFramingType(),
			userviewport,
			viewport
		);

		area = userviewport;
	}
	else if ( !m_overrideCam || (scene->GetName() != m_overrideSceneName) ||  m_overrideCamUseOrtho ) {
		RAS_FramingManager::ComputeViewport(
			scene->GetFramingType(),
			m_canvas->GetDisplayArea(),
			viewport
		);

		area = m_canvas->GetDisplayArea();
	} else {
		viewport.SetLeft(0); 
		viewport.SetBottom(0);
		viewport.SetRight(int(m_canvas->GetWidth()));
		viewport.SetTop(int(m_canvas->GetHeight()));

		area = m_canvas->GetDisplayArea();
	}
}

void KX_KetsjiEngine::RenderShadowBuffers(KX_Scene *scene)
{
	CListValue *lightlist = scene->GetLightList();
	int i, drawmode;

	m_rendertools->SetAuxilaryClientInfo(scene);

	for(i=0; i<lightlist->GetCount(); i++) {
		KX_GameObject *gameobj = (KX_GameObject*)lightlist->GetValue(i);

		KX_LightObject *light = (KX_LightObject*)gameobj;

		light->Update();

		if(m_drawingmode == RAS_IRasterizer::KX_TEXTURED && light->HasShadowBuffer()) {
			/* make temporary camera */
			RAS_CameraData camdata = RAS_CameraData();
			KX_Camera *cam = new KX_Camera(scene, scene->m_callbacks, camdata, true, true);
			cam->SetName("__shadow__cam__");

			MT_Transform camtrans;

			/* switch drawmode for speed */
			drawmode = m_rasterizer->GetDrawingMode();
			m_rasterizer->SetDrawingMode(RAS_IRasterizer::KX_SHADOW);

			/* binds framebuffer object, sets up camera .. */
			light->BindShadowBuffer(m_rasterizer, cam, camtrans);

			/* update scene */
			scene->CalculateVisibleMeshes(m_rasterizer, cam, light->GetShadowLayer());

			/* render */
			m_rasterizer->ClearDepthBuffer();
			scene->RenderBuckets(camtrans, m_rasterizer, m_rendertools);

			/* unbind framebuffer object, restore drawmode, free camera */
			light->UnbindShadowBuffer(m_rasterizer);
			m_rasterizer->SetDrawingMode(drawmode);
			cam->Release();
		}
	}
}
	
// update graphics
void KX_KetsjiEngine::RenderFrame(KX_Scene* scene, KX_Camera* cam)
{
	bool override_camera;
	RAS_Rect viewport, area;
	float nearfrust, farfrust, focallength;
//	KX_Camera* cam = scene->GetActiveCamera();
	
	if (!cam)
		return;
	GetSceneViewport(scene, cam, area, viewport);

	// store the computed viewport in the scene
	scene->SetSceneViewport(viewport);	

	// set the viewport for this frame and scene
	m_canvas->SetViewPort(viewport.GetLeft(), viewport.GetBottom(),
		viewport.GetRight(), viewport.GetTop());	
	
	// see KX_BlenderMaterial::Activate
	//m_rasterizer->SetAmbient();
	m_rasterizer->DisplayFog();

	override_camera = m_overrideCam && (scene->GetName() == m_overrideSceneName);
	override_camera = override_camera && (cam->GetName() == "__default__cam__");

	if (override_camera && m_overrideCamUseOrtho) {
		m_rasterizer->SetProjectionMatrix(m_overrideCamProjMat);
		if (!cam->hasValidProjectionMatrix()) {
			// needed to get frustrum planes for culling
			MT_Matrix4x4 projmat;
			projmat.setValue(m_overrideCamProjMat.getPointer());
			cam->SetProjectionMatrix(projmat);
		}
	} else if (cam->hasValidProjectionMatrix() && !cam->GetViewport() )
	{
		m_rasterizer->SetProjectionMatrix(cam->GetProjectionMatrix());
	} else
	{
		RAS_FrameFrustum frustum;
		bool orthographic = !cam->GetCameraData()->m_perspective;
		nearfrust = cam->GetCameraNear();
		farfrust = cam->GetCameraFar();
		focallength = cam->GetFocalLength();
		MT_Matrix4x4 projmat;

		if(override_camera) {
			nearfrust = m_overrideCamNear;
			farfrust = m_overrideCamFar;
		}

		if (orthographic) {

			RAS_FramingManager::ComputeOrtho(
				scene->GetFramingType(),
				area,
				viewport,
				cam->GetScale(),
				nearfrust,
				farfrust,
				frustum
			);
			if (!cam->GetViewport()) {
				frustum.x1 *= m_cameraZoom;
				frustum.x2 *= m_cameraZoom;
				frustum.y1 *= m_cameraZoom;
				frustum.y2 *= m_cameraZoom;
			}
			projmat = m_rasterizer->GetOrthoMatrix(
				frustum.x1, frustum.x2, frustum.y1, frustum.y2, frustum.camnear, frustum.camfar);

		} else {
			RAS_FramingManager::ComputeFrustum(
				scene->GetFramingType(),
				area,
				viewport,
				cam->GetLens(),
				nearfrust,
				farfrust,
				frustum
			);

			if (!cam->GetViewport()) {
				frustum.x1 *= m_cameraZoom;
				frustum.x2 *= m_cameraZoom;
				frustum.y1 *= m_cameraZoom;
				frustum.y2 *= m_cameraZoom;
			}
			projmat = m_rasterizer->GetFrustumMatrix(
				frustum.x1, frustum.x2, frustum.y1, frustum.y2, frustum.camnear, frustum.camfar, focallength);
		}
		cam->SetProjectionMatrix(projmat);
		
		// Otherwise the projection matrix for each eye will be the same...
		if (!orthographic && m_rasterizer->Stereo())
			cam->InvalidateProjectionMatrix();
	}

	MT_Transform camtrans(cam->GetWorldToCamera());
	MT_Matrix4x4 viewmat(camtrans);
	
	m_rasterizer->SetViewMatrix(viewmat, cam->NodeGetWorldOrientation(), cam->NodeGetWorldPosition(), cam->GetCameraData()->m_perspective);
	cam->SetModelviewMatrix(viewmat);

	// The following actually reschedules all vertices to be
	// redrawn. There is a cache between the actual rescheduling
	// and this call though. Visibility is imparted when this call
	// runs through the individual objects.

	m_logger->StartLog(tc_scenegraph, m_kxsystem->GetTimeInSeconds(), true);
	SG_SetActiveStage(SG_STAGE_CULLING);

	scene->CalculateVisibleMeshes(m_rasterizer,cam);

	m_logger->StartLog(tc_rasterizer, m_kxsystem->GetTimeInSeconds(), true);
	SG_SetActiveStage(SG_STAGE_RENDER);

#ifndef DISABLE_PYTHON
	// Run any pre-drawing python callbacks
	scene->RunDrawingCallbacks(scene->GetPreDrawCB());
#endif

	scene->RenderBuckets(camtrans, m_rasterizer, m_rendertools);
	
	if (scene->GetPhysicsEnvironment())
		scene->GetPhysicsEnvironment()->debugDrawWorld();
}
/*
To run once per scene
*/
void KX_KetsjiEngine::PostRenderScene(KX_Scene* scene)
{
	m_rendertools->MotionBlur(m_rasterizer);
	scene->Render2DFilters(m_canvas);
#ifndef DISABLE_PYTHON
	scene->RunDrawingCallbacks(scene->GetPostDrawCB());	
#endif
	m_rasterizer->FlushDebugLines();
}

void KX_KetsjiEngine::StopEngine()
{
	if (m_bInitialized)
	{

		if (m_animation_record)
		{
//			printf("TestHandlesPhysicsObjectToAnimationIpo\n");
			m_sceneconverter->TestHandlesPhysicsObjectToAnimationIpo();
		}

		KX_SceneList::iterator sceneit;
		for (sceneit = m_scenes.begin();sceneit != m_scenes.end() ; sceneit++)
		{
			KX_Scene* scene = *sceneit;
			m_sceneconverter->RemoveScene(scene);
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

	SG_SetActiveStage(SG_STAGE_SCENE);

	// if there is no activecamera, or the camera is being
	// overridden we need to construct a temporarily camera
	if (!scene->GetActiveCamera() || override_camera)
	{
		KX_Camera* activecam = NULL;

		RAS_CameraData camdata = RAS_CameraData();
		if (override_camera) camdata.m_lens = m_overrideCamLens;

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
			activecam->NodeUpdateGS(0);
		} else {
			activecam->NodeSetLocalPosition(MT_Point3(0.0, 0.0, 0.0));
			activecam->NodeSetLocalOrientation(MT_Vector3(0.0, 0.0, 0.0));
			activecam->NodeUpdateGS(0);
		}

		scene->AddCamera(activecam);
		scene->SetActiveCamera(activecam);
		scene->GetObjectList()->Add(activecam->AddRef());
		scene->GetRootParentList()->Add(activecam->AddRef());
		//done with activecam
		activecam->Release();
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

	// Set viewport to entire canvas
	RAS_Rect viewport;
	m_canvas->SetViewPort(0, 0, int(m_canvas->GetWidth()), int(m_canvas->GetHeight()));
	
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
			debugtxt.Format("%.3fms (%2.2f %%)", time*1000.f, time/tottime * 100.f);
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
				if (propname == "__state__")
				{
					// reserve name for object state
					KX_GameObject* gameobj = static_cast<KX_GameObject*>(propobj);
					unsigned int state = gameobj->GetState();
					debugtxt = objname + "." + propname + " = ";
					bool first = true;
					for (int statenum=1;state;state >>= 1, statenum++)
					{
						if (state & 1)
						{
							if (!first)
							{
								debugtxt += ",";
							}
							debugtxt += STR_String(statenum);
							first = false;
						}
					}
					m_rendertools->RenderText2D(RAS_IRenderTools::RAS_TEXT_PADDED, 
													debugtxt.Ptr(),
													xcoord,
													ycoord,
													m_canvas->GetWidth(),
													m_canvas->GetHeight());
					ycoord += 14;
				}
				else
				{
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
					m_sceneconverter->RemoveScene(scene);
					m_scenes.erase(sceneit);
					break;
				}
			}	
		}
		m_removingScenes.clear();
	}
}

KX_Scene* KX_KetsjiEngine::CreateScene(Scene *scene)
{
	KX_Scene* tmpscene = new KX_Scene(m_keyboarddevice,
									  m_mousedevice,
									  m_networkdevice,
									  scene->id.name+2,
									  scene,
									  m_canvas);

	m_sceneconverter->ConvertScene(tmpscene,
							  m_rendertools,
							  m_canvas);

	return tmpscene;
}

KX_Scene* KX_KetsjiEngine::CreateScene(const STR_String& scenename)
{
	Scene *scene = m_sceneconverter->GetBlenderSceneForName(scenename);
	return CreateScene(scene);
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
					m_sceneconverter->RemoveScene(scene);
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


void	KX_KetsjiEngine::SetAnimRecordMode(bool animation_record, int startFrame)
{
	m_animation_record = animation_record;
	if (animation_record)
	{
		//when recording physics keyframes, always run at a fixed framerate
		m_bFixedTime = true;
	}
	m_currentFrame = startFrame;
}

bool KX_KetsjiEngine::GetUseFixedTime(void) const
{
	return m_bFixedTime;
}

double KX_KetsjiEngine::GetSuspendedDelta()
{
	return m_suspendeddelta;
}

double KX_KetsjiEngine::GetTicRate()
{
	return m_ticrate;
}

void KX_KetsjiEngine::SetTicRate(double ticrate)
{
	m_ticrate = ticrate;
}

int KX_KetsjiEngine::GetMaxLogicFrame()
{
	return m_maxLogicFrame;
}

void KX_KetsjiEngine::SetMaxLogicFrame(int frame)
{
	m_maxLogicFrame = frame;
}

int KX_KetsjiEngine::GetMaxPhysicsFrame()
{
	return m_maxPhysicsFrame;
}

void KX_KetsjiEngine::SetMaxPhysicsFrame(int frame)
{
	m_maxPhysicsFrame = frame;
}

double KX_KetsjiEngine::GetAnimFrameRate()
{
	return m_anim_framerate;
}

double KX_KetsjiEngine::GetClockTime(void) const
{
	return m_clockTime;
}

double KX_KetsjiEngine::GetFrameTime(void) const
{
	return m_frameTime;
}

double KX_KetsjiEngine::GetRealTime(void) const
{
	return m_kxsystem->GetTimeInSeconds();
}

void KX_KetsjiEngine::SetAnimFrameRate(double framerate)
{
	m_anim_framerate = framerate;
}

double KX_KetsjiEngine::GetAverageFrameRate()
{
	return m_average_framerate;
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


