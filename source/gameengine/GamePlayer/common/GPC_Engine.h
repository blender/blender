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

/** \file GPC_Engine.h
 *  \ingroup player
 */

#ifndef __GPC_ENGINE_H__
#define __GPC_ENGINE_H__

#include "GPC_Canvas.h"
#include "GPC_System.h"

class GPC_KeyboardDevice;
class GPC_MouseDevice;

class RAS_IRenderTools;
class KetsjiPortal;
class KX_ISceneConverter;
class NG_LoopBackNetworkDeviceInterface;
class GPC_RawImage;


class GPC_Engine
{
//protected:
public:
	/** Engine construction state. */
	bool m_initialized;
	/** Engine state. */
	bool m_running;
	/** loading state, ie a file is requested and is being loaded. Different
	 *  from initialized and/or running */
	bool m_loading;

	bool m_customLoadingAnimation;

	/** Last file download progress measurement. */
	float m_previousProgress;

	/** The game engine's system abstraction. */
	GPC_System* m_system;
	/** The game engine's keyboard abstraction. */
	GPC_KeyboardDevice* m_keyboarddev;
	/** The game engine's mouse abstraction. */
	GPC_MouseDevice* m_mousedev;
	/** The game engine's canvas abstraction. */
	GPC_Canvas* m_canvas;
	/** The game engine's platform dependent render tools. */
	RAS_IRenderTools* m_rendertools;
	/** The portal used to start the engine. */
	KetsjiPortal* m_portal;
	/** Converts Blender data files. */
	KX_ISceneConverter* m_sceneconverter;
	/** Network interface. */
	NG_LoopBackNetworkDeviceInterface* m_networkdev;
	
	struct ScrArea *m_curarea;  // for future use, not used yet

	char *m_customLoadingAnimationURL;
	int m_foregroundColor;
	int m_backgroundColor;
	int m_frameRate;

	GPC_RawImage *m_BlenderLogo;
	GPC_Canvas::TBannerId m_BlenderLogoId;
	GPC_RawImage *m_Blender3DLogo;
	GPC_Canvas::TBannerId m_Blender3DLogoId;
#if 0
	GPC_RawImage *m_NaNLogo;
	GPC_Canvas::TBannerId m_NaNLogoId;
#endif

public:
	GPC_Engine(char *customLoadingAnimation,
			int foregroundColor, int backgroundColor, int frameRate);
	virtual ~GPC_Engine();
	// Initialize() functions are not put here since they have
	// different prototypes for Unix and Windows
	void StartLoadingAnimation();
	bool Start(const char *filename);  // file-on-disk starter
	bool Start(unsigned char *blenderDataBuffer,
	           unsigned int blenderDataBufferSize);  // file-in-memory starter

	void Stop();
	virtual void Exit();

	bool Initialized(void) {return m_initialized;}
	bool Loading(void) {return m_loading;}
	bool Running(void) const {return m_running;}

	virtual float DetermineProgress(void);  // will be platform dependent
	void UpdateLoadingAnimation(void);

private:
	bool StartKetsji(void);

};

#endif  // __GPC_ENGINE_H__

