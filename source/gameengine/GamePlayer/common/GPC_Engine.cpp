/**
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
	#pragma warning (disable:4786) // suppress stl-MSVC debug info warning
#endif // WIN32

#include <iostream>

#include "BKE_blender.h"  // initglobals()
#include "BKE_global.h"  // Global G
#include "BKE_report.h"
#include "DNA_scene_types.h"
#include "DNA_camera_types.h"  // Camera
#include "DNA_object_types.h"  // Object

#include "BLO_readfile.h"
#include "BLI_blenlib.h"

// include files needed by "KX_BlenderSceneConverter.h"

#include "GEN_Map.h"
#include "SCA_IActuator.h"
#include "RAS_MeshObject.h"

#include "KX_BlenderSceneConverter.h"
#include "KX_KetsjiEngine.h"
#include "NG_LoopBackNetworkDeviceInterface.h"

#include "RAS_IRenderTools.h"

#include "GPC_Engine.h"
#include "GPC_KeyboardDevice.h"
#include "GPC_MouseDevice.h"
#include "GPC_RawImage.h"
#include "GPC_RawLoadDotBlendArray.h"



GPC_Engine::GPC_Engine(char *customLoadingAnimationURL,
		int foregroundColor, int backgroundColor, int frameRate) :
		m_initialized(false), m_running(false), m_loading(false),
		m_customLoadingAnimation(false), m_previousProgress(0.0),
		m_system(NULL), m_keyboarddev(NULL),
		m_mousedev(NULL), m_canvas(NULL), m_rendertools(NULL),
		m_portal(NULL), m_sceneconverter(NULL), m_networkdev(NULL),
		m_curarea(NULL), m_customLoadingAnimationURL(NULL),
		m_foregroundColor(foregroundColor), m_backgroundColor(backgroundColor),
		m_frameRate(frameRate),
		m_BlenderLogo(0), m_Blender3DLogo(0)/*, m_NaNLogo(0)*/
{
	if(customLoadingAnimationURL[0] != '\0')
	{
		m_customLoadingAnimationURL = new char[sizeof(customLoadingAnimationURL)];
// not yet, need to be implemented first...		m_customLoadingAnimation = true;
	}

	// load the Blender logo into memory
	m_BlenderLogo = new GPC_RawImage();
	// blender3d size is 115 x 32 so make resulting texture 128 x 128
	if(!m_BlenderLogo->Load("BlenderLogo", 128, 128, GPC_RawImage::alignTopLeft, 8, 8))
		m_BlenderLogo = 0;

	// load the Blender3D logo into memory
	m_Blender3DLogo = new GPC_RawImage();
	// blender3d size is 136 x 11 so make resulting texture 256 x 256
	if(!m_Blender3DLogo->Load("Blender3DLogo", 256, 256, GPC_RawImage::alignBottomRight, 8, 8))
		m_Blender3DLogo = 0;

#if 0
	// obsolete logo
	// load the NaN logo into memory
	m_NaNLogo = new GPC_RawImage();
	// blender3d size is 32 x 31 so make resulting texture 64 x 64
	if(!m_NaNLogo->Load("NaNLogo", 64, 64, GPC_RawImage::alignBottomRight, 8, 8))
		m_NaNLogo = 0;
#endif
}


GPC_Engine::~GPC_Engine()
{
	// deleting everything in reverse order of creation
#if 0
// hmm deleted in Stop()	delete m_portal;
// hmm deleted in Stop()	delete m_sceneconverter;
	delete m_system;
	delete m_networkdev;
	delete m_rendertools;
	delete m_canvas;
	delete m_mousedev;
	delete m_keyboarddev;
// not yet used so be careful and not delete them
//	delete m_WaveCache;
//	delete m_curarea;  // for future use, not used yet
#endif
	delete m_BlenderLogo;
	delete m_Blender3DLogo;
#if 0
	delete m_NaNLogo;
#endif
}


bool GPC_Engine::Start(char *filename)
{
	ReportList reports;
	BlendFileData *bfd;
	
	BKE_reports_init(&reports, RPT_STORE);
	bfd= BLO_read_from_file(filename, &reports);
	BKE_reports_clear(&reports);

	if (!bfd) {
			// XXX, deal with error here
		cout << "Unable to load: " << filename << endl;
		return false;
	}

	StartKetsji();

	if(bfd->type == BLENFILETYPE_PUB)
		m_canvas->SetBannerDisplayEnabled(false);

	return true;
}


