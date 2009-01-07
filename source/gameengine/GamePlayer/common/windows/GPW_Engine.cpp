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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#pragma warning (disable : 4786)

#include <assert.h>

#include "GPC_MouseDevice.h"
#include "GPC_RenderTools.h"
#include "GPC_RawImage.h"

#include "GPW_Canvas.h"
#include "GPW_Engine.h"
#include "GPW_KeyboardDevice.h"
#include "GPW_System.h"

#include "SND_DeviceManager.h"

#include "NG_NetworkScene.h"
#include "NG_LoopBackNetworkDeviceInterface.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

GPW_Engine::GPW_Engine(char *customLoadingAnimationURL,
		int foregroundColor, int backgroundColor, int frameRate) :
		GPC_Engine(customLoadingAnimationURL, foregroundColor, backgroundColor,
		frameRate)
{
}


GPW_Engine::~GPW_Engine()
{
}


bool GPW_Engine::Initialize(HDC hdc, int width, int height)
{
	SND_DeviceManager::Subscribe();
	m_audiodevice = SND_DeviceManager::Instance();

	m_keyboarddev = new GPW_KeyboardDevice();
	m_mousedev = new GPC_MouseDevice();
		
	// constructor only initializes data
	m_canvas = new GPW_Canvas(0, hdc, width, height);
	m_canvas->Init();  // create the actual visual and rendering context
	
	// put the Blender logo in the topleft corner
	if(m_BlenderLogo != 0)
		// adding a banner automatically enables them
		m_BlenderLogoId = m_canvas->AddBanner(m_BlenderLogo->Width(), m_BlenderLogo->Height(),
				m_BlenderLogo->Width(), m_BlenderLogo->Height(),
				m_BlenderLogo->Data(), GPC_Canvas::alignTopLeft);

	// put the Blender3D logo in the bottom right corner
	if(m_Blender3DLogo != 0)
		// adding a banner automatically enables them
		m_Blender3DLogoId = m_canvas->AddBanner(m_Blender3DLogo->Width(), m_Blender3DLogo->Height(),
				m_Blender3DLogo->Width(), m_Blender3DLogo->Height(),
				m_Blender3DLogo->Data(), GPC_Canvas::alignBottomRight);
#if 0
	// put the NaN logo in the bottom right corner
	if(m_NaNLogo != 0)
		// adding a banner automatically enables them
		m_NaNLogoId = m_canvas->AddBanner(m_NaNLogo->Width(), m_NaNLogo->Height(),
				m_NaNLogo->Width(), m_NaNLogo->Height(),
				m_NaNLogo->Data(), GPC_Canvas::alignBottomRight);
#endif
	// enable the display of all banners
	m_canvas->SetBannerDisplayEnabled(true);

	// stuff that must be done after creation of a rendering context
	//m_canvas->InitPostRenderingContext();

	m_rendertools = new GPC_RenderTools();

	m_networkdev = new NG_LoopBackNetworkDeviceInterface();
	assert(m_networkdev);

	// creation of system needs 'current rendering context', this is taken care
	// of by the GPW_Canvas
	m_system = new GPW_System();

//	m_system->SetKeyboardDevice((GPW_KeyboardDevice *)m_keyboarddev);
//	m_system->SetMouseDevice(m_mousedev);
//	m_system->SetNetworkDevice(m_networkdev);

	m_initialized = true;

	return m_initialized;
}