bool GPC_Engine::Start(unsigned char *blenderDataBuffer,
		unsigned int blenderDataBufferSize)
{
	ReportList reports;
	BlendFileData *bfd;
	
	BKE_reports_init(&reports, RPT_STORE);
	bfd= BLO_read_from_memory(blenderDataBuffer, blenderDataBufferSize, &reports);
	BKE_reports_clear(&reports);

	if (!bfd) {
			// XXX, deal with error here
		cout << "Unable to load. " << endl;
		return false;
	}
	
	StartKetsji();

	if(bfd->type == BLENFILETYPE_PUB)
		m_canvas->SetBannerDisplayEnabled(false);

	return true;
}


bool GPC_Engine::StartKetsji(void)
{
	STR_String startSceneName = ""; // XXX scene->id.name + 2;
/*
	KX_KetsjiEngine* ketsjieng = new KX_KetsjiEngine(m_system);
	m_portal = new KetsjiPortal(ketsjieng);
	m_portal->setSecurity(psl_Highest);
		
	KX_ISceneConverter *sceneconverter = new KX_BlenderSceneConverter(&G, ketsjieng);
		
	m_portal->Enter(
			startSceneName,
			sceneconverter,
			m_canvas,
			m_rendertools,
			m_keyboarddev,
			m_mousedev,
			m_networkdev,
			m_system);

	m_system->SetMainLoop(m_portal->m_ketsjieng);

	m_running = true;
	*/
	return true;
}


void GPC_Engine::StartLoadingAnimation()
{
	if(m_customLoadingAnimation)
	{
	}
	else
	{
		unsigned char *blenderDataBuffer;
		int blenderDataBufferSize;
		GetRawLoadingAnimation(&blenderDataBuffer, &blenderDataBufferSize);
		if(!Start(blenderDataBuffer, blenderDataBufferSize))
			cout << "something went wrong when starting the engine" << endl;
		delete blenderDataBuffer;  // created with 'new' in GetRawLoadingAnimation()
	}
}

	
// will be platform dependant
float GPC_Engine::DetermineProgress(void)
{
#if 0
	float progress;
	if ((m_blenderData.m_ulProgress > 0) &&
			(m_blenderData.m_ulProgressMax != m_blenderData.m_ulProgress)) {
		progress = (float)m_blenderData.m_ulProgress;
		progress /= (float)m_blenderData.m_ulProgressMax;
	}
	else {
		progress = 0.f;
	}
	progress *= 100.f;
	return (unsigned int) progress ;
#endif
	return m_previousProgress + 0.01;  // temporary TODO
}

	
void GPC_Engine::UpdateLoadingAnimation(void)
{
	//int delta;

	float progress = DetermineProgress();

	if(progress > m_previousProgress)
	{
//		delta = progress - m_previousProgress;
		m_previousProgress = progress;
		if(m_previousProgress > 1.0)
			m_previousProgress = 1.0;  // limit to 1.0 (has to change !)
//			m_engine->m_previousProgress = 0.0;
	}

	STR_String to = "";
	STR_String from = "";
	STR_String subject = "progress";
	STR_String body;
	body.Format("%f", progress);  // a number between 0.0 and 1.0

	if(m_networkdev)
	{
		// Store a progress message in the network device.
		NG_NetworkMessage* msg = new NG_NetworkMessage(to, from, subject, body);
		m_networkdev->SendNetworkMessage(msg);
		msg->Release();
	}
}


void GPC_Engine::Stop()
{
	// only delete things that are created in StartKetsji()
/*	if(m_portal)
	{
		m_portal->Leave();
		delete m_portal;  // also gets rid of KX_KetsjiEngine (says Maarten)
		m_portal = 0;
	}
*/	if(m_sceneconverter)
	{
		delete m_sceneconverter;
		m_sceneconverter = 0;
	}
#if 0
	if(m_frameTimerID)
	{
		::KillTimer(0, m_frameTimerID);
		m_frameTimerID = 0;
	}
	m_engineRunning = false;
#endif

	m_running = false;
}


void GPC_Engine::Exit()
{
	if(m_running)
		Stop();

	if (m_system) {
		delete m_system;
		m_system = 0;
	}
	if (m_keyboarddev) {
		delete m_keyboarddev;
		m_keyboarddev = 0;
	}
	if (m_mousedev) {
		delete m_mousedev;
		m_mousedev = 0;
	}
	if (m_canvas) {
		delete m_canvas;
		m_canvas = 0;
	}
	if (m_rendertools) {
		delete m_rendertools;
		m_rendertools = 0;
	}
	if (m_networkdev) {
		delete m_networkdev;
		m_networkdev = 0;
	}

	m_initialized = false;
}

